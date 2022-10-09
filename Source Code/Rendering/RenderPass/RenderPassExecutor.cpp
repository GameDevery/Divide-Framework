#include "stdafx.h"

#include "Headers/RenderPassExecutor.h"
#include "Headers/RenderQueue.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/EngineTaskPool.h"

#include "Editor/Headers/Editor.h"

#include "Graphs/Headers/SceneNode.h"

#include "Geometry/Material/Headers/Material.h"
#include "Managers/Headers/RenderPassManager.h"
#include "Managers/Headers/SceneManager.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/GFXRTPool.h"
#include "Platform/Video/Headers/CommandBuffer.h"
#include "Platform/Video/Buffers/RenderTarget/Headers/RTAttachment.h"
#include "Platform/Video/Buffers/ShaderBuffer/Headers/ShaderBuffer.h"
#include "Platform/Video/Headers/GenericDrawCommand.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"

#include "Rendering/Headers/Renderer.h"
#include "Rendering/RenderPass/Headers/RenderQueue.h"
#include "Rendering/PostFX/Headers/PostFX.h"
#include "Rendering/Camera/Headers/Camera.h"

#include "Scenes/Headers/SceneState.h"

#include "ECS/Components/Headers/AnimationComponent.h"
#include "ECS/Components/Headers/BoundsComponent.h"
#include "ECS/Components/Headers/RenderingComponent.h"
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
        constexpr U16 g_maxIndirectionFrameLifetime = 6u;
        // Use to partition parallel jobs
        constexpr U32 g_nodesPerPrepareDrawPartition = 16u;

        template<typename DataContainer>
        using ExecutorBuffer = RenderPassExecutor::ExecutorBuffer<DataContainer>;
        using BufferUpdateRange = RenderPassExecutor::BufferUpdateRange;

        inline bool operator==( const BufferUpdateRange& lhs, const BufferUpdateRange& rhs ) noexcept
        {
            return lhs._firstIDX == rhs._firstIDX && lhs._lastIDX == rhs._lastIDX;
        }

        inline bool operator!=( const BufferUpdateRange& lhs, const BufferUpdateRange& rhs ) noexcept
        {
            return lhs._firstIDX != rhs._firstIDX || lhs._lastIDX != rhs._lastIDX;
        }

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

        template<typename DataContainer>
        FORCE_INLINE U32 GetPreviousIndex( ExecutorBuffer<DataContainer>& executorBuffer, U32 idx )
        {
            if ( idx == 0 )
            {
                idx = executorBuffer._gpuBuffer->queueLength();
            }

            return (idx - 1) % executorBuffer._gpuBuffer->queueLength();
        };

        template<typename DataContainer>
        FORCE_INLINE U32 GetNextIndex( ExecutorBuffer<DataContainer>& executorBuffer, const U32 idx )
        {
            return (idx + 1) % executorBuffer._gpuBuffer->queueLength();
        };

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

        template<typename DataContainer>
        FORCE_INLINE void UpdateBufferRangeLocked( ExecutorBuffer<DataContainer>& executorBuffer, const U32 idx ) noexcept
        {
            if ( executorBuffer._bufferUpdateRange._firstIDX > idx )
            {
                executorBuffer._bufferUpdateRange._firstIDX = idx;
            }
            if ( executorBuffer._bufferUpdateRange._lastIDX < idx )
            {
                executorBuffer._bufferUpdateRange._lastIDX = idx;
            }

            executorBuffer._highWaterMark = std::max( executorBuffer._highWaterMark, idx + 1u );
        }

        template<typename DataContainer>
        void UpdateBufferRange( ExecutorBuffer<DataContainer>& executorBuffer, const U32 idx )
        {
            ScopedLock<Mutex> w_lock( executorBuffer._lock );
            UpdateBufferRangeLocked( executorBuffer, idx );
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

            memCmdInOut._bufferLocks.push_back( executorBuffer._gpuBuffer->writeData( { target._firstIDX, target.range() }, &executorBuffer._data._gpuData[target._firstIDX] ) );
        }

        template<typename DataContainer>
        void WriteToGPUBuffer( ExecutorBuffer<DataContainer>& executorBuffer, GFX::MemoryBarrierCommand& memCmdInOut )
        {
            PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

            BufferUpdateRange writeRange, prevWriteRange;
            {
                ScopedLock<Mutex> r_lock( executorBuffer._lock );

                if ( !MergeBufferUpdateRanges( executorBuffer._bufferUpdateRangeHistory.back(), executorBuffer._bufferUpdateRange ) )
                {
                    NOP();
                }
                writeRange = executorBuffer._bufferUpdateRange;
                executorBuffer._bufferUpdateRange.reset();

                if_constexpr( RenderPass::DataBufferRingSize > 1u )
                {
                    // We don't need to write everything again as big chunks have been written as part of the normal frame update process
                    // Try and find only the items unoutched this frame
                    prevWriteRange = GetPrevRangeDiff( executorBuffer._bufferUpdateRangeHistory.back(), executorBuffer._bufferUpdateRangePrev );
                    executorBuffer._bufferUpdateRangePrev.reset();
                }
            }

            WriteToGPUBufferInternal( executorBuffer, writeRange, memCmdInOut );
            if_constexpr( RenderPass::DataBufferRingSize > 1u )
            {
                WriteToGPUBufferInternal( executorBuffer, prevWriteRange, memCmdInOut );
            }
        }

        template<typename DataContainer>
        bool NodeNeedsUpdate( ExecutorBuffer<DataContainer>& executorBuffer, const U32 indirectionIDX )
        {
            PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

            {
                SharedLock<SharedMutex> w_lock( executorBuffer._proccessedLock );
                if ( contains( executorBuffer._nodeProcessedThisFrame, indirectionIDX ) )
                {
                    return false;
                }
            }

            ScopedLock<SharedMutex> w_lock( executorBuffer._proccessedLock );
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

            ScopedLock<Mutex> w_lock( executorBuffer._lock );
            const BufferUpdateRange rangeWrittenThisFrame = executorBuffer._bufferUpdateRangeHistory.back();

            // At the end of the frame, bump our history queue by one position and prepare the tail for a new write
            if_constexpr( RenderPass::DataBufferRingSize > 1u )
            {
                PROFILE_SCOPE( "History Update", Profiler::Category::Scene );
                for ( U8 i = 0u; i < RenderPass::DataBufferRingSize - 1; ++i )
                {
                    executorBuffer._bufferUpdateRangeHistory[i] = executorBuffer._bufferUpdateRangeHistory[i + 1];
                }
                executorBuffer._bufferUpdateRangeHistory[RenderPass::DataBufferRingSize - 1].reset();

                // We can gather all of our history (once we evicted the oldest entry) into our "previous frame written range" entry
                executorBuffer._bufferUpdateRangePrev.reset();
                for ( U32 i = 0u; i < executorBuffer._gpuBuffer->queueLength() - 1u; ++i )
                {
                    MergeBufferUpdateRanges( executorBuffer._bufferUpdateRangePrev, executorBuffer._bufferUpdateRangeHistory[i] );
                }
            }
            // We need to increment our buffer queue to get the new write range into focus
            executorBuffer._gpuBuffer->incQueue();
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
        _renderQueue = eastl::make_unique<RenderQueue>( parent.parent(), stage );

        ShaderBufferDescriptor bufferDescriptor = {};
        bufferDescriptor._bufferParams._updateFrequency = BufferUpdateFrequency::OCASSIONAL;
        bufferDescriptor._bufferParams._updateUsage = BufferUpdateUsage::CPU_W_GPU_R;
        bufferDescriptor._ringBufferLength = RenderPass::DataBufferRingSize;
        bufferDescriptor._bufferParams._elementCount = Config::MAX_VISIBLE_NODES * TotalPassCountForStage( stage );
        bufferDescriptor._usage = ShaderBuffer::Usage::COMMAND_BUFFER;
        bufferDescriptor._bufferParams._elementSize = sizeof( IndirectDrawCommand );
        bufferDescriptor._name = Util::StringFormat( "CMD_DATA_%s", TypeUtil::RenderStageToString( stage ) );
        _cmdBuffer = _context.newSB( bufferDescriptor );
    }

    void RenderPassExecutor::OnStartup( const GFXDevice& gfx )
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

    void RenderPassExecutor::postInit( const ShaderProgram_ptr& OITCompositionShader,
                                       const ShaderProgram_ptr& OITCompositionShaderMS,
                                       const ShaderProgram_ptr& ResolveGBufferShaderMS )
    {

        if ( !s_globalDataInit )
        {
            s_globalDataInit = true;

            PipelineDescriptor pipelineDescriptor;
            pipelineDescriptor._stateHash = _context.get2DStateBlock();
            pipelineDescriptor._primitiveTopology = PrimitiveTopology::TRIANGLES;

            pipelineDescriptor._shaderProgramHandle = ResolveGBufferShaderMS->handle();
            s_ResolveGBufferPipeline = _context.newPipeline( pipelineDescriptor );

            BlendingSettings& state0 = pipelineDescriptor._blendStates._settings[to_U8( GFXDevice::ScreenTargets::ALBEDO )];
            state0.enabled( true );
            state0.blendOp( BlendOperation::ADD );
            if_constexpr( Config::USE_COLOURED_WOIT )
            {
                state0.blendSrc( BlendProperty::INV_SRC_ALPHA );
                state0.blendDest( BlendProperty::ONE );
            }
        else
        {
            state0.blendSrc( BlendProperty::SRC_ALPHA );
            state0.blendDest( BlendProperty::INV_SRC_ALPHA );
        }

        pipelineDescriptor._shaderProgramHandle = OITCompositionShader->handle();
        s_OITCompositionPipeline = _context.newPipeline( pipelineDescriptor );

        pipelineDescriptor._shaderProgramHandle = OITCompositionShaderMS->handle();
        s_OITCompositionMSPipeline = _context.newPipeline( pipelineDescriptor );

        }

        _transformBuffer._data._freeList.fill( true );
        _materialBuffer._data._lookupInfo.fill( { INVALID_MAT_HASH, g_invalidMaterialIndex } );

        ShaderBufferDescriptor bufferDescriptor = {};
        bufferDescriptor._bufferParams._updateFrequency = BufferUpdateFrequency::OCASSIONAL;
        bufferDescriptor._bufferParams._updateUsage = BufferUpdateUsage::CPU_W_GPU_R;
        bufferDescriptor._ringBufferLength = RenderPass::DataBufferRingSize;
        bufferDescriptor._usage = ShaderBuffer::Usage::UNBOUND_BUFFER;
        {// Node Transform buffer
            bufferDescriptor._bufferParams._elementCount = to_U32( _transformBuffer._data._gpuData.size() );
            bufferDescriptor._bufferParams._elementSize = sizeof( NodeTransformData );
            bufferDescriptor._name = Util::StringFormat( "NODE_TRANSFORM_DATA_%s", TypeUtil::RenderStageToString( _stage ) );
            _transformBuffer._gpuBuffer = _context.newSB( bufferDescriptor );
            _transformBuffer._bufferUpdateRangeHistory.resize( bufferDescriptor._ringBufferLength );
        }
        {// Node Material buffer
            bufferDescriptor._bufferParams._elementCount = to_U32( _materialBuffer._data._gpuData.size() );
            bufferDescriptor._bufferParams._elementSize = sizeof( NodeMaterialData );
            bufferDescriptor._name = Util::StringFormat( "NODE_MATERIAL_DATA_%s", TypeUtil::RenderStageToString( _stage ) );
            _materialBuffer._gpuBuffer = _context.newSB( bufferDescriptor );
            _materialBuffer._bufferUpdateRangeHistory.resize( bufferDescriptor._ringBufferLength );
        }
        {// Indirection Buffer
            bufferDescriptor._bufferParams._elementCount = to_U32( _indirectionBuffer._data._gpuData.size() );
            bufferDescriptor._bufferParams._elementSize = sizeof( NodeIndirectionData );
            bufferDescriptor._name = Util::StringFormat( "NODE_INDIRECTION_DATA_%s", TypeUtil::RenderStageToString( _stage ) );
            _indirectionBuffer._gpuBuffer = _context.newSB( bufferDescriptor );
            _indirectionBuffer._bufferUpdateRangeHistory.resize( bufferDescriptor._ringBufferLength );
        }
    }

    void RenderPassExecutor::processVisibleNodeTransform( RenderingComponent* rComp, const D64 interpolationFactor )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        const U32 indirectionIDX = Attorney::RenderingCompRenderPassExecutor::getIndirectionBufferEntry( rComp );

        if ( !NodeNeedsUpdate( _transformBuffer, indirectionIDX ) )
        {
            return;
        }

        NodeTransformData transformOut;

        const SceneGraphNode* node = rComp->parentSGN();

        { // Transform
            PROFILE_SCOPE( "Transform query", Profiler::Category::Scene );
            const TransformComponent* const transform = node->get<TransformComponent>();

            // Get the node's world matrix properly interpolated
            transform->getPreviousWorldMatrix( transformOut._prevWorldMatrix );
            transform->getWorldMatrix( interpolationFactor, transformOut._worldMatrix );
            transformOut._normalMatrixW.set( mat3<F32>( transformOut._worldMatrix ) );

            if ( !transform->isUniformScaled() )
            {
                // Non-uniform scaling requires an inverseTranspose to negatescaling contribution but preserve rotation
                transformOut._normalMatrixW.inverseTranspose();
            }
        }

        U8 boneCount = 0u;
        U8 frameTicked = 0u;
        { //Animation
            if ( node->HasComponents( ComponentType::ANIMATION ) )
            {
                const AnimationComponent* animComp = node->get<AnimationComponent>();
                boneCount = animComp->boneCount();
                if ( animComp->playAnimations() && animComp->frameTicked() )
                {
                    frameTicked = 1u;
                }
            }
        }
        { //Misc
            transformOut._normalMatrixW.setRow( 3, node->get<BoundsComponent>()->getBoundingSphere().asVec4() );

            transformOut._normalMatrixW.element( 0, 3 ) = to_F32( Util::PACK_UNORM4x8(
                0u,
                frameTicked,
                rComp->getLoDLevel( _stage ),
                rComp->occlusionCull() ? 1u : 0u
            ) );

            U8 selectionFlag = 0u;
            // We don't propagate selection flags to children outside of the editor, so check for that
            if ( node->hasFlag( SceneGraphNode::Flags::SELECTED ) ||
                 node->parent() && node->parent()->hasFlag( SceneGraphNode::Flags::SELECTED ) )
            {
                selectionFlag = 2u;
            }
            else if ( node->hasFlag( SceneGraphNode::Flags::HOVERED ) )
            {
                selectionFlag = 1u;
            }
            transformOut._normalMatrixW.element( 1, 3 ) = to_F32( selectionFlag );
            transformOut._normalMatrixW.element( 2, 3 ) = to_F32( boneCount );
        }
        {
            PROFILE_SCOPE( "Buffer idx update", Profiler::Category::Scene );
            U32 transformIdx = U32_MAX;
            {
                ScopedLock<Mutex> w_lock( _transformBuffer._lock );
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
            _transformBuffer._data._gpuData[transformIdx] = transformOut;
            UpdateBufferRangeLocked( _transformBuffer, transformIdx );

            if ( _indirectionBuffer._data._gpuData[indirectionIDX][TRANSFORM_IDX] != transformIdx || transformIdx == 0u )
            {
                _indirectionBuffer._data._gpuData[indirectionIDX][TRANSFORM_IDX] = transformIdx;
                UpdateBufferRange( _indirectionBuffer, indirectionIDX );
            }
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

        const auto findMaterialMatch = []( const size_t targetHash, BufferMaterialData::LookupInfoContainer& data ) -> U16
        {
            const U16 count = to_U16( data.size() );
            for ( U16 idx = 0u; idx < count; ++idx )
            {
                const auto [hash, _] = data[idx];
                if ( hash == targetHash )
                {
                    return idx;
                }
            }

            return g_invalidMaterialIndex;
        };

        ScopedLock<Mutex> w_lock( _materialBuffer._lock );
        BufferMaterialData::LookupInfoContainer& infoContainer = _materialBuffer._data._lookupInfo;
        {// Try and match an existing material
            PROFILE_SCOPE( "processVisibleNode - try match material", Profiler::Category::Scene );
            const U16 idx = findMaterialMatch( materialHash, infoContainer );
            if ( idx != g_invalidMaterialIndex )
            {
                infoContainer[idx]._framesSinceLastUsed = 0u;
                cacheHit = true;
                UpdateBufferRangeLocked( _materialBuffer, idx );
                return idx;
            }
        }

        // If we fail, try and find an empty slot and update it
        PROFILE_SCOPE( "processVisibleNode - process unmatched material", Profiler::Category::Scene );
        // No match found (cache miss) so add a new entry.
        BufferCandidate bestCandidate{ g_invalidMaterialIndex, 0u };

        const U16 count = to_U16( infoContainer.size() );
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
        UpdateBufferRangeLocked( _materialBuffer, bestCandidate._index );

        return bestCandidate._index;
    }


    void RenderPassExecutor::parseMaterialRange( RenderBin::SortedQueue& queue, const U32 start, const U32 end )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        for ( U32 i = start; i < end; ++i )
        {

            const U32 indirectionIDX = Attorney::RenderingCompRenderPassExecutor::getIndirectionBufferEntry( queue[i] );
            if ( !NodeNeedsUpdate( _materialBuffer, indirectionIDX ) )
            {
                continue;
            }

            [[maybe_unused]] bool cacheHit = false;
            const U16 idx = processVisibleNodeMaterial( queue[i], cacheHit );
            DIVIDE_ASSERT( idx != g_invalidMaterialIndex && idx != U32_MAX );

            if ( _indirectionBuffer._data._gpuData[indirectionIDX][MATERIAL_IDX] != idx || idx == 0u )
            {
                _indirectionBuffer._data._gpuData[indirectionIDX][MATERIAL_IDX] = idx;
                UpdateBufferRange( _indirectionBuffer, indirectionIDX );
            }
        }
    }

    size_t RenderPassExecutor::buildDrawCommands( const RenderPassParams& params, const bool doPrePass, const bool doOITPass, GFX::CommandBuffer& bufferInOut, GFX::MemoryBarrierCommand& memCmdInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        constexpr bool doMainPass = true;

        RenderStagePass stagePass = params._stagePass;
        RenderPass::BufferData bufferData = _parent.getPassForStage( _stage ).getBufferData( stagePass );

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

        TaskPool& pool = _context.context().taskPool( TaskPoolType::HIGH_PRIORITY );
        Task* updateTask = CreateTask( TASK_NOP );
        {
            PROFILE_SCOPE( "buildDrawCommands - process nodes: Transforms", Profiler::Category::Scene );

            U32& nodeCount = *bufferData._lastNodeCount;
            nodeCount = 0u;
            for ( RenderBin::SortedQueue& queue : _sortedQueues )
            {
                const U32 queueSize = to_U32( queue.size() );
                if ( queueSize > g_nodesPerPrepareDrawPartition )
                {
                    Start( *CreateTask( updateTask, [this, &queue, queueSize]( const Task& )
                                        {
                                            const D64 interpFactor = GFXDevice::FrameInterpolationFactor();
                                            for ( U32 i = 0u; i < queueSize / 2; ++i )
                                            {
                                                processVisibleNodeTransform( queue[i], interpFactor );
                                            }
                                        } ), pool );
                    Start( *CreateTask( updateTask, [this, &queue, queueSize]( const Task& )
                                        {
                                            const D64 interpFactor = GFXDevice::FrameInterpolationFactor();
                                            for ( U32 i = queueSize / 2; i < queueSize; ++i )
                                            {
                                                processVisibleNodeTransform( queue[i], interpFactor );
                                            }
                                        } ), pool );
                }
                else
                {
                    const D64 interpFactor = GFXDevice::FrameInterpolationFactor();
                    for ( U32 i = 0u; i < queueSize; ++i )
                    {
                        processVisibleNodeTransform( queue[i], interpFactor );
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
                }
                else
                {
                    parseMaterialRange( queue, 0u, queueSize );
                }
            }
        }
        {
            PROFILE_SCOPE( "buildDrawCommands - process nodes: Waiting for tasks to finish", Profiler::Category::Scene );
            StartAndWait( *updateTask, pool );
        }

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
        *bufferData._lastCommandCount = cmdCount;


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
            DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 0u, ShaderStageVisibility::NONE ); //Command buffer only
            Set( binding._data, _cmdBuffer.get(), { startOffset, cmdCount});
        }
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 2u, ShaderStageVisibility::COMPUTE );
            Set( binding._data, _cmdBuffer.get(), { startOffset, cmdCount } );
        }
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 3u, ShaderStageVisibility::ALL );
            Set(binding._data, _transformBuffer._gpuBuffer.get(), { 0u, _transformBuffer._highWaterMark });
        }
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 4u, ShaderStageVisibility::ALL );
            Set( binding._data, _indirectionBuffer._gpuBuffer.get(), { 0u, _indirectionBuffer._highWaterMark });
        }
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 5u, ShaderStageVisibility::FRAGMENT );
            Set( binding._data, _materialBuffer._gpuBuffer.get(), { 0u, _materialBuffer._highWaterMark });
        }

        return queueTotalSize;
    }

    size_t RenderPassExecutor::prepareNodeData( const RenderPassParams& params,
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
        const SceneRenderState& sceneRenderState = _parent.parent().sceneManager()->getActiveScene().state()->renderState();
        {
            _renderQueue->clear();
            ParallelForDescriptor descriptor = {};
            descriptor._iterCount = to_U32( _visibleNodesCache.size() );
            descriptor._cbk = [&]( const Task* /*parentTask*/, const U32 start, const U32 end )
            {
                for ( U32 i = start; i < end; ++i )
                {
                    const VisibleNode& node = _visibleNodesCache.node( i );
                    RenderingComponent* rComp = node._node->get<RenderingComponent>();
                    if ( Attorney::RenderingCompRenderPass::prepareDrawPackage( *rComp, cameraSnapshot, sceneRenderState, stagePass, true ) )
                    {
                        _renderQueue->addNodeToQueue( node._node, stagePass, node._distanceToCameraSq );
                    }
                }
            };

            if ( descriptor._iterCount < g_nodesPerPrepareDrawPartition )
            {
                descriptor._cbk( nullptr, 0, descriptor._iterCount );
            }
            else
            {
                descriptor._partitionSize = g_nodesPerPrepareDrawPartition;
                descriptor._priority = TaskPriority::DONT_CARE;
                descriptor._useCurrentThread = true;
                parallel_for( _parent.parent().platformContext(), descriptor );
            }
            _renderQueue->sort( stagePass );
        }

        _renderQueuePackages.resize( 0 );

        RenderQueue::PopulateQueueParams queueParams{};
        queueParams._stagePass = stagePass;
        queueParams._binType = RenderBinType::COUNT;
        queueParams._filterByBinType = false;
        _renderQueue->populateRenderQueues( queueParams, _renderQueuePackages );

        return buildDrawCommands( params, doPrePass, doOITPass, bufferInOut, memCmdInOut );
    }

    void RenderPassExecutor::prepareRenderQueues( const RenderPassParams& params,
                                                  const CameraSnapshot& cameraSnapshot,
                                                  bool transparencyPass,
                                                  const RenderingOrder renderOrder,
                                                  GFX::CommandBuffer& bufferInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        RenderStagePass stagePass = params._stagePass;
        const RenderBinType targetBin = transparencyPass ? RenderBinType::TRANSLUCENT : RenderBinType::COUNT;
        const SceneRenderState& sceneRenderState = _parent.parent().sceneManager()->getActiveScene().state()->renderState();

        _renderQueue->clear( targetBin );

        const U32 nodeCount = to_U32( _visibleNodesCache.size() );
        ParallelForDescriptor descriptor = {};
        descriptor._cbk = [&]( const Task* /*parentTask*/, const U32 start, const U32 end )
        {
            for ( U32 i = start; i < end; ++i )
            {
                const VisibleNode& node = _visibleNodesCache.node( i );
                SceneGraphNode* sgn = node._node;
                if ( sgn->getNode().renderState().drawState( stagePass ) )
                {
                    if ( Attorney::RenderingCompRenderPass::prepareDrawPackage( *sgn->get<RenderingComponent>(), cameraSnapshot, sceneRenderState, stagePass, false ) )
                    {
                        _renderQueue->addNodeToQueue( sgn, stagePass, node._distanceToCameraSq, targetBin );
                    }
                }
            }
        };

        if ( nodeCount > g_nodesPerPrepareDrawPartition * 2 )
        {
            PROFILE_SCOPE( "prepareRenderQueues - parallel gather", Profiler::Category::Scene );
            descriptor._iterCount = nodeCount;
            descriptor._partitionSize = g_nodesPerPrepareDrawPartition;
            descriptor._priority = TaskPriority::DONT_CARE;
            descriptor._useCurrentThread = true;
            parallel_for( _parent.parent().platformContext(), descriptor );
        }
        else
        {
            PROFILE_SCOPE( "prepareRenderQueues - serial gather", Profiler::Category::Scene );
            descriptor._cbk( nullptr, 0u, nodeCount );
        }

        // Sort all bins
        _renderQueue->sort( stagePass, targetBin, renderOrder );

        _renderQueuePackages.resize( 0 );
        _renderQueuePackages.reserve( Config::MAX_VISIBLE_NODES );

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
            GFX::EnqueueCommand<GFX::BeginDebugScopeCommand>( bufferInOut )->_scopeName = Util::StringFormat( "Post Render pass for stage [ %s ]", TypeUtil::RenderStageToString( stagePass._stage ), to_U32( stagePass._stage ) );

            _renderQueue->postRender( Attorney::SceneManagerRenderPass::renderState( _parent.parent().sceneManager() ),
                                      params._stagePass,
                                      bufferInOut );

            GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
        }
    }

    void RenderPassExecutor::prePass( const RenderPassParams& params, const CameraSnapshot& cameraSnapshot, GFX::CommandBuffer& bufferInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        assert( params._stagePass._passType == RenderPassType::PRE_PASS );

        GFX::EnqueueCommand( bufferInOut, GFX::BeginDebugScopeCommand{ " - PrePass" } );


        GFX::BeginRenderPassCommand renderPassCmd{};
        renderPassCmd._name = "DO_PRE_PASS";
        renderPassCmd._target = params._target;
        renderPassCmd._descriptor = params._targetDescriptorPrePass;
        renderPassCmd._clearDescriptor = params._clearDescriptorPrePass;
        renderPassCmd._descriptor._writeLayers = params._layerParams;

        GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut, renderPassCmd );
        prepareRenderQueues( params, cameraSnapshot, false, RenderingOrder::COUNT, bufferInOut );
        GFX::EnqueueCommand( bufferInOut, GFX::EndRenderPassCommand{} );
        GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
    }

    void RenderPassExecutor::occlusionPass( const PlayerIndex idx,
                                            const CameraSnapshot& cameraSnapshot,
                                            [[maybe_unused]] const size_t visibleNodeCount,
                                            RenderStagePass stagePass,
                                            const RenderTargetID& sourceDepthBuffer,
                                            const RenderTargetID& targetHiZBuffer,
                                            GFX::CommandBuffer& bufferInOut ) const
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        //ToDo: Find a way to skip occlusion culling for low number of nodes in view but also keep light culling up and running -Ionut
        assert( stagePass._passType == RenderPassType::PRE_PASS );

        GFX::EnqueueCommand( bufferInOut, GFX::BeginDebugScopeCommand{ "HiZ Construct & Cull" } );

        // Update HiZ Target
        const auto [hizTexture, hizSampler] = _context.constructHIZ( sourceDepthBuffer, targetHiZBuffer, bufferInOut );
        // Run occlusion culling CS
        _context.occlusionCull( _parent.getPassForStage( _stage ).getBufferData( stagePass ),
                                hizTexture,
                                hizSampler,
                                cameraSnapshot,
                                _stage == RenderStage::DISPLAY,
                                bufferInOut );

        GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
    }

    void RenderPassExecutor::mainPass( const RenderPassParams& params, const CameraSnapshot& cameraSnapshot, RenderTarget& target, const bool prePassExecuted, const bool hasHiZ, GFX::CommandBuffer& bufferInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        assert( params._stagePass._passType == RenderPassType::MAIN_PASS );

        GFX::EnqueueCommand( bufferInOut, GFX::BeginDebugScopeCommand{ " - MainPass" } );

        if ( params._target != INVALID_RENDER_TARGET_ID )
        {

            GFX::BeginRenderPassCommand renderPassCmd{};
            renderPassCmd._name = "DO_MAIN_PASS";
            renderPassCmd._target = params._target;
            renderPassCmd._descriptor = params._targetDescriptorMainPass;
            renderPassCmd._descriptor._writeLayers = params._layerParams;
            renderPassCmd._clearDescriptor = params._clearDescriptorMainPass;
            if ( prePassExecuted )
            {
                SetEnabled( renderPassCmd._descriptor._drawMask, RTAttachmentType::DEPTH, RTColourAttachmentSlot::SLOT_0, false );
            }

            //prePassPolicy._alphaToCoverage = true;


            GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut, renderPassCmd );

            const RenderTarget* screenTargetMS = _context.renderTargetPool().getRenderTarget( RenderTargetNames::SCREEN_MS );
            const RTAttachment* normalsAttMS = screenTargetMS->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::NORMALS );

            auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>( bufferInOut );
            cmd->_usage = DescriptorSetUsage::PER_PASS;
            {
                DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 0u, ShaderStageVisibility::FRAGMENT );
                Set( binding._data, normalsAttMS->texture()->sampledView(), normalsAttMS->descriptor()._samplerHash );
            }
            if ( hasHiZ )
            {
                DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 1u, ShaderStageVisibility::FRAGMENT );
                const RenderTarget* hizTarget = _context.renderTargetPool().getRenderTarget( params._targetHIZ );
                RTAttachment* hizAtt = hizTarget->getAttachment( RTAttachmentType::COLOUR );
                Set( binding._data, hizAtt->texture()->sampledView(), hizAtt->descriptor()._samplerHash );
            }
            else if ( prePassExecuted )
            {
                DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 1u, ShaderStageVisibility::FRAGMENT );
                RTAttachment* depthAtt = target.getAttachment( RTAttachmentType::DEPTH );
                Set( binding._data, depthAtt->texture()->sampledView(), depthAtt->descriptor()._samplerHash );
            }

            prepareRenderQueues( params, cameraSnapshot, false, RenderingOrder::COUNT, bufferInOut );
            GFX::EnqueueCommand( bufferInOut, GFX::EndRenderPassCommand{} );
        }

        GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
    }

    void RenderPassExecutor::woitPass( const RenderPassParams& params, const CameraSnapshot& cameraSnapshot, GFX::CommandBuffer& bufferInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        assert( params._stagePass._passType == RenderPassType::OIT_PASS );

        GFX::EnqueueCommand( bufferInOut, GFX::BeginDebugScopeCommand{ " - W-OIT Pass" } );

        // Step1: Draw translucent items into the accumulation and revealage buffers
        GFX::BeginRenderPassCommand* beginRenderPassOitCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut );
        beginRenderPassOitCmd->_name = "DO_OIT_PASS_1";
        beginRenderPassOitCmd->_target = params._targetOIT;
        beginRenderPassOitCmd->_clearDescriptor._clearColourDescriptors[0] = { VECTOR4_ZERO,           GFXDevice::ScreenTargets::ACCUMULATION };
        beginRenderPassOitCmd->_clearDescriptor._clearColourDescriptors[1] = { { 1.f, 0.f, 0.f, 0.f }, GFXDevice::ScreenTargets::REVEALAGE };
        SetEnabled( beginRenderPassOitCmd->_descriptor._drawMask, RTAttachmentType::DEPTH, RTColourAttachmentSlot::SLOT_0, false );

        //beginRenderPassOitCmd->_descriptor._alphaToCoverage = true;
        {
            const RenderTarget* nonMSTarget = _context.renderTargetPool().getRenderTarget( RenderTargetNames::SCREEN );
            const auto& colourAtt = nonMSTarget->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::ALBEDO );

            auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>( bufferInOut );
            cmd->_usage = DescriptorSetUsage::PER_PASS;
            DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 2u, ShaderStageVisibility::FRAGMENT );
            Set( binding._data, colourAtt->texture()->sampledView(), colourAtt->descriptor()._samplerHash );
        }

        prepareRenderQueues( params, cameraSnapshot, true, RenderingOrder::COUNT, bufferInOut );
        GFX::EnqueueCommand<GFX::EndRenderPassCommand>( bufferInOut );

        const bool useMSAA = params._target == RenderTargetNames::SCREEN_MS;


        RenderTarget* oitRT = _context.renderTargetPool().getRenderTarget( params._targetOIT );
        const auto& accumAtt = oitRT->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::ACCUMULATION );
        const auto& revAtt = oitRT->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::REVEALAGE );

        GFX::BeginRenderPassCommand beginRenderPassCompCmd{};
        beginRenderPassCompCmd._name = "DO_OIT_PASS_2";
        beginRenderPassCompCmd._target = params._target;
        beginRenderPassCompCmd._descriptor = params._targetDescriptorComposition;
        beginRenderPassCompCmd._descriptor._writeLayers = params._layerParams;

        // Step2: Composition pass
        // Don't clear depth & colours and do not write to the depth buffer
        GFX::EnqueueCommand( bufferInOut, GFX::SetCameraCommand{ Camera::GetUtilityCamera( Camera::UtilityCamera::_2D )->snapshot() } );
        GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut, beginRenderPassCompCmd );
        GFX::EnqueueCommand( bufferInOut, GFX::BindPipelineCommand{ useMSAA ? s_OITCompositionMSPipeline : s_OITCompositionPipeline } );
        {
            auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>( bufferInOut );
            cmd->_usage = DescriptorSetUsage::PER_DRAW;
            {
                DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 0u, ShaderStageVisibility::FRAGMENT );
                Set( binding._data, accumAtt->texture()->sampledView(), accumAtt->descriptor()._samplerHash );
            }
            {
                DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 1u, ShaderStageVisibility::FRAGMENT );
                Set( binding._data, revAtt->texture()->sampledView(), revAtt->descriptor()._samplerHash );
            }
        }
        GFX::EnqueueCommand<GFX::DrawCommand>( bufferInOut );
        GFX::EnqueueCommand( bufferInOut, GFX::EndRenderPassCommand{} );
        GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
    }

    void RenderPassExecutor::transparencyPass( const RenderPassParams& params, const CameraSnapshot& cameraSnapshot, GFX::CommandBuffer& bufferInOut )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Scene );

        assert( params._stagePass._passType == RenderPassType::TRANSPARENCY_PASS );

        //Grab all transparent geometry
        GFX::EnqueueCommand( bufferInOut, GFX::BeginDebugScopeCommand{ " - Transparency Pass" } );

        GFX::BeginRenderPassCommand beginRenderPassTransparentCmd{};
        beginRenderPassTransparentCmd._name = "DO_TRANSPARENCY_PASS";
        beginRenderPassTransparentCmd._target = params._target;
        beginRenderPassTransparentCmd._descriptor._writeLayers = params._layerParams;
        SetEnabled( beginRenderPassTransparentCmd._descriptor._drawMask, RTAttachmentType::DEPTH, RTColourAttachmentSlot::SLOT_0, false );

        GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut, beginRenderPassTransparentCmd );
        prepareRenderQueues( params, cameraSnapshot, true, RenderingOrder::BACK_TO_FRONT, bufferInOut );
        GFX::EnqueueCommand( bufferInOut, GFX::EndRenderPassCommand{} );
        GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
    }

    void RenderPassExecutor::resolveMainScreenTarget( const RenderPassParams& params,
                                                      const bool resolveDepth,
                                                      const bool resolveGBuffer,
                                                      const bool resolveColourBuffer,
                                                      GFX::CommandBuffer& bufferInOut ) const
    {
        if ( !resolveDepth && !resolveGBuffer && !resolveColourBuffer )
        {
            return;
        }

        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        // If we rendered to the multisampled screen target, we can now copy the colour to our regular buffer as we are done with it at this point
        if ( params._target == RenderTargetNames::SCREEN_MS )
        {
            GFX::EnqueueCommand( bufferInOut, GFX::BeginDebugScopeCommand{ " - Resolve Screen Targets" } );

            if ( resolveDepth || resolveColourBuffer )
            {
                // If we rendered to the multisampled screen target, we can now blit the depth buffer to our resolved target
                GFX::BlitRenderTargetCommand* blitCmd = GFX::EnqueueCommand<GFX::BlitRenderTargetCommand>( bufferInOut );
                blitCmd->_source = RenderTargetNames::SCREEN_MS;
                blitCmd->_destination = RenderTargetNames::SCREEN;
                if ( resolveDepth )
                {
                    blitCmd->_params._blitDepth._inputLayer = 0u;
                    blitCmd->_params._blitDepth._outputLayer = 0u;
                }
                if ( resolveColourBuffer )
                {
                    auto& blitParams = blitCmd->_params._blitColours[0];
                    blitParams._input._index = to_U8( GFXDevice::ScreenTargets::ALBEDO );
                    blitParams._output._index = to_U8( GFXDevice::ScreenTargets::ALBEDO );
                }
            }
            if ( resolveGBuffer )
            {
                GFX::BeginRenderPassCommand* beginRenderPassCommand = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut );
                beginRenderPassCommand->_target = RenderTargetNames::SCREEN;
                beginRenderPassCommand->_clearDescriptor._clearColourDescriptors[0] = { VECTOR4_ZERO,  GFXDevice::ScreenTargets::VELOCITY };
                beginRenderPassCommand->_clearDescriptor._clearColourDescriptors[1] = { VECTOR4_ZERO,  GFXDevice::ScreenTargets::NORMALS };
                SetEnabled( beginRenderPassCommand->_descriptor._drawMask, RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::ALBEDO, false );
                SetEnabled( beginRenderPassCommand->_descriptor._drawMask, RTAttachmentType::DEPTH, RTColourAttachmentSlot::SLOT_0, false );
                beginRenderPassCommand->_name = "RESOLVE_MAIN_GBUFFER";

                GFX::EnqueueCommand( bufferInOut, GFX::BindPipelineCommand{ s_ResolveGBufferPipeline } );

                const RenderTarget* MSSource = _context.renderTargetPool().getRenderTarget( params._target );
                RTAttachment* velocityAtt = MSSource->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::VELOCITY );
                RTAttachment* normalsAtt = MSSource->getAttachment( RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::NORMALS );

                auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>( bufferInOut );
                cmd->_usage = DescriptorSetUsage::PER_DRAW;
                {
                    DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 0u, ShaderStageVisibility::FRAGMENT );
                    Set( binding._data, velocityAtt->texture()->sampledView(), velocityAtt->descriptor()._samplerHash );
                }
                {
                    DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 1u, ShaderStageVisibility::FRAGMENT );
                    Set( binding._data, normalsAtt->texture()->sampledView(), normalsAtt->descriptor()._samplerHash );
                }

                GFX::EnqueueCommand<GFX::DrawCommand>( bufferInOut );
                GFX::EnqueueCommand<GFX::EndRenderPassCommand>( bufferInOut );

            }
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

        GFX::EnqueueCommand<GFX::BeginDebugScopeCommand>( bufferInOut )->_scopeName =
            Util::StringFormat( "Custom pass ( %s - %s )", TypeUtil::RenderStageToString( _stage ), params._passName.empty() ? "N/A" : params._passName.c_str() );

        RenderTarget* target = _context.renderTargetPool().getRenderTarget( params._target );

        _visibleNodesCache.reset();
        if ( params._singleNodeRenderGUID == 0 )
        {
            // Render nothing!
            NOP();
        }
        else if ( params._singleNodeRenderGUID == -1 )
        {
            // Cull the scene and grab the visible nodes
            I64 ignoreGUID = params._sourceNode == nullptr ? -1 : params._sourceNode->getGUID();

            NodeCullParams cullParams = {};
            Attorney::SceneManagerRenderPass::initDefaultCullValues( _parent.parent().sceneManager(), _stage, cullParams );

            cullParams._clippingPlanes = params._clippingPlanes;
            cullParams._stage = _stage;
            cullParams._minExtents = params._minExtents;
            cullParams._ignoredGUIDS = { &ignoreGUID, 1 };
            cullParams._cameraEyePos = camSnapshot._eye;
            cullParams._frustum = &camera->getFrustum();
            cullParams._cullMaxDistance = std::min( cullParams._cullMaxDistance, camSnapshot._zPlanes.y );
            cullParams._maxLoD = params._maxLoD;

            U16 cullFlags = to_base( CullOptions::DEFAULT_CULL_OPTIONS );
            if ( !TestBit( params._drawMask, to_U8( 1 << to_base( RenderPassParams::Flags::DRAW_DYNAMIC_NODES ) ) ) )
            {
                cullFlags |= to_base( CullOptions::CULL_DYNAMIC_NODES );
            }
            if ( !TestBit( params._drawMask, to_U8( 1 << to_base( RenderPassParams::Flags::DRAW_STATIC_NODES ) ) ) )
            {
                cullFlags |= to_base( CullOptions::CULL_STATIC_NODES );
            }
            if ( TestBit( params._drawMask, to_U8( 1 << to_base( RenderPassParams::Flags::DRAW_SKY_NODES ) ) ) )
            {
                cullFlags |= to_base( CullOptions::KEEP_SKY_NODES );
            }
            Attorney::SceneManagerRenderPass::cullScene( _parent.parent().sceneManager(), cullParams, cullFlags, _visibleNodesCache );
        }
        else
        {
            Attorney::SceneManagerRenderPass::findNode( _parent.parent().sceneManager(), camera->snapshot()._eye, params._singleNodeRenderGUID, _visibleNodesCache );
        }

        constexpr bool doMainPass = true;
        // PrePass requires a depth buffer
        const bool doPrePass = _stage != RenderStage::SHADOW &&
            params._target != INVALID_RENDER_TARGET_ID &&
            target->usesAttachment( RTAttachmentType::DEPTH );
        const bool doOITPass = params._targetOIT != INVALID_RENDER_TARGET_ID;
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
            else
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

        if ( params._layerParams._colourLayers[0] == 0u ||
             params._layerParams._colourLayers[0] == INVALID_LAYER_INDEX )
        {
            // Either the first layer or not specified
            Attorney::SceneManagerRenderPass::prepareLightData( _parent.parent().sceneManager(), _stage, camSnapshot, memCmdInOut );
        }

        GFX::EnqueueCommand( bufferInOut, GFX::SetClipPlanesCommand{ params._clippingPlanes } );

        auto cmd = GFX::EnqueueCommand<GFX::BindShaderResourcesCommand>( bufferInOut );
        cmd->_usage = DescriptorSetUsage::PER_PASS;
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 0u, ShaderStageVisibility::FRAGMENT );
            Set( binding._data, DescriptorCombinedImageSampler{});
        }
        {
            DescriptorSetBinding& binding = AddBinding( cmd->_bindings, 1u, ShaderStageVisibility::FRAGMENT );
            Set( binding._data, DescriptorCombinedImageSampler{} );
        }

        // We prepare all nodes for the MAIN_PASS rendering. PRE_PASS and OIT_PASS are support passes only. Their order and sorting are less important.
        params._stagePass._passType = RenderPassType::MAIN_PASS;
        const size_t visibleNodeCount = prepareNodeData( params, camSnapshot, hasInvalidNodes, doPrePass, doOITPass, bufferInOut, memCmdInOut );

