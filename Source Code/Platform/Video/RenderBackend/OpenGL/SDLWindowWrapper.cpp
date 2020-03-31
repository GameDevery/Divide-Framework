﻿#include "stdafx.h"

#include "Headers/GLWrapper.h"
#include "Headers/glHardwareQuery.h"

#include "GUI/Headers/GUI.h"
#include "Core/Headers/Kernel.h"
#include "Core/Headers/Console.h"
#include "Core/Headers/Application.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"

#include "Utility/Headers/Localization.h"

#include "Platform/Headers/PlatformRuntime.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/RenderBackend/OpenGL/glsw/Headers/glsw.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/Headers/glMemoryManager.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/ShaderBuffer/Headers/glUniformBuffer.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/VertexBuffer/Headers/glVertexArray.h"
#include "Platform/Video/Buffers/VertexBuffer/GenericBuffer/Headers/GenericVertexData.h"

#include <CEGUI/CEGUI.h>
#include <GL3Renderer.h>

#include <GLIM/glim.h>
#include <chrono>
#include <thread>

#define HAVE_M_PI
#include <SDL.h>

#include <glbinding/Binding.h>

namespace Divide {
namespace {
    constexpr bool g_useAALines = true;

    const U32 g_maxVAOS = 512u;
    const U32 g_maxQueryRings = 64;

    class ContextPool {
    public:
        ContextPool() noexcept
        {
            _contexts.reserve((size_t)HARDWARE_THREAD_COUNT() * 2);
        }

        ~ContextPool() 
        {
            assert(_contexts.empty());
        }

        bool init(U32 size, const DisplayWindow& window) {
            SDL_Window* raw = window.getRawWindow();
            UniqueLock lock(_glContextLock);
            _contexts.resize(size, std::make_pair(nullptr, false));
            for (std::pair<SDL_GLContext, bool>& ctx : _contexts) {
                ctx.first = SDL_GL_CreateContext(raw);
            }
            return true;
        }

        bool destroy() {
            UniqueLock lock(_glContextLock);
            for (std::pair<SDL_GLContext, bool>& ctx : _contexts) {
                SDL_GL_DeleteContext(ctx.first);
            }
            _contexts.clear();
            return true;
        }

        bool getAvailableContext(SDL_GLContext& ctx) {
            UniqueLock lock(_glContextLock);
            for (std::pair<SDL_GLContext, bool>& ctxIt : _contexts) {
                if (!ctxIt.second) {
                    ctx = ctxIt.first;
                    ctxIt.second = true;
                    return true;
                }
            }

            return false;
        }

    private:
        std::mutex _glContextLock;
        std::vector<std::pair<SDL_GLContext, bool /*in use*/>> _contexts;
    } g_ContextPool;
};

RenderAPI GL_API::renderAPI() const noexcept {
    return (s_glConfig._glES ? RenderAPI::OpenGLES : RenderAPI::OpenGL);
}

/// Try and create a valid OpenGL context taking in account the specified resolution and command line arguments
ErrorCode GL_API::initRenderingAPI(GLint argc, char** argv, Configuration& config) {
    s_enabledDebugMSGGroups = config.debug.enableDebugMsgGroups;

    // Fill our (abstract API <-> openGL) enum translation tables with proper values
    GLUtil::fillEnumTables();

    const DisplayWindow& window = _context.context().app().windowManager().getMainWindow();
    g_ContextPool.init((size_t)HARDWARE_THREAD_COUNT() * 2, window);

    SDL_GL_MakeCurrent(window.getRawWindow(), (SDL_GLContext)window.userData());
    GLUtil::s_glMainRenderWindow = &window;
    _currentContext = std::make_pair(window.getGUID(), window.userData());

    glbinding::Binding::initialize([](const char *proc) { return (glbinding::ProcAddress)SDL_GL_GetProcAddress(proc); }, true);

    if (SDL_GL_GetCurrentContext() == NULL) {
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
        } else {
            renderer = GPURenderer::UNKNOWN;
        }
    } else {
        gpuRendererStr = "Unknown GPU Renderer";
        renderer = GPURenderer::UNKNOWN;
    }

