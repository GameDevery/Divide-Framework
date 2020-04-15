#include "stdafx.h"

#include "Headers/Terrain.h"
#include "Headers/TerrainLoader.h"
#include "Headers/TerrainDescriptor.h"
#include "Headers/TerrainTessellator.h"

#include "Core/Headers/PlatformContext.h"
#include "Core/Headers/Configuration.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/RenderStateBlock.h"

#include "Geometry/Material/Headers/Material.h"
#include "Managers/Headers/SceneManager.h"
#include "Geometry/Shapes/Predefined/Headers/Quad3D.h"

namespace Divide {

namespace {
    constexpr bool g_disableLoadFromCache = false;
    const char* g_materialFileExtension = ".mat.xml";
    Str256 climatesLocation(U8 textureQuality) {
       CLAMP<U8>(textureQuality, 0u, 3u);

       return Paths::g_assetsLocation + Paths::g_heightmapLocation +
             (textureQuality == 3u ? Paths::g_climatesHighResLocation
                                   : textureQuality == 2u ? Paths::g_climatesMedResLocation
                                                          : Paths::g_climatesLowResLocation);
    }

    std::pair<U8, bool> findOrInsert(U8 textureQuality, vectorSTD<stringImpl>& container, const stringImpl& texture, stringImpl materialName) {
        if (!fileExists((climatesLocation(textureQuality) + "/" + materialName + "/" + texture).c_str())) {
            materialName = "std_default";
        }
        const stringImpl item = materialName + "/" + texture;
        auto it = std::find(std::cbegin(container),
                            std::cend(container),
                            item);
        if (it != std::cend(container)) {
            return std::make_pair(to_U8(std::distance(std::cbegin(container), it)), false);
        }

        container.push_back(item);
        return std::make_pair(to_U8(container.size() - 1), true);
    }
};

bool TerrainLoader::loadTerrain(Terrain_ptr terrain,
                                const std::shared_ptr<TerrainDescriptor>& terrainDescriptor,
                                PlatformContext& context,
                                bool threadedLoading) {

    auto waitForReasoureTask = [&context](CachedResource_wptr res) {
        context.taskPool(TaskPoolType::HIGH_PRIORITY).threadWaiting();
    };

    const stringImpl& name = terrainDescriptor->getVariable("terrainName");
    
    stringImpl terrainLocation = Paths::g_assetsLocation + Paths::g_heightmapLocation + terrainDescriptor->getVariable("descriptor");

    Attorney::TerrainLoader::descriptor(*terrain, terrainDescriptor);

    const U8 textureQuality = to_U8(context.config().rendering.terrainTextureQuality);
    // Blend maps
    ResourceDescriptor textureBlendMap("Terrain Blend Map_" + name);
    textureBlendMap.assetLocation(terrainLocation);
    textureBlendMap.flag(true);
    textureBlendMap.waitForReadyCbk(waitForReasoureTask);

    // Albedo maps and roughness
    ResourceDescriptor textureAlbedoMaps("Terrain Albedo Maps_" + name);
    textureAlbedoMaps.assetLocation(climatesLocation(textureQuality));
    textureAlbedoMaps.flag(true);
    textureAlbedoMaps.waitForReadyCbk(waitForReasoureTask);

    // Normals
    ResourceDescriptor textureNormalMaps("Terrain Normal Maps_" + name);
    textureNormalMaps.assetLocation(climatesLocation(textureQuality));
    textureNormalMaps.flag(true);
    textureNormalMaps.waitForReadyCbk(waitForReasoureTask);

    // AO and displacement
    ResourceDescriptor textureExtraMaps("Terrain Extra Maps_" + name);
    textureExtraMaps.assetLocation(climatesLocation(textureQuality));
    textureExtraMaps.flag(true);
    textureExtraMaps.waitForReadyCbk(waitForReasoureTask);

    //temp data
    stringImpl layerOffsetStr;
    stringImpl currentMaterial;

    U8 layerCount = terrainDescriptor->textureLayers();

    const vectorSTD<std::pair<stringImpl, TerrainTextureChannel>> channels = {
        {"red", TerrainTextureChannel::TEXTURE_RED_CHANNEL},
        {"green", TerrainTextureChannel::TEXTURE_GREEN_CHANNEL},
        {"blue", TerrainTextureChannel::TEXTURE_BLUE_CHANNEL},
        {"alpha", TerrainTextureChannel::TEXTURE_ALPHA_CHANNEL}
    };

    const F32 albedoTilingFactor = terrainDescriptor->getVariablef("albedoTilingFactor");

    vectorSTD<stringImpl> textures[to_base(TerrainTextureType::COUNT)] = {};
    vectorSTD<stringImpl> splatTextures = {};

    size_t idxCount = layerCount * to_size(TerrainTextureChannel::COUNT);
    std::array<vectorSTD<U16>, to_base(TerrainTextureType::COUNT)> indices;
    std::array<U16, to_base(TerrainTextureType::COUNT)> offsets;
    vectorSTD<U8> channelCountPerLayer(layerCount, 0u);

    for (auto& it : indices) {
        it.resize(idxCount, 255u);
    }
    offsets.fill(0u);

    const char* textureNames[to_base(TerrainTextureType::COUNT)] = {
        "Albedo_roughness", "Normal", "Displacement"
    };

    U16 idx = 0;
    for (U8 i = 0; i < layerCount; ++i) {
        layerOffsetStr = to_stringImpl(i);
        splatTextures.push_back(terrainDescriptor->getVariable("blendMap" + layerOffsetStr));
        U8 j = 0;
        for (auto it : channels) {
            currentMaterial = terrainDescriptor->getVariable(it.first + layerOffsetStr + "_mat");
            if (currentMaterial.empty()) {
                continue;
            }

            for (U8 k = 0; k < to_base(TerrainTextureType::COUNT); ++k) {
                auto ret = findOrInsert(textureQuality, textures[k], Util::StringFormat("%s.%s", textureNames[k], k == to_base(TerrainTextureType::ALBEDO_ROUGHNESS) ? "png" : "jpg"), currentMaterial.c_str());
                indices[k][idx] = ret.first;
                if (ret.second) {
                    ++offsets[k];
                }
            }

            ++j;
            ++idx;
        }
         channelCountPerLayer[i] = j;
    }
    
    for (U8 k = 0; k < to_base(TerrainTextureType::COUNT); ++k) {
        indices[k].resize(idx);
    }

    stringImpl blendMapArray = "";
    stringImpl albedoMapArray = "";
    stringImpl normalMapArray = "";
    stringImpl extraMapArray = "";

    U16 extraMapCount = 0;
    for (const stringImpl& tex : textures[to_base(TerrainTextureType::ALBEDO_ROUGHNESS)]) {
        albedoMapArray += tex + ",";
    }
    for (const stringImpl& tex : textures[to_base(TerrainTextureType::NORMAL)]) {
        normalMapArray += tex + ",";
    }

    for (U8 i = to_U8(TerrainTextureType::DISPLACEMENT_AO); i < to_U8(TerrainTextureType::COUNT); ++i) {
        for (const stringImpl& tex : textures[i]) {
            extraMapArray += tex + ",";
            ++extraMapCount;
        }
    }

    for (const stringImpl& tex : splatTextures) {
        blendMapArray += tex + ",";
    }
    
    auto removeLastChar = [](stringImpl& strIn) {
        strIn = strIn.substr(0, strIn.length() - 1);
    };

    removeLastChar(blendMapArray);
    removeLastChar(albedoMapArray);
    removeLastChar(normalMapArray);
    removeLastChar(extraMapArray);

    SamplerDescriptor heightMapSampler = {};
    heightMapSampler.wrapU(TextureWrap::CLAMP_TO_EDGE);
    heightMapSampler.wrapV(TextureWrap::CLAMP_TO_EDGE);
    heightMapSampler.wrapW(TextureWrap::CLAMP_TO_EDGE);
    heightMapSampler.minFilter(TextureFilter::LINEAR);
    heightMapSampler.magFilter(TextureFilter::LINEAR);
    heightMapSampler.anisotropyLevel(0);

    SamplerDescriptor blendMapSampler = {};
    blendMapSampler.wrapU(TextureWrap::CLAMP_TO_EDGE);
    blendMapSampler.wrapV(TextureWrap::CLAMP_TO_EDGE);
    blendMapSampler.wrapW(TextureWrap::CLAMP_TO_EDGE);
    blendMapSampler.minFilter(TextureFilter::LINEAR_MIPMAP_LINEAR);
    blendMapSampler.magFilter(TextureFilter::LINEAR);
    blendMapSampler.anisotropyLevel(0);

    SamplerDescriptor albedoSampler = {};
    if (false) {
        albedoSampler.wrapU(TextureWrap::MIRROR_REPEAT);
        albedoSampler.wrapV(TextureWrap::MIRROR_REPEAT);
        albedoSampler.wrapW(TextureWrap::MIRROR_REPEAT);
    }
    albedoSampler.minFilter(TextureFilter::LINEAR_MIPMAP_LINEAR);
    albedoSampler.magFilter(TextureFilter::LINEAR);
    albedoSampler.anisotropyLevel(4);

    TextureDescriptor blendMapDescriptor(TextureType::TEXTURE_2D_ARRAY);
    blendMapDescriptor.samplerDescriptor(blendMapSampler);
    blendMapDescriptor.layerCount(to_U16(splatTextures.size()));
    blendMapDescriptor.srgb(false);

    TextureDescriptor albedoDescriptor(TextureType::TEXTURE_2D_ARRAY);
    albedoDescriptor.samplerDescriptor(albedoSampler);
    albedoDescriptor.layerCount(to_U16(textures[to_base(TerrainTextureType::ALBEDO_ROUGHNESS)].size()));
    albedoDescriptor.srgb(false);

    TextureDescriptor normalDescriptor(TextureType::TEXTURE_2D_ARRAY);
    normalDescriptor.samplerDescriptor(albedoSampler);
    normalDescriptor.layerCount(to_U16(textures[to_base(TerrainTextureType::NORMAL)].size()));
    normalDescriptor.srgb(false);

    TextureDescriptor extraDescriptor(TextureType::TEXTURE_2D_ARRAY);
    extraDescriptor.samplerDescriptor(albedoSampler);
    extraDescriptor.layerCount(extraMapCount);
    extraDescriptor.srgb(false);

    textureBlendMap.assetName(blendMapArray);
    textureBlendMap.propertyDescriptor(blendMapDescriptor);

    textureAlbedoMaps.assetName(albedoMapArray);
    textureAlbedoMaps.propertyDescriptor(albedoDescriptor);

    textureNormalMaps.assetName(normalMapArray);
    textureNormalMaps.propertyDescriptor(normalDescriptor);

    textureExtraMaps.assetName(extraMapArray);
    textureExtraMaps.propertyDescriptor(extraDescriptor);

    ResourceDescriptor terrainMaterialDescriptor("terrainMaterial_" + name);
    Material_ptr terrainMaterial = CreateResource<Material>(terrain->parentResourceCache(), terrainMaterialDescriptor);
    terrainMaterial->ignoreXMLData(true);

    const vec2<U16> & terrainDimensions = terrainDescriptor->dimensions();
    const vec2<F32> & altitudeRange = terrainDescriptor->altitudeRange();

    Console::d_printfn(Locale::get(_ID("TERRAIN_INFO")), terrainDimensions.width, terrainDimensions.height);

    const F32 underwaterTileScale = terrainDescriptor->getVariablef("underwaterTileScale");
    terrainMaterial->setShadingMode(ShadingMode::COOK_TORRANCE);

    if (terrainDescriptor->parallaxMode() == TerrainDescriptor::ParallaxMode::NORMAL) {
        terrainMaterial->setBumpMethod(BumpMethod::PARALLAX);
    } else if (terrainDescriptor->parallaxMode() == TerrainDescriptor::ParallaxMode::OCCLUSION) {
        terrainMaterial->setBumpMethod(BumpMethod::PARALLAX_OCCLUSION);
    } else {
        terrainMaterial->setBumpMethod(BumpMethod::NONE);
    }

    terrainMaterial->getColourData().baseColour(FColour4(DefaultColours::WHITE.rgb() * 0.5f, 1.0f));
    terrainMaterial->getColourData().metallic(0.2f);
    terrainMaterial->getColourData().roughness(0.8f);
    terrainMaterial->setParallaxFactor(terrainDescriptor->parallaxHeightScale());
    terrainMaterial->disableTranslucency();

    U8 totalLayerCount = 0;
    stringImpl layerCountData = Util::StringFormat("const uint CURRENT_LAYER_COUNT[ %d ] = {", layerCount);
    stringImpl blendAmntStr = "INSERT_BLEND_AMNT_ARRAY float blendAmount[TOTAL_LAYER_COUNT] = {";
    for (U8 i = 0; i < layerCount; ++i) {
        layerCountData.append(Util::StringFormat("%d,", channelCountPerLayer[i]));
        for (U8 j = 0; j < channelCountPerLayer[i]; ++j) {
            blendAmntStr.append("0.0f,");
        }
        totalLayerCount += channelCountPerLayer[i];
    }
    removeLastChar(layerCountData);
    layerCountData.append("};");

    removeLastChar(blendAmntStr);
    blendAmntStr.append("};");

    const char* idxNames[to_base(TerrainTextureType::COUNT)] = {
        "ALBEDO_IDX", "NORMAL_IDX", "DISPLACEMENT_IDX"
    };

    std::array<stringImpl, to_base(TerrainTextureType::COUNT)> indexData = {};
    for (U8 i = 0; i < to_base(TerrainTextureType::COUNT); ++i) {
        stringImpl& dataStr = indexData[i];
        dataStr = Util::StringFormat("const uint %s[ %d ] = {", idxNames[i], indices[i].size());
        for (size_t j = 0; j < indices[i].size(); ++j) {
            dataStr.append(Util::StringFormat("%d,", indices[i][j]));
        }
        removeLastChar(dataStr);
        dataStr.append("};");
    }

    stringImpl helperTextures = terrainDescriptor->getVariable("waterCaustics") + "," +
                                terrainDescriptor->getVariable("underwaterAlbedoTexture") + "," + 
                                terrainDescriptor->getVariable("underwaterDetailTexture") + "," +
                                terrainDescriptor->getVariable("tileNoiseTexture");

    TextureDescriptor helperTexDescriptor(TextureType::TEXTURE_2D_ARRAY);
    helperTexDescriptor.samplerDescriptor(blendMapSampler);

    ResourceDescriptor textureWaterCaustics("Terrain Helper Textures_" + name);
    textureWaterCaustics.assetLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
    textureWaterCaustics.assetName(helperTextures);
    textureWaterCaustics.propertyDescriptor(helperTexDescriptor);
    textureWaterCaustics.waitForReadyCbk(waitForReasoureTask);

    ResourceDescriptor heightMapTexture("Terrain Heightmap_" + name);
    heightMapTexture.assetLocation(terrainLocation);
    heightMapTexture.assetName(terrainDescriptor->getVariable("heightfieldTex"));
    heightMapTexture.waitForReadyCbk(waitForReasoureTask);

    TextureDescriptor heightMapDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::RGB, GFXDataFormat::UNSIGNED_SHORT);
    heightMapDescriptor.samplerDescriptor(heightMapSampler);
    heightMapDescriptor.autoMipMaps(false);
    heightMapTexture.propertyDescriptor(heightMapDescriptor);

