#include "Headers/PostFX.h"
#include "Headers/PreRenderStage.h"
#include "Headers/PreRenderOperator.h"
#include "Headers/PreRenderStageBuilder.h"

#include "Core/Headers/ParamHandler.h"
#include "Core/Headers/ApplicationTimer.h"
#include "Core/Resources/Headers/ResourceCache.h"

#include "Managers/Headers/SceneManager.h"
#include "Rendering/Camera/Headers/Camera.h"
#include "Geometry/Shapes/Headers/Predefined/Quad3D.h"
#include "Platform/Video/Buffers/Framebuffer/Headers/Framebuffer.h"

namespace Divide {

PostFX::PostFX()
    : _underwaterTexture(nullptr),
      _anaglyphShader(nullptr),
      _postProcessingShader(nullptr),
      _noise(nullptr),
      _screenBorder(nullptr),
      _bloomFB(nullptr),
      _SSAO_FB(nullptr),
      _fxaaOP(nullptr),
      _dofOP(nullptr),
      _bloomOP(nullptr),
      _underwater(false),
      _depthPreview(false),
      _gfx(nullptr) {
    PreRenderStageBuilder::createInstance();
    ParamHandler::getInstance().setParam<bool>("postProcessing.enableVignette",
                                               false);
    ParamHandler::getInstance().setParam<bool>(
        "postProcessing.fullScreenDepthBuffer", false);
}

PostFX::~PostFX() {
    if (_postProcessingShader) {
        RemoveResource(_postProcessingShader);

        if (_gfx->anaglyphEnabled()) {
            RemoveResource(_anaglyphShader);
        }

        if (_underwaterTexture) {
            RemoveResource(_underwaterTexture);
        }

        if (_enableNoise) {
            RemoveResource(_noise);
            RemoveResource(_screenBorder);
        }

        MemoryManager::DELETE(_bloomFB);
        MemoryManager::DELETE(_SSAO_FB);
    }

    PreRenderStageBuilder::destroyInstance();
}

void PostFX::init(const vec2<U16>& resolution) {
    _resolutionCache = resolution;

    Console::printfn(Locale::get("START_POST_FX"));
    ParamHandler& par = ParamHandler::getInstance();
    _gfx = &GFX_DEVICE;
    _enableBloom = par.getParam<bool>("postProcessing.enableBloom");
    _enableSSAO = par.getParam<bool>("postProcessing.enableSSAO");
    _enableDOF = par.getParam<bool>("postProcessing.enableDepthOfField");
    _enableNoise = par.getParam<bool>("postProcessing.enableNoise");
    _enableVignette = par.getParam<bool>("postProcessing.enableVignette");
    _enableFXAA = _gfx->gpuState().FXAAEnabled();

    if (_gfx->postProcessingEnabled()) {
        ResourceDescriptor postFXShader("postProcessing");
        postFXShader.setThreadedLoading(false);
        std::stringstream ss;
        ss << "TEX_BIND_POINT_SCREEN "
           << std::to_string(
                  to_uint(TexOperatorBindPoint::TEX_BIND_POINT_SCREEN)) << ", ";
        ss << "TEX_BIND_POINT_BLOOM "
           << std::to_string(
                  to_uint(TexOperatorBindPoint::TEX_BIND_POINT_BLOOM)) << ", ";
        ss << "TEX_BIND_POINT_SSAO "
           << std::to_string(to_uint(TexOperatorBindPoint::TEX_BIND_POINT_SSAO))
           << ", ";
        ss << "TEX_BIND_POINT_NOISE "
           << std::to_string(
                  to_uint(TexOperatorBindPoint::TEX_BIND_POINT_NOISE)) << ", ";
        ss << "TEX_BIND_POINT_BORDER "
           << std::to_string(
                  to_uint(TexOperatorBindPoint::TEX_BIND_POINT_BORDER)) << ", ";
        ss << "TEX_BIND_POINT_UNDERWATER "
           << std::to_string(
                  to_uint(TexOperatorBindPoint::TEX_BIND_POINT_UNDERWATER));
        postFXShader.setPropertyList(stringAlg::toBase(ss.str()));
        _postProcessingShader = CreateResource<ShaderProgram>(postFXShader);

        ResourceDescriptor textureWaterCaustics("Underwater Caustics");
        textureWaterCaustics.setResourceLocation(
            par.getParam<stringImpl>("assetsLocation") +
            "/misc_images/terrain_water_NM.jpg");
        _underwaterTexture = CreateResource<Texture>(textureWaterCaustics);

        createOperators();

        _postProcessingShader->Uniform("_noiseTile", 0.05f);
        _postProcessingShader->Uniform("_noiseFactor", 0.02f);
        _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "Vignette"));  // 0
        _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "Noise"));  // 1
        _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "Bloom"));  // 2
        _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "SSAO"));  // 3
        _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "screenUnderwater"));  // 4
        _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "screenNormal"));  // 5
        _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "ColorPassThrough"));  // 6
        _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "outputScreen"));  // 7
        _shaderFunctionList.push_back(_postProcessingShader->GetSubroutineIndex(
            ShaderType::FRAGMENT, "outputDepth"));  // 8
        _shaderFunctionSelection.resize(
            _postProcessingShader->GetSubroutineUniformCount(
                ShaderType::FRAGMENT),
            0);
    }

    _timer = 0;
    _tickInterval = 1.0f / 24.0f;
    _randomNoiseCoefficient = 0;
    _randomFlashCoefficient = 0;

    updateResolution(resolution.width, resolution.height);

    // par.setParam("postProcessing.enableDepthOfField", false); //enable using
    // keyboard;
}

