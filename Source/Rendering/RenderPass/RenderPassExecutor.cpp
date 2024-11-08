

#include "Headers/RenderPassExecutor.h"
#include "Headers/RenderQueue.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/Configuration.h"

#include "Editor/Headers/Editor.h"

#include "Graphs/Headers/SceneNode.h"

#include "Geometry/Material/Headers/Material.h"
#include "Managers/Headers/RenderPassManager.h"
#include "Managers/Headers/ProjectManager.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/GFXRTPool.h"
#include "Platform/Video/Buffers/RenderTarget/Headers/RTAttachment.h"
#include "Platform/Video/Buffers/ShaderBuffer/Headers/ShaderBuffer.h"
#include "Platform/Video/Headers/GenericDrawCommand.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"
#include "Platform/Video/Headers/RenderStateBlock.h"

#include "Rendering/Headers/Renderer.h"
#include "Rendering/RenderPass/Headers/RenderQueue.h"
#include "Rendering/PostFX/Headers/PostFX.h"
#include "Rendering/Camera/Headers/Camera.h"

#include "Scenes/Headers/SceneState.h"

#include "ECS/Components/Headers/AnimationComponent.h"
#include "ECS/Components/Headers/BoundsComponent.h"
#include "ECS/Components/Headers/RenderingComponent.h"
#include "ECS/Components/Headers/SelectionComponent.h"
#include "ECS/Components/Headers/TransformComponent.h"


namespace Divide
{
    namespace
    {
        struct BufferCandidate
        {
            U16 _index{ U16_MAX };
            U16 _framesSinceLastUsed{ U16_MAX };
        };

        // Remove materials that haven't been indexed in this amount of frames to make space for new ones
        constexpr U16 g_maxMaterialFrameLifetime = 6u;
        // Use to partition parallel jobs
        constexpr U32 g_nodesPerPrepareDrawPartition = 16u;

        template<typename DataContainer>
        using ExecutorBuffer = RenderPassExecutor::ExecutorBuffer<DataContainer>;
        using BufferUpdateRange = RenderPassExecutor::BufferUpdateRange;


        inline bool Contains( const BufferUpdateRange& lhs, const BufferUpdateRange& rhs ) noexcept
        {
            return lhs._firstIDX <= rhs._firstIDX && lhs._lastIDX >= rhs._lastIDX;
        }

        inline BufferUpdateRange GetPrevRangeDiff( const BufferUpdateRange& crtRange, const BufferUpdateRange& prevRange ) noexcept
        {
            if ( crtRange.range() == 0u )
            {
                return prevRange;
            }

            BufferUpdateRange ret;
            // We only care about the case where the previous range is not fully contained by the current one
            if ( prevRange.range() > 0u && prevRange != crtRange && !Contains( crtRange, prevRange ) )
            {
                if ( prevRange._firstIDX < crtRange._firstIDX && prevRange._lastIDX > crtRange._lastIDX )
                {
                    ret = prevRange;
                }
                else if ( prevRange._firstIDX < crtRange._firstIDX )
                {
                    ret._firstIDX = prevRange._firstIDX;
                    ret._lastIDX = crtRange._firstIDX;
                }
                else if ( prevRange._lastIDX > crtRange._lastIDX )
                {
                    ret._firstIDX = crtRange._lastIDX;
                    ret._lastIDX = prevRange._lastIDX;
                }
                else
                {
                    DIVIDE_UNEXPECTED_CALL();
                }
            }

            return ret;
        }

        FORCE_INLINE bool MergeBufferUpdateRanges( BufferUpdateRange& target, const BufferUpdateRange& source ) noexcept
        {
            bool ret = false;
            if ( target._firstIDX > source._firstIDX )
            {
                target._firstIDX = source._firstIDX;
                ret = true;
            }
            if ( target._lastIDX < source._lastIDX )
            {
                target._lastIDX = source._lastIDX;
                ret = true;
            }

            return ret;
        };

        FORCE_INLINE void UpdateBufferRangeLocked( RenderPassExecutor::ExecutorBufferRange& range, const U32 idx ) noexcept
        {
            if ( range._bufferUpdateRange._firstIDX > idx )
            {
                range._bufferUpdateRange._firstIDX = idx;
            }
            if ( range._bufferUpdateRange._lastIDX < idx )
            {
                range._bufferUpdateRange._lastIDX = idx;
            }

            range._highWaterMark = std::max( range._highWaterMark, idx + 1u );
        }

        void UpdateBufferRange( RenderPassExecutor::ExecutorBufferRange& range, const U32 idx )
        {
            LockGuard<SharedMutex> w_lock( range._lock );
            UpdateBufferRangeLocked( range, idx );
        }

        void UpdateBufferRange( RenderPassExecutor::ExecutorBufferRange& range, const U32 minIdx, const U32 maxIdx )
        {
            LockGuard<SharedMutex> w_lock( range._lock );
            UpdateBufferRangeLocked( range, minIdx );
            UpdateBufferRangeLocked( range, maxIdx );
        }

        template<typename DataContainer>
        void WriteToGPUBufferInternal( ExecutorBuffer<DataContainer>& executorBuffer, BufferUpdateRange target, GFX::MemoryBarrierCommand& memCmdInOut )
        {
            if ( target.range() == 0u )
            {
                return;
            }

            PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

            const size_t bufferAlignmentRequirement = ShaderBuffer::AlignmentRequirement( executorBuffer._gpuBuffer->getUsage() );
            const size_t bufferPrimitiveSize = executorBuffer._gpuBuffer->getPrimitiveSize();
            if ( bufferPrimitiveSize < bufferAlignmentRequirement )
            {
                // We need this due to alignment concerns
                if ( target._firstIDX % 2u != 0 )
                {
                    target._firstIDX -= 1u;
                }
                if ( target._lastIDX % 2u == 0 )
                {
                    target._lastIDX += 1u;
                }
            }

            bufferPtr data = nullptr;
            if constexpr ( std::is_same<DataContainer, RenderPassExecutor::BufferIndirectionData>::value )
            {
                data = &executorBuffer._data[target._firstIDX];

            }
            else
            {
                data = &executorBuffer._data._gpuData[target._firstIDX];
            }

            memCmdInOut._bufferLocks.push_back( executorBuffer._gpuBuffer->writeData( { target._firstIDX, target.range() }, data));
        }

        template<typename DataContainer>
        void WriteToGPUBuffer( ExecutorBuffer<DataContainer>& executorBuffer, GFX::MemoryBarrierCommand& memCmdInOut )
        {
            PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

            BufferUpdateRange writeRange, prevWriteRange;
            {
                LockGuard<SharedMutex> r_lock( executorBuffer._range._lock );

                if ( !MergeBufferUpdateRanges( executorBuffer._range._bufferUpdateRangeHistory.back(), executorBuffer._range._bufferUpdateRange ) )
                {
                    NOP();
                }
                std::swap(writeRange, executorBuffer._range._bufferUpdateRange);

                // We don't need to write everything again as big chunks have been written as part of the normal frame update process
                // Try and find only the items unoutched this frame
                prevWriteRange = GetPrevRangeDiff( executorBuffer._range._bufferUpdateRangeHistory.back(), executorBuffer._range._bufferUpdateRangePrev );
                executorBuffer._range._bufferUpdateRangePrev.reset();
            }

            WriteToGPUBufferInternal( executorBuffer, writeRange, memCmdInOut );
            WriteToGPUBufferInternal( executorBuffer, prevWriteRange, memCmdInOut );
        }

        template<typename DataContainer>
        bool NodeNeedsUpdate( ExecutorBuffer<DataContainer>& executorBuffer, const U32 indirectionIDX )
        {
            PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

            {
                SharedLock<SharedMutex> w_lock( executorBuffer._nodeProcessedLock );
                if ( contains( executorBuffer._nodeProcessedThisFrame, indirectionIDX ) )
                {
                    return false;
                }
            }

            LockGuard<SharedMutex> w_lock( executorBuffer._nodeProcessedLock );
            // Check again
            if ( !contains( executorBuffer._nodeProcessedThisFrame, indirectionIDX ) )
            {
                executorBuffer._nodeProcessedThisFrame.push_back( indirectionIDX );
            }
            return true;
        }

