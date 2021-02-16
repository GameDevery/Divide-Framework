#include "stdafx.h"

#include "Headers/PostFX.h"
#include "Headers/PreRenderOperator.h"

#include "Core/Headers/ParamHandler.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Headers/StringHelper.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Core/Time/Headers/ApplicationTimer.h"

#include "Geometry/Shapes/Predefined/Headers/Quad3D.h"
#include "Managers/Headers/SceneManager.h"
#include "Platform/Video/Buffers/RenderTarget/Headers/RenderTarget.h"
#include "Rendering/Camera/Headers/Camera.h"

namespace Divide {

const char* PostFX::FilterName(const FilterType filter) noexcept {
    switch (filter) {
        case FilterType::FILTER_SS_ANTIALIASING:  return "SS_ANTIALIASING";
        case FilterType::FILTER_SS_REFLECTIONS:  return "SS_REFLECTIONS";
        case FilterType::FILTER_SS_AMBIENT_OCCLUSION:  return "SS_AMBIENT_OCCLUSION";
        case FilterType::FILTER_DEPTH_OF_FIELD:  return "DEPTH_OF_FIELD";
        case FilterType::FILTER_MOTION_BLUR:  return "MOTION_BLUR";
        case FilterType::FILTER_BLOOM: return "BLOOM";
        case FilterType::FILTER_LUT_CORECTION:  return "LUT_CORRECTION";
        case FilterType::FILTER_UNDERWATER: return "UNDERWATER";
        case FilterType::FILTER_NOISE: return "NOISE";
        case FilterType::FILTER_VIGNETTE: return "VIGNETTE";
        default: break;
    }

    return "Unknown";
};

PostFX::PostFX(PlatformContext& context, ResourceCache* cache)
    : PlatformContextComponent(context),
     _preRenderBatch(context.gfx(), *this, cache)
{
    std::atomic_uint loadTasks = 0u;

    context.paramHandler().setParam<bool>(_ID("postProcessing.enableVignette"), false);

    _postFXTarget.drawMask().disableAll();
    _postFXTarget.drawMask().setEnabled(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::ALBEDO), true);

    Console::printfn(Locale::Get(_ID("START_POST_FX")));

    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "baseVertexShaders.glsl";
    vertModule._variant = "FullScreenQuad";

    ShaderModuleDescriptor fragModule = {};
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "postProcessing.glsl";
    fragModule._defines.emplace_back(Util::StringFormat("TEX_BIND_POINT_SCREEN %d", to_base(TexOperatorBindPoint::TEX_BIND_POINT_SCREEN)).c_str(), true);
    fragModule._defines.emplace_back(Util::StringFormat("TEX_BIND_POINT_NOISE %d", to_base(TexOperatorBindPoint::TEX_BIND_POINT_NOISE)).c_str(), true);
    fragModule._defines.emplace_back(Util::StringFormat("TEX_BIND_POINT_BORDER %d", to_base(TexOperatorBindPoint::TEX_BIND_POINT_BORDER)).c_str(), true);
    fragModule._defines.emplace_back(Util::StringFormat("TEX_BIND_POINT_UNDERWATER %d", to_base(TexOperatorBindPoint::TEX_BIND_POINT_UNDERWATER)).c_str(), true);
    fragModule._defines.emplace_back(Util::StringFormat("TEX_BIND_POINT_SSR %d", to_base(TexOperatorBindPoint::TEX_BIND_POINT_SSR)).c_str(), true);
    fragModule._defines.emplace_back(Util::StringFormat("TEX_BIND_POINT_POSTFXDATA %d", to_base(TexOperatorBindPoint::TEX_BIND_POINT_POSTFXDATA)).c_str(), true);

    ShaderProgramDescriptor postFXShaderDescriptor = {};
    postFXShaderDescriptor._modules.push_back(vertModule);
    postFXShaderDescriptor._modules.push_back(fragModule);

    _drawConstants.set(_ID("_noiseTile"), GFX::PushConstantType::FLOAT, 0.1f);
    _drawConstants.set(_ID("_noiseFactor"), GFX::PushConstantType::FLOAT, 0.02f);
    _drawConstants.set(_ID("_fadeActive"), GFX::PushConstantType::BOOL, false);
    _drawConstants.set(_ID("_zPlanes"), GFX::PushConstantType::VEC2, vec2<F32>(0.01f, 500.0f));

    TextureDescriptor texDescriptor(TextureType::TEXTURE_2D);

    ResourceDescriptor textureWaterCaustics("Underwater Caustics");
    textureWaterCaustics.assetName(ResourcePath("terrain_water_NM.jpg"));
    textureWaterCaustics.assetLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
    textureWaterCaustics.propertyDescriptor(texDescriptor);
    textureWaterCaustics.waitForReady(false);
    _underwaterTexture = CreateResource<Texture>(cache, textureWaterCaustics, loadTasks);

    ResourceDescriptor noiseTexture("noiseTexture");
    noiseTexture.assetName(ResourcePath("bruit_gaussien.jpg"));
    noiseTexture.assetLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
    noiseTexture.propertyDescriptor(texDescriptor);
    noiseTexture.waitForReady(false);
    _noise = CreateResource<Texture>(cache, noiseTexture, loadTasks);