void PostFX::createOperators() {
    ParamHandler& par = ParamHandler::getInstance();

    PreRenderStageBuilder& stageBuilder = PreRenderStageBuilder::getInstance();
    Framebuffer* screenBuffer =
        _gfx->getRenderTarget(GFXDevice::RenderTarget::SCREEN);
    Framebuffer* depthBuffer =
        _gfx->getRenderTarget(GFXDevice::RenderTarget::DEPTH);

    if (_gfx->anaglyphEnabled()) {
        ResourceDescriptor anaglyph("anaglyph");
        anaglyph.setThreadedLoading(false);
        _anaglyphShader = CreateResource<ShaderProgram>(anaglyph);
        _anaglyphShader->Uniform(
            "texRightEye",
            to_uint(TexOperatorBindPoint::TEX_BIND_POINT_RIGHT_EYE));
        _anaglyphShader->Uniform(
            "texLeftEye",
            to_uint(TexOperatorBindPoint::TEX_BIND_POINT_LEFT_EYE));
    }

    // Bloom and Ambient Occlusion generate textures that are applied in the
    // PostFX shader
    if (_enableBloom && !_bloomFB) {
        _bloomFB = _gfx->newFB();
        _bloomOP = stageBuilder.addPreRenderOperator<BloomPreRenderOperator>(
            _enableBloom, _bloomFB, _resolutionCache);
        _bloomOP->addInputFB(screenBuffer);
        //_bloomOP->genericFlag(toneMapEnabled);
    }

    if (_enableSSAO && !_SSAO_FB) {
        _SSAO_FB = _gfx->newFB();
        PreRenderOperator* ssaoOP = nullptr;
        ssaoOP = stageBuilder.addPreRenderOperator<SSAOPreRenderOperator>(
            _enableSSAO, _SSAO_FB, _resolutionCache);
        ssaoOP->addInputFB(screenBuffer);
        ssaoOP->addInputFB(depthBuffer);
    }

    // DOF and FXAA modify the screen FB
    if (_enableDOF && !_dofOP) {
        _dofOP = stageBuilder.addPreRenderOperator<DoFPreRenderOperator>(
            _enableDOF, screenBuffer, _resolutionCache);
        _dofOP->addInputFB(screenBuffer);
        _dofOP->addInputFB(depthBuffer);
    }

    if (_enableFXAA && !_fxaaOP) {
        _fxaaOP = stageBuilder.addPreRenderOperator<FXAAPreRenderOperator>(
            _enableFXAA, screenBuffer, _resolutionCache);
        _fxaaOP->addInputFB(screenBuffer);
    }

    if (_enableNoise && !_noise) {
        ResourceDescriptor noiseTexture("noiseTexture");
        ResourceDescriptor borderTexture("borderTexture");
        noiseTexture.setResourceLocation(
            par.getParam<stringImpl>("assetsLocation") +
            "/misc_images//bruit_gaussien.jpg");
        borderTexture.setResourceLocation(
            par.getParam<stringImpl>("assetsLocation") +
            "/misc_images//vignette.jpeg");
        _noise = CreateResource<Texture>(noiseTexture);
        _screenBorder = CreateResource<Texture>(borderTexture);
    }
}