        template<typename DataContainer>
        void ExecutorBufferPostRender( ExecutorBuffer<DataContainer>& executorBuffer )
        {
            PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

            {
                LockGuard<SharedMutex> w_lock( executorBuffer._range._lock );
                const BufferUpdateRange rangeWrittenThisFrame = executorBuffer._range._bufferUpdateRangeHistory.back();

                // At the end of the frame, bump our history queue by one position and prepare the tail for a new write
                PROFILE_SCOPE( "History Update", Profiler::Category::Scene );
                for ( U8 i = 0u; i < executorBuffer._queueLength - 1u; ++i )
                {
                    executorBuffer._range._bufferUpdateRangeHistory[i] = executorBuffer._range._bufferUpdateRangeHistory[i + 1];
                }
                executorBuffer._range._bufferUpdateRangeHistory[executorBuffer._queueLength - 1u].reset();

                // We can gather all of our history (once we evicted the oldest entry) into our "previous frame written range" entry
                executorBuffer._range._bufferUpdateRangePrev.reset();
                for ( U32 i = 0u; i < executorBuffer._gpuBuffer->queueLength() - 1u; ++i )
                {
                    MergeBufferUpdateRanges( executorBuffer._range._bufferUpdateRangePrev, executorBuffer._range._bufferUpdateRangeHistory[i] );
                }
        
                executorBuffer._range._highWaterMark = 0u;
                executorBuffer._gpuBuffer->incQueue();
            }

            // We need to increment our buffer queue to get the new write range into focus
            LockGuard<SharedMutex> w_lock( executorBuffer._nodeProcessedLock );
            efficient_clear( executorBuffer._nodeProcessedThisFrame );
        }
    }

    bool RenderPassExecutor::s_globalDataInit = false;
    Mutex RenderPassExecutor::s_indirectionGlobalLock;
    std::array<bool, RenderPassExecutor::MAX_INDIRECTION_ENTRIES> RenderPassExecutor::s_indirectionFreeList{};

    Pipeline* RenderPassExecutor::s_OITCompositionPipeline = nullptr;
    Pipeline* RenderPassExecutor::s_OITCompositionMSPipeline = nullptr;
    Pipeline* RenderPassExecutor::s_ResolveGBufferPipeline = nullptr;


    [[nodiscard]] U32 BufferUpdateRange::range() const noexcept
    {
        return _lastIDX >= _firstIDX ? _lastIDX - _firstIDX + 1u : 0u;
    }

    void BufferUpdateRange::reset() noexcept
    {
        _firstIDX = U32_MAX;
        _lastIDX = 0u;
    }

    RenderPassExecutor::RenderPassExecutor( RenderPassManager& parent, GFXDevice& context, const RenderStage stage )
        : _parent( parent )
        , _context( context )
        , _stage( stage )
    {
        _renderQueue = std::make_unique<RenderQueue>( parent.parent(), stage );

        ShaderBufferDescriptor bufferDescriptor = {};
        bufferDescriptor._bufferParams._updateFrequency = BufferUpdateFrequency::OCASSIONAL;
        bufferDescriptor._bufferParams._usageType = BufferUsageType::COMMAND_BUFFER;
        bufferDescriptor._bufferParams._elementCount = Config::MAX_VISIBLE_NODES * TotalPassCountForStage( stage );
        bufferDescriptor._bufferParams._elementSize = sizeof( IndirectIndexedDrawCommand );
        bufferDescriptor._ringBufferLength = Config::MAX_FRAMES_IN_FLIGHT + 1u;
        Util::StringFormat( bufferDescriptor._name, "CMD_DATA_{}", TypeUtil::RenderStageToString( stage ) );
        _cmdBuffer = _context.newSB( bufferDescriptor );
    }

    void RenderPassExecutor::OnStartup( [[maybe_unused]] const GFXDevice& gfx )
    {
        s_indirectionFreeList.fill( true );

        Material::OnStartup();
    }

    void RenderPassExecutor::OnShutdown( [[maybe_unused]] const GFXDevice& gfx )
    {
        s_globalDataInit = false;
        s_OITCompositionPipeline = nullptr;
        s_OITCompositionMSPipeline = nullptr;
        s_ResolveGBufferPipeline = nullptr;

        Material::OnShutdown();
    }

    void RenderPassExecutor::postInit( const Handle<ShaderProgram> OITCompositionShader,
                                       const Handle<ShaderProgram> OITCompositionShaderMS,
                                       const Handle<ShaderProgram> ResolveGBufferShaderMS )
    {
        if ( !s_globalDataInit )
        {
            s_globalDataInit = true;


            PipelineDescriptor pipelineDescriptor;
            pipelineDescriptor._stateBlock._cullMode = CullMode::NONE;
            pipelineDescriptor._stateBlock._depthTestEnabled = false;
            pipelineDescriptor._stateBlock._depthWriteEnabled = false;
            pipelineDescriptor._primitiveTopology = PrimitiveTopology::TRIANGLES;
            pipelineDescriptor._shaderProgramHandle = ResolveGBufferShaderMS;
            s_ResolveGBufferPipeline = _context.newPipeline( pipelineDescriptor );

            BlendingSettings& state0 = pipelineDescriptor._blendStates._settings[to_U8( GFXDevice::ScreenTargets::ALBEDO )];
            state0.enabled( true );
            state0.blendOp( BlendOperation::ADD );
            state0.blendSrc( BlendProperty::INV_SRC_ALPHA );
            state0.blendDest( BlendProperty::ONE );

            pipelineDescriptor._shaderProgramHandle = OITCompositionShader;
            s_OITCompositionPipeline = _context.newPipeline( pipelineDescriptor );

            pipelineDescriptor._shaderProgramHandle = OITCompositionShaderMS;
            s_OITCompositionMSPipeline = _context.newPipeline( pipelineDescriptor );

        }

        _transformBuffer._data._freeList.fill( true );
        _materialBuffer._data._lookupInfo.fill( { INVALID_MAT_HASH, g_invalidMaterialIndex } );

        ShaderBufferDescriptor bufferDescriptor = {};
        bufferDescriptor._bufferParams._updateFrequency = BufferUpdateFrequency::OCASSIONAL;
        bufferDescriptor._bufferParams._usageType = BufferUsageType::UNBOUND_BUFFER;
        bufferDescriptor._ringBufferLength = Config::MAX_FRAMES_IN_FLIGHT + 1u;
        {// Node Transform buffer
            bufferDescriptor._bufferParams._elementCount = to_U32( _transformBuffer._data._gpuData.size() );
            bufferDescriptor._bufferParams._elementSize = sizeof( NodeTransformData );
            Util::StringFormat( bufferDescriptor._name, "NODE_TRANSFORM_DATA_{}", TypeUtil::RenderStageToString( _stage ) );
            _transformBuffer._gpuBuffer = _context.newSB( bufferDescriptor );
            _transformBuffer._range._bufferUpdateRangeHistory.resize( bufferDescriptor._ringBufferLength );
            _transformBuffer._queueLength = bufferDescriptor._ringBufferLength;
        }
        {// Node Material buffer
            bufferDescriptor._bufferParams._elementCount = to_U32( _materialBuffer._data._gpuData.size() );
            bufferDescriptor._bufferParams._elementSize = sizeof( NodeMaterialData );
            Util::StringFormat( bufferDescriptor._name, "NODE_MATERIAL_DATA_{}", TypeUtil::RenderStageToString( _stage ) );
            _materialBuffer._gpuBuffer = _context.newSB( bufferDescriptor );
            _materialBuffer._range._bufferUpdateRangeHistory.resize( bufferDescriptor._ringBufferLength );
            _materialBuffer._queueLength = bufferDescriptor._ringBufferLength;
        }
        {// Indirection Buffer
            bufferDescriptor._bufferParams._elementCount = to_U32( _indirectionBuffer._data.size() );
            bufferDescriptor._bufferParams._elementSize = sizeof( NodeIndirectionData );
            Util::StringFormat( bufferDescriptor._name, "NODE_INDIRECTION_DATA_{}", TypeUtil::RenderStageToString( _stage ) );
            _indirectionBuffer._gpuBuffer = _context.newSB( bufferDescriptor );
            _indirectionBuffer._range._bufferUpdateRangeHistory.resize( bufferDescriptor._ringBufferLength );
            _indirectionBuffer._queueLength = bufferDescriptor._ringBufferLength;
        }
    }

