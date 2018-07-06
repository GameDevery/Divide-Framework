#include "stdafx.h"

#include "config.h"

#include "Headers/GFXDevice.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/Application.h"
#include "Core/Headers/ParamHandler.h"
#include "Core/Headers/StringHelper.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Resources/Headers/ResourceCache.h"

#include "Rendering/Headers/Renderer.h"
#include "Rendering/Camera/Headers/Camera.h"
#include "Rendering/PostFX/Headers/PostFX.h"
#include "Rendering/Headers/EnvironmentProbe.h"

#include "Managers/Headers/RenderPassManager.h"

#include "Platform/Video/Headers/IMPrimitive.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Platform/Video/Textures/Headers/Texture.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"
#include "Platform/Video/Buffers/RenderTarget/Headers/RenderTarget.h"
#include "Platform/Video/Buffers/ShaderBuffer/Headers/ShaderBuffer.h"

#include "Geometry/Material/Headers/ShaderComputeQueue.h"

#include "Platform/Video/OpenGL/Headers/GLWrapper.h"
#include "Platform/Video/Direct3D/Headers/DXWrapper.h"

#include "RenderDoc-Manager/RenderDocManager.h"

#include <AntTweakBar/include/AntTweakBar.h>

namespace Divide {

/// Create a display context using the selected API and create all of the needed
/// primitives needed for frame rendering
ErrorCode GFXDevice::initRenderingAPI(I32 argc, char** argv, const vec2<U16>& renderResolution) {
    ErrorCode hardwareState = createAPIInstance();
    _config._enableDebugMsgGroups = Util::findCommandLineArgument(argc, argv, "enableGPUMessageGroups");
    Configuration& config = _parent.platformContext().config();

    if (hardwareState == ErrorCode::NO_ERR) {
        // Initialize the rendering API
        if (Config::ENABLE_GPU_VALIDATION) {
           //_renderDocManager = 
           //   std::make_shared<RenderDocManager>(const_sysInfo()._windowHandle,
           //                                      ".\\RenderDoc\\renderdoc.dll",
           //                                      L"\\RenderDoc\\Captures\\");
        }
        hardwareState = _api->initRenderingAPI(argc, argv, config);
    }

    if (hardwareState != ErrorCode::NO_ERR) {
        // Validate initialization
        return hardwareState;
    }

    stringImpl refreshRates;
    vectorAlg::vecSize displayCount = gpuState().getDisplayCount();
    for (vectorAlg::vecSize idx = 0; idx < displayCount; ++idx) {
        const vectorImpl<GPUState::GPUVideoMode>& registeredModes = gpuState().getDisplayModes(idx);
        Console::printfn(Locale::get(_ID("AVAILABLE_VIDEO_MODES")), idx, registeredModes.size());

        for (const GPUState::GPUVideoMode& mode : registeredModes) {
            // Optionally, output to console/file each display mode
            refreshRates = Util::StringFormat("%d", mode._refreshRate.front());
            vectorAlg::vecSize refreshRateCount = mode._refreshRate.size();
            for (vectorAlg::vecSize i = 1; i < refreshRateCount; ++i) {
                refreshRates += Util::StringFormat(", %d", mode._refreshRate[i]);
            }
            Console::printfn(Locale::get(_ID("CURRENT_DISPLAY_MODE")),
                mode._resolution.width,
                mode._resolution.height,
                mode._bitDepth,
                mode._formatName.c_str(),
                refreshRates.c_str());
        }
    }

    ResourceCache& cache = parent().resourceCache();
    _rtPool = MemoryManager_NEW GFXRTPool(*this);

    // Initialize the shader manager
    ShaderProgram::onStartup(*this, cache);
    EnvironmentProbe::onStartup(*this);
    PostFX::createInstance();
    // Create a shader buffer to store the GFX rendering info (matrices, options, etc)
    ShaderBufferDescriptor bufferDescriptor;
    bufferDescriptor._primitiveCount = 1;
    bufferDescriptor._primitiveSizeInBytes = sizeof(GFXShaderData::GPUData);
    bufferDescriptor._ringBufferLength = 1;
    bufferDescriptor._unbound = false;
    bufferDescriptor._updateFrequency = BufferUpdateFrequency::OFTEN;
    bufferDescriptor._initialData = &_gpuBlock._data;
    _gfxDataBuffer = newSB(bufferDescriptor);

    _shaderComputeQueue = MemoryManager_NEW ShaderComputeQueue(cache);

    // Create general purpose render state blocks
    RenderStateBlock::init();
    RenderStateBlock defaultState;
    _defaultStateBlockHash = defaultState.getHash();

    RenderStateBlock defaultStateNoDepth;
    defaultStateNoDepth.setZRead(false);
    _defaultStateNoDepthHash = defaultStateNoDepth.getHash();

    RenderStateBlock state2DRendering;
    state2DRendering.setCullMode(CullMode::NONE);
    state2DRendering.setZRead(false);
    _state2DRenderingHash = state2DRendering.getHash();

    RenderStateBlock stateDepthOnlyRendering;
    stateDepthOnlyRendering.setColourWrites(false, false, false, false);
    stateDepthOnlyRendering.setZFunc(ComparisonFunction::ALWAYS);
    _stateDepthOnlyRenderingHash = stateDepthOnlyRendering.getHash();

    // The general purpose render state blocks are both mandatory and must
    // differ from each other at a state hash level
    assert(_stateDepthOnlyRenderingHash != _state2DRenderingHash &&
           "GFXDevice error: Invalid default state hash detected!");
    assert(_state2DRenderingHash != _defaultStateNoDepthHash &&
           "GFXDevice error: Invalid default state hash detected!");
    assert(_defaultStateNoDepthHash != _defaultStateBlockHash &&
           "GFXDevice error: Invalid default state hash detected!");
    // Activate the default render states
    _api->setStateBlock(_defaultStateBlockHash);

    // We need to create all of our attachments for the default render targets
    // Start with the screen render target: Try a half float, multisampled
    // buffer (MSAA + HDR rendering if possible)
    TextureDescriptor screenDescriptor(TextureType::TEXTURE_2D_MS,
                                       GFXImageFormat::RGBA16F,
                                       GFXDataFormat::FLOAT_16);

    TextureDescriptor hiZDescriptor(TextureType::TEXTURE_2D_MS,
                                    GFXImageFormat::DEPTH_COMPONENT32F,
                                    GFXDataFormat::FLOAT_32);

    SamplerDescriptor hiZSampler;
    hiZSampler.setFilters(TextureFilter::NEAREST_MIPMAP_NEAREST);
    hiZSampler.setWrapMode(TextureWrap::CLAMP_TO_EDGE);

    hiZDescriptor.setSampler(hiZSampler);
    hiZDescriptor.toggleAutomaticMipMapGeneration(false);

    SamplerDescriptor screenSampler;
    screenSampler.setFilters(TextureFilter::NEAREST);
    screenSampler.setWrapMode(TextureWrap::CLAMP_TO_EDGE);
    screenDescriptor.setSampler(screenSampler);

    TextureDescriptor normalDescriptor(TextureType::TEXTURE_2D,
                                       GFXImageFormat::RG16F,
                                       GFXDataFormat::FLOAT_16);
    normalDescriptor.setSampler(screenSampler);
    
    TextureDescriptor velocityDescriptor(TextureType::TEXTURE_2D,
                                         GFXImageFormat::RG16F,
                                         GFXDataFormat::FLOAT_16);
    velocityDescriptor.setSampler(screenSampler);
    
    {
        vectorImpl<RTAttachmentDescriptor> attachments = {
            { screenDescriptor,   RTAttachmentType::Colour, to_U8(ScreenTargets::ALBEDO), DefaultColours::DIVIDE_BLUE() },
            { normalDescriptor,   RTAttachmentType::Colour, to_U8(ScreenTargets::NORMALS) },
            { velocityDescriptor, RTAttachmentType::Colour, to_U8(ScreenTargets::VELOCITY) },
            { hiZDescriptor,      RTAttachmentType::Depth }
        };

        RenderTargetDescriptor screenDesc = {};
        screenDesc._name = "Screen";
        screenDesc._resolution = renderResolution;
        screenDesc._attachmentCount = to_U8(attachments.size());
        screenDesc._attachments = attachments.data();

        // Our default render targets hold the screen buffer, depth buffer, and a special, on demand,
        // down-sampled version of the depth buffer
        // Screen FB should use MSAA if available
        _rtPool->allocateRT(RenderTargetUsage::SCREEN, screenDesc);
    }
    {
        TextureDescriptor accumulationDescriptor(TextureType::TEXTURE_2D,
                                                 GFXImageFormat::RGBA16F,
                                                 GFXDataFormat::FLOAT_16);
        accumulationDescriptor.toggleAutomaticMipMapGeneration(false);

        SamplerDescriptor accumulationSampler;
        accumulationSampler.setFilters(TextureFilter::NEAREST);
        accumulationSampler.setWrapMode(TextureWrap::CLAMP_TO_EDGE);
        accumulationDescriptor.setSampler(accumulationSampler);

        TextureDescriptor revealageDescriptor(TextureType::TEXTURE_2D,
                                              GFXImageFormat::RED8,
                                              GFXDataFormat::UNSIGNED_BYTE);
        revealageDescriptor.setSampler(accumulationSampler);

        vectorImpl<RTAttachmentDescriptor> attachments = {
            { accumulationDescriptor, RTAttachmentType::Colour, to_U8(ScreenTargets::ACCUMULATION), VECTOR4_ZERO },
            { revealageDescriptor, RTAttachmentType::Colour, to_U8(ScreenTargets::REVEALAGE), VECTOR4_UNIT }
        };

        RenderTargetDescriptor oitDesc = {};
        oitDesc._name = "OIT";
        oitDesc._resolution = renderResolution;
        oitDesc._attachmentCount = to_U8(attachments.size());
        oitDesc._attachments = attachments.data();

        _rtPool->allocateRT(RenderTargetUsage::OIT, oitDesc);
    }
    // Reflection Targets
    SamplerDescriptor reflectionSampler;
    reflectionSampler.setFilters(TextureFilter::NEAREST);
    reflectionSampler.setWrapMode(TextureWrap::CLAMP_TO_EDGE);

    TextureDescriptor environmentDescriptorCube(TextureType::TEXTURE_CUBE_ARRAY,
                                                GFXImageFormat::RGBA8,
                                                GFXDataFormat::UNSIGNED_BYTE);
    environmentDescriptorCube.setSampler(reflectionSampler);

    TextureDescriptor depthDescriptorCube(TextureType::TEXTURE_CUBE_ARRAY,
                                          GFXImageFormat::DEPTH_COMPONENT32F,
                                          GFXDataFormat::FLOAT_32);

    depthDescriptorCube.setSampler(reflectionSampler);

    TextureDescriptor environmentDescriptorPlanar(TextureType::TEXTURE_2D,
                                                  GFXImageFormat::RGBA8,
                                                  GFXDataFormat::UNSIGNED_BYTE);
    environmentDescriptorPlanar.setSampler(reflectionSampler);

    TextureDescriptor depthDescriptorPlanar(TextureType::TEXTURE_2D,
                                            GFXImageFormat::DEPTH_COMPONENT32F,
                                            GFXDataFormat::FLOAT_32);
    depthDescriptorPlanar.setSampler(reflectionSampler);

    RenderTargetHandle tempHandle;
    U16 reflectRes = std::max(renderResolution.width, renderResolution.height) / Config::REFLECTION_TARGET_RESOLUTION_DOWNSCALE_FACTOR;
    {
        vectorImpl<RTAttachmentDescriptor> attachments = {
            { environmentDescriptorPlanar, RTAttachmentType::Colour },
            { depthDescriptorPlanar, RTAttachmentType::Depth },
        };

        RenderTargetDescriptor refDesc = {};
        refDesc._resolution = vec2<U16>(reflectRes);
        refDesc._attachmentCount = to_U8(attachments.size());
        refDesc._attachments = attachments.data();

        for (U32 i = 0; i < Config::MAX_REFLECTIVE_NODES_IN_VIEW; ++i) {
            refDesc._name = Util::StringFormat("Reflection_Planar_%d", i);

            tempHandle = _rtPool->allocateRT(RenderTargetUsage::REFLECTION_PLANAR, refDesc);
        }

        for (U32 i = 0; i < Config::MAX_REFRACTIVE_NODES_IN_VIEW; ++i) {
            refDesc._name = Util::StringFormat("Refraction_Planar_%d", i);

            tempHandle = _rtPool->allocateRT(RenderTargetUsage::REFRACTION_PLANAR, refDesc);
        }

    }
    {
        vectorImpl<RTAttachmentDescriptor> attachments = {
            { environmentDescriptorCube, RTAttachmentType::Colour },
            { depthDescriptorCube, RTAttachmentType::Depth },
        };

        RenderTargetDescriptor refDesc = {};
        refDesc._resolution = vec2<U16>(reflectRes);
        refDesc._attachmentCount = to_U8(attachments.size());
        refDesc._attachments = attachments.data();

        refDesc._name = "Reflection_Cube_Array";
        tempHandle = _rtPool->allocateRT(RenderTargetUsage::REFLECTION_CUBE, refDesc);

        refDesc._name = "Refraction_Cube_Array";

        tempHandle = _rtPool->allocateRT(RenderTargetUsage::REFRACTION_CUBE, refDesc);
    }

    // Initialized our HierarchicalZ construction shader (takes a depth
    // attachment and down-samples it for every mip level)
    _HIZConstructProgram = CreateResource<ShaderProgram>(cache, ResourceDescriptor("HiZConstruct"));
    _HIZCullProgram = CreateResource<ShaderProgram>(cache, ResourceDescriptor("HiZOcclusionCull"));
    _displayShader = CreateResource<ShaderProgram>(cache, ResourceDescriptor("display"));

    PostFX& postFX = PostFX::instance();

    // Register a 2D function used for previewing the depth buffer.
    if (Config::Build::IS_DEBUG_BUILD) {
        add2DRenderFunction(GUID_DELEGATE_CBK([this]() { renderDebugViews(); }), 0);
    }
    
    ParamHandler::instance().setParam<bool>(_ID("rendering.previewDebugViews"), false);
    // If render targets ready, we initialize our post processing system
    postFX.init(*this, cache);
    if (config.rendering.postAASamples > 0) {
        postFX.pushFilter(FilterType::FILTER_SS_ANTIALIASING);
    }
    if (false) {
        postFX.pushFilter(FilterType::FILTER_SS_REFLECTIONS);
    }
    if (config.rendering.enableSSAO) {
        postFX.pushFilter(FilterType::FILTER_SS_AMBIENT_OCCLUSION);
    }
    if (config.rendering.enableDepthOfField) {
        postFX.pushFilter(FilterType::FILTER_DEPTH_OF_FIELD);
    }
    if (false) {
        postFX.pushFilter(FilterType::FILTER_MOTION_BLUR);
    }
    if (config.rendering.enableBloom) {
        postFX.pushFilter(FilterType::FILTER_BLOOM);
    }
    if (false) {
        postFX.pushFilter(FilterType::FILTER_LUT_CORECTION);
    }

    PipelineDescriptor pipelineDesc;

    _axisGizmo = newIMP();
    _axisGizmo->name("GFXDeviceAxisGizmo");
    RenderStateBlock primitiveDescriptor(RenderStateBlock::get(getDefaultStateBlock(true)));
    pipelineDesc._stateHash = primitiveDescriptor.getHash();
    Pipeline primitivePipeline = newPipeline(pipelineDesc);
    _axisGizmo->pipeline(primitivePipeline);

    _debugFrustumPrimitive = newIMP();
    _debugFrustumPrimitive->name("DebugFrustum");
    _debugFrustumPrimitive->pipeline(primitivePipeline);

    ResourceDescriptor previewNormalsShader("fbPreview");
    previewNormalsShader.setThreadedLoading(false);
    _renderTargetDraw = CreateResource<ShaderProgram>(cache, previewNormalsShader);
    assert(_renderTargetDraw != nullptr);

    ResourceDescriptor immediateModeShader("ImmediateModeEmulation.GUI");
    immediateModeShader.setThreadedLoading(false);
    _textRenderShader = CreateResource<ShaderProgram>(cache, immediateModeShader);
    assert(_textRenderShader != nullptr);
    _textRenderConstants.set("dvd_WorldMatrix", PushConstantType::MAT4, mat4<F32>());
    PipelineDescriptor descriptor;
    descriptor._shaderProgram = _textRenderShader;
    descriptor._stateHash = _defaultStateNoDepthHash;
    _textRenderPipeline = newPipeline(descriptor);


    // Create initial buffers, cameras etc for this resolution. It should match window size
    WindowManager& winMgr = _parent.platformContext().app().windowManager();
    winMgr.handleWindowEvent(WindowEvent::RESOLUTION_CHANGED,
                             winMgr.getActiveWindow().getGUID(),
                             to_I32(renderResolution.width),
                             to_I32(renderResolution.height));
    setBaseViewport(vec4<I32>(0, 0, to_I32(renderResolution.width), to_I32(renderResolution.height)));


    for (RenderPackageQueue& queue : _renderQueues) {
        queue.reserve(Config::MAX_VISIBLE_NODES);
    }
    // Everything is ready from the rendering point of view
    return ErrorCode::NO_ERR;
}

/// Revert everything that was set up in initRenderingAPI()
void GFXDevice::closeRenderingAPI() {
    assert(_api != nullptr && "GFXDevice error: closeRenderingAPI called without init!");
    _axisGizmo->clear();
    _debugFrustumPrimitive->clear();
    _debugViews.clear();
    // Destroy our post processing system
    Console::printfn(Locale::get(_ID("STOP_POST_FX")));
    PostFX::destroyInstance();
    // Delete the renderer implementation
    Console::printfn(Locale::get(_ID("CLOSING_RENDERER")));
    RenderStateBlock::clear();

    EnvironmentProbe::onShutdown(*this);
    MemoryManager::DELETE(_rtPool);

    _previewDepthMapShader = nullptr;
    _renderTargetDraw = nullptr;
    _HIZConstructProgram = nullptr;
    _HIZCullProgram = nullptr;
    _displayShader = nullptr;
    _textRenderShader = nullptr;

    MemoryManager::DELETE(_renderer);

    // Close the shader manager
    MemoryManager::DELETE(_shaderComputeQueue);
    ShaderProgram::onShutdown();
    _gpuObjectArena.clear();
    // Close the rendering API
    _api->closeRenderingAPI();
    _api.reset();
    if (!_graphicResources.empty()) {
        Console::errorfn(Locale::get(_ID("ERROR_GFX_LEAKED_RESOURCES")), _graphicResources.size());
    }

    if (Config::USE_ANT_TWEAK_BAR) {
        TwTerminate();
    }
}

/// After a swap buffer call, the CPU may be idle waiting for the GPU to draw to
/// the screen, so we try to do some processing
void GFXDevice::idle() {
    static const Task idleTask;
    _shaderComputeQueue->idle();
    // Pass the idle call to the post processing system
    PostFX::instance().idle(_parent.platformContext().config());
    // And to the shader manager
    ShaderProgram::idle();

    UpgradableReadLock r_lock(_GFXLoadQueueLock);
    if (!_GFXLoadQueue.empty()) {
        for(DELEGATE_CBK<void, const Task&>& cbk : _GFXLoadQueue) {
            cbk(idleTask);
        }
        UpgradeToWriteLock w_lock(r_lock);
        _GFXLoadQueue.clear();
    }
}

void GFXDevice::beginFrame() {
    if (Config::ENABLE_GPU_VALIDATION) {
        if (_renderDocManager) {
            _renderDocManager->StartFrameCapture();
        }
    }

    _api->beginFrame();
    _api->setStateBlock(_defaultStateBlockHash);
    setViewport(_baseViewport);
}

void GFXDevice::endFrame() {
    FRAME_COUNT++;
    FRAME_DRAW_CALLS_PREV = FRAME_DRAW_CALLS;
    FRAME_DRAW_CALLS = 0;
    
    // Activate the default render states
    _api->setStateBlock(_defaultStateBlockHash);

    if (Config::USE_ANT_TWEAK_BAR) {
        if (_parent.platformContext().config().gui.enableDebugVariableControls) {
            TwDraw();
        }
    }

    _api->endFrame();


    if (Config::ENABLE_GPU_VALIDATION) {
        if (_renderDocManager) {
            _renderDocManager->EndFrameCapture();
        }
    }
}

void GFXDevice::resizeHistory(U8 historySize) {
    while (_prevDepthBuffers.size() > historySize) {
        _prevDepthBuffers.pop_back();
    }

    while (_prevDepthBuffers.size() < historySize) {
        const Texture_ptr& src = _rtPool->renderTarget(RenderTargetID(RenderTargetUsage::SCREEN)).getAttachment(RTAttachmentType::Depth, 0).texture();

        ResourceDescriptor prevDepthTex(Util::StringFormat("PREV_DEPTH_%d", _prevDepthBuffers.size()));
        prevDepthTex.setPropertyDescriptor(src->getDescriptor());
        
        Texture_ptr tex = CreateResource<Texture>(parent().resourceCache(), prevDepthTex);
        assert(tex);
        Texture::TextureLoadInfo info;
        
        tex->loadData(info, NULL, vec2<U16>(src->getWidth(), src->getHeight()));

        _prevDepthBuffers.push_back(tex);
    }
}

void GFXDevice::historyIndex(U8 index, bool copyPrevious) {
    if (copyPrevious && index < _historyIndex) {
        getPrevDepthBuffer()->copy(_rtPool->renderTarget(RenderTargetID(RenderTargetUsage::SCREEN)).getAttachment(RTAttachmentType::Depth, 0).texture());
    }

    _historyIndex = index;
}

ErrorCode GFXDevice::createAPIInstance() {
    assert(_api == nullptr && "GFXDevice error: initRenderingAPI called twice!");

    ErrorCode err = ErrorCode::NO_ERR;
    switch (_API_ID) {
        case RenderAPI::OpenGL:
        case RenderAPI::OpenGLES: {
            _api = std::make_unique<GL_API>(*this);
        } break;
        case RenderAPI::Direct3D: {
            _api = std::make_unique<DX_API>(*this);
        } break;

        default:
        case RenderAPI::None:
        case RenderAPI::Vulkan: {
            err = ErrorCode::GFX_NON_SPECIFIED;
        } break;
    };

    DIVIDE_ASSERT(_api != nullptr, Locale::get(_ID("ERROR_GFX_DEVICE_API")));
    return err;
}

};