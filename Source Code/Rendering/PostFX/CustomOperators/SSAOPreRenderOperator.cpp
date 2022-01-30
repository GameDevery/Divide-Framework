#include "stdafx.h"

#include "Headers/SSAOPreRenderOperator.h"

#include "Core/Resources/Headers/ResourceCache.h"
#include "Managers/Headers/SceneManager.h"
#include "Core/Headers/StringHelper.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Geometry/Shapes/Predefined/Headers/Quad3D.h"

#include "Rendering/PostFX/Headers/PreRenderBatch.h"

namespace Divide {

namespace {
    constexpr U8 SSAO_NOISE_SIZE = 4;
    constexpr U8 SSAO_BLUR_SIZE = SSAO_NOISE_SIZE / 2;
    constexpr U8 MAX_KERNEL_SIZE = 64;
    constexpr U8 MIN_KERNEL_SIZE = 8;
    vector<vec4<F32>> g_kernels;

    [[nodiscard]] const vector<vec4<F32>>& ComputeKernel(const U8 sampleCount) {
        g_kernels.resize(sampleCount);
        for (U16 i = 0; i < sampleCount; ++i) {
            vec3<F32>& k = g_kernels[i].xyz;
            k.set(Random(-1.f, 1.f),
                  Random(-1.f, 1.f),
                  Random( 0.f, 1.f)); // Kernel hemisphere points to positive Z-Axis.
            k.normalize();             // Normalize, so included in the hemisphere.
            k *= FastLerp(0.1f, 1.0f, SQUARED(to_F32(i) / to_F32(sampleCount))); // Create a scale value between [0;1].
        }

        return g_kernels;
    }
}

//ref: http://john-chapman-graphics.blogspot.co.uk/2013/01/ssao-tutorial.html
SSAOPreRenderOperator::SSAOPreRenderOperator(GFXDevice& context, PreRenderBatch& parent, ResourceCache* cache)
    : PreRenderOperator(context, parent, FilterType::FILTER_SS_AMBIENT_OCCLUSION) {
    const auto& config = context.context().config().rendering.postFX.ssao;
    _genHalfRes = config.UseHalfResolution;
    _blurThreshold[0] = config.FullRes.BlurThreshold;
    _blurThreshold[1] = config.HalfRes.BlurThreshold; 
    _blurSharpness[0] = config.FullRes.BlurSharpness;
    _blurSharpness[1] = config.HalfRes.BlurSharpness;
    _kernelSampleCount[0] = CLAMPED(config.FullRes.KernelSampleCount, MIN_KERNEL_SIZE, MAX_KERNEL_SIZE);
    _kernelSampleCount[1] = CLAMPED(config.HalfRes.KernelSampleCount, MIN_KERNEL_SIZE, MAX_KERNEL_SIZE);
    _blur[0] = config.FullRes.Blur;
    _blur[1] = config.HalfRes.Blur;
    _radius[0] = config.FullRes.Radius;
    _radius[1] = config.HalfRes.Radius;
    _power[0] = config.FullRes.Power;
    _power[1] = config.HalfRes.Power;
    _bias[0] = config.FullRes.Bias;
    _bias[1] = config.HalfRes.Bias;
    _maxRange[0] = config.FullRes.MaxRange;
    _maxRange[1] = config.HalfRes.MaxRange;
    _fadeStart[0] = config.FullRes.FadeDistance;
    _fadeStart[1] = config.HalfRes.FadeDistance;

    std::array<vec3<F32>, SQUARED(SSAO_NOISE_SIZE)> noiseData;

    for (vec3<F32>& noise : noiseData) {
        noise.set(Random(-1.f, 1.f), Random(-1.f, 1.f), 0.f);
        noise.normalize();
    }

    SamplerDescriptor nearestSampler = {};
    nearestSampler.wrapUVW(TextureWrap::CLAMP_TO_EDGE);
    nearestSampler.minFilter(TextureFilter::NEAREST);
    nearestSampler.magFilter(TextureFilter::NEAREST);
    nearestSampler.anisotropyLevel(0);

    SamplerDescriptor screenSampler = {};
    screenSampler.wrapUVW(TextureWrap::CLAMP_TO_EDGE);
    screenSampler.minFilter(TextureFilter::LINEAR);
    screenSampler.magFilter(TextureFilter::LINEAR);
    screenSampler.anisotropyLevel(0);

    SamplerDescriptor noiseSampler = {};
    noiseSampler.wrapUVW(TextureWrap::REPEAT);
    noiseSampler.minFilter(TextureFilter::NEAREST);
    noiseSampler.magFilter(TextureFilter::NEAREST);
    noiseSampler.anisotropyLevel(0);
    _noiseSampler = noiseSampler.getHash();

    const Str64 attachmentName("SSAOPreRenderOperator_NoiseTexture");

    TextureDescriptor noiseDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::RGB, GFXDataFormat::FLOAT_32);
    noiseDescriptor.mipMappingState(TextureDescriptor::MipMappingState::OFF);

