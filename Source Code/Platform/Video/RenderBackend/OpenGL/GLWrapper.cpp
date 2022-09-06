﻿#include "stdafx.h"

#include "Headers/GLWrapper.h"

#include "Platform/Headers/PlatformRuntime.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/GFXRTPool.h"
#include "Platform/File/Headers/FileManagement.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/Configuration.h"
#include "Core/Time/Headers/ProfileTimer.h"

#include "Utility/Headers/Localization.h"

#include "GUI/Headers/GUI.h"

#include "CEGUIOpenGLRenderer/include/Texture.h"

#include "Platform/Video/Headers/DescriptorSets.h"
#include "Platform/Video/Textures/Headers/SamplerDescriptor.h"
#include "Platform/Video/RenderBackend/OpenGL/CEGUIOpenGLRenderer/include/GL3Renderer.h"

#include "Platform/Video/RenderBackend/OpenGL/Shaders/Headers/glShaderProgram.h"

#include "Platform/Video/RenderBackend/OpenGL/Textures/Headers/glTexture.h"
#include "Platform/Video/RenderBackend/OpenGL/Textures/Headers/glSamplerObject.h"

#include "Platform/Video/RenderBackend/OpenGL/Buffers/Headers/glBufferImpl.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/Headers/glShaderBuffer.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/Headers/glFramebuffer.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/Headers/glGenericVertexData.h"

#include "Platform/Video/GLIM/glim.h"

#ifndef GLFONTSTASH_IMPLEMENTATION
#define GLFONTSTASH_IMPLEMENTATION
#define FONTSTASH_IMPLEMENTATION
#include "Headers/fontstash.h"
#include "Headers/glfontstash.h"
#endif

#include <glbinding-aux/Meta.h>
#include <glbinding-aux/ContextInfo.h>
#include <glbinding/Binding.h>

namespace Divide {

namespace {
    // Weird stuff happens if this is enabled (i.e. certain draw calls hang forever)
    constexpr bool g_runAllQueriesInSameFrame = false;
}

GLStateTracker_uptr GL_API::s_stateTracker = nullptr;
GL_API::VAOMap GL_API::s_vaoCache;
std::atomic_bool GL_API::s_glFlushQueued;
GLUtil::glTextureViewCache GL_API::s_textureViewCache{};
U32 GL_API::s_fenceSyncCounter[GL_API::s_LockFrameLifetime]{};
SharedMutex GL_API::s_samplerMapLock;
GL_API::SamplerObjectMap GL_API::s_samplerMap{};
glHardwareQueryPool* GL_API::s_hardwareQueryPool = nullptr;
eastl::fixed_vector<GL_API::TexBindEntry, GLStateTracker::MAX_BOUND_TEXTURE_UNITS, false> GL_API::s_TexBindQueue;

std::array<GLUtil::GLMemory::DeviceAllocator, to_base(GLUtil::GLMemory::GLMemoryType::COUNT)> GL_API::s_memoryAllocators = {
    GLUtil::GLMemory::DeviceAllocator(GLUtil::GLMemory::GLMemoryType::SHADER_BUFFER),
    GLUtil::GLMemory::DeviceAllocator(GLUtil::GLMemory::GLMemoryType::VERTEX_BUFFER),
    GLUtil::GLMemory::DeviceAllocator(GLUtil::GLMemory::GLMemoryType::INDEX_BUFFER),
    GLUtil::GLMemory::DeviceAllocator(GLUtil::GLMemory::GLMemoryType::OTHER)
};

#define TO_MEGABYTES(X) (X * 1024u * 1024u)
std::array<size_t, to_base(GLUtil::GLMemory::GLMemoryType::COUNT)> GL_API::s_memoryAllocatorSizes {
    TO_MEGABYTES(512),
    TO_MEGABYTES(1024),
    TO_MEGABYTES(256),
    TO_MEGABYTES(256)
};

namespace {
    struct SDLContextEntry {
        SDL_GLContext _context = nullptr;
        bool _inUse = false;
    };

    struct ContextPool {
        bool init(const size_t size, const DisplayWindow& window) {
            SDL_Window* raw = window.getRawWindow();
            _contexts.resize(size);
            for (SDLContextEntry& contextEntry : _contexts) {
                contextEntry._context = SDL_GL_CreateContext(raw);
            }
            return true;
        }

        bool destroy() noexcept {
            for (const SDLContextEntry& contextEntry : _contexts) {
                SDL_GL_DeleteContext(contextEntry._context);
            }
            _contexts.clear();
            return true;
        }

        bool getAvailableContext(SDL_GLContext& ctx) noexcept {
            assert(!_contexts.empty());
            for (SDLContextEntry& contextEntry : _contexts) {
                if (!contextEntry._inUse) {
                    ctx = contextEntry._context;
                    contextEntry._inUse = true;
                    return true;
                }
            }

            return false;
        }

