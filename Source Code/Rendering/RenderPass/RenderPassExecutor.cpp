#include "stdafx.h"

#include "Headers/RenderPassExecutor.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/EngineTaskPool.h"
#include "Geometry/Material/Headers/Material.h"
#include "Managers/Headers/RenderPassManager.h"
#include "Managers/Headers/SceneManager.h"
#include "Platform/Video/Buffers/RenderTarget/Headers/RTAttachment.h"
#include "Platform/Video/Buffers/ShaderBuffer/Headers/ShaderBuffer.h"
#include "Platform/Video/Headers/GenericDrawCommand.h"
#include "Rendering/RenderPass/Headers/RenderQueue.h"
#include "Scenes/Headers/SceneState.h"

#include "ECS/Components/Headers/AnimationComponent.h"
#include "ECS/Components/Headers/BoundsComponent.h"
#include "ECS/Components/Headers/RenderingComponent.h"
#include "ECS/Components/Headers/TransformComponent.h"
#include "Rendering/Headers/Renderer.h"

namespace Divide {
namespace {
    // Remove materials that haven't been indexed in this amount of frames to make space for new ones
    constexpr U16 g_maxMaterialFrameLifetime = 6u;
    // Use to partition parallel jobs
    constexpr U32 g_nodesPerPrepareDrawPartition = 16u;

