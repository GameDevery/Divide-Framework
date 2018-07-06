#include "Headers/PostFX.h"
#include "Headers/PreRenderOperator.h"

#include "Core/Headers/ParamHandler.h"
#include "Core/Headers/StringHelper.h"
#include "Core/Time/Headers/ApplicationTimer.h"
#include "Core/Resources/Headers/ResourceCache.h"

#include "Managers/Headers/SceneManager.h"
#include "Rendering/Camera/Headers/Camera.h"
#include "Geometry/Shapes/Headers/Predefined/Quad3D.h"
#include "Platform/Video/Buffers/RenderTarget/Headers/RenderTarget.h"

namespace Divide {

PostFX::PostFX()
    : _preRenderBatch(nullptr),
      _screenBorder(nullptr),
      _noise(nullptr),
      _randomNoiseCoefficient(0.0f),
      _randomFlashCoefficient(0.0f),
      _noiseTimer(0.0),
      _tickInterval(1),
      _postProcessingShader(nullptr),
      _underwaterTexture(nullptr),
      _gfx(nullptr),
      _currentFadeTimeMS(0.0),
      _targetFadeTimeMS(0.0),
      _fadeWaitDurationMS(0.0),
      _fadeOut(false),
      _fadeActive(false)
{
    ParamHandler::instance().setParam<bool>(_ID("postProcessing.enableVignette"), false);

    _postFXTarget.disableState(RTDrawDescriptor::State::CLEAR_DEPTH_BUFFER);
    _postFXTarget.drawMask().disableAll();
    _postFXTarget.drawMask().setEnabled(RTAttachment::Type::Colour, 0, true);

    _filterStackCount.fill(0);
}

PostFX::~PostFX()
{
    if (_preRenderBatch) {
        _preRenderBatch->destroy();
        MemoryManager::SAFE_DELETE(_preRenderBatch);
    }
}

void PostFX::init(GFXDevice& context, ResourceCache& cache) {
    Console::printfn(Locale::get(_ID("START_POST_FX")));
    _gfx = &context;
    _preRenderBatch = MemoryManager_NEW PreRenderBatch(context, cache);

    ResourceDescriptor postFXShader("postProcessing");
    postFXShader.setThreadedLoading(false);
    postFXShader.setPropertyList(
    Util::StringFormat("TEX_BIND_POINT_SCREEN %d, "
                       "TEX_BIND_POINT_NOISE %d, "
                       "TEX_BIND_POINT_BORDER %d, "
                       "TEX_BIND_POINT_UNDERWATER %d",
                        to_const_uint(TexOperatorBindPoint::TEX_BIND_POINT_SCREEN),
                        to_const_uint(TexOperatorBindPoint::TEX_BIND_POINT_NOISE),
                        to_const_uint(TexOperatorBindPoint::TEX_BIND_POINT_BORDER),
                        to_const_uint(TexOperatorBindPoint::TEX_BIND_POINT_UNDERWATER)).c_str());

    _postProcessingShader = CreateResource<ShaderProgram>(cache, postFXShader);
    _postProcessingShader->Uniform("_noiseTile", 0.05f);
    _postProcessingShader->Uniform("_noiseFactor", 0.02f);
    _postProcessingShader->Uniform("_fadeActive", false);
    
    _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "Vignette"));  // 0
    _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "Noise"));  // 1
    _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "screenUnderwater"));  // 2
    _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "screenNormal"));  // 3
    _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "ColourPassThrough"));  // 4

    _shaderFunctionSelection.resize(
        _postProcessingShader->GetSubroutineUniformCount(ShaderType::FRAGMENT), 0);
    
    ResourceDescriptor textureWaterCaustics("Underwater Caustics");
    textureWaterCaustics.setResourceName("terrain_water_NM.jpg");
    textureWaterCaustics.setResourceLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
    textureWaterCaustics.setEnumValue(to_const_uint(TextureType::TEXTURE_2D));
    _underwaterTexture = CreateResource<Texture>(cache, textureWaterCaustics);

     ResourceDescriptor noiseTexture("noiseTexture");
     noiseTexture.setResourceName("bruit_gaussien.jpg");
     noiseTexture.setResourceLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
     noiseTexture.setEnumValue(to_const_uint(TextureType::TEXTURE_2D));
     _noise = CreateResource<Texture>(cache, noiseTexture);

     ResourceDescriptor borderTexture("borderTexture");
     borderTexture.setResourceName("vignette.jpeg");
     borderTexture.setResourceLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
     borderTexture.setEnumValue(to_const_uint(TextureType::TEXTURE_2D));
     _screenBorder = CreateResource<Texture>(cache, borderTexture);

     _drawCommand.primitiveType(PrimitiveType::TRIANGLES);
     _drawCommand.drawCount(1);
     _drawCommand.shaderProgram(_postProcessingShader);

     _preRenderBatch->init(RenderTargetID(RenderTargetUsage::SCREEN));

    _noiseTimer = 0.0;
    _tickInterval = 1.0f / 24.0f;
    _randomNoiseCoefficient = 0;
    _randomFlashCoefficient = 0;
}

void PostFX::updateResolution(U16 width, U16 height) {
    if ((_resolutionCache.width == width &&
         _resolutionCache.height == height)|| 
        width < 1 || height < 1)
    {
        return;
    }

    _resolutionCache.set(width, height);

    _preRenderBatch->reshape(width, height);
}