        vector<SDLContextEntry> _contexts;
    } g_ContextPool;
};

GL_API::GL_API(GFXDevice& context)
    : RenderAPIWrapper(),
    _context(context),
    _swapBufferTimer(Time::ADD_TIMER("Swap Buffer Timer"))
{
    std::atomic_init(&s_glFlushQueued, false);
}

/// Try and create a valid OpenGL context taking in account the specified resolution and command line arguments
ErrorCode GL_API::initRenderingAPI([[maybe_unused]] GLint argc, [[maybe_unused]] char** argv, Configuration& config) {
    // Fill our (abstract API <-> openGL) enum translation tables with proper values
    GLUtil::fillEnumTables();

    const DisplayWindow& window = *_context.context().app().windowManager().mainWindow();
    g_ContextPool.init(_context.parent().totalThreadCount(), window);

    SDL_GL_MakeCurrent(window.getRawWindow(), window.userData()->_glContext);
    GLUtil::s_glMainRenderWindow = &window;
    _currentContext._windowGUID = window.getGUID();
    _currentContext._context = window.userData()->_glContext;

    glbinding::Binding::initialize([](const char *proc) noexcept  {
                                        return (glbinding::ProcAddress)SDL_GL_GetProcAddress(proc);
                                  }, true);

    if (SDL_GL_GetCurrentContext() == nullptr) {
        return ErrorCode::GLBINGING_INIT_ERROR;
    }

    glbinding::Binding::useCurrentContext();
    
    // Query GPU vendor to enable/disable vendor specific features
    GPUVendor vendor = GPUVendor::COUNT;
    const char* gpuVendorStr = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    if (gpuVendorStr != nullptr) {
        if (strstr(gpuVendorStr, "Intel") != nullptr) {
            vendor = GPUVendor::INTEL;
        } else if (strstr(gpuVendorStr, "NVIDIA") != nullptr) {
            vendor = GPUVendor::NVIDIA;
        } else if (strstr(gpuVendorStr, "ATI") != nullptr || strstr(gpuVendorStr, "AMD") != nullptr) {
            vendor = GPUVendor::AMD;
        } else if (strstr(gpuVendorStr, "Microsoft") != nullptr) {
            vendor = GPUVendor::MICROSOFT;
        } else if (strstr(gpuVendorStr, "Mesa") != nullptr) {
            vendor = GPUVendor::MESA;
        } else {
            vendor = GPUVendor::OTHER;
        }
    } else {
        gpuVendorStr = "Unknown GPU Vendor";
        vendor = GPUVendor::OTHER;
    }
    GPURenderer renderer = GPURenderer::COUNT;
    const char* gpuRendererStr = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    if (gpuRendererStr != nullptr) {
        if (strstr(gpuRendererStr, "Tegra") || strstr(gpuRendererStr, "GeForce") || strstr(gpuRendererStr, "NV")) {
            renderer = GPURenderer::GEFORCE;
        } else if (strstr(gpuRendererStr, "PowerVR") || strstr(gpuRendererStr, "Apple")) {
            renderer = GPURenderer::POWERVR;
            vendor = GPUVendor::IMAGINATION_TECH;
        } else if (strstr(gpuRendererStr, "Mali")) {
            renderer = GPURenderer::MALI;
            vendor = GPUVendor::ARM;
        } else if (strstr(gpuRendererStr, "Adreno")) {
            renderer = GPURenderer::ADRENO;
            vendor = GPUVendor::QUALCOMM;
        } else if (strstr(gpuRendererStr, "AMD") || strstr(gpuRendererStr, "ATI")) {
            renderer = GPURenderer::RADEON;
        } else if (strstr(gpuRendererStr, "Intel")) {
            renderer = GPURenderer::INTEL;
        } else if (strstr(gpuRendererStr, "Vivante")) {
            renderer = GPURenderer::VIVANTE;
            vendor = GPUVendor::VIVANTE;
        } else if (strstr(gpuRendererStr, "VideoCore")) {
            renderer = GPURenderer::VIDEOCORE;
            vendor = GPUVendor::ALPHAMOSAIC;
        } else if (strstr(gpuRendererStr, "WebKit") || strstr(gpuRendererStr, "Mozilla") || strstr(gpuRendererStr, "ANGLE")) {
            renderer = GPURenderer::WEBGL;
            vendor = GPUVendor::WEBGL;
        } else if (strstr(gpuRendererStr, "GDI Generic")) {
            renderer = GPURenderer::GDI;
        } else if (strstr(gpuRendererStr, "Mesa")) {
            renderer = GPURenderer::SOFTWARE;
        } else {
            renderer = GPURenderer::UNKNOWN;
        }
    } else {
        gpuRendererStr = "Unknown GPU Renderer";
        renderer = GPURenderer::UNKNOWN;
    }
    // GPU info, including vendor, gpu and driver
    Console::printfn(Locale::Get(_ID("GL_VENDOR_STRING")), gpuVendorStr, gpuRendererStr, glGetString(GL_VERSION));

    DeviceInformation deviceInformation{};
    deviceInformation._vendor = vendor;
    deviceInformation._renderer = renderer;

    if (s_hardwareQueryPool == nullptr) {
        s_hardwareQueryPool = MemoryManager_NEW glHardwareQueryPool(_context);
    }

    // OpenGL has a nifty error callback system, available in every build configuration if required
    if (Config::ENABLE_GPU_VALIDATION && config.debug.enableRenderAPIDebugging) {
        // GL_DEBUG_OUTPUT_SYNCHRONOUS is essential for debugging gl commands in the IDE
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        // hard-wire our debug callback function with OpenGL's implementation
        glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_MARKER, GL_DONT_CARE, 0, NULL, GL_FALSE);
        glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_PUSH_GROUP, GL_DONT_CARE, 0, NULL, GL_FALSE);
        glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_POP_GROUP, GL_DONT_CARE, 0, NULL, GL_FALSE);
        glDebugMessageCallback((GLDEBUGPROC)GLUtil::DebugCallback, nullptr);
    }

    // If we got here, let's figure out what capabilities we have available
    // Maximum addressable texture image units in the fragment shader
    deviceInformation._maxTextureUnits = CLAMPED(GLUtil::getGLValue(GL_MAX_TEXTURE_IMAGE_UNITS), 16u, 255u);
    DIVIDE_ASSERT(deviceInformation._maxTextureUnits >= GLStateTracker::MAX_BOUND_TEXTURE_UNITS);

    GLUtil::getGLValue(GL_MAX_VERTEX_ATTRIB_BINDINGS, deviceInformation._maxVertAttributeBindings);

    deviceInformation._versionInfo._major = to_U8(GLUtil::getGLValue(GL_MAJOR_VERSION));
    deviceInformation._versionInfo._minor = to_U8(GLUtil::getGLValue(GL_MINOR_VERSION));
    Console::printfn(Locale::Get(_ID("GL_MAX_VERSION")), deviceInformation._versionInfo._major, deviceInformation._versionInfo._minor);

    if (deviceInformation._versionInfo._major < 4 || (deviceInformation._versionInfo._major == 4 && deviceInformation._versionInfo._minor < 6)) {
        Console::errorfn(Locale::Get(_ID("ERROR_OPENGL_VERSION_TO_OLD")));
        return ErrorCode::GFX_NOT_SUPPORTED;
    }

    // Maximum number of colour attachments per framebuffer
    GLUtil::getGLValue(GL_MAX_COLOR_ATTACHMENTS, deviceInformation._maxRTColourAttachments);

    s_stateTracker = eastl::make_unique<GLStateTracker>();

    glMaxShaderCompilerThreadsARB(0xFFFFFFFF);
    deviceInformation._shaderCompilerThreads = GLUtil::getGLValue(GL_MAX_SHADER_COMPILER_THREADS_ARB);
    Console::printfn(Locale::Get(_ID("GL_SHADER_THREADS")), deviceInformation._shaderCompilerThreads);

    glEnable(GL_MULTISAMPLE);
    // Line smoothing should almost always be used
    glEnable(GL_LINE_SMOOTH);

    // GL_FALSE causes a conflict here. Thanks glbinding ...
    glClampColor(GL_CLAMP_READ_COLOR, GL_NONE);

    // Cap max anisotropic level to what the hardware supports
    CLAMP(config.rendering.maxAnisotropicFilteringLevel,
          to_U8(0),
          to_U8(GLUtil::getGLValue(GL_MAX_TEXTURE_MAX_ANISOTROPY)));

    deviceInformation._maxAnisotropy = config.rendering.maxAnisotropicFilteringLevel;

    // Number of sample buffers associated with the framebuffer & MSAA sample count
    const U8 maxGLSamples = to_U8(std::min(254, GLUtil::getGLValue(GL_MAX_SAMPLES)));
    // If we do not support MSAA on a hardware level for whatever reason, override user set MSAA levels
    config.rendering.MSAASamples = std::min(config.rendering.MSAASamples, maxGLSamples);

    config.rendering.shadowMapping.csm.MSAASamples = std::min(config.rendering.shadowMapping.csm.MSAASamples, maxGLSamples);
    config.rendering.shadowMapping.spot.MSAASamples = std::min(config.rendering.shadowMapping.spot.MSAASamples, maxGLSamples);
    _context.gpuState().maxMSAASampleCount(maxGLSamples);

    // Print all of the OpenGL functionality info to the console and log
    // How many uniforms can we send to fragment shaders
    Console::printfn(Locale::Get(_ID("GL_MAX_UNIFORM")), GLUtil::getGLValue(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS));
    // How many uniforms can we send to vertex shaders
    Console::printfn(Locale::Get(_ID("GL_MAX_VERT_UNIFORM")), GLUtil::getGLValue(GL_MAX_VERTEX_UNIFORM_COMPONENTS));
    // How many uniforms can we send to vertex + fragment shaders at the same time
    Console::printfn(Locale::Get(_ID("GL_MAX_FRAG_AND_VERT_UNIFORM")), GLUtil::getGLValue(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS));
    // How many attributes can we send to a vertex shader
    deviceInformation._maxVertAttributes = GLUtil::getGLValue(GL_MAX_VERTEX_ATTRIBS);
    Console::printfn(Locale::Get(_ID("GL_MAX_VERT_ATTRIB")), deviceInformation._maxVertAttributes);
        
    // How many workgroups can we have per compute dispatch
    for (U8 i = 0u; i < 3; ++i) {
        GLUtil::getGLValue(GL_MAX_COMPUTE_WORK_GROUP_COUNT, deviceInformation._maxWorgroupCount[i], i);
        GLUtil::getGLValue(GL_MAX_COMPUTE_WORK_GROUP_SIZE,  deviceInformation._maxWorgroupSize[i], i);
    }

    deviceInformation._maxWorgroupInvocations = GLUtil::getGLValue(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS);
    deviceInformation._maxComputeSharedMemoryBytes = GLUtil::getGLValue(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE);

    Console::printfn(Locale::Get(_ID("MAX_COMPUTE_WORK_GROUP_INFO")),
                     deviceInformation._maxWorgroupCount[0], deviceInformation._maxWorgroupCount[1], deviceInformation._maxWorgroupCount[2],
                     deviceInformation._maxWorgroupSize[0],  deviceInformation._maxWorgroupSize[1],  deviceInformation._maxWorgroupSize[2],
                     deviceInformation._maxWorgroupInvocations);
    Console::printfn(Locale::Get(_ID("MAX_COMPUTE_SHARED_MEMORY_SIZE")), deviceInformation._maxComputeSharedMemoryBytes / 1024);
    
    // Maximum number of texture units we can address in shaders
    Console::printfn(Locale::Get(_ID("GL_MAX_TEX_UNITS")),
                     GLUtil::getGLValue(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS),
                     deviceInformation._maxTextureUnits);
    // Maximum number of varying components supported as outputs in the vertex shader
    deviceInformation._maxVertOutputComponents = GLUtil::getGLValue(GL_MAX_VERTEX_OUTPUT_COMPONENTS);
    Console::printfn(Locale::Get(_ID("MAX_VERTEX_OUTPUT_COMPONENTS")), deviceInformation._maxVertOutputComponents);

    // Query shading language version support
    Console::printfn(Locale::Get(_ID("GL_GLSL_SUPPORT")),
                     glGetString(GL_SHADING_LANGUAGE_VERSION));
    // In order: Maximum number of uniform buffer binding points,
    //           maximum size in basic machine units of a uniform block and
    //           minimum required alignment for uniform buffer sizes and offset
    GLUtil::getGLValue(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, deviceInformation._UBOffsetAlignmentBytes);
    GLUtil::getGLValue(GL_MAX_UNIFORM_BLOCK_SIZE, deviceInformation._UBOMaxSizeBytes);
    const bool UBOSizeOver1Mb = deviceInformation._UBOMaxSizeBytes / 1024 > 1024;
    Console::printfn(Locale::Get(_ID("GL_VK_UBO_INFO")),
                     GLUtil::getGLValue(GL_MAX_UNIFORM_BUFFER_BINDINGS),
                     (deviceInformation._UBOMaxSizeBytes / 1024) / (UBOSizeOver1Mb ? 1024 : 1),
                     UBOSizeOver1Mb ? "Mb" : "Kb",
                     deviceInformation._UBOffsetAlignmentBytes);

    // In order: Maximum number of shader storage buffer binding points,
    //           maximum size in basic machine units of a shader storage block,
    //           maximum total number of active shader storage blocks that may
    //           be accessed by all active shaders and
    //           minimum required alignment for shader storage buffer sizes and
    //           offset.
    GLUtil::getGLValue(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, deviceInformation._SSBOffsetAlignmentBytes);
    GLUtil::getGLValue(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, deviceInformation._SSBOMaxSizeBytes);
    deviceInformation._maxSSBOBufferBindings = GLUtil::getGLValue(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS);
    Console::printfn(Locale::Get(_ID("GL_VK_SSBO_INFO")),
                     deviceInformation._maxSSBOBufferBindings,
                     deviceInformation._SSBOMaxSizeBytes / 1024 / 1024,
                     GLUtil::getGLValue(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS),
                     deviceInformation._SSBOffsetAlignmentBytes);

    // Maximum number of subroutines and maximum number of subroutine uniform
    // locations usable in a shader
    Console::printfn(Locale::Get(_ID("GL_SUBROUTINE_INFO")),
                     GLUtil::getGLValue(GL_MAX_SUBROUTINES),
                     GLUtil::getGLValue(GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS));

    GLint range[2];
    GLUtil::getGLValue(GL_SMOOTH_LINE_WIDTH_RANGE, range);
    Console::printfn(Locale::Get(_ID("GL_LINE_WIDTH_INFO")), range[0], range[1]);

    const I32 clipDistanceCount = std::max(GLUtil::getGLValue(GL_MAX_CLIP_DISTANCES), 0);
    const I32 cullDistanceCount = std::max(GLUtil::getGLValue(GL_MAX_CULL_DISTANCES), 0);

    deviceInformation._maxClipAndCullDistances = GLUtil::getGLValue(GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES);
    deviceInformation._maxClipDistances = to_U32(clipDistanceCount);
    deviceInformation._maxCullDistances = to_U32(cullDistanceCount);

    GFXDevice::OverrideDeviceInformation(deviceInformation);
    // Seamless cubemaps are a nice feature to have enabled (core since 3.2)
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    //glEnable(GL_FRAMEBUFFER_SRGB);
    // Culling is enabled by default, but RenderStateBlocks can toggle it on a per-draw call basis
    glEnable(GL_CULL_FACE);

    // Enable all clip planes, I guess
    for (U8 i = 0u; i < Config::MAX_CLIP_DISTANCES; ++i) {
        glEnable(static_cast<GLenum>(static_cast<U32>(GL_CLIP_DISTANCE0) + i));
    }

    for (U8 i = 0u; i < to_base(GLUtil::GLMemory::GLMemoryType::COUNT); ++i) {
        s_memoryAllocators[i].init(s_memoryAllocatorSizes[i]);
    }

    s_textureViewCache.init(256);

    // FontStash library initialization
    // 512x512 atlas with bottom-left origin
    _fonsContext = glfonsCreate(512, 512, FONS_ZERO_BOTTOMLEFT);
    if (_fonsContext == nullptr) {
        Console::errorfn(Locale::Get(_ID("ERROR_FONT_INIT")));
        return ErrorCode::FONT_INIT_ERROR;
    }

    // Initialize our query pool
    s_hardwareQueryPool->init(
        {
            { GL_TIME_ELAPSED, 9 },
            { GL_TRANSFORM_FEEDBACK_OVERFLOW, 6 },
            { GL_VERTICES_SUBMITTED, 6 },
            { GL_PRIMITIVES_SUBMITTED, 6 },
            { GL_VERTEX_SHADER_INVOCATIONS, 6 },
            { GL_SAMPLES_PASSED, 6 },
            { GL_ANY_SAMPLES_PASSED, 6 },
            { GL_PRIMITIVES_GENERATED, 6 },
            { GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, 6 },
            { GL_ANY_SAMPLES_PASSED_CONSERVATIVE, 6 },
            { GL_TESS_CONTROL_SHADER_PATCHES, 6},
            { GL_TESS_EVALUATION_SHADER_INVOCATIONS, 6}
        }
    );

    // Once OpenGL is ready for rendering, init CEGUI
    if (config.gui.cegui.enabled) {
        _GUIGLrenderer = &CEGUI::OpenGL3Renderer::create();
        _GUIGLrenderer->enableExtraStateSettings(false);
        _context.context().gui().setRenderer(*_GUIGLrenderer);
    }

    glClearColor(DefaultColours::BLACK.r,
                 DefaultColours::BLACK.g,
                 DefaultColours::BLACK.b,
                 DefaultColours::BLACK.a);

    _performanceQueries[to_base(GlobalQueryTypes::VERTICES_SUBMITTED)] = eastl::make_unique<glHardwareQueryRing>(_context, GL_VERTICES_SUBMITTED, 6);
    _performanceQueries[to_base(GlobalQueryTypes::PRIMITIVES_GENERATED)] = eastl::make_unique<glHardwareQueryRing>(_context, GL_PRIMITIVES_GENERATED, 6);
    _performanceQueries[to_base(GlobalQueryTypes::TESSELLATION_PATCHES)] = eastl::make_unique<glHardwareQueryRing>(_context, GL_TESS_CONTROL_SHADER_PATCHES, 6);
    _performanceQueries[to_base(GlobalQueryTypes::TESSELLATION_EVAL_INVOCATIONS)] = eastl::make_unique<glHardwareQueryRing>(_context, GL_TESS_EVALUATION_SHADER_INVOCATIONS, 6);
    _performanceQueries[to_base(GlobalQueryTypes::GPU_TIME)] = eastl::make_unique<glHardwareQueryRing>(_context, GL_TIME_ELAPSED, 6);

    // That's it. Everything should be ready for draw calls
    Console::printfn(Locale::Get(_ID("START_OGL_API_OK")));
    return ErrorCode::NO_ERR;
}

