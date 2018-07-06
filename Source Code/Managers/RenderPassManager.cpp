#include "stdafx.h"

#include "Headers/RenderPassManager.h"

#include "Core/Headers/TaskPool.h"
#include "Core/Headers/StringHelper.h"
#include "Managers/Headers/SceneManager.h"
#include "Rendering/Camera/Headers/Camera.h"
#include "Rendering/RenderPass/Headers/RenderQueue.h"
#include "Platform/Video/Textures/Headers/Texture.h"

namespace Divide {

RenderPassManager::RenderPassManager(Kernel& parent, GFXDevice& context)
    : KernelComponent(parent),
      _context(context),
      _renderQueue(nullptr),
      _OITCompositionShader(nullptr)
{
}

RenderPassManager::~RenderPassManager()
{
    destroy();
}

bool RenderPassManager::init() {
    if (_renderQueue == nullptr) {
        _renderQueue = MemoryManager_NEW RenderQueue(_context);

        ResourceDescriptor shaderDesc("OITComposition");
        _OITCompositionShader = CreateResource<ShaderProgram>(parent().resourceCache(), shaderDesc);
        return true;
    }

    return false;
}

void RenderPassManager::destroy() {
    MemoryManager::DELETE_VECTOR(_renderPasses);
    MemoryManager::DELETE(_renderQueue);
    _OITCompositionShader.reset();
}

void RenderPassManager::render(SceneRenderState& sceneRenderState) {
    for (RenderPass* rp : _renderPasses) {
        rp->render(sceneRenderState);
    }
}

RenderPass& RenderPassManager::addRenderPass(const stringImpl& renderPassName,
                                             U8 orderKey,
                                             RenderStage renderStage) {
    assert(!renderPassName.empty());

    _renderPasses.emplace_back(MemoryManager_NEW RenderPass(*this, _context, renderPassName, orderKey, renderStage));
    RenderPass& item = *_renderPasses.back();

    std::sort(std::begin(_renderPasses), std::end(_renderPasses),
              [](RenderPass* a, RenderPass* b)
                  -> bool { return a->sortKey() < b->sortKey(); });

    assert(item.sortKey() == orderKey);

    return item;
}

void RenderPassManager::removeRenderPass(const stringImpl& name) {
    for (vectorImpl<RenderPass*>::iterator it = std::begin(_renderPasses);
         it != std::end(_renderPasses); ++it) {
        if ((*it)->getName().compare(name) == 0) {
            _renderPasses.erase(it);
            break;
        }
    }
}

U16 RenderPassManager::getLastTotalBinSize(RenderStage renderStage) const {
    return getPassForStage(renderStage)->getLastTotalBinSize();
}

RenderPass* RenderPassManager::getPassForStage(RenderStage renderStage) const {
    for (RenderPass* pass : _renderPasses) {
        if (pass->stageFlag() == renderStage) {
            return pass;
        }
    }

    DIVIDE_UNEXPECTED_CALL();
    return nullptr;
}

const RenderPass::BufferData& 
RenderPassManager::getBufferData(RenderStage renderStage, I32 bufferIndex) const {
    return getPassForStage(renderStage)->getBufferData(bufferIndex);
}

RenderPass::BufferData&
RenderPassManager::getBufferData(RenderStage renderStage, I32 bufferIndex) {
    return getPassForStage(renderStage)->getBufferData(bufferIndex);
}

GFX::CommandBuffer RenderPassManager::prePass(const PassParams& params, const RenderTarget& target) {
    static const vectorImpl<RenderBinType> depthExclusionList
    {
        RenderBinType::RBT_DECAL,
        RenderBinType::RBT_TRANSLUCENT
    };

    GFX::CommandBuffer buffer;

    GFX::BeginDebugScopeCommand beginDebugScopeCmd;
    beginDebugScopeCmd._scopeID = 0;
    beginDebugScopeCmd._scopeName = Util::StringFormat("Custom pass ( %s ): PrePass", TypeUtil::renderStageToString(params.stage));
    GFX::BeginDebugScope(buffer, beginDebugScopeCmd);

    // PrePass requires a depth buffer
    bool doPrePass = params.doPrePass && target.getAttachment(RTAttachmentType::Depth, 0).used();

    if (doPrePass) {
        SceneManager& sceneManager = parent().sceneManager();

        _context.setRenderStagePass(RenderStagePass(params.stage, RenderPassType::DEPTH_PASS));

        Attorney::SceneManagerRenderPass::populateRenderQueue(sceneManager,
                                                              *params.camera,
                                                              true,
                                                              params.pass);

        if (params.target._usage != RenderTargetUsage::COUNT) {
           
            if (params.bindTargets) {
                GFX::BeginRenderPassCommand beginRenderPassCommand;
                beginRenderPassCommand._target = params.target;
                beginRenderPassCommand._descriptor = RenderTarget::defaultPolicyDepthOnly();
                GFX::BeginRenderPass(buffer, beginRenderPassCommand);
            }

            for (U32 i = 0; i < to_U32(RenderBinType::COUNT); ++i) {
                if (std::find(std::begin(depthExclusionList),
                    std::end(depthExclusionList),
                    RenderBinType::_from_integral(i)) == std::cend(depthExclusionList))
                {
                    if (i != to_U32(RenderBinType::RBT_TRANSLUCENT)) {
                        _context.renderQueueToSubPasses(RenderBinType::_from_integral(i), buffer);
                    }
                }
            }

            Attorney::SceneManagerRenderPass::postRender(sceneManager, *params.camera, buffer);

            if (params.bindTargets) {
                GFX::EndRenderPassCommand endRenderPassCommand;
                GFX::EndRenderPass(buffer, endRenderPassCommand);
            }
        }
        
    }

    return buffer;
}

GFX::CommandBuffer RenderPassManager::mainPass(const PassParams& params, RenderTarget& target) {
    static const vectorImpl<RenderBinType> shadowExclusionList
    {
        RenderBinType::RBT_DECAL,
        RenderBinType::RBT_SKY,
        RenderBinType::RBT_TRANSLUCENT
    };
    constexpr U32 binCount = to_U32(RenderBinType::COUNT);

    GFX::CommandBuffer buffer;

    GFX::BeginDebugScopeCommand beginDebugScopeCmd;
    beginDebugScopeCmd._scopeID = 1;
    beginDebugScopeCmd._scopeName = Util::StringFormat("Custom pass ( %s ): RenderPass", TypeUtil::renderStageToString(params.stage));
    GFX::BeginDebugScope(buffer, beginDebugScopeCmd);

    SceneManager& sceneManager = parent().sceneManager();

    _context.setRenderStagePass(RenderStagePass(params.stage, RenderPassType::COLOUR_PASS));
    Attorney::SceneManagerRenderPass::populateRenderQueue(sceneManager,
                                                          *params.camera,
                                                          !params.doPrePass,
                                                          params.pass);

    if (params.target._usage != RenderTargetUsage::COUNT) {
        bool drawToDepth = true;
        if (params.stage != RenderStage::SHADOW) {
            Attorney::SceneManagerRenderPass::preRender(sceneManager, *params.camera, target, buffer);
            if (params.doPrePass) {
                drawToDepth = Config::DEBUG_HIZ_CULLING;
            }
        }

        if (params.stage == RenderStage::DISPLAY) {
            GFX::BindDescriptorSetsCommand bindDescriptorSets;
            
            // Bind the depth buffers
            TextureData depthBufferTextureData = target.getAttachment(RTAttachmentType::Depth, 0).texture()->getData();
            depthBufferTextureData.setBinding(to_U32(ShaderProgram::TextureUsage::DEPTH));
            bindDescriptorSets._set._textureData.addTexture(depthBufferTextureData);

            const RTAttachment& velocityAttachment = target.getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::VELOCITY));
            if (velocityAttachment.used()) {
                const Texture_ptr& prevDepthTexture = _context.getPrevDepthBuffer();
                TextureData prevDepthData = (prevDepthTexture ? prevDepthTexture->getData() : depthBufferTextureData);
                prevDepthData.setBinding(to_U32(ShaderProgram::TextureUsage::DEPTH_PREV));
                bindDescriptorSets._set._textureData.addTexture(prevDepthData);
            }

            GFX::BindDescriptorSets(buffer, bindDescriptorSets);
        }