#   pragma region PRE_PASS
        // We need the pass to be PRE_PASS even if we skip the prePass draw stage as it is the default state subsequent operations expect
        params._stagePass._passType = RenderPassType::PRE_PASS;
        if ( doPrePass )
        {
            prePass( params, camSnapshot, bufferInOut );
        }
#   pragma endregion

        if ( _stage == RenderStage::DISPLAY )
        {
            resolveMainScreenTarget( params, true, true, false, bufferInOut );
        }

#   pragma region HI_Z
        if ( doOcclusionPass )
        {
            // This also renders into our HiZ texture that we may want to use later in PostFX
            occlusionPass( playerIdx, camSnapshot, visibleNodeCount, params._stagePass, RenderTargetNames::SCREEN, params._targetHIZ, bufferInOut );
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
        if ( doMainPass )
        {
            mainPass( params, camSnapshot, *target, doPrePass, doOcclusionPass, bufferInOut );
        }
        else
        {
            DIVIDE_UNEXPECTED_CALL();
        }
#   pragma endregion

#   pragma region TRANSPARENCY_PASS
        if ( _stage != RenderStage::SHADOW )
        {
            // If doIOTPass is false, use forward pass shaders (i.e. MAIN_PASS again for transparents)
            if ( doOITPass )
            {
                params._stagePass._passType = RenderPassType::OIT_PASS;
                woitPass( params, camSnapshot, bufferInOut );
            }
            else
            {
                params._stagePass._passType = RenderPassType::TRANSPARENCY_PASS;
                transparencyPass( params, camSnapshot, bufferInOut );
            }
        }
#   pragma endregion

        if ( _stage == RenderStage::DISPLAY || _stage == RenderStage::NODE_PREVIEW )
        {
            GFX::EnqueueCommand( bufferInOut, GFX::PushCameraCommand{ camSnapshot } );

            GFX::BeginRenderPassCommand* beginRenderPassCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>( bufferInOut );
            beginRenderPassCmd->_name = "DO_POST_RENDER_PASS";
            beginRenderPassCmd->_target = params._target;
            SetEnabled( beginRenderPassCmd->_descriptor._drawMask, RTAttachmentType::DEPTH, RTColourAttachmentSlot::SLOT_0, false );

            if ( _stage == RenderStage::DISPLAY )
            {
                SetEnabled( beginRenderPassCmd->_descriptor._drawMask, RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::VELOCITY, false );
                SetEnabled( beginRenderPassCmd->_descriptor._drawMask, RTAttachmentType::COLOUR, GFXDevice::ScreenTargets::NORMALS, false );
                GFX::EnqueueCommand<GFX::BeginDebugScopeCommand>( bufferInOut )->_scopeName = "Debug Draw Pass";
                Attorney::SceneManagerRenderPass::debugDraw( _parent.parent().sceneManager(), bufferInOut );
                GFX::EnqueueCommand<GFX::EndDebugScopeCommand>( bufferInOut );
            }

            if_constexpr( Config::Build::ENABLE_EDITOR )
            {
                Attorney::EditorRenderPassExecutor::postRender( _context.context().editor(), _stage, camSnapshot, params._target, bufferInOut );
            }

            GFX::EnqueueCommand( bufferInOut, GFX::EndRenderPassCommand{} );
            GFX::EnqueueCommand( bufferInOut, GFX::PopCameraCommand{} );

            if ( _stage == RenderStage::DISPLAY )
            {
                resolveMainScreenTarget( params, false, false, true, bufferInOut );
            }
        }

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
                if ( it._hash != INVALID_MAT_HASH )
                {
                    ++it._framesSinceLastUsed;
                }
            }
        }
        {
            PROFILE_SCOPE( "Clear Freelists", Profiler::Category::Scene );
            ScopedLock<Mutex> w_lock( _transformBuffer._lock );
            _transformBuffer._data._freeList.fill( true );
        }

        _materialBuffer._highWaterMark = _transformBuffer._highWaterMark = 0u;
        _cmdBuffer->incQueue();
    }

    U32 RenderPassExecutor::renderQueueSize() const
    {
        return to_U32( _renderQueuePackages.size() );
    }

    void RenderPassExecutor::OnRenderingComponentCreation( RenderingComponent* rComp )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Streaming );

        ScopedLock<Mutex> w_lock( s_indirectionGlobalLock );
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
        ScopedLock<Mutex> w_lock( s_indirectionGlobalLock );
        assert( !s_indirectionFreeList[entry] );
        s_indirectionFreeList[entry] = true;
    }

} //namespace Divide