/// Clear everything that was setup in initRenderingAPI()
void GL_API::closeRenderingAPI() {
    if (_GUIGLrenderer) {
        CEGUI::OpenGL3Renderer::destroy(*_GUIGLrenderer);
        _GUIGLrenderer = nullptr;
    }

    glShaderProgram::DestroyStaticData();

    // Destroy sampler objects
    {
        for (auto &sampler : s_samplerMap) {
            glSamplerObject::Destruct(sampler.second);
        }
        s_samplerMap.clear();
    }
    // Destroy the text rendering system
    glfonsDelete(_fonsContext);
    _fonsContext = nullptr;

    _fonts.clear();
    s_textureViewCache.destroy();
    if (s_hardwareQueryPool != nullptr) {
        s_hardwareQueryPool->destroy();
        MemoryManager::DELETE(s_hardwareQueryPool);
    }
    for (GLUtil::GLMemory::DeviceAllocator& allocator : s_memoryAllocators) {
        allocator.deallocate();
    }
    g_ContextPool.destroy();

    for (VAOMap::value_type& value : s_vaoCache) {
        if (value.second != GLUtil::k_invalidObjectID) {
            GL_API::DeleteVAOs(1, &value.second);
        }
    }
    s_vaoCache.clear();
    s_stateTracker.reset();
    glLockManager::Clear();
}

/// Prepare the GPU for rendering a frame
bool GL_API::beginFrame(DisplayWindow& window, const bool global) {
    OPTICK_EVENT();
    // Start a duration query in debug builds
    if (global && _runQueries) {
        if_constexpr(g_runAllQueriesInSameFrame) {
            for (U8 i = 0u; i < to_base(GlobalQueryTypes::COUNT); ++i) {
                _performanceQueries[i]->begin();
            }
        } else {
            _performanceQueries[_queryIdxForCurrentFrame]->begin();
        }
    }

    auto& stateTracker = GetStateTracker();
    while(!stateTracker->_endFrameFences.empty()) {
        auto& sync = stateTracker->_endFrameFences.front();
        const GLenum waitRet = glClientWaitSync(sync._syncObject, SyncObjectMask::GL_NONE_BIT, 0u);
        DIVIDE_ASSERT(waitRet != GL_WAIT_FAILED, "GL_API::beginFrame error: Not sure what to do here. Probably raise an exception or something.");
        if (waitRet == GL_ALREADY_SIGNALED || waitRet == GL_CONDITION_SATISFIED) {
            stateTracker->_lastSyncedFrameNumber = sync._frameNumber;
            DestroyFenceSync(sync._syncObject);
            stateTracker->_endFrameFences.pop();
        } else {
            break;
        }
    }

    glLockManager::CleanExpiredSyncObjects(stateTracker->_lastSyncedFrameNumber);

    SDL_GLContext glContext = window.userData()->_glContext;
    const I64 windowGUID = window.getGUID();

    if (glContext != nullptr && (_currentContext._windowGUID != windowGUID || _currentContext._context != glContext)) {
        SDL_GL_MakeCurrent(window.getRawWindow(), glContext);
        _currentContext._windowGUID = windowGUID;
        _currentContext._context = glContext;
    }

    // Clear our buffers
    if (!window.minimized() && !window.hidden()) {
        bool shouldClearColour = false, shouldClearDepth = false, shouldClearStencil = false;
        stateTracker->setClearColour(window.clearColour(shouldClearColour, shouldClearDepth));
        ClearBufferMask mask = ClearBufferMask::GL_NONE_BIT;
        if (shouldClearColour) {
            mask |= ClearBufferMask::GL_COLOR_BUFFER_BIT;
        }
        if (shouldClearDepth) {
            mask |= ClearBufferMask::GL_DEPTH_BUFFER_BIT;
        }
        if (shouldClearStencil) {
            mask |= ClearBufferMask::GL_STENCIL_BUFFER_BIT;
        }
        if (mask != ClearBufferMask::GL_NONE_BIT) {
            glClear(mask);
        }
    }
    // Clears are registered as draw calls by most software, so we do the same
    // to stay in sync with third party software
    _context.registerDrawCall();

    clearStates(window, *stateTracker, global);

    const vec2<U16>& drawableSize = window.getDrawableSize();
    _context.setViewport(0, 0, drawableSize.width, drawableSize.height);

    return true;
}

void GL_API::endFrameLocal(const DisplayWindow& window) {
    OPTICK_EVENT();

    // Swap buffers    
    SDL_GLContext glContext = window.userData()->_glContext;
    const I64 windowGUID = window.getGUID();

    if (glContext != nullptr && (_currentContext._windowGUID != windowGUID || _currentContext._context != glContext)) {
        OPTICK_EVENT("GL_API: Swap Context");
        SDL_GL_MakeCurrent(window.getRawWindow(), glContext);
        _currentContext._windowGUID = windowGUID;
        _currentContext._context = glContext;
    }
    {
        OPTICK_EVENT("GL_API: Swap Buffers");
        SDL_GL_SwapWindow(window.getRawWindow());
    }
}

