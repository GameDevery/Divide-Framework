/*
Copyright (c) 2018 DIVIDE-Studio
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

#pragma once
#ifndef _RENDERING_COMPONENT_H_
#define _RENDERING_COMPONENT_H_

#include "SGNComponent.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/RenderPackage.h"
#include "Geometry/Material/Headers/MaterialEnums.h"
#include "Rendering/Lighting/ShadowMapping/Headers/ShadowMap.h"

namespace Divide {

class Sky;
class SubMesh;
class Material;
class GFXDevice;
class RenderBin;
class WaterPlane;
class SceneGraphNode;
class ParticleEmitter;
class SceneEnvironmentProbePool;
class EnvironmentProbeComponent;

struct RenderBinItem;

using EnvironmentProbeList = vectorEASTL<EnvironmentProbeComponent*>;

TYPEDEF_SMART_POINTERS_FOR_TYPE(Material);

namespace Attorney {
    class RenderingCompRenderPass;
    class RenderingCompGFXDevice;
    class RenderingCompRenderBin;
    class RenderingComponentSGN;
};

struct RenderParams {
    GenericDrawCommand _cmd;
    Pipeline _pipeline;
};

struct TargetDataBufferParams {
    GFX::CommandBuffer* _targetBuffer = nullptr;
    const Camera* _camera = nullptr;
    I32 _writeIndex = 0;
    U32 _dataIndex = 0u;
};

using DrawCommandContainer = eastl::fixed_vector<IndirectDrawCommand, Config::MAX_VISIBLE_NODES, false>;

struct RefreshNodeDataParams {
    DrawCommandContainer* _drawCommandsInOut = nullptr;
    GFX::CommandBuffer* _bufferInOut = nullptr;
    const RenderStagePass* _stagePass = nullptr;
    const Camera* _camera = nullptr;
};

struct RenderCbkParams {
    explicit RenderCbkParams(GFXDevice& context,
                             const SceneGraphNode* sgn,
                             const SceneRenderState& sceneRenderState,
                             const RenderTargetID& renderTarget,
                             U16 passIndex,
                             U8 passVariant,
                             Camera* camera) noexcept
        : _context(context),
          _sgn(sgn),
          _sceneRenderState(sceneRenderState),
          _renderTarget(renderTarget),
          _passIndex(passIndex),
          _passVariant(passVariant),
          _camera(camera)
    {
    }

    GFXDevice& _context;
    const SceneGraphNode* _sgn = nullptr;
    const SceneRenderState& _sceneRenderState;
    const RenderTargetID& _renderTarget;
    Camera* _camera;
    U16 _passIndex;
    U8  _passVariant;
};

using RenderCallback = DELEGATE<void, RenderCbkParams&, GFX::CommandBuffer&>;

constexpr U16 g_invalidRefIndex = std::numeric_limits<U16>::max();

class RenderingComponent final : public BaseComponentType<RenderingComponent, ComponentType::RENDERING> {
    friend class Attorney::RenderingCompRenderPass;
    friend class Attorney::RenderingCompGFXDevice;
    friend class Attorney::RenderingCompRenderBin;
    friend class Attorney::RenderingComponentSGN;

   public:
       enum class RenderOptions : U16 {
           RENDER_GEOMETRY = toBit(1),
           RENDER_WIREFRAME = toBit(2),
           RENDER_SKELETON = toBit(3),
           RENDER_SELECTION = toBit(4),
           RENDER_AXIS = toBit(5),
           CAST_SHADOWS = toBit(6),
           RECEIVE_SHADOWS = toBit(7),
           IS_VISIBLE = toBit(8)
       };

       struct NodeRenderingProperties {
           U32 _reflectionIndex = 0u;
           U32 _refractionIndex = 0u;
           F32 _cullFlagValue = 1.0f;
           U8 _lod = 0u;
           TextureOperation _texOperation = TextureOperation::REPLACE;
           BumpMethod _bumpMethod = BumpMethod::NONE;
           bool _receivesShadows = false;
           bool _isHovered = false;
           bool _isSelected = false;
       };

   public:
    explicit RenderingComponent(SceneGraphNode* parentSGN,
                                PlatformContext& context);
    ~RenderingComponent();

    void Update(const U64 deltaTimeUS) final;

    void toggleRenderOption(RenderOptions option, bool state, bool recursive = true);
    bool renderOptionEnabled(RenderOptions option) const;
    bool renderOptionsEnabled(U32 mask) const;

    void setMinRenderRange(F32 minRange) noexcept;
    void setMaxRenderRange(F32 maxRange) noexcept;
    inline void setRenderRange(F32 minRange, F32 maxRange) noexcept { setMinRenderRange(minRange); setMaxRenderRange(maxRange); }
    inline const vec2<F32>& renderRange() const noexcept { return _renderRange; }

    inline void lockLoD(U8 level) { _lodLockLevels.fill({ true, level }); }
    inline void unlockLoD() { _lodLockLevels.fill({ false, to_U8(0u) }); }
    inline void lockLoD(RenderStage stage, U8 level) noexcept { _lodLockLevels[to_base(stage)] = { true, level }; }
    inline void unlockLoD(RenderStage stage, U8 level) noexcept { _lodLockLevels[to_base(stage)] = { false, to_U8(0u) }; }
    inline bool lodLocked(RenderStage stage) const noexcept { return _lodLockLevels[to_base(stage)].first; }
    void instantiateMaterial(const Material_ptr& material);

    void getMaterialColourMatrix(const RenderStagePass& stagePass, mat4<F32>& matOut) const;

    void getRenderingProperties(const RenderStagePass& stagePass, NodeRenderingProperties& propertiesOut) const;

    RenderPackage& getDrawPackage(const RenderStagePass& renderStagePass);
    const RenderPackage& getDrawPackage(const RenderStagePass& renderStagePass) const;

    size_t getSortKeyHash(const RenderStagePass& renderStagePass) const;

    inline const Material_ptr& getMaterialInstance() const noexcept { return _materialInstance; }

    void rebuildMaterial();

    inline void setReflectionAndRefractionType(ReflectorType reflectType, RefractorType refractType) noexcept { _reflectorType = reflectType;  _refractorType = refractType; }
    inline void setReflectionCallback(const RenderCallback& cbk) { _reflectionCallback = cbk; }
    inline void setRefractionCallback(const RenderCallback& cbk) { _refractionCallback = cbk; }

    void drawDebugAxis(GFX::CommandBuffer& bufferInOut);
    void drawSelectionGizmo(GFX::CommandBuffer& bufferInOut);
    void drawSkeleton(GFX::CommandBuffer& bufferInOut);
    void drawBounds(const bool AABB, const bool Sphere, GFX::CommandBuffer& bufferInOut);
    
    void prepareRender(const RenderStagePass& renderStagePass);

    U8 getLoDLevel(const vec3<F32>& center, const vec3<F32>& cameraEye, RenderStage renderStage, const vec4<U16>& lodThresholds);

    inline void addShaderBuffer(const ShaderBufferBinding& binding) { _externalBufferBindings.push_back(binding); }
    inline const ShaderBufferList& getShaderBuffers() const noexcept { return _externalBufferBindings; }

  protected:
    void toggleBoundsDraw(bool showAABB, bool showBS, bool recursive);

    bool onRefreshNodeData(RefreshNodeDataParams& refreshParams, const TargetDataBufferParams& bufferParams, bool quick);

    bool canDraw(const RenderStagePass& renderStagePass, U8 LoD, bool refreshData);

    /// Called after the parent node was rendered
    void postRender(const SceneRenderState& sceneRenderState,
                    const RenderStagePass& renderStagePass,
                    GFX::CommandBuffer& bufferInOut);

    void rebuildDrawCommands(const RenderStagePass& stagePass, const Camera& crtCamera, RenderPackage& pkg);

    bool prepareDrawPackage(const Camera& camera, const SceneRenderState& sceneRenderState, const RenderStagePass& renderStagePass, bool refreshData);

    // This returns false if the node is not reflective, otherwise it generates a new reflection cube map
    // and saves it in the appropriate material slot
    bool updateReflection(U16 reflectionIndex, 
                          bool inBudget,
                          Camera* camera,
                          const SceneRenderState& renderState,
                          GFX::CommandBuffer& bufferInOut);

    bool updateRefraction(U16 refractionIndex,
                          bool inBudget,
                          Camera* camera,
                          const SceneRenderState& renderState,
                          GFX::CommandBuffer& bufferInOut);

    void updateNearestProbes(const SceneEnvironmentProbePool& probePool, const vec3<F32>& position);

    PROPERTY_R(bool, showAxis, false);
    // If the new value is negative, this disables occlusion culling!
    PROPERTY_RW(F32, cullFlag, 1.0f);

   protected:

    void onMaterialChanged();
    void onParentUsageChanged(NodeUsageContext context);

    void OnData(const ECS::CustomEvent& data) final;

   protected:
    using PackagesPerIndex = vectorEASTLFast<RenderPackage>;
    using PackagesPerPassType = std::array<PackagesPerIndex, to_base(RenderPassType::COUNT)>;
    using PackagesPerStage = std::array<PackagesPerPassType, to_base(RenderStage::COUNT)>;
    PackagesPerStage _renderPackages;

    RenderCallback _reflectionCallback;
    RenderCallback _refractionCallback;
    size_t _reflectionSampler = 0u;
    size_t _refractionSampler = 0u;
    Texture* _reflectionTexture = nullptr;
    Texture* _refractionTexture = nullptr;

    vectorEASTL<EnvironmentProbeComponent*> _envProbes;
    ShaderBufferList _externalBufferBindings;

    Material_ptr _materialInstance = nullptr;
    GFXDevice& _context;
    const Configuration& _config;

    mat4<F32> _worldMatrixCache;
    mat4<F32> _worldOffsetMatrixCache;
    vec3<F32> _boundsCenterCache;
    vec2<F32> _renderRange;

    Pipeline*    _primitivePipeline[3] = {nullptr, nullptr, nullptr};
    IMPrimitive* _boundingBoxPrimitive = nullptr;
    IMPrimitive* _boundingSpherePrimitive = nullptr;
    IMPrimitive* _skeletonPrimitive = nullptr;
    IMPrimitive* _axisGizmo = nullptr;
    IMPrimitive* _selectionGizmo = nullptr;

    /// used to keep track of what GFXDevice::reflectionTarget we are using for this rendering pass
    U16 _reflectionIndex = 0u;
    U16 _refractionIndex = 0u;
    U32 _renderMask = 0u;

    bool _drawAABB = false;
    bool _drawBS = false;

    std::array<U8, to_base(RenderStage::COUNT)> _lodLevels;
    ReflectorType _reflectorType = ReflectorType::CUBE;
    RefractorType _refractorType = RefractorType::COUNT;

    std::array<std::pair<bool, U8>, to_base(RenderStage::COUNT)> _lodLockLevels;

    static hashMap<U32, DebugView*> s_debugViews[2];
};

INIT_COMPONENT(RenderingComponent);

namespace Attorney {
class RenderingCompRenderPass {
    private:
        /// Returning true or false controls our reflection/refraction budget only. 

        /// Return true if we executed an external render pass (e.g. water planar reflection)
        /// Return false for no or non-expensive updates (e.g. selected the nearest probe)
        static bool updateReflection(RenderingComponent& renderable,
                                     U16 reflectionIndex,
                                     bool inBudget,
                                     Camera* camera,
                                     const SceneRenderState& renderState,
                                     GFX::CommandBuffer& bufferInOut)
        {
            return renderable.updateReflection(reflectionIndex, inBudget, camera, renderState, bufferInOut);
        }

        /// Return true if we executed an external render pass (e.g. water planar refraction)
        /// Return false for no or non-expensive updates (e.g. selected the nearest probe)
        static bool updateRefraction(RenderingComponent& renderable,
                                     U16 refractionIndex,
                                     bool inBudget,
                                     Camera* camera,
                                     const SceneRenderState& renderState,
                                     GFX::CommandBuffer& bufferInOut)
        {
            return renderable.updateRefraction(refractionIndex, inBudget, camera, renderState, bufferInOut);
        }

        static bool prepareDrawPackage(RenderingComponent& renderable,
                                       const Camera& camera,
                                       const SceneRenderState& sceneRenderState,
                                       const RenderStagePass& renderStagePass,
                                       bool refreshData) {
            return renderable.prepareDrawPackage(camera, sceneRenderState, renderStagePass, refreshData);
        }

        static bool onRefreshNodeData(RenderingComponent& renderable, RefreshNodeDataParams& refreshParams, const TargetDataBufferParams& bufferParams, bool quick) {
            return renderable.onRefreshNodeData(refreshParams, bufferParams, quick);
        }

        friend class Divide::RenderPass;
        friend class Divide::RenderPassManager;
};

class RenderingCompRenderBin {
   private:
    static void postRender(RenderingComponent* renderable,
                           const SceneRenderState& sceneRenderState,
                           const RenderStagePass& renderStagePass,
                           GFX::CommandBuffer& bufferInOut) {
        renderable->postRender(sceneRenderState, renderStagePass, bufferInOut);
    }

    friend class Divide::RenderBin;
    friend struct Divide::RenderBinItem;
};

class RenderingComponentSGN {
    static void onParentUsageChanged(RenderingComponent& comp, NodeUsageContext context) {
        comp.onParentUsageChanged(context);
    }
    friend class Divide::SceneGraphNode;
};
};  // namespace Attorney
};  // namespace Divide
#endif