#include "stdafx.h"

#include "Headers/BloomPreRenderOperator.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Textures/Headers/Texture.h"

#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Geometry/Shapes/Predefined/Headers/Quad3D.h"

#include "Platform/Video/Buffers/RenderTarget/Headers/RenderTarget.h"
#include "Rendering/PostFX/Headers/PreRenderBatch.h"

namespace Divide {

namespace {
    F32 resolutionDownscaleFactor = 2.0f;
};

BloomPreRenderOperator::BloomPreRenderOperator(GFXDevice& context, PreRenderBatch& parent, ResourceCache* cache)
    : PreRenderOperator(context, parent, FilterType::FILTER_BLOOM),
      _bloomFactor(0.8f),
      _bloomThreshold(0.75f)
{
    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "baseVertexShaders.glsl";
    vertModule._variant = "FullScreenQuad";

    ShaderModuleDescriptor fragModule = {};
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "bloom.glsl";
    fragModule._variant = "BloomCalc";

    ShaderProgramDescriptor shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.push_back(fragModule);

    ResourceDescriptor bloomCalc("BloomCalc");
    bloomCalc.propertyDescriptor(shaderDescriptor);
    bloomCalc.waitForReady(false);

    _bloomCalc = CreateResource<ShaderProgram>(cache, bloomCalc);
    _bloomCalc->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
        PipelineDescriptor pipelineDescriptor;
        pipelineDescriptor._stateHash = _context.get2DStateBlock();
        pipelineDescriptor._shaderProgramHandle = _bloomCalc->getGUID();

        _bloomCalcPipeline = _context.newPipeline(pipelineDescriptor);
    });

    fragModule._variant = "BloomApply";
    shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.push_back(fragModule);

    ResourceDescriptor bloomApply("BloomApply");
    bloomApply.propertyDescriptor(shaderDescriptor);
    bloomApply.waitForReady(false);
    _bloomApply = CreateResource<ShaderProgram>(cache, bloomApply);
    _bloomApply->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
        PipelineDescriptor pipelineDescriptor;
        pipelineDescriptor._stateHash = _context.get2DStateBlock();
        pipelineDescriptor._shaderProgramHandle = _bloomApply->getGUID();
        _bloomApplyPipeline = _context.newPipeline(pipelineDescriptor);
    });

    vec2<U16> res = parent.screenRT()._rt->getResolution();
    if (res.width > 2500) { //over 1440p
        resolutionDownscaleFactor = 4.0f;
    }

    const auto& screenAtt = parent.screenRT()._rt->getAttachment(RTAttachmentType::Colour, to_base(GFXDevice::ScreenTargets::ALBEDO));
    vectorEASTL<RTAttachmentDescriptor> att = {
        {
            screenAtt.texture()->descriptor(),
            screenAtt.samplerHash(),
            RTAttachmentType::Colour,
            0,
            DefaultColours::BLACK
        },
    };

    RenderTargetDescriptor desc = {};
    desc._resolution = res;
    desc._attachmentCount = to_U8(att.size());
    desc._attachments = att.data();

    desc._name = "Bloom_Blur_0";
    _bloomBlurBuffer[0] = _context.renderTargetPool().allocateRT(desc);
    desc._name = "Bloom_Blur_1";
    _bloomBlurBuffer[1] = _context.renderTargetPool().allocateRT(desc);

    desc._name = "Bloom";
    desc._resolution = vec2<U16>(res / resolutionDownscaleFactor);
    _bloomOutput = _context.renderTargetPool().allocateRT(desc);

    factor(_context.context().config().rendering.postFX.bloomFactor);
    luminanceThreshold(_context.context().config().rendering.postFX.bloomThreshold);
}

BloomPreRenderOperator::~BloomPreRenderOperator() {
    _context.renderTargetPool().deallocateRT(_bloomOutput);
    _context.renderTargetPool().deallocateRT(_bloomBlurBuffer[0]);
    _context.renderTargetPool().deallocateRT(_bloomBlurBuffer[1]);
}