    GFXDevice::setGPURenderer(renderer);
    GFXDevice::setGPUVendor(vendor);

    if (s_hardwareQueryPool == nullptr) {
        s_hardwareQueryPool = MemoryManager_NEW glHardwareQueryPool(_context);
    }

    // OpenGL has a nifty error callback system, available in every build configuration if required
    if (Config::ENABLE_GPU_VALIDATION && config.debug.enableRenderAPIDebugging) {
        // GL_DEBUG_OUTPUT_SYNCHRONOUS is essential for debugging gl commands in the IDE
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        // hardwire our debug callback function with OpenGL's implementation
        glDebugMessageCallback((GLDEBUGPROC)GLUtil::DebugCallback, nullptr);
        if (GFXDevice::getGPUVendor() == GPUVendor::NVIDIA) {
            // nVidia flushes a lot of useful info about buffer allocations and shader
            // recompiles due to state and what now, but those aren't needed until that's
            // what's actually causing the bottlenecks
            const U32 nvidiaBufferErrors[] = { 131185, 131218, 131186 };
            // Disable shader compiler errors (shader class handles that)
            glDebugMessageControl(GL_DEBUG_SOURCE_SHADER_COMPILER, GL_DEBUG_TYPE_ERROR,
                                  GL_DONT_CARE, 0, nullptr, GL_FALSE);
            // Disable nVidia buffer allocation info (an easy enable is to change the
            // count param to 0)
            glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER,
                                  GL_DONT_CARE, 3, nvidiaBufferErrors, GL_FALSE);
            // Shader will be recompiled nVidia error
            glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_PERFORMANCE,
                                  GL_DONT_CARE, 3, nvidiaBufferErrors, GL_FALSE);
        }
    }

    // If we got here, let's figure out what capabilities we have available
    // Maximum addressable texture image units in the fragment shader
    s_maxTextureUnits = std::max(GLUtil::getGLValue(GL_MAX_TEXTURE_IMAGE_UNITS), 8);
    GLUtil::getGLValue(GL_MAX_VERTEX_ATTRIB_BINDINGS, s_maxAttribBindings);

    if (to_base(ShaderProgram::TextureUsage::COUNT) >= to_U32(s_maxTextureUnits)) {
        Console::errorfn(Locale::get(_ID("ERROR_INSUFFICIENT_TEXTURE_UNITS")));
        return ErrorCode::GFX_NOT_SUPPORTED;
    }

    if (to_base(AttribLocation::COUNT) >= to_U32(s_maxAttribBindings)) {
        Console::errorfn(Locale::get(_ID("ERROR_INSUFFICIENT_ATTRIB_BINDS")));
        return ErrorCode::GFX_NOT_SUPPORTED;
    }

    GLint majGLVersion = GLUtil::getGLValue(GL_MAJOR_VERSION);
    GLint minGLVersion = GLUtil::getGLValue(GL_MINOR_VERSION);
    Console::printfn(Locale::get(_ID("GL_MAX_VERSION")), majGLVersion, minGLVersion);

    if (majGLVersion <= 4 && minGLVersion < 3) {
        Console::errorfn(Locale::get(_ID("ERROR_OPENGL_VERSION_TO_OLD")));
        return ErrorCode::GFX_NOT_SUPPORTED;
    }

    // Maximum number of colour attachments per framebuffer
    GLUtil::getGLValue(GL_MAX_COLOR_ATTACHMENTS, s_maxFBOAttachments);

    s_stateTracker = {};
    s_stateTracker.init(nullptr);

    if (s_stateTracker._opengl46Supported) {
        glMaxShaderCompilerThreadsARB(0xFFFFFFFF);
    }

    // Cap max anisotropic level to what the hardware supports
    CLAMP(config.rendering.anisotropicFilteringLevel,
          to_U8(0),
          to_U8(s_stateTracker._opengl46Supported ? GLUtil::getGLValue(GL_MAX_TEXTURE_MAX_ANISOTROPY)
                                                  : GLUtil::getGLValue(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)));
    GL_API::s_anisoLevel = config.rendering.anisotropicFilteringLevel;

    // Number of sample buffers associated with the framebuffer & MSAA sample count
    GLint samplerBuffers = GLUtil::getGLValue(GL_SAMPLES);
    GLint sampleCount = GLUtil::getGLValue(GL_SAMPLE_BUFFERS);
    Console::printfn(Locale::get(_ID("GL_MULTI_SAMPLE_INFO")), sampleCount, samplerBuffers);
    // If we do not support MSAA on a hardware level for whatever reason, override user set MSAA levels
    if (samplerBuffers == 0 || sampleCount == 0) {
        config.rendering.msaaSamples = 0;
        config.rendering.shadowMapping.msaaSamples = 0;
    }

    if (s_stateTracker._opengl46Supported) {
        Console::printfn(Locale::get(_ID("GL_SHADER_THREADS")),
                         GLUtil::getGLValue(GL_MAX_SHADER_COMPILER_THREADS_ARB));
    }
    // Print all of the OpenGL functionality info to the console and log
    // How many uniforms can we send to fragment shaders
    Console::printfn(Locale::get(_ID("GL_MAX_UNIFORM")),
                     GLUtil::getGLValue(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS));
    // How many uniforms can we send to vertex shaders
    Console::printfn(Locale::get(_ID("GL_MAX_VERT_UNIFORM")),
                     GLUtil::getGLValue(GL_MAX_VERTEX_UNIFORM_COMPONENTS));
    // How many attributes can we send to a vertex shader
    Console::printfn(Locale::get(_ID("GL_MAX_VERT_ATTRIB")),
                     GLUtil::getGLValue(GL_MAX_VERTEX_ATTRIBS));
    // Maximum number of texture units we can address in shaders
    Console::printfn(Locale::get(_ID("GL_MAX_TEX_UNITS")),
                     GLUtil::getGLValue(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS),
                     GL_API::s_maxTextureUnits);
    // Query shading language version support
    Console::printfn(Locale::get(_ID("GL_GLSL_SUPPORT")),
                     glGetString(GL_SHADING_LANGUAGE_VERSION));
    // GPU info, including vendor, gpu and driver
    Console::printfn(Locale::get(_ID("GL_VENDOR_STRING")),
                     gpuVendorStr, gpuRendererStr, glGetString(GL_VERSION));
    // In order: Maximum number of uniform buffer binding points,
    //           maximum size in basic machine units of a uniform block and
    //           minimum required alignment for uniform buffer sizes and offset
    GLUtil::getGLValue(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, GL_API::s_UBOffsetAlignment);
    GLUtil::getGLValue(GL_MAX_UNIFORM_BLOCK_SIZE, GL_API::s_UBMaxSize);
    Console::printfn(Locale::get(_ID("GL_UBO_INFO")),
                     GLUtil::getGLValue(GL_MAX_UNIFORM_BUFFER_BINDINGS),
                     GL_API::s_UBMaxSize / 1024,
                     GL_API::s_UBOffsetAlignment);

    // In order: Maximum number of shader storage buffer binding points,
    //           maximum size in basic machine units of a shader storage block,
    //           maximum total number of active shader storage blocks that may
    //           be accessed by all active shaders and
    //           minimum required alignment for shader storage buffer sizes and
    //           offset.
    GLUtil::getGLValue(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, GL_API::s_SSBOffsetAlignment);
    GLUtil::getGLValue(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, GL_API::s_SSBMaxSize);
    Console::printfn(
        Locale::get(_ID("GL_SSBO_INFO")),
        GLUtil::getGLValue(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS),
        (GLUtil::getGLValue(GL_MAX_SHADER_STORAGE_BLOCK_SIZE) / 1024) / 1024,
        GLUtil::getGLValue(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS),
        GL_API::s_SSBOffsetAlignment);

    // Maximum number of subroutines and maximum number of subroutine uniform
    // locations usable in a shader
    Console::printfn(Locale::get(_ID("GL_SUBROUTINE_INFO")),
                     GLUtil::getGLValue(GL_MAX_SUBROUTINES),
                     GLUtil::getGLValue(GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS));

    // Seamless cubemaps are a nice feature to have enabled (core since 3.2)
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    //glEnable(GL_FRAMEBUFFER_SRGB);
    // Enable multisampling if we actually support and request it
    (config.rendering.msaaSamples > 0 || config.rendering.shadowMapping.msaaSamples > 0) 
        ? glEnable(GL_MULTISAMPLE)
        : glDisable(GL_MULTISAMPLE);

    // Line smoothing should almost always be used
    if (g_useAALines) {
        glEnable(GL_LINE_SMOOTH);
        glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, &s_lineWidthLimit);
    } else {
        glGetIntegerv(GL_ALIASED_LINE_WIDTH_RANGE, &s_lineWidthLimit);
    }

    // Culling is enabled by default, but RenderStateBlocks can toggle it on a per-draw call basis
    glEnable(GL_CULL_FACE);

    // Enable all 6 clip planes, I guess
    for (U8 i = 0; i < to_U8(Frustum::FrustPlane::COUNT); ++i) {
        glEnable(GLenum((U32)GL_CLIP_DISTANCE0 + i));
    }

    vectorEASTL<std::pair<GLenum, U32>> poolSizes = {
        {GL_NONE, 256}, //Generic texture handles (created with glGen instead of glCreate)
        {GL_TEXTURE_2D, 1024}, //Used by most renderable items
        {GL_TEXTURE_2D_ARRAY, 256}, //Used mainly by shadow maps and some materials
        {GL_TEXTURE_2D_MULTISAMPLE, 128}, //Used by render tartgets
        {GL_TEXTURE_CUBE_MAP, 64}, //Used for reflections and environment probes
        {GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 16}, //Used by the CSM system mostly
        {GL_TEXTURE_3D, 8} //Will eventually be usefull for volumetric stuff
    };

    s_texturePool.init(poolSizes);

    // Prepare font rendering subsystem
    if (!createFonsContext()) {
        Console::errorfn(Locale::get(_ID("ERROR_FONT_INIT")));
        return ErrorCode::FONT_INIT_ERROR;
    }

    // Prepare immediate mode emulation rendering
    NS_GLIM::glim.SetVertexAttribLocation(to_base(AttribLocation::POSITION));
    // Initialize our VAO pool
    GL_API::s_vaoPool.init(g_maxVAOS);
    // Initialize our query pool
    GL_API::s_hardwareQueryPool->init(g_maxQueryRings);
    // Initialize shader buffers
    glUniformBuffer::onGLInit();
    // Init static program data
    glShaderProgram::onStartup(_context, _context.parent().resourceCache());
    // We need a dummy VAO object for point rendering
    s_dummyVAO = GL_API::s_vaoPool.allocate();

    // Once OpenGL is ready for rendering, init CEGUI
    _GUIGLrenderer = &CEGUI::OpenGL3Renderer::create();
    _GUIGLrenderer->enableExtraStateSettings(config.gui.cegui.extraStates);
    _context.context().gui().setRenderer(*_GUIGLrenderer);

    glClearColor(DefaultColours::DIVIDE_BLUE.r,
                 DefaultColours::DIVIDE_BLUE.g,
                 DefaultColours::DIVIDE_BLUE.b,
                 DefaultColours::DIVIDE_BLUE.a);

    _elapsedTimeQuery = std::make_shared<glHardwareQueryRing>(_context, 6);

    // Prepare shader headers and various shader related states
    if (initShaders() && initGLSW(config)) {
        // That's it. Everything should be ready for draw calls
        Console::printfn(Locale::get(_ID("START_OGL_API_OK")));

        return ErrorCode::NO_ERR;
    }


    return ErrorCode::GLSL_INIT_ERROR;
}