    void InitMaterialData(const RenderStage stage, RenderPassExecutor::PerRingEntryMaterialData& data) {
        data._nodeMaterialLookupInfo.resize(RenderStagePass::totalPassCountForStage(stage) * Config::MAX_CONCURRENT_MATERIALS, { Material::INVALID_MAT_HASH, 0u });
        data._nodeMaterialData.resize(RenderStagePass::totalPassCountForStage(stage) * Config::MAX_CONCURRENT_MATERIALS);
    }
}

Pipeline* RenderPassExecutor::s_OITCompositionPipeline = nullptr;
Pipeline* RenderPassExecutor::s_OITCompositionMSPipeline = nullptr;
Pipeline* RenderPassExecutor::s_ResolveScreenTargetsPipeline = nullptr;

RenderPassExecutor::RenderPassExecutor(RenderPassManager& parent, GFXDevice& context, const RenderStage stage)
    : _parent(parent)
    , _context(context)
    , _stage(stage)
    , _renderQueue(parent.parent(), stage)
{
    _materialData.resize(_materialData.max_size());
    for (PerRingEntryMaterialData& data : _materialData) {
        InitMaterialData(stage, data);
    }
}

void RenderPassExecutor::postInit(const ShaderProgram_ptr& OITCompositionShader,
                                  const ShaderProgram_ptr& OITCompositionShaderMS,
                                  const ShaderProgram_ptr& ResolveScreenTargetsShaderMS) const {
    PipelineDescriptor pipelineDescriptor;
    pipelineDescriptor._stateHash = _context.get2DStateBlock();

    if (s_OITCompositionPipeline == nullptr) {
        pipelineDescriptor._shaderProgramHandle = OITCompositionShader->getGUID();
        s_OITCompositionPipeline = _context.newPipeline(pipelineDescriptor);
    }
    if (s_OITCompositionMSPipeline == nullptr) {
        pipelineDescriptor._shaderProgramHandle = OITCompositionShaderMS->getGUID();
        s_OITCompositionMSPipeline = _context.newPipeline(pipelineDescriptor);
    }  
    if (s_ResolveScreenTargetsPipeline == nullptr) {
        pipelineDescriptor._shaderProgramHandle = ResolveScreenTargetsShaderMS->getGUID();
        s_ResolveScreenTargetsPipeline = _context.newPipeline(pipelineDescriptor);
    }
}

void RenderPassExecutor::addTexturesAt(const size_t idx, const NodeMaterialTextures& tempTextures) {
    // GL_ARB_bindless_texture:
    // In the following four constructors, the low 32 bits of the sampler
    // type correspond to the .x component of the uvec2 and the high 32 bits
    // correspond to the .y component.
    // uvec2(any sampler type)     // Converts a sampler type to a pair of 32-bit unsigned integers
    // any sampler type(uvec2)     // Converts a pair of 32-bit unsigned integers to a sampler type
    // uvec2(any image type)       // Converts an image type to a pair of 32-bit unsigned integers
    // any image type(uvec2)       // Converts a pair of 32-bit unsigned integers to an image type

    NodeMaterialData& target = _materialData[_materialBufferIndex]._nodeMaterialData[idx];
    for (U8 i = 0; i < MATERIAL_TEXTURE_COUNT; ++i) {
        const SamplerAddress combined = tempTextures[i];
        target._textures[i / 2][(i % 2) * 2 + 0] = to_U32(combined & 0xFFFFFFFF); //low
        target._textures[i / 2][(i % 2) * 2 + 1] = to_U32(combined >> 32); //high
    }

    // second loop for cache reasons. 0u is fine as an address since we filter it at graphics API level.
    for (U8 i = 0; i < MATERIAL_TEXTURE_COUNT; ++i) {
        _uniqueTextureAddresses.insert(tempTextures[i]);
    }
}

/// Prepare the list of visible nodes for rendering
NodeDataIdx RenderPassExecutor::processVisibleNode(const RenderingComponent& rComp,
                                                   const RenderStage stage,
                                                   const D64 interpolationFactor,
                                                   const U32 materialElementOffset,
                                                   U32 nodeIndex) {
    OPTICK_EVENT();

    // Rewrite all transforms
    // ToDo: Cache transforms for static nodes -Ionut
    NodeDataIdx ret = {};
    ret._transformIDX = to_U16(nodeIndex);
    NodeTransformData& transformOut = _nodeTransformData[ret._transformIDX];
    transformOut = {};

    NodeMaterialData tempData{};
    NodeMaterialTextures tempTextures{};
    // Get the colour matrix (base colour, metallic, etc)
    rComp.getMaterialData(tempData, tempTextures);

    // Match materials
    size_t materialHash = HashMaterialData(tempData);
    Util::Hash_combine(materialHash, HashTexturesData(tempTextures));

    PerRingEntryMaterialData& materialData = _materialData[_materialBufferIndex];
    auto& materialInfo = materialData._nodeMaterialLookupInfo;
    // Try and match an existing material
    bool foundMatch = false;
    U16 idx = 0;
    for (; idx < Config::MAX_CONCURRENT_MATERIALS; ++idx) {
        auto& [hash, lifetime] = materialInfo[idx + materialElementOffset];

        if (hash == materialHash) {
            // Increment lifetime (but clamp it so we don't overflow in time)
            lifetime = CLAMPED(++lifetime, to_U16(0u), g_maxMaterialFrameLifetime);
            foundMatch = true;
            break;
        }
    }

    // If we fail, try and find an empty slot
    if (!foundMatch) {
        std::pair<U16, U16> bestCandidate = { 0u, 0u };

        idx = 0u;
        for (; idx < Config::MAX_CONCURRENT_MATERIALS; ++idx) {
            auto& [hash, lifetime] = materialInfo[idx + materialElementOffset];
            if (hash == Material::INVALID_MAT_HASH) {
                foundMatch = true;
                break;
            }
            // Find a candidate to replace in case we fail
            if (lifetime >= g_maxMaterialFrameLifetime && lifetime > bestCandidate.second) {
                bestCandidate.first = idx;
                bestCandidate.second = lifetime;
            }
        }
        if (!foundMatch) {
            foundMatch = bestCandidate.second > 0u;
            idx = bestCandidate.first;
        }

        DIVIDE_ASSERT(foundMatch, "RenderPassExecutor::processVisibleNode error: too many concurrent materials! Increase Config::MAX_CONCURRENT_MATERIALS");

        auto& range = materialData._matUpdateRange;
        if (range._firstIDX > idx) {
            range._firstIDX = idx;
        }
        if (range._lastIDX < idx) {
            range._lastIDX = idx;
        }

        const U32 offsetIdx = idx + materialElementOffset;
        materialInfo[offsetIdx] = { materialHash, 0u };

        materialData._nodeMaterialData[offsetIdx] = tempData;
        addTexturesAt(offsetIdx, tempTextures);
    }

    ret._materialIDX = idx;
 
    const SceneGraphNode* node = rComp.getSGN();
    const TransformComponent* const transform = node->get<TransformComponent>();
    assert(transform != nullptr);

    // ... get the node's world matrix properly interpolated
    transform->getPreviousWorldMatrix(transformOut._prevWorldMatrix);
    if (interpolationFactor < 0.985) {
        transform->getWorldMatrix(interpolationFactor, transformOut._worldMatrix);
    } else {
        transform->getWorldMatrix(transformOut._worldMatrix);
    }

    transformOut._normalMatrixW.set(transformOut._worldMatrix);
    transformOut._normalMatrixW.setRow(3, 0.0f, 0.0f, 0.0f, 1.0f);
    if (!transform->isUniformScaled()) {
        // Non-uniform scaling requires an inverseTranspose to negate
        // scaling contribution but preserve rotation
        transformOut._normalMatrixW.inverseTranspose();
    }

    // Get the material property matrix (alpha test, texture count, texture operation, etc.)
    bool frameTicked = false;
    U8 boneCount = 0u;
    AnimationComponent* animComp = node->get<AnimationComponent>();
    if (animComp && animComp->playAnimations()) {
        boneCount = animComp->boneCount();
        frameTicked = animComp->frameTicked();
    }

    RenderingComponent::NodeRenderingProperties properties = {};
    rComp.getRenderingProperties(stage, properties);

    // Since the normal matrix is 3x3, we can use the extra row and column to store additional data
    const BoundsComponent* const bounds = node->get<BoundsComponent>();
    const vec4<F32> bSphere = bounds->getBoundingSphere().asVec4();
    const vec3<F32> bBoxHalfExtents = bounds->getBoundingBox().getHalfExtent();

    constexpr F32 reserved = 0.0f;
    transformOut._normalMatrixW.setRow(3, vec4<F32>{bSphere.xyz, 0.f});

    transformOut._normalMatrixW.element(0, 3) = to_F32(Util::PACK_UNORM4x8(boneCount, properties._lod, 1u, 1u));
    transformOut._normalMatrixW.element(1, 3) = to_F32(Util::PACK_HALF2x16(bBoxHalfExtents.xy));
    transformOut._normalMatrixW.element(2, 3) = to_F32(Util::PACK_HALF2x16(bBoxHalfExtents.z, bSphere.w));
    transformOut._normalMatrixW.element(3, 3) = properties._nodeFlagValue;

    transformOut._prevWorldMatrix.element(0, 3) = to_F32(Util::PACK_UNORM4x8(frameTicked ? 1.0f : 0.0f,
                                                  properties._occlusionCull ? 1.0f : 0.0f,
                                                  reserved,
                                                  reserved));
    transformOut._prevWorldMatrix.element(1, 3) = reserved;
    transformOut._prevWorldMatrix.element(2, 3) = reserved;
    transformOut._prevWorldMatrix.element(3, 3) = reserved;

    return ret;
}

U16 RenderPassExecutor::buildDrawCommands(const RenderPassParams& params, const bool doPrePass, const bool doOITPass, GFX::CommandBuffer& bufferInOut) {
    OPTICK_EVENT();
    constexpr bool doMainPass = true;

    RenderStagePass stagePass = params._stagePass;
    RenderPass::BufferData bufferData = _parent.getPassForStage(_stage).getBufferData(stagePass);
    ShaderBuffer* cmdBuffer = bufferData._commandBuffer;

    const D64 interpFactor = GFXDevice::FrameInterpolationFactor();

    _drawCommands.clear();
    _materialBufferIndex = bufferData._materialBuffer->queueWriteIndex();
    while (_materialBufferIndex >= _materialData.size()) {
        InitMaterialData(_stage, _materialData.emplace_back());
    }

    _materialData[_materialBufferIndex]._matUpdateRange.reset();
    _uniqueTextureAddresses.clear();

    for (RenderBin::SortedQueue& sQueue : _sortedQueues) {
        sQueue.resize(0);
        sQueue.reserve(Config::MAX_VISIBLE_NODES);
    }

    const U16 queueTotalSize = _renderQueue.getSortedQueues({}, _sortedQueues);

    for (RenderBin::SortedQueue& queue : _sortedQueues) {
        erase_if(queue, [&stagePass](RenderingComponent* rComp) {
            return !Attorney::RenderingCompRenderPass::hasDrawCommands(*rComp, stagePass);
        });
    }

    U32& nodeCount = *bufferData._lastNodeCount;
    nodeCount = 0u;
    for (RenderBin::SortedQueue& queue : _sortedQueues) {
        for (RenderingComponent* rComp : queue) {
            const NodeDataIdx newDataIdx = processVisibleNode(*rComp, stagePass._stage, interpFactor, bufferData._materialElementOffset, nodeCount++);
            Attorney::RenderingCompRenderPass::setCommandDataIndex(*rComp, stagePass._stage, newDataIdx);
        }
    }

    const U32 cmdOffset = (cmdBuffer->queueWriteIndex() * cmdBuffer->getPrimitiveCount()) + bufferData._commandElementOffset;
    const auto retrieveCommands = [&]() {
        for (RenderBin::SortedQueue& queue : _sortedQueues) {
            for (RenderingComponent* rComp : queue) {
                Attorney::RenderingCompRenderPass::retrieveDrawCommands(*rComp, stagePass, cmdOffset, _drawCommands);
            }
        }
    };

    const RenderPassType prevType = stagePass._passType;
    if (doPrePass) {
        stagePass._passType = RenderPassType::PRE_PASS;
        retrieveCommands();
    }
    if (doMainPass) {
        stagePass._passType = RenderPassType::MAIN_PASS;
        retrieveCommands();
    }
    if (doOITPass) {
        stagePass._passType = RenderPassType::OIT_PASS;
        retrieveCommands();
    }
    stagePass._passType = prevType;

    *bufferData._lastCommandCount = to_U32(_drawCommands.size());

    {
        OPTICK_EVENT("RenderPassExecutor::buildBufferData - UpdateBuffers");

        cmdBuffer->writeData(bufferData._commandElementOffset, *bufferData._lastCommandCount, _drawCommands.data());

        bufferData._transformBuffer->writeData(bufferData._transformElementOffset, nodeCount, _nodeTransformData.data());

        // Copy the same data to the entire ring buffer
        PerRingEntryMaterialData& materialData = _materialData[_materialBufferIndex];
        MaterialUpdateRange& crtRange = materialData._matUpdateRange;

        if (crtRange.range() > 0u) {
            const U32 offsetIDX = bufferData._materialElementOffset + crtRange._firstIDX;
            bufferData._materialBuffer->writeData(offsetIDX, crtRange.range(), &materialData._nodeMaterialData[offsetIDX]);
        }
        crtRange.reset();
    }

    ShaderBufferBinding cmdBufferBinding = {};
    cmdBufferBinding._binding = ShaderBufferLocation::CMD_BUFFER;
    cmdBufferBinding._buffer = cmdBuffer;
    cmdBufferBinding._elementRange = { 0u, cmdBuffer->getPrimitiveCount() };

    ShaderBufferBinding transformBufferBinding = {};
    transformBufferBinding._binding = ShaderBufferLocation::NODE_TRANSFORM_DATA;
    transformBufferBinding._buffer = bufferData._transformBuffer;
    transformBufferBinding._elementRange = { bufferData._transformElementOffset, *bufferData._lastNodeCount };

    ShaderBufferBinding materialBufferBinding = {};
    materialBufferBinding._binding = ShaderBufferLocation::NODE_MATERIAL_DATA;
    materialBufferBinding._buffer = bufferData._materialBuffer;
    materialBufferBinding._elementRange = { bufferData._materialElementOffset, Config::MAX_CONCURRENT_MATERIALS };

    GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
    descriptorSetCmd._set._buffers.add(cmdBufferBinding);
    descriptorSetCmd._set._buffers.add(transformBufferBinding);
    descriptorSetCmd._set._buffers.add(materialBufferBinding);
    EnqueueCommand(bufferInOut, descriptorSetCmd);

    if (_uniqueTextureAddresses.size() > 0) {
        GFX::SetTexturesResidencyCommand residencyCmd = {};
        residencyCmd._addresses = _uniqueTextureAddresses;
        residencyCmd._state = true;
        EnqueueCommand(bufferInOut, residencyCmd);
    }

    return queueTotalSize;
}

U16 RenderPassExecutor::prepareNodeData(VisibleNodeList<>& nodes, const RenderPassParams& params, const bool hasInvalidNodes, const bool doPrePass, const bool doOITPass, GFX::CommandBuffer& bufferInOut) {

    if (hasInvalidNodes) {
        VisibleNodeList<> tempNodes{};
        for (size_t i = 0; i < nodes.size(); ++i) {
            const VisibleNode& node = nodes.node(i);
            if (node._materialReady) {
                tempNodes.append(node);
            }
        }
        nodes.reset();
        nodes.append(tempNodes);
    }

    const RenderStagePass& stagePass = params._stagePass;
    const SceneRenderState& sceneRenderState = _parent.parent().sceneManager()->getActiveScene().renderState();
    const Camera& cam = *params._camera;

    _renderQueue.refresh();

    ParallelForDescriptor descriptor = {};
    descriptor._iterCount = to_U32(nodes.size());
    descriptor._cbk = [&](const Task* /*parentTask*/, const U32 start, const U32 end) {
        for (U32 i = start; i < end; ++i) {
            VisibleNode& node = nodes.node(i);
            assert(node._materialReady);
            RenderingComponent * rComp = node._node->get<RenderingComponent>();
            Attorney::RenderingCompRenderPass::prepareDrawPackage(*rComp, cam, sceneRenderState, stagePass, true);
            _renderQueue.addNodeToQueue(node._node, stagePass, node._distanceToCameraSq);
        }
    };

    if (descriptor._iterCount < g_nodesPerPrepareDrawPartition) {
        descriptor._cbk(nullptr, 0, descriptor._iterCount);
    } else {
        descriptor._partitionSize = g_nodesPerPrepareDrawPartition;
        descriptor._priority = TaskPriority::DONT_CARE;
        descriptor._useCurrentThread = true;
        parallel_for(_parent.parent().platformContext(), descriptor);
    }

    _renderQueue.sort(stagePass);

    _renderQueuePackages.resize(0);
    _renderQueuePackages.reserve(Config::MAX_VISIBLE_NODES);

    // Draw everything in the depth pass but only draw stuff from the translucent bin in the OIT Pass and everything else in the colour pass
    _renderQueue.populateRenderQueues(stagePass, std::make_pair(RenderBinType::RBT_COUNT, true), _renderQueuePackages);

    return buildDrawCommands(params, doPrePass, doOITPass, bufferInOut);
}

void RenderPassExecutor::prepareRenderQueues(const RenderPassParams& params, const VisibleNodeList<>& nodes, bool transparencyPass, const RenderingOrder renderOrder) {
    OPTICK_EVENT();

    const RenderStagePass& stagePass = params._stagePass;
    const RenderBinType targetBin = transparencyPass ? RenderBinType::RBT_TRANSLUCENT : RenderBinType::RBT_COUNT;
    const SceneRenderState& sceneRenderState = _parent.parent().sceneManager()->getActiveScene().renderState();

    const Camera& cam = *params._camera;
    _renderQueue.refresh(targetBin);

    ParallelForDescriptor descriptor = {};
    descriptor._iterCount = to_U32(nodes.size());
    descriptor._partitionSize = g_nodesPerPrepareDrawPartition;
    descriptor._priority = TaskPriority::DONT_CARE;
    descriptor._useCurrentThread = true;
    descriptor._cbk = [&](const Task* /*parentTask*/, const U32 start, const U32 end) {
        for (U32 i = start; i < end; ++i) {
            const VisibleNode& node = nodes.node(i);
            assert(node._materialReady);
            if (Attorney::SceneGraphNodeRenderPassManager::shouldDraw(node._node, stagePass)) {
                RenderingComponent * rComp = nodes.node(i)._node->get<RenderingComponent>();
                Attorney::RenderingCompRenderPass::prepareDrawPackage(*rComp, cam, sceneRenderState, stagePass, false);
                _renderQueue.addNodeToQueue(node._node, stagePass, node._distanceToCameraSq, targetBin);
            }
        }
    };

    parallel_for(_parent.parent().platformContext(), descriptor);

    // Sort all bins
    _renderQueue.sort(stagePass, targetBin, renderOrder);

    _renderQueuePackages.resize(0);
    _renderQueuePackages.reserve(Config::MAX_VISIBLE_NODES);

    // Draw everything in the depth pass but only draw stuff from the translucent bin in the OIT Pass and everything else in the colour pass
    _renderQueue.populateRenderQueues(stagePass, stagePass.isDepthPass()
                                                   ? std::make_pair(RenderBinType::RBT_COUNT, true)
                                                   : std::make_pair(RenderBinType::RBT_TRANSLUCENT, transparencyPass),
                                      _renderQueuePackages);

    
    for (RenderBin::SortedQueue& sQueue : _sortedQueues) {
        sQueue.resize(0);
        sQueue.reserve(Config::MAX_VISIBLE_NODES);
    }

    static const vectorEASTL<RenderBinType> allBins{};
    static const vectorEASTL<RenderBinType> prePassBins{
         RenderBinType::RBT_OPAQUE,
         RenderBinType::RBT_IMPOSTOR,
         RenderBinType::RBT_TERRAIN,
         RenderBinType::RBT_TERRAIN_AUX,
         RenderBinType::RBT_SKY,
         RenderBinType::RBT_TRANSLUCENT
    };

    _renderQueue.getSortedQueues(stagePass._passType == RenderPassType::PRE_PASS ? prePassBins : allBins, _sortedQueues);
}

void RenderPassExecutor::prePass(const VisibleNodeList<>& nodes, const RenderPassParams& params, GFX::CommandBuffer& bufferInOut) {
    OPTICK_EVENT();

    assert(params._stagePass._passType == RenderPassType::PRE_PASS);

    EnqueueCommand(bufferInOut, GFX::BeginDebugScopeCommand{ " - PrePass" });

    prepareRenderQueues(params, nodes, false);

    const bool layeredRendering = params._layerParams._layer > 0;

    GFX::BeginRenderPassCommand beginRenderPassCommand = {};
    beginRenderPassCommand._target = params._target;
    beginRenderPassCommand._descriptor = params._targetDescriptorPrePass;
    beginRenderPassCommand._name = "DO_PRE_PASS";
    EnqueueCommand(bufferInOut, beginRenderPassCommand);

    if (layeredRendering) {
        GFX::BeginRenderSubPassCommand beginRenderSubPassCmd = {};
        beginRenderSubPassCmd._writeLayers.push_back(params._layerParams);
        EnqueueCommand(bufferInOut, beginRenderSubPassCmd);
    }

    renderQueueToSubPasses(bufferInOut);

    postRender(params._stagePass, *params._camera, _renderQueue, bufferInOut);

    if (layeredRendering) {
        EnqueueCommand(bufferInOut, GFX::EndRenderSubPassCommand{});
    }

    EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});

    EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
}