    ResourceDescriptor borderTexture("borderTexture");
    borderTexture.assetName(ResourcePath("vignette.jpeg"));
    borderTexture.assetLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
    borderTexture.propertyDescriptor(texDescriptor);
    borderTexture.waitForReady(false);
    _screenBorder = CreateResource<Texture>(cache, borderTexture), loadTasks;

    _noiseTimer = 0.0;
    _tickInterval = 1.0f / 24.0f;
    _randomNoiseCoefficient = 0;
    _randomFlashCoefficient = 0;

    ResourceDescriptor postFXShader("postProcessing");
    postFXShader.propertyDescriptor(postFXShaderDescriptor);
    _postProcessingShader = CreateResource<ShaderProgram>(cache, postFXShader, loadTasks);
    _postProcessingShader->addStateCallback(ResourceState::RES_LOADED, [this, &context](CachedResource*) {
        PipelineDescriptor pipelineDescriptor;
        pipelineDescriptor._stateHash = context.gfx().get2DStateBlock();
        pipelineDescriptor._shaderProgramHandle = _postProcessingShader->getGUID();
        _drawPipeline = context.gfx().newPipeline(pipelineDescriptor);
    });

    WAIT_FOR_CONDITION(loadTasks.load() == 0);
}

void PostFX::updateResolution(const U16 newWidth, const U16 newHeight) {
    if (_resolutionCache.width == newWidth &&
        _resolutionCache.height == newHeight|| 
        newWidth < 1 || newHeight < 1)
    {
        return;
    }

    _resolutionCache.set(newWidth, newHeight);

    _preRenderBatch.reshape(newWidth, newHeight);
}

void PostFX::apply(const Camera* camera, GFX::CommandBuffer& bufferInOut) {
    static size_t s_samplerHash = 0u;
    if (s_samplerHash == 0u) {
        SamplerDescriptor defaultSampler = {};
        defaultSampler.wrapUVW(TextureWrap::REPEAT);
        s_samplerHash = defaultSampler.getHash();
    }

    EnqueueCommand(bufferInOut, GFX::BeginDebugScopeCommand{ "PostFX: Apply" });

    EnqueueCommand(bufferInOut, GFX::SetCameraCommand{Camera::utilityCamera(Camera::UtilityCamera::_2D)->snapshot()});

    _preRenderBatch.execute(camera, _filterStack | _overrideFilterStack, bufferInOut);
    
    const auto& prbAtt = _preRenderBatch.getOutput(false)._rt->getAttachment(RTAttachmentType::Colour, 0);
    const TextureData output = prbAtt.texture()->data();
    const TextureData data0 = _underwaterTexture->data();
    const TextureData data1 = _noise->data();
    const TextureData data2 = _screenBorder->data();

    RenderTarget& fxDataRT = context().gfx().renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::POSTFX_DATA));
    const auto& fxDataAtt = fxDataRT.getAttachment(RTAttachmentType::Colour, 0);
    const TextureData fxData = fxDataAtt.texture()->data();

    RenderTarget& ssrDataRT = context().gfx().renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::SSR_RESULT));
    const auto& ssrDataAtt = ssrDataRT.getAttachment(RTAttachmentType::Colour, 0);
    const TextureData ssrData = ssrDataAtt.texture()->data();

    GFX::BeginRenderPassCommand beginRenderPassCmd{};
    beginRenderPassCmd._target = RenderTargetID(RenderTargetUsage::SCREEN);
    beginRenderPassCmd._descriptor = _postFXTarget;
    beginRenderPassCmd._name = "DO_POSTFX_PASS";
    EnqueueCommand(bufferInOut, beginRenderPassCmd);

    GFX::BindPipelineCommand bindPipelineCmd;
    bindPipelineCmd._pipeline = _drawPipeline;
    EnqueueCommand(bufferInOut, bindPipelineCmd);

    if (_filtersDirty) {
        _drawConstants.set(_ID("vignetteEnabled"), GFX::PushConstantType::BOOL, getFilterState(FilterType::FILTER_VIGNETTE));
        _drawConstants.set(_ID("noiseEnabled"), GFX::PushConstantType::BOOL, getFilterState(FilterType::FILTER_NOISE));
        _drawConstants.set(_ID("underwaterEnabled"), GFX::PushConstantType::BOOL, getFilterState(FilterType::FILTER_UNDERWATER));
        _filtersDirty = false;
    };

    _drawConstants.set(_ID("_zPlanes"), GFX::PushConstantType::VEC2, camera->getZPlanes());
    EnqueueCommand(bufferInOut, GFX::SendPushConstantsCommand(_drawConstants));

    GFX::BindDescriptorSetsCommand bindDescriptorSetsCmd;
    bindDescriptorSetsCmd._set._textureData.add({ output, prbAtt.samplerHash(),to_U8(TexOperatorBindPoint::TEX_BIND_POINT_SCREEN) });
    bindDescriptorSetsCmd._set._textureData.add({ data0, s_samplerHash, to_U8(TexOperatorBindPoint::TEX_BIND_POINT_UNDERWATER) });
    bindDescriptorSetsCmd._set._textureData.add({ data1, s_samplerHash, to_U8(TexOperatorBindPoint::TEX_BIND_POINT_NOISE) });
    bindDescriptorSetsCmd._set._textureData.add({ data2, s_samplerHash, to_U8(TexOperatorBindPoint::TEX_BIND_POINT_BORDER) });
    bindDescriptorSetsCmd._set._textureData.add({ fxData, s_samplerHash, to_U8(TexOperatorBindPoint::TEX_BIND_POINT_POSTFXDATA) });
    bindDescriptorSetsCmd._set._textureData.add({ ssrData, s_samplerHash, to_U8(TexOperatorBindPoint::TEX_BIND_POINT_SSR) });
    EnqueueCommand(bufferInOut, bindDescriptorSetsCmd);

    GenericDrawCommand drawCommand;
    drawCommand._primitiveType = PrimitiveType::TRIANGLES;
    drawCommand._drawCount = 1;
    EnqueueCommand(bufferInOut, GFX::DrawCommand{ drawCommand });

    EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});

    EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
}