/// Clear everything that was setup in initRenderingAPI()
void GL_API::closeRenderingAPI() {
    glShaderProgram::onShutdown();
    if (!deInitShaders()) {
        DIVIDE_UNEXPECTED_CALL("GLSL failed to shutdown!");
    } else {
        deInitGLSW();
    }

    if (_GUIGLrenderer) {
        CEGUI::OpenGL3Renderer::destroy(*_GUIGLrenderer);
        _GUIGLrenderer = nullptr;
    }
    // Destroy sampler objects
    {
        UniqueLock w_lock(s_samplerMapLock);
        for (auto sampler : s_samplerMap) {
            glSamplerObject::destruct(sampler.second);
        }
        s_samplerMap.clear();
    }
    // Destroy the text rendering system
    deleteFonsContext();
    _fonts.clear();
    if (s_dummyVAO > 0) {
        GL_API::deleteVAOs(1, &s_dummyVAO);
    }
    s_texturePool.destroy();
    glVertexArray::cleanup();
    GLUtil::clearVBOs();
    GL_API::s_vaoPool.destroy();
    if (GL_API::s_hardwareQueryPool != nullptr) {
        GL_API::s_hardwareQueryPool->destroy();
        MemoryManager::DELETE(s_hardwareQueryPool);
    }

    g_ContextPool.destroy();
}