void RenderPassExecutor::occlusionPass(const VisibleNodeList<>& nodes,
                                       const U32 visibleNodeCount,
                                       const RenderStagePass& stagePass,
                                       const Camera& camera,
                                       const RenderTargetID& sourceDepthBuffer,
                                       const RenderTargetID& targetDepthBuffer,
                                       GFX::CommandBuffer& bufferInOut) const {
    OPTICK_EVENT();

    //ToDo: Find a way to skip occlusion culling for low number of nodes in view but also keep light culling up and running -Ionut
    ACKNOWLEDGE_UNUSED(visibleNodeCount);
    assert(stagePass._passType == RenderPassType::PRE_PASS);

    EnqueueCommand(bufferInOut, GFX::BeginDebugScopeCommand{ "HiZ Construct & Cull" });

    // Update HiZ Target
    const auto [hizTexture, hizSampler] = _context.constructHIZ(sourceDepthBuffer, targetDepthBuffer, bufferInOut);

    // ToDo: This should not be needed as we unbind the render target before we dispatch the compute task anyway. See if we can remove this -Ionut
    GFX::MemoryBarrierCommand memCmd = {};
    memCmd._barrierMask = to_base(MemoryBarrierType::RENDER_TARGET) |
                          to_base(MemoryBarrierType::TEXTURE_FETCH) |
                          to_base(MemoryBarrierType::TEXTURE_BARRIER);
    EnqueueCommand(bufferInOut, memCmd);

    // Run occlusion culling CS
    RenderPass::BufferData bufferData = _parent.getPassForStage(_stage).getBufferData(stagePass);

    GFX::SendPushConstantsCommand HIZPushConstantsCMDInOut = {};
    _context.occlusionCull(stagePass, bufferData, hizTexture, hizSampler, HIZPushConstantsCMDInOut, bufferInOut);

    EnqueueCommand(bufferInOut, GFX::BeginDebugScopeCommand{ "Per-node HiZ Cull" });
    const size_t nodeCount = nodes.size();
    for (size_t i = 0; i < nodeCount; ++i) {
        const VisibleNode& node = nodes.node(i);
        Attorney::SceneGraphNodeRenderPassManager::occlusionCullNode(node._node, stagePass, hizTexture, camera, HIZPushConstantsCMDInOut, bufferInOut);
    }
    EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});

    // Occlusion culling barrier
    memCmd._barrierMask = to_base(MemoryBarrierType::COMMAND_BUFFER) | //For rendering
                          to_base(MemoryBarrierType::SHADER_STORAGE) | //For updating later on
                          (bufferData._cullCounterBuffer != nullptr ? to_base(MemoryBarrierType::ATOMIC_COUNTER) : 0u);
    EnqueueCommand(bufferInOut, memCmd);

    if (bufferData._cullCounterBuffer != nullptr) {
        _context.updateCullCount(bufferData, bufferInOut);

        bufferData._cullCounterBuffer->incQueue();

        GFX::ClearBufferDataCommand clearAtomicCounter = {};
        clearAtomicCounter._buffer = bufferData._cullCounterBuffer;
        clearAtomicCounter._offsetElementCount = 0;
        clearAtomicCounter._elementCount = 1;
        EnqueueCommand(bufferInOut, clearAtomicCounter);
    }

    EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
}

