#include "stdafx.h"

#include "Headers/PostAAPreRenderOperator.h"

#include "Utility/Headers/Localization.h"
#include "Core/Headers/Configuration.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Core/Headers/PlatformContext.h"
#include "Rendering/PostFX/Headers/PostFX.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Rendering/PostFX/Headers/PreRenderBatch.h"

namespace Divide {

PostAAPreRenderOperator::PostAAPreRenderOperator(GFXDevice& context, PreRenderBatch& parent, ResourceCache* cache)
    : PreRenderOperator(context, parent, FilterType::FILTER_SS_ANTIALIASING)
{
    useSMAA(_ID(cache->context().config().rendering.postFX.postAAType.c_str()) == _ID("SMAA"));
    postAAQualityLevel(cache->context().config().rendering.postFX.PostAAQualityLevel);

    RenderTargetDescriptor desc = {};
    desc._resolution = parent.screenRT()._rt->getResolution();
    {
        SamplerDescriptor sampler = {};
        sampler.wrapU(TextureWrap::CLAMP_TO_EDGE);
        sampler.wrapV(TextureWrap::CLAMP_TO_EDGE);
        sampler.wrapW(TextureWrap::CLAMP_TO_EDGE);
        sampler.minFilter(TextureFilter::LINEAR);
        sampler.magFilter(TextureFilter::LINEAR);
        sampler.anisotropyLevel(0);

        TextureDescriptor weightsDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::RGBA, GFXDataFormat::FLOAT_16);
        weightsDescriptor.samplerDescriptor(sampler);

        vectorEASTL<RTAttachmentDescriptor> att = {
            { weightsDescriptor, RTAttachmentType::Colour }
        };

        desc._name = "SMAAWeights";
        desc._attachmentCount = to_U8(att.size());
        desc._attachments = att.data();

        _smaaWeights = _context.renderTargetPool().allocateRT(desc);
    }
    { //FXAA Shader
        ShaderModuleDescriptor vertModule = {};
        vertModule._moduleType = ShaderType::VERTEX;
        vertModule._sourceFile = "baseVertexShaders.glsl";
        vertModule._variant = "FullScreenQuad";

        ShaderModuleDescriptor fragModule = {};
        fragModule._moduleType = ShaderType::FRAGMENT;
        fragModule._sourceFile = "FXAA.glsl";

        ShaderProgramDescriptor aaShaderDescriptor = {};
        aaShaderDescriptor._modules = { vertModule, fragModule };

        ResourceDescriptor fxaa("FXAA");
        fxaa.propertyDescriptor(aaShaderDescriptor);
        fxaa.waitForReady(false);
        _fxaa = CreateResource<ShaderProgram>(cache, fxaa);
        _fxaa->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
            PipelineDescriptor pipelineDescriptor;
            pipelineDescriptor._stateHash = _context.get2DStateBlock();
            pipelineDescriptor._shaderProgramHandle = _fxaa->getGUID();
            _fxaaPipeline = _context.newPipeline(pipelineDescriptor);
        });
    }
    { //SMAA Shaders
        ShaderModuleDescriptor vertModule = {};
        vertModule._moduleType = ShaderType::VERTEX;
        vertModule._sourceFile = "SMAA.glsl";

        ShaderModuleDescriptor fragModule = {};
        fragModule._moduleType = ShaderType::FRAGMENT;
        fragModule._sourceFile = "SMAA.glsl";


        vertModule._variant = "Weight";
        fragModule._variant = "Weight";
        ShaderProgramDescriptor weightsDescriptor = {};
        weightsDescriptor._modules = { vertModule, fragModule };

        ResourceDescriptor smaaWeights("SMAA.Weights");
        smaaWeights.propertyDescriptor(weightsDescriptor);
        smaaWeights.waitForReady(false);
        _smaaWeightComputation = CreateResource<ShaderProgram>(cache, smaaWeights);
        _smaaWeightComputation->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
            PipelineDescriptor pipelineDescriptor;
            pipelineDescriptor._stateHash = _context.get2DStateBlock();
            pipelineDescriptor._shaderProgramHandle = _smaaWeightComputation->getGUID();
            _smaaWeightPipeline = _context.newPipeline(pipelineDescriptor);
        });

        vertModule._variant = "Blend";
        fragModule._variant = "Blend";
        ShaderProgramDescriptor blendDescriptor = {};
        blendDescriptor._modules = { vertModule, fragModule };

        ResourceDescriptor smaaBlend("SMAA.Blend");
        smaaBlend.propertyDescriptor(blendDescriptor);
        smaaBlend.waitForReady(false);
        _smaaBlend = CreateResource<ShaderProgram>(cache, smaaBlend);
        _smaaBlend->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
            PipelineDescriptor pipelineDescriptor;
            pipelineDescriptor._stateHash = _context.get2DStateBlock();
            pipelineDescriptor._shaderProgramHandle = _smaaBlend->getGUID();
            _smaaBlendPipeline = _context.newPipeline(pipelineDescriptor);
        });
    }
    { //SMAA Textures
        SamplerDescriptor textureSampler = {};
        TextureDescriptor textureDescriptor(TextureType::TEXTURE_2D);
        textureDescriptor.samplerDescriptor(textureSampler);
        textureDescriptor.srgb(false);

        ResourceDescriptor searchDescriptor("SMAA_Search");
        searchDescriptor.assetName("smaa_search.png");
        searchDescriptor.assetLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
        searchDescriptor.propertyDescriptor(textureDescriptor);
        searchDescriptor.waitForReady(false);
        _searchTexture = CreateResource<Texture>(cache, searchDescriptor);

        ResourceDescriptor areaDescriptor("SMAA_Area");
        areaDescriptor.assetName("smaa_area.png");
        areaDescriptor.assetLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
        areaDescriptor.propertyDescriptor(textureDescriptor);
        areaDescriptor.waitForReady(false);
        _areaTexture = CreateResource<Texture>(cache, areaDescriptor);
    }
    _pushConstantsCommand._constants.set(_ID("dvd_qualityMultiplier"), GFX::PushConstantType::INT, to_I32(postAAQualityLevel() - 1));
}

