#include "Core/Resources/Headers/ResourceCache.h"
#include "Core/Headers/ParamHandler.h"
#include "Environment/Water/Headers/Water.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Geometry/Material/Headers/Material.h"

namespace Divide {

template <>
bool ImplResourceLoader<WaterPlane>::load(WaterPlane* const res,
                                          const stringImpl& name) {
    ParamHandler& param = ParamHandler::getInstance();
    res->setState(ResourceState::RES_LOADING);

    SamplerDescriptor defaultSampler;
    defaultSampler.setWrapMode(TextureWrap::REPEAT);
    defaultSampler.toggleMipMaps(false);
    ResourceDescriptor waterMaterial("waterMaterial_" + name);
    ResourceDescriptor waterShader("water_" + name);
    ResourceDescriptor waterTexture("waterTexture_" + name);
    ResourceDescriptor waterTextureDUDV("waterTextureDUDV_" + name);
    waterTexture.setResourceLocation(
        param.getParam<stringImpl>("assetsLocation") +
        "/misc_images/terrain_water_NM.jpg");
    waterTexture.setPropertyDescriptor(defaultSampler);
    waterTextureDUDV.setResourceLocation(
        param.getParam<stringImpl>("assetsLocation") +
        "/misc_images/water_dudv.jpg");
    waterTextureDUDV.setPropertyDescriptor(defaultSampler);

    Texture* waterNM = CreateResource<Texture>(waterTexture);
    assert(waterNM != nullptr);

    ShaderProgram* waterShaderProgram =
        CreateResource<ShaderProgram>(waterShader);
    waterShaderProgram->Uniform("texWaterNoiseNM", ShaderProgram::TextureUsage::UNIT0);
    waterShaderProgram->Uniform("texWaterReflection", ShaderProgram::TextureUsage::UNIT1);
    waterShaderProgram->Uniform("texWaterRefraction", 2);
    waterShaderProgram->Uniform("texWaterNoiseDUDV", 3);
    assert(waterShaderProgram != nullptr);

    Material* waterMat = CreateResource<Material>(waterMaterial);
    assert(waterMat != nullptr);
    // The material is responsible for the destruction of the textures and
    // shaders it receives!!!!
    res->setMaterialTpl(waterMat);

    waterMat->dumpToFile(false);
    waterMat->setShadingMode(Material::ShadingMode::BLINN_PHONG);
    waterMat->setTexture(ShaderProgram::TextureUsage::UNIT0, waterNM);
    waterMat->setShaderProgram(waterShaderProgram->getName(), RenderStage::DISPLAY,
                               true);
    waterMat->setShaderProgram("depthPass.PrePass", RenderStage::Z_PRE_PASS, true);

    U32 hash = waterMat->getRenderStateBlock(RenderStage::DISPLAY);
    RenderStateBlock waterMatDesc(GFX_DEVICE.getRenderStateBlock(hash));
    waterMatDesc.setCullMode(CullMode::NONE);
    waterMat->setRenderStateBlock(waterMatDesc.getHash(), RenderStage::DISPLAY);

    return true;
}

template<>
WaterPlane* ImplResourceLoader<WaterPlane>::operator()() {
    WaterPlane* ptr = MemoryManager_NEW WaterPlane();

    if (!load(ptr, _descriptor.getName())) {
        MemoryManager::DELETE(ptr);
    }

    return ptr;
}

};