void GL_API::endFrameGlobal(const DisplayWindow& window) {
    OPTICK_EVENT();

    if (_runQueries) {
        OPTICK_EVENT("End GPU Queries");
        // End the timing query started in beginFrame() in debug builds

        if_constexpr(g_runAllQueriesInSameFrame) {
            for (U8 i = 0; i < to_base(GlobalQueryTypes::COUNT); ++i) {
                _performanceQueries[i]->end();
            }
        } else {
            _performanceQueries[_queryIdxForCurrentFrame]->end();
        }
    }

    if (glGetGraphicsResetStatus() != GL_NO_ERROR) {
        DIVIDE_UNEXPECTED_CALL_MSG("OpenGL Reset Status raised!");
    }

    _swapBufferTimer.start();
    endFrameLocal(window);
    {
        //OPTICK_EVENT("Post-swap delay");
        //SDL_Delay(1);
    }
    _swapBufferTimer.stop();

    for (U32 i = 0u; i < GL_API::s_LockFrameLifetime - 1; ++i) {
        s_fenceSyncCounter[i] = s_fenceSyncCounter[i + 1];
    }

    OPTICK_EVENT("GL_API: Post-Swap cleanup");
    s_textureViewCache.onFrameEnd();
    s_glFlushQueued.store(false);
    glLockManager::CleanExpiredSyncObjects(GFXDevice::FrameCount());

    if (_runQueries) {
        OPTICK_EVENT("GL_API: Time Query");
        static std::array<I64, to_base(GlobalQueryTypes::COUNT)> results{};
        if_constexpr(g_runAllQueriesInSameFrame) {
            for (U8 i = 0; i < to_base(GlobalQueryTypes::COUNT); ++i) {
                results[i] = _performanceQueries[i]->getResultNoWait();
                _performanceQueries[i]->incQueue();
            }
        } else {
            results[_queryIdxForCurrentFrame] = _performanceQueries[_queryIdxForCurrentFrame]->getResultNoWait();
            _performanceQueries[_queryIdxForCurrentFrame]->incQueue();
        }

        _queryIdxForCurrentFrame = (_queryIdxForCurrentFrame + 1) % to_base(GlobalQueryTypes::COUNT);

        if (g_runAllQueriesInSameFrame || _queryIdxForCurrentFrame == 0) {
            _context.getPerformanceMetrics()._gpuTimeInMS = Time::NanosecondsToMilliseconds<F32>(results[to_base(GlobalQueryTypes::GPU_TIME)]);
            _context.getPerformanceMetrics()._verticesSubmitted = to_U64(results[to_base(GlobalQueryTypes::VERTICES_SUBMITTED)]);
            _context.getPerformanceMetrics()._primitivesGenerated = to_U64(results[to_base(GlobalQueryTypes::PRIMITIVES_GENERATED)]);
            _context.getPerformanceMetrics()._tessellationPatches = to_U64(results[to_base(GlobalQueryTypes::TESSELLATION_PATCHES)]);
            _context.getPerformanceMetrics()._tessellationInvocations = to_U64(results[to_base(GlobalQueryTypes::TESSELLATION_EVAL_INVOCATIONS)]);
        }
    }

    const size_t fenceSize = std::size(s_fenceSyncCounter);

    for (size_t i = 0u; i < std::size(_context.getPerformanceMetrics()._syncObjectsInFlight); ++i) {
        _context.getPerformanceMetrics()._syncObjectsInFlight[i] = i < fenceSize ? s_fenceSyncCounter[i] : 0u;
    }

    _context.getPerformanceMetrics()._generatedRenderTargetCount = to_U32(_context.renderTargetPool().getRenderTargets().size());
    _runQueries = _context.queryPerformanceStats();

    GetStateTracker()->_endFrameFences.push(CreateFrameFenceSync());
    _context.getPerformanceMetrics()._queuedGPUFrames = GetStateTracker()->_endFrameFences.size();
}

/// Finish rendering the current frame
void GL_API::endFrame(DisplayWindow& window, const bool global) {
    OPTICK_EVENT();

    if (global) {
        endFrameGlobal(window);
    } else {
        endFrameLocal(window);
    }
}

void GL_API::idle([[maybe_unused]] const bool fast) {
    glShaderProgram::Idle(_context.context());
}

/// Text rendering is handled exclusively by Mikko Mononen's FontStash library (https://github.com/memononen/fontstash)
/// with his OpenGL frontend adapted for core context profiles
void GL_API::drawText(const TextElementBatch& batch) {
    OPTICK_EVENT();

    BlendingSettings textBlend{};
    textBlend.blendSrc(BlendProperty::SRC_ALPHA);
    textBlend.blendDest(BlendProperty::INV_SRC_ALPHA);
    textBlend.blendOp(BlendOperation::ADD);
    textBlend.blendSrcAlpha(BlendProperty::ONE);
    textBlend.blendDestAlpha(BlendProperty::ZERO);
    textBlend.blendOpAlpha(BlendOperation::COUNT);
    textBlend.enabled(true);

    GetStateTracker()->setBlending(0, textBlend);
    GetStateTracker()->setBlendColour(DefaultColours::BLACK_U8);

    const I32 width = _context.renderingResolution().width;
    const I32 height = _context.renderingResolution().height;
        
    size_t drawCount = 0;
    size_t previousStyle = 0;

    fonsClearState(_fonsContext);
    for (const TextElement& entry : batch.data())
    {
        if (previousStyle != entry.textLabelStyleHash()) {
            const TextLabelStyle& textLabelStyle = TextLabelStyle::get(entry.textLabelStyleHash());
            const UColour4& colour = textLabelStyle.colour();
            // Retrieve the font from the font cache
            const I32 font = getFont(TextLabelStyle::fontName(textLabelStyle.font()));
            // The font may be invalid, so skip this text label
            if (font != FONS_INVALID) {
                fonsSetFont(_fonsContext, font);
            }
            fonsSetBlur(_fonsContext, textLabelStyle.blurAmount());
            fonsSetBlur(_fonsContext, textLabelStyle.spacing());
            fonsSetAlign(_fonsContext, textLabelStyle.alignFlag());
            fonsSetSize(_fonsContext, to_F32(textLabelStyle.fontSize()));
            fonsSetColour(_fonsContext, colour.r, colour.g, colour.b, colour.a);
            previousStyle = entry.textLabelStyleHash();
        }

        const F32 textX = entry.position().d_x.d_scale * width + entry.position().d_x.d_offset;
        const F32 textY = height - (entry.position().d_y.d_scale * height + entry.position().d_y.d_offset);

        F32 lh = 0;
        fonsVertMetrics(_fonsContext, nullptr, nullptr, &lh);

const TextElement::TextType& text = entry.text();
const size_t lineCount = text.size();
for (size_t i = 0; i < lineCount; ++i) {
    fonsDrawText(_fonsContext,
        textX,
        textY - lh * i,
        text[i].c_str(),
        nullptr);
}
drawCount += lineCount;


// Register each label rendered as a draw call
_context.registerDrawCalls(to_U32(drawCount));
    }
}

bool GL_API::draw(const GenericDrawCommand& cmd) const {
    OPTICK_EVENT();

    if (cmd._sourceBuffer._id == 0) {
        U32 indexCount = 0u;
        switch (GL_API::GetStateTracker()->_activeTopology) {
        case PrimitiveTopology::COMPUTE:
        case PrimitiveTopology::COUNT: DIVIDE_UNEXPECTED_CALL();         break;
        case PrimitiveTopology::TRIANGLES: indexCount = cmd._drawCount * 3;  break;
        case PrimitiveTopology::POINTS: indexCount = cmd._drawCount * 1;  break;
        default: indexCount = cmd._cmd.indexCount; break;
        }

        glDrawArrays(GLUtil::glPrimitiveTypeTable[to_base(GL_API::GetStateTracker()->_activeTopology)], cmd._cmd.firstIndex, indexCount);
    }
    else {
        // Because this can only happen on the main thread, try and avoid costly lookups for hot-loop drawing
        static VertexDataInterface::Handle s_lastID = VertexDataInterface::INVALID_VDI_HANDLE;
        static VertexDataInterface* s_lastBuffer = nullptr;

        if (s_lastID != cmd._sourceBuffer) {
            s_lastID = cmd._sourceBuffer;
            s_lastBuffer = VertexDataInterface::s_VDIPool.find(s_lastID);
        }

        DIVIDE_ASSERT(s_lastBuffer != nullptr);
        s_lastBuffer->draw(cmd, nullptr);
    }

    return true;
}

void GL_API::flushTextureBindQueue() {
    static std::array<TexBindEntry, GLStateTracker::MAX_BOUND_TEXTURE_UNITS> s_textureCache;
    static std::array<GLuint, GLStateTracker::MAX_BOUND_TEXTURE_UNITS> s_textureHandles;
    static std::array<GLuint, GLStateTracker::MAX_BOUND_TEXTURE_UNITS> s_textureSamplers;

    if (s_TexBindQueue.empty()) {
        return;
    }

    if (s_TexBindQueue.size() == 1u) {
        const TexBindEntry& texEntry = s_TexBindQueue.front();
        if (GetStateTracker()->bindTexture(texEntry._slot, texEntry._handle, texEntry._sampler) == GLStateTracker::BindResult::FAILED) {
            DIVIDE_UNEXPECTED_CALL();
        }
    } else {
        auto& stateTracker = GetStateTracker();

        // Reset our cache
        for (auto& it : s_textureCache) {
            it._handle = GLUtil::k_invalidObjectID;
        }

        // Sort by slot number
        eastl::sort(eastl::begin(s_TexBindQueue),
            eastl::end(s_TexBindQueue),
            [](const  TexBindEntry& lhs, const TexBindEntry& rhs) {
                return lhs._slot < rhs._slot;
            });

        // Grab min and max slot and fill in the cache with all of the available data
        U8 minSlot = U8_MAX, maxSlot = 0u;
        for (const TexBindEntry& texEntry : s_TexBindQueue) {
            s_textureCache[texEntry._slot] = texEntry;
            minSlot = std::min(texEntry._slot, minSlot);
            maxSlot = std::max(texEntry._slot, maxSlot);
        }

        U8 idx = 0u, bindOffset = minSlot, texCount = maxSlot - minSlot;
        for (U8 i = 0u; i < texCount + 1; ++i) {
            const U8 slot = i + minSlot;
            const TexBindEntry& it = s_textureCache[slot];

            if (it._handle != GLUtil::k_invalidObjectID) {
                s_textureHandles[idx] = it._handle;
                s_textureSamplers[idx] = it._sampler;

                if (idx++ == 0u) {
                    // Start a new range so remember the starting offset
                    bindOffset = slot;
                }
            } else if (idx > 0u) {
                // We found a hole in the texture range. Flush the current range and start a new one
                if (stateTracker->bindTextures(bindOffset, idx, s_textureHandles.data(), s_textureSamplers.data()) == GLStateTracker::BindResult::FAILED) {
                    DIVIDE_UNEXPECTED_CALL();
                }
                idx = 0u;
            }
        }
        // Handle the final range (if any)
        if (idx > 0u) {
            if (stateTracker->bindTextures(bindOffset, idx, s_textureHandles.data(), s_textureSamplers.data()) == GLStateTracker::BindResult::FAILED) {
                DIVIDE_UNEXPECTED_CALL();
            }
        }
    }

    s_TexBindQueue.clear();
}

