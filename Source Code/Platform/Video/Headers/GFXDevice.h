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
#ifndef _HARDWARE_VIDEO_GFX_DEVICE_H_
#define _HARDWARE_VIDEO_GFX_DEVICE_H_

#include "config.h"

#include "GFXState.h"
#include "GFXRTPool.h"
#include "GFXShaderData.h"
#include "CommandBufferPool.h"
#include "Core/Math/Headers/Line.h"
#include "Core/Math/Headers/MathMatrices.h"

#include "Core/Headers/KernelComponent.h"
#include "Core/Headers/PlatformContextComponent.h"

#include "Platform/Video/Headers/RenderStagePass.h"
#include "Platform/Video/Headers/RenderAPIWrapper.h"

#include "Rendering/Camera/Headers/Frustum.h"
#include "Rendering/RenderPass/Headers/RenderPass.h"
#include "Rendering/RenderPass/Headers/RenderQueue.h"

class RenderDocManager;

namespace Divide {

enum class SceneNodeType : U16;
enum class WindowEvent : U8;

class GUI;
class GUIText;
class SceneGUIElements;

class GL_API;
class DX_API;

class Light;
class Camera;
class Quad3D;
class Texture;
class Object3D;
class Renderer;
class IMPrimitive;
class PixelBuffer;
class VertexBuffer;
class SceneGraphNode;
class SceneRenderState;
class GenericVertexData;
class ShaderComputeQueue;

struct SizeChangeParams;
struct SizeChangeParams;
struct ShaderBufferDescriptor;

FWD_DECLARE_MANAGED_CLASS(Texture);

namespace Time {
    class ProfileTimer;
};

namespace Attorney {
    class GFXDeviceAPI;
    class GFXDeviceGUI;
    class GFXDeviceKernel;
    class GFXDeviceGraphicsResource;
    class GFXDeviceGFXRTPool;
    class KernelApplication;
};

namespace TypeUtil {
    const char* GraphicResourceTypeToName(GraphicsResource::Type type);

    const char* RenderStageToString(RenderStage stage);
    RenderStage StringToRenderStage(const char* stage);

    const char* RenderPassTypeToString(RenderPassType pass);
    RenderPassType StringToRenderPassType(const char* pass);
};

struct DebugView : public GUIDWrapper {
    DebugView() noexcept
        : GUIDWrapper()
        , _textureBindSlot(0)
        , _sortIndex(-1)
        , _enabled (true)
    {
    }

    DebugView(I16 sortIndex) noexcept
        : GUIDWrapper()
        , _textureBindSlot(0)
        , _sortIndex(to_I16(sortIndex))
        , _enabled(true)
    {
    }

    bool _enabled;
    U8 _textureBindSlot;
    Texture_ptr _texture;
    ShaderProgram_ptr _shader;
    PushConstants _shaderData;

    I16 _sortIndex;
    stringImpl _name;
};

FWD_DECLARE_MANAGED_STRUCT(DebugView);

/// Rough around the edges Adapter pattern abstracting the actual rendering API
/// and access to the GPU
class GFXDevice : public KernelComponent, public PlatformContextComponent {
    friend class Attorney::GFXDeviceAPI;
    friend class Attorney::GFXDeviceGUI;
    friend class Attorney::GFXDeviceKernel;
    friend class Attorney::GFXDeviceGraphicsResource;
    friend class Attorney::GFXDeviceGFXRTPool;

public:
    enum class ScreenTargets : U8 {
        ALBEDO = 0,
        NORMALS_AND_VELOCITY = 1,
        EXTRA = 2,
        COUNT,
        MODULATE = EXTRA,
        ACCUMULATION = ALBEDO,
        REVEALAGE = NORMALS_AND_VELOCITY,
    };

    struct NodeData {
        mat4<F32> _worldMatrix = MAT4_IDENTITY;
        mat4<F32> _normalMatrixW = MAT4_IDENTITY;
        mat4<F32> _colourMatrix = MAT4_ZERO;
    };