    void RenderPassExecutor::processVisibleNodeTransform( const PlayerIndex playerIdx, RenderingComponent* rComp )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        U8 boneCount = 0u;
        U8 selectionFlag = 0u;
        AnimEvaluator::FrameIndex frameIndex{};

        const U32 indirectionIDX = Attorney::RenderingCompRenderPassExecutor::getIndirectionBufferEntry( rComp );

        if ( !NodeNeedsUpdate( _transformBuffer, indirectionIDX ) )
        {
            return;
        }

        PROFILE_SCOPE( "Buffer idx update", Profiler::Category::Scene );
        U32 transformIdx = U32_MAX;
        {
            LockGuard<Mutex> w_lock( _transformBuffer._data._freeListlock );
            for ( U32 idx = 0u; idx < Config::MAX_VISIBLE_NODES; ++idx )
            {
                if ( _transformBuffer._data._freeList[idx] )
                {
                    _transformBuffer._data._freeList[idx] = false;
                    transformIdx = idx;
                    break;
                }
            }
            DIVIDE_ASSERT( transformIdx != U32_MAX );
        }

        NodeTransformData& transformOut = _transformBuffer._data._gpuData[transformIdx];
        // Out last used transform matrix is now our previous transform
        mat4<F32>::Multiply( _context.previousFrameData( playerIdx )._previousViewProjectionMatrix, transformOut._worldMatrix, transformOut._prevWVPMatrix);

        const SceneGraphNode* node = rComp->parentSGN();

        if ( node->HasComponents( ComponentType::ANIMATION ) )
        {
            const AnimationComponent* animComp = node->get<AnimationComponent>();
            boneCount = animComp->boneCount();
            if ( animComp->playAnimations() )
            {
                frameIndex = animComp->frameIndex();
            }
        }
        if ( node->HasComponents( ComponentType::SELECTION ) )
        {
            selectionFlag = to_U8( node->get<SelectionComponent>()->selectionType() );
        }

        const TransformComponent* const transform = node->get<TransformComponent>();
        transform->getWorldMatrixInterpolated( transformOut._worldMatrix );
        transform->getWorldRotationMatrixInterpolated( transformOut._normalMatrixW );
        transformOut._normalMatrixW.setRow( 3, node->get<BoundsComponent>()->getBoundingSphere().asVec4() );

        transformOut._normalMatrixW.element( 0, 3 ) = to_F32( Util::PACK_UNORM4x8(
            0u,
            0u,
            rComp->getLoDLevel( _stage ),
            rComp->occlusionCull() ? 1u : 0u
        ) );

        transformOut._normalMatrixW.element( 1, 3 ) = to_F32( std::max( frameIndex._curr, 0) );
        transformOut._normalMatrixW.element( 2, 3 ) = to_F32( boneCount );


        UpdateBufferRangeLocked( _transformBuffer._range, transformIdx );

