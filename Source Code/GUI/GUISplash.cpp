#include "stdafx.h"

#include "Headers/GUISplash.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/StringHelper.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Geometry/Shapes/Predefined/Headers/Quad3D.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Textures/Headers/Texture.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"

namespace Divide {

GUISplash::GUISplash(ResourceCache& cache,
                     const Str64& splashImageName,
                     const vec2<U16>& dimensions) 
    : _dimensions(dimensions)
{
    SamplerDescriptor splashSampler = {};
    splashSampler.wrapU(TextureWrap::CLAMP);
    splashSampler.wrapV(TextureWrap::CLAMP);
    splashSampler.wrapW(TextureWrap::CLAMP);
    splashSampler.minFilter(TextureFilter::NEAREST);
    splashSampler.magFilter(TextureFilter::NEAREST);
    splashSampler.anisotropyLevel(0);
    
    TextureDescriptor splashDescriptor(TextureType::TEXTURE_2D);
    splashDescriptor.samplerDescriptor(splashSampler);

    ResourceDescriptor splashImage("SplashScreen Texture");
    splashImage.threaded(false);
    splashImage.assetName(splashImageName);
    splashImage.assetLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
    splashImage.propertyDescriptor(splashDescriptor);

    _splashImage = CreateResource<Texture>(cache, splashImage);

    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "baseVertexShaders.glsl";
    vertModule._variant = "FullScreenQuad";

    ShaderModuleDescriptor fragModule = {};
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "fbPreview.glsl";

    ShaderProgramDescriptor shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.push_back(fragModule);

    ResourceDescriptor splashShader("fbPreview");
    splashShader.propertyDescriptor(shaderDescriptor);
    splashShader.threaded(false);
    _splashShader = CreateResource<ShaderProgram>(cache, splashShader);
}

GUISplash::~GUISplash()
{
}

void GUISplash::render(GFXDevice& context, const U64 deltaTimeUS) {
    PipelineDescriptor pipelineDescriptor;
    pipelineDescriptor._stateHash = context.get2DStateBlock();
    pipelineDescriptor._shaderProgramHandle = _splashShader->getGUID();

    GFX::ScopedCommandBuffer sBuffer = GFX::allocateScopedCommandBuffer();
    GFX::CommandBuffer& buffer = sBuffer();

    GFX::BindPipelineCommand pipelineCmd;
    pipelineCmd._pipeline = context.newPipeline(pipelineDescriptor);
    GFX::EnqueueCommand(buffer, pipelineCmd);


    GFX::SetViewportCommand viewportCommand;
    viewportCommand._viewport.set(0, 0, _dimensions.width, _dimensions.height);
    GFX::EnqueueCommand(buffer, viewportCommand);

    GFX::BindDescriptorSetsCommand descriptorSetCmd;
    descriptorSetCmd._set._textureData.setTexture(_splashImage->data(), to_U8(ShaderProgram::TextureUsage::UNIT0));
    GFX::EnqueueCommand(buffer, descriptorSetCmd);

    GenericDrawCommand triangleCmd;
    triangleCmd._primitiveType = PrimitiveType::TRIANGLES;

    GFX::DrawCommand drawCmd = { triangleCmd };
    GFX::EnqueueCommand(buffer, drawCmd);

    context.flushCommandBuffer(buffer);
}

};