void RenderPassExecutor::mainPass(const VisibleNodeList<>& nodes, const RenderPassParams& params, RenderTarget& target, const bool prePassExecuted, const bool hasHiZ, GFX::CommandBuffer& bufferInOut) {
    OPTICK_EVENT();

    const RenderStagePass& stagePass = params._stagePass;
    assert(stagePass._passType == RenderPassType::MAIN_PASS);

    EnqueueCommand(bufferInOut, GFX::BeginDebugScopeCommand{ " - MainPass" });

    prepareRenderQueues(params, nodes, false);

    if (params._target._usage != RenderTargetUsage::COUNT) {
        LightPool& activeLightPool = Attorney::SceneManagerRenderPass::lightPool(_parent.parent().sceneManager());

        Texture_ptr hizTex = nullptr;
        size_t hizSampler = 0;
        if (hasHiZ) {
            const RenderTarget& hizTarget = _context.renderTargetPool().renderTarget(params._targetHIZ);
            const auto& hizAtt = hizTarget.getAttachment(RTAttachmentType::Depth, 0);
            hizTex = hizAtt.texture();
            hizSampler = hizAtt.samplerHash();
        }

        const RenderTarget& nonMSTarget = _context.renderTargetPool().renderTarget(RenderTargetUsage::SCREEN);

        _context.getRenderer().preRender(stagePass, hizTex, hizSampler, activeLightPool, params._camera, bufferInOut);

        if (hasHiZ) {
            GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
            descriptorSetCmd._set._textureData.add({ hizTex->data(), hizSampler, TextureUsage::DEPTH });
            EnqueueCommand(bufferInOut, descriptorSetCmd);
        } else if (prePassExecuted) {
            GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
            if (params._target._usage == RenderTargetUsage::SCREEN_MS) {
                const auto& depthAtt = nonMSTarget.getAttachment(RTAttachmentType::Depth, 0);
                descriptorSetCmd._set._textureData.add({ depthAtt.texture()->data(), depthAtt.samplerHash(), TextureUsage::DEPTH });
            } else {
                const auto& depthAtt = target.getAttachment(RTAttachmentType::Depth, 0);
                descriptorSetCmd._set._textureData.add({ depthAtt.texture()->data(), depthAtt.samplerHash(), TextureUsage::DEPTH });
            }
            EnqueueCommand(bufferInOut, descriptorSetCmd);
        }

        const bool layeredRendering = params._layerParams._layer > 0;

        GFX::BeginRenderPassCommand beginRenderPassCommand = {};
        beginRenderPassCommand._target = params._target;
        beginRenderPassCommand._descriptor = params._targetDescriptorMainPass;
        beginRenderPassCommand._name = "DO_MAIN_PASS";
        EnqueueCommand(bufferInOut, beginRenderPassCommand);

        if (layeredRendering) {
            GFX::BeginRenderSubPassCommand beginRenderSubPassCmd = {};
            beginRenderSubPassCmd._writeLayers.push_back(params._layerParams);
            EnqueueCommand(bufferInOut, beginRenderSubPassCmd);
        }

        // We try and render translucent items in the shadow pass and due some alpha-discard tricks
        renderQueueToSubPasses(bufferInOut);

        postRender(params._stagePass, *params._camera, _renderQueue, bufferInOut);

        if (layeredRendering) {
            EnqueueCommand(bufferInOut, GFX::EndRenderSubPassCommand{});
        }

        EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
    }

    EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
}