    using ObjectArena = MyArena<Config::REQUIRED_RAM_SIZE / 4>;

public:  // GPU interface
    explicit GFXDevice(Kernel& parent);
    ~GFXDevice();

    static constexpr U32 MaxFrameQueueSize = 2;
    static_assert(MaxFrameQueueSize > 0, "FrameQueueSize is invalid!");

    ErrorCode initRenderingAPI(I32 argc, char** argv, RenderAPI API, const vec2<U16>& renderResolution);
    ErrorCode postInitRenderingAPI();
    void closeRenderingAPI();

    void idle();
    void beginFrame(DisplayWindow& window, bool global);
    void endFrame(DisplayWindow& window, bool global);

    void debugDraw(const SceneRenderState& sceneRenderState, const Camera& activeCamera, GFX::CommandBuffer& bufferInOut);

    void flushCommandBuffer(GFX::CommandBuffer& commandBuffer, bool batch = true);
    
    /// Generate a cubemap from the given position
    /// It renders the entire scene graph (with culling) as default
    /// use the callback param to override the draw function
    void generateCubeMap(
        RenderTargetID cubeMap,
        const U16 arrayOffset,
        const vec3<F32>& pos,
        const vec2<F32>& zPlanes,
        RenderStagePass stagePass,
        U32 passIndex,
        GFX::CommandBuffer& commandsInOut,
        SceneGraphNode* sourceNode = nullptr,
        Camera* camera = nullptr);

    void generateDualParaboloidMap(RenderTargetID targetBuffer,
        const U16 arrayOffset,
        const vec3<F32>& pos,
        const vec2<F32>& zPlanes,
        RenderStagePass stagePass,
        U32 passIndex,
        GFX::CommandBuffer& commandsInOut,
        SceneGraphNode* sourceNode = nullptr,
        Camera* camera = nullptr);

    /// Access (Read Only) rendering data used by the GFX
    inline const GFXShaderData::GPUData& renderingData() const noexcept;

    /// Returns true if the viewport was changed
           bool setViewport(const Rect<I32>& viewport);
    inline bool setViewport(I32 x, I32 y, I32 width, I32 height);
    inline const Rect<I32>& getViewport() const noexcept;

    inline F32 renderingAspectRatio() const noexcept;
    inline const vec2<U16>& renderingResolution() const noexcept;

    /// Switch between fullscreen rendering
    void toggleFullScreen();
    void increaseResolution();
    void decreaseResolution();

    /// Save a screenshot in TGA format
    void Screenshot(const stringImpl& filename);

    ShaderComputeQueue& shaderComputeQueue();
    const ShaderComputeQueue& shaderComputeQueue() const;

public:  // Accessors and Mutators
    inline Renderer& getRenderer() const;
    inline const GPUState& gpuState() const noexcept;
    inline GPUState& gpuState() noexcept;
    inline void debugDrawFrustum(const Frustum* frustum) noexcept;
    /// returns the standard state block
    inline size_t getDefaultStateBlock(bool noDepth) const noexcept;
    inline size_t get2DStateBlock() const noexcept;
    inline const Texture_ptr& getPrevDepthBuffer() const noexcept;
    inline GFXRTPool& renderTargetPool() noexcept;
    inline const GFXRTPool& renderTargetPool() const noexcept;
    inline const ShaderProgram_ptr& getRTPreviewShader(bool depthOnly) const noexcept;
    inline U32 getFrameCount() const noexcept;
    inline I32 getDrawCallCount() const noexcept;
    /// Return the last number of HIZ culled items
    inline U32 getLastCullCount() const noexcept;
    inline Arena::Statistics getObjectAllocStats() const noexcept;
    inline void registerDrawCall() noexcept;
    inline void registerDrawCalls(U32 count) noexcept;
    inline const Rect<I32>& getCurrentViewport() const noexcept;

    DebugView* addDebugView(const eastl::shared_ptr<DebugView>& view);
    bool removeDebugView(DebugView* view);