GLuint GL_API::getGLTextureView(const ImageView srcView, const U8 lifetimeInFrames) {
    auto [handle, cacheHit] = s_textureViewCache.allocate(srcView.getHash());

    if (!cacheHit) {
        const GLenum glInternalFormat = GLUtil::internalFormat(srcView._descriptor._baseFormat,
                                                               srcView._descriptor._dataType,
                                                               srcView._descriptor._srgb,
                                                               srcView._descriptor._normalized);
        OPTICK_EVENT("GL: cache miss  - Image");
        glTextureView(handle,
                      GLUtil::internalTextureType(srcView.targetType(), srcView._descriptor._msaaSamples),
                      static_cast<glTexture*>(srcView._srcTexture)->textureHandle(),
                      glInternalFormat,
                      static_cast<GLuint>(srcView._mipLevels.x),
                      static_cast<GLuint>(srcView._mipLevels.y),
                      static_cast<GLuint>(srcView._layerRange.x),
                      static_cast<GLuint>(srcView._layerRange.y));
    }

    s_textureViewCache.deallocate(handle, lifetimeInFrames);

    return handle;
}

void GL_API::preFlushCommandBuffer([[maybe_unused]] const GFX::CommandBuffer& commandBuffer) {
    NOP();
}

void GL_API::flushCommand(GFX::CommandBase* cmd) {
    static GFX::MemoryBarrierCommand pushConstantsMemCommand{};
    static bool pushConstantsNeedLock = false;

    OPTICK_EVENT();

    OPTICK_TAG("Type", to_base(cmd->Type()));

    OPTICK_EVENT(GFX::Names::commandType[to_base(cmd->Type())]);
    if (GFXDevice::IsSubmitCommand(cmd->Type())) {
        flushTextureBindQueue();
    }

    switch (cmd->Type()) {
        case GFX::CommandType::BEGIN_RENDER_PASS: {
            const GFX::BeginRenderPassCommand* crtCmd = cmd->As<GFX::BeginRenderPassCommand>();

            glFramebuffer* rt = static_cast<glFramebuffer*>(_context.renderTargetPool().getRenderTarget(crtCmd->_target));
            Attorney::GLAPIRenderTarget::begin(*rt, crtCmd->_descriptor);
            GetStateTracker()->_activeRenderTarget = rt;
            PushDebugMessage(crtCmd->_name.c_str(), rt->framebufferHandle());
        }break;
        case GFX::CommandType::END_RENDER_PASS: {
            PopDebugMessage();

            assert(GL_API::GetStateTracker()->_activeRenderTarget != nullptr);
            Attorney::GLAPIRenderTarget::end(
                *GetStateTracker()->_activeRenderTarget,
                cmd->As<GFX::EndRenderPassCommand>()->_setDefaultRTState
            );
        }break;
        case GFX::CommandType::BEGIN_RENDER_SUB_PASS: {
            const GFX::BeginRenderSubPassCommand* crtCmd = cmd->As<GFX::BeginRenderSubPassCommand>();

            assert(GL_API::GetStateTracker()->_activeRenderTarget != nullptr);
            for (const RenderTarget::DrawLayerParams& params : crtCmd->_writeLayers) {
                GetStateTracker()->_activeRenderTarget->drawToLayer(params);
            }

            GetStateTracker()->_activeRenderTarget->setMipLevel(crtCmd->_mipWriteLevel);
        }break;
        case GFX::CommandType::END_RENDER_SUB_PASS: {
            NOP();
        }break;
        case GFX::CommandType::BEGIN_GPU_QUERY: {
            const GFX::BeginGPUQuery* crtCmd = cmd->As<GFX::BeginGPUQuery>();
            if (crtCmd->_queryMask == 0u) {
                return;
            }

            U8 i = 0u;
            auto& queryContext = _queryContext.emplace();
            if (BitCompare(crtCmd->_queryMask, QueryType::VERTICES_SUBMITTED)) {
                queryContext[0]._query = &GL_API::GetHardwareQueryPool()->allocate(GL_VERTICES_SUBMITTED);
                queryContext[0]._type = QueryType::VERTICES_SUBMITTED;
                queryContext[0]._index = i++;
            }
            if (BitCompare(crtCmd->_queryMask, QueryType::PRIMITIVES_GENERATED)) {
                queryContext[1]._query = &GL_API::GetHardwareQueryPool()->allocate(GL_PRIMITIVES_GENERATED);
                queryContext[1]._type = QueryType::VERTICES_SUBMITTED;
                queryContext[1]._index = i++;
            }
            if (BitCompare(crtCmd->_queryMask, QueryType::TESSELLATION_PATCHES)) {
                queryContext[2]._query = &GL_API::GetHardwareQueryPool()->allocate(GL_TESS_CONTROL_SHADER_PATCHES);
                queryContext[2]._type = QueryType::VERTICES_SUBMITTED;
                queryContext[2]._index = i++;
            }
            if (BitCompare(crtCmd->_queryMask, QueryType::TESSELLATION_EVAL_INVOCATIONS)) {
                queryContext[3]._query = &GL_API::GetHardwareQueryPool()->allocate(GL_TESS_EVALUATION_SHADER_INVOCATIONS);
                queryContext[3]._type = QueryType::VERTICES_SUBMITTED;
                queryContext[3]._index = i++;
            }
            if (BitCompare(crtCmd->_queryMask, QueryType::GPU_TIME)) {
                queryContext[4]._query = &GL_API::GetHardwareQueryPool()->allocate(GL_TIME_ELAPSED);
                queryContext[4]._type = QueryType::VERTICES_SUBMITTED;
                queryContext[4]._index = i++;
            }
            if (BitCompare(crtCmd->_queryMask, QueryType::SAMPLE_COUNT)) {
                queryContext[5]._query = &GL_API::GetHardwareQueryPool()->allocate(GL_SAMPLES_PASSED);
                queryContext[5]._type = QueryType::VERTICES_SUBMITTED;
                queryContext[5]._index = i++;
            }
            if (BitCompare(crtCmd->_queryMask, QueryType::ANY_SAMPLE_RENDERED)) {
                queryContext[6]._query = &GL_API::GetHardwareQueryPool()->allocate(GL_ANY_SAMPLES_PASSED_CONSERVATIVE);
                queryContext[6]._type = QueryType::VERTICES_SUBMITTED;
                queryContext[6]._index = i++;
            }

            for (auto& queryEntry : queryContext) {
                if (queryEntry._query != nullptr) {
                    queryEntry._query->begin();
                }
            }
        }break;
        case GFX::CommandType::END_GPU_QUERY: {
            const GFX::EndGPUQuery* crtCmd = cmd->As<GFX::EndGPUQuery>();
            if (_queryContext.empty()) {
                return;
            }
            DIVIDE_ASSERT(crtCmd->_resultContainer != nullptr);

            for (auto& queryEntry : _queryContext.top()) {
                if (queryEntry._query != nullptr) {
                    queryEntry._query->end();

                    const I64 qResult =  crtCmd->_waitForResults ? queryEntry._query->getResult() : queryEntry._query->getResultNoWait();
                    (*crtCmd->_resultContainer)[queryEntry._index] = { queryEntry._type,  qResult };
                }
            }

            _queryContext.pop();
        }break;
        case GFX::CommandType::COPY_TEXTURE: {
            const GFX::CopyTextureCommand* crtCmd = cmd->As<GFX::CopyTextureCommand>();
            glTexture::copy(static_cast<glTexture*>(crtCmd->_source),
                            crtCmd->_sourceMSAASamples,
                            static_cast<glTexture*>(crtCmd->_destination),
                            crtCmd->_destinationMSAASamples,
                            crtCmd->_params);
        }break;
        case GFX::CommandType::BIND_PIPELINE: {
            if (pushConstantsNeedLock) {
                flushCommand(&pushConstantsMemCommand);
                pushConstantsMemCommand._bufferLocks.clear();
                pushConstantsNeedLock = false;
            }

            const Pipeline* pipeline = cmd->As<GFX::BindPipelineCommand>()->_pipeline;
            assert(pipeline != nullptr);
            if (bindPipeline(*pipeline) == ShaderResult::Failed) {
                Console::errorfn(Locale::Get(_ID("ERROR_GLSL_INVALID_BIND")), pipeline->descriptor()._shaderProgramHandle);
            }
        } break;
        case GFX::CommandType::SEND_PUSH_CONSTANTS: {
            const auto dumpLogs = [this]() {
                Console::d_errorfn(Locale::Get(_ID("ERROR_GLSL_INVALID_PUSH_CONSTANTS")));
                if (Config::ENABLE_GPU_VALIDATION) {
                    // Shader failed to compile probably. Dump all shader caches for inspection.
                    glShaderProgram::Idle(_context.context());
                    Console::flush();
                }
            };

            const Pipeline* activePipeline = GetStateTracker()->_activePipeline;
            if (activePipeline == nullptr) {
                dumpLogs();
                break;
            }

            if (GetStateTracker()->_activeShaderProgram == nullptr) {
                // Should we skip the upload?
                dumpLogs();
                break;
            }

            const PushConstants& pushConstants = cmd->As<GFX::SendPushConstantsCommand>()->_constants;
            GetStateTracker()->_activeShaderProgram->uploadPushConstants(pushConstants, _context.descriptorSet(DescriptorSetUsage::PER_DRAW).impl(), pushConstantsMemCommand);
            pushConstantsNeedLock = !pushConstantsMemCommand._bufferLocks.empty();
        } break;
        case GFX::CommandType::SET_SCISSOR: {
            GetStateTracker()->setScissor(cmd->As<GFX::SetScissorCommand>()->_rect);
        }break;
        case GFX::CommandType::BEGIN_DEBUG_SCOPE: {
            const auto& crtCmd = cmd->As<GFX::BeginDebugScopeCommand>();
            PushDebugMessage(crtCmd->_scopeName.c_str(), crtCmd->_scopeId);
        } break;
        case GFX::CommandType::END_DEBUG_SCOPE: {
            PopDebugMessage();
        } break;
        case GFX::CommandType::ADD_DEBUG_MESSAGE: {
            const auto& crtCmd = cmd->As<GFX::AddDebugMessageCommand>();
            PushDebugMessage(crtCmd->_msg.c_str(), crtCmd->_msgId);
            PopDebugMessage();
        }break;
        case GFX::CommandType::COMPUTE_MIPMAPS: {
            const GFX::ComputeMipMapsCommand* crtCmd = cmd->As<GFX::ComputeMipMapsCommand>();

            if (crtCmd->_layerRange.x == 0 && crtCmd->_layerRange.y == crtCmd->_texture->descriptor().layerCount()) {
                OPTICK_EVENT("GL: In-place computation - Full");
                glGenerateTextureMipmap(static_cast<glTexture*>(crtCmd->_texture)->textureHandle());
            } else {
                OPTICK_EVENT("GL: View-based computation");

                ImageView view = crtCmd->_texture->defaultView();
                view._layerRange.set(crtCmd->_layerRange);

                if (crtCmd->_mipRange.max != 0u) {
                    view._mipLevels.set(crtCmd->_mipRange);
                }
                
                DIVIDE_ASSERT(view.targetType() != TextureType::COUNT);
                
                if (IsArrayTexture(view.targetType()) && view._layerRange.max == 1) {
                    switch (view.targetType()) {
                        case TextureType::TEXTURE_1D_ARRAY:
                            view.targetType(TextureType::TEXTURE_1D);
                            break;
                        case TextureType::TEXTURE_2D_ARRAY:
                            view.targetType(TextureType::TEXTURE_2D);
                            break;
                        case TextureType::TEXTURE_CUBE_ARRAY:
                            view.targetType(TextureType::TEXTURE_CUBE_MAP);
                            break;
                        default: break;
                    }
                }

                if (IsCubeTexture(view.targetType())) {
                    view._layerRange *= 6; //offset and count
                }

                if (view._mipLevels.x != 0u || view._mipLevels.y != 0u) {
                    OPTICK_EVENT("GL: In-place computation - Image");
                    glGenerateTextureMipmap(getGLTextureView(view, 3));
                }
            }
        }break;
        case GFX::CommandType::DRAW_TEXT: {
            if (GetStateTracker()->_activePipeline != nullptr) {
                drawText(cmd->As<GFX::DrawTextCommand>()->_batch);
            }
        }break;
        case GFX::CommandType::DRAW_COMMANDS : {
            DIVIDE_ASSERT(GetStateTracker()->_activePipeline != nullptr);

            U32 drawCount = 0u;
            const GFX::DrawCommand::CommandContainer& drawCommands = cmd->As<GFX::DrawCommand>()->_drawCommands;
            for (const GenericDrawCommand& currentDrawCommand : drawCommands) {
                if (draw(currentDrawCommand)) {
                    drawCount += isEnabledOption(currentDrawCommand, CmdRenderOptions::RENDER_WIREFRAME) 
                                       ? 2 
                                       : isEnabledOption(currentDrawCommand, CmdRenderOptions::RENDER_GEOMETRY) ? 1 : 0;
                }
            }
            _context.registerDrawCalls(drawCount);
        }break;
        case GFX::CommandType::DISPATCH_COMPUTE: {
            if (GetStateTracker()->_activePipeline != nullptr) {
                assert(GetStateTracker()->_activeTopology == PrimitiveTopology::COMPUTE);

                const GFX::DispatchComputeCommand* crtCmd = cmd->As<GFX::DispatchComputeCommand>();
                glDispatchCompute(crtCmd->_computeGroupSize.x, crtCmd->_computeGroupSize.y, crtCmd->_computeGroupSize.z);
            }
        }break;
        case GFX::CommandType::SET_CLIPING_STATE: {
            const GFX::SetClippingStateCommand* crtCmd = cmd->As<GFX::SetClippingStateCommand>();
            GetStateTracker()->setClippingPlaneState(crtCmd->_lowerLeftOrigin, crtCmd->_negativeOneToOneDepth);
        } break;
        case GFX::CommandType::MEMORY_BARRIER: {
            const GFX::MemoryBarrierCommand* crtCmd = cmd->As<GFX::MemoryBarrierCommand>();
            const U32 barrierMask = crtCmd->_barrierMask;
            if (barrierMask != 0) {
                if (BitCompare(barrierMask, to_base(MemoryBarrierType::TEXTURE_BARRIER))) {
                    glTextureBarrier();
                }
                if (barrierMask == to_base(MemoryBarrierType::ALL_MEM_BARRIERS)) {
                    glMemoryBarrier(MemoryBarrierMask::GL_ALL_BARRIER_BITS);
                } else {
                    MemoryBarrierMask glMask = MemoryBarrierMask::GL_NONE_BIT;
                    for (U8 i = 0; i < to_U8(MemoryBarrierType::COUNT) + 1; ++i) {
                        if (BitCompare(barrierMask, 1u << i)) {
                            switch (static_cast<MemoryBarrierType>(1 << i)) {
                            case MemoryBarrierType::BUFFER_UPDATE:
                                glMask |= MemoryBarrierMask::GL_BUFFER_UPDATE_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::SHADER_STORAGE:
                                glMask |= MemoryBarrierMask::GL_SHADER_STORAGE_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::COMMAND_BUFFER:
                                glMask |= MemoryBarrierMask::GL_COMMAND_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::ATOMIC_COUNTER:
                                glMask |= MemoryBarrierMask::GL_ATOMIC_COUNTER_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::QUERY:
                                glMask |= MemoryBarrierMask::GL_QUERY_BUFFER_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::RENDER_TARGET:
                                glMask |= MemoryBarrierMask::GL_FRAMEBUFFER_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::TEXTURE_UPDATE:
                                glMask |= MemoryBarrierMask::GL_TEXTURE_UPDATE_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::TEXTURE_FETCH:
                                glMask |= MemoryBarrierMask::GL_TEXTURE_FETCH_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::SHADER_IMAGE:
                                glMask |= MemoryBarrierMask::GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::TRANSFORM_FEEDBACK:
                                glMask |= MemoryBarrierMask::GL_TRANSFORM_FEEDBACK_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::VERTEX_ATTRIB_ARRAY:
                                glMask |= MemoryBarrierMask::GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::INDEX_ARRAY:
                                glMask |= MemoryBarrierMask::GL_ELEMENT_ARRAY_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::UNIFORM_DATA:
                                glMask |= MemoryBarrierMask::GL_UNIFORM_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::PIXEL_BUFFER:
                                glMask |= MemoryBarrierMask::GL_PIXEL_BUFFER_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::PERSISTENT_BUFFER:
                                glMask |= MemoryBarrierMask::GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT;
                                break;
                            default:
                                NOP();
                                break;
                            }
                        }
                    }
                    glMemoryBarrier(glMask);
                }
            }

            if (!crtCmd->_bufferLocks.empty()) {
                const SyncObjectHandle sync = glLockManager::CreateSyncObject(crtCmd->_syncFlag);
                for (const BufferLock& lock : crtCmd->_bufferLocks) {
                    if (!static_cast<const glShaderBuffer*>(lock._targetBuffer)->bufferImpl()->lockByteRange(lock._range, sync)) {
                        DIVIDE_UNEXPECTED_CALL();
                    }
                }
            }

            for (auto it : crtCmd->_fenceLocks) {
                Attorney::glGenericVertexDataGL_API::insertFencesIfNeeded(static_cast<glGenericVertexData*>(it));
            }
        } break;
        default: break;
    }

    if (GFXDevice::IsSubmitCommand(cmd->Type())) {
        if (pushConstantsNeedLock) {
            flushCommand(&pushConstantsMemCommand);
            pushConstantsMemCommand._bufferLocks.clear();
            pushConstantsNeedLock = false;
        }
    }
}

