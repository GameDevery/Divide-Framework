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
    vectorEASTL<vec4<F32>> g_kernels;

    [[nodiscard]] const vectorEASTL<vec4<F32>>& ComputeKernel(const U8 sampleCount) {
        std::uniform_real_distribution<F32> randomFloats(0.0f, 1.0f);
        std::default_random_engine generator;

        g_kernels.resize(sampleCount);
        for (U16 i = 0; i < sampleCount; ++i) {
            vec3<F32>& k = g_kernels[i].xyz;
            k.set(randomFloats(generator) * 2.0f - 1.0f,
                  randomFloats(generator) * 2.0f - 1.0f,
                  randomFloats(generator)); // Kernel hemisphere points to positive Z-Axis.
            k.normalize();             // Normalize, so included in the hemisphere.
            k *= randomFloats(generator);
            const F32 scale = FastLerp(0.1f, 1.0f, SQUARED(to_F32(i) / to_F32(sampleCount))); // Create a scale value between [0;1].
            k *= scale;
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

    std::array<vec3<F32>, SSAO_NOISE_SIZE * SSAO_NOISE_SIZE> noiseData;

    std::uniform_real_distribution<F32> randomFloats(-1.f, 1.f);
    std::default_random_engine generator;
    for (vec3<F32>& noise : noiseData) {
        noise.set(randomFloats(generator), randomFloats(generator), 0.f);
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

    Str64 attachmentName("SSAOPreRenderOperator_NoiseTexture");

    TextureDescriptor noiseDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::RGB, GFXDataFormat::FLOAT_32);
    noiseDescriptor.mipCount(1u);

    ResourceDescriptor textureAttachment(attachmentName);
    textureAttachment.propertyDescriptor(noiseDescriptor);
    _noiseTexture = CreateResource<Texture>(cache, textureAttachment);

    _noiseTexture->loadData({ (Byte*)noiseData.data(), noiseData.size() * sizeof(vec3<F32>) }, vec2<U16>(SSAO_NOISE_SIZE));

    {

        TextureDescriptor outputDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::RED, GFXDataFormat::FLOAT_16);
        outputDescriptor.mipCount(1u);

        RTAttachmentDescriptors att = {
            { outputDescriptor, nearestSampler.getHash(), RTAttachmentType::Colour },
        };

        RenderTargetDescriptor desc = {};
        desc._name = "SSAO_Out";
        desc._resolution = parent.screenRT()._rt->getResolution();
        desc._attachmentCount = to_U8(att.size());
        desc._attachments = att.data();

        //Colour0 holds the AO texture
        _ssaoOutput = _context.renderTargetPool().allocateRT(desc);

        desc._name = "SSAO_Out_HalfRes";
        desc._resolution /= 2;

        _ssaoHalfResOutput = _context.renderTargetPool().allocateRT(desc);
    }

    {
        TextureDescriptor outputDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::RGB, GFXDataFormat::FLOAT_32);
        outputDescriptor.mipCount(1u);

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
        fragModule._variant = "SSAOBlur";
        fragModule._defines.resize(0);
        fragModule._defines.emplace_back(Util::StringFormat("BLUR_SIZE %d", SSAO_BLUR_SIZE).c_str(), true);

        ShaderProgramDescriptor ssaoShaderDescriptor = {};
        ssaoShaderDescriptor._modules.push_back(vertModule);
        ssaoShaderDescriptor._modules.push_back(fragModule);

        ResourceDescriptor ssaoBlur("SSAOBlur");
        ssaoBlur.propertyDescriptor(ssaoShaderDescriptor);
        ssaoBlur.waitForReady(false);
        _ssaoBlurShader = CreateResource<ShaderProgram>(cache, ssaoBlur);
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

    _ssaoGenerateConstants.set(_ID("sampleKernel"), GFX::PushConstantType::VEC4, ComputeKernel(sampleCount()));
    _ssaoGenerateConstants.set(_ID("SSAO_RADIUS"), GFX::PushConstantType::FLOAT, radius());
    _ssaoGenerateConstants.set(_ID("SSAO_INTENSITY"), GFX::PushConstantType::FLOAT, power());
    _ssaoGenerateConstants.set(_ID("SSAO_BIAS"), GFX::PushConstantType::FLOAT, bias());
    _ssaoGenerateConstants.set(_ID("SSAO_NOISE_SCALE"), GFX::PushConstantType::VEC2, vec2<F32>(1920.f,1080.f) / SSAO_NOISE_SIZE);
    _ssaoGenerateConstants.set(_ID("maxRange"), GFX::PushConstantType::FLOAT, maxRange());
    _ssaoGenerateConstants.set(_ID("fadeStart"), GFX::PushConstantType::FLOAT, fadeStart());

    _ssaoBlurConstants.set(_ID("depthThreshold"), GFX::PushConstantType::FLOAT, blurThreshold());
}