void RenderPassExecutor::woitPass(const VisibleNodeList<>& nodes, const RenderPassParams& params, GFX::CommandBuffer& bufferInOut) {
    const bool isMSAATarget = params._targetOIT._usage == RenderTargetUsage::OIT_MS;

    assert(params._stagePass._passType == RenderPassType::OIT_PASS);

    prepareRenderQueues(params, nodes, true);

    EnqueueCommand(bufferInOut, GFX::BeginDebugScopeCommand{ " - W-OIT Pass" });

    if (renderQueueSize() > 0) {
        GFX::ClearRenderTargetCommand clearRTCmd = {};
        clearRTCmd._target = params._targetOIT;
        if_constexpr(Config::USE_COLOURED_WOIT) {
            // Don't clear our screen target. That would be BAD.
            clearRTCmd._descriptor.clearColour(to_U8(GFXDevice::ScreenTargets::MODULATE), false);
        }
        // Don't clear and don't write to depth buffer
        clearRTCmd._descriptor.clearDepth(false);
        EnqueueCommand(bufferInOut, clearRTCmd);

        // Step1: Draw translucent items into the accumulation and revealage buffers
        GFX::BeginRenderPassCommand beginRenderPassOitCmd = {};
        beginRenderPassOitCmd._name = "DO_OIT_PASS_1";
        beginRenderPassOitCmd._target = params._targetOIT;
        beginRenderPassOitCmd._descriptor.drawMask().setEnabled(RTAttachmentType::Depth, 0, false);
        //beginRenderPassOitCmd._descriptor.alphaToCoverage(true);
        EnqueueCommand(bufferInOut, beginRenderPassOitCmd);

        GFX::SetBlendStateCommand setBlendStateCmd = {};
        {
            RTBlendState& state0 = setBlendStateCmd._blendStates[to_U8(GFXDevice::ScreenTargets::ACCUMULATION)];
            state0._blendProperties._enabled = true;
            state0._blendProperties._blendSrc = BlendProperty::ONE;
            state0._blendProperties._blendDest = BlendProperty::ONE;
            state0._blendProperties._blendOp = BlendOperation::ADD;

            RTBlendState& state1 = setBlendStateCmd._blendStates[to_U8(GFXDevice::ScreenTargets::REVEALAGE)];
            state1._blendProperties._enabled = true;
            state1._blendProperties._blendSrc = BlendProperty::ZERO;
            state1._blendProperties._blendDest = BlendProperty::INV_SRC_COLOR;
            state1._blendProperties._blendOp = BlendOperation::ADD;

            RTBlendState& state2 = setBlendStateCmd._blendStates[to_U8(GFXDevice::ScreenTargets::NORMALS_AND_MATERIAL_PROPERTIES)];
            state2._blendProperties._enabled = true; 
            state2._blendProperties._blendOp = BlendOperation::MAX;
            state2._blendProperties._blendOpAlpha = BlendOperation::MAX;

            RTBlendState& state3 = setBlendStateCmd._blendStates[to_U8(GFXDevice::ScreenTargets::SPECULAR)];
            state3._blendProperties._enabled = true;
            state3._blendProperties._blendOp = BlendOperation::MAX;
            state3._blendProperties._blendOpAlpha = BlendOperation::MAX;

            if_constexpr(Config::USE_COLOURED_WOIT) {
                RTBlendState& state4 = setBlendStateCmd._blendStates[to_U8(GFXDevice::ScreenTargets::MODULATE)];
                state4._blendProperties._enabled = true;
                state4._blendProperties._blendSrc = BlendProperty::ONE;
                state4._blendProperties._blendDest = BlendProperty::ONE;
                state4._blendProperties._blendOp = BlendOperation::ADD;
            }
        }
        EnqueueCommand(bufferInOut, setBlendStateCmd);

        {
            const RenderTarget& nonMSTarget = _context.renderTargetPool().renderTarget(RenderTargetUsage::SCREEN);
            GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
            const auto& colourAtt = nonMSTarget.getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::ALBEDO));
            descriptorSetCmd._set._textureData.add({ colourAtt.texture()->data(), colourAtt.samplerHash(), TextureUsage::POST_FX_DATA });
            EnqueueCommand(bufferInOut, descriptorSetCmd);
        }

        renderQueueToSubPasses(bufferInOut/*, quality*/);

        postRender(params._stagePass, *params._camera, _renderQueue, bufferInOut);

        // Reset blend states
        setBlendStateCmd._blendStates = {};
        EnqueueCommand(bufferInOut, setBlendStateCmd);

        GFX::EndRenderPassCommand endRenderPassCmd = {};
        endRenderPassCmd._setDefaultRTState = isMSAATarget; // We're gonna do a new bind soon enough
        EnqueueCommand(bufferInOut, endRenderPassCmd);

        const bool useMSAA = params._target == RenderTargetUsage::SCREEN_MS;

        // Step2: Composition pass
        // Don't clear depth & colours and do not write to the depth buffer
        EnqueueCommand(bufferInOut, GFX::SetCameraCommand{ Camera::utilityCamera(Camera::UtilityCamera::_2D)->snapshot() });

        const bool layeredRendering = params._layerParams._layer > 0;
        GFX::BeginRenderPassCommand beginRenderPassCompCmd = {};
        beginRenderPassCompCmd._name = "DO_OIT_PASS_2";
        beginRenderPassCompCmd._target = params._target;
        beginRenderPassCompCmd._descriptor = params._targetDescriptorComposition;
        EnqueueCommand(bufferInOut, beginRenderPassCompCmd);

        if (layeredRendering) {
            GFX::BeginRenderSubPassCommand beginRenderSubPassCmd = {};
            beginRenderSubPassCmd._writeLayers.push_back(params._layerParams);
            EnqueueCommand(bufferInOut, beginRenderSubPassCmd);
        }

        setBlendStateCmd = {};
        {
            RTBlendState& state0 = setBlendStateCmd._blendStates[to_U8(GFXDevice::ScreenTargets::ALBEDO)];
            state0._blendProperties._enabled = true;
            state0._blendProperties._blendOp = BlendOperation::ADD;
            if_constexpr(Config::USE_COLOURED_WOIT) {
                state0._blendProperties._blendSrc = BlendProperty::INV_SRC_ALPHA;
                state0._blendProperties._blendDest = BlendProperty::ONE;
            } else {
                state0._blendProperties._blendSrc = BlendProperty::SRC_ALPHA;
                state0._blendProperties._blendDest = BlendProperty::INV_SRC_ALPHA;
            }
        }
        EnqueueCommand(bufferInOut, setBlendStateCmd);

        EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ useMSAA ? s_OITCompositionMSPipeline : s_OITCompositionPipeline });

        RenderTarget& oitRT = _context.renderTargetPool().renderTarget(params._targetOIT);
        const auto& accumAtt = oitRT.getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::ACCUMULATION));
        const auto& revAtt = oitRT.getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::REVEALAGE));

        const TextureData accum = accumAtt.texture()->data();
        const TextureData revealage = revAtt.texture()->data();

        GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
        descriptorSetCmd._set._textureData.add({ accum, accumAtt.samplerHash(),to_base(TextureUsage::UNIT0) });
        descriptorSetCmd._set._textureData.add({ revealage, revAtt.samplerHash(), to_base(TextureUsage::UNIT1) });
        EnqueueCommand(bufferInOut, descriptorSetCmd);

        GenericDrawCommand drawCommand = {};
        drawCommand._primitiveType = PrimitiveType::TRIANGLES;

        EnqueueCommand(bufferInOut, GFX::DrawCommand{ drawCommand });

        // Reset blend states
        setBlendStateCmd._blendStates = {};
        EnqueueCommand(bufferInOut, setBlendStateCmd);

        if (layeredRendering) {
            EnqueueCommand(bufferInOut, GFX::EndRenderSubPassCommand{});
        }

        EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
    }

    EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
}