bool PostAAPreRenderOperator::ready() const {
    if (_smaaBlendPipeline != nullptr && _smaaWeightPipeline != nullptr && _fxaaPipeline != nullptr) {
        if (_searchTexture->getState() == ResourceState::RES_LOADED && _areaTexture->getState() == ResourceState::RES_LOADED) {
            return PreRenderOperator::ready();
        }
    }

    return false;
}

void PostAAPreRenderOperator::reshape(U16 width, U16 height) {
    PreRenderOperator::reshape(width, height);
    _smaaWeights._rt->resize(width, height);
}

/// This is tricky as we use our screen as both input and output
bool PostAAPreRenderOperator::execute(const Camera& camera, const RenderTargetHandle& input, const RenderTargetHandle& output, GFX::CommandBuffer& bufferInOut) {
    if (useSMAA() != currentUseSMAA()) {
        currentUseSMAA(useSMAA());

        _context.context().config().rendering.postFX.postAAType = useSMAA() ? "SMAA" : "FXAA";
        _context.context().config().changed(true);
    }

    if (postAAQualityLevel() != currentPostAAQualityLevel()) {
        currentPostAAQualityLevel(postAAQualityLevel());

        _pushConstantsCommand._constants.set(_ID("dvd_qualityMultiplier"), GFX::PushConstantType::INT, to_I32(postAAQualityLevel() - 1));

        _context.context().config().rendering.postFX.PostAAQualityLevel = postAAQualityLevel();
        _context.context().config().changed(true);

        if (currentPostAAQualityLevel() == 0) {
            _parent.parent().popFilter(_operatorType);
        } else {
            _parent.parent().pushFilter(_operatorType);
        }
    }

    const TextureData screenTex = input._rt->getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::ALBEDO)).texture()->data();

    if (useSMAA()) {
        { //Step 1: Compute weights
            RTClearDescriptor clearTarget = {};
            clearTarget.clearDepth(false);
            clearTarget.clearColours(true);

            GFX::ClearRenderTargetCommand clearRenderTargetCmd = {};
            clearRenderTargetCmd._target = _smaaWeights._targetID;
            clearRenderTargetCmd._descriptor = clearTarget;
            GFX::EnqueueCommand(bufferInOut, clearRenderTargetCmd);

            GFX::BeginRenderPassCommand beginRenderPassCmd = {};
            beginRenderPassCmd._target = _smaaWeights._targetID;
            beginRenderPassCmd._name = "DO_SMAA_WEIGHT_PASS";
            GFX::EnqueueCommand(bufferInOut, beginRenderPassCmd);

            const TextureData edgesTex = _parent.edgesRT()._rt->getAttachment(RTAttachmentType::Colour, 0).texture()->data();
            const TextureData areaTex = _areaTexture->data();
            const TextureData searchTex = _searchTexture->data();

            GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
            descriptorSetCmd._set._textureData.setTexture(edgesTex, TextureUsage::UNIT0);
            descriptorSetCmd._set._textureData.setTexture(areaTex, TextureUsage::UNIT1);
            descriptorSetCmd._set._textureData.setTexture(searchTex, to_U8(TextureUsage::UNIT1) + 1);
            GFX::EnqueueCommand(bufferInOut, descriptorSetCmd);

            GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _smaaWeightPipeline });
            GFX::EnqueueCommand(bufferInOut, _pushConstantsCommand);

            GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

            GFX::EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{ });
        }
        { //Step 2: Blend
            GFX::BeginRenderPassCommand beginRenderPassCmd = {};
            beginRenderPassCmd._target = output._targetID;
            beginRenderPassCmd._descriptor = _screenOnlyDraw;
            beginRenderPassCmd._name = "DO_SMAA_BLEND_PASS";
            GFX::EnqueueCommand(bufferInOut, beginRenderPassCmd);

            const TextureData blendTex = _smaaWeights._rt->getAttachment(RTAttachmentType::Colour, 0).texture()->data();

            GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
            descriptorSetCmd._set._textureData.setTexture(screenTex, TextureUsage::UNIT0);
            descriptorSetCmd._set._textureData.setTexture(blendTex, TextureUsage::UNIT1);
            GFX::EnqueueCommand(bufferInOut, descriptorSetCmd);

            GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _smaaBlendPipeline });
            GFX::EnqueueCommand(bufferInOut, _pushConstantsCommand);

            GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

            GFX::EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{ });
        }
    } else {
        // Apply FXAA/SMAA to the specified render target
        GFX::BeginRenderPassCommand beginRenderPassCmd;
        beginRenderPassCmd._target = output._targetID;
        beginRenderPassCmd._descriptor = _screenOnlyDraw;
        beginRenderPassCmd._name = "DO_POSTAA_PASS";
        GFX::EnqueueCommand(bufferInOut, beginRenderPassCmd);

        GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _fxaaPipeline });
        GFX::EnqueueCommand(bufferInOut, _pushConstantsCommand);

        GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
        descriptorSetCmd._set._textureData.setTexture(screenTex, TextureUsage::UNIT0);
        GFX::EnqueueCommand(bufferInOut, descriptorSetCmd);

        GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

        GFX::EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{ });
    }

    return true;
}

};