void PostFX::idle(const Configuration& config) {
    OPTICK_EVENT();

    // Update states
    if (getFilterState(FilterType::FILTER_NOISE)) {
        _noiseTimer += Time::Game::ElapsedMilliseconds();
        if (_noiseTimer > _tickInterval) {
            _noiseTimer = 0.0;
            _randomNoiseCoefficient = Random(1000) * 0.001f;
            _randomFlashCoefficient = Random(1000) * 0.001f;
        }

        _drawConstants.set(_ID("randomCoeffNoise"), GFX::PushConstantType::FLOAT, _randomNoiseCoefficient);
        _drawConstants.set(_ID("randomCoeffNoise"), GFX::PushConstantType::FLOAT, _randomFlashCoefficient);
    }

    _preRenderBatch.idle(config);
}

void PostFX::update(const U64 deltaTimeUS) {
    OPTICK_EVENT();

    if (_fadeActive) {
        _currentFadeTimeMS += Time::MicrosecondsToMilliseconds<D64>(deltaTimeUS);
        F32 fadeStrength = to_F32(std::min(_currentFadeTimeMS / _targetFadeTimeMS , 1.0));
        if (!_fadeOut) {
            fadeStrength = 1.0f - fadeStrength;
        }

        if (fadeStrength > 0.99) {
            if (_fadeWaitDurationMS < std::numeric_limits<D64>::epsilon()) {
                if (_fadeOutComplete) {
                    _fadeOutComplete();
                    _fadeOutComplete = DELEGATE<void>();
                }
            } else {
                _fadeWaitDurationMS -= Time::MicrosecondsToMilliseconds<D64>(deltaTimeUS);
            }
        }

        _drawConstants.set(_ID("_fadeStrength"), GFX::PushConstantType::FLOAT, fadeStrength);
        
        _fadeActive = fadeStrength > std::numeric_limits<D64>::epsilon();
        if (!_fadeActive) {
            _drawConstants.set(_ID("_fadeActive"), GFX::PushConstantType::BOOL, false);
            if (_fadeInComplete) {
                _fadeInComplete();
                _fadeInComplete = DELEGATE<void>();
            }
        }
    }

    _preRenderBatch.update(deltaTimeUS);
}

void PostFX::setFadeOut(const UColour3& targetColour, const D64 durationMS, const D64 waitDurationMS, DELEGATE<void> onComplete) {
    _drawConstants.set(_ID("_fadeColour"), GFX::PushConstantType::VEC4, Util::ToFloatColour(targetColour));
    _drawConstants.set(_ID("_fadeActive"), GFX::PushConstantType::BOOL, true);
    _targetFadeTimeMS = durationMS;
    _currentFadeTimeMS = 0.0;
    _fadeWaitDurationMS = waitDurationMS;
    _fadeOut = true;
    _fadeActive = true;
    _fadeOutComplete = MOV(onComplete);
}

// clear any fading effect currently active over the specified time interval
// set durationMS to instantly clear the fade effect
void PostFX::setFadeIn(const D64 durationMS, DELEGATE<void> onComplete) {
    _targetFadeTimeMS = durationMS;
    _currentFadeTimeMS = 0.0;
    _fadeOut = false;
    _fadeActive = true;
    _drawConstants.set(_ID("_fadeActive"), GFX::PushConstantType::BOOL, true);
    _fadeInComplete = MOV(onComplete);
}

void PostFX::setFadeOutIn(const UColour3& targetColour, const D64 durationFadeOutMS, const D64 waitDurationMS) {
    if (waitDurationMS > 0.0) {
        setFadeOutIn(targetColour, waitDurationMS * 0.5, waitDurationMS * 0.5, durationFadeOutMS);
    }
}

void PostFX::setFadeOutIn(const UColour3& targetColour, const D64 durationFadeOutMS, const D64 durationFadeInMS, const D64 waitDurationMS) {
    setFadeOut(targetColour, durationFadeOutMS, waitDurationMS, [this, durationFadeInMS]() {setFadeIn(durationFadeInMS); });
}

};