    ResourceDescriptor textureAttachment(attachmentName);
    textureAttachment.propertyDescriptor(noiseDescriptor);
    _noiseTexture = CreateResource<Texture>(cache, textureAttachment);

    _noiseTexture->loadData({ (Byte*)noiseData.data(), noiseData.size() * sizeof(vec3<F32>) }, vec2<U16>(SSAO_NOISE_SIZE));

    {
        TextureDescriptor outputDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::RED, GFXDataFormat::FLOAT_16);
        outputDescriptor.mipMappingState(TextureDescriptor::MipMappingState::OFF);
        const vec2<U16> res = parent.screenRT()._rt->getResolution();
        RTAttachmentDescriptors att = {
        {
            outputDescriptor, nearestSampler.getHash(), RTAttachmentType::Colour },
        };

        RenderTargetDescriptor desc = {};
        desc._name = "SSAO_Out";
        desc._resolution = res;
        desc._attachmentCount = to_U8(att.size());
        desc._attachments = att.data();

        //Colour0 holds the AO texture
        _ssaoOutput = _context.renderTargetPool().allocateRT(desc);

        desc._name = "SSAO_Blur_Buffer";
        _ssaoBlurBuffer = _context.renderTargetPool().allocateRT(desc);

        desc._name = "SSAO_Out_HalfRes";
        desc._resolution /= 2;