        RTDrawDescriptor& drawPolicy = 
            params.drawPolicy ? *params.drawPolicy
                              : (!Config::DEBUG_HIZ_CULLING ? RenderTarget::defaultPolicyKeepDepth()
                                                            : RenderTarget::defaultPolicy());

        drawPolicy.drawMask().setEnabled(RTAttachmentType::Depth, 0, drawToDepth);

        if (params.bindTargets) {
            GFX::BeginRenderPassCommand beginRenderPassCommand;
            beginRenderPassCommand._target = params.target;
            beginRenderPassCommand._descriptor = drawPolicy;
            GFX::BeginRenderPass(buffer, beginRenderPassCommand);
        }

        if (params.stage == RenderStage::SHADOW) {
            for (U32 i = 0; i < binCount; ++i) {
                if (std::find(std::begin(shadowExclusionList),
                              std::end(shadowExclusionList),
                               RenderBinType::_from_integral(i)) == std::cend(shadowExclusionList))
                {
                    if (i != to_U32(RenderBinType::RBT_TRANSLUCENT)) {
                        _context.renderQueueToSubPasses(RenderBinType::_from_integral(i), buffer);
                    }
                }
            }
        } else {
            for (U32 i = 0; i < binCount; ++i) {
                if (i != to_U32(RenderBinType::RBT_TRANSLUCENT)) {
                    _context.renderQueueToSubPasses(RenderBinType::_from_integral(i), buffer);
                }
            }
        }