        if ( 0u == transformIdx ||
             _indirectionBuffer._data[indirectionIDX][TRANSFORM_IDX] != transformIdx ||
             _indirectionBuffer._data[indirectionIDX][SELECTION_FLAG] != selectionFlag)
        {
            _indirectionBuffer._data[indirectionIDX][TRANSFORM_IDX] = transformIdx;
            _indirectionBuffer._data[indirectionIDX][SELECTION_FLAG] = selectionFlag;
            UpdateBufferRange( _indirectionBuffer._range, indirectionIDX );
        }
    }

    U16 RenderPassExecutor::processVisibleNodeMaterial( RenderingComponent* rComp, bool& cacheHit )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        cacheHit = false;

        NodeMaterialData tempData{};
        // Get the colour matrix (base colour, metallic, etc)
        rComp->getMaterialData( tempData );

        // Match materials
        const size_t materialHash = HashMaterialData( tempData );

        BufferMaterialData::LookupInfoContainer& infoContainer = _materialBuffer._data._lookupInfo;
        {// Try and match an existing material
            SharedLock<SharedMutex> r_lock( _materialBuffer._range._lock );
            PROFILE_SCOPE( "processVisibleNode - try match material", Profiler::Category::Scene );
            {
                const size_t count = infoContainer.size();
                for ( size_t idx = 0u; idx < count; ++idx )
                {
                    const auto [hash, _] = infoContainer[idx];
                    if ( hash == materialHash )
                    {
                        cacheHit = true;
                        infoContainer[idx]._framesSinceLastUsed = 0u;
                        return to_U16(idx);
                    }
                }
            }
        }

        // If we fail, try and find an empty slot and update it
        PROFILE_SCOPE( "processVisibleNode - process unmatched material", Profiler::Category::Scene );

        LockGuard<SharedMutex> w_lock(_materialBuffer._range._lock);
        // No match found (cache miss) so try again and add a new entry if we still fail
        const size_t count = infoContainer.size();
        for ( size_t idx = 0u; idx < count; ++idx )
        {
            const auto [hash, _] = infoContainer[idx];
            if ( hash == materialHash )
            {
                cacheHit = true;
                infoContainer[idx]._framesSinceLastUsed = 0u;
                return to_U16(idx);
            }
        }

        BufferCandidate bestCandidate{ g_invalidMaterialIndex, 0u };

        for ( U16 idx = 0u; idx < count; ++idx )
        {
            const auto [hash, framesSinceLastUsed] = infoContainer[idx];
            // Two cases here. We either have empty slots (e.g. startup, cache clear, etc) ...
            if ( hash == INVALID_MAT_HASH )
            {
                // ... in which case our current idx is what we are looking for ...
                bestCandidate._index = idx;
                bestCandidate._framesSinceLastUsed = g_maxMaterialFrameLifetime;
                break;
            }
            // ... else we need to find a slot with a stale entry (but not one that is still in flight!)
            if ( framesSinceLastUsed >= std::max( g_maxMaterialFrameLifetime, bestCandidate._framesSinceLastUsed ) )
            {
                bestCandidate._index = idx;
                bestCandidate._framesSinceLastUsed = framesSinceLastUsed;
                // Keep going and see if we can find an even older entry
            }
        }

        DIVIDE_ASSERT( bestCandidate._index != g_invalidMaterialIndex, "RenderPassExecutor::processVisibleNode error: too many concurrent materials! Increase Config::MAX_CONCURRENT_MATERIALS" );

        infoContainer[bestCandidate._index] = { materialHash, 0u };
        assert( bestCandidate._index < _materialBuffer._data._gpuData.size() );

        _materialBuffer._data._gpuData[bestCandidate._index] = tempData;

        return bestCandidate._index;
    }


    void RenderPassExecutor::parseMaterialRange( RenderBin::SortedQueue& queue, const U32 start, const U32 end )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        U32 minMaterial = U32_MAX, maxMaterial = 0u;
        U32 minIndirection = U32_MAX, maxIndirection = 0u;

        for ( U32 i = start; i < end; ++i )
        {
            const U32 indirectionIDX = Attorney::RenderingCompRenderPassExecutor::getIndirectionBufferEntry( queue[i] );
            if ( !NodeNeedsUpdate( _materialBuffer, indirectionIDX ) )
            {
                continue;
            }

            [[maybe_unused]] bool cacheHit = false;
            const U16 idx = processVisibleNodeMaterial( queue[i], cacheHit );
            DIVIDE_ASSERT( idx != g_invalidMaterialIndex && idx != U16_MAX );

            if ( idx < minMaterial)
            {
                minMaterial = idx;
            }
            if ( idx > maxMaterial)
            {
                maxMaterial = idx;
            }

            if ( 0u == idx || _indirectionBuffer._data[indirectionIDX][MATERIAL_IDX] != idx )
            {
                _indirectionBuffer._data[indirectionIDX][MATERIAL_IDX] = idx;

                if ( indirectionIDX < minIndirection )
                {
                    minIndirection = indirectionIDX;
                }
                if ( indirectionIDX > maxIndirection )
                {
                    maxIndirection = indirectionIDX;
                }
            }
        }

        UpdateBufferRange( _indirectionBuffer._range, minIndirection, maxIndirection );
        UpdateBufferRange( _materialBuffer._range, minMaterial, maxMaterial );
    }

    [[nodiscard]] constexpr size_t MIN_NODE_COUNT( const size_t N, const size_t L) noexcept
    {
        return N == 0u ? L : N;
    }

    size_t RenderPassExecutor::buildDrawCommands( const PlayerIndex index, const RenderPassParams& params, const bool doPrePass, const bool doOITPass, GFX::CommandBuffer& bufferInOut, GFX::MemoryBarrierCommand& memCmdInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        constexpr bool doMainPass = true;

        RenderPass::PassData passData = _parent.getPassForStage( _stage ).getPassData();

        efficient_clear( _drawCommands );

        for ( RenderBin::SortedQueue& sQueue : _sortedQueues )
        {
            sQueue.resize( 0 );
            sQueue.reserve( Config::MAX_VISIBLE_NODES );
        }

        const size_t queueTotalSize = _renderQueue->getSortedQueues( {}, _sortedQueues );

        //Erase nodes with no draw commands
        for ( RenderBin::SortedQueue& queue : _sortedQueues )
        {
            erase_if( queue, []( RenderingComponent* item ) noexcept
                      {
                          return !Attorney::RenderingCompRenderPass::hasDrawCommands( *item );
                      } );
        }

        bool updateTaskDirty = false;

        TaskPool& pool = _context.context().taskPool( TaskPoolType::RENDERER );
        Task* updateTask = CreateTask( TASK_NOP );
        {
            PROFILE_SCOPE( "buildDrawCommands - process nodes: Transforms", Profiler::Category::Scene );

            U32& nodeCount = *passData._lastNodeCount;
            nodeCount = 0u;
            for ( RenderBin::SortedQueue& queue : _sortedQueues )
            {
                const U32 queueSize = to_U32( queue.size() );
                if ( queueSize > g_nodesPerPrepareDrawPartition )
                {
                    Start( *CreateTask( updateTask, [this, index, &queue, queueSize]( const Task& )
                                        {
                                            for ( U32 i = 0u; i < queueSize / 2; ++i )
                                            {
                                                processVisibleNodeTransform( index, queue[i] );
                                            }
                                        } ), pool );
                    Start( *CreateTask( updateTask, [this, index, &queue, queueSize]( const Task& )
                                        {
                                            for ( U32 i = queueSize / 2; i < queueSize; ++i )
                                            {
                                                processVisibleNodeTransform( index, queue[i] );
                                            }
                                        } ), pool );
                    updateTaskDirty = true;
                }
                else
                {
                    for ( U32 i = 0u; i < queueSize; ++i )
                    {
                        processVisibleNodeTransform( index, queue[i] );
                    }
                }
                nodeCount += queueSize;
            }
            assert( nodeCount < Config::MAX_VISIBLE_NODES );
        }
        {
            PROFILE_SCOPE( "buildDrawCommands - process nodes: Materials", Profiler::Category::Scene );
            for ( RenderBin::SortedQueue& queue : _sortedQueues )
            {
                const U32 queueSize = to_U32( queue.size() );
                if ( queueSize > g_nodesPerPrepareDrawPartition )
                {
                    const U32 midPoint = queueSize / 2;
                    Start( *CreateTask( updateTask, [this, &queue, midPoint]( const Task& )
                                        {
                                            parseMaterialRange( queue, 0u, midPoint );
                                        } ), pool );
                    Start( *CreateTask( updateTask, [this, &queue, midPoint, queueSize]( const Task& )
                                        {
                                            parseMaterialRange( queue, midPoint, queueSize );
                                        } ), pool );
                    updateTaskDirty = true;
                }
                else
                {
                    parseMaterialRange( queue, 0u, queueSize );
                }
            }
        }
        if (updateTaskDirty) 
        {
            PROFILE_SCOPE( "buildDrawCommands - process nodes: Waiting for tasks to finish", Profiler::Category::Scene );
            Start( *updateTask, pool );
            Wait( *updateTask, pool );
        }

        RenderStagePass stagePass = params._stagePass;
        const U32 startOffset = Config::MAX_VISIBLE_NODES * IndexForStage( stagePass );
        const auto retrieveCommands = [&]()
        {
            for ( RenderBin::SortedQueue& queue : _sortedQueues )
            {
                for ( RenderingComponent* rComp : queue )
                {
                    Attorney::RenderingCompRenderPass::retrieveDrawCommands( *rComp, stagePass, to_U32( startOffset ), _drawCommands );
                }
            }
        };

        {
            const RenderPassType prevType = stagePass._passType;
            if ( doPrePass )
            {
                PROFILE_SCOPE( "buildDrawCommands - retrieve draw commands: PRE_PASS", Profiler::Category::Scene );
                stagePass._passType = RenderPassType::PRE_PASS;
                retrieveCommands();
            }
            if ( doMainPass )
            {
                PROFILE_SCOPE( "buildDrawCommands - retrieve draw commands: MAIN_PASS", Profiler::Category::Scene );
                stagePass._passType = RenderPassType::MAIN_PASS;
                retrieveCommands();
            }
            if ( doOITPass )
            {
                PROFILE_SCOPE( "buildDrawCommands - retrieve draw commands: OIT_PASS", Profiler::Category::Scene );
                stagePass._passType = RenderPassType::OIT_PASS;
                retrieveCommands();
            }
            else
            {
                PROFILE_SCOPE( "buildDrawCommands - retrieve draw commands: TRANSPARENCY_PASS", Profiler::Category::Scene );
                stagePass._passType = RenderPassType::TRANSPARENCY_PASS;
                retrieveCommands();
            }
            stagePass._passType = prevType;
        }

        const U32 cmdCount = to_U32( _drawCommands.size() );
        *passData._lastCommandCount = cmdCount;


        if ( cmdCount > 0u )
        {
            PROFILE_SCOPE( "buildDrawCommands - update command buffer", Profiler::Category::Graphics );
            memCmdInOut._bufferLocks.push_back( _cmdBuffer->writeData( { startOffset, cmdCount }, _drawCommands.data() ) );
        }
        {
            PROFILE_SCOPE( "buildDrawCommands - update material buffer", Profiler::Category::Graphics );
            WriteToGPUBuffer( _materialBuffer, memCmdInOut );
        }
        {
            PROFILE_SCOPE( "buildDrawCommands - update transform buffer", Profiler::Category::Graphics );
            WriteToGPUBuffer( _transformBuffer, memCmdInOut );
        }
        {
            PROFILE_SCOPE( "buildDrawCommands - update indirection buffer", Profiler::Category::Graphics );
            WriteToGPUBuffer( _indirectionBuffer, memCmdInOut );
        }

        auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>( bufferInOut );
        cmd->_usage = DescriptorSetUsage::PER_BATCH;
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_set, 0u, ShaderStageVisibility::NONE ); //Command buffer only
            Set( binding._data, _cmdBuffer.get(), { startOffset, Config::MAX_VISIBLE_NODES});
        }
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_set, 2u, ShaderStageVisibility::COMPUTE );
            Set( binding._data, _cmdBuffer.get(), { startOffset, Config::MAX_VISIBLE_NODES } );
        }
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_set, 3u, ShaderStageVisibility::ALL );
            Set(binding._data, _transformBuffer._gpuBuffer.get(), { 0u, MIN_NODE_COUNT(_transformBuffer._range._highWaterMark, Config::MAX_VISIBLE_NODES )});
        }
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_set, 4u, ShaderStageVisibility::ALL );
            Set( binding._data, _indirectionBuffer._gpuBuffer.get(), { 0u, MIN_NODE_COUNT(_indirectionBuffer._range._highWaterMark, Config::MAX_VISIBLE_NODES )});
        }
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_set, 5u, ShaderStageVisibility::FRAGMENT );
            Set( binding._data, _materialBuffer._gpuBuffer.get(), { 0u, MIN_NODE_COUNT(_materialBuffer._range._highWaterMark, Config::MAX_CONCURRENT_MATERIALS )});
        }

        return queueTotalSize;
    }

    size_t RenderPassExecutor::prepareNodeData( const PlayerIndex index,
                                                const RenderPassParams& params,
                                                const CameraSnapshot& cameraSnapshot,
                                                const bool hasInvalidNodes,
                                                const bool doPrePass,
                                                const bool doOITPass,
                                                GFX::CommandBuffer& bufferInOut,
                                                GFX::MemoryBarrierCommand& memCmdInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        if ( hasInvalidNodes )
        {
            bool nodeRemoved = true;
            while ( nodeRemoved )
            {
                nodeRemoved = false;
                for ( size_t i = 0; i < _visibleNodesCache.size(); ++i )
                {
                    if ( !_visibleNodesCache.node( i )._materialReady )
                    {
                        _visibleNodesCache.remove( i );
                        nodeRemoved = true;
                        break;
                    }
                }
            }
        }

        RenderStagePass stagePass = params._stagePass;
        const SceneRenderState& sceneRenderState = _parent.parent().projectManager()->activeProject()->getActiveScene()->state()->renderState();
        {
            Mutex memCmdLock;

            _renderQueue->clear();

            const auto cbk = [&]( const Task* /*parentTask*/, const U32 start, const U32 end )
            {
                GFX::MemoryBarrierCommand postDrawMemCmd{};
                for ( U32 i = start; i < end; ++i )
                {
                    const VisibleNode& node = _visibleNodesCache.node( i );
                    RenderingComponent* rComp = node._node->get<RenderingComponent>();
                    if ( Attorney::RenderingCompRenderPass::prepareDrawPackage( *rComp, cameraSnapshot, sceneRenderState, stagePass, postDrawMemCmd, true ) )
                    {
                        _renderQueue->addNodeToQueue( node._node, stagePass, node._distanceToCameraSq );
                    }
                }

                LockGuard<Mutex> w_lock(memCmdLock);
                memCmdInOut._bufferLocks.insert(memCmdInOut._bufferLocks.cend(), postDrawMemCmd._bufferLocks.cbegin(), postDrawMemCmd._bufferLocks.cend());
            };

            if ( _visibleNodesCache.size() < g_nodesPerPrepareDrawPartition )
            {
                cbk( nullptr, 0, to_U32(_visibleNodesCache.size()) );
            }
            else
            {
                ParallelForDescriptor descriptor = {};
                descriptor._iterCount = to_U32( _visibleNodesCache.size() );
                descriptor._partitionSize = g_nodesPerPrepareDrawPartition;
                descriptor._priority = TaskPriority::DONT_CARE;
                descriptor._useCurrentThread = true;
                Parallel_For( _parent.parent().platformContext().taskPool( TaskPoolType::RENDERER ), descriptor, cbk );
            }
            _renderQueue->sort( stagePass );
        }

        efficient_clear( _renderQueuePackages );

        RenderQueue::PopulateQueueParams queueParams{};
        queueParams._stagePass = stagePass;
        queueParams._binType = RenderBinType::COUNT;
        queueParams._filterByBinType = false;
        _renderQueue->populateRenderQueues( queueParams, _renderQueuePackages );

        return buildDrawCommands( index, params, doPrePass, doOITPass, bufferInOut, memCmdInOut );
    }

    void RenderPassExecutor::prepareRenderQueues( const RenderPassParams& params,
                                                  const CameraSnapshot& cameraSnapshot,
                                                  bool transparencyPass,
                                                  const RenderingOrder renderOrder,
                                                  GFX::CommandBuffer& bufferInOut,
                                                  GFX::MemoryBarrierCommand& memCmdInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        RenderStagePass stagePass = params._stagePass;
        const RenderBinType targetBin = transparencyPass ? RenderBinType::TRANSLUCENT : RenderBinType::COUNT;
        const SceneRenderState& sceneRenderState = _parent.parent().projectManager()->activeProject()->getActiveScene()->state()->renderState();

        _renderQueue->clear( targetBin );

        Mutex memCmdLock;
        GFX::MemoryBarrierCommand postDrawMemCmd{};

        const U32 nodeCount = to_U32( _visibleNodesCache.size() );

        const auto cbk = [&]( const Task* /*parentTask*/, const U32 start, const U32 end )
        {
            for ( U32 i = start; i < end; ++i )
            {
                const VisibleNode& node = _visibleNodesCache.node( i );
                SceneGraphNode* sgn = node._node;
                if ( sgn->getNode().renderState().drawState( stagePass ) )
                {
                    if ( Attorney::RenderingCompRenderPass::prepareDrawPackage( *sgn->get<RenderingComponent>(), cameraSnapshot, sceneRenderState, stagePass, postDrawMemCmd, false ) )
                    {
                        _renderQueue->addNodeToQueue( sgn, stagePass, node._distanceToCameraSq, targetBin );
                    }
                }
            }

            LockGuard<Mutex> w_lock( memCmdLock );
            memCmdInOut._bufferLocks.insert( memCmdInOut._bufferLocks.cend(), postDrawMemCmd._bufferLocks.cbegin(), postDrawMemCmd._bufferLocks.cend() );
        };

        if ( nodeCount > g_nodesPerPrepareDrawPartition * 2 )
        {
            PROFILE_SCOPE( "prepareRenderQueues - parallel gather", Profiler::Category::Scene );
            ParallelForDescriptor descriptor = {};
            descriptor._iterCount = nodeCount;
            descriptor._partitionSize = g_nodesPerPrepareDrawPartition;
            descriptor._priority = TaskPriority::DONT_CARE;
            descriptor._useCurrentThread = true;
            Parallel_For( _parent.parent().platformContext().taskPool( TaskPoolType::RENDERER ), descriptor, cbk );
        }
        else
        {
            PROFILE_SCOPE( "prepareRenderQueues - serial gather", Profiler::Category::Scene );
            cbk( nullptr, 0u, nodeCount );
        }

        // Sort all bins
        _renderQueue->sort( stagePass, targetBin, renderOrder );

        efficient_clear( _renderQueuePackages );

        // Draw everything in the depth pass but only draw stuff from the translucent bin in the OIT Pass and everything else in the colour pass
        RenderQueue::PopulateQueueParams queueParams{};

        queueParams._stagePass = stagePass;
        if ( IsDepthPass( stagePass ) )
        {
            queueParams._binType = RenderBinType::COUNT;
            queueParams._filterByBinType = false;
        }
        else
        {
            queueParams._binType = RenderBinType::TRANSLUCENT;
            queueParams._filterByBinType = !transparencyPass;
        }

        _renderQueue->populateRenderQueues( queueParams, _renderQueuePackages );


        for ( const auto& [rComp, pkg] : _renderQueuePackages )
        {
            Attorney::RenderingCompRenderPassExecutor::getCommandBuffer( rComp, pkg, bufferInOut );
        }

        if ( params._stagePass._passType != RenderPassType::PRE_PASS )
        {
            auto cmd = GFX::EnqueueCommand<GFX::BeginDebugScopeCommand>( bufferInOut );
            Util::StringFormat( cmd->_scopeName, "Post Render pass for stage [ {} ]", TypeUtil::RenderStageToString( stagePass._stage ), to_U32( stagePass._stage ) );

            _renderQueue->postRender( Attorney::ProjectManagerRenderPass::renderState( _parent.parent().projectManager().get() ),
                                      params._stagePass,
                                      bufferInOut );

            GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
        }
    }

    void RenderPassExecutor::prePass( const RenderPassParams& params, const CameraSnapshot& cameraSnapshot, GFX::CommandBuffer& bufferInOut, GFX::MemoryBarrierCommand& memCmdInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        assert( params._stagePass._passType == RenderPassType::PRE_PASS );

        GFX::EnqueueCommand<GFX::BeginDebugScopeCommand>( bufferInOut )->_scopeName = " - PrePass";


        GFX::BeginRenderPassCommand renderPassCmd{};
        renderPassCmd._name = "DO_PRE_PASS";
        renderPassCmd._target = params._target;
        renderPassCmd._descriptor = params._targetDescriptorPrePass;
        renderPassCmd._clearDescriptor = params._clearDescriptorPrePass;

        GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut, renderPassCmd );
        
        prepareRenderQueues( params, cameraSnapshot, false, RenderingOrder::COUNT, bufferInOut, memCmdInOut );
        GFX::EnqueueCommand( bufferInOut, GFX::EndRenderPassCommand{} );
        
        GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
    }

    void RenderPassExecutor::occlusionPass( [[maybe_unused]] const PlayerIndex idx,
                                            const CameraSnapshot& cameraSnapshot,
                                            [[maybe_unused]] const size_t visibleNodeCount,
                                            const RenderTargetID& sourceDepthBuffer,
                                            const RenderTargetID& targetHiZBuffer,
                                            GFX::CommandBuffer& bufferInOut ) const
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        GFX::EnqueueCommand<GFX::BeginDebugScopeCommand>( bufferInOut )->_scopeName = "HiZ Construct & Cull";

        GFX::EnqueueCommand<GFX::MemoryBarrierCommand>( bufferInOut )->_bufferLocks.emplace_back( BufferLock
        {
            ._range = {0u, U32_MAX },
            ._type = BufferSyncUsage::GPU_READ_TO_GPU_WRITE,
            ._buffer = _cmdBuffer->getBufferImpl()
        });

        // Update HiZ Target
        const auto [hizTexture, hizSampler] = _context.constructHIZ( sourceDepthBuffer, targetHiZBuffer, bufferInOut );
        // Run occlusion culling CS
        _context.occlusionCull( _parent.getPassForStage( _stage ).getPassData(),
                                hizTexture,
                                hizSampler,
                                cameraSnapshot,
                                _stage == RenderStage::DISPLAY,
                                bufferInOut );

        GFX::EnqueueCommand<GFX::MemoryBarrierCommand>( bufferInOut )->_bufferLocks.emplace_back( BufferLock
        {
            ._range = {0u, U32_MAX },
            ._type = BufferSyncUsage::GPU_WRITE_TO_GPU_READ,
            ._buffer = _cmdBuffer->getBufferImpl()
        });

        GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
    }

    void RenderPassExecutor::mainPass( const RenderPassParams& params, const CameraSnapshot& cameraSnapshot, RenderTarget& target, const bool prePassExecuted, const bool hasHiZ, GFX::CommandBuffer& bufferInOut, GFX::MemoryBarrierCommand& memCmdInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        assert( params._stagePass._passType == RenderPassType::MAIN_PASS );

        GFX::EnqueueCommand<GFX::BeginDebugScopeCommand>( bufferInOut )->_scopeName = " - MainPass";

        if ( params._target != INVALID_RENDER_TARGET_ID ) [[likely]]
        {

            GFX::BeginRenderPassCommand renderPassCmd{};
            renderPassCmd._name = "DO_MAIN_PASS";
            renderPassCmd._target = params._target;
            renderPassCmd._descriptor = params._targetDescriptorMainPass;
            renderPassCmd._clearDescriptor = params._clearDescriptorMainPass;

            GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut, renderPassCmd );

            auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>( bufferInOut );
            cmd->_usage = DescriptorSetUsage::PER_PASS;
            if ( params._stagePass._stage == RenderStage::DISPLAY )
            {
                const RenderTarget* MSSource = _context.renderTargetPool().getRenderTarget( params._target );
                RTAttachment* normalsAtt = MSSource->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::NORMALS );

                DescriptorSetBinding& binding = AddBinding( cmd->_set, 0u, ShaderStageVisibility::FRAGMENT );
                Set( binding._data, normalsAtt->texture(), normalsAtt->_descriptor._sampler );
            }
            if ( hasHiZ )
            {
                DescriptorSetBinding& binding = AddBinding( cmd->_set, 1u, ShaderStageVisibility::FRAGMENT );
                const RenderTarget* hizTarget = _context.renderTargetPool().getRenderTarget( params._targetHIZ );
                RTAttachment* hizAtt = hizTarget->getAttachment( RTAttachmentType::COLOUR );
                Set( binding._data, hizAtt->texture(), hizAtt->_descriptor._sampler );
            }
            else if ( prePassExecuted )
            {
                DescriptorSetBinding& binding = AddBinding( cmd->_set, 1u, ShaderStageVisibility::FRAGMENT );
                RTAttachment* depthAtt = target.getAttachment( RTAttachmentType::DEPTH );
                Set( binding._data, depthAtt->texture(), depthAtt->_descriptor._sampler );
            }

            prepareRenderQueues( params, cameraSnapshot, false, RenderingOrder::COUNT, bufferInOut, memCmdInOut );
            GFX::EnqueueCommand( bufferInOut, GFX::EndRenderPassCommand{} );
        }

        GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
    }

    void RenderPassExecutor::woitPass( const RenderPassParams& params, const CameraSnapshot& cameraSnapshot, GFX::CommandBuffer& bufferInOut, GFX::MemoryBarrierCommand& memCmdInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        assert( params._stagePass._passType == RenderPassType::OIT_PASS );

        GFX::EnqueueCommand<GFX::BeginDebugScopeCommand>( bufferInOut )->_scopeName = " - W-OIT Pass";

        // Step1: Draw translucent items into the accumulation and revealage buffers
        GFX::BeginRenderPassCommand* beginRenderPassOitCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut );
        beginRenderPassOitCmd->_name = "OIT PASS 1";
        beginRenderPassOitCmd->_target = params._targetOIT;
        beginRenderPassOitCmd->_clearDescriptor[to_base( GFXDevice::ScreenTargets::ACCUMULATION )] = { VECTOR4_ZERO, true };
        beginRenderPassOitCmd->_clearDescriptor[to_base( GFXDevice::ScreenTargets::REVEALAGE )]    = { VECTOR4_UNIT, true };
        beginRenderPassOitCmd->_clearDescriptor[to_base( GFXDevice::ScreenTargets::NORMALS )]      = { VECTOR4_ZERO, true };
        beginRenderPassOitCmd->_clearDescriptor[to_base( GFXDevice::ScreenTargets::MODULATE )]     = { VECTOR4_ZERO, false };
        beginRenderPassOitCmd->_descriptor._drawMask[to_base( GFXDevice::ScreenTargets::ACCUMULATION )] = true;
        beginRenderPassOitCmd->_descriptor._drawMask[to_base( GFXDevice::ScreenTargets::REVEALAGE )] = true;
        beginRenderPassOitCmd->_descriptor._drawMask[to_base( GFXDevice::ScreenTargets::NORMALS )]  = true;
        beginRenderPassOitCmd->_descriptor._drawMask[to_base( GFXDevice::ScreenTargets::MODULATE )]  = true;
        beginRenderPassOitCmd->_descriptor._autoResolveMSAA = params._targetDescriptorMainPass._autoResolveMSAA;
        beginRenderPassOitCmd->_descriptor._keepMSAADataAfterResolve = params._targetDescriptorMainPass._keepMSAADataAfterResolve;

        prepareRenderQueues( params, cameraSnapshot, true, RenderingOrder::COUNT, bufferInOut, memCmdInOut );
        GFX::EnqueueCommand<GFX::EndRenderPassCommand>( bufferInOut )->_transitionMask[to_base( GFXDevice::ScreenTargets::MODULATE )] = false;

        RenderTarget* oitRT = _context.renderTargetPool().getRenderTarget( params._targetOIT );
        const auto& accumAtt = oitRT->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::ACCUMULATION );
        const auto& revAtt = oitRT->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::REVEALAGE );
        const auto& normalsAtt = oitRT->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::NORMALS );

        // Step2: Composition pass
        // Don't clear depth & colours and do not write to the depth buffer
        GFX::BeginRenderPassCommand* beginRenderPassCompCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut);
        beginRenderPassCompCmd->_name = "OIT PASS 2";
        beginRenderPassCompCmd->_target = params._target;
        beginRenderPassCompCmd->_descriptor._drawMask[to_base( GFXDevice::ScreenTargets::ALBEDO )] = true;
        beginRenderPassCompCmd->_descriptor._drawMask[to_base( GFXDevice::ScreenTargets::VELOCITY )] = false;
        beginRenderPassCompCmd->_descriptor._drawMask[to_base( GFXDevice::ScreenTargets::NORMALS )] = true;
        beginRenderPassCompCmd->_descriptor._autoResolveMSAA = params._targetDescriptorMainPass._autoResolveMSAA;
        beginRenderPassCompCmd->_descriptor._keepMSAADataAfterResolve = params._targetDescriptorMainPass._keepMSAADataAfterResolve;

        GFX::EnqueueCommand<GFX::SetCameraCommand>( bufferInOut )->_cameraSnapshot = Camera::GetUtilityCamera( Camera::UtilityCamera::_2D )->snapshot();
        GFX::EnqueueCommand<GFX::BindPipelineCommand>( bufferInOut )->_pipeline = params._useMSAA ? s_OITCompositionMSPipeline : s_OITCompositionPipeline;
        {
            auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>( bufferInOut );
            cmd->_usage = DescriptorSetUsage::PER_DRAW;
            {
                DescriptorSetBinding& binding = AddBinding( cmd->_set, 0u, ShaderStageVisibility::FRAGMENT );
                Set( binding._data, accumAtt->texture(), accumAtt->_descriptor._sampler );
            }
            {
                DescriptorSetBinding& binding = AddBinding( cmd->_set, 1u, ShaderStageVisibility::FRAGMENT );
                Set( binding._data, revAtt->texture(), revAtt->_descriptor._sampler );
            }
            {
                DescriptorSetBinding& binding = AddBinding( cmd->_set, 2u, ShaderStageVisibility::FRAGMENT );
                Set( binding._data, normalsAtt->texture(), normalsAtt->_descriptor._sampler );
            }
        }
        GFX::EnqueueCommand<GFX::DrawCommand>( bufferInOut )->_drawCommands.emplace_back();
        GFX::EnqueueCommand<GFX::EndRenderPassCommand>( bufferInOut );
        GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
    }

    void RenderPassExecutor::transparencyPass( const RenderPassParams& params, const CameraSnapshot& cameraSnapshot, GFX::CommandBuffer& bufferInOut, GFX::MemoryBarrierCommand& memCmdInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        assert( params._stagePass._passType == RenderPassType::TRANSPARENCY_PASS );

        //Grab all transparent geometry
        GFX::BeginRenderPassCommand beginRenderPassTransparentCmd{};
        beginRenderPassTransparentCmd._name = "DO_TRANSPARENCY_PASS";
        beginRenderPassTransparentCmd._target = params._target;
        beginRenderPassTransparentCmd._descriptor = params._targetDescriptorMainPass;

        GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut, beginRenderPassTransparentCmd );
        prepareRenderQueues( params, cameraSnapshot, true, RenderingOrder::BACK_TO_FRONT, bufferInOut, memCmdInOut );
        GFX::EnqueueCommand<GFX::EndRenderPassCommand>( bufferInOut );
    }

    void RenderPassExecutor::resolveMainScreenTarget( const RenderPassParams& params, GFX::CommandBuffer& bufferInOut ) const
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        // If we rendered to the multisampled screen target, we can now copy the colour to our regular buffer as we are done with it at this point
        if ( params._target == RenderTargetNames::SCREEN && params._useMSAA )
        {
            GFX::EnqueueCommand<GFX::BeginDebugScopeCommand>( bufferInOut )->_scopeName = " - Resolve Screen Targets";

            GFX::BeginRenderPassCommand* beginRenderPassCommand = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut );
            beginRenderPassCommand->_target = RenderTargetNames::NORMALS_RESOLVED;
            beginRenderPassCommand->_clearDescriptor[0u] = { VECTOR4_ZERO, true };
            beginRenderPassCommand->_descriptor._drawMask[0u] = true;
            beginRenderPassCommand->_name = "RESOLVE_MAIN_GBUFFER";

            GFX::EnqueueCommand<GFX::BindPipelineCommand>( bufferInOut )->_pipeline = s_ResolveGBufferPipeline;

            const RenderTarget* MSSource = _context.renderTargetPool().getRenderTarget( params._target );
            RTAttachment* normalsAtt = MSSource->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::NORMALS );

            auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>( bufferInOut );
            cmd->_usage = DescriptorSetUsage::PER_DRAW;
            DescriptorSetBinding& binding = AddBinding( cmd->_set, 0u, ShaderStageVisibility::FRAGMENT );
            Set( binding._data, normalsAtt->texture(), normalsAtt->_descriptor._sampler );

            GFX::EnqueueCommand<GFX::DrawCommand>( bufferInOut )->_drawCommands.emplace_back();
            GFX::EnqueueCommand<GFX::EndRenderPassCommand>( bufferInOut );

            GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
        }
    }

    bool RenderPassExecutor::validateNodesForStagePass( const RenderStagePass stagePass )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        bool ret = false;
        const I32 nodeCount = to_I32( _visibleNodesCache.size() );
        for ( I32 i = nodeCount - 1; i >= 0; i-- )
        {
            VisibleNode& node = _visibleNodesCache.node( i );
            if ( node._node == nullptr || (node._materialReady && !Attorney::SceneGraphNodeRenderPassManager::canDraw( node._node, stagePass )) )
            {
                node._materialReady = false;
                ret = true;
            }
        }
        return ret;
    }

    void RenderPassExecutor::doCustomPass( const PlayerIndex playerIdx, Camera* camera, RenderPassParams params, GFX::CommandBuffer& bufferInOut, GFX::MemoryBarrierCommand& memCmdInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        assert( params._stagePass._stage == _stage );

        if ( !camera->updateLookAt() )
        {
            NOP();
        }
        const CameraSnapshot& camSnapshot = camera->snapshot();

        GFX::BeginDebugScopeCommand* beginDebugScopeCmd = GFX::EnqueueCommand<GFX::BeginDebugScopeCommand>( bufferInOut );
        if ( params._passName.empty() )
        {
            Util::StringFormat<Str<64>>( beginDebugScopeCmd->_scopeName, "Custom pass ( {} )", TypeUtil::RenderStageToString( _stage ) );
        }
        else
        {
            Util::StringFormat<Str<64>>( beginDebugScopeCmd->_scopeName, "Custom pass ( {} - {} )", TypeUtil::RenderStageToString( _stage ), params._passName );
        }

        

        RenderTarget* target = _context.renderTargetPool().getRenderTarget( params._target );

        _visibleNodesCache.reset();
        if ( params._singleNodeRenderGUID == 0 ) [[unlikely]]
        {
            // Render nothing!
            NOP();
        }
        else if ( params._singleNodeRenderGUID == -1 ) [[likely]]
        {
            // Cull the scene and grab the visible nodes
            I64 ignoreGUID = params._sourceNode == nullptr ? -1 : params._sourceNode->getGUID();

            NodeCullParams cullParams = {};
            Attorney::ProjectManagerRenderPass::initDefaultCullValues( _parent.parent().projectManager().get(), _stage, cullParams );

            cullParams._clippingPlanes = params._clippingPlanes;
            cullParams._stage = _stage;
            cullParams._minExtents = params._minExtents;
            cullParams._ignoredGUIDS = { &ignoreGUID, 1 };
            cullParams._cameraEyePos = camSnapshot._eye;
            cullParams._frustum = &camera->getFrustum();
            cullParams._cullMaxDistance = std::min( cullParams._cullMaxDistance, camSnapshot._zPlanes.y );
            cullParams._maxLoD = params._maxLoD;

            U16 cullFlags = to_base( CullOptions::DEFAULT_CULL_OPTIONS );
            if ( !( params._drawMask & to_U8( 1 << to_base( RenderPassParams::Flags::DRAW_DYNAMIC_NODES ) ) ) )
            {
                cullFlags |= to_base( CullOptions::CULL_DYNAMIC_NODES );
            }
            if ( !(params._drawMask & to_U8( 1 << to_base( RenderPassParams::Flags::DRAW_STATIC_NODES ) ) ) )
            {
                cullFlags |= to_base( CullOptions::CULL_STATIC_NODES );
            }
            Attorney::ProjectManagerRenderPass::cullScene( _parent.parent().projectManager().get(), cullParams, cullFlags, _visibleNodesCache );
        }
        else
        {
            Attorney::ProjectManagerRenderPass::findNode( _parent.parent().projectManager().get(), camera->snapshot()._eye, params._singleNodeRenderGUID, _visibleNodesCache );
        }

        const bool drawTranslucents = (params._drawMask & to_U8( 1 << to_base( RenderPassParams::Flags::DRAW_TRANSLUCENT_NODES))) && _stage != RenderStage::SHADOW;

        constexpr bool doMainPass = true;
        // PrePass requires a depth buffer
        const bool doPrePass = _stage != RenderStage::SHADOW &&
                               params._target != INVALID_RENDER_TARGET_ID &&
                               target->usesAttachment( RTAttachmentType::DEPTH );
        const bool doOITPass = drawTranslucents && params._targetOIT != INVALID_RENDER_TARGET_ID;
        const bool doOcclusionPass = doPrePass && params._targetHIZ != INVALID_RENDER_TARGET_ID;

        bool hasInvalidNodes = false;
        {
            PROFILE_SCOPE( "doCustomPass: Validate draw", Profiler::Category::Scene );
            if ( doPrePass )
            {
                params._stagePass._passType = RenderPassType::PRE_PASS;
                hasInvalidNodes = validateNodesForStagePass( params._stagePass ) || hasInvalidNodes;
            }
            if ( doMainPass )
            {
                params._stagePass._passType = RenderPassType::MAIN_PASS;
                hasInvalidNodes = validateNodesForStagePass( params._stagePass ) || hasInvalidNodes;
            }
            if ( doOITPass )
            {
                params._stagePass._passType = RenderPassType::OIT_PASS;
                hasInvalidNodes = validateNodesForStagePass( params._stagePass ) || hasInvalidNodes;
            }
            else if ( drawTranslucents )
            {
                params._stagePass._passType = RenderPassType::TRANSPARENCY_PASS;
                hasInvalidNodes = validateNodesForStagePass( params._stagePass ) || hasInvalidNodes;
            }
        }

        if ( params._feedBackContainer != nullptr )
        {
            params._feedBackContainer->resize( _visibleNodesCache.size() );
            std::memcpy( params._feedBackContainer->data(), _visibleNodesCache.data(), _visibleNodesCache.size() * sizeof( VisibleNode ) );
            if ( hasInvalidNodes )
            {
                // This may hurt ... a lot ... -Ionut
                dvd_erase_if( *params._feedBackContainer, []( VisibleNode& node )
                              {
                                  return node._node == nullptr || !node._materialReady;
                              } );
            };
        }

        // Tell the Rendering API to draw from our desired PoV
        GFX::SetCameraCommand* camCmd = GFX::EnqueueCommand<GFX::SetCameraCommand>( bufferInOut );
        camCmd->_cameraSnapshot = camSnapshot;

        if ( params._refreshLightData )
        {
            Attorney::ProjectManagerRenderPass::prepareLightData( _parent.parent().projectManager().get(), _stage, camSnapshot, memCmdInOut );
        }

        GFX::EnqueueCommand<GFX::SetClipPlanesCommand>( bufferInOut )->_clippingPlanes =  params._clippingPlanes;

        // We prepare all nodes for the MAIN_PASS rendering. PRE_PASS and OIT_PASS are support passes only. Their order and sorting are less important.
        params._stagePass._passType = RenderPassType::MAIN_PASS;
        const size_t visibleNodeCount = prepareNodeData( playerIdx, params, camSnapshot, hasInvalidNodes, doPrePass, doOITPass, bufferInOut, memCmdInOut );

#   pragma region PRE_PASS
        // We need the pass to be PRE_PASS even if we skip the prePass draw stage as it is the default state subsequent operations expect
        params._stagePass._passType = RenderPassType::PRE_PASS;
        if ( doPrePass )
        {
            prePass( params, camSnapshot, bufferInOut, memCmdInOut );
            if ( _stage == RenderStage::DISPLAY )
            {
                resolveMainScreenTarget( params, bufferInOut );
            }
        }
#   pragma endregion
#   pragma region HI_Z
        if ( doOcclusionPass )
        {
            //ToDo: Find a way to skip occlusion culling for low number of nodes in view but also keep light culling up and running -Ionut
            assert( params._stagePass._passType == RenderPassType::PRE_PASS );

            // This also renders into our HiZ texture that we may want to use later in PostFX
            occlusionPass( playerIdx, camSnapshot, visibleNodeCount, RenderTargetNames::SCREEN, params._targetHIZ, bufferInOut );
        }
#   pragma endregion

#   pragma region LIGHT_PASS
        _context.getRenderer().prepareLighting( _stage, { 0, 0, target->getWidth(), target->getHeight() }, camSnapshot, bufferInOut );
#   pragma endregion

#   pragma region MAIN_PASS
        // Same as for PRE_PASS. Subsequent operations expect a certain state
        params._stagePass._passType = RenderPassType::MAIN_PASS;
        if ( _stage == RenderStage::DISPLAY )
        {
            _context.getRenderer().postFX().prePass( playerIdx, camSnapshot, bufferInOut );
        }
        if ( doMainPass ) [[likely]]
        {
            // If we draw translucents, no point in resolving now. We can resolve after translucent pass
            mainPass( params, camSnapshot, *target, doPrePass, doOcclusionPass, bufferInOut, memCmdInOut );
        }
        else [[unlikely]]
        {
            DIVIDE_UNEXPECTED_CALL();
        }
#   pragma endregion

#   pragma region TRANSPARENCY_PASS
        if ( drawTranslucents )
        {
            // If doIOTPass is false, use forward pass shaders (i.e. MAIN_PASS again for transparents)
            if ( doOITPass )
            {
                params._stagePass._passType = RenderPassType::OIT_PASS;
                woitPass( params, camSnapshot, bufferInOut, memCmdInOut );
            }
            else
            {
                params._stagePass._passType = RenderPassType::TRANSPARENCY_PASS;
                transparencyPass( params, camSnapshot, bufferInOut, memCmdInOut );
            }
        }
#   pragma endregion

        GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
    }

    void RenderPassExecutor::postRender()
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        ExecutorBufferPostRender( _indirectionBuffer );
        ExecutorBufferPostRender( _materialBuffer );
        ExecutorBufferPostRender( _transformBuffer );
        {
            PROFILE_SCOPE( "Increment Lifetime", Profiler::Category::Scene );
            for ( BufferMaterialData::LookupInfo& it : _materialBuffer._data._lookupInfo )
            {
                it._framesSinceLastUsed += ( it._hash != INVALID_MAT_HASH ) ? 1u : 0u;
            }
        }
        {
            PROFILE_SCOPE( "Clear Freelists", Profiler::Category::Scene );
            LockGuard<Mutex> w_lock( _transformBuffer._data._freeListlock );
            _transformBuffer._data._freeList.fill( true );
        }

        _cmdBuffer->incQueue();
    }

    U32 RenderPassExecutor::renderQueueSize() const
    {
        return to_U32( _renderQueuePackages.size() );
    }

    void RenderPassExecutor::OnRenderingComponentCreation( RenderingComponent* rComp )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Streaming );

        LockGuard<Mutex> w_lock( s_indirectionGlobalLock );
        assert( Attorney::RenderingCompRenderPassExecutor::getIndirectionBufferEntry( rComp ) == U32_MAX );
        U32 entry = U32_MAX;
        for ( U32 i = 0u; i < MAX_INDIRECTION_ENTRIES; ++i )
        {
            if ( s_indirectionFreeList[i] )
            {
                s_indirectionFreeList[i] = false;
                entry = i;
                break;
            }
        }
        DIVIDE_ASSERT( entry != U32_MAX, "Insufficient space left in indirection buffer. Consider increasing available storage!" );
        Attorney::RenderingCompRenderPassExecutor::setIndirectionBufferEntry( rComp, entry );
    }

    void RenderPassExecutor::OnRenderingComponentDestruction( RenderingComponent* rComp )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Streaming );

        const U32 entry = Attorney::RenderingCompRenderPassExecutor::getIndirectionBufferEntry( rComp );
        if ( entry == U32_MAX )
        {
            DIVIDE_UNEXPECTED_CALL();
            return;
        }
        LockGuard<Mutex> w_lock( s_indirectionGlobalLock );
        assert( !s_indirectionFreeList[entry] );
        s_indirectionFreeList[entry] = true;
    }

} //namespace Divide