void PostFX::updateResolution(I32 width, I32 height) {
    if (!_gfx || !_gfx->postProcessingEnabled()) {
        return;
    }

    if (vec2<U16>(width, height) == _resolutionCache || width < 1 ||
        height < 1) {
        return;
    }

    _resolutionCache.set(width, height);

    PreRenderStageBuilder::getInstance().getPreRenderBatch()->reshape(width,
                                                                      height);
}

void PostFX::displayScene() {
    GFX::Scoped2DRendering scoped2D(true);

    PreRenderStageBuilder::getInstance().getPreRenderBatch()->execute();

    ShaderProgram* drawShader = nullptr;
    if (_gfx->anaglyphEnabled()) {
        drawShader = _anaglyphShader;
        _anaglyphShader->bind();
        _gfx->getRenderTarget(GFXDevice::RenderTarget::SCREEN)
            ->Bind(to_uint(
                TexOperatorBindPoint::TEX_BIND_POINT_RIGHT_EYE));  // right eye
                                                                   // buffer
        _gfx->getRenderTarget(GFXDevice::RenderTarget::ANAGLYPH)
            ->Bind(to_uint(
                TexOperatorBindPoint::TEX_BIND_POINT_LEFT_EYE));  // left eye
                                                                  // buffer
    } else {
        drawShader = _postProcessingShader;
        _postProcessingShader->bind();
        _postProcessingShader->SetSubroutines(ShaderType::FRAGMENT,
                                              _shaderFunctionSelection);

#ifdef _DEBUG
        _gfx->getRenderTarget(
                  _depthPreview ? GFXDevice::RenderTarget::DEPTH
                                : GFXDevice::RenderTarget::SCREEN)
            ->Bind(to_uint(TexOperatorBindPoint::TEX_BIND_POINT_SCREEN),
                   _depthPreview ? TextureDescriptor::AttachmentType::Depth
                                 : TextureDescriptor::AttachmentType::Color0);
#else
        _gfx->getRenderTarget(GFXDevice::RenderTarget::SCREEN)
            ->Bind(to_uint(TexOperatorBindPoint::TEX_BIND_POINT_SCREEN));
#endif

        if (_underwaterTexture) {
            _underwaterTexture->Bind(
                to_uint(TexOperatorBindPoint::TEX_BIND_POINT_UNDERWATER));
        }

        if (_noise) {
            _noise->Bind(to_uint(TexOperatorBindPoint::TEX_BIND_POINT_NOISE));
        }

        if (_screenBorder) {
            _screenBorder->Bind(
                to_uint(TexOperatorBindPoint::TEX_BIND_POINT_BORDER));
        }

        if (_bloomFB) {
            _bloomFB->Bind(to_uint(TexOperatorBindPoint::TEX_BIND_POINT_BLOOM));
        }

        if (_SSAO_FB) {
            _SSAO_FB->Bind(to_uint(TexOperatorBindPoint::TEX_BIND_POINT_SSAO));
        }
    }

    _gfx->drawPoints(1, _gfx->getDefaultStateBlock(true), drawShader);
}

