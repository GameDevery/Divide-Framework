/*
Copyright (c) 2016 DIVIDE-Studio
Copyright (c) 2009 Ionut Cava

This file is part of DIVIDE Framework.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software
and associated documentation files (the "Software"), to deal in the Software
without restriction,
including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef _RENDERING_COMPONENT_H_
#define _RENDERING_COMPONENT_H_

#include "SGNComponent.h"
#include "Core/Math/Headers/MathMatrices.h"
#include "Platform/Video/Headers/GFXDevice.h"

namespace Divide {

class Sky;
class Material;
class GFXDevice;
class RenderBin;
class ImpostorBox;
class ShaderProgram;
class SceneGraphNode;
class ParticleEmitter;

namespace Attorney {
    class RenderingCompRenderPass;
    class RenderingCompGFXDevice;
    class RenderingCompRenderBin;
    class RenderingCompPassCuller;
    class RenderingCompSceneNode;
};

typedef DELEGATE_CBK_PARAM_3<const SceneGraphNode&, 
                             const SceneRenderState&,
                             GFXDevice::RenderTarget&> RenderCallback;

class RenderingComponent : public SGNComponent {
    friend class Attorney::RenderingCompRenderPass;
    friend class Attorney::RenderingCompGFXDevice;
    friend class Attorney::RenderingCompRenderBin;
    friend class Attorney::RenderingCompPassCuller;
    friend class Attorney::RenderingCompSceneNode;

   public:
    bool onRender(RenderStage currentStage) override;
    void update(const U64 deltaTime) override;

    void renderGeometry(const bool state);
    void renderWireframe(const bool state);
    void renderBoundingBox(const bool state);
    void renderBoundingSphere(const bool state);
    void renderSkeleton(const bool state);

    inline bool renderGeometry() const { return _renderGeometry; }
    inline bool renderWireframe() const { return _renderWireframe; }
    inline bool renderBoundingBox() const { return _renderBoundingBox; }
    inline bool renderBoundingSphere() const { return _renderBoundingSphere; }
    inline bool renderSkeleton() const { return _renderSkeleton; }

    void castsShadows(const bool state);
    void receivesShadows(const bool state);

    bool castsShadows() const;
    bool receivesShadows() const;
    
    inline U32 drawOrder() const { return _drawOrder; }

    inline U32 commandIndex() const { return _commandIndex; }

    inline U32 commandOffset() const { return _commandOffset; }

    ShaderProgram* const getDrawShader(RenderStage renderStage = RenderStage::DISPLAY);

    size_t getDrawStateHash(RenderStage renderStage);

    void getMaterialColorMatrix(mat4<F32>& matOut) const;

    void getRenderingProperties(vec4<F32>& propertiesOut) const;

    inline Material* const getMaterialInstance() const { return _materialInstance; }

    void registerShaderBuffer(ShaderBufferLocation slot,
                              vec2<U32> bindRange,
                              ShaderBuffer& shaderBuffer);

    void unregisterShaderBuffer(ShaderBufferLocation slot);


    void registerTextureDependency(const TextureData& additionalTexture);
    void removeTextureDependency(const TextureData& additionalTexture);

    inline void setReflectionCallback(RenderCallback cbk) { _reflectionCallback = cbk; }
    inline void setRefractionCallback(RenderCallback cbk) { _refractionCallback = cbk; }

#ifdef _DEBUG
    void drawDebugAxis();
#endif

    void setActive(const bool state) override;

   protected:
    friend class SceneGraphNode;
    RenderingComponent(Material* const materialInstance,
                       SceneGraphNode& parentSGN);
    ~RenderingComponent();

   protected:
    bool canDraw(const SceneRenderState& sceneRenderState,
                 RenderStage renderStage);

    bool preDraw(const SceneRenderState& renderState,
                 RenderStage renderStage) const;

    /// Called after the parent node was rendered
    void postRender(const SceneRenderState& sceneRenderState,
                    RenderStage renderStage);

    GFXDevice::RenderPackage& getDrawPackage(const SceneRenderState& sceneRenderState,
                                             RenderStage renderStage);

    GFXDevice::RenderPackage& getDrawPackage(RenderStage renderStage);

    inline void drawOrder(U32 index) { _drawOrder = index; }

    inline void commandIndex(U32 index) { _commandIndex = index; }

    inline void commandOffset(U32 offset) { _commandOffset = offset; }

    // This returns false if the node is not reflective, otherwise it generates a new reflection cube map
    // and saves it in the appropriate material slot
    bool updateReflection(U32 reflectionIndex, 
                          const vec3<F32>& camPos,
                          const SceneRenderState& renderState);
    bool updateRefraction(U32 refractionIndex,
                          const vec3<F32>& camPos,
                          const SceneRenderState& renderState);
    bool clearReflection();
    bool clearRefraction();

   protected:
    Material* _materialInstance;
    /// LOD level is updated at every visibility check
    U8  _lodLevel;  ///<Relative to camera distance
    U32 _drawOrder;
    U32 _commandIndex;
    U32 _commandOffset;
    bool _castsShadows;
    bool _receiveShadows;
    bool _renderGeometry;
    bool _renderWireframe;
    bool _renderBoundingBox;
    bool _renderBoundingSphere;
    bool _renderSkeleton;
    TextureDataContainer _textureDependencies;
    std::array<GFXDevice::RenderPackage, to_const_uint(RenderStage::COUNT)> _renderData;
    
    IMPrimitive* _boundingBoxPrimitive[2];
    IMPrimitive* _boundingSpherePrimitive;
    IMPrimitive* _skeletonPrimitive;

    RenderCallback _reflectionCallback;
    RenderCallback _refractionCallback;
#ifdef _DEBUG
    vectorImpl<Line> _axisLines;
    IMPrimitive* _axisGizmo;
#endif
};

namespace Attorney {
class RenderingCompRenderPass {
    private:
        static bool updateReflection(RenderingComponent& renderable,
                                     U32 reflectionIndex,
                                     const vec3<F32>& camPos,
                                     const SceneRenderState& renderState)
        {
            return renderable.updateReflection(reflectionIndex, camPos, renderState);
        }

        static bool updateRefraction(RenderingComponent& renderable,
                                     U32 refractionIndex,
                                     const vec3<F32>& camPos,
                                     const SceneRenderState& renderState)
        {
            return renderable.updateRefraction(refractionIndex, camPos, renderState);
        }

        static bool clearReflection(RenderingComponent& renderable)
        {
            return renderable.clearReflection();
        }

        static bool clearRefraction(RenderingComponent& renderable)
        {
            return renderable.clearRefraction();
        }

        friend class Divide::RenderPass;
};

class RenderingCompSceneNode {
    private:
        static GFXDevice::RenderPackage& getDrawPackage(RenderingComponent& renderable,
                                                        RenderStage renderStage) {
            return renderable.getDrawPackage(renderStage);
        }

    friend class Divide::Sky;
    friend class Divide::SubMesh;
    friend class Divide::ParticleEmitter;
};

class RenderingCompGFXDevice {
   private:
    static GFXDevice::RenderPackage& getDrawPackage(RenderingComponent& renderable,
                                                    const SceneRenderState& sceneRenderState,
                                                    RenderStage renderStage) {
        return renderable.getDrawPackage(sceneRenderState, renderStage);
    }

    static GFXDevice::RenderPackage& getDrawPackage(RenderingComponent& renderable,
                                                    const SceneRenderState& sceneRenderState,
                                                    RenderStage renderStage,
                                                    U32 cmdOffset,
                                                    U32 cmdIndex) {
        renderable.commandIndex(cmdIndex);
        renderable.commandOffset(cmdOffset);
        return getDrawPackage(renderable, sceneRenderState, renderStage);
    }

    friend class Divide::GFXDevice;
};

class RenderingCompRenderBin {
   private:
    static const GFXDevice::RenderPackage& getRenderData(RenderingComponent& renderable,
                                                         RenderStage renderStage) {
        return renderable._renderData[to_uint(renderStage)];
    }

    static void postRender(RenderingComponent& renderable,
                           const SceneRenderState& sceneRenderState,
                           RenderStage renderStage) {
        renderable.postRender(sceneRenderState, renderStage);
    }

    static void drawOrder(RenderingComponent& renderable, U32 index) {
        renderable.drawOrder(index);
    }

    friend class Divide::RenderBin;
};

class RenderingCompPassCuller {
   private:
    static bool canDraw(RenderingComponent& renderable,
                        const SceneRenderState& sceneRenderState,
                        RenderStage renderStage) {
        return renderable.canDraw(sceneRenderState, renderStage);
    }

    friend class Divide::RenderPassCuller;
};

};  // namespace Attorney
};  // namespace Divide
#endif