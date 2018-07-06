#include "Headers/SSAOPreRenderOperator.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Managers/Headers/SceneManager.h"
#include "Core/Headers/ParamHandler.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Geometry/Shapes/Headers/Predefined/Quad3D.h"

namespace Divide {

//ref: http://john-chapman-graphics.blogspot.co.uk/2013/01/ssao-tutorial.html
SSAOPreRenderOperator::SSAOPreRenderOperator(RenderTarget* hdrTarget, RenderTarget* ldrTarget)
    : PreRenderOperator(FilterType::FILTER_SS_AMBIENT_OCCLUSION, hdrTarget, ldrTarget)
{

    _samplerCopy = GFX_DEVICE.allocateRT(false);
    _samplerCopy._rt->addAttachment(_hdrTarget->getDescriptor(RTAttachment::Type::Colour, 0), RTAttachment::Type::Colour, 0);

    U16 ssaoNoiseSize = 4;
    U16 noiseDataSize = ssaoNoiseSize * ssaoNoiseSize;
    vectorImpl<vec3<F32>> noiseData(noiseDataSize);

    for (vec3<F32>& noise : noiseData) {
        noise.set(Random(-1.0f, 1.0f),
                  Random(-1.0f, 1.0f),
                  0.0f);
        noise.normalize();
    }

    U16 kernelSize = 32;
    vectorImpl<vec3<F32>> kernel(kernelSize);
    for (U16 i = 0; i < kernelSize; ++i) {
        vec3<F32>& k = kernel[i];
        k.set(Random(-1.0f, 1.0f),
              Random(-1.0f, 1.0f),
              Random( 0.0f, 1.0f));
        k.normalize();
        F32 scale = to_float(i) / to_float(kernelSize);
        k *= Lerp(0.1f, 1.0f, scale * scale);
    }
    
    SamplerDescriptor noiseSampler;
    noiseSampler.toggleMipMaps(false);
    noiseSampler.setAnisotropy(0);
    noiseSampler.setFilters(TextureFilter::NEAREST);
    noiseSampler.setWrapMode(TextureWrap::REPEAT);
    stringImpl attachmentName("SSAOPreRenderOperator_NoiseTexture");

    ResourceDescriptor textureAttachment(attachmentName);
    textureAttachment.setThreadedLoading(false);
    textureAttachment.setPropertyDescriptor(noiseSampler);
    textureAttachment.setEnumValue(to_const_uint(TextureType::TEXTURE_2D));
    _noiseTexture = CreateResource<Texture>(textureAttachment);

    TextureDescriptor noiseDescriptor;
    noiseDescriptor._internalFormat = GFXImageFormat::RGB16F;
    noiseDescriptor._samplerDescriptor = noiseSampler;
    noiseDescriptor._type = TextureType::TEXTURE_2D;
    _noiseTexture->loadData(Texture::TextureLoadInfo(),
                            noiseDescriptor,
                            noiseData.data(),
                            vec2<U16>(ssaoNoiseSize),
                            vec2<U16>(0, 1));

    SamplerDescriptor screenSampler;
    screenSampler.setWrapMode(TextureWrap::CLAMP_TO_EDGE);
    screenSampler.setFilters(TextureFilter::LINEAR);
    screenSampler.toggleMipMaps(false);
    screenSampler.setAnisotropy(0);

    _ssaoOutput = GFX_DEVICE.allocateRT(false);
    TextureDescriptor outputDescriptor(TextureType::TEXTURE_2D,
                                       GFXImageFormat::RED16,
                                       GFXDataFormat::FLOAT_16);
    outputDescriptor.setSampler(screenSampler);
    
    //Colour0 holds the AO texture
    _ssaoOutput._rt->addAttachment(outputDescriptor, RTAttachment::Type::Colour, 0);

    _ssaoOutputBlurred = GFX_DEVICE.allocateRT(false);
    _ssaoOutputBlurred._rt->addAttachment(outputDescriptor, RTAttachment::Type::Colour, 0);

    ResourceDescriptor ssaoGenerate("SSAOPass.SSAOCalc");
    ssaoGenerate.setThreadedLoading(false);
    ssaoGenerate.setPropertyList(Util::StringFormat("USE_SCENE_ZPLANES,KERNEL_SIZE %d", kernelSize));
    _ssaoGenerateShader = CreateResource<ShaderProgram>(ssaoGenerate);

    ResourceDescriptor ssaoBlur("SSAOPass.SSAOBlur");
    ssaoBlur.setThreadedLoading(false);
    ssaoBlur.setPropertyList(Util::StringFormat("BLUR_SIZE %d", ssaoNoiseSize));
    _ssaoBlurShader = CreateResource<ShaderProgram>(ssaoBlur);
    
    ResourceDescriptor ssaoApply("SSAOPass.SSAOApply");
    ssaoApply.setThreadedLoading(false);
    _ssaoApplyShader = CreateResource<ShaderProgram>(ssaoApply);

    _ssaoGenerateShader->Uniform("sampleKernel", kernel);
}

SSAOPreRenderOperator::~SSAOPreRenderOperator() 
{
    GFX_DEVICE.deallocateRT(_ssaoOutput);
    GFX_DEVICE.deallocateRT(_ssaoOutputBlurred);
}

void SSAOPreRenderOperator::idle() {
}

void SSAOPreRenderOperator::reshape(U16 width, U16 height) {
    PreRenderOperator::reshape(width, height);

    _ssaoOutput._rt->create(width, height);
    _ssaoOutputBlurred._rt->create(width, height);

    _ssaoGenerateShader->Uniform("noiseScale", vec2<F32>(width, height) / to_float(_noiseTexture->getWidth()));
    _ssaoBlurShader->Uniform("ssaoTexelSize", vec2<F32>(1.0f / _ssaoOutput._rt->getWidth(),
                                                        1.0f / _ssaoOutput._rt->getHeight()));
}

void SSAOPreRenderOperator::execute() {
     GenericDrawCommand triangleCmd;
     triangleCmd.primitiveType(PrimitiveType::TRIANGLES);
     triangleCmd.drawCount(1);
     triangleCmd.stateHash(GFX_DEVICE.getDefaultStateBlock(true));

    _ssaoGenerateShader->Uniform("projectionMatrix", PreRenderOperator::s_mainCamProjectionMatrixCache);
    _ssaoGenerateShader->Uniform("invProjectionMatrix", PreRenderOperator::s_mainCamProjectionMatrixCache.getInverse());

    _noiseTexture->bind(to_const_ubyte(ShaderProgram::TextureUsage::UNIT0)); // noise texture
    _inputFB[0]->bind(to_const_ubyte(ShaderProgram::TextureUsage::DEPTH), RTAttachment::Type::Depth, 0);  // depth
    _inputFB[0]->bind(to_const_ubyte(ShaderProgram::TextureUsage::NORMALMAP), RTAttachment::Type::Colour, 1);  // normals
    
    _ssaoOutput._rt->begin(RenderTarget::defaultPolicy());
        triangleCmd.shaderProgram(_ssaoGenerateShader);
        GFX_DEVICE.draw(triangleCmd);
    _ssaoOutput._rt->end();


    _ssaoOutput._rt->bind(to_const_ubyte(ShaderProgram::TextureUsage::UNIT0), RTAttachment::Type::Colour, 0);  // AO texture
    _ssaoOutputBlurred._rt->begin(RenderTarget::defaultPolicy());
        triangleCmd.shaderProgram(_ssaoBlurShader);
        GFX_DEVICE.draw(triangleCmd);
    _ssaoOutputBlurred._rt->end();
    
    _samplerCopy._rt->blitFrom(_hdrTarget);
    _samplerCopy._rt->bind(to_const_ubyte(ShaderProgram::TextureUsage::UNIT0), RTAttachment::Type::Colour, 0);  // screen
    _ssaoOutputBlurred._rt->bind(to_const_ubyte(ShaderProgram::TextureUsage::UNIT1), RTAttachment::Type::Colour, 0);  // AO texture

    _hdrTarget->begin(_screenOnlyDraw);
        triangleCmd.shaderProgram(_ssaoApplyShader);
        GFX_DEVICE.draw(triangleCmd);
    _hdrTarget->end();
    
}

void SSAOPreRenderOperator::debugPreview(U8 slot) const {
    _ssaoOutputBlurred._rt->bind(slot, RTAttachment::Type::Colour, 0);
}

};