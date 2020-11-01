/*
   Copyright (c) 2018 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software
   and associated documentation files (the "Software"), to deal in the Software
   without restriction,
   including without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
   PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#pragma once
#ifndef _LIGHT_POOL_H_
#define _LIGHT_POOL_H_

#include "config.h"

#include "Light.h"

#include "Scenes/Headers/SceneComponent.h"
#include "Core/Headers/PlatformContextComponent.h"

namespace Divide {

namespace Time {
    class ProfileTimer;
};

class ShaderBuffer;
class SceneGraphNode;
class SceneRenderState;

class LightPool : public SceneComponent,
                  public PlatformContextComponent {
  protected:
      struct LightProperties {
          /// rgb = diffuse
          /// w = cosOuterConeAngle;
          FColour4 _diffuse = { DefaultColours::WHITE.rgb, 0.0f };
          /// light position ((0,0,0) for directional lights)
          /// w = range
          vec4<F32> _position = { 0.0f, 0.0f, 0.0f, 0.0f };
          /// xyz = light direction (spot and directional lights only. (0,0,0) for point)
          /// w = spot angle
          vec4<F32> _direction = { 0.0f, 0.0f, 0.0f, 45.0f };
          /// x = light type: 0 - directional, 1 - point, 2 - spot, 3 - none
          /// y = shadow index (-1 = no shadows)
          /// z = reserved
          /// w = reserved
          vec4<I32> _options = { 3, 1, 0, 0 };
      };

#pragma pack(push, 1)
      struct PointShadowProperties
      {
          vec4<F32> _details;
          vec4<F32> _position;
      };
      struct SpotShadowProperties
      {
          vec4<F32> _details;
          vec4<F32> _position;
          mat4<F32> _vpMatrix;
      };
      struct CSMShadowProperties
      {
          vec4<F32> _details;
          std::array<vec4<F32>, Config::Lighting::MAX_CSM_SPLITS_PER_LIGHT> _position;
          std::array<mat4<F32>, Config::Lighting::MAX_CSM_SPLITS_PER_LIGHT> _vpMatrix;
      };
      struct ShadowProperties {
          std::array<PointShadowProperties, Config::Lighting::MAX_SHADOW_CASTING_POINT_LIGHTS> _pointLights;
          std::array<SpotShadowProperties, Config::Lighting::MAX_SHADOW_CASTING_SPOT_LIGHTS> _spotLights;
          std::array<CSMShadowProperties, Config::Lighting::MAX_SHADOW_CASTING_DIRECTIONAL_LIGHTS> _dirLights;

          [[nodiscard]] bufferPtr data() const noexcept { return (bufferPtr)_pointLights.data(); }
      };
#pragma pack(pop)

  public:
    struct ShadowLightList
    {
        U16 count = 0;
        Light* entries[Config::Lighting::MAX_SHADOW_CASTING_LIGHTS];
    };

    using LightList = vectorEASTL<Light*>;

    explicit LightPool(Scene& parentScene, PlatformContext& context);
    ~LightPool();

    /// Add a new light to the manager
    [[nodiscard]] bool addLight(Light& light);
    /// remove a light from the manager
    [[nodiscard]] bool removeLight(Light& light);
    /// disable or enable a specific light type
    void toggleLightType(const LightType type)                   noexcept { toggleLightType(type, !lightTypeEnabled(type)); }
    void toggleLightType(const LightType type, const bool state) noexcept { _lightTypeState[to_U32(type)] = state; }

    [[nodiscard]] bool lightTypeEnabled(const LightType type) const noexcept { return _lightTypeState[to_U32(type)]; }
    /// Retrieve the number of active lights in the scene;
    [[nodiscard]] U32 getActiveLightCount(const RenderStage stage, const LightType type) const noexcept { return _activeLightCount[to_base(stage)][to_U32(type)]; }

    bool clear();
    [[nodiscard]] LightList& getLights(const LightType type) {
        SharedLock<SharedMutex> r_lock(_lightLock); 
        return _lights[to_U32(type)];
    }

    [[nodiscard]] Light* getLight(I64 lightGUID, LightType type);

    void prepareLightData(RenderStage stage, const vec3<F32>& eyePos, const mat4<F32>& viewMatrix);

    void uploadLightData(RenderStage stage, GFX::CommandBuffer& bufferInOut);

    void drawLightImpostors(RenderStage stage, GFX::CommandBuffer& bufferInOut) const;

    void preRenderAllPasses(const Camera* playerCamera);
    void postRenderAllPasses(const Camera* playerCamera);

    /// nullptr = disabled
    void debugLight(Light* light);

    static void idle();

    /// Get the appropriate shadow bind slot for every light's shadow
    [[nodiscard]] static U8 getShadowBindSlotOffset(const ShadowType type) noexcept {
        return to_U8(_shadowLocation[to_U32(type)]);
    }

    /// Get the appropriate shadow bind slot offset for every light's shadow
    [[nodiscard]] static U8 getShadowBindSlotOffset(const LightType lightType) noexcept {
        switch (lightType) {
            case LightType::SPOT:
                return getShadowBindSlotOffset(ShadowType::SINGLE);
            case LightType::POINT:
                return getShadowBindSlotOffset(ShadowType::CUBEMAP);
            case LightType::DIRECTIONAL:
                return getShadowBindSlotOffset(ShadowType::LAYERED);
            default:
                DIVIDE_UNEXPECTED_CALL();
                return 0u;
        };
    }
    
    PROPERTY_RW(bool, lightImpostorsEnabled, false);
    POINTER_R(Light, debugLight, nullptr);

  protected:
    using LightShadowProperties = std::array<Light::ShadowProperties, Config::Lighting::MAX_SHADOW_CASTING_LIGHTS>;

    friend class RenderPass;
    void generateShadowMaps(const Camera& playerCamera, GFX::CommandBuffer& bufferInOut);

    friend class SceneManager;
    [[nodiscard]] LightList::const_iterator findLight(const I64 GUID, const LightType type) const {
        SharedLock<SharedMutex> r_lock(_lightLock);
        return findLightLocked(GUID, type);
    }

    [[nodiscard]] LightList::const_iterator findLightLocked(I64 GUID, const LightType type) const {
        return eastl::find_if(eastl::cbegin(_lights[to_U32(type)]), eastl::cend(_lights[to_U32(type)]),
                             [&GUID](Light* const light) noexcept {
                                 return (light && light->getGUID() == GUID);
                             });
    }

  private:
      void init();
      U32 uploadLightList(RenderStage stage,
                          const LightList& lights,
                          const mat4<F32>& viewMatrix);

  private:
     struct BufferData {
         // x = directional light count, y = point light count, z = spot light count, w = shadow light count
         vec4<U32> _globalData = {0, 0, 0, 0};
         // a = reserved
         vec4<F32> _ambientColour = DefaultColours::BLACK;
         std::array<LightProperties, Config::Lighting::MAX_POSSIBLE_LIGHTS> _lightProperties;
     };

    std::array<std::array<U32, to_base(LightType::COUNT)>, to_base(RenderStage::COUNT)> _activeLightCount;
    std::array<LightList, to_base(RenderStage::COUNT)> _sortedLights;
    std::array<BufferData, to_base(RenderStage::COUNT)> _sortedLightProperties;
    std::array<LightList, to_base(LightType::COUNT)> _lights;
    std::array<bool, to_base(LightType::COUNT)> _lightTypeState;

    ShadowLightList _sortedShadowLights = {};
    ShadowProperties _shadowBufferData;

    mutable SharedMutex _lightLock;

    Texture_ptr _lightIconsTexture = nullptr;
    ShaderProgram_ptr _lightImpostorShader = nullptr;
    ShaderBuffer* _lightShaderBuffer = nullptr;
    ShaderBuffer* _shadowBuffer = nullptr;
    Time::ProfileTimer& _shadowPassTimer;
    bool _init = false;

    static std::array<TextureUsage, to_base(ShadowType::COUNT)> _shadowLocation;
};

};  // namespace Divide

#endif //_LIGHT_POOL_H_