        Attorney::SceneManagerRenderPass::postRender(sceneManager, *params.camera, buffer);

        if (params.stage == RenderStage::DISPLAY) {
            /// These should be OIT rendered as well since things like debug nav meshes have translucency
            Attorney::SceneManagerRenderPass::debugDraw(sceneManager, *params.camera, buffer);
        }

        if (params.bindTargets) {
            GFX::EndRenderPassCommand endRenderPassCommand;
            GFX::EndRenderPass(buffer, endRenderPassCommand);
        }
    }

    GFX::EndDebugScopeCommand endDebugScopeCmd;
    GFX::EndDebugScope(buffer, endDebugScopeCmd);

    return buffer;
}

GFX::CommandBuffer RenderPassManager::woitPass(const PassParams& params, const RenderTarget& target) {
    static bool init = false;
    static GFX::DrawCommand drawCmd;
    static RTDrawDescriptor noClearPolicy;
    static GFX::BindPipelineCommand bindPipelineCmd;
    static GFX::BeginRenderPassCommand beginRenderPassOitCmd, beginRenderPassCompCmd;
    static GFX::EndRenderPassCommand endRenderPassOitCmd, endRenderPassCompCmd;

    GFX::CommandBuffer buffer;
    // Weighted Blended Order Independent Transparency
    if (_context.renderQueueSize(RenderBinType::RBT_TRANSLUCENT) > 0) {
        if (!init) {
            beginRenderPassOitCmd._target = RenderTargetID(RenderTargetUsage::OIT);
            RTBlendState& state0 = beginRenderPassOitCmd._descriptor.blendState(0);
            RTBlendState& state1 = beginRenderPassOitCmd._descriptor.blendState(1);

            state0._blendEnable = true;
            state0._blendProperties._blendSrc = BlendProperty::ONE;
            state0._blendProperties._blendDest = BlendProperty::ONE;
            state0._blendProperties._blendOp = BlendOperation::ADD;

            state1._blendEnable = true;
            state1._blendProperties._blendSrc = BlendProperty::ZERO;
            state1._blendProperties._blendDest = BlendProperty::INV_SRC_COLOR;
            state1._blendProperties._blendOp = BlendOperation::ADD;

            // Don't clear depth & colours and do not write to the depth buffer
            noClearPolicy.disableState(RTDrawDescriptor::State::CLEAR_DEPTH_BUFFER);
            noClearPolicy.disableState(RTDrawDescriptor::State::CLEAR_COLOUR_BUFFERS);
            noClearPolicy.drawMask().setEnabled(RTAttachmentType::Depth, 0, false);
            noClearPolicy.blendState(0)._blendEnable = true;
            noClearPolicy.blendState(0)._blendProperties._blendSrc = BlendProperty::SRC_ALPHA;
            noClearPolicy.blendState(0)._blendProperties._blendDest = BlendProperty::INV_SRC_ALPHA;

            beginRenderPassCompCmd._target = RenderTargetID(RenderTargetUsage::SCREEN);
            beginRenderPassCompCmd._descriptor = noClearPolicy;
            PipelineDescriptor pipelineDescriptor;
            pipelineDescriptor._stateHash = _context.get2DStateBlock();
            pipelineDescriptor._shaderProgram = _OITCompositionShader;
            bindPipelineCmd._pipeline = _context.newPipeline(pipelineDescriptor);

            GenericDrawCommand drawCommand;
            drawCommand.primitiveType(PrimitiveType::TRIANGLES);
            drawCommand.drawCount(1);
            drawCmd._drawCommands.push_back(drawCommand);

            init = true;
        }

        // Step1: Draw translucent items into the accumulation and revealage buffers
        GFX::BeginRenderPass(buffer, beginRenderPassOitCmd);
        _context.renderQueueToSubPasses(RenderBinType::RBT_TRANSLUCENT, buffer);
        GFX::EndRenderPass(buffer, endRenderPassOitCmd);

        // Step2: Composition pass
        GFX::BeginRenderPass(buffer, beginRenderPassCompCmd);
        GFX::BindPipeline(buffer, bindPipelineCmd);
        TextureData accum = _context.renderTargetPool().renderTarget(beginRenderPassOitCmd._target).getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::ACCUMULATION)).texture()->getData();
        TextureData revealage = _context.renderTargetPool().renderTarget(beginRenderPassOitCmd._target).getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::REVEALAGE)).texture()->getData();
        accum.setHandleLow(0);
        revealage.setHandleLow(1);
        GFX::BindDescriptorSetsCommand descriptorSetCmd;
        descriptorSetCmd._set._textureData.addTexture(accum);
        descriptorSetCmd._set._textureData.addTexture(revealage);
        GFX::BindDescriptorSets(buffer, descriptorSetCmd);
        GFX::AddDrawCommands(buffer, drawCmd);
        GFX::EndRenderPass(buffer, endRenderPassCompCmd);
    }

    return buffer;
}

GFX::CommandBuffer RenderPassManager::doCustomPass(PassParams& params) {
    GFX::CommandBuffer commandBuffer;

    // Tell the Rendering API to draw from our desired PoV
    GFX::SetCameraCommand setCameraCommand;
    setCameraCommand._camera = params.camera;
    GFX::SetCamera(commandBuffer, setCameraCommand);

    GFX::SetClipPlanesCommand setClipPlanesCommand;
    setClipPlanesCommand._clippingPlanes = params.clippingPlanes;
    GFX::SetClipPlanes(commandBuffer, setClipPlanesCommand);

    RenderTarget& target = _context.renderTargetPool().renderTarget(params.target);
    commandBuffer.add(prePass(params, target));

    if (params.occlusionCull) {
        _context.constructHIZ(params.target, commandBuffer);
        _context.occlusionCull(getBufferData(params.stage, params.pass), target.getAttachment(RTAttachmentType::Depth, 0).texture(), commandBuffer);
    }

    commandBuffer.add(mainPass(params, target));
    if (false) {
        commandBuffer.add(woitPass(params, target));
    }
    

    return commandBuffer;
}

};