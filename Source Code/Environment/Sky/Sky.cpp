#include "stdafx.h"

#include "Headers/Sky.h"

#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Headers/EngineTaskPool.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Headers/Sun.h"

#include "Managers/Headers/SceneManager.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Geometry/Material/Headers/Material.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Platform/Video/Headers/RenderPackage.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Geometry/Shapes/Predefined/Headers/Sphere3D.h"
#include "ECS/Components/Headers/RenderingComponent.h"

#ifdef _MSC_VER
# pragma warning (push)
# pragma warning (disable: 4505)
#endif

#include <TileableVolumeNoise/TileableVolumeNoise.h>
#include <CurlNoise/CurlNoise/Curl.h>

#ifndef STBI_INCLUDE_STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <STB/stb_image_write.h>
#include <STB/stb_image.h>
#endif //STBI_INCLUDE_STB_IMAGE_WRITE_H
#ifdef _MSC_VER
# pragma warning (pop)
#endif

namespace Divide {

namespace {
    const auto procLocation = []() {
        return Paths::g_assetsLocation + Paths::g_texturesLocation + Paths::g_proceduralTxturesLocation;
    };

    const char* curlTexName = "curlnoise.bmp";
    const char* weatherTexName = "weather.bmp";
    const char* worlTexName = "worlnoise.bmp";
    const char* perlWorlTexName = "perlworlnoise.tga";

    void GenerateCurlNoise(const char* fileName, const I32 width, const I32 height, const U8 channelCount) {
        Byte* data = MemoryManager_NEW Byte[width * height * channelCount];
        constexpr F32 frequency[] = { 8.0f, 6.0f, 4.0f };

        for (U8 pass = 0u; pass < 3; ++pass) {
            CurlNoise::SetCurlSettings(false, frequency[pass], 3, 2.f, 0.5f);
            for (I32 i = 0; i < width * height * channelCount; i += 3) {
                const Vectormath::Aos::Vector3 pos
                {
                    to_F32( (i / channelCount) % height)  / height,
                    to_F32(((i / channelCount) / height)) / height,
                    height / 10000.f 
                };

                const auto res = CurlNoise::ComputeCurlNoBoundaries(pos);
                const vec3<F32> curl = Normalized(vec3<F32>{res.val[0], res.val[1], res.val[2]});
                const F32 cellFBM0 = curl.r * 0.5f + curl.g * 0.35f + curl.b * 0.15f;

                data[i + pass] = to_byte(cellFBM0 * 128.f + 127.f);
            }
        }
        stbi_write_bmp(fileName, width, height, channelCount, data);
        MemoryManager::SAFE_DELETE(data);
    }

    void GeneratePerlinNoise(const char* fileName, const I32 width, const I32 height, const U8 channelCount) {
        const auto smoothstep = [](const F32 edge0, const F32 edge1, const F32 x) {
            const F32 t = std::min(std::max((x - edge0) / (edge1 - edge0), 0.0f), 1.0f);
            return t * t * (3.f - 2.f * t);
        };


        Byte* data = MemoryManager_NEW Byte[width * height * channelCount];
        for (I32 i = 0; i < width * height * channelCount; i += 3) {
            const glm::vec3 pos = glm::vec3(to_F32((i / channelCount) % width) / to_F32(width),
                                            to_F32((i / channelCount) / height) / to_F32(height),
                                            0.051f);
            const glm::vec3 offset1 = glm::vec3(0.f, 0.f, 581.163f);
            const glm::vec3 offset2 = glm::vec3(0.f, 0.f, 1245.463f);
            //const glm::vec3 offset3 = glm::vec3(0.f, 0.f, 2245.863f);

            const F32 perlinNoise  = Tileable3dNoise::PerlinNoise(pos, 8, 3);
            const F32 perlinNoise2 = Tileable3dNoise::PerlinNoise(pos + offset1, 8, 3);
                  F32 perlinNoise3 = Tileable3dNoise::PerlinNoise(pos + offset2, 2, 3);
                  //F32 perlinNoise4 = Tileable3dNoise::PerlinNoise(pos + offset3, 4, 3);
            perlinNoise3 = std::min(1.f, (smoothstep(0.45f, 0.8f, perlinNoise3) + smoothstep(0.25f, 0.45f, perlinNoise3) * 0.5f));
            data[i + 0] = to_byte(perlinNoise * 128.f + 127.f);
            data[i + 1] = to_byte(smoothstep(0.5f, 0.7f, perlinNoise2) * 255.f);
            data[i + 2] = to_byte(perlinNoise3 * 255.f);
        }
        stbi_write_bmp(fileName, width, height, channelCount, data);
        MemoryManager::SAFE_DELETE(data);
    }