        _ssaoHalfResOutput = _context.renderTargetPool().allocateRT(desc);
    }
    {
        TextureDescriptor outputDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::RGB, GFXDataFormat::FLOAT_32);
        outputDescriptor.mipMappingState(TextureDescriptor::MipMappingState::OFF);

        RTAttachmentDescriptors att = {
            { outputDescriptor, nearestSampler.getHash(), RTAttachmentType::Colour },
        };

        RenderTargetDescriptor desc = {};
        desc._name = "HalfRes_Normals_Depth";
        desc._resolution = parent.screenRT()._rt->getResolution() / 2;
        desc._attachmentCount = to_U8(att.size());
        desc._attachments = att.data();

        _halfDepthAndNormals = _context.renderTargetPool().allocateRT(desc);
    }

    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "baseVertexShaders.glsl";
    vertModule._variant = "FullScreenQuad";

    ShaderModuleDescriptor fragModule = {};
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "SSAOPass.glsl";

    { //Calc Full
        fragModule._variant = "SSAOCalc";
        fragModule._defines.resize(0);
        fragModule._defines.emplace_back(Util::StringFormat("SSAO_SAMPLE_COUNT %d", _kernelSampleCount[0]).c_str(), true);

        ShaderProgramDescriptor ssaoShaderDescriptor = {};
        ssaoShaderDescriptor._modules.push_back(vertModule);
        ssaoShaderDescriptor._modules.push_back(fragModule);

        ResourceDescriptor ssaoGenerate("SSAOCalc");
        ssaoGenerate.propertyDescriptor(ssaoShaderDescriptor);
        ssaoGenerate.waitForReady(false);
        _ssaoGenerateShader = CreateResource<ShaderProgram>(cache, ssaoGenerate);
    }
    { //Calc Half
        fragModule._variant = "SSAOCalc";
        fragModule._defines.resize(0);
        fragModule._defines.emplace_back(Util::StringFormat("SSAO_SAMPLE_COUNT %d", _kernelSampleCount[1]).c_str(), true);
        fragModule._defines.emplace_back("COMPUTE_HALF_RES", true);

        ShaderProgramDescriptor ssaoShaderDescriptor = {};
        ssaoShaderDescriptor._modules.push_back(vertModule);
        ssaoShaderDescriptor._modules.push_back(fragModule);

        ResourceDescriptor ssaoGenerateHalfRes("SSAOCalcHalfRes");
        ssaoGenerateHalfRes.propertyDescriptor(ssaoShaderDescriptor);
        ssaoGenerateHalfRes.waitForReady(false);
        _ssaoGenerateHalfResShader = CreateResource<ShaderProgram>(cache, ssaoGenerateHalfRes);
    }

    { //Blur
        fragModule._variant = "SSAOBlur.Nvidia";
        fragModule._defines.resize(0);
        fragModule._defines.emplace_back(Util::StringFormat("BLUR_SIZE %d", SSAO_BLUR_SIZE).c_str(), true);

        ShaderProgramDescriptor ssaoShaderDescriptor = {};
        ssaoShaderDescriptor._modules.push_back(vertModule);
        ssaoShaderDescriptor._modules.push_back(fragModule);

        ssaoShaderDescriptor._modules.back()._defines.emplace_back("HORIZONTAL", true);
        ResourceDescriptor ssaoBlurH("SSAOBlur.Horizontal");
        ssaoBlurH.propertyDescriptor(ssaoShaderDescriptor);
        ssaoBlurH.waitForReady(false);
        _ssaoBlurShaderHorizontal = CreateResource<ShaderProgram>(cache, ssaoBlurH);

        ssaoShaderDescriptor._modules.back()._defines.back() = { "VERTICAL", true };
        ResourceDescriptor ssaoBlurV("SSAOBlur.Vertical");
        ssaoBlurV.propertyDescriptor(ssaoShaderDescriptor);
        ssaoBlurV.waitForReady(false);
        _ssaoBlurShaderVertical = CreateResource<ShaderProgram>(cache, ssaoBlurV);
    }
    { //Pass-through
        fragModule._variant = "SSAOPassThrough";
        fragModule._defines.resize(0);

        ShaderProgramDescriptor ssaoShaderDescriptor  = {};
        ssaoShaderDescriptor._modules.push_back(vertModule);
        ssaoShaderDescriptor._modules.push_back(fragModule);

        ResourceDescriptor ssaoPassThrough("SSAOPAssThrough");
        ssaoPassThrough.propertyDescriptor(ssaoShaderDescriptor);
        ssaoPassThrough.waitForReady(false);

        _ssaoPassThroughShader = CreateResource<ShaderProgram>(cache, ssaoPassThrough);
    }
    { //DownSample
        fragModule._variant = "SSAODownsample";
        fragModule._defines.resize(0);

        ShaderProgramDescriptor ssaoShaderDescriptor = {};
        ssaoShaderDescriptor._modules.push_back(vertModule);
        ssaoShaderDescriptor._modules.push_back(fragModule);

        ResourceDescriptor ssaoDownSample("SSAODownSample");
        ssaoDownSample.propertyDescriptor(ssaoShaderDescriptor);
        ssaoDownSample.waitForReady(false);

        _ssaoDownSampleShader = CreateResource<ShaderProgram>(cache, ssaoDownSample);
    }
    { //UpSample
        fragModule._variant = "SSAOUpsample";
        fragModule._defines.resize(0);

        ShaderProgramDescriptor ssaoShaderDescriptor = {};
        ssaoShaderDescriptor._modules.push_back(vertModule);
        ssaoShaderDescriptor._modules.push_back(fragModule);

        ResourceDescriptor ssaoUpSample("SSAOUpSample");
        ssaoUpSample.propertyDescriptor(ssaoShaderDescriptor);
        ssaoUpSample.waitForReady(false);

        _ssaoUpSampleShader = CreateResource<ShaderProgram>(cache, ssaoUpSample);
    }

    _ssaoGenerateConstantsCmd._constants.set(_ID("sampleKernel"), GFX::PushConstantType::VEC4, ComputeKernel(sampleCount()));
    _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_RADIUS"), GFX::PushConstantType::FLOAT, radius());
    _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_INTENSITY"), GFX::PushConstantType::FLOAT, power());
    _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_BIAS"), GFX::PushConstantType::FLOAT, bias());
    _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_NOISE_SCALE"), GFX::PushConstantType::VEC2, (parent.screenRT()._rt->getResolution() * (_genHalfRes ? 0.5f : 1.f)) / SSAO_NOISE_SIZE);
    _ssaoGenerateConstantsCmd._constants.set(_ID("maxRange"), GFX::PushConstantType::FLOAT, maxRange());
    _ssaoGenerateConstantsCmd._constants.set(_ID("fadeStart"), GFX::PushConstantType::FLOAT, fadeStart());

    _ssaoBlurConstantsCmd._constants.set(_ID("depthThreshold"), GFX::PushConstantType::FLOAT, blurThreshold());
    _ssaoBlurConstantsCmd._constants.set(_ID("blurSharpness"), GFX::PushConstantType::FLOAT, blurSharpness());
    _ssaoBlurConstantsCmd._constants.set(_ID("blurKernelSize"), GFX::PushConstantType::INT, blurKernelSize());
}