void RenderPassExecutor::transparencyPass(const VisibleNodeList<>& nodes, const RenderPassParams& params, GFX::CommandBuffer& bufferInOut) {
    OPTICK_EVENT();
    if (_stage == RenderStage::SHADOW) {
        return;
    }

    if (params._stagePass._passType == RenderPassType::OIT_PASS) {
        woitPass(nodes, params, bufferInOut);
    } else {
        assert(params._stagePass._passType == RenderPassType::MAIN_PASS);

        //Grab all transparent geometry
        prepareRenderQueues(params, nodes, true, RenderingOrder::BACK_TO_FRONT);

        EnqueueCommand(bufferInOut, GFX::BeginDebugScopeCommand{ " - Transparency Pass" });

        if (renderQueueSize() > 0) {
            const bool layeredRendering = params._layerParams._layer > 0;

            GFX::BeginRenderPassCommand beginRenderPassTransparentCmd = {};
            beginRenderPassTransparentCmd._name = "DO_TRANSPARENCY_PASS";
            beginRenderPassTransparentCmd._target = params._target;
            beginRenderPassTransparentCmd._descriptor.drawMask().setEnabled(RTAttachmentType::Depth, 0, false);
            EnqueueCommand(bufferInOut, beginRenderPassTransparentCmd);

            if (layeredRendering) {
                GFX::BeginRenderSubPassCommand beginRenderSubPassCmd = {};
                beginRenderSubPassCmd._writeLayers.push_back(params._layerParams);
                EnqueueCommand(bufferInOut, beginRenderSubPassCmd);
            }

            GFX::SetBlendStateCommand setBlendStateCmd = {};
            RTBlendState& state0 = setBlendStateCmd._blendStates[to_U8(GFXDevice::ScreenTargets::ALBEDO)];
            state0._blendProperties._enabled = true;
            state0._blendProperties._blendSrc = BlendProperty::SRC_ALPHA;
            state0._blendProperties._blendDest = BlendProperty::INV_SRC_ALPHA;
            state0._blendProperties._blendOp = BlendOperation::ADD;

            renderQueueToSubPasses(bufferInOut/*, quality*/);

            postRender(params._stagePass, *params._camera, _renderQueue, bufferInOut);

            // Reset blend states
            setBlendStateCmd._blendStates = {};
            EnqueueCommand(bufferInOut, setBlendStateCmd);

            if (layeredRendering) {
                EnqueueCommand(bufferInOut, GFX::EndRenderSubPassCommand{});
            }

            EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});

            resolveMainScreenTarget(params, bufferInOut);
        }

        EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
    }
}