void PostFX::apply(GFXDevice& context) {
    _shaderFunctionSelection[0] = _shaderFunctionList[getFilterState(FilterType::FILTER_VIGNETTE) ? 0 : 4];
    _shaderFunctionSelection[1] = _shaderFunctionList[getFilterState(FilterType::FILTER_NOISE) ? 1 : 4];
    _shaderFunctionSelection[2] = _shaderFunctionList[getFilterState(FilterType::FILTER_UNDERWATER) ? 2 : 3];

    GFX::Scoped2DRendering scoped2D(context);
    _preRenderBatch->execute(_filterStackCount);
    _postProcessingShader->bind();
    _postProcessingShader->SetSubroutines(ShaderType::FRAGMENT, _shaderFunctionSelection);

    _preRenderBatch->bindOutput(to_const_ubyte(TexOperatorBindPoint::TEX_BIND_POINT_SCREEN));
    _underwaterTexture->bind(to_const_ubyte(TexOperatorBindPoint::TEX_BIND_POINT_UNDERWATER));
    _noise->bind(to_const_ubyte(TexOperatorBindPoint::TEX_BIND_POINT_NOISE));
    _screenBorder->bind(to_const_ubyte(TexOperatorBindPoint::TEX_BIND_POINT_BORDER));

    RenderTarget& screenRT = _gfx->renderTarget(RenderTargetID(RenderTargetUsage::SCREEN));

    screenRT.begin(_postFXTarget);
        _drawCommand.stateHash(_gfx->getDefaultStateBlock(true));
        _gfx->draw(_drawCommand);
    screenRT.end();
}

void PostFX::idle(const Configuration& config) {
    // Update states
    if (getFilterState(FilterType::FILTER_NOISE)) {
        _noiseTimer += Time::ElapsedMilliseconds();
        if (_noiseTimer > _tickInterval) {
            _noiseTimer = 0.0;
            _randomNoiseCoefficient = Random(1000) * 0.001f;
            _randomFlashCoefficient = Random(1000) * 0.001f;
        }

        _postProcessingShader->Uniform("randomCoeffNoise", _randomNoiseCoefficient);
        _postProcessingShader->Uniform("randomCoeffFlash", _randomFlashCoefficient);
    }

    _preRenderBatch->idle(config);
}

void PostFX::update(const U64 deltaTime) {
    if (_fadeActive) {
        _currentFadeTimeMS += Time::MicrosecondsToMilliseconds<D64>(deltaTime);
        F32 fadeStrength = to_float(std::min(_currentFadeTimeMS / _targetFadeTimeMS , 1.0));
        if (!_fadeOut) {
            fadeStrength = 1.0f - fadeStrength;
        }

        if (fadeStrength > 0.99) {
            if (_fadeWaitDurationMS < EPSILON_D64) {
                if (_fadeOutComplete) {
                    _fadeOutComplete();
                    _fadeOutComplete = DELEGATE_CBK<>();
                }
            } else {
                _fadeWaitDurationMS -= Time::MicrosecondsToMilliseconds<D64>(deltaTime);
            }
        }

        _postProcessingShader->Uniform("_fadeStrength", fadeStrength);
        
        _fadeActive = fadeStrength > EPSILON_D64;
        if (!_fadeActive) {
            _postProcessingShader->Uniform("_fadeActive", false);
            if (_fadeInComplete) {
                _fadeInComplete();
                _fadeInComplete = DELEGATE_CBK<>();
            }
        }
    }
}

void PostFX::setFadeOut(const vec4<U8>& targetColour, D64 durationMS, D64 waitDurationMS, DELEGATE_CBK<> onComplete) {
    _postProcessingShader->Uniform("_fadeColour", Util::ToFloatColour(targetColour));
    _targetFadeTimeMS = durationMS;
    _currentFadeTimeMS = 0.0;
    _fadeWaitDurationMS = waitDurationMS;
    _fadeOut = true;
    _fadeActive = true;
    _postProcessingShader->Uniform("_fadeActive", true);
    _fadeOutComplete = onComplete;
}

// clear any fading effect currently active over the specified time interval
// set durationMS to instantly clear the fade effect
void PostFX::setFadeIn(D64 durationMS, DELEGATE_CBK<> onComplete) {
    _targetFadeTimeMS = durationMS;
    _currentFadeTimeMS = 0.0;
    _fadeOut = false;
    _fadeActive = true;
    _postProcessingShader->Uniform("_fadeActive", true);
    _fadeInComplete = onComplete;
}

void PostFX::setFadeOutIn(const vec4<U8>& targetColour, D64 durationFadeOutMS, D64 durationMS) {
    if (durationMS > 0.0) {
        setFadeOutIn(targetColour, durationMS * 0.5, durationMS * 0.5, durationFadeOutMS);
    }
}

void PostFX::setFadeOutIn(const vec4<U8>& targetColour, D64 durationFadeOutMS, D64 durationFadeInMS, D64 waitDurationMS) {
    setFadeOut(targetColour, durationFadeOutMS, waitDurationMS, [this, durationFadeInMS]() {setFadeIn(durationFadeInMS); });
}

};