SSAOPreRenderOperator::~SSAOPreRenderOperator() 
{
    _context.renderTargetPool().deallocateRT(_ssaoOutput);
    _context.renderTargetPool().deallocateRT(_halfDepthAndNormals);
    _context.renderTargetPool().deallocateRT(_ssaoHalfResOutput);
}

bool SSAOPreRenderOperator::ready() const {
    if (_ssaoBlurShader->getState() == ResourceState::RES_LOADED && 
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

    _ssaoGenerateConstants.set(_ID("SSAO_NOISE_SCALE"), GFX::PushConstantType::VEC2, targetDim  / SSAO_NOISE_SIZE);
}

void SSAOPreRenderOperator::genHalfRes(const bool state) {
    if (_genHalfRes != state) {
        _genHalfRes = state;
        _context.context().config().rendering.postFX.ssao.UseHalfResolution = state;
        _context.context().config().changed(true);

        const U16 width = state ? _halfDepthAndNormals._rt->getWidth() : _ssaoOutput._rt->getWidth();
        const U16 height = state ? _halfDepthAndNormals._rt->getHeight() : _ssaoOutput._rt->getHeight();

        _ssaoGenerateConstants.set(_ID("SSAO_NOISE_SCALE"), GFX::PushConstantType::VEC2, vec2<F32>(width, height) / SSAO_NOISE_SIZE);
        _ssaoGenerateConstants.set(_ID("sampleKernel"), GFX::PushConstantType::VEC4, ComputeKernel(sampleCount()));
        _ssaoGenerateConstants.set(_ID("SSAO_RADIUS"), GFX::PushConstantType::FLOAT, radius());
        _ssaoGenerateConstants.set(_ID("SSAO_INTENSITY"), GFX::PushConstantType::FLOAT, power());
        _ssaoGenerateConstants.set(_ID("SSAO_BIAS"), GFX::PushConstantType::FLOAT, bias());
        _ssaoGenerateConstants.set(_ID("maxRange"), GFX::PushConstantType::FLOAT, maxRange());
        _ssaoGenerateConstants.set(_ID("fadeStart"), GFX::PushConstantType::FLOAT, fadeStart());
        _ssaoBlurConstants.set(_ID("depthThreshold"), GFX::PushConstantType::FLOAT, blurThreshold());
    }
}

void SSAOPreRenderOperator::radius(const F32 val) {
    if (!COMPARE(radius(), val)) {
        _radius[_genHalfRes ? 1 : 0] = val;
        _ssaoGenerateConstants.set(_ID("SSAO_RADIUS"), GFX::PushConstantType::FLOAT, val);
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
        _ssaoGenerateConstants.set(_ID("SSAO_INTENSITY"), GFX::PushConstantType::FLOAT, val);
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
        _ssaoGenerateConstants.set(_ID("SSAO_BIAS"), GFX::PushConstantType::FLOAT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.Bias = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.Bias = val;
        }
        _context.context().config().changed(true);
    }
}

void SSAOPreRenderOperator::blurResults(const bool state) {
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
        _ssaoBlurConstants.set(_ID("depthThreshold"), GFX::PushConstantType::FLOAT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.BlurThreshold = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.BlurThreshold = val;
        }
        _context.context().config().changed(true);
    }
}

void SSAOPreRenderOperator::maxRange(F32 val) {
    val = std::max(0.01f, val);

    if (!COMPARE(maxRange(), val)) {
        _maxRange[_genHalfRes ? 1 : 0] = val;
        _ssaoGenerateConstants.set(_ID("maxRange"), GFX::PushConstantType::FLOAT, val);
        if (_genHalfRes) {
            _context.context().config().rendering.postFX.ssao.HalfRes.MaxRange = val;
        } else {
            _context.context().config().rendering.postFX.ssao.FullRes.MaxRange = val;
        }
        _context.context().config().changed(true);

        // Make sure we clamp fade properly
        fadeStart(fadeStart());
    }
}