    heightMapTexture.flag(true);

    terrainMaterial->setTexture(TextureUsage::TERRAIN_SPLAT, CreateResource<Texture>(terrain->parentResourceCache(), textureBlendMap));
    terrainMaterial->setTexture(TextureUsage::TERRAIN_ALBEDO_TILE, CreateResource<Texture>(terrain->parentResourceCache(), textureAlbedoMaps));
    terrainMaterial->setTexture(TextureUsage::TERRAIN_NORMAL_TILE, CreateResource<Texture>(terrain->parentResourceCache(), textureNormalMaps));
    terrainMaterial->setTexture(TextureUsage::TERRAIN_EXTRA_TILE, CreateResource<Texture>(terrain->parentResourceCache(), textureExtraMaps));
    terrainMaterial->setTexture(TextureUsage::TERRAIN_HELPER_TEXTURES, CreateResource<Texture>(terrain->parentResourceCache(), textureWaterCaustics));
    terrainMaterial->setTexture(TextureUsage::HEIGHTMAP, CreateResource<Texture>(terrain->parentResourceCache(), heightMapTexture));

    terrainMaterial->setTextureUseForDepth(TextureUsage::TERRAIN_SPLAT, true);
    terrainMaterial->setTextureUseForDepth(TextureUsage::TERRAIN_NORMAL_TILE, true);
    terrainMaterial->setTextureUseForDepth(TextureUsage::TERRAIN_HELPER_TEXTURES, true);
    terrainMaterial->setTextureUseForDepth(TextureUsage::HEIGHTMAP, true);

    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "terrainTess.glsl";

