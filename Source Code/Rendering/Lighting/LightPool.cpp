#include "stdafx.h"

#include "Headers/LightPool.h"

#include "Core/Headers/PlatformContext.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Core/Time/Headers/ProfileTimer.h"
#include "Managers/Headers/SceneManager.h"
#include "Platform/Video/Buffers/ShaderBuffer/Headers/ShaderBuffer.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"
#include "Platform/Video/Textures/Headers/Texture.h"
#include "Rendering/Lighting/ShadowMapping/Headers/ShadowMap.h"

#include "ECS/Components/Headers/SpotLightComponent.h"

#include <execution>

namespace Divide {

std::array<TextureUsage, to_base(ShadowType::COUNT)> LightPool::_shadowLocation = { {
    TextureUsage::SHADOW_SINGLE,
    TextureUsage::SHADOW_LAYERED,
    TextureUsage::SHADOW_CUBE
}};

namespace {
    constexpr bool g_GroupSortedLightsByType = true;

    LightPool::LightList g_sortedLightsContainer = {};
    const auto MaxLights = [](const LightType type) noexcept -> U32 {
        switch (type) {
            case LightType::DIRECTIONAL: return Config::Lighting::MAX_SHADOW_CASTING_DIRECTIONAL_LIGHTS;
            case LightType::POINT: return Config::Lighting::MAX_SHADOW_CASTING_POINT_LIGHTS;
            case LightType::SPOT: return Config::Lighting::MAX_SHADOW_CASTING_SPOT_LIGHTS;
            case LightType::COUNT: break;
        }
        return 0u;
    };
}

LightPool::LightPool(Scene& parentScene, PlatformContext& context)
    : SceneComponent(parentScene),
      PlatformContextComponent(context),
     // shadowPassTimer is used to measure the CPU-duration of shadow map generation step
     _activeLightCount{},
     _lightTypeState{},
     _shadowPassTimer(Time::ADD_TIMER("Shadow Pass Timer"))
{

    for (U8 i = 0; i < to_U8(RenderStage::COUNT); ++i) {
        _activeLightCount[i].fill(0);
        _sortedLights[i].reserve(Config::Lighting::MAX_POSSIBLE_LIGHTS);
    }

    _lightTypeState.fill(true);

    init();
}

LightPool::~LightPool()
{
    SharedLock<SharedMutex> r_lock(_lightLock);
    for (const LightList& lightList : _lights) {
        if (!lightList.empty()) {
            Console::errorfn(Locale::get(_ID("ERROR_LIGHT_POOL_LIGHT_LEAKED")));
        }
    }
}

void LightPool::init() {
    if (_init) {
        return;
    }

    ShaderBufferDescriptor bufferDescriptor = {};
    bufferDescriptor._usage = ShaderBuffer::Usage::UNBOUND_BUFFER;
    bufferDescriptor._ringBufferLength = 6;
    bufferDescriptor._separateReadWrite = false;
    bufferDescriptor._flags = to_U32(ShaderBuffer::Flags::ALLOW_THREADED_WRITES) |
                              to_U32(ShaderBuffer::Flags::AUTO_RANGE_FLUSH);

    bufferDescriptor._updateFrequency = BufferUpdateFrequency::OCASSIONAL;
    bufferDescriptor._updateUsage = BufferUpdateUsage::CPU_W_GPU_R;
    
    {
        bufferDescriptor._name = "LIGHT_BUFFER";
        bufferDescriptor._elementCount = to_base(RenderStage::COUNT) - 1; ///< no shadows
        bufferDescriptor._elementSize = sizeof(BufferData);
        // Holds general info about the currently active lights: position, colour, etc.
        _lightShaderBuffer = _context.gfx().newSB(bufferDescriptor);
    }
    {
        // Holds info about the currently active shadow casting lights:
        // ViewProjection Matrices, View Space Position, etc
        bufferDescriptor._name = "LIGHT_SHADOW_BUFFER";
        bufferDescriptor._elementCount = 1;
        bufferDescriptor._elementSize = sizeof(ShadowProperties);
        _shadowBuffer = _context.gfx().newSB(bufferDescriptor);
    }

    ShaderModuleDescriptor vertModule = {};
    vertModule._moduleType = ShaderType::VERTEX;
    vertModule._sourceFile = "lightImpostorShader.glsl";

    ShaderModuleDescriptor geomModule = {};
    geomModule._moduleType = ShaderType::GEOMETRY;
    geomModule._sourceFile = "lightImpostorShader.glsl";

    ShaderModuleDescriptor fragModule = {};
    fragModule._moduleType = ShaderType::FRAGMENT;
    fragModule._sourceFile = "lightImpostorShader.glsl";

    ShaderProgramDescriptor shaderDescriptor = {};
    shaderDescriptor._modules.push_back(vertModule);
    shaderDescriptor._modules.push_back(geomModule);
    shaderDescriptor._modules.push_back(fragModule);

    std::atomic_uint loadingTasks = 0u;
    ResourceDescriptor lightImpostorShader("lightImpostorShader");
    lightImpostorShader.propertyDescriptor(shaderDescriptor);
    lightImpostorShader.waitForReady(false);
    _lightImpostorShader = CreateResource<ShaderProgram>(_parentScene.resourceCache(), lightImpostorShader, loadingTasks);

    TextureDescriptor iconDescriptor(TextureType::TEXTURE_2D);
    iconDescriptor.srgb(true);

    ResourceDescriptor iconImage("LightIconTexture");
    iconImage.assetLocation(Paths::g_assetsLocation + Paths::g_imagesLocation);
    iconImage.assetName(ResourcePath("lightIcons.png"));
    iconImage.propertyDescriptor(iconDescriptor);
    iconImage.waitForReady(false);

    _lightIconsTexture = CreateResource<Texture>(_parentScene.resourceCache(), iconImage, loadingTasks);

    WAIT_FOR_CONDITION(loadingTasks.load() == 0u);

    _init = true;
}

bool LightPool::clear() {
    if (!_init) {
        return true;
    }
    g_sortedLightsContainer.clear();

    return _lights.empty();
}

bool LightPool::addLight(Light& light) {
    const LightType type = light.getLightType();
    const U32 lightTypeIdx = to_base(type);

    UniqueLock<SharedMutex> r_lock(_lightLock);
    if (findLightLocked(light.getGUID(), type) != end(_lights[lightTypeIdx])) {

        Console::errorfn(Locale::get(_ID("ERROR_LIGHT_POOL_DUPLICATE")),
                         light.getGUID());
        return false;
    }

    _lights[lightTypeIdx].emplace_back(&light);

    return true;
}

// try to remove any leftover lights
bool LightPool::removeLight(Light& light) {
    UniqueLock<SharedMutex> lock(_lightLock);
    const LightList::const_iterator it = findLightLocked(light.getGUID(), light.getLightType());

    if (it == end(_lights[to_U32(light.getLightType())])) {
        Console::errorfn(Locale::get(_ID("ERROR_LIGHT_POOL_REMOVE_LIGHT")),
                         light.getGUID());
        return false;
    }

    _lights[to_U32(light.getLightType())].erase(it);  // remove it from the map
    return true;
}

void LightPool::idle() {
}

void LightPool::generateShadowMaps(const Camera& playerCamera, GFX::CommandBuffer& bufferInOut) {
    Time::ScopedTimer timer(_shadowPassTimer);

    ShadowMap::clearShadowMapBuffers(bufferInOut);

    std::array<U32, to_base(LightType::COUNT)> indexCounter{};

    bool dirty = false;
    for (U16 i = 0; i < _sortedShadowLights._count; ++i) {
        Light* light = _sortedShadowLights._entries[i];

        const LightType lType = light->getLightType();
        const U32 lTypeIndex = to_U32(lType);
        if (indexCounter[lTypeIndex] == MaxLights(lType)) {
            continue;
        }
        dirty = true;

        ShadowMap::generateShadowMaps(playerCamera, *light, bufferInOut);
        const Light::ShadowProperties& propsSource = light->getShadowProperties();

        const U32 shadowIndex = indexCounter[lTypeIndex]++;
        switch (lType) {
            case LightType::POINT: {
                PointShadowProperties& propsTarget = _shadowBufferData._pointLights[shadowIndex];
                propsTarget._details = propsSource._lightDetails;
                propsTarget._position = propsSource._lightPosition[0];
            }break;
            case LightType::SPOT: {
                SpotShadowProperties& propsTarget = _shadowBufferData._spotLights[shadowIndex];
                propsTarget._details = propsSource._lightDetails;
                propsTarget._vpMatrix = propsSource._lightVP[0];
                propsTarget._position = propsSource._lightPosition[0];
            }break;
            case LightType::DIRECTIONAL: {
                CSMShadowProperties& propsTarget = _shadowBufferData._dirLights[shadowIndex];
                propsTarget._details = propsSource._lightDetails;
                std::memcpy(propsTarget._position.data(), propsSource._lightPosition.data(), sizeof(vec4<F32>) * Config::Lighting::MAX_CSM_SPLITS_PER_LIGHT);
                std::memcpy(propsTarget._vpMatrix.data(), propsSource._lightVP.data(), sizeof(mat4<F32>) * Config::Lighting::MAX_CSM_SPLITS_PER_LIGHT);
            }break;
            case LightType::COUNT: break;
        }
    }

    if (dirty) {
        _shadowBuffer->writeData(_shadowBufferData.data());
    }

    ShadowMap::bindShadowMaps(bufferInOut);
}

void LightPool::debugLight(Light* light) {
    _debugLight = light;
    ShadowMap::setDebugViewLight(context().gfx(), _debugLight);
}

Light* LightPool::getLight(const I64 lightGUID, const LightType type) const {
    SharedLock<SharedMutex> r_lock(_lightLock);

    const LightList::const_iterator it = findLight(lightGUID, type);
    assert(it != eastl::end(_lights[to_U32(type)]));

    return *it;
}

U32 LightPool::uploadLightList(const RenderStage stage, const LightList& lights, const mat4<F32>& viewMatrix) {
    const U8 stageIndex = to_U8(stage);
    U32 ret = 0u;

    auto& lightCount = _activeLightCount[stageIndex];
    BufferData& crtData = _sortedLightProperties[stageIndex];

    SpotLightComponent* spot = nullptr;

    lightCount.fill(0);
    vec3<F32> tempColour;
    for (Light* light : lights) {
        const LightType type = light->getLightType();
        const U32 typeIndex = to_U32(type);

        if (_lightTypeState[typeIndex] && light->enabled()) {
            if (++ret > Config::Lighting::MAX_POSSIBLE_LIGHTS) {
                break;
            }

            const bool isDir = type == LightType::DIRECTIONAL;
            const bool isOmni = type == LightType::POINT;
            const bool isSpot = type == LightType::SPOT;
            if (isSpot) {
                spot = static_cast<SpotLightComponent*>(light);
            }

            LightProperties& temp = crtData._lightProperties[ret - 1];
            light->getDiffuseColour(tempColour);
            temp._diffuse.set(tempColour * light->intensity(), isSpot ? std::cos(Angle::DegreesToRadians(spot->outerConeCutoffAngle())) : 0.f);
            // Omni and spot lights have a position. Directional lights have this set to (0,0,0)
            temp._position.set( isDir  ? VECTOR3_ZERO : (viewMatrix * vec4<F32>(light->positionCache(),  1.0f)).xyz, light->range());
            temp._direction.set(isOmni ? VECTOR3_ZERO : (viewMatrix * vec4<F32>(light->directionCache(), 0.0f)).xyz, isSpot ? std::cos(Angle::DegreesToRadians(spot->coneCutoffAngle())) : 0.f);
            temp._options.xyz = {typeIndex, light->shadowIndex(), isSpot ? to_I32(spot->coneSlantHeight()) : 0};

            ++lightCount[typeIndex];
        }
    }

    return ret;
}

// This should be called in a separate thread for each RenderStage
void LightPool::prepareLightData(const RenderStage stage, const vec3<F32>& eyePos, const mat4<F32>& viewMatrix) {
    const U8 stageIndex = to_U8(stage);

    LightList& sortedLights = _sortedLights[stageIndex];
    sortedLights.resize(0);
    {
        SharedLock<SharedMutex> r_lock(_lightLock);
        size_t totalLightCount = 0;
        for (U8 i = 0; i < to_base(LightType::COUNT); ++i) {
            totalLightCount += _lights[i].size();
        }
        sortedLights.reserve(totalLightCount);

        for (U8 i = 1; i < to_base(LightType::COUNT); ++i) {
            sortedLights.insert(cend(sortedLights), cbegin(_lights[i]), cend(_lights[i]));
        }
    }
    {
        OPTICK_EVENT("LightPool::SortLights");
        eastl::sort(begin(sortedLights),
                    end(sortedLights),
                    [&eyePos](Light* a, Light* b) noexcept {
                        if_constexpr(g_GroupSortedLightsByType) {
                            return  a->getLightType() < b->getLightType() ||
                                a->getLightType() == b->getLightType() && a->distanceSquared(eyePos) < b->distanceSquared(eyePos);
                        } else {
                            return a->distanceSquared(eyePos) < b->distanceSquared(eyePos);
                        }
                    });
    }
    {
        SharedLock<SharedMutex> r_lock(_lightLock);
        const LightList& dirLights = _lights[to_base(LightType::DIRECTIONAL)];
        sortedLights.insert(begin(sortedLights), cbegin(dirLights), cend(dirLights));
    }

    const U32 totalLightCount = uploadLightList(stage, sortedLights, viewMatrix);

    BufferData& crtData = _sortedLightProperties[stageIndex];
    crtData._globalData.set(
        _activeLightCount[stageIndex][to_base(LightType::DIRECTIONAL)],
        _activeLightCount[stageIndex][to_base(LightType::POINT)],
        _activeLightCount[stageIndex][to_base(LightType::SPOT)],
        to_U32(_sortedShadowLights._count));

    if (!sortedLights.empty()) {
        crtData._ambientColour.rgb = { 0.05f * sortedLights.front()->getDiffuseColour() };
    } else {
        crtData._ambientColour = DefaultColours::BLACK;
    }

    {
        OPTICK_EVENT("LightPool::UploadLightDataToGPU");

        _lightShaderBuffer->writeBytes((stageIndex - 1) * _lightShaderBuffer->getPrimitiveSize(),
                                       sizeof(vec4<U32>) + sizeof(vec4<F32>) + totalLightCount * sizeof(LightProperties),
                                       (bufferPtr)&crtData);
    }
}

void LightPool::uploadLightData(const RenderStage stage, GFX::CommandBuffer& bufferInOut) {

    ShaderBufferBinding bufferLight;
    bufferLight._binding = ShaderBufferLocation::LIGHT_NORMAL;
    bufferLight._buffer = _lightShaderBuffer;
    bufferLight._elementRange = { to_base(stage) - 1, 1 };

    ShaderBufferBinding bufferShadow;
    bufferShadow._binding = ShaderBufferLocation::LIGHT_SHADOW;
    bufferShadow._buffer = _shadowBuffer;
    bufferShadow._elementRange = { 0u, Config::Lighting::MAX_SHADOW_PASSES };

    GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
    descriptorSetCmd._set.addShaderBuffer(bufferLight);
    descriptorSetCmd._set.addShaderBuffer(bufferShadow);
    EnqueueCommand(bufferInOut, descriptorSetCmd);
}

void LightPool::preRenderAllPasses(const Camera* playerCamera) {
    constexpr bool USE_PARALLEL_SORT = false;

    OPTICK_EVENT();

    const vec3<F32>& eyePos = playerCamera->getEye();
    const Frustum& camFrustum = playerCamera->getFrustum();

    _sortedShadowLights._count = 0;

    g_sortedLightsContainer.resize(0);
    {
        SharedLock<SharedMutex> r_lock(_lightLock);
        g_sortedLightsContainer.reserve([&]() {
            size_t ret = 0;
            for (U8 i = 0; i < to_base(LightType::COUNT); ++i) {
                ret += _lights[i].size();
            }
            return ret;
        }());

        for (U8 i = 0; i < to_base(LightType::COUNT); ++i) {
            g_sortedLightsContainer.insert(cend(g_sortedLightsContainer), cbegin(_lights[i]), cend(_lights[i]));
        }
    }

    const auto sortPredicate = [&eyePos](Light* a, Light* b) noexcept {
        // directional lights first
        if (a->getLightType() != b->getLightType()) {
            return to_base(a->getLightType()) < to_base(b->getLightType());
        }
        return a->positionCache().distanceSquared(eyePos) < b->positionCache().distanceSquared(eyePos);
    };

    if_constexpr(USE_PARALLEL_SORT) {
        std::sort(std::execution::par, begin(g_sortedLightsContainer), end(g_sortedLightsContainer), sortPredicate);
    } else {
        eastl::sort(begin(g_sortedLightsContainer), end(g_sortedLightsContainer), sortPredicate);
    }

    const auto isInViewFrustum = [](const Frustum& frustum, Light* light) {
        const LightType lType = light->getLightType();

        if (lType != LightType::DIRECTIONAL) {
            vec3<F32> position = light->positionCache();
            F32 radius = 0.f;

            if (lType == LightType::POINT) {
                radius = light->range();
            } else if (lType == LightType::SPOT) {
                const F32 range = light->range();
                const Angle::RADIANS<F32> angle = Angle::DegreesToRadians(static_cast<SpotLightComponent*>(light)->outerConeCutoffAngle());
                radius = angle > M_PI_4 ? range * tan(angle) : range * 0.5f / pow(cos(angle), 2.0f);
                position += light->directionCache() * radius;
            } else {
                DIVIDE_UNEXPECTED_CALL();
            }

            return frustum.ContainsSphere(position, radius) != FrustumCollision::FRUSTUM_OUT;
        }

        return true;
    };

    std::array<U32, to_base(LightType::COUNT)> indexCounter = {};
    indexCounter.fill(0u);
    for (Light* light : g_sortedLightsContainer) {
        light->shadowIndex(-1);
        if (light->enabled() && light->castsShadows() && _lightTypeState[to_base(light->getLightType())]) {
            if (_sortedShadowLights._count < Config::Lighting::MAX_SHADOW_CASTING_LIGHTS) {
                const LightType lType = light->getLightType();
                const U32 lTypeIndex = to_U32(lType);
                if (indexCounter[lTypeIndex] == MaxLights(lType) || !isInViewFrustum(camFrustum, light)) {
                    continue;
                }

                light->shadowIndex(to_I32(indexCounter[lTypeIndex]++));
                _sortedShadowLights._entries[_sortedShadowLights._count++] = light;
            }
        }
    }
}

void LightPool::postRenderAllPasses(const Camera* playerCamera) {
    ACKNOWLEDGE_UNUSED(playerCamera);

    // Move backwards so that we don't step on our own toes with the lockmanager
    _lightShaderBuffer->decQueue();
    _shadowBuffer->decQueue();
}

void LightPool::drawLightImpostors(RenderStage stage, GFX::CommandBuffer& bufferInOut) const {
    if (!lightImpostorsEnabled()) {
        return;
    }

    static size_t s_samplerHash = 0;
    if (s_samplerHash == 0) {
        SamplerDescriptor iconSampler = {};
        iconSampler.wrapUVW(TextureWrap::REPEAT);
        iconSampler.minFilter(TextureFilter::LINEAR);
        iconSampler.magFilter(TextureFilter::LINEAR);
        iconSampler.anisotropyLevel(0);
        s_samplerHash = iconSampler.getHash();
    }

    const U8 stageIndex = to_U8(stage);

    assert(_lightImpostorShader);

    U32 totalLightCount = 0;
    for (U8 i = 0; i < to_base(LightType::COUNT); ++i) {
        totalLightCount += _activeLightCount[stageIndex][i];
    }

    if (totalLightCount > 0) {
        PipelineDescriptor pipelineDescriptor;
        pipelineDescriptor._stateHash = _context.gfx().getDefaultStateBlock(false);
        pipelineDescriptor._shaderProgramHandle = _lightImpostorShader->getGUID();

        GenericDrawCommand pointsCmd;
        pointsCmd._primitiveType = PrimitiveType::API_POINTS;
        pointsCmd._drawCount = to_U16(totalLightCount);

        GFX::BindPipelineCommand bindPipeline;
        bindPipeline._pipeline = _context.gfx().newPipeline(pipelineDescriptor);
        EnqueueCommand(bufferInOut, bindPipeline);
        
        GFX::BindDescriptorSetsCommand descriptorSetCmd;
        descriptorSetCmd._set._textureData.setTexture(_lightIconsTexture->data(), s_samplerHash, TextureUsage::UNIT0);
        EnqueueCommand(bufferInOut, descriptorSetCmd);

        GFX::DrawCommand drawCommand = { pointsCmd };
        EnqueueCommand(bufferInOut, drawCommand);
    }
}

}