void PostFX::idle() {
    if (!_gfx || !_gfx->postProcessingEnabled()) {
        return;
    }

    ParamHandler& par = ParamHandler::getInstance();
    // Update states

    if (!_postProcessingShader) {
        init(GFX_DEVICE.getRenderTarget(
                            GFXDevice::RenderTarget::SCREEN)
                 ->getResolution());
    }

    _underwater = GET_ACTIVE_SCENE().state().cameraUnderwater();
    _enableDOF = par.getParam<bool>("postProcessing.enableDepthOfField");
    _enableNoise = par.getParam<bool>("postProcessing.enableNoise");
    _enableVignette = par.getParam<bool>("postProcessing.enableVignette");
    _depthPreview = par.getParam<bool>("postProcessing.fullScreenDepthBuffer");
    _enableFXAA = _gfx->gpuState().FXAAEnabled();

    bool recompileShader = false;
    if (_enableBloom != par.getParam<bool>("postProcessing.enableBloom")) {
        _enableBloom = !_enableBloom;
        if (_enableBloom) {
            _postProcessingShader->addShaderDefine("POSTFX_ENABLE_BLOOM");
        } else {
            _postProcessingShader->removeShaderDefine("POSTFX_ENABLE_BLOOM");
        }
        recompileShader = true;
    }

    if (_enableSSAO != par.getParam<bool>("postProcessing.enableSSAO")) {
        _enableSSAO = !_enableSSAO;
        if (_enableSSAO) {
            _postProcessingShader->addShaderDefine("POSTFX_ENABLE_SSAO");
        } else {
            _postProcessingShader->removeShaderDefine("POSTFX_ENABLE_SSAO");
        }
        recompileShader = true;
    }

    createOperators();

    // recreate only the fragment shader
    if (recompileShader) {
        _postProcessingShader->recompile(false, true);
        _postProcessingShader->Uniform(
            "texScreen", to_int(TexOperatorBindPoint::TEX_BIND_POINT_SCREEN));
        _postProcessingShader->Uniform(
            "texBloom", to_int(TexOperatorBindPoint::TEX_BIND_POINT_BLOOM));
        _postProcessingShader->Uniform(
            "texSSAO", to_int(TexOperatorBindPoint::TEX_BIND_POINT_SSAO));
        _postProcessingShader->Uniform(
            "texNoise", to_int(TexOperatorBindPoint::TEX_BIND_POINT_NOISE));
        _postProcessingShader->Uniform(
            "texVignette", to_int(TexOperatorBindPoint::TEX_BIND_POINT_BORDER));
        _postProcessingShader->Uniform(
            "texWaterNoiseNM",
            to_int(TexOperatorBindPoint::TEX_BIND_POINT_UNDERWATER));
    }

    if (_enableBloom) {
        _postProcessingShader->Uniform(
            "bloomFactor", par.getParam<F32>("postProcessing.bloomFactor"));
    }

    if (_enableNoise) {
        _timer += Time::ElapsedMilliseconds();

        if (_timer > _tickInterval) {
            _timer = 0.0;
            _randomNoiseCoefficient = (F32)Random(1000) * 0.001f;
            _randomFlashCoefficient = (F32)Random(1000) * 0.001f;
        }

        _postProcessingShader->Uniform("randomCoeffNoise",
                                       _randomNoiseCoefficient);
        _postProcessingShader->Uniform("randomCoeffFlash",
                                       _randomFlashCoefficient);
    }

    _shaderFunctionSelection[0] =
        _enableVignette ? _shaderFunctionList[0] : _shaderFunctionList[6];
    _shaderFunctionSelection[1] =
        _enableNoise ? _shaderFunctionList[1] : _shaderFunctionList[6];
    _shaderFunctionSelection[2] =
        _enableBloom ? _shaderFunctionList[2] : _shaderFunctionList[6];
    _shaderFunctionSelection[3] =
        _enableSSAO ? _shaderFunctionList[3] : _shaderFunctionList[6];
    _shaderFunctionSelection[4] =
        _underwater ? _shaderFunctionList[4] : _shaderFunctionList[5];
    _shaderFunctionSelection[5] =
        _depthPreview ? _shaderFunctionList[8] : _shaderFunctionList[7];
}
};