    ShaderModuleDescriptor tescModule = {};
    tescModule._moduleType = ShaderType::TESSELLATION_CTRL;
    tescModule._sourceFile = "terrainTess.glsl";

    ShaderModuleDescriptor teseModule = {};
    teseModule._moduleType = ShaderType::TESSELLATION_EVAL;
    teseModule._sourceFile = "terrainTess.glsl";

    ShaderModuleDescriptor geomModule = {};
    geomModule._moduleType = ShaderType::GEOMETRY;
    geomModule._sourceFile = "terrainTess.glsl";

    ShaderModuleDescriptor fragModule = {};
    fragModule._batchSameFile = false;
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "terrainTess.glsl";

    ShaderProgramDescriptor shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.push_back(tescModule);
    shaderDescriptor._modules.push_back(teseModule);
    if (terrainDescriptor->wireframeDebug() != TerrainDescriptor::WireframeMode::NONE) {
        shaderDescriptor._modules.push_back(geomModule);
    }
    shaderDescriptor._modules.push_back(fragModule);

    Texture_ptr albedoTile = terrainMaterial->getTexture(TextureUsage::TERRAIN_ALBEDO_TILE).lock();
    WAIT_FOR_CONDITION(albedoTile->getState() == ResourceState::RES_LOADED);

