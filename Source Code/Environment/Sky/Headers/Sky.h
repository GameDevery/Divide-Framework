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
#ifndef _SKY_H_
#define _SKY_H_

#include "Sun.h"
#include "Graphs/Headers/SceneNode.h"

namespace Divide {

class RenderStateBlock;

FWD_DECLARE_MANAGED_CLASS(Texture);
FWD_DECLARE_MANAGED_CLASS(Sphere3D);
FWD_DECLARE_MANAGED_CLASS(ShaderProgram);
FWD_DECLARE_MANAGED_CLASS(SceneGraphNode);

enum class RenderStage : U8;

enum class RebuildCommandsState : U8 {
    NONE,
    REQUESTED,
    DONE
};

class Sky final : public SceneNode {
   public: 
       struct Atmosphere {
           vec3<F32> _RayleighCoeff = { 5.5f, 13.0f, 22.4f };     // Rayleigh scattering coefficient
           vec2<F32> _cloudLayerMinMaxHeight = { 1000.f, 2300.f}; // Clouds will be limited between [planerRadius + min - planetRadius + height]
           F32 _sunIntensity = 1.f;        // x 1000. visual size of the sun disc
           F32 _sunPenetrationPower = 30.f;// Factor used to calculate atmosphere transmitance for clour layer
           F32 _planetRadius = 6360e3f;    // radius of the planet in meters
           F32 _cloudSphereRadius = 200e3f;// cloud sphere radius. Does not need to match planet radius
           F32 _atmosphereOffset = 60.f;   // planetRadius + atmoOffset = radius of the atmosphere in meters
           F32 _MieCoeff = 21e-6f;         // Mie scattering coefficient
           F32 _RayleighScale = 7994.f;    // Rayleigh scale height
           F32 _MieScaleHeight = 1200.f;   // Mie scale height
       };

   public:
    explicit Sky(GFXDevice& context, ResourceCache* parentCache, size_t descriptorHash, const Str256& name, U32 diameter);

    static void OnStartup(PlatformContext& context);
    // Returns the sun position and intensity details for the specified date-time
    const SunInfo& setDateTime(struct tm *dateTime) noexcept;
    const SunInfo& setGeographicLocation(SimpleLocation location) noexcept;
    const SunInfo& setDateTimeAndLocation(struct tm *dateTime, SimpleLocation location) noexcept;

    [[nodiscard]] SimpleTime GetTimeOfDay() const noexcept;
    [[nodiscard]] SimpleLocation GetGeographicLocation() const noexcept;

    [[nodiscard]] const SunInfo& getCurrentDetails() const;
    [[nodiscard]] vec3<F32> getSunPosition(F32 radius = 1.f) const;
    [[nodiscard]] bool isDay() const;

    PROPERTY_R(Atmosphere, atmosphere);
    void setAtmosphere(const Atmosphere& atmosphere) noexcept;

    PROPERTY_R(Atmosphere, defaultAtmosphere);
    PROPERTY_R(Atmosphere, initialAtmosphere);

    PROPERTY_R(size_t, skyboxSampler, 0);
    PROPERTY_R(size_t, noiseSamplerLinear, 0);
    PROPERTY_R(size_t, noiseSamplerMipMap, 0);

    PROPERTY_RW(bool, enableProceduralClouds, true);
    PROPERTY_RW(bool, useDaySkybox, true);
    PROPERTY_RW(bool, useNightSkybox, true);
    PROPERTY_RW(F32,  moonScale, 0.5f);
    PROPERTY_RW(F32,  weatherScale, 8.f);
    PROPERTY_RW(FColour4, moonColour, DefaultColours::WHITE);
    PROPERTY_RW(FColour4, nightSkyColour, DefaultColours::BLACK);

    [[nodiscard]] const Texture_ptr& activeSkyBox() const noexcept;

   protected:
    void postLoad(SceneGraphNode* sgn) override;

    void sceneUpdate(U64 deltaTimeUS, SceneGraphNode* sgn, SceneState& sceneState) override;

    void buildDrawCommands(SceneGraphNode* sgn,
                           const RenderStagePass& renderStagePass,
                           const Camera& crtCamera,
                           RenderPackage& pkgInOut) override;

    void prepareRender(SceneGraphNode* sgn,
                        RenderingComponent& rComp,
                        const RenderStagePass& renderStagePass,
                        const Camera& camera,
                        bool refreshData) override;

   protected:
    template <typename T>
    friend class ImplResourceLoader;

    bool load() override;

    [[nodiscard]] const char* getResourceTypeName() const noexcept  override { return "Sky"; }
    void setSkyShaderData(U32 rayCount, PushConstants& constantsInOut);

protected:
    GFXDevice& _context;
    Sun _sun;
    Texture_ptr  _skybox = nullptr;
    Texture_ptr _weatherTex = nullptr;
    Texture_ptr _curlNoiseTex = nullptr;
    Texture_ptr _worlNoiseTex = nullptr;
    Texture_ptr _perWorlNoiseTex = nullptr;
    Sphere3D_ptr _sky = nullptr;
    ShaderProgram_ptr _skyShader = nullptr;
    ShaderProgram_ptr _skyShaderLQ = nullptr;
    ShaderProgram_ptr _skyShaderPrePass = nullptr;
    size_t _skyboxRenderStateHash = 0;
    size_t _skyboxRenderStateHashPrePass = 0;

    size_t _skyboxRenderStateReflectedHash = 0;
    size_t _skyboxRenderStateReflectedHashPrePass = 0;
    U32  _diameter = 1u;
    EditorDataState _atmosphereChanged = EditorDataState::IDLE;
};

TYPEDEF_SMART_POINTERS_FOR_TYPE(Sky);

}  // namespace Divide

#endif //_SKY_H_