SSAOPreRenderOperator::~SSAOPreRenderOperator() 
{
    _context.renderTargetPool().deallocateRT(_ssaoOutput);
    _context.renderTargetPool().deallocateRT(_halfDepthAndNormals);
    _context.renderTargetPool().deallocateRT(_ssaoHalfResOutput);
    _context.renderTargetPool().deallocateRT(_ssaoBlurBuffer);
}

bool SSAOPreRenderOperator::ready() const noexcept {
    if (_ssaoBlurShaderHorizontal->getState() == ResourceState::RES_LOADED && 
        _ssaoBlurShaderVertical->getState() == ResourceState::RES_LOADED && 
        _ssaoGenerateShader->getState() == ResourceState::RES_LOADED && 
        _ssaoGenerateHalfResShader->getState() == ResourceState::RES_LOADED &&
        _ssaoDownSampleShader->getState() == ResourceState::RES_LOADED &&
        _ssaoPassThroughShader->getState() == ResourceState::RES_LOADED &&
        _ssaoUpSampleShader->getState() == ResourceState::RES_LOADED)
    {
        return PreRenderOperator::ready();
    }

    return false;
}

void SSAOPreRenderOperator::reshape(const U16 width, const U16 height) {
    PreRenderOperator::reshape(width, height);

    _ssaoOutput._rt->resize(width, height);
    _halfDepthAndNormals._rt->resize(width / 2, height / 2);

    const vec2<F32> targetDim = vec2<F32>(width, height) * (_genHalfRes ? 0.5f : 1.f);

    _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_NOISE_SCALE"), GFX::PushConstantType::VEC2, targetDim  / SSAO_NOISE_SIZE);
}

void SSAOPreRenderOperator::genHalfRes(const bool state) {
    if (_genHalfRes != state) {
        _genHalfRes = state;
        _context.context().config().rendering.postFX.ssao.UseHalfResolution = state;
        _context.context().config().changed(true);

        const U16 width = state ? _halfDepthAndNormals._rt->getWidth() : _ssaoOutput._rt->getWidth();
        const U16 height = state ? _halfDepthAndNormals._rt->getHeight() : _ssaoOutput._rt->getHeight();

        _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_NOISE_SCALE"), GFX::PushConstantType::VEC2, vec2<F32>(width, height) / SSAO_NOISE_SIZE);
        _ssaoGenerateConstantsCmd._constants.set(_ID("sampleKernel"), GFX::PushConstantType::VEC4, ComputeKernel(sampleCount()));
        _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_RADIUS"), GFX::PushConstantType::FLOAT, radius());
        _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_INTENSITY"), GFX::PushConstantType::FLOAT, power());
        _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_BIAS"), GFX::PushConstantType::FLOAT, bias());
        _ssaoGenerateConstantsCmd._constants.set(_ID("maxRange"), GFX::PushConstantType::FLOAT, maxRange());
        _ssaoGenerateConstantsCmd._constants.set(_ID("fadeStart"), GFX::PushConstantType::FLOAT, fadeStart());
        _ssaoBlurConstantsCmd._constants.set(_ID("depthThreshold"), GFX::PushConstantType::FLOAT, blurThreshold());
        _ssaoBlurConstantsCmd._constants.set(_ID("blurSharpness"), GFX::PushConstantType::FLOAT, blurSharpness());
        _ssaoBlurConstantsCmd._constants.set(_ID("blurKernelSize"), GFX::PushConstantType::INT, blurKernelSize());
    }
}