    const U16 tileMapSize = albedoTile->width();
    for (ShaderModuleDescriptor& shaderModule : shaderDescriptor._modules) {
        if (terrainDescriptor->wireframeDebug() == TerrainDescriptor::WireframeMode::EDGES) {
            shaderModule._defines.emplace_back("TOGGLE_WIREFRAME", true);
        } else if (terrainDescriptor->wireframeDebug() == TerrainDescriptor::WireframeMode::NORMALS) {
            shaderModule._defines.emplace_back("TOGGLE_NORMALS", true);
        }

        if (GFXDevice::getGPUVendor() == GPUVendor::AMD) {
            if (shaderModule._moduleType == (terrainDescriptor->wireframeDebug() != TerrainDescriptor::WireframeMode::NONE ? ShaderType::GEOMETRY : ShaderType::TESSELLATION_EVAL)) {
                shaderModule._defines.emplace_back("USE_CUSTOM_CLIP_PLANES", true);
            }
        } else {
            shaderModule._defines.emplace_back("USE_CUSTOM_CLIP_PLANES", true);
        }

        shaderModule._defines.emplace_back(Util::StringFormat("PATCHES_PER_TILE_EDGE %d", Terrain::PATCHES_PER_TILE_EDGE), true);
        shaderModule._defines.emplace_back(Util::StringFormat("CONTROL_VTX_PER_TILE_EDGE %5.2ff", to_F32(Terrain::VTX_PER_TILE_EDGE)), true);

        if (context.config().rendering.terrainDetailLevel > 0) {
            shaderModule._defines.emplace_back("HAS_PARALLAX", true);
            if (context.config().rendering.terrainDetailLevel > 1) {
                shaderModule._defines.emplace_back("REDUCE_TEXTURE_TILE_ARTIFACT", true);
                if (context.config().rendering.terrainDetailLevel > 2) {
                    shaderModule._defines.emplace_back("HIGH_QUALITY_TILE_ARTIFACT_REDUCTION", true);
                    if (context.config().rendering.terrainDetailLevel > 2) {
                        shaderModule._defines.emplace_back("REDUCE_TEXTURE_TILE_ARTIFACT_ALL_LODS", true);
                    }
                }
            }
        }

        shaderModule._defines.emplace_back("COMPUTE_TBN", true);
        shaderModule._defines.emplace_back("OVERRIDE_DATA_IDX", true);
        shaderModule._defines.emplace_back("TEXTURE_TILE_SIZE " + to_stringImpl(tileMapSize), true);
        shaderModule._defines.emplace_back("ALBEDO_TILING " + to_stringImpl(albedoTilingFactor), true);
        shaderModule._defines.emplace_back("MAX_RENDER_NODES " + to_stringImpl(Terrain::MAX_RENDER_NODES), true);
        shaderModule._defines.emplace_back("TERRAIN_WIDTH " + to_stringImpl(terrainDimensions.width), true);
        shaderModule._defines.emplace_back("TERRAIN_LENGTH " + to_stringImpl(terrainDimensions.height), true);
        shaderModule._defines.emplace_back("TERRAIN_MIN_HEIGHT " + to_stringImpl(altitudeRange.x) + "f", true);
        shaderModule._defines.emplace_back("TERRAIN_HEIGHT_RANGE " + to_stringImpl(altitudeRange.y - altitudeRange.x) + "f", true);
        shaderModule._defines.emplace_back("NODE_STATIC", true);

        if (shaderModule._moduleType == ShaderType::FRAGMENT) {
            shaderModule._defines.emplace_back(blendAmntStr, true);

            if (!context.config().rendering.shadowMapping.enabled) {
                shaderModule._defines.emplace_back("DISABLE_SHADOW_MAPPING", true);
            }

            shaderModule._defines.emplace_back("SKIP_TEXTURES", true);
            shaderModule._defines.emplace_back("USE_SHADING_COOK_TORRANCE", true);
            shaderModule._defines.emplace_back(Util::StringFormat("UNDERWATER_TILE_SCALE %d", to_I32(underwaterTileScale)), true);
            shaderModule._defines.emplace_back(Util::StringFormat("TOTAL_LAYER_COUNT %d", totalLayerCount), true);
            shaderModule._defines.emplace_back(layerCountData, false);
            for (const stringImpl& str : indexData) {
                shaderModule._defines.emplace_back(str, false);
            }

            shaderModule._defines.emplace_back(Util::StringFormat("MAX_TEXTURE_LAYERS %d", layerCount), true);

            shaderModule._defines.emplace_back(Util::StringFormat("TEXTURE_SPLAT %d", to_base(TextureUsage::TERRAIN_SPLAT)), true);
            shaderModule._defines.emplace_back(Util::StringFormat("TEXTURE_ALBEDO_TILE %d", to_base(TextureUsage::TERRAIN_ALBEDO_TILE)), true);
            shaderModule._defines.emplace_back(Util::StringFormat("TEXTURE_NORMAL_TILE %d", to_base(TextureUsage::TERRAIN_NORMAL_TILE)), true);
            shaderModule._defines.emplace_back(Util::StringFormat("TEXTURE_EXTRA_TILE %d", to_base(TextureUsage::TERRAIN_EXTRA_TILE)), true);
            shaderModule._defines.emplace_back(Util::StringFormat("TEXTURE_HELPER_TEXTURES %d", to_base(TextureUsage::TERRAIN_HELPER_TEXTURES)), true);

        }
    }