void SSAOPreRenderOperator::fadeStart(F32 val) {
    val = std::min(val, maxRange()- 0.01f);

    if (!COMPARE(fadeStart(), val)) {
        _fadeStart[_genHalfRes ? 1 : 0] = val;
        _ssaoGenerateConstants.set(_ID("fadeStart"), GFX::PushConstantType::FLOAT, val);
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

void SSAOPreRenderOperator::prepare(const Camera* camera, GFX::CommandBuffer& bufferInOut) {
    if (_enabled) {
        RenderStateBlock redChannelOnly = RenderStateBlock::get(_context.get2DStateBlock());
        redChannelOnly.setColourWrites(true, false, false, false);

        PipelineDescriptor pipelineDescriptor = {};
        pipelineDescriptor._stateHash = _context.get2DStateBlock();

        const mat4<F32> projectionMatrix = camera->projectionMatrix();
        mat4<F32> invProjectionMatrix = GetInverse(projectionMatrix);

        _ssaoGenerateConstants.set(_ID("zPlanes"), GFX::PushConstantType::VEC2, camera->getZPlanes());
        _ssaoGenerateConstants.set(_ID("projectionMatrix"), GFX::PushConstantType::MAT4, projectionMatrix);
        _ssaoGenerateConstants.set(_ID("invProjectionMatrix"), GFX::PushConstantType::MAT4, invProjectionMatrix);

        if(genHalfRes()) {
            { // DownSample depth and normals
                GFX::BeginRenderPassCommand beginRenderPassCmd = {};
                beginRenderPassCmd._target = _halfDepthAndNormals._targetID;
                beginRenderPassCmd._name = "DO_SSAO_DOWNSAMPLE_NORMALS";
                EnqueueCommand(bufferInOut, beginRenderPassCmd);

                pipelineDescriptor._shaderProgramHandle = _ssaoDownSampleShader->getGUID();
                EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _context.newPipeline(pipelineDescriptor) });

                const auto& depthAtt = _parent.screenRT()._rt->getAttachment(RTAttachmentType::Depth, 0);
                const auto& screenAtt = _parent.screenRT()._rt->getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::NORMALS_AND_VELOCITY));

                GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
                descriptorSetCmd._set._textureData.add({ depthAtt.texture()->data(), depthAtt.samplerHash(), TextureUsage::DEPTH });
                descriptorSetCmd._set._textureData.add({ screenAtt.texture()->data(), screenAtt.samplerHash(), TextureUsage::SCENE_NORMALS });
                EnqueueCommand(bufferInOut, descriptorSetCmd);

                EnqueueCommand(bufferInOut, _triangleDrawCmd);

                EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
            }
            { // Generate Half Res AO
                GFX::BeginRenderPassCommand beginRenderPassCmd = {};
                beginRenderPassCmd._target = _ssaoHalfResOutput._targetID;
                beginRenderPassCmd._name = "DO_SSAO_HALF_RES_CALC";
                EnqueueCommand(bufferInOut, beginRenderPassCmd);

                pipelineDescriptor._shaderProgramHandle = _ssaoGenerateHalfResShader->getGUID();
                EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _context.newPipeline(pipelineDescriptor) });

                EnqueueCommand(bufferInOut, GFX::SendPushConstantsCommand{ _ssaoGenerateConstants });

                const auto& depthAtt = _halfDepthAndNormals._rt->getAttachment(RTAttachmentType::Colour, 0);

                GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
                descriptorSetCmd._set._textureData.add({ _noiseTexture->data(), _noiseSampler, TextureUsage::UNIT0 });
                descriptorSetCmd._set._textureData.add({ depthAtt.texture()->data(), depthAtt.samplerHash(), TextureUsage::DEPTH });
                EnqueueCommand(bufferInOut, descriptorSetCmd);

                EnqueueCommand(bufferInOut, _triangleDrawCmd);

                EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
            }
            { // UpSample AO
                GFX::BeginRenderPassCommand beginRenderPassCmd = {};
                beginRenderPassCmd._target = _ssaoOutput._targetID;
                beginRenderPassCmd._name = "DO_SSAO_UPSAMPLE_AO";
                EnqueueCommand(bufferInOut, beginRenderPassCmd);

                pipelineDescriptor._shaderProgramHandle = _ssaoUpSampleShader->getGUID();
                EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _context.newPipeline(pipelineDescriptor) });

                SamplerDescriptor linearSampler = {};
                linearSampler.wrapUVW(TextureWrap::CLAMP_TO_EDGE);
                linearSampler.minFilter(TextureFilter::LINEAR);
                linearSampler.magFilter(TextureFilter::LINEAR);
                linearSampler.anisotropyLevel(0);

                GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
                descriptorSetCmd._set._textureData.add({ _noiseTexture->data(), _noiseSampler, TextureUsage::UNIT0 });

                const auto& halfResAOAtt = _ssaoHalfResOutput._rt->getAttachment(RTAttachmentType::Colour, 0);
                const auto& halfDepthAtt = _halfDepthAndNormals._rt->getAttachment(RTAttachmentType::Colour, 0);
                const auto& depthAtt = _parent.screenRT()._rt->getAttachment(RTAttachmentType::Depth, 0);

                descriptorSetCmd._set._textureData.add({ halfResAOAtt.texture()->data(), linearSampler.getHash(), TextureUsage::UNIT0 });
                descriptorSetCmd._set._textureData.add({ halfResAOAtt.texture()->data(), halfResAOAtt.samplerHash(), TextureUsage::UNIT1 });
                descriptorSetCmd._set._textureData.add({ halfDepthAtt.texture()->data(), halfDepthAtt.samplerHash(), TextureUsage::NORMALMAP });
                descriptorSetCmd._set._textureData.add({ depthAtt.texture()->data(), depthAtt.samplerHash(), TextureUsage::DEPTH });

                EnqueueCommand(bufferInOut, descriptorSetCmd);

                EnqueueCommand(bufferInOut, _triangleDrawCmd);

                EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
            }
        } else {
            { // Generate Full Res AO
                GFX::BeginRenderPassCommand beginRenderPassCmd = {};
                beginRenderPassCmd._target = _ssaoOutput._targetID;
                beginRenderPassCmd._name = "DO_SSAO_CALC";
                EnqueueCommand(bufferInOut, beginRenderPassCmd);

                pipelineDescriptor._shaderProgramHandle = _ssaoGenerateShader->getGUID();
                EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _context.newPipeline(pipelineDescriptor) });

                const auto& depthAtt = _parent.screenRT()._rt->getAttachment(RTAttachmentType::Depth, 0);
                const auto& screenAtt = _parent.screenRT()._rt->getAttachment(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::NORMALS_AND_VELOCITY));

                GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
                descriptorSetCmd._set._textureData.add({ _noiseTexture->data(), _noiseSampler, TextureUsage::UNIT0 });
                descriptorSetCmd._set._textureData.add({ depthAtt.texture()->data(), depthAtt.samplerHash(), TextureUsage::DEPTH });
                descriptorSetCmd._set._textureData.add({ screenAtt.texture()->data(), screenAtt.samplerHash(), TextureUsage::SCENE_NORMALS });
                EnqueueCommand(bufferInOut, descriptorSetCmd);

                EnqueueCommand(bufferInOut, GFX::SendPushConstantsCommand{ _ssaoGenerateConstants });

                EnqueueCommand(bufferInOut, _triangleDrawCmd);

                EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
            }
        }
        {
            const auto& ssaoAtt = _ssaoOutput._rt->getAttachment(RTAttachmentType::Colour, 0);
            const auto& depthAtt = _parent.screenRT()._rt->getAttachment(RTAttachmentType::Depth, 0);
            const vec2<F32> ssaoTexelSize{ 1.f / ssaoAtt.texture()->width(), 1.f / ssaoAtt.texture()->height() };

            _ssaoBlurConstants.set(_ID("zPlanes"), GFX::PushConstantType::VEC2, camera->getZPlanes());
            _ssaoBlurConstants.set(_ID("texelSize"), GFX::PushConstantType::VEC2, ssaoTexelSize);

            // Blur AO
            GFX::BeginRenderPassCommand beginRenderPassCmd = {};
            beginRenderPassCmd._descriptor.drawMask().disableAll();
            beginRenderPassCmd._descriptor.drawMask().setEnabled(RTAttachmentType::Colour, to_U8(GFXDevice::ScreenTargets::EXTRA), true);
            beginRenderPassCmd._target = _parent.screenRT()._targetID;
            beginRenderPassCmd._name = "DO_SSAO_BLUR";
            EnqueueCommand(bufferInOut, beginRenderPassCmd);

            pipelineDescriptor._shaderProgramHandle = blurResults() ? _ssaoBlurShader->getGUID()
                                                                    : _ssaoPassThroughShader->getGUID();
                 
            pipelineDescriptor._stateHash = redChannelOnly.getHash();
            EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _context.newPipeline(pipelineDescriptor) });

            GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
            descriptorSetCmd._set._textureData.add({ ssaoAtt.texture()->data(), ssaoAtt.samplerHash(), TextureUsage::UNIT0 });
            descriptorSetCmd._set._textureData.add({ depthAtt.texture()->data(), depthAtt.samplerHash(), TextureUsage::DEPTH });
            EnqueueCommand(bufferInOut, descriptorSetCmd);

            EnqueueCommand(bufferInOut, GFX::SendPushConstantsCommand{ _ssaoBlurConstants });

            EnqueueCommand(bufferInOut, _triangleDrawCmd);

            EnqueueCommand(bufferInOut, GFX::EndRenderPassCommand{});
        }
    }
}

bool SSAOPreRenderOperator::execute(const Camera* camera, const RenderTargetHandle& input, const RenderTargetHandle& output, GFX::CommandBuffer& bufferInOut) {
    ACKNOWLEDGE_UNUSED(camera);
    ACKNOWLEDGE_UNUSED(input);
    ACKNOWLEDGE_UNUSED(output);
    ACKNOWLEDGE_UNUSED(bufferInOut);

    return false;
}

void SSAOPreRenderOperator::onToggle(const bool state) {
    PreRenderOperator::onToggle(state);
    _enabled = state;
}
}