void SSAOPreRenderOperator::radius(const F32 val) {
    if (!COMPARE(radius(), val)) {
        _radius[_genHalfRes ? 1 : 0] = val;
        _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_RADIUS"), GFX::PushConstantType::FLOAT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.Radius = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.Radius = val;
        }
        _context.context().config().changed(true);
    }
}

void SSAOPreRenderOperator::power(const F32 val) {
    if (!COMPARE(power(), val)) {
        _power[_genHalfRes ? 1 : 0] = val;
        _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_INTENSITY"), GFX::PushConstantType::FLOAT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.Power = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.Power = val;
        }
        _context.context().config().changed(true);
    }
}

void SSAOPreRenderOperator::bias(const F32 val) {
    if (!COMPARE(bias(), val)) {
        _bias[_genHalfRes ? 1 : 0] = val;
        _ssaoGenerateConstantsCmd._constants.set(_ID("SSAO_BIAS"), GFX::PushConstantType::FLOAT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.Bias = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.Bias = val;
        }
        _context.context().config().changed(true);
    }
}

void SSAOPreRenderOperator::blurResults(const bool state) noexcept {
    if (blurResults() != state) {
        _blur[_genHalfRes ? 1 : 0] = state;
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.Blur = state;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.Blur = state;
        }
        _context.context().config().changed(true);
    }
}

void SSAOPreRenderOperator::blurThreshold(const F32 val) {
    if (!COMPARE(blurThreshold(), val)) {
        _blurThreshold[_genHalfRes ? 1 : 0] = val;
        _ssaoBlurConstantsCmd._constants.set(_ID("depthThreshold"), GFX::PushConstantType::FLOAT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.BlurThreshold = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.BlurThreshold = val;
        }
        _context.context().config().changed(true);
    }
}

void SSAOPreRenderOperator::blurSharpness(const F32 val) {
    if (!COMPARE(blurSharpness(), val)) {
        _blurSharpness[_genHalfRes ? 1 : 0] = val;
        _ssaoBlurConstantsCmd._constants.set(_ID("blurSharpness"), GFX::PushConstantType::FLOAT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.BlurSharpness = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.BlurSharpness = val;
        }
        _context.context().config().changed(true);
    }
}

void SSAOPreRenderOperator::blurKernelSize(const I32 val) {
    if (blurKernelSize() != val) {
        _kernelSize[_genHalfRes ? 1 : 0] = val;
        _ssaoBlurConstantsCmd._constants.set(_ID("blurKernelSize"), GFX::PushConstantType::INT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.BlurKernelSize = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.BlurKernelSize = val;
        }
        _context.context().config().changed(true);
    }
}

void SSAOPreRenderOperator::maxRange(F32 val) {
    val = std::max(0.01f, val);

    if (!COMPARE(maxRange(), val)) {
        _maxRange[_genHalfRes ? 1 : 0] = val;
        _ssaoGenerateConstantsCmd._constants.set(_ID("maxRange"), GFX::PushConstantType::FLOAT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.MaxRange = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.MaxRange = val;
        }
        _context.context().config().changed(true);
    }
}

void SSAOPreRenderOperator::fadeStart(F32 val) {
    val = std::max(0.01f, val);

    if (!COMPARE(fadeStart(), val)) {
        _fadeStart[_genHalfRes ? 1 : 0] = val;
        _ssaoGenerateConstantsCmd._constants.set(_ID("fadeStart"), GFX::PushConstantType::FLOAT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.FadeDistance = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.FadeDistance = val;
        }
        _context.context().config().changed(true);
    }
}

U8 SSAOPreRenderOperator::sampleCount() const noexcept {
    return _kernelSampleCount[_genHalfRes ? 1 : 0];
}

void SSAOPreRenderOperator::prepare([[maybe_unused]] const PlayerIndex idx, GFX::CommandBuffer& bufferInOut) {
    PreRenderOperator::prepare(idx, bufferInOut);

    if (_stateChanged && !_enabled) {
        RTClearDescriptor clearDescriptor = {};
        clearDescriptor._clearDepth = true;
        clearDescriptor._clearColours = true;
        clearDescriptor._resetToDefault = true;

        GFX::ClearRenderTargetCommand clearMainTarget = {};
        clearMainTarget._target = _ssaoOutput._targetID;
        clearMainTarget._descriptor = clearDescriptor;
        EnqueueCommand(bufferInOut, clearMainTarget);
    }

    _stateChanged = false;
}