void GL_API::postFlushCommandBuffer([[maybe_unused]] const GFX::CommandBuffer& commandBuffer) {
    OPTICK_EVENT();

    bool expected = true;
    if (s_glFlushQueued.compare_exchange_strong(expected, false)) {
        OPTICK_EVENT("GL_FLUSH");
        glFlush();
    }
}

vec2<U16> GL_API::getDrawableSize(const DisplayWindow& window) const noexcept {
    int w = 1, h = 1;
    SDL_GL_GetDrawableSize(window.getRawWindow(), &w, &h);
    return vec2<U16>(w, h);
}

U32 GL_API::getHandleFromCEGUITexture(const CEGUI::Texture& textureIn) const {
    return to_U32(static_cast<const CEGUI::OpenGLTexture&>(textureIn).getOpenGLTexture());
}

void GL_API::onThreadCreated([[maybe_unused]] const std::thread::id& threadID) {
    // Double check so that we don't run into a race condition!
    ScopedLock<Mutex> lock(GLUtil::s_glSecondaryContextMutex);
    assert(SDL_GL_GetCurrentContext() == NULL);

    // This also makes the context current
    assert(GLUtil::s_glSecondaryContext == nullptr && "GL_API::syncToThread: double init context for current thread!");
    [[maybe_unused]] const bool ctxFound = g_ContextPool.getAvailableContext(GLUtil::s_glSecondaryContext);
    assert(ctxFound && "GL_API::syncToThread: context not found for current thread!");

    SDL_GL_MakeCurrent(GLUtil::s_glMainRenderWindow->getRawWindow(), GLUtil::s_glSecondaryContext);
    glbinding::Binding::initialize([](const char* proc) noexcept {
        return (glbinding::ProcAddress)SDL_GL_GetProcAddress(proc);
    });
    
    // Enable OpenGL debug callbacks for this context as well
    if_constexpr(Config::ENABLE_GPU_VALIDATION) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_MARKER, GL_DONT_CARE, 0, NULL, GL_FALSE);
        glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_PUSH_GROUP, GL_DONT_CARE, 0, NULL, GL_FALSE);
        glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_POP_GROUP, GL_DONT_CARE, 0, NULL, GL_FALSE);
        // Debug callback in a separate thread requires a flag to distinguish it from the main thread's callbacks
        glDebugMessageCallback((GLDEBUGPROC)GLUtil::DebugCallback, GLUtil::s_glSecondaryContext);
    }

    glMaxShaderCompilerThreadsARB(0xFFFFFFFF);
}