bool BloomPreRenderOperator::ready() const {
    if (_bloomCalcPipeline != nullptr && _bloomApplyPipeline != nullptr) {
        return PreRenderOperator::ready();
    }

    return false;
}

void BloomPreRenderOperator::reshape(U16 width, U16 height) {
    PreRenderOperator::reshape(width, height);

    const U16 w = to_U16(width / resolutionDownscaleFactor);
    const U16 h = to_U16(height / resolutionDownscaleFactor);
    _bloomOutput._rt->resize(w, h);
    _bloomBlurBuffer[0]._rt->resize(width, height);
    _bloomBlurBuffer[1]._rt->resize(width, height);
}

void BloomPreRenderOperator::factor(F32 val) {
    _bloomFactor = val;
    _bloomApplyConstants.set(_ID("bloomFactor"), GFX::PushConstantType::FLOAT, _bloomFactor);
    _context.context().config().rendering.postFX.bloomFactor = val;
}

void BloomPreRenderOperator::luminanceThreshold(F32 val) {
    _bloomThreshold = val;
    _bloomCalcConstants.set(_ID("luminanceThreshold"), GFX::PushConstantType::FLOAT, _bloomThreshold);
    _context.context().config().rendering.postFX.bloomThreshold = val;
}

// Order: luminance calc -> bloom -> tonemap
bool BloomPreRenderOperator::execute(const Camera* camera, const RenderTargetHandle& input, const RenderTargetHandle& output, GFX::CommandBuffer& bufferInOut) {
    assert(input._targetID != output._targetID);

    const auto& screenAtt = input._rt->getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::ALBEDO));
    const TextureData screenTex = screenAtt.texture()->data();

    
    GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
    descriptorSetCmd._set._textureData.setTexture(screenTex, screenAtt.samplerHash(),TextureUsage::UNIT0);
    GFX::EnqueueCommand(bufferInOut, descriptorSetCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _bloomCalcPipeline });
    GFX::EnqueueCommand(bufferInOut, GFX::SendPushConstantsCommand{ _bloomCalcConstants });

    // Step 1: generate bloom - render all of the "bright spots"
    RTClearDescriptor clearTarget = {};
    clearTarget.clearDepth(false);
    clearTarget.clearColours(true);

    GFX::ClearRenderTargetCommand clearRenderTargetCmd = {};
    clearRenderTargetCmd._target = _bloomOutput._targetID;
    clearRenderTargetCmd._descriptor = clearTarget;
    GFX::EnqueueCommand(bufferInOut, clearRenderTargetCmd);

    GFX::BeginRenderPassCommand beginRenderPassCmd = {};
    beginRenderPassCmd._target = _bloomOutput._targetID;
    beginRenderPassCmd._name = "DO_BLOOM_PASS";
    GFX::EnqueueCommand(bufferInOut, beginRenderPassCmd);

    GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});

    // Step 2: blur bloom
    _context.blurTarget(_bloomOutput,
                        _bloomBlurBuffer[0],
                        _bloomBlurBuffer[1],
                        RTAttachmentType::Colour,
                        0,
                        10,
                        true,
                        bufferInOut);

    // Step 3: apply bloom
    const auto& bloomAtt = _bloomBlurBuffer[1]._rt->getAttachment(RTAttachmentType::Colour, 0); 
    const TextureData bloomTex = bloomAtt.texture()->data();

    descriptorSetCmd._set._textureData.setTexture(screenTex, screenAtt.samplerHash(), TextureUsage::UNIT0);
    descriptorSetCmd._set._textureData.setTexture(bloomTex, bloomAtt.samplerHash(),TextureUsage::UNIT1);
    GFX::EnqueueCommand(bufferInOut, descriptorSetCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _bloomApplyPipeline });
    GFX::EnqueueCommand(bufferInOut, GFX::SendPushConstantsCommand{ _bloomApplyConstants });

    beginRenderPassCmd._target = output._targetID;
    beginRenderPassCmd._descriptor = _screenOnlyDraw;
    GFX::EnqueueCommand(bufferInOut, beginRenderPassCmd);

    GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});


    return true;
}

};