void RenderPassExecutor::postRender(const RenderStagePass& stagePass,
                                    const Camera& camera,
                                    RenderQueue& renderQueue,
                                    GFX::CommandBuffer& bufferInOut) const {
    SceneManager* sceneManager = _parent.parent().sceneManager();
    const SceneRenderState& activeSceneRenderState = Attorney::SceneManagerRenderPass::renderState(sceneManager);
    renderQueue.postRender(activeSceneRenderState, stagePass, bufferInOut);

    if (stagePass._stage == RenderStage::DISPLAY && stagePass._passType == RenderPassType::MAIN_PASS) {
        /// These should be OIT rendered as well since things like debug nav meshes have translucency
        Attorney::SceneManagerRenderPass::debugDraw(sceneManager, stagePass, &camera, bufferInOut);
    }
}

void RenderPassExecutor::resolveMainScreenTarget(const RenderPassParams& params, GFX::CommandBuffer& bufferInOut) const {
    // If we rendered to the multisampled screen target, we can now copy the colour to our regular buffer as we are done with it at this point
    if (params._target._usage == RenderTargetUsage::SCREEN_MS) {
        EnqueueCommand(bufferInOut, GFX::BeginDebugScopeCommand{ " - Resolve Screen Targets" });

        const RenderTarget& MSSource = _context.renderTargetPool().renderTarget(params._target);
        const auto& albedoAtt = MSSource.getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::ALBEDO));
        const auto& velocityAtt = MSSource.getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::VELOCITY));
        const auto& normalsAtt = MSSource.getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::NORMALS_AND_MATERIAL_PROPERTIES));
        const auto& specularAtt = MSSource.getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::SPECULAR));

        const TextureData albedoTex = albedoAtt.texture()->data();
        const TextureData velocityTex = velocityAtt.texture()->data();
        const TextureData normalsTex = normalsAtt.texture()->data();
        const TextureData specularTex = specularAtt.texture()->data();

        GFX::BeginRenderPassCommand beginRenderPassCommand = {};
        beginRenderPassCommand._target = { RenderTargetUsage::SCREEN, 0u };
        beginRenderPassCommand._descriptor.drawMask().setEnabled(RTAttachmentType::Depth, 0, false);
        beginRenderPassCommand._name = "RESOLVE_MAIN_PASS";
        EnqueueCommand(bufferInOut, beginRenderPassCommand);

        EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ s_ResolveScreenTargetsPipeline });

        GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
        descriptorSetCmd._set._textureData.add({ albedoTex, albedoAtt.samplerHash(), to_base(TextureUsage::UNIT0) });
        descriptorSetCmd._set._textureData.add({ velocityTex, velocityAtt.samplerHash(), to_base(TextureUsage::NORMALMAP) });
        descriptorSetCmd._set._textureData.add({ normalsTex, normalsAtt.samplerHash(), to_base(TextureUsage::HEIGHTMAP) });
        descriptorSetCmd._set._textureData.add({ specularTex, specularAtt.samplerHash(), to_base(TextureUsage::OPACITY) });
        EnqueueCommand(bufferInOut, descriptorSetCmd);

        GenericDrawCommand drawCommand = {};
        drawCommand._primitiveType = PrimitiveType::TRIANGLES;

        EnqueueCommand(bufferInOut, GFX::DrawCommand{ drawCommand });

        EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});

        EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
    }
}