bool SSAOPreRenderOperator::execute([[maybe_unused]] const PlayerIndex idx, const CameraSnapshot& cameraSnapshot, [[maybe_unused]] const RenderTargetHandle& input, [[maybe_unused]] const RenderTargetHandle& output, GFX::CommandBuffer& bufferInOut) {
    assert(_enabled);

    static GFX::BindPipelineCommand s_downsamplePipelineCmd{};
    static GFX::BindPipelineCommand s_generateHalfResPipelineCmd{};
    static GFX::BindPipelineCommand s_upsamplePipelineCmd{};
    static GFX::BindPipelineCommand s_generateFullResPipelineCmd{};
    static GFX::BindPipelineCommand s_blurHorizontalPipelineCmd{};
    static GFX::BindPipelineCommand s_blurVerticalPipelineCmd{};
    static GFX::BindPipelineCommand s_passThroughPipelineCmd{};

    static bool s_commandsInit = false;
    if (!s_commandsInit) {
        s_commandsInit = true;

        PipelineDescriptor pipelineDescriptor = {};
        pipelineDescriptor._stateHash = _context.get2DStateBlock();
        pipelineDescriptor._shaderProgramHandle = _ssaoDownSampleShader->getGUID();

        s_downsamplePipelineCmd._pipeline = _context.newPipeline(pipelineDescriptor);

        pipelineDescriptor._shaderProgramHandle = _ssaoGenerateHalfResShader->getGUID();
        s_generateHalfResPipelineCmd._pipeline = _context.newPipeline(pipelineDescriptor);

        pipelineDescriptor._shaderProgramHandle = _ssaoUpSampleShader->getGUID();
        s_upsamplePipelineCmd._pipeline = _context.newPipeline(pipelineDescriptor);

        pipelineDescriptor._shaderProgramHandle = _ssaoGenerateShader->getGUID();
        s_generateFullResPipelineCmd._pipeline = _context.newPipeline(pipelineDescriptor);

        RenderStateBlock redChannelOnly = RenderStateBlock::get(_context.get2DStateBlock());
        redChannelOnly.setColourWrites(true, false, false, false);

        pipelineDescriptor._stateHash = redChannelOnly.getHash();
        pipelineDescriptor._shaderProgramHandle = _ssaoBlurShaderHorizontal->getGUID();
        s_blurHorizontalPipelineCmd._pipeline = _context.newPipeline(pipelineDescriptor);

        pipelineDescriptor._shaderProgramHandle = _ssaoBlurShaderVertical->getGUID();
        s_blurVerticalPipelineCmd._pipeline = _context.newPipeline(pipelineDescriptor);

        pipelineDescriptor._shaderProgramHandle = _ssaoPassThroughShader->getGUID();
        s_passThroughPipelineCmd._pipeline = _context.newPipeline(pipelineDescriptor);
    }

    _ssaoGenerateConstantsCmd._constants.set(_ID("zPlanes"),             GFX::PushConstantType::VEC2, cameraSnapshot._zPlanes);
    _ssaoGenerateConstantsCmd._constants.set(_ID("projectionMatrix"),    GFX::PushConstantType::MAT4, cameraSnapshot._projectionMatrix);
    _ssaoGenerateConstantsCmd._constants.set(_ID("invProjectionMatrix"), GFX::PushConstantType::MAT4, cameraSnapshot._invProjectionMatrix);

    const auto& depthAtt   = _parent.screenRT()._rt->getAttachment(RTAttachmentType::Depth, 0);
    const auto& normalsAtt = _parent.screenRT()._rt->getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::NORMALS));

    GFX::ClearRenderTargetCommand clearSSAOTargetCmd{};
    clearSSAOTargetCmd._target = _ssaoOutput._targetID;
    clearSSAOTargetCmd._descriptor._clearDepth = false;
    clearSSAOTargetCmd._descriptor._clearColours = true;
    GFX::EnqueueCommand(bufferInOut, clearSSAOTargetCmd);

    if(genHalfRes()) {
        { // DownSample depth and normals
            GFX::BeginRenderPassCommand* renderPassCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>(bufferInOut);
            renderPassCmd->_name = "DO_SSAO_DOWNSAMPLE_NORMALS";
            renderPassCmd->_target = _halfDepthAndNormals._targetID;

            GFX::EnqueueCommand(bufferInOut, s_downsamplePipelineCmd);

            DescriptorSet& set = GFX::EnqueueCommand<GFX::BindDescriptorSetsCommand>(bufferInOut)->_set;
            set._textureData.add(TextureEntry{ depthAtt.texture()->data(),   depthAtt.samplerHash(),   TextureUsage::DEPTH });
            set._textureData.add(TextureEntry{ normalsAtt.texture()->data(), normalsAtt.samplerHash(), TextureUsage::SCENE_NORMALS });

            GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

            GFX::EnqueueCommand<GFX::EndRenderPassCommand>(bufferInOut);
        }
        { // Generate Half Res AO
            GFX::BeginRenderPassCommand* renderPassCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>(bufferInOut);
            renderPassCmd->_name = "DO_SSAO_HALF_RES_CALC";
            renderPassCmd->_target = _ssaoHalfResOutput._targetID;

            GFX::EnqueueCommand(bufferInOut, s_generateHalfResPipelineCmd);

            GFX::EnqueueCommand(bufferInOut, _ssaoGenerateConstantsCmd);

            const auto& halfDepthAtt  = _halfDepthAndNormals._rt->getAttachment(RTAttachmentType::Colour, 0);

            DescriptorSet& set = GFX::EnqueueCommand<GFX::BindDescriptorSetsCommand>(bufferInOut)->_set;
            set._textureData.add(TextureEntry{ _noiseTexture->data(),         _noiseSampler,              TextureUsage::UNIT0 });
            set._textureData.add(TextureEntry{ halfDepthAtt.texture()->data(), halfDepthAtt.samplerHash(),TextureUsage::DEPTH });

            GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

            GFX::EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
        }
        { // UpSample AO
            GFX::BeginRenderPassCommand* renderPassCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>(bufferInOut);
            renderPassCmd->_name = "DO_SSAO_UPSAMPLE_AO";
            renderPassCmd->_target = _ssaoOutput._targetID;

            GFX::EnqueueCommand(bufferInOut, s_upsamplePipelineCmd);

            SamplerDescriptor linearSampler = {};
            linearSampler.wrapUVW(TextureWrap::CLAMP_TO_EDGE);
            linearSampler.minFilter(TextureFilter::LINEAR);
            linearSampler.magFilter(TextureFilter::LINEAR);
            linearSampler.anisotropyLevel(0);

            const auto& halfResAOAtt = _ssaoHalfResOutput._rt->getAttachment(RTAttachmentType::Colour, 0);
            const auto& halfDepthAtt = _halfDepthAndNormals._rt->getAttachment(RTAttachmentType::Colour, 0);

            DescriptorSet& set = GFX::EnqueueCommand<GFX::BindDescriptorSetsCommand>(bufferInOut)->_set;
            set._textureData.add(TextureEntry{ halfResAOAtt.texture()->data(), linearSampler.getHash(),    TextureUsage::UNIT0 });
            set._textureData.add(TextureEntry{ halfResAOAtt.texture()->data(), halfResAOAtt.samplerHash(), TextureUsage::UNIT1 });
            set._textureData.add(TextureEntry{ halfDepthAtt.texture()->data(), halfDepthAtt.samplerHash(), TextureUsage::NORMALMAP });
            set._textureData.add(TextureEntry{ depthAtt.texture()->data(),     depthAtt.samplerHash(),     TextureUsage::DEPTH });

            GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

            GFX::EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
        }
    } else {
        { // Generate Full Res AO
            GFX::BeginRenderPassCommand* renderPassCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>(bufferInOut);
            renderPassCmd->_name = "DO_SSAO_CALC";
            renderPassCmd->_target = _ssaoOutput._targetID;

            GFX::EnqueueCommand(bufferInOut, s_generateFullResPipelineCmd);

            DescriptorSet& set = GFX::EnqueueCommand<GFX::BindDescriptorSetsCommand>(bufferInOut)->_set;
            set._textureData.add(TextureEntry{ _noiseTexture->data(),        _noiseSampler,            TextureUsage::UNIT0 });
            set._textureData.add(TextureEntry{ normalsAtt.texture()->data(), normalsAtt.samplerHash(), TextureUsage::UNIT1 });
            set._textureData.add(TextureEntry{ depthAtt.texture()->data(),   depthAtt.samplerHash(),   TextureUsage::DEPTH });

            GFX::EnqueueCommand(bufferInOut, _ssaoGenerateConstantsCmd);

            GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

            GFX::EnqueueCommand<GFX::EndRenderPassCommand>(bufferInOut);
        }
    }
    {
        const auto& ssaoAtt = _ssaoOutput._rt->getAttachment(RTAttachmentType::Colour, 0);

        if (blurResults() && blurKernelSize() > 0) {
            _ssaoBlurConstantsCmd._constants.set(_ID("invProjectionMatrix"), GFX::PushConstantType::MAT4, cameraSnapshot._invProjectionMatrix);
            _ssaoBlurConstantsCmd._constants.set(_ID("zPlanes"), GFX::PushConstantType::VEC2, cameraSnapshot._zPlanes);
            _ssaoBlurConstantsCmd._constants.set(_ID("texelSize"), GFX::PushConstantType::VEC2, vec2<F32>{ 1.f / ssaoAtt.texture()->width(), 1.f / ssaoAtt.texture()->height() });

            // Blur AO
            { //Horizontal
                GFX::BeginRenderPassCommand* renderPassCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>(bufferInOut);
                renderPassCmd->_name = "DO_SSAO_BLUR_HORIZONTAL";
                renderPassCmd->_target = _ssaoBlurBuffer._targetID;

                GFX::EnqueueCommand(bufferInOut, s_blurHorizontalPipelineCmd);

                GFX::EnqueueCommand(bufferInOut, _ssaoBlurConstantsCmd);

                DescriptorSet& set = GFX::EnqueueCommand<GFX::BindDescriptorSetsCommand>(bufferInOut)->_set;
                set._textureData.add(TextureEntry{ depthAtt.texture()->data(),   depthAtt.samplerHash(),  TextureUsage::DEPTH });
                set._textureData.add(TextureEntry{ normalsAtt.texture()->data(), normalsAtt.samplerHash(), TextureUsage::SCENE_NORMALS });
                set._textureData.add(TextureEntry{ ssaoAtt.texture()->data(),    ssaoAtt.samplerHash(),   TextureUsage::UNIT0 });

                GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

                GFX::EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
            }
            { //Vertical
                GFX::BeginRenderPassCommand* renderPassCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>(bufferInOut);
                renderPassCmd->_name = "DO_SSAO_BLUR_VERTICAL";
                renderPassCmd->_target = { RenderTargetUsage::SSAO_RESULT };

                GFX::EnqueueCommand(bufferInOut, s_blurVerticalPipelineCmd);

                GFX::EnqueueCommand(bufferInOut, _ssaoBlurConstantsCmd);

                const auto& horizBlur = _ssaoBlurBuffer._rt->getAttachment(RTAttachmentType::Colour, 0);
                DescriptorSet& set = GFX::EnqueueCommand<GFX::BindDescriptorSetsCommand>(bufferInOut)->_set;
                set._textureData.add(TextureEntry{ depthAtt.texture()->data(),   depthAtt.samplerHash(),  TextureUsage::DEPTH });
                set._textureData.add(TextureEntry{ normalsAtt.texture()->data(), normalsAtt.samplerHash(), TextureUsage::SCENE_NORMALS });
                set._textureData.add(TextureEntry{ horizBlur.texture()->data(),  ssaoAtt.samplerHash(),   TextureUsage::UNIT0 });

                GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

                GFX::EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
            }
        } else {
            GFX::BeginRenderPassCommand* renderPassCmd = GFX::EnqueueCommand<GFX::BeginRenderPassCommand>(bufferInOut);
            renderPassCmd->_name = "DO_SSAO_PASS_THROUGH";
            renderPassCmd->_target = { RenderTargetUsage::SSAO_RESULT };

            GFX::EnqueueCommand(bufferInOut, s_passThroughPipelineCmd);

            DescriptorSet& set = GFX::EnqueueCommand<GFX::BindDescriptorSetsCommand>(bufferInOut)->_set;
            set._textureData.add(TextureEntry{ ssaoAtt.texture()->data(), ssaoAtt.samplerHash(), TextureUsage::UNIT0 });

            GFX::EnqueueCommand(bufferInOut, _triangleDrawCmd);

            GFX::EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
        }
    }

    // No need to swap targets
    return false;
}

}