/// Try to find the requested font in the font cache. Load on cache miss.
I32 GL_API::getFont(const Str64& fontName) {
    if (_fontCache.first.compare(fontName) != 0) {
        _fontCache.first = fontName;
        const U64 fontNameHash = _ID(fontName.c_str());
        // Search for the requested font by name
        const auto& it = _fonts.find(fontNameHash);
        // If we failed to find it, it wasn't loaded yet
        if (it == std::cend(_fonts)) {
            // Fonts are stored in the general asset directory -> in the GUI
            // subfolder -> in the fonts subfolder
            ResourcePath fontPath(Paths::g_assetsLocation + Paths::g_GUILocation + Paths::g_fontsPath);
            fontPath += fontName.c_str();
            // We use FontStash to load the font file
            _fontCache.second = fonsAddFont(_fonsContext, fontName.c_str(), fontPath.c_str());
            // If the font is invalid, inform the user, but map it anyway, to avoid
            // loading an invalid font file on every request
            if (_fontCache.second == FONS_INVALID) {
                Console::errorfn(Locale::Get(_ID("ERROR_FONT_FILE")), fontName.c_str());
            }
            // Save the font in the font cache
            hashAlg::insert(_fonts, fontNameHash, _fontCache.second);
            
        } else {
            _fontCache.second = it->second;
        }

    }

    // Return the font
    return _fontCache.second;
}

/// Reset as much of the GL default state as possible within the limitations given
void GL_API::clearStates(const DisplayWindow& window, GLStateTracker& stateTracker, const bool global) const {
    if (global) {
        if (!stateTracker.unbindTextures()) {
            DIVIDE_UNEXPECTED_CALL();
        }
        stateTracker.setPixelPackUnpackAlignment();
    }

    if (stateTracker.setActiveVAO(0) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }
    if (stateTracker.setActiveBuffer(GL_ELEMENT_ARRAY_BUFFER, 0) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }
    if (stateTracker.setActiveFB(RenderTarget::RenderTargetUsage::RT_READ_WRITE, 0) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }
    stateTracker._activeClearColour.set(window.clearColour());
    const U8 blendCount = to_U8(stateTracker._blendEnabled.size());
    for (U8 i = 0u; i < blendCount; ++i) {
        stateTracker.setBlending(i, {});
    }
    stateTracker.setBlendColour({ 0u, 0u, 0u, 0u });

    const vec2<U16>& drawableSize = _context.getDrawableSize(window);
    stateTracker.setScissor({ 0, 0, drawableSize.width, drawableSize.height });

    stateTracker._activePipeline = nullptr;
    stateTracker._activeRenderTarget = nullptr;
    if (stateTracker.setActiveProgram(0u) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }
    if (stateTracker.setActiveShaderPipeline(0u) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }
    if (stateTracker.setStateBlock(RenderStateBlock::DefaultHash()) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }

    stateTracker.setDepthWrite(true);
}

bool GL_API::bindShaderResources(const DescriptorSetUsage usage, const DescriptorSet& bindings) {
    OPTICK_EVENT("BIND_SHADER_RESOURCES");

    for (auto& srcBinding : bindings) {
        const DescriptorSetBindingType type = srcBinding._data.Type();

        switch (type) {
            case DescriptorSetBindingType::UNIFORM_BUFFER:
            case DescriptorSetBindingType::SHADER_STORAGE_BUFFER: {
                if (!srcBinding._data.Has<ShaderBufferEntry>() ||
                    srcBinding._data.As<ShaderBufferEntry>()._buffer == nullptr ||
                    srcBinding._data.As<ShaderBufferEntry>()._range._length == 0u)
                {
                    continue;
                }

                const ShaderBufferEntry& bufferEntry = srcBinding._data.As<ShaderBufferEntry>();
                DIVIDE_ASSERT(bufferEntry._buffer != nullptr);
                glShaderBuffer* glBuffer = static_cast<glShaderBuffer*>(bufferEntry._buffer);

                if (!glBuffer->bindByteRange(
                    ShaderProgram::GetGLBindingForDescriptorSlot(usage, srcBinding._slot, type),
                    {
                        bufferEntry._range._startOffset * glBuffer->getPrimitiveSize(),
                        bufferEntry._range._length * glBuffer->getPrimitiveSize()
                    }))
                {
                    NOP();
                }
            } break;
            case DescriptorSetBindingType::COMBINED_IMAGE_SAMPLER: {
                if (srcBinding._slot == INVALID_TEXTURE_BINDING) {
                    continue;
                }

                const DescriptorCombinedImageSampler& imageSampler = srcBinding._data.As<DescriptorCombinedImageSampler>();
                    if (!makeTextureViewResident(usage, srcBinding._slot, imageSampler._image, imageSampler._samplerHash)) {
                    DIVIDE_UNEXPECTED_CALL();
                }
            } break;
            case DescriptorSetBindingType::IMAGE: {
                if (!srcBinding._data.Has<ImageView>()) {
                    continue;
                }
                const ImageView& image = srcBinding._data.As<ImageView>();
                DIVIDE_ASSERT(image._srcTexture != nullptr);

                assert(image.targetType() != TextureType::COUNT);
                assert(image._layerRange.max > 0u);

                GLenum access = GL_NONE;
                switch (image._flag) {
                    case ImageFlag::READ: access = GL_READ_ONLY; break;
                    case ImageFlag::WRITE: access = GL_WRITE_ONLY; break;
                    case ImageFlag::READ_WRITE: access = GL_READ_WRITE; break;
                    default: DIVIDE_UNEXPECTED_CALL();  break;
                }

                DIVIDE_ASSERT(image._mipLevels.max == 1u);

                const GLenum glInternalFormat = GLUtil::internalFormat(image._descriptor._baseFormat,
                                                                       image._descriptor._dataType,
                                                                       image._descriptor._srgb,
                                                                       image._descriptor._normalized);

                if (GL_API::GetStateTracker()->bindTextureImage(srcBinding._slot, 
                                                                static_cast<glTexture*>(image._srcTexture)->textureHandle(),
                                                                image._mipLevels.min,
                                                                image._layerRange.max > 1u,
                                                                image._layerRange.min,
                                                                access,
                                                                glInternalFormat) == GLStateTracker::BindResult::FAILED)
                {
                    DIVIDE_UNEXPECTED_CALL();
                }
            } break;
            case DescriptorSetBindingType::COUNT: {
                DIVIDE_UNEXPECTED_CALL();
            } break;
        };
    }

    return true;
}

bool GL_API::makeTextureViewResident(const DescriptorSetUsage set, const U8 bindingSlot, const ImageView& imageView, const size_t samplerHash) const {
    const U8 glBinding = ShaderProgram::GetGLBindingForDescriptorSlot(set, bindingSlot, DescriptorSetBindingType::COMBINED_IMAGE_SAMPLER);

    const GLuint samplerHandle = GetSamplerHandle(samplerHash);

    GLuint texHandle = static_cast<glTexture*>(imageView._srcTexture)->textureHandle();
    if (!imageView.isDefaultView()) {
        const size_t viewHash = imageView.getHash();

        auto [textureID, cacheHit] = s_textureViewCache.allocate(viewHash);
        DIVIDE_ASSERT(textureID != 0u);

        if (!cacheHit)
        {
            const GLenum glInternalFormat = GLUtil::internalFormat(imageView._descriptor._baseFormat,
                imageView._descriptor._dataType,
                imageView._descriptor._srgb,
                imageView._descriptor._normalized);

            const bool isCube = IsCubeTexture(imageView.targetType());

            glTextureView(textureID,
                GLUtil::internalTextureType(imageView.targetType(), imageView._descriptor._msaaSamples),
                static_cast<glTexture*>(imageView._srcTexture)->textureHandle(),
                glInternalFormat,
                static_cast<GLuint>(imageView._mipLevels.x),
                static_cast<GLuint>(imageView._mipLevels.y),
                isCube ? imageView._layerRange.min * 6 : imageView._layerRange.min,
                isCube ? imageView._layerRange.max * 6 : imageView._layerRange.max);
        }

        // Self delete after 3 frames unless we use it again
        s_textureViewCache.deallocate(textureID, 3u);
        texHandle = textureID;
    }

    for (TexBindEntry& it : s_TexBindQueue) {
        if (it._slot == glBinding) {
            it._handle = texHandle;
            it._sampler = samplerHandle;
            return true;
        }
    }

    TexBindEntry entry{};
    entry._slot = glBinding;
    entry._handle = texHandle;
    entry._sampler = samplerHandle;

    s_TexBindQueue.push_back(MOV(entry));

    return true;
}

bool GL_API::setViewport(const Rect<I32>& viewport) {
    return GetStateTracker()->setViewport(viewport);
}