    // SHADOW

    ShaderProgramDescriptor shadowDescriptorVSM = {}; ;
    for (const ShaderModuleDescriptor& shaderModule : shaderDescriptor._modules) {
        shadowDescriptorVSM._modules.push_back(shaderModule);
        ShaderModuleDescriptor& tempModule = shadowDescriptorVSM._modules.back();

        if (tempModule._moduleType == ShaderType::FRAGMENT) {
            tempModule._variant = "Shadow.VSM";
        }
        tempModule._defines.emplace_back("SHADOW_PASS", true);
        tempModule._defines.emplace_back("MAX_TESS_SCALE 16", true);
        tempModule._defines.emplace_back("MIN_TESS_SCALE 16", true);

    }

    ResourceDescriptor terrainShaderShadowVSM("Terrain_ShadowVSM-" + name);
    terrainShaderShadowVSM.propertyDescriptor(shadowDescriptorVSM);
    terrainShaderShadowVSM.waitForReadyCbk(waitForReasoureTask);

    ShaderProgram_ptr terrainShadowShaderVSM = CreateResource<ShaderProgram>(terrain->parentResourceCache(), terrainShaderShadowVSM);

    ShaderProgramDescriptor shadowDescriptor = {};
    for (ShaderModuleDescriptor& shaderModule : shadowDescriptorVSM._modules) {
        if (shaderModule._moduleType == ShaderType::FRAGMENT) {
            continue;
        } 
        shadowDescriptor._modules.push_back(shaderModule);
    }

    ResourceDescriptor terrainShaderShadow("Terrain_Shadow-" + name);
    terrainShaderShadow.propertyDescriptor(shadowDescriptor);
    terrainShaderShadow.waitForReadyCbk(waitForReasoureTask);

    ShaderProgram_ptr terrainShadowShader = CreateResource<ShaderProgram>(terrain->parentResourceCache(), terrainShaderShadow);



    // MAIN PASS
    ShaderProgramDescriptor colourDescriptor = shaderDescriptor;
    for (ShaderModuleDescriptor& shaderModule : colourDescriptor._modules) {
        if (shaderModule._moduleType == ShaderType::FRAGMENT) {
            shaderModule._variant = "MainPass";
        }
        shaderModule._defines.emplace_back("USE_SSAO", true);
        shaderModule._defines.emplace_back("MAX_TESS_SCALE 64", true);
        shaderModule._defines.emplace_back("MIN_TESS_SCALE 2", true);
        shaderModule._defines.emplace_back("USE_DEFERRED_NORMALS", true);
    }

    ResourceDescriptor terrainShaderColour("Terrain_Colour-" + name);
    terrainShaderColour.propertyDescriptor(colourDescriptor);
    terrainShaderColour.waitForReadyCbk(waitForReasoureTask);

    ShaderProgram_ptr terrainColourShader = CreateResource<ShaderProgram>(terrain->parentResourceCache(), terrainShaderColour);

    // PRE PASS
    ShaderProgramDescriptor prePassDescriptor = colourDescriptor;
    for (ShaderModuleDescriptor& shaderModule : prePassDescriptor._modules) {
        shaderModule._defines.emplace_back("PRE_PASS", true);
    }

    ResourceDescriptor terrainShaderPrePass("Terrain_PrePass-" + name);
    terrainShaderPrePass.propertyDescriptor(prePassDescriptor);
    terrainShaderPrePass.waitForReadyCbk(waitForReasoureTask);

    ShaderProgram_ptr terrainPrePassShader = CreateResource<ShaderProgram>(terrain->parentResourceCache(), terrainShaderPrePass);

    // PRE PASS LQ
    ShaderProgramDescriptor prePassDescriptorLQ = shaderDescriptor;
    for (ShaderModuleDescriptor& shaderModule : prePassDescriptorLQ._modules) {
        if (shaderModule._moduleType == ShaderType::FRAGMENT) {
            shaderModule._variant = "";
        }
        shaderModule._defines.emplace_back("PRE_PASS", true);
        shaderModule._defines.emplace_back("MAX_TESS_SCALE 32", true);
        shaderModule._defines.emplace_back("MIN_TESS_SCALE 4", true);
        shaderModule._defines.emplace_back("LOW_QUALITY", true);
    }

    ResourceDescriptor terrainShaderPrePassLQ("Terrain_PrePass_LowQuality-" + name);
    terrainShaderPrePassLQ.propertyDescriptor(prePassDescriptorLQ);
    terrainShaderPrePassLQ.waitForReadyCbk(waitForReasoureTask);

