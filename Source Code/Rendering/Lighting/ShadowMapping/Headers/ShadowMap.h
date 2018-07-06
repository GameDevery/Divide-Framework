/*
   Copyright (c) 2015 DIVIDE-Studio
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

#ifndef _SHADOW_MAP_H_
#define _SHADOW_MAP_H_

#include "Core/Math/Headers/MathMatrices.h"
#include "Platform/Video/Buffers/Framebuffer/Headers/Framebuffer.h"

namespace Divide {

enum class ShadowType : U32 {
    SINGLE = 0,
    LAYERED,
    CUBEMAP,
    COUNT
};

class Light;
class Camera;
class ParamHandler;
class ShadowMapInfo;
class SceneRenderState;

/// All the information needed for a single light's shadowmap
class NOINITVTABLE ShadowMap {
   public:
    ShadowMap(Light* light, Camera* shadowCamera, ShadowType type);
    virtual ~ShadowMap();

    /// Render the scene and save the frame to the shadow map
    virtual void render(SceneRenderState& renderState) = 0;
    /// Get the current shadow mapping tehnique
    inline ShadowType getShadowMapType() const { return _shadowMapType; }

    inline Framebuffer* getDepthMap() { return _depthMaps[to_uint(getShadowMapType())]; }

    inline U32 getArrayOffset() const {
        return _arrayOffset;
    }

    virtual void init(ShadowMapInfo* const smi) = 0;
    virtual void previewShadowMaps(U32 rowIndex) = 0;
    
    virtual void onCameraUpdate(Camera& camera) {}

    static void initShadowMaps();
    static void clearShadowMaps();
    static void bindShadowMaps();
    static U32 findDepthMapLayer(ShadowType shadowType);
    static void commitDepthMapLayer(ShadowType shadowType, U32 layer);
    static bool freeDepthMapLayer(ShadowType shadowType, U32 layer);
    static void clearShadowMapBuffers();
   protected:
    vec4<I32> getViewportForRow(U32 rowIndex) const;

   protected:
    /// The depth maps. Using 1 array for each type: CSM, Cube and single
    static std::array<Framebuffer*, to_const_uint(ShadowType::COUNT)> _depthMaps;
    typedef std::array<bool, Config::Lighting::MAX_SHADOW_CASTING_LIGHTS> LayerUsageMask;
    static std::array<LayerUsageMask, to_const_uint(ShadowType::COUNT)> _depthMapUsage;

    ShadowType _shadowMapType;
    U32 _arrayOffset;
    /// Internal pointer to the parent light
    Light* _light;
    Camera* _shadowCamera;
    ParamHandler& _par;
    bool _init;
    mat4<F32> _bias;
};

class ShadowMapInfo {
   public:
    ShadowMapInfo(Light* light);
    virtual ~ShadowMapInfo();

    inline ShadowMap* getShadowMap() { return _shadowMap; }

    ShadowMap* createShadowMap(const SceneRenderState& sceneRenderState, Camera* shadowCamera);

    inline U8 numLayers() const { return _numLayers; }

    inline void numLayers(U8 layerCount) {
        _numLayers = std::min(layerCount, to_ubyte(Config::Lighting::MAX_SPLITS_PER_LIGHT));
    }

   private:
    U8 _numLayers;
    ShadowMap* _shadowMap;
    Light* _light;
};

};  // namespace Divide

#endif