ShaderResult GL_API::bindPipeline(const Pipeline& pipeline) {
    OPTICK_EVENT();
    auto& stateTracker = GetStateTracker();

    if (stateTracker->_activePipeline && *stateTracker->_activePipeline == pipeline) {
        return ShaderResult::OK;
    }
   
    stateTracker->_activePipeline = &pipeline;

    const PipelineDescriptor& pipelineDescriptor = pipeline.descriptor();
    {
        OPTICK_EVENT("Set Raster State");
        // Set the proper render states
        const size_t stateBlockHash = pipelineDescriptor._stateHash == 0u ? _context.getDefaultStateBlock(false) : pipelineDescriptor._stateHash;
        // Passing 0 is a perfectly acceptable way of enabling the default render state block
        if (stateTracker->setStateBlock(stateBlockHash) == GLStateTracker::BindResult::FAILED) {
            DIVIDE_UNEXPECTED_CALL();
        }
    }
    {
        OPTICK_EVENT("Set Blending");
        U16 i = 0u;
        stateTracker->setBlendColour(pipelineDescriptor._blendStates._blendColour);
        for (const BlendingSettings& blendState : pipelineDescriptor._blendStates._settings) {
            stateTracker->setBlending(i++, blendState);
        }
    }

    ShaderResult ret = ShaderResult::Failed;
    ShaderProgram* program = ShaderProgram::FindShaderProgram(pipelineDescriptor._shaderProgramHandle);
    glShaderProgram* glProgram = static_cast<glShaderProgram*>(program);
    if (glProgram != nullptr)
    {
        {
            OPTICK_EVENT("Set Vertex Format");
            stateTracker->setVertexFormat(pipelineDescriptor._primitiveTopology,
                                          pipelineDescriptor._primitiveRestartEnabled,
                                          pipelineDescriptor._vertexFormat,
                                          pipeline.vertexFormatHash());
        }
        {
            OPTICK_EVENT("Set Shader Program");
            // We need a valid shader as no fixed function pipeline is available
            // Try to bind the shader program. If it failed to load, or isn't loaded yet, cancel the draw request for this frame
            ret = Attorney::GLAPIShaderProgram::bind(*glProgram);
        }
 
        if (ret != ShaderResult::OK) {
            if (stateTracker->setActiveProgram(0u) == GLStateTracker::BindResult::FAILED) {
                DIVIDE_UNEXPECTED_CALL();
            }
            if (stateTracker->setActiveShaderPipeline(0u) == GLStateTracker::BindResult::FAILED) {
                DIVIDE_UNEXPECTED_CALL();
            }
            stateTracker->_activePipeline = nullptr;
        } else {
            stateTracker->_activeShaderProgram = glProgram;
            _context.descriptorSet(DescriptorSetUsage::PER_DRAW).dirty(true);
        }
    }

    return ret;
}

const GLStateTracker_uptr& GL_API::GetStateTracker() noexcept {
    DIVIDE_ASSERT(s_stateTracker != nullptr);

    return s_stateTracker;
}

GLUtil::GLMemory::GLMemoryType GL_API::GetMemoryTypeForUsage(const GLenum usage) noexcept {
    assert(usage != GL_NONE);
    switch (usage) {
        case GL_UNIFORM_BUFFER:
        case GL_SHADER_STORAGE_BUFFER:
            return GLUtil::GLMemory::GLMemoryType::SHADER_BUFFER;
        case GL_ELEMENT_ARRAY_BUFFER:
            return GLUtil::GLMemory::GLMemoryType::INDEX_BUFFER;
        case GL_ARRAY_BUFFER:
            return GLUtil::GLMemory::GLMemoryType::VERTEX_BUFFER;
    };

    return GLUtil::GLMemory::GLMemoryType::OTHER;
}

GLUtil::GLMemory::DeviceAllocator& GL_API::GetMemoryAllocator(const GLUtil::GLMemory::GLMemoryType memoryType) noexcept {
    return s_memoryAllocators[to_base(memoryType)];
}

void GL_API::QueueFlush() noexcept {
    if (Runtime::isMainThread()) {
        glFlush();
    } else {
        s_glFlushQueued.store(true);
    }
}

void GL_API::PushDebugMessage(const char* message, const U32 id) {
    OPTICK_EVENT();

    if_constexpr(Config::ENABLE_GPU_VALIDATION) {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, id, -1, message);
    }
    assert(GetStateTracker()->_debugScopeDepth < GetStateTracker()->_debugScope.size());
    GetStateTracker()->_debugScope[GetStateTracker()->_debugScopeDepth++] = { message, id };
}

void GL_API::PopDebugMessage() {
    OPTICK_EVENT();

    if_constexpr(Config::ENABLE_GPU_VALIDATION) {
        glPopDebugGroup();
    }
    GetStateTracker()->_debugScope[GetStateTracker()->_debugScopeDepth--] = { "", std::numeric_limits<U32>::max() };
}

bool GL_API::DeleteShaderPrograms(const GLuint count, GLuint* programs) {
    if (count > 0 && programs != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            if (GetStateTracker()->_activeShaderProgramHandle == programs[i]) {
                if (GetStateTracker()->setActiveProgram(0u) == GLStateTracker::BindResult::FAILED) {
                    DIVIDE_UNEXPECTED_CALL();
                }
            }
            glDeleteProgram(programs[i]);
        }
        
        memset(programs, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

bool GL_API::DeleteTextures(const GLuint count, GLuint* textures, const TextureType texType) {
    if (count > 0 && textures != nullptr) {
        
        for (GLuint i = 0u; i < count; ++i) {
            const GLuint crtTex = textures[i];
            if (crtTex != 0) {
                auto& stateTracker = GetStateTracker();

                for (GLuint& handle : stateTracker->_textureBoundMap) {
                    if (handle == crtTex) {
                        handle = GLUtil::k_invalidObjectID;
                    }
                }

                for (ImageBindSettings& settings : stateTracker->_imageBoundMap) {
                    if (settings._texture == crtTex) {
                        settings.reset();
                    }
                }
            }
        }
        glDeleteTextures(count, textures);
        memset(textures, 0, count * sizeof(GLuint));
        return true;
    }

    return false;
}

bool GL_API::DeleteSamplers(const GLuint count, GLuint* samplers) {
    if (count > 0 && samplers != nullptr) {

        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtSampler = samplers[i];
            if (crtSampler != 0) {
                for (GLuint& boundSampler : GetStateTracker()->_samplerBoundMap) {
                    if (boundSampler == crtSampler) {
                        boundSampler = 0;
                    }
                }
            }
        }
        glDeleteSamplers(count, samplers);
        memset(samplers, 0, count * sizeof(GLuint));
        return true;
    }

    return false;
}


bool GL_API::DeleteBuffers(const GLuint count, GLuint* buffers) {
    if (count > 0 && buffers != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtBuffer = buffers[i];
            auto& stateTracker = GetStateTracker();
            for (GLuint& boundBuffer : stateTracker->_activeBufferID) {
                if (boundBuffer == crtBuffer) {
                    boundBuffer = GLUtil::k_invalidObjectID;
                }
            }
            for (auto& boundBuffer : stateTracker->_activeVAOIB) {
                if (boundBuffer.second == crtBuffer) {
                    boundBuffer.second = GLUtil::k_invalidObjectID;
                }
            }
        }

        glDeleteBuffers(count, buffers);
        memset(buffers, 0, count * sizeof(GLuint));
        return true;
    }

    return false;
}

bool GL_API::DeleteVAOs(const GLuint count, GLuint* vaos) {
    if (count > 0u && vaos != nullptr) {
        for (GLuint i = 0u; i < count; ++i) {
            if (GetStateTracker()->_activeVAOID == vaos[i]) {
                GetStateTracker()->_activeVAOID = GLUtil::k_invalidObjectID;
                break;
            }
        }

        glDeleteVertexArrays(count, vaos);
        memset(vaos, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

bool GL_API::DeleteFramebuffers(const GLuint count, GLuint* framebuffers) {
    if (count > 0 && framebuffers != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtFB = framebuffers[i];
            for (GLuint& activeFB : GetStateTracker()->_activeFBID) {
                if (activeFB == crtFB) {
                    activeFB = GLUtil::k_invalidObjectID;
                }
            }
        }
        glDeleteFramebuffers(count, framebuffers);
        memset(framebuffers, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

/// Return the OpenGL sampler object's handle for the given hash value
GLuint GL_API::GetSamplerHandle(const size_t samplerHash) {
    // If the hash value is 0, we assume the code is trying to unbind a sampler object
    if (samplerHash > 0) {
        {
            SharedLock<SharedMutex> r_lock(s_samplerMapLock);
            // If we fail to find the sampler object for the given hash, we print an error and return the default OpenGL handle
            const SamplerObjectMap::const_iterator it = s_samplerMap.find(samplerHash);
            if (it != std::cend(s_samplerMap)) {
                // Return the OpenGL handle for the sampler object matching the specified hash value
                return it->second;
            }
        }
        {
            ScopedLock<SharedMutex> w_lock(s_samplerMapLock);
            // Check again
            const SamplerObjectMap::const_iterator it = s_samplerMap.find(samplerHash);
            if (it == std::cend(s_samplerMap)) {
                // Cache miss. Create the sampler object now.
                // Create and store the newly created sample object. GL_API is responsible for deleting these!
                const GLuint sampler = glSamplerObject::Construct(SamplerDescriptor::Get(samplerHash));
                emplace(s_samplerMap, samplerHash, sampler);
                return sampler;
            }
        }
    }

    return 0u;
}

glHardwareQueryPool* GL_API::GetHardwareQueryPool() noexcept {
    return s_hardwareQueryPool;
}

GLsync GL_API::CreateFenceSync() {
    OPTICK_EVENT("Create Sync");

    DIVIDE_ASSERT(s_fenceSyncCounter[s_LockFrameLifetime - 1u] < std::numeric_limits<U32>::max());

    ++s_fenceSyncCounter[s_LockFrameLifetime - 1u];
    return glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void GL_API::DestroyFenceSync(GLsync& sync) {
    OPTICK_EVENT("Delete Sync");

    DIVIDE_ASSERT(s_fenceSyncCounter[s_LockFrameLifetime - 1u] > 0u);

    --s_fenceSyncCounter[s_LockFrameLifetime - 1u];
    glDeleteSync(sync);
    sync = nullptr;
}

FrameDependendSync GL_API::CreateFrameFenceSync() {
    return {
        CreateFenceSync(),
        GFXDevice::FrameCount()
    };
}

void GL_API::DestroyFrameFenceSync(FrameDependendSync& sync) {
    if (sync._syncObject != nullptr) {
        DestroyFenceSync(sync._syncObject);
    }
    sync._frameNumber = std::numeric_limits<U64>::max();
}

};