    /// In milliseconds
    inline F32 getFrameDurationGPU() const;
    inline vec2<U16> getDrawableSize(const DisplayWindow& window) const;
    inline U32 getHandleFromCEGUITexture(const CEGUI::Texture& textureIn) const;
    inline void onThreadCreated(const std::thread::id& threadID);
    inline RenderAPI getRenderAPI() const;

    static void setFrameInterpolationFactor(const D64 interpolation) noexcept { s_interpolationFactor = interpolation; }
    static D64 getFrameInterpolationFactor() noexcept { return s_interpolationFactor; }
    static void setGPUVendor(GPUVendor gpuvendor) noexcept { s_GPUVendor = gpuvendor; }
    static GPUVendor getGPUVendor() noexcept { return s_GPUVendor; }
    static void setGPURenderer(GPURenderer gpurenderer) noexcept { s_GPURenderer = gpurenderer; }
    static GPURenderer getGPURenderer() noexcept { return s_GPURenderer; }

public:
    std::mutex&       objectArenaMutex();
    ObjectArena&      objectArena();

    /// Create and return a new immediate mode emulation primitive.
    IMPrimitive*       newIMP();
    bool               destroyIMP(IMPrimitive*& primitive);

    /// Create and return a new vertex array (VAO + VB + IB).
    VertexBuffer*      newVB();
    /// Create and return a new pixel buffer using the requested format.
    PixelBuffer*       newPB(PBType type = PBType::PB_TEXTURE_2D, const char* name = nullptr);
    /// Create and return a new generic vertex data object
    GenericVertexData* newGVD(const U32 ringBufferLength, const char* name = nullptr);
    /// Create and return a new texture.
    Texture*           newTexture(size_t descriptorHash,
                                  const Str128& resourceName,
                                  const stringImpl& assetNames,
                                  const stringImpl& assetLocations,
                                  bool isFlipped,
                                  bool asyncLoad,
                                  const TextureDescriptor& texDescriptor);

    /// Create and return a new shader program.
    ShaderProgram*     newShaderProgram(size_t descriptorHash,
                                        const Str128& resourceName,
                                        const Str128& assetName,
                                        const stringImpl& assetLocation,
                                        const ShaderProgramDescriptor& descriptor,
                                        bool asyncLoad);
    /// Create and return a new shader buffer. 
    /// The OpenGL implementation creates either an 'Uniform Buffer Object' if unbound is false
    /// or a 'Shader Storage Block Object' otherwise
    /// The shader buffer can also be persistently mapped, if requested
    ShaderBuffer*      newSB(const ShaderBufferDescriptor& descriptor);
    /// Create and return a new graphics pipeline. This is only used for caching and doesn't use the object arena
    Pipeline*          newPipeline(const PipelineDescriptor& descriptor);

    // Shortcuts
    void drawText(const GFX::DrawTextCommand& cmd, GFX::CommandBuffer& bufferInOut) const;
    void drawText(const TextElementBatch& batch, GFX::CommandBuffer& bufferInOut) const;

    // Render the texture using a custom viewport
    void drawTextureInViewport(TextureData data, const Rect<I32>& viewport, bool convertToSrgb, GFX::CommandBuffer& bufferInOut);

    void blurTarget(RenderTargetHandle& blurSource, 
                    RenderTargetHandle& blurTargetH,
                    RenderTargetHandle& blurTargetV,
                    RTAttachmentType att,
                    U8 index,
                    I32 kernelSize,
                    GFX::CommandBuffer& bufferInOut);

protected:
    /// Create and return a new framebuffer.
    RenderTarget* newRT(const RenderTargetDescriptor& descriptor);

    void drawDebugFrustum(const mat4<F32>& viewMatrix, GFX::CommandBuffer& bufferInOut);

    void drawText(const TextElementBatch& batch);

    void fitViewportInWindow(U16 w, U16 h);

    void onSizeChange(const SizeChangeParams& params);