    ShaderProgram_ptr terrainPrePassShaderLQ = CreateResource<ShaderProgram>(terrain->parentResourceCache(), terrainShaderPrePassLQ);

    // MAIN PASS LQ
    ShaderProgramDescriptor lowQualityDescriptor = shaderDescriptor;
    for (ShaderModuleDescriptor& shaderModule : lowQualityDescriptor._modules) {
        if (shaderModule._moduleType == ShaderType::FRAGMENT) {
            shaderModule._variant = "LQPass";
        }

        shaderModule._defines.emplace_back("MAX_TESS_SCALE 32", true);
        shaderModule._defines.emplace_back("MIN_TESS_SCALE 4", true);
        shaderModule._defines.emplace_back("LOW_QUALITY", true);
    }

    ResourceDescriptor terrainShaderColourLQ("Terrain_Colour_LowQuality-" + name);
    terrainShaderColourLQ.propertyDescriptor(lowQualityDescriptor);
    terrainShaderColourLQ.waitForReadyCbk(waitForReasoureTask);

    ShaderProgram_ptr terrainColourShaderLQ = CreateResource<ShaderProgram>(terrain->parentResourceCache(), terrainShaderColourLQ);

    terrainMaterial->setShaderProgram(terrainPrePassShader, RenderStage::DISPLAY, RenderPassType::PRE_PASS, 0u);
    terrainMaterial->setShaderProgram(terrainColourShader, RenderStage::DISPLAY, RenderPassType::MAIN_PASS, 0u);

    terrainMaterial->setShaderProgram(terrainColourShaderLQ, RenderStage::REFLECTION, RenderPassType::MAIN_PASS, 0u);
    terrainMaterial->setShaderProgram(terrainPrePassShaderLQ, RenderStage::REFLECTION, RenderPassType::PRE_PASS, 0u);

    terrainMaterial->setShaderProgram(terrainColourShaderLQ, RenderStage::REFRACTION, RenderPassType::MAIN_PASS, 0u);
    terrainMaterial->setShaderProgram(terrainPrePassShaderLQ, RenderStage::REFRACTION, RenderPassType::PRE_PASS, 0u);
    
    terrainMaterial->setShaderProgram(terrainShadowShader,  RenderStage::SHADOW, RenderPassType::COUNT, to_U8(LightType::POINT));
    terrainMaterial->setShaderProgram(terrainShadowShader,  RenderStage::SHADOW, RenderPassType::COUNT, to_U8(LightType::SPOT));
    terrainMaterial->setShaderProgram(terrainShadowShaderVSM,  RenderStage::SHADOW, RenderPassType::COUNT, to_U8(LightType::DIRECTIONAL));

    // Generate a render state
    RenderStateBlock terrainRenderState;
    terrainRenderState.setCullMode(CullMode::BACK);
    terrainRenderState.setZFunc(ComparisonFunction::EQUAL);
    terrainRenderState.setTessControlPoints(4);

    // Generate a render state for drawing reflections
    RenderStateBlock terrainRenderStatePrePass = terrainRenderState;
    terrainRenderStatePrePass.setZFunc(ComparisonFunction::LEQUAL);

    RenderStateBlock terrainRenderStateReflection = terrainRenderState;
    terrainRenderStateReflection.setCullMode(CullMode::FRONT);

    RenderStateBlock terrainRenderStateReflectionPrePass = terrainRenderStateReflection;
    terrainRenderStateReflectionPrePass.setZFunc(ComparisonFunction::LEQUAL);

    // Generate a shadow render state
    RenderStateBlock terrainRenderStateShadow = terrainRenderState;
    terrainRenderStateShadow.setZBias(1.0f, 1.0f);
    terrainRenderStateShadow.setZFunc(ComparisonFunction::LEQUAL);
    terrainRenderStateShadow.setColourWrites(false, false, false, false);

    RenderStateBlock terrainRenderStateShadowVSM = terrainRenderStateShadow;
    if_constexpr(!Config::Lighting::USE_SEPARATE_VSM_PASS) {
        terrainRenderStateShadowVSM.setColourWrites(true, true, false, false);
    }

    terrainMaterial->setRenderStateBlock(terrainRenderState.getHash(), RenderStage::COUNT, RenderPassType::MAIN_PASS, 0u);
    terrainMaterial->setRenderStateBlock(terrainRenderStatePrePass.getHash(), RenderStage::COUNT, RenderPassType::PRE_PASS, 0u);

    terrainMaterial->setRenderStateBlock(terrainRenderStateReflection.getHash(), RenderStage::REFLECTION, RenderPassType::MAIN_PASS, 0u);
    terrainMaterial->setRenderStateBlock(terrainRenderStateReflectionPrePass.getHash(), RenderStage::REFLECTION, RenderPassType::PRE_PASS, 0u);
    
    terrainMaterial->setRenderStateBlock(terrainRenderStateShadowVSM.getHash(), RenderStage::SHADOW, RenderPassType::COUNT, to_U8(LightType::DIRECTIONAL));
    terrainMaterial->setRenderStateBlock(terrainRenderStateShadow.getHash(), RenderStage::SHADOW, RenderPassType::COUNT, to_U8(LightType::SPOT));
    terrainMaterial->setRenderStateBlock(terrainRenderStateShadow.getHash(), RenderStage::SHADOW, RenderPassType::COUNT, to_U8(LightType::POINT));

    terrain->setMaterialTpl(terrainMaterial);