void RenderPassExecutor::doCustomPass(RenderPassParams params, GFX::CommandBuffer& bufferInOut) {
    OPTICK_EVENT();

    assert(params._stagePass._stage == _stage);

    params._camera->updateLookAt();
    const CameraSnapshot& camSnapshot = params._camera->snapshot();

    EnqueueCommand(bufferInOut, 
                   GFX::BeginDebugScopeCommand{
                       Util::StringFormat("Custom pass ( %s - %s )", TypeUtil::RenderStageToString(_stage), params._passName.empty() ? "N/A" : params._passName.c_str()).c_str()
                   });

    const bool layeredRendering = params._layerParams._layer > 0;
    if (!layeredRendering) {
        Attorney::SceneManagerRenderPass::prepareLightData(_parent.parent().sceneManager(), _stage, camSnapshot._eye, camSnapshot._viewMatrix);
    }

    // Tell the Rendering API to draw from our desired PoV
    EnqueueCommand(bufferInOut, GFX::SetCameraCommand{ camSnapshot });
    EnqueueCommand(bufferInOut, GFX::SetClipPlanesCommand{ params._clippingPlanes });

    RenderTarget& target = _context.renderTargetPool().renderTarget(params._target);

    // Cull the scene and grab the visible nodes
    I64 ignoreGUID = params._sourceNode == nullptr ? -1 : params._sourceNode->getGUID();

    NodeCullParams cullParams = {};
    Attorney::SceneManagerRenderPass::initDefaultCullValues(_parent.parent().sceneManager(), _stage, cullParams);

    cullParams._clippingPlanes = params._clippingPlanes;
    cullParams._stage = _stage;
    cullParams._minExtents = params._minExtents;
    cullParams._ignoredGUIDS = { &ignoreGUID, 1 };
    cullParams._currentCamera = params._camera;
    cullParams._cullMaxDistanceSq = std::min(cullParams._cullMaxDistanceSq, SQUARED(params._camera->getZPlanes().y));
    cullParams._maxLoD = params._maxLoD;
    cullParams._cullAllDynamicNodes = !BitCompare(params._drawMask, to_U8(1 << to_base(RenderPassParams::Flags::DRAW_DYNAMIC_NODES)));
    cullParams._cullAllStaticNodes = !BitCompare(params._drawMask, to_U8(1 << to_base(RenderPassParams::Flags::DRAW_STATIC_NODES)));

    VisibleNodeList<>& visibleNodes = Attorney::SceneManagerRenderPass::cullScene(_parent.parent().sceneManager(), cullParams);

    if (params._feedBackContainer != nullptr) {
        auto& container = params._feedBackContainer->_visibleNodes;
        container.resize(visibleNodes.size());
        std::memcpy(container.data(), visibleNodes.data(), visibleNodes.size() * sizeof(VisibleNode));
    }

    constexpr bool doMainPass = true;
    // PrePass requires a depth buffer
    const bool doPrePass = _stage != RenderStage::SHADOW &&
                           params._target._usage != RenderTargetUsage::COUNT &&
                           target.getAttachment(RTAttachmentType::Depth, 0).used();
    const bool doOITPass = params._targetOIT._usage != RenderTargetUsage::COUNT;
    const bool doOcclusionPass = doPrePass && params._targetHIZ._usage != RenderTargetUsage::COUNT;
    bool hasInvalidNodes = false;
    {
        const auto ValidateNodesForStagePass = [&visibleNodes, &hasInvalidNodes](const RenderStagePass& stagePass) {
            const I32 nodeCount = to_I32(visibleNodes.size());
            for (I32 i = nodeCount - 1; i >= 0; i--) {
                VisibleNode& node = visibleNodes.node(i);
                if (node._materialReady && !Attorney::SceneGraphNodeRenderPassManager::canDraw(node._node, stagePass)) {
                    node._materialReady = false;
                    hasInvalidNodes = true;
                }
            }
        };

        if (doPrePass) {
            params._stagePass._passType = RenderPassType::PRE_PASS;
            ValidateNodesForStagePass(params._stagePass);
        }
        if (doMainPass) {
            params._stagePass._passType = RenderPassType::MAIN_PASS;
            ValidateNodesForStagePass(params._stagePass);
        }
        if (doOITPass) {
            params._stagePass._passType = RenderPassType::OIT_PASS;
            ValidateNodesForStagePass(params._stagePass);
        }
    }

    // We prepare all nodes for the MAIN_PASS rendering. PRE_PASS and OIT_PASS are support passes only. Their order and sorting are less important.
    params._stagePass._passType = RenderPassType::MAIN_PASS;
    const U32 visibleNodeCount = prepareNodeData(visibleNodes, params, hasInvalidNodes, doPrePass, doOITPass, bufferInOut);

#   pragma region PRE_PASS
    // We need the pass to be PRE_PASS even if we skip the prePass draw stage as it is the default state subsequent operations expect
    params._stagePass._passType = RenderPassType::PRE_PASS;
    if (doPrePass) {
        prePass(visibleNodes, params, bufferInOut);
    }
#   pragma endregion

    RenderTargetID sourceID = params._target;
    if (params._target._usage == RenderTargetUsage::SCREEN_MS) {
        // If we rendered to the multisampled screen target, we can now blit the depth buffer to our resolved target
        const RenderTargetID targetID = { RenderTargetUsage::SCREEN, params._target._index };
        GFX::BlitRenderTargetCommand blitScreenDepthCmd = {};
        blitScreenDepthCmd._source = sourceID;
        blitScreenDepthCmd._destination = targetID;
        blitScreenDepthCmd._blitDepth._inputLayer = 0u;
        blitScreenDepthCmd._blitDepth._outputLayer = 0u;
        EnqueueCommand(bufferInOut, blitScreenDepthCmd);

        sourceID = targetID;
    }

#   pragma region HI_Z
    if (doOcclusionPass) {
        // This also renders into our HiZ texture that we may want to use later in PostFX
        occlusionPass(visibleNodes, visibleNodeCount, params._stagePass, *params._camera, sourceID, params._targetHIZ, bufferInOut);
    }
#   pragma endregion

#   pragma region MAIN_PASS
    // Same as for PRE_PASS. Subsequent operations expect a certain state
    params._stagePass._passType = RenderPassType::MAIN_PASS;
    if (doMainPass) {
        mainPass(visibleNodes, params, target, doPrePass, doOcclusionPass, bufferInOut);
    } else {
        DIVIDE_UNEXPECTED_CALL();
    }
#   pragma endregion

#   pragma region TRANSPARENCY_PASS
    // If doIOTPass is false, use forward pass shaders (i.e. MAIN_PASS again for transparents)
    if (doOITPass) {
        params._stagePass._passType = RenderPassType::OIT_PASS;
    }
    transparencyPass(visibleNodes, params, bufferInOut);
#   pragma endregion

    resolveMainScreenTarget(params, bufferInOut);

    EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
}

U32 RenderPassExecutor::renderQueueSize(const RenderPackage::MinQuality qualityRequirement) const {
    if (qualityRequirement == RenderPackage::MinQuality::COUNT) {
        return to_U32(_renderQueuePackages.size());
    }

    U32 size = 0;
    for (const RenderPackage* item : _renderQueuePackages) {
        if (item->qualityRequirement() == qualityRequirement) {
            ++size;
        }
    }

    return size;
}

void RenderPassExecutor::renderQueueToSubPasses(GFX::CommandBuffer& commandsInOut, const RenderPackage::MinQuality qualityRequirement) const {
    OPTICK_EVENT();

    eastl::fixed_vector<GFX::CommandBuffer*, Config::MAX_VISIBLE_NODES> buffers = {};

    if (qualityRequirement == RenderPackage::MinQuality::COUNT) {
        for (RenderPackage* item : _renderQueuePackages) {
            buffers.push_back(Attorney::RenderPackageRenderPassExecutor::getCommandBuffer(item));
        }
    } else {
        for (RenderPackage* item : _renderQueuePackages) {
            if (item->qualityRequirement() == qualityRequirement) {
                buffers.push_back(Attorney::RenderPackageRenderPassExecutor::getCommandBuffer(item));
            }
        }
    }

    commandsInOut.add(buffers.data(), buffers.size());
}
} //namespace Divide