    void GenerateWorleyNoise(const char* fileName, const I32 width, const I32 height, const U8 channelCount, const I32 slices) {
        Byte* data = MemoryManager_NEW Byte[slices * width * height * channelCount];
        for (I32 i = 0; i < slices * width * height * channelCount; i += 3) {
            const glm::vec3 pos = glm::vec3(to_F32((i / channelCount) % height) / to_F32(height), 
                                            to_F32(((i / channelCount) / height) % height) / to_F32(height), 
                                            to_F32((i / channelCount) / (width * slices)) / to_F32(height));
            const F32 cell0 = 1.0f - Tileable3dNoise::WorleyNoise(pos, 2);
            const F32 cell1 = 1.0f - Tileable3dNoise::WorleyNoise(pos, 4);
            const F32 cell2 = 1.0f - Tileable3dNoise::WorleyNoise(pos, 8);
            const F32 cell3 = 1.0f - Tileable3dNoise::WorleyNoise(pos, 16);

            const F32 cellFBM0 = cell0 * 0.5f + cell1 * 0.35f + cell2 * 0.15f;
            const F32 cellFBM1 = cell1 * 0.5f + cell2 * 0.35f + cell3 * 0.15f;
            const F32 cellFBM2 = cell2 * 0.75f + cell3 * 0.25f; // cellCount=4 -> worleyNoise4 is just noise due to sampling frequency=texel freque. So only take into account 2 frequenciM
            data[i + 0] = to_byte(cellFBM0 * 255);
            data[i + 1] = to_byte(cellFBM1 * 255);
            data[i + 2] = to_byte(cellFBM2 * 255);
        }
        stbi_write_bmp(fileName, width * slices, height, channelCount, data);
        MemoryManager::SAFE_DELETE(data);
    }