    if (threadedLoading) {
        Start(*CreateTask(context.taskPool(TaskPoolType::HIGH_PRIORITY), [terrain, terrainDescriptor, &context](const Task & parent) {
            loadThreadedResources(terrain, context, terrainDescriptor);
        }));
    } else {
        loadThreadedResources(terrain, context, terrainDescriptor);
    }

    return true;
}

bool TerrainLoader::loadThreadedResources(Terrain_ptr terrain,
                                          PlatformContext& context,
                                          const std::shared_ptr<TerrainDescriptor> terrainDescriptor) {

    stringImpl terrainMapLocation = Paths::g_assetsLocation + Paths::g_heightmapLocation + terrainDescriptor->getVariable("descriptor");
    stringImpl terrainRawFile(terrainDescriptor->getVariable("heightfield"));

    const vec2<U16>& terrainDimensions = terrainDescriptor->dimensions();
    
    F32 minAltitude = terrainDescriptor->altitudeRange().x;
    F32 maxAltitude = terrainDescriptor->altitudeRange().y;
    BoundingBox& terrainBB = Attorney::TerrainLoader::boundingBox(*terrain);
    terrainBB.set(vec3<F32>(-terrainDimensions.x * 0.5f, minAltitude, -terrainDimensions.y * 0.5f),
                  vec3<F32>( terrainDimensions.x * 0.5f, maxAltitude,  terrainDimensions.y * 0.5f));

    const vec3<F32>& bMin = terrainBB.getMin();
    const vec3<F32>& bMax = terrainBB.getMax();

    ByteBuffer terrainCache;
    if (!g_disableLoadFromCache && terrainCache.loadFromFile((Paths::g_cacheLocation + Paths::g_terrainCacheLocation).c_str(), (terrainRawFile + ".cache").c_str())) {
        terrainCache >> terrain->_physicsVerts;
    }

    if (terrain->_physicsVerts.empty()) {

        vectorSTD<Byte> data(to_size(terrainDimensions.width) * terrainDimensions.height * (sizeof(U16) / sizeof(char)), Byte{0});
        readFile((terrainMapLocation + "/").c_str(), terrainRawFile.c_str(), data, FileType::BINARY);

        constexpr F32 ushortMax = std::numeric_limits<U16>::max() + 1.0f;

        I32 terrainWidth = to_I32(terrainDimensions.x);
        I32 terrainHeight = to_I32(terrainDimensions.y);

        terrain->_physicsVerts.resize(to_size(terrainWidth) * terrainHeight);

        // scale and translate all heights by half to convert from 0-255 (0-65335) to -127 - 128 (-32767 - 32768)
        F32 altitudeRange = maxAltitude - minAltitude;
        
        F32 bXRange = bMax.x - bMin.x;
        F32 bZRange = bMax.z - bMin.z;

        #pragma omp parallel for
        for (I32 height = 0; height < terrainHeight ; ++height) {
            for (I32 width = 0; width < terrainWidth; ++width) {
                I32 idxIMG = TER_COORD(width < terrainWidth - 1 ? width : width - 1,
                                       height < terrainHeight - 1 ? height : height - 1,
                                       terrainWidth);

                vec3<F32>& vertexData = terrain->_physicsVerts[TER_COORD(width, height, terrainWidth)]._position;


                F32 yOffset = 0.0f;
                U16* heightData = reinterpret_cast<U16*>(data.data());
                yOffset = (altitudeRange * (heightData[idxIMG] / ushortMax)) + minAltitude;


                //#pragma omp critical
                //Surely the id is unique and memory has also been allocated beforehand
                vertexData.set(bMin.x + (to_F32(width)) * bXRange / (terrainWidth - 1),       //X
                                yOffset,                                                      //Y
                                bMin.z + (to_F32(height)) * (bZRange) / (terrainHeight - 1)); //Z
            }
        }

        I32 offset = 2;
        #pragma omp parallel for
        for (I32 j = offset; j < terrainHeight - offset; ++j) {
            for (I32 i = offset; i < terrainWidth - offset; ++i) {
                vec3<F32> vU, vV, vUV;

                vU.set(terrain->_physicsVerts[TER_COORD(i + offset, j + 0, terrainWidth)]._position -
                       terrain->_physicsVerts[TER_COORD(i - offset, j + 0, terrainWidth)]._position);
                vV.set(terrain->_physicsVerts[TER_COORD(i + 0, j + offset, terrainWidth)]._position -
                       terrain->_physicsVerts[TER_COORD(i + 0, j - offset, terrainWidth)]._position);

                vUV.cross(vV, vU);
                vUV.normalize();
                vU = -vU;
                vU.normalize();

                //Again, not needed, I think
                //#pragma omp critical
                {
                    I32 idx = TER_COORD(i, j, terrainWidth);
                    VertexBuffer::Vertex& vert = terrain->_physicsVerts[idx];
                    vert._normal = Util::PACK_VEC3(vUV);
                    vert._tangent = Util::PACK_VEC3(vU);
                }
            }
        }
        
        for (I32 j = 0; j < offset; ++j) {
            for (I32 i = 0; i < terrainWidth; ++i) {
                I32 idx0 = TER_COORD(i, j, terrainWidth);
                I32 idx1 = TER_COORD(i, offset, terrainWidth);

                terrain->_physicsVerts[idx0]._normal = terrain->_physicsVerts[idx1]._normal;
                terrain->_physicsVerts[idx0]._tangent = terrain->_physicsVerts[idx1]._tangent;

                idx0 = TER_COORD(i, terrainHeight - 1 - j, terrainWidth);
                idx1 = TER_COORD(i, terrainHeight - 1 - offset, terrainWidth);

                terrain->_physicsVerts[idx0]._normal = terrain->_physicsVerts[idx1]._normal;
                terrain->_physicsVerts[idx0]._tangent = terrain->_physicsVerts[idx1]._tangent;
            }
        }

        for (I32 i = 0; i < offset; ++i) {
            for (I32 j = 0; j < terrainHeight; ++j) {
                I32 idx0 = TER_COORD(i, j, terrainWidth);
                I32 idx1 = TER_COORD(offset, j, terrainWidth);

                terrain->_physicsVerts[idx0]._normal = terrain->_physicsVerts[idx1]._normal;
                terrain->_physicsVerts[idx0]._tangent = terrain->_physicsVerts[idx1]._tangent;

                idx0 = TER_COORD(terrainWidth - 1 - i, j, terrainWidth);
                idx1 = TER_COORD(terrainWidth - 1 - offset, j, terrainWidth);

                terrain->_physicsVerts[idx0]._normal = terrain->_physicsVerts[idx1]._normal;
                terrain->_physicsVerts[idx0]._tangent = terrain->_physicsVerts[idx1]._tangent;
            }
        }
        terrainCache << terrain->_physicsVerts;
        terrainCache.dumpToFile((Paths::g_cacheLocation + Paths::g_terrainCacheLocation).c_str(), (terrainRawFile + ".cache").c_str());
    }

    // Do this first in case we have any threaded loads
    VegetationDetails& vegDetails = initializeVegetationDetails(terrain, context, terrainDescriptor);
    // Then compute quadtree and all additional terrain-related structures
    Attorney::TerrainLoader::postBuild(*terrain);
    Vegetation::createAndUploadGPUData(context.gfx(), terrain, vegDetails);

    Console::printfn(Locale::get(_ID("TERRAIN_LOAD_END")), terrain->resourceName().c_str());
    return terrain->load();
}