vec2<U16> GL_API::getDrawableSize(const DisplayWindow& window) const {
    int w = 1, h = 1;
    SDL_GL_GetDrawableSize(window.getRawWindow(), &w, &h);
    return vec2<U16>(w, h);
}

void GL_API::queueComputeMipMap(GLuint textureHandle) {
    UniqueLockShared w_lock(s_mipmapQueueSetLock);
    if (s_mipmapQueue.find(textureHandle) != std::cend(s_mipmapQueue)) {
        return;
    }
    s_mipmapQueue.insert(textureHandle);
}

void GL_API::dequeueComputeMipMap(GLuint textureHandle) {
    UniqueLockShared w_lock(s_mipmapQueueSetLock);
    const auto it = s_mipmapQueue.find(textureHandle);
    if (it != std::cend(s_mipmapQueue)) {
        s_mipmapQueue.erase(it);
    }
}

void GL_API::onThreadCreated(const std::thread::id& threadID) {
    // Double check so that we don't run into a race condition!
    UniqueLock lock(GLUtil::s_glSecondaryContextMutex);
    assert(SDL_GL_GetCurrentContext() == NULL);

    // This also makes the context current
    assert(GLUtil::s_glSecondaryContext == nullptr && "GL_API::syncToThread: double init context for current thread!");
    const bool ctxFound = g_ContextPool.getAvailableContext(GLUtil::s_glSecondaryContext);
    assert(ctxFound && "GL_API::syncToThread: context not found for current thread!");
    ACKNOWLEDGE_UNUSED(ctxFound);

    SDL_GL_MakeCurrent(GLUtil::s_glMainRenderWindow->getRawWindow(), GLUtil::s_glSecondaryContext);
    glbinding::Binding::initialize([](const char* proc) {
        return (glbinding::ProcAddress)SDL_GL_GetProcAddress(proc);
    });
    
    // Enable OpenGL debug callbacks for this context as well
    if (Config::ENABLE_GPU_VALIDATION) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        // Debug callback in a separate thread requires a flag to distinguish it
        // from the main thread's callbacks
        glDebugMessageCallback((GLDEBUGPROC)GLUtil::DebugCallback, GLUtil::s_glSecondaryContext);
    }

    if (s_stateTracker._opengl46Supported) {
        glMaxShaderCompilerThreadsARB(0xFFFFFFFF);
    }

    initGLSW(_context.context().config());
}
};
