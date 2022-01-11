#include "stdafx.h"

#include "Headers/Terrain.h"
#include "Headers/TerrainLoader.h"
#include "Headers/TerrainDescriptor.h"

#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Platform/Video/Headers/RenderStateBlock.h"

#include "Geometry/Material/Headers/Material.h"
#include "Managers/Headers/SceneManager.h"

namespace Divide {

namespace {
    constexpr U16 BYTE_BUFFER_VERSION = 1u;

    ResourcePath ClimatesLocation(U8 textureQuality) {
       CLAMP<U8>(textureQuality, 0u, 3u);

       return Paths::g_assetsLocation + Paths::g_heightmapLocation +
             (textureQuality == 3u ? Paths::g_climatesHighResLocation
                                   : textureQuality == 2u ? Paths::g_climatesMedResLocation
                                                          : Paths::g_climatesLowResLocation);
    }

    std::pair<U8, bool> FindOrInsert(const U8 textureQuality, vector<string>& container, const string& texture, string materialName) {
        if (!fileExists((ClimatesLocation(textureQuality) + "/" + materialName + "/" + texture).c_str())) {
            materialName = "std_default";
        }
        const string item = materialName + "/" + texture;
        const auto* const it = eastl::find(eastl::cbegin(container),
                                    eastl::cend(container),
                                    item);
        if (it != eastl::cend(container)) {
            return std::make_pair(to_U8(eastl::distance(eastl::cbegin(container), it)), false);
        }

        container.push_back(item);
        return std::make_pair(to_U8(container.size() - 1), true);
    }
}

bool TerrainLoader::loadTerrain(const Terrain_ptr& terrain,
                                const std::shared_ptr<TerrainDescriptor>& terrainDescriptor,
                                PlatformContext& context,
                                bool threadedLoading) {

    const string& name = terrainDescriptor->getVariable("terrainName");

    ResourcePath terrainLocation = Paths::g_assetsLocation + Paths::g_heightmapLocation;
    terrainLocation += terrainDescriptor->getVariable("descriptor");

    Attorney::TerrainLoader::descriptor(*terrain, terrainDescriptor);
    const U8 textureQuality = context.config().terrain.textureQuality;
    // Blend maps
    ResourceDescriptor textureBlendMap("Terrain Blend Map_" + name);
    textureBlendMap.assetLocation(terrainLocation);

    // Albedo maps and roughness
    ResourceDescriptor textureAlbedoMaps("Terrain Albedo Maps_" + name);
    textureAlbedoMaps.assetLocation(ClimatesLocation(textureQuality));

    // Normals
    ResourceDescriptor textureNormalMaps("Terrain Normal Maps_" + name);
    textureNormalMaps.assetLocation(ClimatesLocation(textureQuality));

    // AO and displacement
    ResourceDescriptor textureExtraMaps("Terrain Extra Maps_" + name);
    textureExtraMaps.assetLocation(ClimatesLocation(textureQuality));

    //temp data
    string layerOffsetStr;
    string currentMaterial;

    U8 layerCount = terrainDescriptor->textureLayers();

    const vector<std::pair<string, TerrainTextureChannel>> channels = {
        {"red", TerrainTextureChannel::TEXTURE_RED_CHANNEL},
        {"green", TerrainTextureChannel::TEXTURE_GREEN_CHANNEL},
        {"blue", TerrainTextureChannel::TEXTURE_BLUE_CHANNEL},
        {"alpha", TerrainTextureChannel::TEXTURE_ALPHA_CHANNEL}
    };

    vector<string> textures[to_base(TerrainTextureType::COUNT)] = {};
    vector<string> splatTextures = {};

    const size_t idxCount = layerCount * to_size(TerrainTextureChannel::COUNT);
    std::array<vector<U16>, to_base(TerrainTextureType::COUNT)> indices;
    std::array<U16, to_base(TerrainTextureType::COUNT)> offsets{};
    vector<U8> channelCountPerLayer(layerCount, 0u);

    for (auto& it : indices) {
        it.resize(idxCount, 255u);
    }
    offsets.fill(0u);

    const char* textureNames[to_base(TerrainTextureType::COUNT)] = {
        "Albedo_roughness", "Normal", "Displacement"
    };

    U16 idx = 0;
    for (U8 i = 0; i < layerCount; ++i) {
        layerOffsetStr = Util::to_string(i);
        splatTextures.push_back(terrainDescriptor->getVariable("blendMap" + layerOffsetStr));
        U8 j = 0;
        for (const auto &[channelName, channel] : channels) {
            currentMaterial = terrainDescriptor->getVariable(channelName + layerOffsetStr + "_mat");
            if (currentMaterial.empty()) {
                continue;
            }

            for (U8 k = 0; k < to_base(TerrainTextureType::COUNT); ++k) {
                const auto [index, wasNew] = FindOrInsert(textureQuality, textures[k], Util::StringFormat("%s.%s", textureNames[k], k == to_base(TerrainTextureType::ALBEDO_ROUGHNESS) ? "png" : "jpg"), currentMaterial);
                indices[k][idx] = index;
                if (wasNew) {
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

    ResourcePath blendMapArray = {};
    ResourcePath albedoMapArray = {};
    ResourcePath normalMapArray = {};
    ResourcePath extraMapArray = {};

    U16 extraMapCount = 0;
    for (const string& tex : textures[to_base(TerrainTextureType::ALBEDO_ROUGHNESS)]) {
        albedoMapArray.append(tex + ",");
    }
    for (const string& tex : textures[to_base(TerrainTextureType::NORMAL)]) {
        normalMapArray.append(tex + ",");
    }

    for (U8 i = to_U8(TerrainTextureType::DISPLACEMENT_AO); i < to_U8(TerrainTextureType::COUNT); ++i) {
        for (const string& tex : textures[i]) {
            extraMapArray.append(tex + ",");
            ++extraMapCount;
        }
    }

    for (const string& tex : splatTextures) {
        blendMapArray.append(tex + ",");
    }

    blendMapArray.pop_back();
    albedoMapArray.pop_back();
    normalMapArray.pop_back();
    extraMapArray.pop_back();

    SamplerDescriptor heightMapSampler = {};
    heightMapSampler.wrapUVW(TextureWrap::CLAMP_TO_BORDER);
    heightMapSampler.minFilter(TextureFilter::LINEAR);
    heightMapSampler.magFilter(TextureFilter::LINEAR);
    heightMapSampler.borderColour(DefaultColours::BLACK);
    heightMapSampler.anisotropyLevel(16);
    const size_t heightSamplerHash = heightMapSampler.getHash();

    SamplerDescriptor blendMapSampler = {};
    blendMapSampler.wrapUVW(TextureWrap::CLAMP_TO_EDGE);
    blendMapSampler.minFilter(TextureFilter::LINEAR_MIPMAP_LINEAR);
    blendMapSampler.magFilter(TextureFilter::LINEAR);
    blendMapSampler.anisotropyLevel(16);
    const size_t blendMapHash = blendMapSampler.getHash();

    SamplerDescriptor albedoSampler = {};
    albedoSampler.wrapUVW(TextureWrap::REPEAT);
    albedoSampler.minFilter(TextureFilter::LINEAR_MIPMAP_LINEAR);
    albedoSampler.magFilter(TextureFilter::LINEAR);
    albedoSampler.anisotropyLevel(16);
    const size_t albedoHash = albedoSampler.getHash();

    TextureDescriptor albedoDescriptor(TextureType::TEXTURE_2D_ARRAY);
    albedoDescriptor.layerCount(to_U16(textures[to_base(TerrainTextureType::ALBEDO_ROUGHNESS)].size()));
    albedoDescriptor.srgb(false);

    TextureDescriptor blendMapDescriptor(TextureType::TEXTURE_2D_ARRAY);
    blendMapDescriptor.layerCount(to_U16(splatTextures.size()));
    blendMapDescriptor.srgb(false);
    blendMapDescriptor.useDDSCache(false);

    TextureDescriptor normalDescriptor(TextureType::TEXTURE_2D_ARRAY);
    normalDescriptor.layerCount(to_U16(textures[to_base(TerrainTextureType::NORMAL)].size()));
    normalDescriptor.srgb(false);
    STUBBED("Find a way to properly compress normal maps! -Ionut");
    normalDescriptor.useDDSCache(false);

    TextureDescriptor extraDescriptor(TextureType::TEXTURE_2D_ARRAY);
    extraDescriptor.layerCount(extraMapCount);
    extraDescriptor.srgb(false);
    extraDescriptor.useDDSCache(false);

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

    Console::d_printfn(Locale::Get(_ID("TERRAIN_INFO")), terrainDimensions.width, terrainDimensions.height);

    const F32 underwaterTileScale = terrainDescriptor->getVariablef("underwaterTileScale");
    terrainMaterial->properties().shadingMode(ShadingMode::PBR_MR);

    const Terrain::ParallaxMode pMode = static_cast<Terrain::ParallaxMode>(CLAMPED(to_I32(to_U8(context.config().terrain.parallaxMode)), 0, 2));
    if (pMode == Terrain::ParallaxMode::NORMAL) {
        terrainMaterial->properties().bumpMethod(BumpMethod::PARALLAX);
    } else if (pMode == Terrain::ParallaxMode::OCCLUSION) {
        terrainMaterial->properties().bumpMethod(BumpMethod::PARALLAX_OCCLUSION);
    } else {
        terrainMaterial->properties().bumpMethod(BumpMethod::NONE);
    }

    terrainMaterial->properties().baseColour(FColour4(DefaultColours::WHITE.rgb * 0.5f, 1.0f));
    terrainMaterial->properties().metallic(0.0f);
    terrainMaterial->properties().roughness(0.8f);
    terrainMaterial->properties().parallaxFactor(0.3f);
    terrainMaterial->properties().toggleTransparency(false);

    U8 totalLayerCount = 0;
    string layerCountDataStr = Util::StringFormat("const uint CURRENT_LAYER_COUNT[ %d ] = {", layerCount);
    string blendAmntStr = "INSERT_BLEND_AMNT_ARRAY float blendAmount[TOTAL_LAYER_COUNT] = {";
    for (U8 i = 0; i < layerCount; ++i) {
        layerCountDataStr.append(Util::StringFormat("%d,", channelCountPerLayer[i]));
        for (U8 j = 0; j < channelCountPerLayer[i]; ++j) {
            blendAmntStr.append("0.0f,");
        }
        totalLayerCount += channelCountPerLayer[i];
    }
    layerCountDataStr.pop_back();
    layerCountDataStr.append("};");

    blendAmntStr.pop_back();
    blendAmntStr.append("};");

    const TerrainDescriptor::LayerData& layerTileData = terrainDescriptor->layerDataEntries();
    string tileFactorStr = "const vec2 CURRENT_TILE_FACTORS[TOTAL_LAYER_COUNT] = {\n";
    for (U8 i = 0; i < layerCount; ++i) {
        const TerrainDescriptor::LayerDataEntry& entry = layerTileData[i];
        for (U8 j = 0; j < channelCountPerLayer[i]; ++j) {
            const vec2<F32>& factors = entry[j];
            tileFactorStr.append(fmt::sprintf("%*c", 8, ' '));
            tileFactorStr.append(Util::StringFormat("vec2(%3.2f, %3.2f),\n", factors.s, factors.t));
        }
    }
    tileFactorStr.pop_back();
    tileFactorStr.pop_back();
    tileFactorStr.append("\n};");

    const char* idxNames[to_base(TerrainTextureType::COUNT)] = {
        "ALBEDO_IDX", "NORMAL_IDX", "DISPLACEMENT_IDX"
    };

    std::array<string, to_base(TerrainTextureType::COUNT)> indexData = {};
    for (U8 i = 0; i < to_base(TerrainTextureType::COUNT); ++i) {
        string& dataStr = indexData[i];
        dataStr = Util::StringFormat("const uint %s[ %d ] = {", idxNames[i], indices[i].size());
        for (const U16 idxTemp : indices[i]) {
            dataStr.append(Util::StringFormat("%d,", idxTemp));
        }
        dataStr.pop_back();
        dataStr.append("};");
    }

    ResourcePath helperTextures { terrainDescriptor->getVariable("waterCaustics") + "," +
                                  terrainDescriptor->getVariable("underwaterAlbedoTexture") + "," +
                                  terrainDescriptor->getVariable("underwaterDetailTexture") + "," +
                                  terrainDescriptor->getVariable("tileNoiseTexture") };

    TextureDescriptor helperTexDescriptor(TextureType::TEXTURE_2D_ARRAY);

    ResourceDescriptor textureWaterCaustics("Terrain Helper Textures_" + name);
    textureWaterCaustics.assetLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
    textureWaterCaustics.assetName(helperTextures);
    textureWaterCaustics.propertyDescriptor(helperTexDescriptor);

    ResourceDescriptor heightMapTexture("Terrain Heightmap_" + name);
    heightMapTexture.assetLocation(terrainLocation);
    heightMapTexture.assetName(ResourcePath{ terrainDescriptor->getVariable("heightfieldTex") });

    TextureDescriptor heightMapDescriptor(TextureType::TEXTURE_2D_ARRAY, GFXDataFormat::COUNT);
    heightMapTexture.propertyDescriptor(heightMapDescriptor);
    terrainMaterial->properties().isStatic(true);
    terrainMaterial->properties().isInstanced(true);
    terrainMaterial->setTexture(TextureUsage::UNIT0, CreateResource<Texture>(terrain->parentResourceCache(), textureAlbedoMaps), albedoHash, TextureOperation::NONE, TexturePrePassUsage::ALWAYS);
    terrainMaterial->setTexture(TextureUsage::OPACITY, CreateResource<Texture>(terrain->parentResourceCache(), textureBlendMap), blendMapHash, TextureOperation::NONE, TexturePrePassUsage::ALWAYS);
    terrainMaterial->setTexture(TextureUsage::NORMALMAP, CreateResource<Texture>(terrain->parentResourceCache(), textureNormalMaps), albedoHash, TextureOperation::NONE, TexturePrePassUsage::ALWAYS);
    terrainMaterial->setTexture(TextureUsage::HEIGHTMAP, CreateResource<Texture>(terrain->parentResourceCache(), heightMapTexture), heightSamplerHash, TextureOperation::NONE, TexturePrePassUsage::ALWAYS);
    terrainMaterial->setTexture(TextureUsage::SPECULAR, CreateResource<Texture>(terrain->parentResourceCache(), textureWaterCaustics), albedoHash, TextureOperation::NONE, TexturePrePassUsage::ALWAYS);
    terrainMaterial->setTexture(TextureUsage::METALNESS, CreateResource<Texture>(terrain->parentResourceCache(), textureExtraMaps), albedoHash, TextureOperation::NONE, TexturePrePassUsage::ALWAYS);

    const Configuration::Terrain terrainConfig = context.config().terrain;
    const vec2<F32> WorldScale = terrain->tessParams().WorldScale();
    Texture_ptr albedoTile = terrainMaterial->getTexture(TextureUsage::UNIT0).lock();
    WAIT_FOR_CONDITION(albedoTile->getState() == ResourceState::RES_LOADED);
    const U16 tileMapSize = albedoTile->width();

    terrainMaterial->addShaderDefine(ShaderType::COUNT, "TEXTURE_TILE_SIZE " + Util::to_string(tileMapSize), true);
    terrainMaterial->addShaderDefine(ShaderType::COUNT, "TERRAIN_HEIGHT_OFFSET " + Util::to_string(altitudeRange.x), true);
    terrainMaterial->addShaderDefine(ShaderType::COUNT, "WORLD_SCALE_X " + Util::to_string(WorldScale.width), true);
    terrainMaterial->addShaderDefine(ShaderType::COUNT, "WORLD_SCALE_Y " + Util::to_string(altitudeRange.y - altitudeRange.x), true);
    terrainMaterial->addShaderDefine(ShaderType::COUNT, "WORLD_SCALE_Z " + Util::to_string(WorldScale.height), true);
    terrainMaterial->addShaderDefine(ShaderType::COUNT, "INV_CONTROL_VTX_PER_TILE_EDGE " + Util::to_string(1.f / TessellationParams::VTX_PER_TILE_EDGE), true);
    terrainMaterial->addShaderDefine(ShaderType::COUNT, Util::StringFormat("CONTROL_VTX_PER_TILE_EDGE %d", TessellationParams::VTX_PER_TILE_EDGE), true);
    terrainMaterial->addShaderDefine(ShaderType::COUNT, Util::StringFormat("PATCHES_PER_TILE_EDGE %d", TessellationParams::PATCHES_PER_TILE_EDGE), true);
    terrainMaterial->addShaderDefine(ShaderType::COUNT, Util::StringFormat("MAX_TEXTURE_LAYERS %d", layerCount), true);
    if (terrainConfig.detailLevel > 1) {
        terrainMaterial->addShaderDefine(ShaderType::COUNT, "REDUCE_TEXTURE_TILE_ARTIFACT", true);
        if (terrainConfig.detailLevel > 2) {
            terrainMaterial->addShaderDefine(ShaderType::COUNT, "REDUCE_TEXTURE_TILE_ARTIFACT_ALL_LODS", true);
            if (terrainConfig.detailLevel > 3) {
                terrainMaterial->addShaderDefine(ShaderType::COUNT, "HIGH_QUALITY_TILE_ARTIFACT_REDUCTION", true);
            }
        }
    }
    terrainMaterial->addShaderDefine(ShaderType::FRAGMENT, Util::StringFormat("UNDERWATER_TILE_SCALE %d", to_I32(underwaterTileScale)), true);
    terrainMaterial->addShaderDefine(ShaderType::FRAGMENT, Util::StringFormat("TOTAL_LAYER_COUNT %d", totalLayerCount), true);
    terrainMaterial->addShaderDefine(ShaderType::FRAGMENT, layerCountDataStr, false);
    terrainMaterial->addShaderDefine(ShaderType::FRAGMENT, tileFactorStr, false);
    for (const string& str : indexData) {
        terrainMaterial->addShaderDefine(ShaderType::FRAGMENT, str, false);
    }
    terrainMaterial->addShaderDefine(ShaderType::FRAGMENT, blendAmntStr, true);

    if (!terrainMaterial->properties().receivesShadows()) {
        terrainMaterial->addShaderDefine(ShaderType::FRAGMENT, "DISABLE_SHADOW_MAPPING", true);
    }

    const Terrain::WireframeMode wMode = terrainConfig.wireframe 
                                                ? Terrain::WireframeMode::EDGES 
                                                : terrainConfig.showNormals
                                                        ? Terrain::WireframeMode::NORMALS
                                                        : terrainConfig.showLoDs 
                                                            ? Terrain::WireframeMode::LODS
                                                            : terrainConfig.showTessLevels 
                                                                ? Terrain::WireframeMode::TESS_LEVELS
                                                                : terrainConfig.showBlendMap 
                                                                    ? Terrain::WireframeMode::BLEND_MAP
                                                                    : Terrain::WireframeMode::NONE;
    const auto buildShaders = [=](Material* matInstance) {
        assert(matInstance != nullptr);
        
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

        const bool hasGeometryPass = wMode == Terrain::WireframeMode::EDGES || wMode == Terrain::WireframeMode::NORMALS;
        if (hasGeometryPass) {
            shaderDescriptor._modules.push_back(geomModule);
        }

        shaderDescriptor._modules.push_back(fragModule);

        string propName;
        bool hasParallax = false;
        for (ShaderModuleDescriptor& shaderModule : shaderDescriptor._modules) {
            string shaderPropName;

            if (wMode != Terrain::WireframeMode::NONE) {
                if (hasGeometryPass) {
                    shaderPropName += ".DebugView";
                    shaderModule._defines.emplace_back("TOGGLE_DEBUG", true);
                }

                if (wMode == Terrain::WireframeMode::EDGES) {
                    shaderPropName += ".WireframeView";
                    shaderModule._defines.emplace_back("TOGGLE_WIREFRAME", true);
                } else if (wMode == Terrain::WireframeMode::NORMALS) {
                    shaderPropName += ".PreviewNormals";
                    shaderModule._defines.emplace_back("TOGGLE_NORMALS", true);
                } else if (wMode == Terrain::WireframeMode::LODS) {
                    shaderPropName += ".PreviewLoDs";
                    shaderModule._defines.emplace_back("TOGGLE_LODS", true);
                } else if (wMode == Terrain::WireframeMode::TESS_LEVELS) {
                    shaderPropName += ".PreviewTessLevels";
                    shaderModule._defines.emplace_back("TOGGLE_TESS_LEVEL", true);
                } else if (wMode == Terrain::WireframeMode::BLEND_MAP) {
                    shaderPropName += ".PreviewBlendMap";
                    shaderModule._defines.emplace_back("TOGGLE_BLEND_MAP", true);
                }
            }

            if (shaderPropName.length() > propName.length()) {
                propName = shaderPropName;
            }
        }

        // SHADOW
        ShaderProgramDescriptor shadowDescriptorVSM = {};
        for (const ShaderModuleDescriptor& shaderModule : shaderDescriptor._modules) {
            shadowDescriptorVSM._modules.push_back(shaderModule);
            ShaderModuleDescriptor& tempModule = shadowDescriptorVSM._modules.back();

            if (tempModule._moduleType == ShaderType::FRAGMENT) {
                tempModule._variant = "Shadow.VSM";
            }
            tempModule._defines.emplace_back("MAX_TESS_LEVEL 32", true);
        }

        shadowDescriptorVSM._name = "Terrain_ShadowVSM-" + name + propName;

        // MAIN PASS
        ShaderProgramDescriptor colourDescriptor = shaderDescriptor;
        if (hasParallax) {
            for (ShaderModuleDescriptor& shaderModule : colourDescriptor._modules) {
                if (shaderModule._moduleType == ShaderType::FRAGMENT) {
                    shaderModule._defines.emplace_back("HAS_PARALLAX", true);
                    break;
                }
            }
        }

        colourDescriptor._name = "Terrain_Colour-" + name + propName + (hasParallax ? ".Parallax":"");

        // PRE PASS
        ShaderProgramDescriptor prePassDescriptor = colourDescriptor;

        for (ShaderModuleDescriptor& shaderModule : prePassDescriptor._modules) {
            if (shaderModule._moduleType == ShaderType::FRAGMENT) {
                shaderModule._variant = "PrePass";
            }
            shaderModule._defines.emplace_back("PRE_PASS", true);
        }

        prePassDescriptor._name = "Terrain_PrePass-" + name + propName + (hasParallax ? ".Parallax" : "");

        // PRE PASS LQ
        ShaderProgramDescriptor prePassDescriptorLQ = prePassDescriptor;
        for (ShaderModuleDescriptor& shaderModule : prePassDescriptorLQ._modules) {
            shaderModule._defines.emplace_back("LOW_QUALITY", true);
            shaderModule._defines.emplace_back("MAX_TESS_LEVEL 16", true);
        }
        
        prePassDescriptorLQ._name = "Terrain_PrePass_LowQuality-" + name + propName;

        // MAIN PASS LQ
        ShaderProgramDescriptor lowQualityDescriptor = shaderDescriptor;
        for (ShaderModuleDescriptor& shaderModule : lowQualityDescriptor._modules) {
            if (shaderModule._moduleType == ShaderType::FRAGMENT) {
                shaderModule._variant = "LQPass";
            }

            shaderModule._defines.emplace_back("LOW_QUALITY", true);
            shaderModule._defines.emplace_back("MAX_TESS_LEVEL 16", true);
        }

        lowQualityDescriptor._name = "Terrain_Colour_LowQuality-" + name + propName;

        // Reflect Pass LQ
        ShaderProgramDescriptor lowQualityDescriptorReflect = lowQualityDescriptor;
        for (ShaderModuleDescriptor& shaderModule : lowQualityDescriptorReflect._modules) {
            shaderModule._defines.emplace_back("REFLECTION_PASS", true);
        }
        lowQualityDescriptorReflect._name = "Terrain_Colour_LowQuality_Reflect-" + name + propName;

        matInstance->setShaderProgram(prePassDescriptorLQ,         RenderStage::COUNT,      RenderPassType::PRE_PASS);
        matInstance->setShaderProgram(prePassDescriptor,           RenderStage::DISPLAY,    RenderPassType::PRE_PASS);
        matInstance->setShaderProgram(lowQualityDescriptor,        RenderStage::REFRACTION, RenderPassType::MAIN_PASS);
        matInstance->setShaderProgram(lowQualityDescriptorReflect, RenderStage::REFLECTION, RenderPassType::MAIN_PASS);
        matInstance->setShaderProgram(colourDescriptor,            RenderStage::DISPLAY,    RenderPassType::MAIN_PASS);
        matInstance->setShaderProgram(shadowDescriptorVSM,         RenderStage::SHADOW,     RenderPassType::COUNT);
    };

    buildShaders(terrainMaterial.get());

    terrainMaterial->customShaderCBK([=](Material& material, [[maybe_unused]] const RenderStagePass stagePass) {

        buildShaders(&material);
        return true;
    });

    RenderStateBlock terrainRenderState = {};
    terrainRenderState.setTessControlPoints(4);
    terrainRenderState.setCullMode(CullMode::BACK);
    terrainRenderState.setZFunc(ComparisonFunction::EQUAL);

    RenderStateBlock terrainRenderStatePrePass = terrainRenderState;
    terrainRenderStatePrePass.setZFunc(ComparisonFunction::LEQUAL);

    { //Normal rendering
        terrainMaterial->setRenderStateBlock(terrainRenderStatePrePass.getHash(), RenderStage::COUNT, RenderPassType::PRE_PASS);
        terrainMaterial->setRenderStateBlock(terrainRenderState.getHash()       , RenderStage::COUNT, RenderPassType::MAIN_PASS);
    }
    { //Reflected rendering
        RenderStateBlock terrainRenderStateReflection = terrainRenderState;
        terrainRenderStateReflection.setCullMode(CullMode::FRONT);

        RenderStateBlock terrainRenderStateReflectionPrePass = terrainRenderStatePrePass;
        terrainRenderStateReflectionPrePass.setCullMode(CullMode::FRONT);

        terrainMaterial->setRenderStateBlock(terrainRenderStateReflectionPrePass.getHash(), RenderStage::REFLECTION, RenderPassType::PRE_PASS, static_cast<RenderStagePass::VariantType>(ReflectorType::PLANAR));
        terrainMaterial->setRenderStateBlock(terrainRenderStateReflection.getHash(),        RenderStage::REFLECTION, RenderPassType::MAIN_PASS, static_cast<RenderStagePass::VariantType>(ReflectorType::PLANAR));

    }
    { //Shadow rendering
        RenderStateBlock terrainRenderStateShadow = terrainRenderStatePrePass;
        terrainRenderStateShadow.setColourWrites(true, true, false, false);
        terrainRenderStateShadow.setZFunc(ComparisonFunction::LESS);

        terrainMaterial->setRenderStateBlock(terrainRenderStateShadow.getHash(), RenderStage::SHADOW, RenderPassType::COUNT);
    }

    terrain->setMaterialTpl(terrainMaterial);

    Start(*CreateTask([terrain, terrainDescriptor, &context](const Task&) {
        if (!loadThreadedResources(terrain, context, terrainDescriptor)) {
            DIVIDE_UNEXPECTED_CALL();
        }
    }), 
    context.taskPool(TaskPoolType::HIGH_PRIORITY),
    threadedLoading ? TaskPriority::DONT_CARE : TaskPriority::REALTIME);

    return true;
}

bool TerrainLoader::loadThreadedResources(const Terrain_ptr& terrain,
                                          PlatformContext& context,
                                          const std::shared_ptr<TerrainDescriptor>& terrainDescriptor) {

    ResourcePath terrainMapLocation{ Paths::g_assetsLocation + Paths::g_heightmapLocation + terrainDescriptor->getVariable("descriptor") };
    ResourcePath terrainRawFile{ terrainDescriptor->getVariable("heightfield") };

    const vec2<U16>& terrainDimensions = terrainDescriptor->dimensions();
    
    const F32 minAltitude = terrainDescriptor->altitudeRange().x;
    const F32 maxAltitude = terrainDescriptor->altitudeRange().y;
    BoundingBox& terrainBB = Attorney::TerrainLoader::boundingBox(*terrain);
    terrainBB.set(vec3<F32>(-terrainDimensions.x * 0.5f, minAltitude, -terrainDimensions.y * 0.5f),
                  vec3<F32>( terrainDimensions.x * 0.5f, maxAltitude,  terrainDimensions.y * 0.5f));

    const vec3<F32>& bMin = terrainBB.getMin();
    const vec3<F32>& bMax = terrainBB.getMax();

    ByteBuffer terrainCache;
    if (terrainCache.loadFromFile((Paths::g_cacheLocation + Paths::g_terrainCacheLocation).c_str(), (terrainRawFile + ".cache").c_str())) {
        auto tempVer = decltype(BYTE_BUFFER_VERSION){0};
        terrainCache >> tempVer;
        if (tempVer == BYTE_BUFFER_VERSION) {
            terrainCache >> terrain->_physicsVerts;
        } else {
            terrainCache.clear();
        }
    }

    if (terrain->_physicsVerts.empty()) {

        vector<Byte> data(to_size(terrainDimensions.width) * terrainDimensions.height * (sizeof(U16) / sizeof(char)), Byte{0});
        if (readFile((terrainMapLocation + "/").c_str(), terrainRawFile.c_str(), data, FileType::BINARY) != FileError::NONE) {
            NOP();
        }

        constexpr F32 ushortMax = 1.f + U16_MAX;

        const I32 terrainWidth = to_I32(terrainDimensions.x);
        const I32 terrainHeight = to_I32(terrainDimensions.y);

        terrain->_physicsVerts.resize(to_size(terrainWidth) * terrainHeight);

        // scale and translate all heights by half to convert from 0-255 (0-65335) to -127 - 128 (-32767 - 32768)
        const F32 altitudeRange = maxAltitude - minAltitude;
        
        const F32 bXRange = bMax.x - bMin.x;
        const F32 bZRange = bMax.z - bMin.z;

        const bool flipHeight = !ImageTools::UseUpperLeftOrigin();

        #pragma omp parallel for
        for (I32 height = 0; height < terrainHeight; ++height) {
            for (I32 width = 0; width < terrainWidth; ++width) {
                const I32 idxTER = TER_COORD(width, height, terrainWidth);
                vec3<F32>& vertexData = terrain->_physicsVerts[idxTER]._position;


                F32 yOffset = 0.0f;
                const U16* heightData = reinterpret_cast<U16*>(data.data());

                const I32 coordX = width < terrainWidth - 1 ? width : width - 1;
                I32 coordY = (height < terrainHeight - 1 ? height : height - 1);
                if (flipHeight) {
                    coordY = terrainHeight - 1 - coordY;
                }
                const I32 idxIMG = TER_COORD(coordX, coordY, terrainWidth);
                yOffset = altitudeRange * (heightData[idxIMG] / ushortMax) + minAltitude;


                //#pragma omp critical
                //Surely the id is unique and memory has also been allocated beforehand
                vertexData.set(bMin.x + to_F32(width) * bXRange / (terrainWidth - 1),       //X
                               yOffset,                                                     //Y
                               bMin.z + to_F32(height) * bZRange / (terrainHeight - 1));    //Z
            }
        }

        constexpr I32 offset = 2;
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
                    const I32 idx = TER_COORD(i, j, terrainWidth);
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

        terrainCache << BYTE_BUFFER_VERSION;
        terrainCache << terrain->_physicsVerts;
        if (!terrainCache.dumpToFile((Paths::g_cacheLocation + Paths::g_terrainCacheLocation).c_str(), (terrainRawFile + ".cache").c_str())) {
            DIVIDE_UNEXPECTED_CALL();
        }
    }

    // Then compute quadtree and all additional terrain-related structures
    Attorney::TerrainLoader::postBuild(*terrain);

    // Do this first in case we have any threaded loads
    const VegetationDetails& vegDetails = initializeVegetationDetails(terrain, context, terrainDescriptor);
    Vegetation::createAndUploadGPUData(context.gfx(), terrain, vegDetails);

    Console::printfn(Locale::Get(_ID("TERRAIN_LOAD_END")), terrain->resourceName().c_str());
    return terrain->load();
}

VegetationDetails& TerrainLoader::initializeVegetationDetails(const Terrain_ptr& terrain,
                                                              PlatformContext& context,
                                                              const std::shared_ptr<TerrainDescriptor>& terrainDescriptor) {
    VegetationDetails& vegDetails = Attorney::TerrainLoader::vegetationDetails(*terrain);

    const U32 chunkSize = terrain->getQuadtree().targetChunkDimension();
    assert(chunkSize > 0u);

    const U32 terrainWidth = terrainDescriptor->dimensions().width;
    const U32 terrainHeight = terrainDescriptor->dimensions().height;
    const U32 maxChunkCount = to_U32(std::ceil(terrainWidth * terrainHeight / (chunkSize * chunkSize * 1.0f)));

    Vegetation::precomputeStaticData(context.gfx(), chunkSize, maxChunkCount);

    for (I32 i = 1; i < 5; ++i) {
        string currentMesh = terrainDescriptor->getVariable(Util::StringFormat("treeMesh%d", i));
        if (!currentMesh.empty()) {
            vegDetails.treeMeshes.push_back(ResourcePath{ currentMesh });
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
 
    string currentImage = terrainDescriptor->getVariable("grassBillboard1");
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

    const ResourcePath terrainLocation{ Paths::g_assetsLocation + Paths::g_heightmapLocation + terrainDescriptor->getVariable("descriptor") };

    vegDetails.grassMap.reset(new ImageTools::ImageData);
    vegDetails.treeMap.reset(new ImageTools::ImageData);

    const ResourcePath grassMap{ terrainDescriptor->getVariable("grassMap")};
    const ResourcePath treeMap{ terrainDescriptor->getVariable("treeMap") };
    if (!vegDetails.grassMap->loadFromFile(false, 0, 0, terrainLocation, grassMap, true)) {
        DIVIDE_UNEXPECTED_CALL();
    }
    if (!vegDetails.treeMap->loadFromFile(false, 0, 0, terrainLocation, treeMap, true)) {
        DIVIDE_UNEXPECTED_CALL();
    }

    return vegDetails;
}

bool TerrainLoader::Save([[maybe_unused]] const char* fileName) {
    return true;
}

bool TerrainLoader::Load([[maybe_unused]] const char* fileName) {
    return true;
}

}