VegetationDetails& TerrainLoader::initializeVegetationDetails(std::shared_ptr<Terrain> terrain,
                                                              PlatformContext& context,
                                                              const std::shared_ptr<TerrainDescriptor> terrainDescriptor) {
    VegetationDetails& vegDetails = Attorney::TerrainLoader::vegetationDetails(*terrain);

    const U32 terrainWidth = terrainDescriptor->dimensions().width;
    const U32 terrainHeight = terrainDescriptor->dimensions().height;
    U32 chunkSize = to_U32(terrainDescriptor->tessellationSettings().x);
    U32 maxChunkCount = to_U32(std::ceil((terrainWidth * terrainHeight) / (chunkSize * chunkSize * 1.0f)));

    Vegetation::precomputeStaticData(terrain->getGeometryVB()->context(), chunkSize, maxChunkCount);

    for (I32 i = 1; i < 5; ++i) {
        stringImpl currentMesh = terrainDescriptor->getVariable(Util::StringFormat("treeMesh%d", i));
        if (!currentMesh.empty()) {
            vegDetails.treeMeshes.push_back(currentMesh);
        }

        vegDetails.treeRotations[i - 1].set(
            terrainDescriptor->getVariablef(Util::StringFormat("treeRotationX%d", i)),
            terrainDescriptor->getVariablef(Util::StringFormat("treeRotationY%d", i)),
            terrainDescriptor->getVariablef(Util::StringFormat("treeRotationZ%d", i))
        );
    }

    vegDetails.grassScales.set(
        terrainDescriptor->getVariablef("grassScale1"),
        terrainDescriptor->getVariablef("grassScale2"),
        terrainDescriptor->getVariablef("grassScale3"),
        terrainDescriptor->getVariablef("grassScale4"));

    vegDetails.treeScales.set(
        terrainDescriptor->getVariablef("treeScale1"),
        terrainDescriptor->getVariablef("treeScale2"),
        terrainDescriptor->getVariablef("treeScale3"),
        terrainDescriptor->getVariablef("treeScale4"));
 
    stringImpl currentImage = terrainDescriptor->getVariable("grassBillboard1");
    if (!currentImage.empty()) {
        vegDetails.billboardTextureArray += currentImage;
        vegDetails.billboardCount++;
    }

    currentImage = terrainDescriptor->getVariable("grassBillboard2");
    if (!currentImage.empty()) {
        vegDetails.billboardTextureArray += "," + currentImage;
        vegDetails.billboardCount++;
    }

    currentImage = terrainDescriptor->getVariable("grassBillboard3");
    if (!currentImage.empty()) {
        vegDetails.billboardTextureArray += "," + currentImage;
        vegDetails.billboardCount++;
    }

    currentImage = terrainDescriptor->getVariable("grassBillboard4");
    if (!currentImage.empty()) {
        vegDetails.billboardTextureArray += "," + currentImage;
        vegDetails.billboardCount++;
    }

    vegDetails.name = terrain->resourceName() + "_vegetation";
    vegDetails.parentTerrain = terrain;

    stringImpl terrainLocation = Paths::g_assetsLocation + Paths::g_heightmapLocation + terrainDescriptor->getVariable("descriptor");

    vegDetails.grassMap.reset(new ImageTools::ImageData);
    ImageTools::ImageDataInterface::CreateImageData(terrainLocation + "/" + terrainDescriptor->getVariable("grassMap"),
                                                    0, 0, false,
                                                    *vegDetails.grassMap);

    vegDetails.treeMap.reset(new ImageTools::ImageData);
    ImageTools::ImageDataInterface::CreateImageData(terrainLocation + "/" + terrainDescriptor->getVariable("treeMap"),
                                                    0, 0, false,
                                                    *vegDetails.treeMap);

    return vegDetails;
}

bool TerrainLoader::Save(const char* fileName) { return true; }

bool TerrainLoader::Load(const char* filename) { return true; }

const boost::property_tree::ptree& empty_ptree() {
    static boost::property_tree::ptree t;
    return t;
}
};