    void renderDebugViews(const Rect<I32>& targetViewport, const I32 padding, GFX::CommandBuffer& bufferInOut);
    
    void stepResolution(bool increment);

protected:
    void renderDebugUI(const Rect<I32>& targetViewport, GFX::CommandBuffer& bufferInOut);

protected:
    friend class SceneManager;
    friend class RenderPass;
    friend class RenderPassManager;

    void occlusionCull(const RenderPass::BufferData& bufferData,
                       const Texture_ptr& depthBuffer,
                       const Camera& camera,
                       GFX::CommandBuffer& bufferInOut);

    // Returns the HiZ texture that can be sent directly to occlusionCull
    const Texture_ptr& constructHIZ(RenderTargetID depthBuffer, RenderTargetID HiZTarget, GFX::CommandBuffer& cmdBufferInOut);

    void updateCullCount(const RenderPass::BufferData& bufferData, GFX::CommandBuffer& cmdBufferInOut);

    RenderAPIWrapper& getAPIImpl() noexcept { return *_api; }
    const RenderAPIWrapper& getAPIImpl() const noexcept { return *_api; }

private:
    /// Upload draw related data to the GPU (view & projection matrices, viewport settings, etc)
    void uploadGPUBlock();
    void setClipPlanes(const FrustumClipPlanes& clipPlanes);
    void renderFromCamera(const CameraSnapshot& cameraSnapshot);

    ErrorCode createAPIInstance(RenderAPI api);

private:
    std::unique_ptr<RenderAPIWrapper> _api = nullptr;
    std::unique_ptr<Renderer> _renderer = nullptr;

    ShaderComputeQueue* _shaderComputeQueue = nullptr;

    vector<Line> _axisLines;
    IMPrimitive* _axisGizmo = nullptr;
    vector<Line> _axisLinesTransformed;

    const Frustum*  _debugFrustum = nullptr;
    IMPrimitive*    _debugFrustumPrimitive = nullptr;

    CameraSnapshot  _activeCameraSnapshot;

    GPUState _state;
    GFXRTPool* _rtPool = nullptr;

    std::pair<vec2<U16>, bool> _resolutionChangeQueued;

    Texture_ptr _prevDepthBuffer = nullptr;
    size_t _defaultStateBlockHash = 0;
    /// The default render state buth with depth testing disabled
    size_t _defaultStateNoDepthHash = 0;
    /// Special render state for 2D rendering
    size_t _state2DRenderingHash = 0;
    size_t _stateDepthOnlyRenderingHash = 0;
    /// The interpolation factor between the current and the last frame
    FrustumClipPlanes _clippingPlanes;

    // number of draw calls (rough estimate)
    I32 FRAME_DRAW_CALLS = 0;
    U32 FRAME_DRAW_CALLS_PREV = 0u;
    U32 FRAME_COUNT = 0u;

    mutable bool _queueCullRead = false;
    U32 LAST_CULL_COUNT = 0;

    /// shader used to preview the depth buffer
    ShaderProgram_ptr _previewDepthMapShader = nullptr;
    ShaderProgram_ptr _previewRenderTargetColour = nullptr;
    ShaderProgram_ptr _previewRenderTargetDepth = nullptr;
    ShaderProgram_ptr _renderTargetDraw = nullptr;
    ShaderProgram_ptr _HIZConstructProgram = nullptr;
    ShaderProgram_ptr _HIZCullProgram = nullptr;
    ShaderProgram_ptr _displayShader = nullptr;
    ShaderProgram_ptr _textRenderShader = nullptr;
    ShaderProgram_ptr _blurShader = nullptr;
    
    Pipeline* _HIZPipeline = nullptr;
    Pipeline* _HIZCullPipeline = nullptr;
    Pipeline* _DrawFSTexturePipeline = nullptr;
    Pipeline* _AxisGizmoPipeline = nullptr;
    Pipeline* _BlurVPipeline = nullptr;
    Pipeline* _BlurHPipeline = nullptr;

