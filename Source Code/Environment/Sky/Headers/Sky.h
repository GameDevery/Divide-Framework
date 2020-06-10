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

class Sky : public SceneNode {
   public: 
       struct Atmosphere {
           vec3<F32> _RayleighCoeff =
           { 5.5f, 13.0f, 22.4f };         // Rayleigh scattering coefficient
           F32 _sunIntensity = 11.2f;      // intensity of the sun
           F32 _planetRadius = 0.5f;       // x 6371e3m (radius of the planet in meters)
           F32 _atmosphereOffset = 100.f;  // planetRadius + (atmoOffset * distanceMult) = radius of the atmosphere in meters
           F32 _MieCoeff = 21.f;           // Mie scattering coefficient
           F32 _RayleighScale = 8.f;       // Rayleigh scale height
           F32 _MieScaleHeight = 1.2f;     // Mie scale height
           F32 _MieScatterDir = 0.758f;    // Mie preferred scattering direction
           F32 _rayOriginDistance = 1.25f; // Offset from planetRadius for the ray origin
           I32 _distanceMultiplier = 1000; // Factor to multiply all distances by
       };

   public:
    explicit Sky(GFXDevice& context, ResourceCache* parentCache, size_t descriptorHash, const Str256& name, U32 diameter);
    ~Sky();

    // Returns the sun position and intensity details for the specified date-time
    SunDetails setDateTime(struct tm *dateTime);
    SunDetails getCurrentDetails() const noexcept;

    PROPERTY_R(Atmosphere, atmosphere);
    void setAtmosphere(const Atmosphere& atmosphere) noexcept;

    PROPERTY_R(Atmosphere, defaultAtmosphere);

    PROPERTY_R(size_t, skyboxSampler, 0);

    PROPERTY_RW(bool, useDaySkybox, true);
    PROPERTY_RW(bool, useNightSkybox, true);
    PROPERTY_RW(FColour4, nightSkyColour, DefaultColours::BLACK);

    const Texture_ptr& activeSkyBox() const noexcept;
    

   protected:
    void postLoad(SceneGraphNode* sgn) override;

    void sceneUpdate(const U64 deltaTimeUS, SceneGraphNode* sgn, SceneState& sceneState) final;

    void buildDrawCommands(SceneGraphNode* sgn,
                           const RenderStagePass& renderStagePass,
                           const Camera& crtCamera,
                           RenderPackage& pkgInOut) override;

    bool prepareRender(SceneGraphNode* sgn,
                        RenderingComponent& rComp,
                        const RenderStagePass& renderStagePass,
                        const Camera& camera,
                        bool refreshData) final;

   protected:
    template <typename T>
    friend class ImplResourceLoader;

    bool load() override;

    const char* getResourceTypeName() const noexcept  override { return "Sky"; }

    const std::array<vec4<F32>, 3> atmoToShaderData() const noexcept;
  
    GFXDevice& _context;
    eastl::unique_ptr<Sun> _sun = nullptr;
    vec4<F32> _sunDirectionAndIntensity;
    Texture_ptr  _skybox[2] = { nullptr, nullptr };
    Sphere3D_ptr _sky = nullptr;
    ShaderProgram_ptr _skyShader = nullptr;
    ShaderProgram_ptr _skyShaderPrePass = nullptr;
    size_t _skyboxRenderStateHash = 0;
    size_t _skyboxRenderStateHashPrePass = 0;

    size_t _skyboxRenderStateReflectedHash = 0;
    size_t _skyboxRenderStateReflectedHashPrePass = 0;
    U32  _diameter = 1u;
    EditorDataState _atmosphereChanged = EditorDataState::IDLE;
};

TYPEDEF_SMART_POINTERS_FOR_TYPE(Sky);

};  // namespace Divide

#endif //_SKY_H_