    void GeneratePerlinWorleyNoise(PlatformContext& context, const char* fileName, const I32 width, const I32 height, const U8 channelCount, const I32 slices) {
        Byte* data = MemoryManager_NEW Byte[slices * width * height * channelCount];
#if 0
        ParallelForDescriptor descriptor = {};
        descriptor._iterCount = slices * width * height * channelCount;
        descriptor._partitionSize = slices * 4;
        descriptor._cbk = [&](const Task*, const U32 start, const U32 end) -> void {
           for (U32 i = start; i < end; i += 4) {
#else
            for (U32 i = 0; i < to_U32(slices * width * height * channelCount); i += 4) {
#endif
                const glm::vec3 pos = glm::vec3(to_F32((i / channelCount) % height) / to_F32(height),
                                                to_F32(((i / channelCount) / height) % height) / to_F32(height),
                                                to_F32((i / channelCount) / (slices * width)) / to_F32(height));
                // Perlin FBM noise
                const F32 perlinNoise = Tileable3dNoise::PerlinNoise(pos, 8, 3);

                const F32 worleyNoise00 = (1.0f - Tileable3dNoise::WorleyNoise(pos, 8));
                const F32 worleyNoise01 = (1.0f - Tileable3dNoise::WorleyNoise(pos, 32));
                const F32 worleyNoise02 = (1.0f - Tileable3dNoise::WorleyNoise(pos, 56));
                //const F32 worleyNoise3 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 80));
                //const F32 worleyNoise4 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 104));
                //const F32 worleyNoise5 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 128)); // half the frequency of texel, we should not go further (with cellCount = 32 and texture size = 64)
                                                                                              // PerlinWorley noise as described p.101 of GPU Pro 7
                const F32 worleyFBM = worleyNoise00 * 0.625f + worleyNoise01 * 0.25f + worleyNoise02 * 0.125f;
                const F32 PerlWorlNoise = MAP(perlinNoise, 0.f, 1.f, worleyFBM, 1.f);

                //F32 worleyNoise0 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 4));
                //F32 worleyNoise1 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 8));
                const F32 worleyNoise12 = (1.0f - Tileable3dNoise::WorleyNoise(pos, 16));
                //F32 worleyNoise3 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 32));
                const F32 worleyNoise14 = (1.0f - Tileable3dNoise::WorleyNoise(pos, 64));
                // Three frequency of Worley FBM noise
                const F32 worleyFBM0 = worleyNoise00 * 0.625f + worleyNoise12 * 0.25f + worleyNoise01 * 0.125f;
                const F32 worleyFBM1 = worleyNoise12 * 0.625f + worleyNoise01 * 0.25f + worleyNoise14 * 0.125f;
                const F32 worleyFBM2 = worleyNoise01 * 0.75f + worleyNoise14 * 0.25f; // cellCount=4 -> worleyNoise5 is just noise due to sampling frequency=texel frequency. So only take into account 2 frequencies for FBM
                data[i + 0] = to_byte(PerlWorlNoise * 255);
                data[i + 1] = to_byte(worleyFBM0 * 255);
                data[i + 2] = to_byte(worleyFBM1 * 255);
                data[i + 3] = to_byte(worleyFBM2 * 255);
            }
#if 0
        };
        parallel_for(context, descriptor);
#endif
        stbi_write_tga(fileName, slices * width, height, channelCount, data);
        MemoryManager::DELETE_ARRAY(data);
    }
}


void Sky::OnStartup(PlatformContext& context) {
    static bool init = false;
    if (init) {
        return;
    }

    init = true;
    //ref: https://github.com/clayjohn/realtime_clouds/blob/master/gen_noise.cpp
    std::array<Task*, 3> tasks{ nullptr };

    const ResourcePath curlNoise = procLocation() + curlTexName;
    const ResourcePath weather = procLocation() + weatherTexName;
    const ResourcePath worlNoise = procLocation() + worlTexName;
    const ResourcePath perWordNoise = procLocation() + perlWorlTexName;

    if (!fileExists(curlNoise)) {
        Console::printfn("Generating Curl Noise 128x128 RGB");
        tasks[0] = CreateTask(context, [&curlNoise](const Task&) { GenerateCurlNoise(curlNoise.c_str(), 128, 128, 3); });
        Start(*tasks[0]);
        Console::printfn("Done!");
    }

    if (!fileExists(weather)) {
        Console::printfn("Generating Perlin Noise for LUT's");
        Console::printfn("Generating weather Noise 512x512 RGB");
        tasks[1] = CreateTask(context, [&weather](const Task&) { GeneratePerlinNoise(weather.c_str(), 512, 512, 3); });
        Start(*tasks[1]);
        Console::printfn("Done!");
    }

    if (!fileExists(worlNoise)) {
        //worley and perlin-worley are from github/sebh/TileableVolumeNoise
        //which is in turn based on noise described in 'real time rendering of volumetric cloudscapes for horizon zero dawn'
        Console::printfn("Generating Worley Noise 32x32x32 RGB");
        tasks[2] = CreateTask(context, [&worlNoise](const Task&) { GenerateWorleyNoise(worlNoise.c_str(), 32, 32, 3, 32); });
        Start(*tasks[2]);
        Console::printfn("Done!");
    }

    if (!fileExists(perWordNoise)) {
        Console::printfn("Generating Perlin-Worley Noise 128x128x128 RGBA");
        GeneratePerlinWorleyNoise(context, perWordNoise.c_str(), 128, 128, 4, 128);
        Console::printfn("Done!");
    }

    bool keepWaiting = true;
    while(keepWaiting) {
        keepWaiting = false;
        for (Task* task : tasks) {
            if (task != nullptr && !Finished(*task)) {
                keepWaiting = true;
                break;
            }
        }
    }
}

Sky::Sky(GFXDevice& context, ResourceCache* parentCache, size_t descriptorHash, const Str256& name, U32 diameter)
    : SceneNode(parentCache, descriptorHash, name, ResourcePath{ name }, {}, SceneNodeType::TYPE_SKY, to_base(ComponentType::TRANSFORM) | to_base(ComponentType::BOUNDS) | to_base(ComponentType::SCRIPT)),
      _context(context),
      _diameter(diameter)
{
    nightSkyColour({ 0.05f, 0.06f, 0.1f, 1.f });
    moonColour({1.0f, 1.f, 0.8f});

    time_t t = time(nullptr);
    _sun.SetLocation(-2.589910f, 51.45414f); // Bristol :D
    _sun.SetDate(localtime(&t));

    _renderState.addToDrawExclusionMask(RenderStage::SHADOW);

    // Generate a render state
    RenderStateBlock skyboxRenderState = {};
    skyboxRenderState.setCullMode(CullMode::FRONT);
    skyboxRenderState.setZFunc(ComparisonFunction::EQUAL);
    _skyboxRenderStateHash = skyboxRenderState.getHash();

    skyboxRenderState.setZFunc(ComparisonFunction::LEQUAL);
    skyboxRenderState.setColourWrites(false, false, false, false);
    _skyboxRenderStateHashPrePass = skyboxRenderState.getHash();

    RenderStateBlock skyboxRenderStateReflection = {};
    skyboxRenderStateReflection.setCullMode(CullMode::BACK);
    skyboxRenderStateReflection.setZFunc(ComparisonFunction::EQUAL);
    _skyboxRenderStateReflectedHash = skyboxRenderStateReflection.getHash();

    skyboxRenderStateReflection.setZFunc(ComparisonFunction::LEQUAL);
    skyboxRenderStateReflection.setColourWrites(false, false, false, false);
    _skyboxRenderStateReflectedHashPrePass = skyboxRenderStateReflection.getHash();

    getEditorComponent().onChangedCbk([this](const std::string_view field) {
        if (field == "Reset To Default") {
            _atmosphere = defaultAtmosphere();
        } else if (field == "Enable Procedural Clouds") {
            rebuildDrawCommands(true);
        }

        _atmosphereChanged = EditorDataState::QUEUED; 
    });

    EditorComponentField separatorField = {};
    separatorField._name = "Atmosphere";
    separatorField._type = EditorComponentFieldType::SEPARATOR;
    getEditorComponent().registerField(MOV(separatorField));

    EditorComponentField sunIntensityField = {};
    sunIntensityField._name = "Sun Intensity";
    sunIntensityField._tooltip = "(0.01x - 100.0x)";
    sunIntensityField._data = &_atmosphere._sunIntensity;
    sunIntensityField._type = EditorComponentFieldType::PUSH_TYPE;
    sunIntensityField._readOnly = false;
    sunIntensityField._range = { 0.01f, 100.0f };
    sunIntensityField._basicType = GFX::PushConstantType::FLOAT;
    getEditorComponent().registerField(MOV(sunIntensityField));

    EditorComponentField sunScaleField = {};
    sunScaleField._name = "Sun Scale";
    sunScaleField._tooltip = "(0.01x - 100.0x)";
    sunScaleField._data = &_atmosphere._sunScale;
    sunScaleField._type = EditorComponentFieldType::PUSH_TYPE;
    sunScaleField._readOnly = false;
    sunScaleField._range = { 0.01f, 100.0f };
    sunScaleField._basicType = GFX::PushConstantType::FLOAT;
    getEditorComponent().registerField(MOV(sunScaleField));

    EditorComponentField distanceMultiplierField = {};
    distanceMultiplierField._name = "Distance Multplier x10";
    distanceMultiplierField._tooltip = "Affects atmosphere and ray offsets";
    distanceMultiplierField._data = &_atmosphere._distanceMultiplier;
    distanceMultiplierField._type = EditorComponentFieldType::PUSH_TYPE;
    distanceMultiplierField._readOnly = false;
    distanceMultiplierField._range = { 10, 2000 };
    distanceMultiplierField._basicType = GFX::PushConstantType::INT;
    getEditorComponent().registerField(MOV(distanceMultiplierField));

    EditorComponentField rayOffsetField = {};
    rayOffsetField._name = "Ray Offset: [-10...10]m";
    rayOffsetField._tooltip = "Adds an offset from the planet radius to the atmosphere ray origin";
    rayOffsetField._data = &_atmosphere._rayOriginDistance;
    rayOffsetField._type = EditorComponentFieldType::PUSH_TYPE;
    rayOffsetField._readOnly = false;
    rayOffsetField._range = { -10.f, 10.f };
    rayOffsetField._basicType = GFX::PushConstantType::FLOAT;
    getEditorComponent().registerField(MOV(rayOffsetField));

    EditorComponentField planetRadiusField = {};
    planetRadiusField._name = "Planet Radius [0.1...100]x";
    planetRadiusField._tooltip = "x Times the radius of the Earth (6371e3m)";
    planetRadiusField._data = &_atmosphere._planetRadius;
    planetRadiusField._type = EditorComponentFieldType::PUSH_TYPE;
    planetRadiusField._readOnly = false;
    planetRadiusField._range = { 0.1f, 100.f };
    planetRadiusField._basicType = GFX::PushConstantType::FLOAT;
    getEditorComponent().registerField(MOV(planetRadiusField));

    EditorComponentField atmosphereOffsetField = {};
    atmosphereOffsetField._name = "Atmosphere Offset [1m - 1000m]";
    atmosphereOffsetField._tooltip = "Atmosphere distance in meters starting from the planet radius";
    atmosphereOffsetField._data = &_atmosphere._atmosphereOffset;
    atmosphereOffsetField._type = EditorComponentFieldType::PUSH_TYPE;
    atmosphereOffsetField._readOnly = false;
    atmosphereOffsetField._range = { 1.f, 1000.f };
    atmosphereOffsetField._basicType = GFX::PushConstantType::FLOAT;
    getEditorComponent().registerField(MOV(atmosphereOffsetField));   


    EditorComponentField cloudHeightOffsetField = {};
    cloudHeightOffsetField._name = "Cloud layer height range (m)";
    cloudHeightOffsetField._tooltip = "Cloud layer will be limited to the range [planetRadius + x, planetRadius + y]";
    cloudHeightOffsetField._data = &_atmosphere._cloudLayerMinMaxHeight;
    cloudHeightOffsetField._type = EditorComponentFieldType::PUSH_TYPE;
    cloudHeightOffsetField._readOnly = false;
    cloudHeightOffsetField._range = { 10.f, 5000.f };
    cloudHeightOffsetField._basicType = GFX::PushConstantType::VEC2;
    getEditorComponent().registerField(MOV(cloudHeightOffsetField));

    EditorComponentField rayleighCoeffField = {};
    rayleighCoeffField._name = "Rayleigh Coefficients";
    rayleighCoeffField._tooltip = "[0.01...100] x e-6";
    rayleighCoeffField._data = _atmosphere._RayleighCoeff._v;
    rayleighCoeffField._type = EditorComponentFieldType::PUSH_TYPE;
    rayleighCoeffField._readOnly = false;
    rayleighCoeffField._range = { 0.01f, 100.0f };
    rayleighCoeffField._basicType = GFX::PushConstantType::VEC3;
    getEditorComponent().registerField(MOV(rayleighCoeffField));

    EditorComponentField rayleighScaleField = {};
    rayleighScaleField._name = "Rayleigh Scale Field";
    rayleighScaleField._tooltip = "[-12...12] x 1e3";
    rayleighScaleField._data = &_atmosphere._RayleighScale;
    rayleighScaleField._type = EditorComponentFieldType::PUSH_TYPE;
    rayleighScaleField._range = { 0.01f, 12.f };
    rayleighScaleField._readOnly = true;
    rayleighScaleField._basicType = GFX::PushConstantType::FLOAT;
    getEditorComponent().registerField(MOV(rayleighScaleField));

    EditorComponentField mieCoeffField = {};
    mieCoeffField._name = "Mie Coefficients";
    mieCoeffField._tooltip = "[0.01...100] x e-6";
    mieCoeffField._data = &_atmosphere._MieCoeff;
    mieCoeffField._type = EditorComponentFieldType::PUSH_TYPE;
    mieCoeffField._readOnly = true;
    mieCoeffField._range = { 0.01f, 100.0f };
    mieCoeffField._basicType = GFX::PushConstantType::FLOAT;
    getEditorComponent().registerField(MOV(mieCoeffField));

    EditorComponentField mieScaleField = {};
    mieScaleField._name = "Mie Scale Height";
    mieScaleField._tooltip = "[0.01...10]x 1e3";
    mieScaleField._data = &_atmosphere._MieScaleHeight;
    mieScaleField._type = EditorComponentFieldType::PUSH_TYPE;
    mieScaleField._readOnly = false;
    mieScaleField._range = { 0.01f, 10.0f };
    mieScaleField._basicType = GFX::PushConstantType::FLOAT;
    getEditorComponent().registerField(MOV(mieScaleField));

    getEditorComponent().registerField(MOV(mieScaleField));
    EditorComponentField resetField = {};
    resetField._name = "Reset To Default";
    resetField._tooltip = "Default = whatever value was set at load time";
    resetField._type = EditorComponentFieldType::BUTTON;
    getEditorComponent().registerField(MOV(resetField));

    EditorComponentField separator2Field = {};
    separator2Field._name = "Weather";
    separator2Field._type = EditorComponentFieldType::SEPARATOR;
    getEditorComponent().registerField(MOV(separator2Field));

    EditorComponentField weatherScaleField = {};
    weatherScaleField._name = "Weather Scale ( x 1e-5)";
    weatherScaleField._data = &_weatherScale;
    weatherScaleField._type = EditorComponentFieldType::PUSH_TYPE;
    weatherScaleField._readOnly = false;
    weatherScaleField._range = { 1.f, 100.f };
    weatherScaleField._basicType = GFX::PushConstantType::FLOAT;
    getEditorComponent().registerField(MOV(weatherScaleField));

    EditorComponentField separator3Field = {};
    separator3Field._name = "Skybox";
    separator3Field._type = EditorComponentFieldType::SEPARATOR;
    getEditorComponent().registerField(MOV(separator3Field));

    EditorComponentField useDaySkyboxField = {};
    useDaySkyboxField._name = "Use Day Skybox";
    useDaySkyboxField._data = &_useDaySkybox;
    useDaySkyboxField._type = EditorComponentFieldType::PUSH_TYPE;
    useDaySkyboxField._readOnly = false;
    useDaySkyboxField._basicType = GFX::PushConstantType::BOOL;
    getEditorComponent().registerField(MOV(useDaySkyboxField));

    EditorComponentField useNightSkyboxField = {};
    useNightSkyboxField._name = "Use Night Skybox";
    useNightSkyboxField._data = &_useNightSkybox;
    useNightSkyboxField._type = EditorComponentFieldType::PUSH_TYPE;
    useNightSkyboxField._readOnly = false;
    useNightSkyboxField._basicType = GFX::PushConstantType::BOOL;
    getEditorComponent().registerField(MOV(useNightSkyboxField));

    EditorComponentField useProceduralCloudsField = {};
    useProceduralCloudsField._name = "Enable Procedural Clouds";
    useProceduralCloudsField._data = &_enableProceduralClouds;
    useProceduralCloudsField._type = EditorComponentFieldType::PUSH_TYPE;
    useProceduralCloudsField._readOnly = false;
    useProceduralCloudsField._basicType = GFX::PushConstantType::BOOL;
    getEditorComponent().registerField(MOV(useProceduralCloudsField));

    EditorComponentField nightColourField = {};
    nightColourField._name = "Night Colour";
    nightColourField._data = &_nightSkyColour;
    nightColourField._type = EditorComponentFieldType::PUSH_TYPE;
    nightColourField._readOnly = false;
    nightColourField._basicType = GFX::PushConstantType::FCOLOUR4;
    getEditorComponent().registerField(MOV(nightColourField));

    EditorComponentField moonColourField = {};
    moonColourField._name = "Moon Colour";
    moonColourField._data = &_moonColour;
    moonColourField._type = EditorComponentFieldType::PUSH_TYPE;
    moonColourField._readOnly = false;
    moonColourField._basicType = GFX::PushConstantType::FCOLOUR4;
    getEditorComponent().registerField(MOV(moonColourField));

    EditorComponentField moonScaleField = {};
    moonScaleField._name = "Moon Scale";
    moonScaleField._data = &_moonScale;
    moonScaleField._type = EditorComponentFieldType::PUSH_TYPE;
    moonScaleField._readOnly = false;
    moonScaleField._range = { 0.001f, 0.99f };
    moonScaleField._basicType = GFX::PushConstantType::FLOAT;
    getEditorComponent().registerField(MOV(moonScaleField));
}

bool Sky::load() {
    if (_sky != nullptr) {
        return false;
    }

    std::atomic_uint loadTasks = 0u;
    I32 x, y, n;
    Byte* perlWorlData = (Byte*)stbi_load((procLocation() + perlWorlTexName).c_str(), &x, &y, &n, 0);
    ImageTools::ImageData imgDataPerl = {};
    if (!imgDataPerl.addLayer(perlWorlData, to_size(x * y * n), to_U16(y), to_U16(y), to_U16(x / y))) {
        DIVIDE_UNEXPECTED_CALL();
    }
    stbi_image_free(perlWorlData);

    Byte* worlNoise = (Byte*)stbi_load((procLocation() + worlTexName).c_str(), &x, &y, &n, 0);
    ImageTools::ImageData imgDataWorl = {};
    if (!imgDataWorl.addLayer(worlNoise, to_size(x * y * n), to_U16(y), to_U16(y), to_U16(x / y))) {
        DIVIDE_UNEXPECTED_CALL();
    }
    stbi_image_free(worlNoise);
    {
        SamplerDescriptor skyboxSampler = {};
        skyboxSampler.wrapUVW(TextureWrap::CLAMP_TO_EDGE);
        skyboxSampler.minFilter(TextureFilter::LINEAR);
        skyboxSampler.magFilter(TextureFilter::LINEAR);
        skyboxSampler.anisotropyLevel(0);
        _skyboxSampler = skyboxSampler.getHash();

        skyboxSampler.wrapUVW(TextureWrap::REPEAT);
        _noiseSamplerLinear = skyboxSampler.getHash();

        skyboxSampler.minFilter(TextureFilter::LINEAR_MIPMAP_LINEAR);
        skyboxSampler.anisotropyLevel(8);
        _noiseSamplerMipMap = skyboxSampler.getHash();
    }
    {
        TextureDescriptor textureDescriptor(TextureType::TEXTURE_3D);
        textureDescriptor.srgb(false);
        textureDescriptor.layerCount(128);
        textureDescriptor.baseFormat(GFXImageFormat::RGBA);

        ResourceDescriptor perlWorlDescriptor("perlWorl");
        perlWorlDescriptor.propertyDescriptor(textureDescriptor);
        perlWorlDescriptor.waitForReady(true);
        perlWorlDescriptor.threaded(false);
        _perWorlNoiseTex = CreateResource<Texture>(_parentCache, perlWorlDescriptor);
        _perWorlNoiseTex->loadData(imgDataPerl);
 
        textureDescriptor.baseFormat(GFXImageFormat::RGB);
        textureDescriptor.layerCount(32);

        ResourceDescriptor worlDescriptor("worlNoise");
        worlDescriptor.propertyDescriptor(textureDescriptor);
        worlDescriptor.waitForReady(true);
        worlDescriptor.threaded(false);
        _worlNoiseTex = CreateResource<Texture>(_parentCache, worlDescriptor);
        _worlNoiseTex->loadData(imgDataWorl);

        textureDescriptor.texType(TextureType::TEXTURE_2D);
        textureDescriptor.layerCount(1u);
        textureDescriptor.mipCount(1u);
        
        ResourceDescriptor weatherDescriptor("Weather");
        weatherDescriptor.assetName(ResourcePath(weatherTexName));
        weatherDescriptor.assetLocation(procLocation());
        weatherDescriptor.propertyDescriptor(textureDescriptor);
        weatherDescriptor.waitForReady(false);
        _weatherTex = CreateResource<Texture>(_parentCache, weatherDescriptor);

        ResourceDescriptor curlDescriptor("CurlNoise");
        curlDescriptor.assetName(ResourcePath(curlTexName));
        curlDescriptor.assetLocation(procLocation());
        curlDescriptor.propertyDescriptor(textureDescriptor);
        curlDescriptor.waitForReady(false);
        _curlNoiseTex = CreateResource<Texture>(_parentCache, curlDescriptor);
    }
    {
        TextureDescriptor skyboxTexture(TextureType::TEXTURE_CUBE_ARRAY);
        skyboxTexture.srgb(true);
    
        ResourceDescriptor skyboxTextures("SkyTextures");
        skyboxTextures.assetName(ResourcePath{ 
            "skyboxDay_FRONT.jpg, skyboxDay_BACK.jpg, skyboxDay_UP.jpg, skyboxDay_DOWN.jpg, skyboxDay_LEFT.jpg, skyboxDay_RIGHT.jpg," //Day
            "Milkyway_posx.jpg, Milkyway_negx.jpg, Milkyway_posy.jpg, Milkyway_negy.jpg, Milkyway_posz.jpg, Milkyway_negz.jpg"  //Night
        });
        skyboxTextures.assetLocation(Paths::g_assetsLocation + Paths::g_imagesLocation + "/SkyBoxes/");
        skyboxTextures.propertyDescriptor(skyboxTexture);
        skyboxTextures.waitForReady(false);
        _skybox = CreateResource<Texture>(_parentCache, skyboxTextures, loadTasks);
    }

    const F32 radius = _diameter * 0.5f;

    ResourceDescriptor skybox("SkyBox");
    skybox.flag(true);  // no default material;
    skybox.ID(4); // resolution
    skybox.enumValue(to_U32(radius)); // radius
    _sky = CreateResource<Sphere3D>(_parentCache, skybox);
    _sky->renderState().drawState(false);

    const bool isEnabledSky = _context.context().config().debug.renderFilter.sky;

    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "sky.glsl";
    vertModule._defines.emplace_back("HAS_CLIPPING_OUT", true);

    ShaderModuleDescriptor fragModule = {};
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "sky.glsl";

    ShaderProgramDescriptor shaderDescriptor = {};
    shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.back()._variant = isEnabledSky ? "Clouds" : "NoClouds";
    shaderDescriptor._modules.push_back(fragModule);
    shaderDescriptor._modules.back()._variant = isEnabledSky ? "Clouds" : "PassThrough";
    
    ResourceDescriptor skyShaderDescriptorLQ("sky_Display_Clouds_LQ");
    skyShaderDescriptorLQ.propertyDescriptor(shaderDescriptor);
    skyShaderDescriptorLQ.waitForReady(false);
    _skyShaderLQ = CreateResource<ShaderProgram>(_parentCache, skyShaderDescriptorLQ, loadTasks);

    shaderDescriptor._modules.back()._defines.emplace_back("MAIN_DISPLAY_PASS", true);

    ResourceDescriptor skyShaderDescriptor("sky_Display_Clouds");
    skyShaderDescriptor.propertyDescriptor(shaderDescriptor);
    skyShaderDescriptor.waitForReady(false);
    _skyShader = CreateResource<ShaderProgram>(_parentCache, skyShaderDescriptor, loadTasks);

    shaderDescriptor = {};
    vertModule._variant = "NoClouds";
    shaderDescriptor._modules.push_back(vertModule);

    ResourceDescriptor skyShaderPrePassDescriptor("sky_PrePass");
    skyShaderPrePassDescriptor.waitForReady(false);
    skyShaderPrePassDescriptor.propertyDescriptor(shaderDescriptor);
    _skyShaderPrePass = CreateResource<ShaderProgram>(_parentCache, skyShaderPrePassDescriptor, loadTasks);

    WAIT_FOR_CONDITION(loadTasks.load() == 0u);
    
    assert(_skyShader && _skyShaderPrePass);
    setBounds(BoundingBox(vec3<F32>(-radius), vec3<F32>(radius)));

    Console::printfn(Locale::Get(_ID("CREATE_SKY_RES_OK")));

    return SceneNode::load();
}

void Sky::postLoad(SceneGraphNode* sgn) {
    assert(_sky != nullptr);

    SceneGraphNodeDescriptor skyNodeDescriptor;
    skyNodeDescriptor._serialize = false;
    skyNodeDescriptor._node = _sky;
    skyNodeDescriptor._name = sgn->name() + "_geometry";
    skyNodeDescriptor._usageContext = NodeUsageContext::NODE_STATIC;
    skyNodeDescriptor._componentMask = to_base(ComponentType::TRANSFORM) |
                                       to_base(ComponentType::BOUNDS) |
                                       to_base(ComponentType::RENDERING) |
                                       to_base(ComponentType::NAVIGATION);
    sgn->addChildNode(skyNodeDescriptor);

    RenderingComponent* renderable = sgn->get<RenderingComponent>();
    if (renderable) {
        renderable->lockLoD(0u);
        renderable->toggleRenderOption(RenderingComponent::RenderOptions::CAST_SHADOWS, false);
    }

    _defaultAtmosphere = atmosphere();

    SceneNode::postLoad(sgn);
}

SunDetails Sky::setDateTime(struct tm *dateTime) {
    _sun.SetDate(dateTime);
    return getCurrentDetails();
}

SunDetails Sky::getCurrentDetails() const noexcept {
    return _sun.GetDetails();
}

bool Sky::isDay() const noexcept {
    return getCurrentDetails()._intensity > 0.f;
}

SimpleTime Sky::GetTimeOfDay() const noexcept {
    return _sun.GetTimeOfDay();
}

void Sky::setAtmosphere(const Atmosphere& atmosphere) noexcept {
    _atmosphere = atmosphere;
    _atmosphereChanged = EditorDataState::QUEUED;
}

const Texture_ptr& Sky::activeSkyBox() const noexcept {
    return _skybox;
}

void Sky::sceneUpdate(const U64 deltaTimeUS, SceneGraphNode* sgn, SceneState& sceneState) {
    if (_atmosphereChanged == EditorDataState::QUEUED) {
        _atmosphereChanged = EditorDataState::CHANGED;
        if (_atmosphere._cloudLayerMinMaxHeight < 1.f) {
            _atmosphere._cloudLayerMinMaxHeight = 1.f;
        }
        if (_atmosphere._cloudLayerMinMaxHeight.min > _atmosphere._cloudLayerMinMaxHeight.max) {
            std::swap(_atmosphere._cloudLayerMinMaxHeight.min, _atmosphere._cloudLayerMinMaxHeight.max);
        }
    } else if (_atmosphereChanged == EditorDataState::PROCESSED) {
        _atmosphereChanged = EditorDataState::IDLE;
    }

    SceneNode::sceneUpdate(deltaTimeUS, sgn, sceneState);
}

void Sky::setSkyShaderData(const U32 rayCount, PushConstants& constantsInOut) {
    constantsInOut.set(_ID("dvd_nightSkyColour"), GFX::PushConstantType::FCOLOUR3, nightSkyColour().rgb);
    constantsInOut.set(_ID("dvd_moonColour"), GFX::PushConstantType::FCOLOUR3, moonColour().rgb);
    if (_atmosphereChanged == EditorDataState::CHANGED || _atmosphereChanged == EditorDataState::PROCESSED) {
        constantsInOut.set(_ID("dvd_useSkyboxes"), GFX::PushConstantType::IVEC2, vec2<I32>(useDaySkybox() ? 1 : 0, useNightSkybox() ? 1 : 0));
        constantsInOut.set(_ID("dvd_raySteps"), GFX::PushConstantType::UINT, rayCount);
        constantsInOut.set(_ID("dvd_RayleighCoeff"), GFX::PushConstantType::VEC3, _atmosphere._RayleighCoeff * 1e-6f);
        constantsInOut.set(_ID("dvd_moonScale"), GFX::PushConstantType::FLOAT, moonScale());
        constantsInOut.set(_ID("dvd_weatherScale"), GFX::PushConstantType::FLOAT, weatherScale() * 1e-5f);
        constantsInOut.set(_ID("dvd_sunIntensity"), GFX::PushConstantType::FLOAT, _atmosphere._sunIntensity);
        constantsInOut.set(_ID("dvd_sunScale"), GFX::PushConstantType::FLOAT, _atmosphere._sunScale);
        constantsInOut.set(_ID("dvd_planetRadius"), GFX::PushConstantType::FLOAT, _atmosphere._planetRadius * 6371e3f);
        constantsInOut.set(_ID("dvd_atmosphereOffset"), GFX::PushConstantType::FLOAT, _atmosphere._atmosphereOffset * _atmosphere._distanceMultiplier);
        constantsInOut.set(_ID("dvd_cloudLayerMinMaxHeight"), GFX::PushConstantType::VEC2, _atmosphere._cloudLayerMinMaxHeight);
        constantsInOut.set(_ID("dvd_MieCoeff"), GFX::PushConstantType::FLOAT, _atmosphere._MieCoeff * 1e-6f);
        constantsInOut.set(_ID("dvd_RayleighScale"), GFX::PushConstantType::FLOAT, _atmosphere._RayleighScale * 1e3f);
        constantsInOut.set(_ID("dvd_MieScaleHeight"), GFX::PushConstantType::FLOAT, _atmosphere._MieScaleHeight * 1e3f);
        constantsInOut.set(_ID("dvd_rayOriginOffset"), GFX::PushConstantType::FLOAT, _atmosphere._rayOriginDistance * _atmosphere._distanceMultiplier);
        constantsInOut.set(_ID("dvd_enableClouds"), GFX::PushConstantType::BOOL, enableProceduralClouds());
        _atmosphereChanged = EditorDataState::PROCESSED;
    }
}

void Sky::prepareRender(SceneGraphNode* sgn,
                RenderingComponent& rComp,
                const RenderStagePass& renderStagePass,
                const Camera& camera,
                const bool refreshData)  {

    RenderPackage& pkg = rComp.getDrawPackage(renderStagePass);
    if (!pkg.empty()) {
        setSkyShaderData(renderStagePass._stage == RenderStage::DISPLAY ? 16 : 8,
                         pkg.get<GFX::SendPushConstantsCommand>(0)->_constants);
    }

    SceneNode::prepareRender(sgn, rComp, renderStagePass, camera, refreshData);
}

void Sky::buildDrawCommands(SceneGraphNode* sgn,
                            const RenderStagePass& renderStagePass,
                            const Camera& crtCamera,
                            RenderPackage& pkgInOut) {
    assert(renderStagePass._stage != RenderStage::SHADOW);

    PipelineDescriptor pipelineDescriptor = {};
    if (renderStagePass._passType == RenderPassType::PRE_PASS) {
        WAIT_FOR_CONDITION(_skyShaderPrePass->getState() == ResourceState::RES_LOADED);
        pipelineDescriptor._stateHash = renderStagePass._stage == RenderStage::REFLECTION && renderStagePass._variant != to_U8(ReflectorType::CUBE)
                                            ? _skyboxRenderStateReflectedHashPrePass
                                            : _skyboxRenderStateHashPrePass;
        pipelineDescriptor._shaderProgramHandle = _skyShaderPrePass->getGUID();
    } else {
        WAIT_FOR_CONDITION(_skyShader->getState() == ResourceState::RES_LOADED);
        pipelineDescriptor._stateHash = renderStagePass._stage == RenderStage::REFLECTION && renderStagePass._variant != to_U8(ReflectorType::CUBE)
                                            ? _skyboxRenderStateReflectedHash
                                            : _skyboxRenderStateHash;
        pipelineDescriptor._shaderProgramHandle = renderStagePass._stage == RenderStage::DISPLAY ? _skyShader->getGUID() : _skyShaderLQ->getGUID();
    }

    GFX::BindPipelineCommand pipelineCommand = {};
    pipelineCommand._pipeline = _context.newPipeline(pipelineDescriptor);
    pkgInOut.add(pipelineCommand);

    GFX::BindDescriptorSetsCommand bindDescriptorSetsCommand = {};
    bindDescriptorSetsCommand._set._textureData.add({ _skybox->data(), _skyboxSampler, TextureUsage::UNIT0 });
    bindDescriptorSetsCommand._set._textureData.add({ _weatherTex->data(), _noiseSamplerLinear, TextureUsage::HEIGHTMAP });
    bindDescriptorSetsCommand._set._textureData.add({ _curlNoiseTex->data(), _noiseSamplerLinear, TextureUsage::OPACITY });
    bindDescriptorSetsCommand._set._textureData.add({ _worlNoiseTex->data(), _noiseSamplerMipMap, TextureUsage::OCCLUSION_METALLIC_ROUGHNESS });
    bindDescriptorSetsCommand._set._textureData.add({ _perWorlNoiseTex->data(), _noiseSamplerMipMap, TextureUsage::NORMALMAP });

    pkgInOut.add(bindDescriptorSetsCommand);

    _atmosphereChanged = EditorDataState::CHANGED;
    GFX::SendPushConstantsCommand pushConstantsCommand = {};
    setSkyShaderData(renderStagePass._stage == RenderStage::DISPLAY ? 16 : 8, pushConstantsCommand._constants);
    pkgInOut.add(pushConstantsCommand);

    GenericDrawCommand cmd = {};
    cmd._primitiveType = PrimitiveType::TRIANGLE_STRIP;
    cmd._sourceBuffer = _sky->getGeometryVB()->handle();
    cmd._bufferIndex = renderStagePass.baseIndex();
    cmd._cmd.indexCount = to_U32(_sky->getGeometryVB()->getIndexCount());

    pkgInOut.add(GFX::DrawCommand{ cmd });

    SceneNode::buildDrawCommands(sgn, renderStagePass, crtCamera, pkgInOut);
}

}