    U32 _horizBlur = 0u;
    U32 _vertBlur = 0u;

    PushConstants _textRenderConstants;
    Pipeline* _textRenderPipeline = nullptr;
        
    std::mutex _graphicsResourceMutex;
    vector<std::tuple<GraphicsResource::Type, I64, U64>> _graphicResources;

    Rect<I32> _viewport;
    vec2<U16> _renderingResolution;

    GFXShaderData _gpuBlock;

    std::array<U32, to_base(RenderStage::COUNT) - 1> _lastCommandCount;
    std::array<U32, to_base(RenderStage::COUNT) - 1> _lastNodeCount;

    std::mutex _debugViewLock;
    bool _debugViewsEnabled = false;
    vector<DebugView_ptr> _debugViews;
    
    ShaderBuffer* _gfxDataBuffer = nullptr;
    GenericDrawCommand _defaultDrawCmd;

    MemoryPool<GenericDrawCommand> _commandPool;

    std::mutex _descriptorSetPoolLock;
    DescriptorSetPool _descriptorSetPool;
    
    std::mutex _pipelineCacheLock;
    hashMap<size_t, Pipeline, NoHash<size_t>> _pipelineCache;
    std::shared_ptr<RenderDocManager> _renderDocManager = nullptr;

    std::stack<CameraSnapshot> _cameraSnapshots;

    std::mutex _gpuObjectArenaMutex;
    std::mutex _imprimitiveMutex;

    ObjectArena _gpuObjectArena;

    static D64 s_interpolationFactor;
    static GPUVendor s_GPUVendor;
    static GPURenderer s_GPURenderer;
};

namespace Attorney {
    class GFXDeviceGUI {
    private:
        static void drawText(const GFXDevice& device, const GFX::DrawTextCommand& cmd, GFX::CommandBuffer& bufferInOut) {
            return device.drawText(cmd, bufferInOut);
        }

        static void drawText(const GFXDevice& device, const TextElementBatch& batch, GFX::CommandBuffer& bufferInOut) {
            return device.drawText(batch, bufferInOut);
        }
        friend class Divide::GUI;
        friend class Divide::GUIText;
        friend class Divide::SceneGUIElements;
    };

    class GFXDeviceKernel {
    private:
        static void onSizeChange(GFXDevice& device, const SizeChangeParams& params) {
            device.onSizeChange(params);
        }
        
        friend class Divide::Kernel;
        friend class Divide::Attorney::KernelApplication;
    };

    class GFXDeviceGraphicsResource {
       private:
       static void onResourceCreate(GFXDevice& device, GraphicsResource::Type type, I64 GUID, U64 nameHash) {
           UniqueLock w_lock(device._graphicsResourceMutex);
           device._graphicResources.emplace_back(type, GUID, nameHash);
       }

       static void onResourceDestroy(GFXDevice& device, GraphicsResource::Type type, I64 GUID, U64 nameHash) {
           UniqueLock w_lock(device._graphicsResourceMutex);
           vector<std::tuple<GraphicsResource::Type, I64, U64>>::iterator it;
           it = std::find_if(std::begin(device._graphicResources),
                std::end(device._graphicResources),
                [type, GUID, nameHash](const std::tuple<GraphicsResource::Type, I64, U64> crtEntry) noexcept -> bool {
                    if (std::get<1>(crtEntry) == GUID) {
                        assert(std::get<0>(crtEntry) == type && std::get<2>(crtEntry) == nameHash);
                        return true;
                    }
                    return false;
                });
           assert(it != std::cend(device._graphicResources));
           device._graphicResources.erase(it);
   
       }
       friend class Divide::GraphicsResource;
    };

    class GFXDeviceGFXRTPool {
        private:
        static RenderTarget* newRT(GFXDevice& device, const RenderTargetDescriptor& descriptor) {
            return device.newRT(descriptor);
        };

        friend class Divide::GFXRTPool;
    };
};  // namespace Attorney
};  // namespace Divide

#include "GFXDevice.inl"

#endif
