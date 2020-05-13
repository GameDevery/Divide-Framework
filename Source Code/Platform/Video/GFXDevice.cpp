#include "stdafx.h"

#include "config.h"

#include "Headers/GFXDevice.h"

#include "Editor/Headers/Editor.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/ParamHandler.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Time/Headers/ProfileTimer.h"
#include "Core/Time/Headers/ApplicationTimer.h"

#include "Managers/Headers/SceneManager.h"
#include "Managers/Headers/RenderPassManager.h"

#include "Rendering/Headers/Renderer.h"
#include "Rendering/PostFX/Headers/PostFX.h"
#include "Rendering/Camera/Headers/FreeFlyCamera.h"

#include "Geometry/Material/Headers/ShaderComputeQueue.h"

#include "Platform/Headers/PlatformRuntime.h"
#include "Platform/Video/Headers/IMPrimitive.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Platform/Video/Textures/Headers/Texture.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"
#include "Platform/Video/Buffers/ShaderBuffer/Headers/ShaderBuffer.h"

#include "Platform/Video/RenderBackend/OpenGL/Headers/GLWrapper.h"
#include "Platform/Video/RenderBackend/Vulkan/Headers/VKWrapper.h"
#include "Platform/Video/RenderBackend/None/Headers/NoneWrapper.h"

#include "Platform/Video/RenderBackend/Vulkan/Headers/VKWrapper.h"
#include "Platform/Video/RenderBackend/None/Headers/NoneWrapper.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/glIMPrimitive.h"
#include "Platform/Video/RenderBackend/OpenGL/Textures/Headers/glTexture.h"
#include "Platform/Video/RenderBackend/OpenGL/Shaders/Headers/glShaderProgram.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/ShaderBuffer/Headers/glUniformBuffer.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/RenderTarget/Headers/glFramebuffer.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/VertexBuffer/Headers/glVertexArray.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/PixelBuffer/Headers/glPixelBuffer.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/VertexBuffer/Headers/glGenericVertexData.h"

#include <RenderDoc-Manager/RenderDocManager.h>

namespace Divide {
    constexpr HiZMethod GetHiZMethod() { return HiZMethod::RASTER_GRID; }

namespace TypeUtil {
    const char* GraphicResourceTypeToName(GraphicsResource::Type type) noexcept {
        return Names::resourceTypes[to_base(type)];
    };

    const char* RenderStageToString(RenderStage stage) noexcept {
        return Names::renderStage[to_base(stage)];
    }

    RenderStage StringToRenderStage(const char* stage) noexcept {
        for (U8 i = 0; i < to_U8(RenderStage::COUNT); ++i) {
            if (strcmp(stage, Names::renderStage[i]) == 0) {
                return static_cast<RenderStage>(i);
            }
        }

        return RenderStage::COUNT;
    }

    const char* RenderPassTypeToString(RenderPassType pass) noexcept {
        return Names::renderPassType[to_base(pass)];
    }

    RenderPassType StringToRenderPassType(const char* pass) noexcept {
        for (U8 i = 0; i < to_U8(RenderPassType::COUNT); ++i) {
            if (strcmp(pass, Names::renderPassType[i]) == 0) {
                return static_cast<RenderPassType>(i);
            }
        }

        return RenderPassType::COUNT;
    }
};

D64 GFXDevice::s_interpolationFactor = 1.0;
GPUVendor GFXDevice::s_GPUVendor = GPUVendor::COUNT;
GPURenderer GFXDevice::s_GPURenderer = GPURenderer::COUNT;

#pragma region Construction, destruction, initialization
GFXDevice::GFXDevice(Kernel & parent)
    : KernelComponent(parent),
    PlatformContextComponent(parent.platformContext()),
    _clippingPlanes(Plane<F32>(0, 0, 0, 0))
{
    _viewport.set(-1);

    Line temp = {};
    temp.widthStart(3.0f);
    temp.widthEnd(3.0f);
    temp.positionStart(VECTOR3_ZERO);

    // Red X-axis
    temp.positionEnd(WORLD_X_AXIS * 2);
    temp.colourStart(UColour4(255, 0, 0, 255));
    temp.colourEnd(UColour4(255, 0, 0, 255));
    _axisLines[0] = temp;

    // Green Y-axis
    temp.positionEnd(WORLD_Y_AXIS * 2);
    temp.colourStart(UColour4(0, 255, 0, 255));
    temp.colourEnd(UColour4(0, 255, 0, 255));
    _axisLines[1] = temp;

    // Blue Z-axis
    temp.positionEnd(WORLD_Z_AXIS * 2);
    temp.colourStart(UColour4(0, 0, 255, 255));
    temp.colourEnd(UColour4(0, 0, 255, 255));
    _axisLines[2] = temp;

    AttribFlags flags;
    flags.fill(true);
    VertexBuffer::setAttribMasks(to_size(to_base(RenderStage::COUNT) * to_base(RenderPassType::COUNT)), flags);

    // Don't (currently) need these for shadow passes
    flags[to_base(AttribLocation::COLOR)] = false;
    for (U8 stage = 0; stage < to_base(RenderStage::COUNT); ++stage) {
        VertexBuffer::setAttribMask(RenderStagePass::baseIndex(static_cast<RenderStage>(stage), RenderPassType::PRE_PASS), flags);
    }
    flags[to_base(AttribLocation::NORMAL)] = false;
    flags[to_base(AttribLocation::TANGENT)] = false;
    for (U8 pass = 0; pass < to_base(RenderPassType::COUNT); ++pass) {
        VertexBuffer::setAttribMask(RenderStagePass::baseIndex(RenderStage::SHADOW, static_cast<RenderPassType>(pass)), flags);
    }
}

GFXDevice::~GFXDevice()
{
    closeRenderingAPI();
}

ErrorCode GFXDevice::createAPIInstance(RenderAPI API) {
    assert(_api == nullptr && "GFXDevice error: initRenderingAPI called twice!");

    ErrorCode err = ErrorCode::NO_ERR;
    switch (API) {
        case RenderAPI::OpenGL:
        case RenderAPI::OpenGLES: {
            _api = std::make_unique<GL_API>(*this, API == RenderAPI::OpenGLES);
        } break;
        case RenderAPI::Vulkan: {
            _api = std::make_unique<VK_API>(*this);
        } break;
        case RenderAPI::None: {
            _api = std::make_unique<NONE_API>(*this);
        } break;
        default:
            err = ErrorCode::GFX_NON_SPECIFIED;
            break;
    };

    DIVIDE_ASSERT(_api != nullptr, Locale::get(_ID("ERROR_GFX_DEVICE_API")));
    renderAPI(API);

    return err;
}

/// Create a display context using the selected API and create all of the needed
/// primitives needed for frame rendering
ErrorCode GFXDevice::initRenderingAPI(I32 argc, char** argv, RenderAPI API, const vec2<U16> & renderResolution) {
    ErrorCode hardwareState = createAPIInstance(API);
    Configuration& config = _parent.platformContext().config();

    if (hardwareState == ErrorCode::NO_ERR) {
        // Initialize the rendering API
        if_constexpr(Config::ENABLE_GPU_VALIDATION) {
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
    const size_t displayCount = gpuState().getDisplayCount();
    for (size_t idx = 0; idx < displayCount; ++idx) {
        const vectorEASTL<GPUState::GPUVideoMode>& registeredModes = gpuState().getDisplayModes(idx);
        Console::printfn(Locale::get(_ID("AVAILABLE_VIDEO_MODES")), idx, registeredModes.size());

        for (const GPUState::GPUVideoMode& mode : registeredModes) {
            // Optionally, output to console/file each display mode
            refreshRates = Util::StringFormat("%d", mode._refreshRate);
            Console::printfn(Locale::get(_ID("CURRENT_DISPLAY_MODE")),
                mode._resolution.width,
                mode._resolution.height,
                mode._bitDepth,
                mode._formatName.c_str(),
                refreshRates.c_str());
        }
    }

    ResourceCache* cache = parent().resourceCache();
    _rtPool = MemoryManager_NEW GFXRTPool(*this);

    // Quarter of a megabyte in size should work. I think.
    constexpr size_t TargetBufferSize = (1024 * 1024) / 4 / sizeof(GFXShaderData::GPUData);

    // Initialize the shader manager
    ShaderProgram::onStartup(*this, cache);
    EnvironmentProbe::onStartup(*this);
    GFX::initPools();

    // Create a shader buffer to store the GFX rendering info (matrices, options, etc)
    ShaderBufferDescriptor bufferDescriptor = {};
    bufferDescriptor._usage = ShaderBuffer::Usage::CONSTANT_BUFFER;
    bufferDescriptor._elementCount = 1;
    bufferDescriptor._elementSize = sizeof(GFXShaderData::GPUData);
    bufferDescriptor._ringBufferLength = TargetBufferSize;
    bufferDescriptor._separateReadWrite = false;
    bufferDescriptor._updateFrequency = BufferUpdateFrequency::OFTEN;
    bufferDescriptor._updateUsage = BufferUpdateUsage::CPU_W_GPU_R;
    bufferDescriptor._initialData = &_gpuBlock._data;
    bufferDescriptor._name = "DVD_GPU_DATA";
    bufferDescriptor._flags = to_base(ShaderBuffer::Flags::AUTO_RANGE_FLUSH);
    bufferDescriptor._flags |= to_base(ShaderBuffer::Flags::AUTO_STORAGE);
    _gfxDataBuffer = newSB(bufferDescriptor);

    _shaderComputeQueue = MemoryManager_NEW ShaderComputeQueue(cache);

    // Create general purpose render state blocks
    RenderStateBlock defaultStateNoDepth;
    defaultStateNoDepth.depthTestEnabled(false);
    _defaultStateNoDepthHash = defaultStateNoDepth.getHash();

    RenderStateBlock state2DRendering;
    state2DRendering.setCullMode(CullMode::NONE);
    state2DRendering.depthTestEnabled(false);
    _state2DRenderingHash = state2DRendering.getHash();

    RenderStateBlock stateDepthOnlyRendering;
    stateDepthOnlyRendering.setColourWrites(false, false, false, false);
    stateDepthOnlyRendering.setZFunc(ComparisonFunction::ALWAYS);
    _stateDepthOnlyRenderingHash = stateDepthOnlyRendering.getHash();

    // The general purpose render state blocks are both mandatory and must
    // differ from each other at a state hash level
    assert(_stateDepthOnlyRenderingHash != _state2DRenderingHash && "GFXDevice error: Invalid default state hash detected!");
    assert(_state2DRenderingHash != _defaultStateNoDepthHash && "GFXDevice error: Invalid default state hash detected!");
    assert(_defaultStateNoDepthHash != RenderStateBlock::defaultHash() && "GFXDevice error: Invalid default state hash detected!");

    // We need to create all of our attachments for the default render targets
    // Start with the screen render target: Try a half float, multisampled
    // buffer (MSAA + HDR rendering if possible)

    SamplerDescriptor defaultSampler = {};
    defaultSampler.wrapU(TextureWrap::CLAMP_TO_EDGE);
    defaultSampler.wrapV(TextureWrap::CLAMP_TO_EDGE);
    defaultSampler.wrapW(TextureWrap::CLAMP_TO_EDGE);
    defaultSampler.minFilter(TextureFilter::NEAREST);
    defaultSampler.magFilter(TextureFilter::NEAREST);

    TextureDescriptor screenDescriptor(TextureType::TEXTURE_2D_MS, GFXImageFormat::RGBA, GFXDataFormat::FLOAT_16);
    TextureDescriptor normalAndVelocityDescriptor(TextureType::TEXTURE_2D_MS, GFXImageFormat::RGBA, GFXDataFormat::FLOAT_16);
    TextureDescriptor lightingDetails(TextureType::TEXTURE_2D_MS, GFXImageFormat::RGB, GFXDataFormat::FLOAT_16);
    TextureDescriptor depthDescriptor(TextureType::TEXTURE_2D_MS, GFXImageFormat::DEPTH_COMPONENT, GFXDataFormat::FLOAT_32);

    // Normal and MSAA
    for (U8 i = 0; i < 2; ++i) {
        const U8 sampleCount = i == 0 ? 0 : config.rendering.MSAAsamples;

        screenDescriptor.samplerDescriptor(defaultSampler);
        screenDescriptor.msaaSamples(sampleCount);

        depthDescriptor.samplerDescriptor(defaultSampler);
        depthDescriptor.msaaSamples(sampleCount);

        normalAndVelocityDescriptor.samplerDescriptor(defaultSampler);
        normalAndVelocityDescriptor.msaaSamples(sampleCount);

        lightingDetails.samplerDescriptor(defaultSampler);
        lightingDetails.msaaSamples(sampleCount);

        {
            vectorEASTL<RTAttachmentDescriptor> attachments = {
                { screenDescriptor,              RTAttachmentType::Colour, to_U8(ScreenTargets::ALBEDO), DefaultColours::DIVIDE_BLUE },
                { normalAndVelocityDescriptor,   RTAttachmentType::Colour, to_U8(ScreenTargets::NORMALS_AND_VELOCITY), VECTOR4_ZERO },
                { lightingDetails,               RTAttachmentType::Colour, to_U8(ScreenTargets::EXTRA), vec4<F32>(1.0f, 1.0f, 1.0f, 0.0f) },
                { depthDescriptor,               RTAttachmentType::Depth }
            };

            RenderTargetDescriptor screenDesc = {};
            screenDesc._name = "Screen";
            screenDesc._resolution = renderResolution;
            screenDesc._attachmentCount = to_U8(attachments.size());
            screenDesc._attachments = attachments.data();
            screenDesc._msaaSamples = sampleCount;

            // Our default render targets hold the screen buffer, depth buffer, and a special, on demand, down-sampled version of the depth buffer
            _rtPool->allocateRT(i == 0 ? RenderTargetUsage::SCREEN : RenderTargetUsage::SCREEN_MS, screenDesc);
        }
    }

    const U16 reflectRes = 512 * config.rendering.reflectionResolutionFactor;

    TextureDescriptor hiZDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::DEPTH_COMPONENT, GFXDataFormat::UNSIGNED_INT);

    SamplerDescriptor hiZSampler = {};
    hiZSampler.wrapU(TextureWrap::CLAMP_TO_EDGE);
    hiZSampler.wrapV(TextureWrap::CLAMP_TO_EDGE);
    hiZSampler.wrapW(TextureWrap::CLAMP_TO_EDGE);
    hiZSampler.anisotropyLevel(0u);

    if (GetHiZMethod() == HiZMethod::ARM) {
        hiZSampler.magFilter(TextureFilter::LINEAR);
        hiZSampler.minFilter(TextureFilter::LINEAR_MIPMAP_NEAREST);
        hiZSampler.useRefCompare(true);
        hiZSampler.cmpFunc(ComparisonFunction::LEQUAL);
    } else {
        hiZSampler.magFilter(TextureFilter::NEAREST);
        hiZSampler.minFilter(TextureFilter::NEAREST_MIPMAP_NEAREST);
    }

    hiZDescriptor.samplerDescriptor(hiZSampler);
    hiZDescriptor.autoMipMaps(false);

    vectorEASTL<RTAttachmentDescriptor> hiZAttachments = {
        { hiZDescriptor, RTAttachmentType::Depth, 0, VECTOR4_ZERO },
    };

    {
        RenderTargetDescriptor hizRTDesc = {};
        hizRTDesc._name = "HiZ";
        hizRTDesc._resolution = renderResolution;
        hizRTDesc._attachmentCount = to_U8(hiZAttachments.size());
        hizRTDesc._attachments = hiZAttachments.data();
        _rtPool->allocateRT(RenderTargetUsage::HI_Z, hizRTDesc);

        hizRTDesc._resolution = reflectRes;
        hizRTDesc._name = "HiZ_Reflect";
        _rtPool->allocateRT(RenderTargetUsage::HI_Z_REFLECT, hizRTDesc);
    }

    if_constexpr(Config::Build::ENABLE_EDITOR) {
        SamplerDescriptor editorSampler = {};
        editorSampler.minFilter(TextureFilter::LINEAR_MIPMAP_LINEAR);
        editorSampler.magFilter(TextureFilter::LINEAR);
        editorSampler.wrapU(TextureWrap::CLAMP_TO_EDGE);
        editorSampler.wrapV(TextureWrap::CLAMP_TO_EDGE);
        editorSampler.wrapW(TextureWrap::CLAMP_TO_EDGE);
        editorSampler.anisotropyLevel(0);

        TextureDescriptor editorDescriptor(TextureType::TEXTURE_2D, GFXImageFormat::RGB, GFXDataFormat::UNSIGNED_BYTE);
        editorDescriptor.samplerDescriptor(editorSampler);

        vectorEASTL<RTAttachmentDescriptor> attachments = {
            { editorDescriptor, RTAttachmentType::Colour, to_U8(ScreenTargets::ALBEDO), DefaultColours::DIVIDE_BLUE }
        };

        RenderTargetDescriptor editorDesc = {};
        editorDesc._name = "Editor";
        editorDesc._resolution = renderResolution;
        editorDesc._attachmentCount = to_U8(attachments.size());
        editorDesc._attachments = attachments.data();
        _rtPool->allocateRT(RenderTargetUsage::EDITOR, editorDesc);
    }
    // Reflection Targets
    SamplerDescriptor reflectionSampler = {};
    reflectionSampler.wrapU(TextureWrap::CLAMP_TO_EDGE);
    reflectionSampler.wrapV(TextureWrap::CLAMP_TO_EDGE);
    reflectionSampler.wrapW(TextureWrap::CLAMP_TO_EDGE);
    reflectionSampler.minFilter(TextureFilter::NEAREST);
    reflectionSampler.magFilter(TextureFilter::NEAREST);

    {
        // "A" could be used for anything (e.g. depth)
        TextureDescriptor environmentDescriptorPlanar(TextureType::TEXTURE_2D, GFXImageFormat::RGBA, GFXDataFormat::UNSIGNED_BYTE);
        environmentDescriptorPlanar.samplerDescriptor(reflectionSampler);

        TextureDescriptor depthDescriptorPlanar(TextureType::TEXTURE_2D, GFXImageFormat::DEPTH_COMPONENT, GFXDataFormat::FLOAT_32);
        depthDescriptorPlanar.samplerDescriptor(reflectionSampler);

        RenderTargetDescriptor hizRTDesc = {};
        hizRTDesc._resolution = reflectRes;
        hizRTDesc._attachmentCount = to_U8(hiZAttachments.size());
        hizRTDesc._attachments = hiZAttachments.data();

        {
            vectorEASTL<RTAttachmentDescriptor> attachments = {
                { environmentDescriptorPlanar, RTAttachmentType::Colour },
                { depthDescriptorPlanar, RTAttachmentType::Depth },
            };

            RenderTargetDescriptor refDesc = {};
            refDesc._resolution = vec2<U16>(reflectRes);
            refDesc._attachmentCount = to_U8(attachments.size());
            refDesc._attachments = attachments.data();

            for (U32 i = 0; i < Config::MAX_REFLECTIVE_NODES_IN_VIEW; ++i) {
                refDesc._name = Util::StringFormat("Reflection_Planar_%d", i);
                _rtPool->allocateRT(RenderTargetUsage::REFLECTION_PLANAR, refDesc);
            }

            for (U32 i = 0; i < Config::MAX_REFRACTIVE_NODES_IN_VIEW; ++i) {
                refDesc._name = Util::StringFormat("Refraction_Planar_%d", i);
                _rtPool->allocateRT(RenderTargetUsage::REFRACTION_PLANAR, refDesc);
            }

            refDesc._attachmentCount = 1; //skip depth
            _rtPool->allocateRT(RenderTargetUsage::REFLECTION_PLANAR_BLUR, refDesc);

        }
    }

    SamplerDescriptor accumulationSampler = {};
    accumulationSampler.wrapU(TextureWrap::CLAMP_TO_EDGE);
    accumulationSampler.wrapV(TextureWrap::CLAMP_TO_EDGE);
    accumulationSampler.wrapW(TextureWrap::CLAMP_TO_EDGE);
    accumulationSampler.minFilter(TextureFilter::NEAREST);
    accumulationSampler.magFilter(TextureFilter::NEAREST);

    TextureDescriptor accumulationDescriptor(TextureType::TEXTURE_2D_MS, GFXImageFormat::RGBA, GFXDataFormat::FLOAT_16);
    accumulationDescriptor.autoMipMaps(false);
    accumulationDescriptor.samplerDescriptor(accumulationSampler);

    TextureDescriptor revealageDescriptor(TextureType::TEXTURE_2D_MS, GFXImageFormat::RED, GFXDataFormat::FLOAT_16);
    revealageDescriptor.autoMipMaps(false);
    revealageDescriptor.samplerDescriptor(accumulationSampler);

    vectorEASTL<RTAttachmentDescriptor> oitAttachments = {
        { accumulationDescriptor, RTAttachmentType::Colour, to_U8(ScreenTargets::ACCUMULATION), VECTOR4_ZERO },
        { revealageDescriptor, RTAttachmentType::Colour, to_U8(ScreenTargets::REVEALAGE), VECTOR4_UNIT }
    };

    for (U8 i = 0; i < 2; ++i) 
    {
        const U8 sampleCount = i == 0 ? 0 : config.rendering.MSAAsamples;

        oitAttachments[0]._texDescriptor.msaaSamples(sampleCount);
        oitAttachments[1]._texDescriptor.msaaSamples(sampleCount);

        const RenderTarget& screenTarget = _rtPool->renderTarget(i == 0 ? RenderTargetUsage::SCREEN : RenderTargetUsage::SCREEN_MS);
        const RTAttachment_ptr& screenDepthAttachment = screenTarget.getAttachmentPtr(RTAttachmentType::Depth, 0);
        
        vectorEASTL<ExternalRTAttachmentDescriptor> externalAttachments = {
            { screenDepthAttachment,  RTAttachmentType::Depth }
        };

        if_constexpr(Config::USE_COLOURED_WOIT) {
            const RTAttachment_ptr& screenAttachment = screenTarget.getAttachmentPtr(RTAttachmentType::Colour, to_U8(ScreenTargets::ALBEDO));
            externalAttachments.push_back(
                { screenAttachment,  RTAttachmentType::Colour, to_U8(ScreenTargets::MODULATE) }
            );
        }

        RenderTargetDescriptor oitDesc = {};
        oitDesc._name = "OIT_FULL_RES";
        oitDesc._resolution = renderResolution;
        oitDesc._attachmentCount = to_U8(oitAttachments.size());
        oitDesc._attachments = oitAttachments.data();
        oitDesc._externalAttachmentCount = to_U8(externalAttachments.size());
        oitDesc._externalAttachments = externalAttachments.data();
        oitDesc._msaaSamples = sampleCount;
        _rtPool->allocateRT(i == 0 ? RenderTargetUsage::OIT : RenderTargetUsage::OIT_MS, oitDesc);
    }
    {
        oitAttachments[0]._texDescriptor.msaaSamples(0);
        oitAttachments[1]._texDescriptor.msaaSamples(0);

        for (U16 i = 0; i < Config::MAX_REFLECTIVE_NODES_IN_VIEW; ++i) {
            const RenderTarget& reflectTarget = _rtPool->renderTarget(RenderTargetID(RenderTargetUsage::REFLECTION_PLANAR, i));
            const RTAttachment_ptr& depthAttachment = reflectTarget.getAttachmentPtr(RTAttachmentType::Depth, 0);

            vectorEASTL<ExternalRTAttachmentDescriptor> externalAttachments = {
                 { depthAttachment,  RTAttachmentType::Depth }
            };

            if_constexpr(Config::USE_COLOURED_WOIT) {
                const RTAttachment_ptr& screenAttachment = reflectTarget.getAttachmentPtr(RTAttachmentType::Colour, 0);
                externalAttachments.push_back(
                    { screenAttachment,  RTAttachmentType::Colour, to_U8(ScreenTargets::MODULATE) }
                );
            }

            RenderTargetDescriptor oitDesc = {};
            oitDesc._name = Util::StringFormat("OIT_REFLECT_RES_%d", i);
            oitDesc._resolution = vec2<U16>(reflectRes);
            oitDesc._attachmentCount = to_U8(oitAttachments.size());
            oitDesc._attachments = oitAttachments.data();
            oitDesc._externalAttachmentCount = to_U8(externalAttachments.size());
            oitDesc._externalAttachments = externalAttachments.data();
            oitDesc._msaaSamples = 0;
            _rtPool->allocateRT(RenderTargetUsage::OIT_REFLECT, oitDesc);
        }
    }
    {
        TextureDescriptor environmentDescriptorCube(TextureType::TEXTURE_CUBE_ARRAY, GFXImageFormat::RGBA, GFXDataFormat::UNSIGNED_BYTE);
        environmentDescriptorCube.samplerDescriptor(reflectionSampler);

        TextureDescriptor depthDescriptorCube(TextureType::TEXTURE_CUBE_ARRAY, GFXImageFormat::DEPTH_COMPONENT, GFXDataFormat::FLOAT_32);
        depthDescriptorCube.samplerDescriptor(reflectionSampler);

        vectorEASTL<RTAttachmentDescriptor> attachments = {
            { environmentDescriptorCube, RTAttachmentType::Colour },
            { depthDescriptorCube, RTAttachmentType::Depth },
        };

        RenderTargetDescriptor refDesc = {};
        refDesc._resolution = vec2<U16>(reflectRes);
        refDesc._attachmentCount = to_U8(attachments.size());
        refDesc._attachments = attachments.data();

        refDesc._name = "Reflection_Cube_Array";
        _rtPool->allocateRT(RenderTargetUsage::REFLECTION_CUBE, refDesc);
    }

    return ErrorCode::NO_ERR;
}

ErrorCode GFXDevice::postInitRenderingAPI() {
    std::atomic_uint loadTasks = 0;

    ResourceCache* cache = parent().resourceCache();
    {
        ShaderModuleDescriptor vertModule = {};
        vertModule._moduleType = ShaderType::VERTEX;
        vertModule._sourceFile = "ImmediateModeEmulation.glsl";
        vertModule._variant = "GUI";

        ShaderModuleDescriptor fragModule = {};
        fragModule._moduleType = ShaderType::FRAGMENT;
        fragModule._sourceFile = "ImmediateModeEmulation.glsl";
        fragModule._variant = "GUI";

        ShaderProgramDescriptor shaderDescriptor = {};
        shaderDescriptor._modules.push_back(vertModule);
        shaderDescriptor._modules.push_back(fragModule);

        ResourceDescriptor immediateModeShader("ImmediateModeEmulationGUI");
        immediateModeShader.waitForReady(true);
        immediateModeShader.propertyDescriptor(shaderDescriptor);
        _textRenderShader = CreateResource<ShaderProgram>(cache, immediateModeShader, loadTasks);
        _textRenderShader->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
            PipelineDescriptor descriptor = {};
            descriptor._shaderProgramHandle = res->getGUID();
            descriptor._stateHash = get2DStateBlock();
            _textRenderPipeline = newPipeline(descriptor);
        });
    }
    {
        ShaderModuleDescriptor vertModule = {};
        vertModule._moduleType = ShaderType::VERTEX;
        vertModule._sourceFile = "baseVertexShaders.glsl";
        vertModule._variant = "FullScreenQuad";

        ShaderModuleDescriptor fragModule = {};
        fragModule._moduleType = ShaderType::FRAGMENT;
        fragModule._sourceFile = "HiZConstruct.glsl";
        switch (GetHiZMethod()) {
            case HiZMethod::NVIDIA:
                fragModule._variant = "Nvidia";
                break;
            case HiZMethod::ARM:
                fragModule._variant = "Arm";
                break;
            case HiZMethod::RASTER_GRID:
            default:
                fragModule._variant = "RasterGrid";
                break;
        };

        ShaderProgramDescriptor shaderDescriptor = {};
        shaderDescriptor._modules.push_back(vertModule);
        shaderDescriptor._modules.push_back(fragModule);

        // Initialized our HierarchicalZ construction shader (takes a depth attachment and down-samples it for every mip level)
        ResourceDescriptor descriptor1("HiZConstruct");
        descriptor1.waitForReady(false);
        descriptor1.propertyDescriptor(shaderDescriptor);
        _HIZConstructProgram = CreateResource<ShaderProgram>(cache, descriptor1, loadTasks);
        _HIZConstructProgram->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
            PipelineDescriptor pipelineDesc;
            pipelineDesc._stateHash = _stateDepthOnlyRenderingHash;
            pipelineDesc._shaderProgramHandle = _HIZConstructProgram->getGUID();
            _HIZPipeline = newPipeline(pipelineDesc);
        });
    }
    {
        ShaderModuleDescriptor compModule = {};
        compModule._moduleType = ShaderType::COMPUTE;
        switch (GetHiZMethod()) {
            case HiZMethod::ARM:
                compModule._defines.emplace_back("USE_ARM", true);
                break;
            case HiZMethod::NVIDIA:
                compModule._defines.emplace_back("USE_NVIDIA", true);
                break;
            default:
            case HiZMethod::RASTER_GRID:
                compModule._defines.emplace_back("USE_RASTERGRID", true);
                break;
        };
        compModule._sourceFile = "HiZOcclusionCull.glsl";

        ShaderProgramDescriptor shaderDescriptor = {};
        shaderDescriptor._modules.push_back(compModule);

        ResourceDescriptor descriptor2("HiZOcclusionCull");
        descriptor2.waitForReady(false);
        descriptor2.propertyDescriptor(shaderDescriptor);
        _HIZCullProgram = CreateResource<ShaderProgram>(cache, descriptor2, loadTasks);
        _HIZCullProgram->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
            PipelineDescriptor pipelineDescriptor = {};
            pipelineDescriptor._shaderProgramHandle = _HIZCullProgram->getGUID();
            _HIZCullPipeline = newPipeline(pipelineDescriptor);
        });
    }
    {
        ShaderModuleDescriptor vertModule = {};
        vertModule._moduleType = ShaderType::VERTEX;
        vertModule._sourceFile = "baseVertexShaders.glsl";
        vertModule._variant = "FullScreenQuad";

        ShaderModuleDescriptor fragModule = {};
        fragModule._moduleType = ShaderType::FRAGMENT;
        fragModule._sourceFile = "fbPreview.glsl";

        ShaderProgramDescriptor shaderDescriptor = {};
        shaderDescriptor._modules.push_back(vertModule);
        shaderDescriptor._modules.push_back(fragModule);

        ResourceDescriptor previewRTShader("fbPreview");
        previewRTShader.waitForReady(true);
        previewRTShader.propertyDescriptor(shaderDescriptor);
        _renderTargetDraw = CreateResource<ShaderProgram>(cache, previewRTShader, loadTasks);
        _renderTargetDraw->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) noexcept {
            _previewRenderTargetColour = _renderTargetDraw;
        });
    }
    {
        ShaderModuleDescriptor vertModule = {};
        vertModule._moduleType = ShaderType::VERTEX;
        vertModule._sourceFile = "baseVertexShaders.glsl";
        vertModule._variant = "FullScreenQuad";

        ShaderModuleDescriptor fragModule = {};
        fragModule._moduleType = ShaderType::FRAGMENT;
        fragModule._sourceFile = "fbPreview.glsl";
        fragModule._variant = "LinearDepth.ScenePlanes";

        ShaderProgramDescriptor shaderDescriptor = {};
        shaderDescriptor._modules.push_back(vertModule);
        shaderDescriptor._modules.push_back(fragModule);

        ResourceDescriptor previewReflectionRefractionDepth("fbPreviewLinearDepthScenePlanes");
        previewReflectionRefractionDepth.waitForReady(false);
        previewReflectionRefractionDepth.propertyDescriptor(shaderDescriptor);
        _previewRenderTargetDepth = CreateResource<ShaderProgram>(cache, previewReflectionRefractionDepth, loadTasks);
    }
    {
        ShaderModuleDescriptor vertModule = {};
        vertModule._moduleType = ShaderType::VERTEX;
        vertModule._sourceFile = "baseVertexShaders.glsl";
        vertModule._variant = "FullScreenQuad";

        ShaderModuleDescriptor fragModule = {};
        fragModule._moduleType = ShaderType::FRAGMENT;
        fragModule._sourceFile = "blur.glsl";
        fragModule._variant = "Generic";

        ShaderProgramDescriptor shaderDescriptor = {};
        shaderDescriptor._modules.push_back(vertModule);
        shaderDescriptor._modules.push_back(fragModule);

        ResourceDescriptor blur("blurGeneric");
        blur.propertyDescriptor(shaderDescriptor);
        _blurShader = CreateResource<ShaderProgram>(cache, blur, loadTasks);
        _blurShader->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
            ShaderProgram* blurShader = static_cast<ShaderProgram*>(res);
            _horizBlur = blurShader->GetSubroutineIndex(ShaderType::FRAGMENT, "blurHorizontal");
            _vertBlur = blurShader->GetSubroutineIndex(ShaderType::FRAGMENT, "blurVertical");

            {
                PipelineDescriptor pipelineDescriptor;
                pipelineDescriptor._stateHash = get2DStateBlock();
                pipelineDescriptor._shaderProgramHandle = blurShader->getGUID();
                pipelineDescriptor._shaderFunctions[to_base(ShaderType::FRAGMENT)].push_back(_horizBlur);
                _BlurHPipeline = newPipeline(pipelineDescriptor);
                pipelineDescriptor._shaderFunctions[to_base(ShaderType::FRAGMENT)].front() = _vertBlur;
                _BlurVPipeline = newPipeline(pipelineDescriptor);
            }
        });
    }
    {
        ShaderModuleDescriptor vertModule = {};
        vertModule._moduleType = ShaderType::VERTEX;
        vertModule._sourceFile = "baseVertexShaders.glsl";
        vertModule._variant = "FullScreenQuad";

        ShaderModuleDescriptor fragModule = {};
        fragModule._moduleType = ShaderType::FRAGMENT;
        fragModule._sourceFile = "display.glsl";

        ResourceDescriptor descriptor3("display");
        ShaderProgramDescriptor shaderDescriptor = {};
        shaderDescriptor._modules.push_back(vertModule);
        shaderDescriptor._modules.push_back(fragModule);
        descriptor3.propertyDescriptor(shaderDescriptor);
        {
            _displayShader = CreateResource<ShaderProgram>(cache, descriptor3, loadTasks);
            _displayShader->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
                PipelineDescriptor pipelineDescriptor = {};
                pipelineDescriptor._stateHash = get2DStateBlock();
                pipelineDescriptor._shaderProgramHandle = _displayShader->getGUID();
                _DrawFSTexturePipeline = newPipeline(pipelineDescriptor);
            });
        }
        {
            shaderDescriptor._modules.back()._defines.emplace_back("DEPTH_ONLY", true);
            descriptor3.propertyDescriptor(shaderDescriptor);
            _depthShader = CreateResource<ShaderProgram>(cache, descriptor3, loadTasks);
            _depthShader->addStateCallback(ResourceState::RES_LOADED, [this](CachedResource* res) {
                PipelineDescriptor pipelineDescriptor = {};
                pipelineDescriptor._stateHash = get2DStateBlock();
                pipelineDescriptor._stateHash = _stateDepthOnlyRenderingHash;
                pipelineDescriptor._shaderProgramHandle = _depthShader->getGUID();
                _DrawFSDepthPipeline = newPipeline(pipelineDescriptor);
            });
        }
    }

    context().paramHandler().setParam<bool>(_ID("rendering.previewDebugViews"), false);
    {
        PipelineDescriptor pipelineDesc;
        pipelineDesc._stateHash = getDefaultStateBlock(true);
        pipelineDesc._shaderProgramHandle = ShaderProgram::defaultShader()->getGUID();
        _AxisGizmoPipeline = newPipeline(pipelineDesc);
    }
    _renderer = std::make_unique<Renderer>(context(), cache);

    WAIT_FOR_CONDITION(loadTasks.load() == 0);
    DisplayWindow* mainWindow = context().app().windowManager().mainWindow();

    SizeChangeParams params = {};
    params.width = _rtPool->screenTarget().getWidth();
    params.height = _rtPool->screenTarget().getHeight();
    params.isWindowResize = false;
    params.winGUID = mainWindow->getGUID();

    if (context().app().onSizeChange(params)) {
        NOP();
    }

    // Everything is ready from the rendering point of view
    return ErrorCode::NO_ERR;
}


/// Revert everything that was set up in initRenderingAPI()
void GFXDevice::closeRenderingAPI() {
    if (_api == nullptr) {
        //closeRenderingAPI called without init!
        return;
    }

    if (_axisGizmo) {
        destroyIMP(_axisGizmo);
    }
    _debugBoxes.reset();
    _debugFrustums.reset();
    _debugCones.reset();
    _debugSpheres.reset();
    _debugViews.clear();

    // Delete the renderer implementation
    Console::printfn(Locale::get(_ID("CLOSING_RENDERER")));
    _renderer.reset(nullptr);

    RenderStateBlock::clear();

    EnvironmentProbe::onShutdown(*this);
    GFX::destroyPools();
    MemoryManager::SAFE_DELETE(_rtPool);

    _previewDepthMapShader = nullptr;
    _previewRenderTargetColour = nullptr;
    _previewRenderTargetDepth = nullptr;
    _renderTargetDraw = nullptr;
    _HIZConstructProgram = nullptr;
    _HIZCullProgram = nullptr;
    _displayShader = nullptr;
    _depthShader = nullptr;
    _textRenderShader = nullptr;
    _blurShader = nullptr;

    // Close the shader manager
    MemoryManager::DELETE(_shaderComputeQueue);
    ShaderProgram::onShutdown();
    _gpuObjectArena.clear();
    assert(ShaderProgram::shaderProgramCount() == 0);
    // Close the rendering API
    _api->closeRenderingAPI();
    _api.reset();

    UniqueLock<Mutex> lock(_graphicsResourceMutex);
    if (!_graphicResources.empty()) {
        stringImpl list = " [ ";
        for (const std::tuple<GraphicsResource::Type, I64, U64>& res : _graphicResources) {
            list.append(TypeUtil::GraphicResourceTypeToName(std::get<0>(res)));
            list.append(" _ ");
            list.append(to_stringImpl(std::get<1>(res)));
            list.append(" _ ");
            list.append(to_stringImpl(std::get<2>(res)));
            list.append(" ");
        }
        list += " ]";
        Console::errorfn(Locale::get(_ID("ERROR_GFX_LEAKED_RESOURCES")), _graphicResources.size());
        Console::errorfn(list.c_str());
    }
    _graphicResources.clear();
}
#pragma endregion

#pragma region Main frame loop
/// After a swap buffer call, the CPU may be idle waiting for the GPU to draw to the screen, so we try to do some processing
void GFXDevice::idle() {
    OPTICK_EVENT();

    _api->idle();

    _shaderComputeQueue->idle();
    // Pass the idle call to the post processing system
    _renderer->idle();
    // And to the shader manager
    ShaderProgram::idle();
}

void GFXDevice::beginFrame(DisplayWindow& window, bool global) {
    OPTICK_EVENT();

    if (global && Config::ENABLE_GPU_VALIDATION) {
        if (_renderDocManager) {
            _renderDocManager->StartFrameCapture();
        }
    }

    if (global && _resolutionChangeQueued.second) {
        SizeChangeParams params;
        params.isWindowResize = false;
        params.isFullScreen = window.fullscreen();
        params.width = _resolutionChangeQueued.first.width;
        params.height = _resolutionChangeQueued.first.height;
        params.winGUID = context().mainWindow().getGUID();

        if (context().app().onSizeChange(params)) {
            NOP();
        }
        _resolutionChangeQueued.second = false;
    }

    _api->beginFrame(window, global);

    const vec2<U16>& drawableSize = window.getDrawableSize();
    setViewport(0, 0, drawableSize.width, drawableSize.height);
}

void GFXDevice::endFrame(DisplayWindow& window, bool global) {
    OPTICK_EVENT();

    if (global) {
        FRAME_COUNT++;
        FRAME_DRAW_CALLS_PREV = FRAME_DRAW_CALLS;
        FRAME_DRAW_CALLS = 0;
    }
    DIVIDE_ASSERT(_cameraSnapshots.empty(), "Not all camera snapshots have been cleared properly! Check command buffers for missmatched push/pop!");
    DIVIDE_ASSERT(_viewportStack.empty(), "Not all viewportshave been cleared properly! Check command buffers for missmatched push/pop!");
    // Activate the default render states
    _api->endFrame(window, global);

    if (global && Config::ENABLE_GPU_VALIDATION) {
        if (_renderDocManager) {
            _renderDocManager->EndFrameCapture();
        }
    }
}
#pragma endregion

#pragma region Utility functions
/// Generate a cube texture and store it in the provided RenderTarget
void GFXDevice::generateCubeMap(RenderTargetID cubeMap,
                                const U16 arrayOffset,
                                const vec3<F32>& pos,
                                const vec2<F32>& zPlanes,
                                RenderStagePass stagePass,
                                GFX::CommandBuffer& bufferInOut,
                                SceneGraphNode* sourceNode,
                                Camera* camera) {

    if (!camera) {
        camera = Camera::utilityCamera(Camera::UtilityCamera::CUBE);
    }

    // Only the first colour attachment or the depth attachment is used for now
    // and it must be a cube map texture
    RenderTarget& cubeMapTarget = _rtPool->renderTarget(cubeMap);
    const RTAttachment& colourAttachment = cubeMapTarget.getAttachment(RTAttachmentType::Colour, 0);
    const RTAttachment& depthAttachment = cubeMapTarget.getAttachment(RTAttachmentType::Depth, 0);
    // Colour attachment takes precedent over depth attachment
    const bool hasColour = colourAttachment.used();
    const bool hasDepth = depthAttachment.used();
    // Everyone's innocent until proven guilty
    bool isValidFB = true;
    if (hasColour) {
        // We only need the colour attachment
        isValidFB = (colourAttachment.texture()->descriptor().isCubeTexture());
    } else {
        // We don't have a colour attachment, so we require a cube map depth
        // attachment
        isValidFB = hasDepth && depthAttachment.texture()->descriptor().isCubeTexture();
    }
    // Make sure we have a proper render target to draw to
    if (!isValidFB) {
        // Future formats must be added later (e.g. cube map arrays)
        Console::errorfn(Locale::get(_ID("ERROR_GFX_DEVICE_INVALID_FB_CUBEMAP")));
        return;
    }
    // No dual-paraboloid rendering here. Just draw once for each face.
    static vec3<F32> TabUp[6] = {WORLD_Y_NEG_AXIS, WORLD_Y_NEG_AXIS,
                                 WORLD_Z_AXIS,     WORLD_Z_NEG_AXIS,
                                 WORLD_Y_NEG_AXIS, WORLD_Y_NEG_AXIS};
    // Get the center and up vectors for each cube face
    vec3<F32> TabCenter[6] = {vec3<F32>( 1.0f,  0.0f,  0.0f),
                              vec3<F32>(-1.0f,  0.0f,  0.0f),
                              vec3<F32>( 0.0f,  1.0f,  0.0f),
                              vec3<F32>( 0.0f, -1.0f,  0.0f),
                              vec3<F32>( 0.0f,  0.0f,  1.0f),
                              vec3<F32>( 0.0f,  0.0f, -1.0f)};

    // Set a 90 degree vertical FoV perspective projection
    camera->setProjection(1.0f, 90.0f, zPlanes);

    // Enable our render target
    GFX::BeginRenderPassCommand beginRenderPassCmd;
    beginRenderPassCmd._target = cubeMap;
    beginRenderPassCmd._name = "GENERATE_CUBE_MAP";
    GFX::EnqueueCommand(bufferInOut, beginRenderPassCmd);

    // For each of the environment's faces (TOP, DOWN, NORTH, SOUTH, EAST, WEST)

    RenderPassManager* passMgr = parent().renderPassManager();
    RenderPassManager::PassParams params;
    params._sourceNode = sourceNode;
    params._camera = camera;
    params._target = cubeMap;
    params._stagePass = stagePass;
    params._stagePass._indexA = to_U16(stagePass._passIndex);
    // We do our own binding
    params._bindTargets = false;
    params._passName = "CubeMap";

    GFX::BeginRenderSubPassCommand beginRenderSubPassCmd = {};
    GFX::EndRenderSubPassCommand endRenderSubPassCommand = {};
    RenderTarget::DrawLayerParams drawParams = {};
    drawParams._type = hasColour ? RTAttachmentType::Colour : RTAttachmentType::Depth;
    drawParams._index = 0;

    for (U8 i = 0; i < 6; ++i) {
        // Draw to the current cubemap face
        drawParams._layer = i + arrayOffset;
        beginRenderSubPassCmd._writeLayers.resize(1, drawParams);
        GFX::EnqueueCommand(bufferInOut, beginRenderSubPassCmd);

        // Point our camera to the correct face
        camera->lookAt(pos, TabCenter[i], TabUp[i]);
        // Pass our render function to the renderer
        params._stagePass._indexB = i;
        passMgr->doCustomPass(params, bufferInOut);
        GFX::EnqueueCommand(bufferInOut, endRenderSubPassCommand);
    }

    // Resolve our render target
    GFX::EndRenderPassCommand endRenderPassCmd;
    GFX::EnqueueCommand(bufferInOut, endRenderPassCmd);
}

void GFXDevice::generateDualParaboloidMap(RenderTargetID targetBuffer,
                                          const U16 arrayOffset,
                                          const vec3<F32>& pos,
                                          const vec2<F32>& zPlanes,
                                          RenderStagePass stagePass,
                                          GFX::CommandBuffer& bufferInOut,
                                          SceneGraphNode* sourceNode,
                                          Camera* camera)
{
    if (!camera) {
        camera = Camera::utilityCamera(Camera::UtilityCamera::DUAL_PARABOLOID);
    }

    RenderTarget& paraboloidTarget = _rtPool->renderTarget(targetBuffer);
    const RTAttachment& colourAttachment = paraboloidTarget.getAttachment(RTAttachmentType::Colour, 0);
    const RTAttachment& depthAttachment = paraboloidTarget.getAttachment(RTAttachmentType::Depth, 0);
    // Colour attachment takes precedent over depth attachment
    const bool hasColour = colourAttachment.used();
    const bool hasDepth = depthAttachment.used();

    bool isValidFB = true;
    if (hasColour) {
        // We only need the colour attachment
        isValidFB = colourAttachment.texture()->descriptor().isArrayTexture();
    } else {
        // We don't have a colour attachment, so we require a cube map depth attachment
        isValidFB = hasDepth && depthAttachment.texture()->descriptor().isArrayTexture();
    }
    // Make sure we have a proper render target to draw to
    if (!isValidFB) {
        // Future formats must be added later (e.g. cube map arrays)
        Console::errorfn(Locale::get(_ID("ERROR_GFX_DEVICE_INVALID_FB_DP")));
        return;
    }

    // Set a 90 degree vertical FoV perspective projection
    camera->setProjection(1.0f, 180.0f, zPlanes);

    RenderPassManager* passMgr = parent().renderPassManager();
    RenderPassManager::PassParams params;
    params._sourceNode = sourceNode;
    params._camera = camera;
    params._stagePass = stagePass;
    params._stagePass._indexA = to_U16(stagePass._passIndex);
    params._target = targetBuffer;
    params._bindTargets = false;
    params._passName = "DualParaboloid";

    // Enable our render target

    GFX::BeginRenderPassCommand beginRenderPassCmd;
    beginRenderPassCmd._target = targetBuffer;
    beginRenderPassCmd._name = "GENERATE_DUAL_PARABOLOID_MAP";
    GFX::EnqueueCommand(bufferInOut, beginRenderPassCmd);

    GFX::BeginRenderSubPassCommand beginRenderSubPassCmd = {};
    GFX::EndRenderSubPassCommand endRenderSubPassCommand = {};

    RenderTarget::DrawLayerParams drawParams = {};
    drawParams._type = hasColour ? RTAttachmentType::Colour : RTAttachmentType::Depth;
    drawParams._index = 0;

    for (U8 i = 0; i < 2; ++i) {
        drawParams._layer = i + arrayOffset;
        beginRenderSubPassCmd._writeLayers.resize(1, drawParams);
        GFX::EnqueueCommand(bufferInOut, beginRenderSubPassCmd);

        // Point our camera to the correct face
        camera->lookAt(pos, (i == 0 ? WORLD_Z_NEG_AXIS : WORLD_Z_AXIS));
        // And generated required matrices
        // Pass our render function to the renderer
        params._stagePass._indexB = i;
        passMgr->doCustomPass(params, bufferInOut);
        GFX::EnqueueCommand(bufferInOut, endRenderSubPassCommand);
    }
    GFX::EndRenderPassCommand endRenderPassCmd;
    GFX::EnqueueCommand(bufferInOut, endRenderPassCmd);
}

void GFXDevice::blurTarget(RenderTargetHandle& blurSource,
                           RenderTargetHandle& blurBuffer,
                           RenderTargetHandle& blurTarget,
                           RTAttachmentType att,
                           U8 index,
                           I32 kernelSize,
                           GFX::CommandBuffer& bufferInOut)
{

    GenericDrawCommand triangleCmd;
    triangleCmd._primitiveType = PrimitiveType::TRIANGLES;
    triangleCmd._drawCount = 1;

    // Blur horizontally
    GFX::BeginRenderPassCommand beginRenderPassCmd;
    beginRenderPassCmd._target = blurBuffer._targetID;
    beginRenderPassCmd._name = "BLUR_RENDER_TARGET_HORIZONTAL";
    GFX::EnqueueCommand(bufferInOut, beginRenderPassCmd);

    TextureData data = blurSource._rt->getAttachment(att, index).texture()->data();
    GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
    descriptorSetCmd._set._textureData.setTexture(data, TextureUsage::UNIT0);
    GFX::EnqueueCommand(bufferInOut, descriptorSetCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _BlurHPipeline });

    GFX::SendPushConstantsCommand pushConstantsCommand;
    pushConstantsCommand._constants.countHint(2);
    pushConstantsCommand._constants.set(_ID("kernelSize"), GFX::PushConstantType::INT, kernelSize);
    pushConstantsCommand._constants.set(_ID("size"), GFX::PushConstantType::VEC2, vec2<F32>(blurBuffer._rt->getResolution()));
    GFX::EnqueueCommand(bufferInOut, pushConstantsCommand);

    GFX::DrawCommand drawCmd = { triangleCmd };
    GFX::EnqueueCommand(bufferInOut, drawCmd);

    GFX::EndRenderPassCommand endRenderPassCmd;
    GFX::EnqueueCommand(bufferInOut, endRenderPassCmd);

    // Blur vertically
    beginRenderPassCmd._target = blurTarget._targetID;
    beginRenderPassCmd._name = "BLUR_RENDER_TARGET_VERTICAL";
    GFX::EnqueueCommand(bufferInOut, beginRenderPassCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _BlurVPipeline });

    pushConstantsCommand._constants.set(_ID("size"), GFX::PushConstantType::VEC2, vec2<F32>(blurTarget._rt->getResolution()));
    GFX::EnqueueCommand(bufferInOut, pushConstantsCommand);

    data = blurBuffer._rt->getAttachment(att, index).texture()->data();
    descriptorSetCmd._set._textureData.setTexture(data, TextureUsage::UNIT0);
    GFX::EnqueueCommand(bufferInOut, descriptorSetCmd);

    GFX::EnqueueCommand(bufferInOut, drawCmd);

    GFX::EnqueueCommand(bufferInOut, endRenderPassCmd);
}
#pragma endregion

#pragma region Resolution, viewport and window management
void GFXDevice::increaseResolution() {
    stepResolution(true);
}

void GFXDevice::decreaseResolution() {
    stepResolution(false);
}

void GFXDevice::stepResolution(bool increment) {
    auto compare = [](const vec2<U16>& a, const vec2<U16>& b) noexcept -> bool {
        return a.x > b.x || a.y > b.y;
    };

    WindowManager& winManager = _parent.platformContext().app().windowManager();

    const vectorEASTL<GPUState::GPUVideoMode>& displayModes = _state.getDisplayModes(winManager.mainWindow()->currentDisplayIndex());

    bool found = false;
    vec2<U16> foundRes;
    if (increment) {
        for (const GPUState::GPUVideoMode& mode : reverse(displayModes)) {
            const vec2<U16>& res = mode._resolution;
            if (compare(res, _renderingResolution)) {
                found = true;
                foundRes.set(res);
                break;
            }
        }
    } else {
        for (const GPUState::GPUVideoMode& mode : displayModes) {
            const vec2<U16>& res = mode._resolution;
            if (compare(_renderingResolution, res)) {
                found = true;
                foundRes.set(res);
                break;
            }
        }
    }
    
    if (found) {
        _resolutionChangeQueued.first.set(foundRes);
        _resolutionChangeQueued.second = true;
    }
}

void GFXDevice::toggleFullScreen() {
    WindowManager& winManager = _parent.platformContext().app().windowManager();

    switch (winManager.mainWindow()->type()) {
        case WindowType::WINDOW:
            winManager.mainWindow()->changeType(WindowType::FULLSCREEN_WINDOWED);
            break;
        case WindowType::FULLSCREEN_WINDOWED:
            winManager.mainWindow()->changeType(WindowType::FULLSCREEN);
            break;
        case WindowType::FULLSCREEN:
            winManager.mainWindow()->changeType(WindowType::WINDOW);
            break;
    };
}

void GFXDevice::setScreenMSAASampleCount(U8 sampleCount) {
    CLAMP(sampleCount, to_U8(0u), gpuState().maxMSAASampleCount());
    if (_context.config().rendering.MSAAsamples != sampleCount) {
        _context.config().rendering.MSAAsamples = sampleCount;
        _rtPool->updateSampleCount(RenderTargetUsage::SCREEN_MS, sampleCount);
        _rtPool->updateSampleCount(RenderTargetUsage::OIT_MS, sampleCount);
    }
}

void GFXDevice::setShadowMSAASampleCount(U8 sampleCount) {
    CLAMP(sampleCount, to_U8(0u), gpuState().maxMSAASampleCount());
    if (_context.config().rendering.shadowMapping.MSAAsamples != sampleCount) {
        _context.config().rendering.shadowMapping.MSAAsamples = sampleCount;
        ShadowMap::setMSAASampleCount(sampleCount);
    }
}

/// The main entry point for any resolution change request
bool GFXDevice::onSizeChange(const SizeChangeParams& params) {
    if (params.winGUID != context().app().windowManager().mainWindow()->getGUID()) {
        return false;
    }

    const U16 w = params.width;
    const U16 h = params.height;

    if (!params.isWindowResize) {
        // Update resolution only if it's different from the current one.
        // Avoid resolution change on minimize so we don't thrash render targets
        if (w < 1 || h < 1 || _renderingResolution == vec2<U16>(w, h)) {
            return false;
        }

        _renderingResolution.set(w, h);

        // Update render targets with the new resolution
        _rtPool->resizeTargets(RenderTargetUsage::SCREEN, w, h);
        _rtPool->resizeTargets(RenderTargetUsage::SCREEN_MS, w, h);
        _rtPool->resizeTargets(RenderTargetUsage::HI_Z, w, h);
        _rtPool->resizeTargets(RenderTargetUsage::OIT, w, h);
        _rtPool->resizeTargets(RenderTargetUsage::OIT_MS, w, h);
        if_constexpr(Config::Build::ENABLE_EDITOR) {
            _rtPool->resizeTargets(RenderTargetUsage::EDITOR, w, h);
        }

        // Update post-processing render targets and buffers
        _renderer->updateResolution(w, h);

        // Update the 2D camera so it matches our new rendering viewport
        if (Camera::utilityCamera(Camera::UtilityCamera::_2D)->setProjection(vec4<F32>(0, to_F32(w), 0, to_F32(h)), vec2<F32>(-1, 1))) {
            Camera::utilityCamera(Camera::UtilityCamera::_2D)->updateFrustum();
        }
        if (Camera::utilityCamera(Camera::UtilityCamera::_2D_FLIP_Y)->setProjection(vec4<F32>(0, to_F32(w), to_F32(h), 0), vec2<F32>(-1, 1))) {
            Camera::utilityCamera(Camera::UtilityCamera::_2D_FLIP_Y)->updateFrustum();
        }
    }

    return fitViewportInWindow(w, h);
}

bool GFXDevice::fitViewportInWindow(U16 w, U16 h) {
    const F32 currentAspectRatio = renderingAspectRatio();

    I32 left = 0, bottom = 0;
    I32 newWidth = w;
    I32 newHeight = h;

    const I32 tempWidth = to_I32(h * currentAspectRatio);
    const I32 tempHeight = to_I32(w / currentAspectRatio);

    const F32 newAspectRatio = to_F32(tempWidth) / tempHeight;

    if (newAspectRatio <= currentAspectRatio) {
        newWidth = tempWidth;
        left = to_I32((w - newWidth) * 0.5f);
    } else {
        newHeight = tempHeight;
        bottom = to_I32((h - newHeight) * 0.5f);
    }
    
    context().mainWindow().renderingViewport(Rect<I32>(left, bottom, newWidth, newHeight));

    if (!COMPARE(newAspectRatio, currentAspectRatio)) {
        context().mainWindow().clearFlags(true, false);
        return true;
    } else {
        // If the aspect ratios match, then we should auto-fit to the entire visible drawing space so 
        // no need to keep clearing the backbuffer. This is one of the most useless micro-optimizations possible
        // but is really easy to add -Ionut
        context().mainWindow().clearFlags(false, false);
    }
    return false;
}
#pragma endregion

#pragma region GPU State
void GFXDevice::uploadGPUBlock() {
    OPTICK_EVENT();

    if (_gpuBlock._needsUpload) {
        _gfxDataBuffer->writeData(&_gpuBlock._data);
        _gfxDataBuffer->bind(ShaderBufferLocation::GPU_BLOCK);
        _gfxDataBuffer->incQueue();
        _gpuBlock._needsUpload = false;
    }
}

/// set a new list of clipping planes. The old one is discarded
void GFXDevice::setClipPlanes(const FrustumClipPlanes& clipPlanes) {
    static_assert(std::is_same<std::remove_reference<decltype(*(_gpuBlock._data._clipPlanes))>::type, vec4<F32>>::value, "GFXDevice error: invalid clip plane type!");
    static_assert(sizeof(vec4<F32>) == sizeof(Plane<F32>), "GFXDevice error: clip plane size mismatch!");

    if (clipPlanes._planes != _clippingPlanes._planes)
    {
        _clippingPlanes = clipPlanes;

        memcpy(&_gpuBlock._data._clipPlanes[0],
               _clippingPlanes._planes.data(),
               sizeof(vec4<F32>) * to_base(ClipPlaneIndex::COUNT));

        _gpuBlock._needsUpload = true;
    }
}

void GFXDevice::renderFromCamera(const CameraSnapshot& cameraSnapshot) {
    OPTICK_EVENT();

    GFXShaderData::GPUData& data = _gpuBlock._data;

    bool needsUpdate = false, projectionDirty = false, viewDirty = false;
    if (cameraSnapshot._projectionMatrix != data._ProjectionMatrix) {
        data._ProjectionMatrix.set(cameraSnapshot._projectionMatrix);
        projectionDirty = true;
    }

    if (cameraSnapshot._viewMatrix != data._ViewMatrix) {
        data._ViewMatrix.set(cameraSnapshot._viewMatrix);
        viewDirty = true;
    }

    if (projectionDirty || viewDirty) {
        mat4<F32>::Multiply(data._ViewMatrix, data._ProjectionMatrix, data._ViewProjectionMatrix);
        needsUpdate = true;
    }

    const vec4<F32> cameraPos(cameraSnapshot._eye, cameraSnapshot._aspectRatio);
    if (cameraPos != data._cameraPosition) {
        data._cameraPosition.set(cameraPos);
        needsUpdate = true;
    }

    const vec4<F32> cameraProperties(cameraSnapshot._zPlanes, cameraSnapshot._FoV, data._renderProperties.w);
    if (data._renderProperties != cameraProperties) {
        data._renderProperties.set(cameraProperties);
        needsUpdate = true;
    }

    const vec4<F32> otherProperties(
        to_F32(materialDebugFlag()),
        to_F32(csmPreviewIndex()),
        0.0f,
        0.0f);

    if (data._otherProperties != otherProperties) {
        data._otherProperties.set(otherProperties);
        needsUpdate = true;
    }

    if (needsUpdate) {
        _gpuBlock._needsUpload = true;
        _activeCameraSnapshot = cameraSnapshot;
    }
}

/// Update the rendering viewport
bool GFXDevice::setViewport(const Rect<I32>& viewport) {
    OPTICK_EVENT();

    // Change the viewport on the Rendering API level
    if (_api->setViewport(viewport)) {
    // Update the buffer with the new value
        _gpuBlock._data._ViewPort.set(viewport.x, viewport.y, viewport.z, viewport.w);
        const U8 tileRes = Light::GetThreadGroupSize(context().config().rendering.lightThreadGroupSize);
        const U32 viewportWidth = to_U32(viewport.z);
        const U32 workGroupsX = (viewportWidth + (viewportWidth % tileRes)) / tileRes;
        _gpuBlock._data._renderProperties.w = to_F32(workGroupsX);
        _gpuBlock._needsUpload = true;
        _viewport.set(viewport);

        return true;
    }

    return false;
}

void GFXDevice::setPreviousViewProjection(const mat4<F32>& view, const mat4<F32>& projection) noexcept {
    mat4<F32>::Multiply(view, projection, _gpuBlock._data._PreviousViewProjectionMatrix);
    _gpuBlock._needsUpload = true;
}

#pragma endregion

#pragma region Command buffers, occlusion culling, etc
void GFXDevice::flushCommandBuffer(GFX::CommandBuffer& commandBuffer, bool batch) {
    OPTICK_EVENT();
    if_constexpr(Config::ENABLE_GPU_VALIDATION) {
        DIVIDE_ASSERT(Runtime::isMainThread(), "GFXDevice::flushCommandBuffer called from worker thread!");

        const I32 debugFrame = _context.config().debug.dumpCommandBuffersOnFrame;
        if (debugFrame >= 0 && to_U32(FRAME_COUNT) == to_U32(debugFrame)) {
            Console::errorfn(commandBuffer.toString().c_str());
        }
    }

    if (batch) {
        commandBuffer.batch();
    }

    if (!commandBuffer.validate()) {
        Console::errorfn(Locale::get(_ID("ERROR_GFX_INVALID_COMMAND_BUFFER")), commandBuffer.toString().c_str());
        DIVIDE_ASSERT(false, "GFXDevice::flushCommandBuffer error: Invalid command buffer. Check error log!");
        return;
    }

    const GFX::CommandBuffer::CommandOrderContainer& commands = commandBuffer();
    for (const GFX::CommandBuffer::CommandEntry& cmd : commands) {
        const GFX::CommandType cmdType = static_cast<GFX::CommandType>(cmd._typeIndex);
        switch (cmdType) {
            case GFX::CommandType::BLIT_RT: {
                GFX::BlitRenderTargetCommand* crtCmd = commandBuffer.get<GFX::BlitRenderTargetCommand>(cmd);
                RenderTarget& source = renderTargetPool().renderTarget(crtCmd->_source);
                RenderTarget& destination = renderTargetPool().renderTarget(crtCmd->_destination);

                RenderTarget::RTBlitParams params = {};
                params._inputFB = &source;
                params._blitDepth = crtCmd->_blitDepth;
                params._blitColours = crtCmd->_blitColours;

                destination.blitFrom(params);
            } break;
            case GFX::CommandType::CLEAR_RT: {
                const GFX::ClearRenderTargetCommand& crtCmd = *commandBuffer.get<GFX::ClearRenderTargetCommand>(cmd);
                RenderTarget& source = renderTargetPool().renderTarget(crtCmd._target);
                source.clear(crtCmd._descriptor);
            }break;
            case GFX::CommandType::RESET_RT: {
                const GFX::ResetRenderTargetCommand& crtCmd = *commandBuffer.get<GFX::ResetRenderTargetCommand>(cmd);
                RenderTarget& source = renderTargetPool().renderTarget(crtCmd._source);
                source.setDefaultState(crtCmd._descriptor);
            } break;
            case GFX::CommandType::RESET_AND_CLEAR_RT: {
                const GFX::ResetAndClearRenderTargetCommand& crtCmd = *commandBuffer.get<GFX::ResetAndClearRenderTargetCommand>(cmd);
                RenderTarget& source = renderTargetPool().renderTarget(crtCmd._source);
                source.setDefaultState(crtCmd._drawDescriptor);
                source.clear(crtCmd._clearDescriptor);
            } break;
            case GFX::CommandType::READ_BUFFER_DATA: {
                const GFX::ReadBufferDataCommand& crtCmd = *commandBuffer.get<GFX::ReadBufferDataCommand>(cmd);
                if (crtCmd._buffer != nullptr && crtCmd._target != nullptr) {
                    crtCmd._buffer->readData(crtCmd._offsetElementCount, crtCmd._elementCount, crtCmd._target);
                }
            } break;
            case GFX::CommandType::CLEAR_BUFFER_DATA: {
                const GFX::ClearBufferDataCommand& crtCmd = *commandBuffer.get<GFX::ClearBufferDataCommand>(cmd);
                if (crtCmd._buffer != nullptr) {
                    crtCmd._buffer->clearData(crtCmd._offsetElementCount, crtCmd._elementCount);
                }
            } break;
            case GFX::CommandType::SET_VIEWPORT:
                setViewport(commandBuffer.get<GFX::SetViewportCommand>(cmd)->_viewport);
                break;
            case GFX::CommandType::PUSH_VIEWPORT: {
                GFX::PushViewportCommand* crtCmd = commandBuffer.get<GFX::PushViewportCommand>(cmd);
                _viewportStack.push(_viewport);
                setViewport(crtCmd->_viewport);
            } break;
            case GFX::CommandType::POP_VIEWPORT: {
                setViewport(_viewportStack.top());
                _viewportStack.pop();
            } break;
            case GFX::CommandType::SET_CAMERA: {
                GFX::SetCameraCommand* crtCmd = commandBuffer.get<GFX::SetCameraCommand>(cmd);
                // Tell the Rendering API to draw from our desired PoV
                renderFromCamera(crtCmd->_cameraSnapshot);
            } break;
            case GFX::CommandType::PUSH_CAMERA: {
                GFX::PushCameraCommand* crtCmd = commandBuffer.get<GFX::PushCameraCommand>(cmd);
                _cameraSnapshots.push(_activeCameraSnapshot);
                renderFromCamera(crtCmd->_cameraSnapshot);
            } break;
            case GFX::CommandType::POP_CAMERA: {
                renderFromCamera(_cameraSnapshots.top());
                _cameraSnapshots.pop();
            } break;
            case GFX::CommandType::SET_MIP_LEVELS: {
                GFX::SetTextureMipLevelsCommand* crtCmd = commandBuffer.get<GFX::SetTextureMipLevelsCommand>(cmd);
                if (crtCmd->_texture != nullptr) {
                    crtCmd->_texture->setMipMapRange(crtCmd->_baseLevel, crtCmd->_maxLevel);
                }
            }break;
            case GFX::CommandType::SET_CLIP_PLANES:
                setClipPlanes(commandBuffer.get<GFX::SetClipPlanesCommand>(cmd)->_clippingPlanes);
                break;
            case GFX::CommandType::EXTERNAL:
                commandBuffer.get<GFX::ExternalCommand>(cmd)->_cbk();
                break;

            case GFX::CommandType::DRAW_TEXT:
            case GFX::CommandType::DRAW_IMGUI:
            case GFX::CommandType::DRAW_COMMANDS:
            case GFX::CommandType::DISPATCH_COMPUTE:
                uploadGPUBlock(); /*no break. fall-through*/

            default: break;
        }

        _api->flushCommand(cmd, commandBuffer);
    }

    _api->postFlushCommandBuffer(commandBuffer);
}

/// Transform our depth buffer to a HierarchicalZ buffer (for occlusion queries and screen space reflections)
/// Based on RasterGrid implementation: http://rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
/// Modified with nVidia sample code: https://github.com/nvpro-samples/gl_occlusion_culling
const Texture_ptr& GFXDevice::constructHIZ(RenderTargetID depthBuffer, RenderTargetID HiZTarget, GFX::CommandBuffer& cmdBufferInOut) {
    assert(depthBuffer != HiZTarget);

    // We use a special shader that downsamples the buffer
    // We will use a state block that disables colour writes as we will render only a depth image,
    // disables depth testing but allows depth writes
    RTDrawDescriptor colourOnlyTarget;
    colourOnlyTarget.setViewport(false);
    colourOnlyTarget.drawMask().disableAll();
    colourOnlyTarget.drawMask().setEnabled(RTAttachmentType::Depth, 0, true);

    // The depth buffer's resolution should be equal to the screen's resolution
    RenderTarget& renderTarget = _rtPool->renderTarget(HiZTarget);
    const U16 width = renderTarget.getWidth();
    const U16 height = renderTarget.getHeight();
    U16 level = 0;
    U16 dim = width > height ? width : height;

    // Store the current width and height of each mip
    const Rect<I32> previousViewport(_viewport);

    GFX::BeginDebugScopeCommand beginDebugScopeCmd = {};
    beginDebugScopeCmd._scopeID = to_I32(depthBuffer._index);
    beginDebugScopeCmd._scopeName = "Construct Hi-Z";
    GFX::EnqueueCommand(cmdBufferInOut, beginDebugScopeCmd);

    RTClearDescriptor clearTarget = {};
    clearTarget.clearDepth(true);
    clearTarget.clearColours(true);

    GFX::ClearRenderTargetCommand clearRenderTargetCmd = {};
    clearRenderTargetCmd._target = HiZTarget;
    clearRenderTargetCmd._descriptor = clearTarget;
    GFX::EnqueueCommand(cmdBufferInOut, clearRenderTargetCmd);

    { // Copy depth buffer to the colour target for compute shaders to use later on

        GFX::BeginRenderPassCommand beginRenderPassCmd;
        beginRenderPassCmd._target = HiZTarget;
        beginRenderPassCmd._descriptor = {};
        beginRenderPassCmd._name = "CONSTRUCT_HI_Z_DEPTH";
        GFX::EnqueueCommand(cmdBufferInOut, beginRenderPassCmd);

        RenderTarget& depthSource = _rtPool->renderTarget(depthBuffer);
        const Texture_ptr& depthTex = depthSource.getAttachment(RTAttachmentType::Depth, 0).texture();

        const Rect<I32> viewport(0, 0, renderTarget.getWidth(), renderTarget.getHeight());
        drawTextureInViewport(depthTex->data(), viewport, false, true, cmdBufferInOut);

        GFX::EnqueueCommand(cmdBufferInOut, GFX::EndRenderPassCommand{});
    }

    const Texture_ptr& hizDepthTex = renderTarget.getAttachment(RTAttachmentType::Depth, 0).texture();
    const TextureData hizData = hizDepthTex->data();
    if (!hizDepthTex->descriptor().autoMipMaps()) {
        GFX::BeginRenderPassCommand beginRenderPassCmd;
        beginRenderPassCmd._target = HiZTarget;
        beginRenderPassCmd._descriptor = colourOnlyTarget;
        beginRenderPassCmd._name = "CONSTRUCT_HI_Z";
        GFX::EnqueueCommand(cmdBufferInOut, beginRenderPassCmd);

        if (GetHiZMethod() == HiZMethod::COUNT) {
            GFX::ComputeMipMapsCommand mipMapCommand = {};
            mipMapCommand._texture = hizDepthTex.get();
            mipMapCommand._layerRange = { 0, hizDepthTex->getMipCount() };
            GFX::EnqueueCommand(cmdBufferInOut, mipMapCommand);
        } else {
            GFX::EnqueueCommand(cmdBufferInOut, GFX::BindPipelineCommand{ _HIZPipeline });

            GFX::SetViewportCommand viewportCommand = {};
            GFX::SendPushConstantsCommand pushConstantsCommand = {};
            GFX::EndRenderSubPassCommand endRenderSubPassCmd = {};
            GFX::SetTextureMipLevelsCommand mipCommand = {};

            GFX::BeginRenderSubPassCommand beginRenderSubPassCmd = {};
            beginRenderSubPassCmd._validateWriteLevel = Config::ENABLE_GPU_VALIDATION;

            GenericDrawCommand triangleCmd = {};
            triangleCmd._primitiveType = PrimitiveType::TRIANGLES;
            triangleCmd._drawCount = 1;

            mipCommand._texture = hizDepthTex.get();

            // for i > 0, use texture views?
            GFX::BindDescriptorSetsCommand descriptorSetCmd = {};
            descriptorSetCmd._set._textureData.setTexture(hizData, TextureUsage::DEPTH);
            GFX::EnqueueCommand(cmdBufferInOut, descriptorSetCmd);

            // We skip the first level as that's our full resolution image
            U16 twidth = width;
            U16 theight = height;
            bool wasEven = false;
            U16 owidth = twidth;
            U16 oheight = theight;
            while (dim) {
                if (level) {
                    twidth = twidth < 1 ? 1 : twidth;
                    theight = theight < 1 ? 1 : theight;

                    // Bind next mip level for rendering but first restrict fetches only to previous level
                    beginRenderSubPassCmd._mipWriteLevel = level;
                    GFX::EnqueueCommand(cmdBufferInOut, beginRenderSubPassCmd);

                    // Update the viewport with the new resolution
                    viewportCommand._viewport.set(0, 0, twidth, theight);
                    GFX::EnqueueCommand(cmdBufferInOut, viewportCommand);

                    if (GetHiZMethod() == HiZMethod::NVIDIA) {
                        pushConstantsCommand._constants.set(_ID("depthInfo"), GFX::PushConstantType::IVEC2, vec2<I32>(level - 1, wasEven ? 1 : 0));
                    } else {
                        mipCommand._baseLevel = level - 1;
                        mipCommand._maxLevel = level - 1;
                        GFX::EnqueueCommand(cmdBufferInOut, mipCommand);

                        if (GetHiZMethod() == HiZMethod::RASTER_GRID) {
                            pushConstantsCommand._constants.set(_ID("LastMipSize"), GFX::PushConstantType::IVEC2, vec2<I32>(owidth, oheight));
                        }
                    }

                    GFX::EnqueueCommand(cmdBufferInOut, pushConstantsCommand);

                    // Dummy draw command as the full screen quad is generated completely in the vertex shader
                    GFX::DrawCommand drawCmd = { triangleCmd };
                    GFX::EnqueueCommand(cmdBufferInOut, drawCmd);

                    GFX::EnqueueCommand(cmdBufferInOut, endRenderSubPassCmd);
                }

                // Calculate next viewport size
                wasEven = (twidth % 2 == 0) && (theight % 2 == 0);
                dim /= 2;
                owidth = twidth;
                oheight = theight;
                twidth /= 2;
                theight /= 2;
                level++;
            }

            // Restore mip level
            beginRenderSubPassCmd._mipWriteLevel = 0;
            GFX::EnqueueCommand(cmdBufferInOut, beginRenderSubPassCmd);

            if (GetHiZMethod() != HiZMethod::NVIDIA) {
                mipCommand._baseLevel = 0;
                mipCommand._maxLevel = hizDepthTex->getMipCount() - 1;
                GFX::EnqueueCommand(cmdBufferInOut, mipCommand);
            }

            GFX::EnqueueCommand(cmdBufferInOut, endRenderSubPassCmd);

            viewportCommand._viewport.set(previousViewport);
            GFX::EnqueueCommand(cmdBufferInOut, viewportCommand);
        }
        // Unbind the render target
        GFX::EnqueueCommand(cmdBufferInOut, GFX::EndRenderPassCommand{});
    }

    GFX::EnqueueCommand(cmdBufferInOut, GFX::EndDebugScopeCommand{});

    return hizDepthTex;
}

void GFXDevice::occlusionCull(const RenderPass::BufferData& bufferData,
                              const Texture_ptr& depthBuffer,
                              const Camera& camera,
                              GFX::CommandBuffer& bufferInOut) {

    OPTICK_EVENT();

    constexpr U32 GROUP_SIZE_AABB = 64;
    const U32 cmdCount = *bufferData._lastCommandCount;

    GFX::BeginDebugScopeCommand beginDebugScopeCmd = {};
    beginDebugScopeCmd._scopeID = 0;
    beginDebugScopeCmd._scopeName = "Occlusion Cull";
    GFX::EnqueueCommand(bufferInOut, beginDebugScopeCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _HIZCullPipeline });

    ShaderBufferBinding shaderBuffer = {};
    shaderBuffer._binding = ShaderBufferLocation::GPU_COMMANDS;
    shaderBuffer._buffer = bufferData._cmdBuffer;
    shaderBuffer._elementRange = { bufferData._elementOffset, cmdCount };

    GFX::BindDescriptorSetsCommand bindDescriptorSetsCmd = {};
    bindDescriptorSetsCmd._set.addShaderBuffer(shaderBuffer);

    if (bufferData._cullCounter != nullptr) {
        ShaderBufferBinding atomicCount = {};
        atomicCount._binding = ShaderBufferLocation::ATOMIC_COUNTER;
        atomicCount._buffer = bufferData._cullCounter;
        atomicCount._elementRange.set(0, 1);
        bindDescriptorSetsCmd._set.addShaderBuffer(atomicCount); // Atomic counter should be cleared by this point
    }

    bindDescriptorSetsCmd._set._textureData.setTexture(depthBuffer->data(), TextureUsage::UNIT0);
    GFX::EnqueueCommand(bufferInOut, bindDescriptorSetsCmd);

    const mat4<F32>& viewMatrix = camera.getViewMatrix();
    const mat4<F32>& projectionMatrix = camera.getProjectionMatrix();

    GFX::SendPushConstantsCommand HIZPushConstantsCMD = {};
    HIZPushConstantsCMD._constants.countHint(GetHiZMethod() == HiZMethod::ARM ? 5 : 6);
    HIZPushConstantsCMD._constants.set(_ID("countCulledItems"), GFX::PushConstantType::UINT, bufferData._cullCounter != nullptr ? 1u : 0u);
    HIZPushConstantsCMD._constants.set(_ID("dvd_numEntities"), GFX::PushConstantType::UINT, cmdCount);
    HIZPushConstantsCMD._constants.set(_ID("viewMatrix"), GFX::PushConstantType::MAT4, viewMatrix);
    HIZPushConstantsCMD._constants.set(_ID("projectionMatrix"), GFX::PushConstantType::MAT4, projectionMatrix);
    HIZPushConstantsCMD._constants.set(_ID("viewProjectionMatrix"), GFX::PushConstantType::MAT4, mat4<F32>::Multiply(viewMatrix, projectionMatrix));
    HIZPushConstantsCMD._constants.set(_ID("dvd_nearPlaneDistance"), GFX::PushConstantType::FLOAT, camera.getZPlanes().x);
    if (GetHiZMethod() != HiZMethod::ARM) {
        HIZPushConstantsCMD._constants.set(_ID("viewportDimensions"), GFX::PushConstantType::VEC2, vec2<F32>(depthBuffer->width(), depthBuffer->height()));
    }
    GFX::EnqueueCommand(bufferInOut, HIZPushConstantsCMD);

    GFX::DispatchComputeCommand computeCmd = {};
    computeCmd._computeGroupSize.set((cmdCount + GROUP_SIZE_AABB - 1) / GROUP_SIZE_AABB, 1, 1);
    GFX::EnqueueCommand(bufferInOut, computeCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
}

void GFXDevice::updateCullCount(const RenderPass::BufferData& bufferData, GFX::CommandBuffer& cmdBufferInOut) {
    if (_queueCullRead) {
        if (bufferData._cullCounter != nullptr) {
            GFX::ReadBufferDataCommand readAtomicCounter;
            readAtomicCounter._buffer = bufferData._cullCounter;
            readAtomicCounter._target = &LAST_CULL_COUNT;
            readAtomicCounter._offsetElementCount = 0;
            readAtomicCounter._elementCount = 1;
            GFX::EnqueueCommand(cmdBufferInOut, readAtomicCounter);
        }

        _queueCullRead = false;
    }
}
#pragma endregion

#pragma region Drawing functions
void GFXDevice::drawText(const TextElementBatch& batch, GFX::CommandBuffer& bufferInOut) const {
    drawText(GFX::DrawTextCommand{ batch }, bufferInOut);
}

void GFXDevice::drawText(const GFX::DrawTextCommand& cmd, GFX::CommandBuffer& bufferInOut) const {
    GFX::EnqueueCommand(bufferInOut, GFX::PushCameraCommand{ Camera::utilityCamera(Camera::UtilityCamera::_2D)->snapshot() });
        GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ _textRenderPipeline });
        GFX::EnqueueCommand(bufferInOut, GFX::SendPushConstantsCommand{ _textRenderConstants });
        GFX::EnqueueCommand(bufferInOut, cmd);
    GFX::EnqueueCommand(bufferInOut, GFX::PopCameraCommand{});
}

void GFXDevice::drawText(const TextElementBatch& batch) {
    GFX::ScopedCommandBuffer sBuffer(GFX::allocateScopedCommandBuffer());
    GFX::CommandBuffer& buffer = sBuffer();

    // Assume full game window viewport for text
    GFX::SetViewportCommand viewportCommand;
    const RenderTarget& screenRT = _rtPool->screenTarget();
    const U16 width = screenRT.getWidth();
    const U16 height = screenRT.getHeight();
    viewportCommand._viewport.set(0, 0, width, height);
    GFX::EnqueueCommand(buffer, viewportCommand);

    drawText(batch, buffer);
    flushCommandBuffer(sBuffer());
}

void GFXDevice::drawTextureInViewport(TextureData data, const Rect<I32>& viewport, bool convertToSrgb, bool drawToDepthOnly, GFX::CommandBuffer& bufferInOut) {
    GenericDrawCommand triangleCmd = {};
    triangleCmd._primitiveType = PrimitiveType::TRIANGLES;
    triangleCmd._drawCount = 1;

    GFX::BeginDebugScopeCommand beginDebugScopeCmd = {};
    beginDebugScopeCmd._scopeID = 123456332;
    beginDebugScopeCmd._scopeName = "Draw Texture In Viewport";
    GFX::EnqueueCommand(bufferInOut, beginDebugScopeCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::PushCameraCommand{ Camera::utilityCamera(Camera::UtilityCamera::_2D)->snapshot() });
    GFX::EnqueueCommand(bufferInOut, GFX::BindPipelineCommand{ (drawToDepthOnly ? _DrawFSDepthPipeline : _DrawFSTexturePipeline) });

    GFX::BindDescriptorSetsCommand bindDescriptorSetsCmd = {};
    bindDescriptorSetsCmd._set._textureData.setTexture(data, TextureUsage::UNIT0);
    GFX::EnqueueCommand(bufferInOut, bindDescriptorSetsCmd);

    GFX::EnqueueCommand(bufferInOut, GFX::PushViewportCommand{ viewport });

    if (!drawToDepthOnly) {

        GFX::SendPushConstantsCommand pushConstantsCommand = {};
        pushConstantsCommand._constants.set(_ID("convertToSRGB"), GFX::PushConstantType::UINT, convertToSrgb ? 1u : 0u);
        GFX::EnqueueCommand(bufferInOut, pushConstantsCommand);
    }

    GFX::EnqueueCommand(bufferInOut, GFX::DrawCommand{ triangleCmd });

    GFX::EnqueueCommand(bufferInOut, GFX::PopViewportCommand{});
    GFX::EnqueueCommand(bufferInOut, GFX::PopCameraCommand{});
    GFX::EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
}
#pragma endregion

#pragma region Debug utilities
void GFXDevice::renderDebugUI(const Rect<I32>& targetViewport, GFX::CommandBuffer& bufferInOut) {
    constexpr I32 padding = 5;

    // Early out if we didn't request the preview
    if_constexpr(Config::ENABLE_GPU_VALIDATION) {

        GFX::BeginDebugScopeCommand beginDebugScopeCmd = {};
        beginDebugScopeCmd._scopeID = 1234567;
        beginDebugScopeCmd._scopeName = "Render Debug Views";
        GFX::EnqueueCommand(bufferInOut, beginDebugScopeCmd);

        renderDebugViews(
            Rect<I32>(targetViewport.x + padding,
                targetViewport.y + padding,
                targetViewport.z - padding * 2,
                targetViewport.w - padding * 2),
            padding,
            bufferInOut);

        GFX::EnqueueCommand(bufferInOut, GFX::EndDebugScopeCommand{});
    }
}

namespace {
    static DebugView* HiZView = nullptr;
};

void GFXDevice::initDebugViews() {
    // Lazy-load preview shader
    if (!_previewDepthMapShader) {
        ShaderModuleDescriptor vertModule = {};
        vertModule._moduleType = ShaderType::VERTEX;
        vertModule._sourceFile = "baseVertexShaders.glsl";
        vertModule._variant = "FullScreenQuad";

        ShaderModuleDescriptor fragModule = {};
        fragModule._moduleType = ShaderType::FRAGMENT;
        fragModule._sourceFile = "fbPreview.glsl";
        fragModule._variant = "LinearDepth";

        ShaderProgramDescriptor shaderDescriptor = {};
        shaderDescriptor._modules.push_back(vertModule);
        shaderDescriptor._modules.push_back(fragModule);

        // The LinearDepth variant converts the depth values to linear values between the 2 scene z-planes
        ResourceDescriptor fbPreview("fbPreviewLinearDepth");
        fbPreview.propertyDescriptor(shaderDescriptor);
        _previewDepthMapShader = CreateResource<ShaderProgram>(parent().resourceCache(), fbPreview);
        assert(_previewDepthMapShader != nullptr);

        DebugView_ptr HiZ = std::make_shared<DebugView>();
        HiZ->_shader = _previewDepthMapShader;
        HiZ->_texture = renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::HI_Z)).getAttachment(RTAttachmentType::Depth, 0).texture();
        HiZ->_name = "Hierarchical-Z";
        HiZ->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, to_F32(HiZ->_texture->getMaxMipLevel() - 1));
        HiZ->_shaderData.set(_ID("zPlanes"), GFX::PushConstantType::VEC2, vec2<F32>(_context.config().runtime.zNear, _context.config().runtime.zFar));

        DebugView_ptr DepthPreview = std::make_shared<DebugView>();
        DepthPreview->_shader = _previewDepthMapShader;
        DepthPreview->_texture = renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::SCREEN)).getAttachment(RTAttachmentType::Depth, 0).texture();
        DepthPreview->_name = "Depth Buffer";
        DepthPreview->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, 0.0f);
        DepthPreview->_shaderData.set(_ID("zPlanes"), GFX::PushConstantType::VEC2, vec2<F32>(_context.config().runtime.zNear, _context.config().runtime.zFar));

        DebugView_ptr NormalPreview = std::make_shared<DebugView>();
        NormalPreview->_shader = _renderTargetDraw;
        NormalPreview->_texture = renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::SCREEN)).getAttachment(RTAttachmentType::Colour, to_U8(ScreenTargets::NORMALS_AND_VELOCITY)).texture();
        NormalPreview->_name = "Normals";
        NormalPreview->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, 0.0f);
        NormalPreview->_shaderData.set(_ID("unpack1Channel"), GFX::PushConstantType::UINT, 0u);
        NormalPreview->_shaderData.set(_ID("unpack2Channel"), GFX::PushConstantType::UINT, 1u);
        NormalPreview->_shaderData.set(_ID("startOnBlue"), GFX::PushConstantType::UINT, 0u);

        DebugView_ptr VelocityPreview = std::make_shared<DebugView>();
        VelocityPreview->_shader = _renderTargetDraw;
        VelocityPreview->_texture = renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::SCREEN)).getAttachment(RTAttachmentType::Colour, to_U8(ScreenTargets::NORMALS_AND_VELOCITY)).texture();
        VelocityPreview->_name = "Velocity Map";
        VelocityPreview->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, 0.0f);
        VelocityPreview->_shaderData.set(_ID("unpack1Channel"), GFX::PushConstantType::UINT, 0u);
        VelocityPreview->_shaderData.set(_ID("unpack2Channel"), GFX::PushConstantType::UINT, 0u);
        VelocityPreview->_shaderData.set(_ID("startOnBlue"), GFX::PushConstantType::UINT, 1u);

        DebugView_ptr SSAOPreview = std::make_shared<DebugView>();
        SSAOPreview->_shader = _renderTargetDraw;
        SSAOPreview->_texture = renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::SCREEN)).getAttachment(RTAttachmentType::Colour, to_U8(ScreenTargets::EXTRA)).texture();
        SSAOPreview->_name = "SSAO Map";
        SSAOPreview->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, 0.0f);
        SSAOPreview->_shaderData.set(_ID("unpack1Channel"), GFX::PushConstantType::UINT, 1u);
        SSAOPreview->_shaderData.set(_ID("unpack2Channel"), GFX::PushConstantType::UINT, 0u);
        SSAOPreview->_shaderData.set(_ID("startOnBlue"), GFX::PushConstantType::UINT, 1u);

        DebugView_ptr AlphaAccumulationHigh = std::make_shared<DebugView>();
        AlphaAccumulationHigh->_shader = _renderTargetDraw;
        AlphaAccumulationHigh->_texture = renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::OIT)).getAttachment(RTAttachmentType::Colour, to_U8(ScreenTargets::ALBEDO)).texture();
        AlphaAccumulationHigh->_name = "Alpha Accumulation High";
        AlphaAccumulationHigh->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, 0.0f);
        AlphaAccumulationHigh->_shaderData.set(_ID("unpack2Channel"), GFX::PushConstantType::UINT, 0u);
        AlphaAccumulationHigh->_shaderData.set(_ID("unpack1Channel"), GFX::PushConstantType::UINT, 0u);
        AlphaAccumulationHigh->_shaderData.set(_ID("startOnBlue"), GFX::PushConstantType::UINT, 0u);

        DebugView_ptr AlphaRevealageHigh = std::make_shared<DebugView>();
        AlphaRevealageHigh->_shader = _renderTargetDraw;
        AlphaRevealageHigh->_texture = renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::OIT)).getAttachment(RTAttachmentType::Colour, to_U8(ScreenTargets::REVEALAGE)).texture();
        AlphaRevealageHigh->_name = "Alpha Revealage High";
        AlphaRevealageHigh->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, 0.0f);
        AlphaRevealageHigh->_shaderData.set(_ID("unpack1Channel"), GFX::PushConstantType::UINT, 1u);
        AlphaRevealageHigh->_shaderData.set(_ID("unpack2Channel"), GFX::PushConstantType::UINT, 0u);
        AlphaRevealageHigh->_shaderData.set(_ID("startOnBlue"), GFX::PushConstantType::UINT, 0u);

        //DebugView_ptr AlphaAccumulationLow = std::make_shared<DebugView>();
        //AlphaAccumulationLow->_shader = _renderTargetDraw;
        //AlphaAccumulationLow->_texture = renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::OIT_QUARTER_RES)).getAttachment(RTAttachmentType::Colour, to_U8(ScreenTargets::ALBEDO)).texture();
        //AlphaAccumulationLow->_name = "Alpha Accumulation Low";
        //AlphaAccumulationLow->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, 0.0f);
        //AlphaAccumulationLow->_shaderData.set(_ID("unpack2Channel"), GFX::PushConstantType::UINT, 0u);
        //AlphaAccumulationLow->_shaderData.set(_ID("unpack1Channel"), GFX::PushConstantType::UINT, 0u);
        //AlphaAccumulationLow->_shaderData.set(_ID("startOnBlue"), GFX::PushConstantType::UINT, 0u);

        //DebugView_ptr AlphaRevealageLow = std::make_shared<DebugView>();
        //AlphaRevealageLow->_shader = _renderTargetDraw;
        //AlphaRevealageLow->_texture = renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::OIT_QUARTER_RES)).getAttachment(RTAttachmentType::Colour, to_U8(ScreenTargets::REVEALAGE)).texture();
        //AlphaRevealageLow->_name = "Alpha Revealage Low";
        //AlphaRevealageLow->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, 0.0f);
        //AlphaRevealageLow->_shaderData.set(_ID("unpack1Channel"), GFX::PushConstantType::UINT, 1u);
        //AlphaRevealageLow->_shaderData.set(_ID("unpack2Channel"), GFX::PushConstantType::UINT, 0u);
        //AlphaRevealageLow->_shaderData.set(_ID("startOnBlue"), GFX::PushConstantType::UINT, 0u);

        DebugView_ptr Luminance = std::make_shared<DebugView>();
        Luminance->_shader = _renderTargetDraw;
        Luminance->_texture = renderTargetPool().renderTarget(getRenderer().postFX().getFilterBatch()->luminanceRT()).getAttachment(RTAttachmentType::Colour, 0u).texture();
        Luminance->_name = "Luminance";
        Luminance->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, 0.0f);
        Luminance->_shaderData.set(_ID("unpack1Channel"), GFX::PushConstantType::UINT, 1u);
        Luminance->_shaderData.set(_ID("unpack2Channel"), GFX::PushConstantType::UINT, 0u);
        Luminance->_shaderData.set(_ID("startOnBlue"), GFX::PushConstantType::UINT, 0u);

        DebugView_ptr Edges = std::make_shared<DebugView>();
        Edges->_shader = _renderTargetDraw;
        Edges->_texture = renderTargetPool().renderTarget(getRenderer().postFX().getFilterBatch()->edgesRT()).getAttachment(RTAttachmentType::Colour, 0u).texture();
        Edges->_name = "Edges";
        Edges->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, 0.0f);
        Edges->_shaderData.set(_ID("unpack1Channel"), GFX::PushConstantType::UINT, 0u);
        Edges->_shaderData.set(_ID("unpack2Channel"), GFX::PushConstantType::UINT, 1u);
        Edges->_shaderData.set(_ID("startOnBlue"), GFX::PushConstantType::UINT, 0u);

        HiZView = addDebugView(HiZ);
        addDebugView(DepthPreview);
        addDebugView(NormalPreview);
        addDebugView(VelocityPreview);
        addDebugView(SSAOPreview);
        addDebugView(AlphaAccumulationHigh);
        addDebugView(AlphaRevealageHigh);
        //addDebugView(AlphaAccumulationLow);
        //addDebugView(AlphaRevealageLow);
        addDebugView(Luminance);
        addDebugView(Edges);
        WAIT_FOR_CONDITION(_previewDepthMapShader->getState() == ResourceState::RES_LOADED);
    }
}

void GFXDevice::renderDebugViews(Rect<I32> targetViewport, const I32 padding, GFX::CommandBuffer& bufferInOut) {
    static size_t labelStyleHash = TextLabelStyle(Font::DROID_SERIF_BOLD, UColour4(128), 96).getHash();

    initDebugViews();

    if (HiZView) {
        //HiZ preview
        I32 LoDLevel = 0;
        RenderTarget& HiZRT = renderTargetPool().renderTarget(RenderTargetID(RenderTargetUsage::HI_Z));
        LoDLevel = to_I32(std::ceil(Time::Game::ElapsedMilliseconds() / 750.0f)) % (HiZRT.getAttachment(RTAttachmentType::Depth, 0).texture()->getMaxMipLevel() - 1);
        HiZView->_shaderData.set(_ID("lodLevel"), GFX::PushConstantType::FLOAT, to_F32(LoDLevel));
    }

    constexpr I32 maxViewportColumnCount = 10;
    I32 viewCount = to_I32(_debugViews.size());
    for (auto view : _debugViews) {
        if (!view->_enabled) {
            --viewCount;
        }
    }

    if (viewCount == 0) {
        return;
    }

    const I32 columnCount = viewCount < 4 ? 4 : std::min(viewCount, maxViewportColumnCount);
    I32 rowCount = viewCount / maxViewportColumnCount;
    if (viewCount % maxViewportColumnCount > 0) {
        rowCount++;
    }

    const I32 screenWidth = targetViewport.z - targetViewport.x;
    const I32 screenHeight = targetViewport.w - targetViewport.y;
    const F32 aspectRatio = to_F32(screenWidth) / screenHeight;

    const I32 viewportWidth = (screenWidth / columnCount) - padding;
    const I32 viewportHeight = to_I32(viewportWidth / aspectRatio) - padding;
    Rect<I32> viewport(screenWidth - viewportWidth, targetViewport.y, viewportWidth, viewportHeight);

    PipelineDescriptor pipelineDesc = {};
    pipelineDesc._stateHash = _state2DRenderingHash;

    GenericDrawCommand triangleCmd = {};
    triangleCmd._primitiveType = PrimitiveType::TRIANGLES;
    triangleCmd._drawCount = 1;

    vectorEASTLFast <std::pair<stringImpl, Rect<I32>>> labelStack;

    GFX::SetViewportCommand setViewport = {};
    GFX::SendPushConstantsCommand pushConstants = {};
    GFX::BindPipelineCommand bindPipeline = {};
    GFX::DrawCommand drawCommand = { triangleCmd };

    const Rect<I32> previousViewport(_viewport);

    I16 idx = 0;
    for (I16 i = 0; i < to_I16(_debugViews.size()); ++i) {
        DebugView& view = *_debugViews[i];

        if (!view._enabled) {
            continue;
        }

        pipelineDesc._shaderProgramHandle = view._shader->getGUID();

        bindPipeline._pipeline = newPipeline(pipelineDesc);
        GFX::EnqueueCommand(bufferInOut, bindPipeline);

        pushConstants._constants = view._shaderData;
        GFX::EnqueueCommand(bufferInOut, pushConstants);

        setViewport._viewport.set(viewport);
        GFX::EnqueueCommand(bufferInOut, setViewport);

        GFX::BindDescriptorSetsCommand bindDescriptorSets = {};
        bindDescriptorSets._set._textureData.setTexture(view._texture->data(), view._textureBindSlot);
        GFX::EnqueueCommand(bufferInOut, bindDescriptorSets);

        GFX::EnqueueCommand(bufferInOut, drawCommand);

        if (!view._name.empty()) {
            labelStack.emplace_back(view._name, viewport);
        }

        if (idx > 0 &&  idx % (columnCount - 1) == 0) {
            viewport.y += viewportHeight + targetViewport.y;
            viewport.x += viewportWidth * columnCount + targetViewport.x * columnCount;
        }
             
        viewport.x -= viewportWidth + targetViewport.x;
        ++idx;
    }

    TextElement text(labelStyleHash, RelativePosition2D(RelativeValue(0.1f, 0.0f), RelativeValue(0.1f, 0.0f)));
    for (const std::pair<stringImpl, Rect<I32>>& entry : labelStack) {
        // Draw labels at the end to reduce number of state changes
        setViewport._viewport.set(entry.second);
        GFX::EnqueueCommand(bufferInOut, setViewport);

        text._position.d_y.d_offset = entry.second.sizeY - 10.0f;
        text.text(entry.first.c_str(), false);
        drawText(text, bufferInOut);
    }

    setViewport._viewport.set(previousViewport);
    GFX::EnqueueCommand(bufferInOut, setViewport);
}

DebugView* GFXDevice::addDebugView(const std::shared_ptr<DebugView>& view) {
    UniqueLock<Mutex> lock(_debugViewLock);

    _debugViews.push_back(view);
    if (_debugViews.back()->_sortIndex == -1) {
        _debugViews.back()->_sortIndex = to_I16(_debugViews.size());
    }
    eastl::sort(eastl::begin(_debugViews),
                eastl::end(_debugViews),
                [](const std::shared_ptr<DebugView>& a, const std::shared_ptr<DebugView>& b) noexcept -> bool {
                    return a->_sortIndex < b->_sortIndex;
                });

    return view.get();
}

bool GFXDevice::removeDebugView(DebugView* view) {
    if (view != nullptr) {
        auto it = eastl::find_if(eastl::begin(_debugViews),
                                 eastl::end(_debugViews),
                                 [view](const std::shared_ptr<DebugView>& entry) noexcept {
                                    return view->getGUID() == entry->getGUID();
                                 });
                   
        if (it != eastl::cend(_debugViews)) {
            _debugViews.erase(it);
            return true;
        }
    }

    return false;
}

void GFXDevice::toggleDebugView(I16 index, const bool state) {
    UniqueLock<Mutex> lock(_debugViewLock);
    for (auto& view : _debugViews) {
        if (view->_sortIndex == index) {
            view->_enabled = state;
            break;
        }
    }
}

void GFXDevice::getDebugViewNames(vectorEASTL<std::tuple<stringImpl, I16, bool>>& namesOut) {
    namesOut.resize(0);

    UniqueLock<Mutex> lock(_debugViewLock);
    for (auto& view : _debugViews) {
        namesOut.emplace_back(view->_name, view->_sortIndex, view->_enabled);
    }
}

void GFXDevice::debugDrawBox(const vec3<F32>& min, const vec3<F32>& max, const FColour3& colour) {
    _debugBoxes.add({ min, max, colour });
}

void GFXDevice::debugDrawBoxes(GFX::CommandBuffer& bufferInOut) {
    const U32 boxesCount = _debugBoxes._Id.load();
    for (U32 f = 0; f < boxesCount; ++f) {
        IMPrimitive*& boxPrimitive = _debugBoxes._debugPrimitives[f];
        if (boxPrimitive == nullptr) {
            boxPrimitive = newIMP();
            boxPrimitive->name(Util::StringFormat("DebugBox_%d", f));
            boxPrimitive->pipeline(*_AxisGizmoPipeline);
            boxPrimitive->skipPostFX(true);
        }

        const auto&[min, max, colour3] = _debugBoxes._debugData[f];
        const FColour4 colour = FColour4(colour3, 1.0f);
        boxPrimitive->fromBox(min, max, Util::ToByteColour(colour));
        bufferInOut.add(boxPrimitive->toCommandBuffer());
    }

    _debugBoxes._Id.store(0u);
}

void GFXDevice::debugDrawSphere(const vec3<F32>& center, F32 radius, const FColour3& colour) {
    _debugSpheres.add({ center, radius, colour });
}

void GFXDevice::debugDrawSpheres(GFX::CommandBuffer& bufferInOut) {
    const U32 spheresCount = _debugSpheres._Id.load();
    for (U32 f = 0; f < spheresCount; ++f) {
        IMPrimitive*& spherePrimitive = _debugSpheres._debugPrimitives[f];
        if (spherePrimitive == nullptr) {
            spherePrimitive = newIMP();
            spherePrimitive->name(Util::StringFormat("DebugSphere_%d", f));
            spherePrimitive->pipeline(*_AxisGizmoPipeline);
            spherePrimitive->skipPostFX(true);
        }

        const auto&[center, radius, colour3] = _debugSpheres._debugData[f];
        const FColour4 colour = FColour4(colour3, 1.0f);
        spherePrimitive->fromSphere(center, radius, Util::ToByteColour(colour));
        bufferInOut.add(spherePrimitive->toCommandBuffer());
    }

    _debugSpheres._Id.store(0u);
}

void GFXDevice::debugDrawCone(const vec3<F32>& root, const vec3<F32>& direction, F32 length, F32 radius, const FColour3& colour) {
    _debugCones.add({root, direction, length, radius, colour});
}

void GFXDevice::debugDrawCones(GFX::CommandBuffer& bufferInOut) {
    const U32 conesCount = _debugCones._Id.load();
    for (U32 f = 0; f < conesCount; ++f) {
        IMPrimitive*& conePrimitive = _debugCones._debugPrimitives[f];
        if (conePrimitive == nullptr) {
            conePrimitive = newIMP();
            conePrimitive->name(Util::StringFormat("DebugCone_%d", f));
            conePrimitive->pipeline(*_AxisGizmoPipeline);
            conePrimitive->skipPostFX(true);
        }

        const auto&[root, dir, length, radius, colour3] = _debugCones._debugData[f];
        const FColour4 colour = FColour4(colour3, 1.0f);
        conePrimitive->fromCone(root, dir, length, radius, Util::ToByteColour(colour));
        bufferInOut.add(conePrimitive->toCommandBuffer());
    }

    _debugCones._Id.store(0u);
}

void GFXDevice::debugDrawFrustum(const Frustum& frustum, const FColour3& colour) {
    _debugFrustums.add({ frustum, colour });
}

void GFXDevice::debugDrawFrustums(GFX::CommandBuffer& bufferInOut) {
    const U32 frustumCount = _debugFrustums._Id.load();

    Line temp = {};
    std::array<Line, to_base(Frustum::FrustPlane::COUNT) * 2> lines = {};
    std::array<vec3<F32>, to_base(Frustum::FrustPoints::COUNT)> corners = {};
    for (U32 f = 0; f < frustumCount; ++f) {
        IMPrimitive*& frustumPrimitive = _debugFrustums._debugPrimitives[f];
        if (frustumPrimitive == nullptr) {
            frustumPrimitive = newIMP();
            frustumPrimitive->name(Util::StringFormat("DebugFrustum_%d", f));
            frustumPrimitive->pipeline(*_AxisGizmoPipeline);
            frustumPrimitive->skipPostFX(true);
        }

        //_debugFrustums[f].first.getCornersViewSpace(_debugFrustums[f].second, corners);
        _debugFrustums._debugData[f].first.getCornersWorldSpace(corners);
        const FColour3& endColour = _debugFrustums._debugData[f].second;
        const FColour3 startColour = endColour * 0.25f;

        UColour4 startColourU = Util::ToUIntColour(startColour);
        UColour4 endColourU = Util::ToUIntColour(endColour);

        U8 lineCount = 0;
        // Draw Near Plane
        temp.positionStart(corners[to_base(Frustum::FrustPoints::NEAR_LEFT_BOTTOM)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::NEAR_RIGHT_BOTTOM)]);
        temp.colourStart(startColourU);
        temp.colourEnd(startColourU);
        lines[lineCount++] = temp;

        temp.positionStart(corners[to_base(Frustum::FrustPoints::NEAR_RIGHT_BOTTOM)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::NEAR_RIGHT_TOP)]);
        temp.colourStart(startColourU);
        temp.colourEnd(startColourU);
        lines[lineCount++] = temp;

        temp.positionStart(corners[to_base(Frustum::FrustPoints::NEAR_RIGHT_TOP)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::NEAR_LEFT_TOP)]);
        temp.colourStart(startColourU);
        temp.colourEnd(startColourU);
        lines[lineCount++] = temp;

        temp.positionStart(corners[to_base(Frustum::FrustPoints::NEAR_LEFT_TOP)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::NEAR_LEFT_BOTTOM)]);
        temp.colourStart(startColourU);
        temp.colourEnd(startColourU);
        lines[lineCount++] = temp;

        // Draw Far Plane
        temp.positionStart(corners[to_base(Frustum::FrustPoints::FAR_LEFT_BOTTOM)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::FAR_RIGHT_BOTTOM)]);
        temp.colourStart(endColourU);
        temp.colourEnd(endColourU);
        lines[lineCount++] = temp;

        temp.positionStart(corners[to_base(Frustum::FrustPoints::FAR_RIGHT_BOTTOM)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::FAR_RIGHT_TOP)]);
        temp.colourStart(endColourU);
        temp.colourEnd(endColourU);
        lines[lineCount++] = temp;

        temp.positionStart(corners[to_base(Frustum::FrustPoints::FAR_RIGHT_TOP)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::FAR_LEFT_TOP)]);
        temp.colourStart(endColourU);
        temp.colourEnd(endColourU);
        lines[lineCount++] = temp;

        temp.positionStart(corners[to_base(Frustum::FrustPoints::FAR_LEFT_TOP)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::FAR_LEFT_BOTTOM)]);
        temp.colourStart(endColourU);
        temp.colourEnd(endColourU);
        lines[lineCount++] = temp;
        
        // Connect Planes
        temp.positionStart(corners[to_base(Frustum::FrustPoints::FAR_RIGHT_BOTTOM)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::NEAR_RIGHT_BOTTOM)]);
        temp.colourStart(endColourU);
        temp.colourEnd(startColourU);
        lines[lineCount++] = temp;

        temp.positionStart(corners[to_base(Frustum::FrustPoints::FAR_RIGHT_TOP)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::NEAR_RIGHT_TOP)]);
        temp.colourStart(endColourU);
        temp.colourEnd(startColourU);
        lines[lineCount++] = temp;

        temp.positionStart(corners[to_base(Frustum::FrustPoints::FAR_LEFT_TOP)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::NEAR_LEFT_TOP)]);
        temp.colourStart(endColourU);
        temp.colourEnd(startColourU);
        lines[lineCount++] = temp;

        temp.positionStart(corners[to_base(Frustum::FrustPoints::FAR_LEFT_BOTTOM)]);
        temp.positionEnd(corners[to_base(Frustum::FrustPoints::NEAR_LEFT_BOTTOM)]);
        temp.colourStart(endColourU);
        temp.colourEnd(startColourU);
        lines[lineCount++] = temp;

        frustumPrimitive->fromLines(lines.data(), lineCount);
        bufferInOut.add(frustumPrimitive->toCommandBuffer());
    }

    _debugFrustums._Id.store(0u);
}

/// Render all of our immediate mode primitives. This isn't very optimised and most are recreated per frame!
void GFXDevice::debugDraw(const SceneRenderState& sceneRenderState, const Camera& activeCamera, GFX::CommandBuffer& bufferInOut) {
    debugDrawFrustums(bufferInOut);
    debugDrawBoxes(bufferInOut);
    debugDrawSpheres(bufferInOut);
    debugDrawCones(bufferInOut);

    if_constexpr(Config::Build::ENABLE_EDITOR)
    {
        // Debug axis form the axis arrow gizmo in the corner of the screen
        // This is toggleable, so check if it's actually requested
        if (sceneRenderState.isEnabledOption(SceneRenderState::RenderOptions::SCENE_GIZMO)) {
            if (!_axisGizmo) {
                _axisGizmo = newIMP();
                _axisGizmo->name("GFXDeviceAxisGizmo");
                _axisGizmo->pipeline(*_AxisGizmoPipeline);
                _axisGizmo->skipPostFX(true);
            }

            // Apply the inverse view matrix so that it cancels out in the shader
            // Submit the draw command, rendering it in a tiny viewport in the lower
            // right corner
            const U16 windowWidth = renderTargetPool().screenTarget().getWidth();
            _axisGizmo->viewport(Rect<I32>(windowWidth - 120, 8, 128, 128));
            _axisGizmo->fromLines(_axisLines.data(), _axisLines.size());
        
            // We need to transform the gizmo so that it always remains axis aligned
            // Create a world matrix using a look at function with the eye position
            // backed up from the camera's view direction
            _axisGizmo->worldMatrix(mat4<F32>(-activeCamera.getForwardDir() * 2,
                                               VECTOR3_ZERO,
                                               activeCamera.getUpDir()) * activeCamera.getWorldMatrix());
            bufferInOut.add(_axisGizmo->toCommandBuffer());
        } else if (_axisGizmo) {
            destroyIMP(_axisGizmo);
        }
    }
}
#pragma endregion

#pragma region GPU Object instantiation
Mutex& GFXDevice::objectArenaMutex() noexcept {
    return _gpuObjectArenaMutex;
}

GFXDevice::ObjectArena& GFXDevice::objectArena() noexcept {
    return _gpuObjectArena;
}

RenderTarget* GFXDevice::newRT(const RenderTargetDescriptor& descriptor) {
    RenderTarget* temp = nullptr;
    {
        UniqueLock<Mutex> w_lock(objectArenaMutex());

        switch (renderAPI()) {
            case RenderAPI::OpenGL:
            case RenderAPI::OpenGLES: {
                temp =  new (objectArena()) glFramebuffer(*this, descriptor);
            } break;
            case RenderAPI::Vulkan: {
                temp = new (objectArena()) vkRenderTarget(*this, descriptor);
            } break;
            case RenderAPI::None: {
                temp = new (objectArena()) noRenderTarget(*this, descriptor);
            } break;
            default: {
                DIVIDE_UNEXPECTED_CALL(Locale::get(_ID("ERROR_GFX_DEVICE_API")));
            } break;
        };

        if (temp != nullptr) {
            objectArena().DTOR(temp);
        }
    }

    bool valid = false;
    if (temp != nullptr) {
        valid = temp->create();
        assert(valid);
    }

    return valid ? temp : nullptr;
}

IMPrimitive* GFXDevice::newIMP() {
    switch (renderAPI()) {
        case RenderAPI::OpenGL:
        case RenderAPI::OpenGLES: {
            return GL_API::newIMP(_imprimitiveMutex , *this);
        } break;
        case RenderAPI::Vulkan: {
            return nullptr;
        } break;
        case RenderAPI::None: {
            return nullptr;
        } break;
        default: {
            DIVIDE_UNEXPECTED_CALL(Locale::get(_ID("ERROR_GFX_DEVICE_API")));
        } break;
    };

    return nullptr;
}

bool GFXDevice::destroyIMP(IMPrimitive*& primitive) {
    switch (renderAPI()) {
        case RenderAPI::OpenGL:
        case RenderAPI::OpenGLES: {
            return GL_API::destroyIMP(_imprimitiveMutex , primitive);
        } break;
        case RenderAPI::Vulkan: {
            return false;
        } break;
        case RenderAPI::None: {
            return false;
        } break;
        default: {
            DIVIDE_UNEXPECTED_CALL(Locale::get(_ID("ERROR_GFX_DEVICE_API")));
        } break;
    };

    return false;
}

VertexBuffer* GFXDevice::newVB() {

    UniqueLock<Mutex> w_lock(objectArenaMutex());

    VertexBuffer* temp = nullptr;
    switch (renderAPI()) {
        case RenderAPI::OpenGL:
        case RenderAPI::OpenGLES: {
            temp = new (objectArena()) glVertexArray(*this);
        } break;
        case RenderAPI::Vulkan: {
            temp = new (objectArena()) vkVertexBuffer(*this);
        } break;
        case RenderAPI::None: {
            temp = new (objectArena()) noVertexBuffer(*this);
        } break;
        default: {
            DIVIDE_UNEXPECTED_CALL(Locale::get(_ID("ERROR_GFX_DEVICE_API")));
        } break;
    };

    if (temp != nullptr) {
        objectArena().DTOR(temp);
    }

    return temp;
}

PixelBuffer* GFXDevice::newPB(PBType type, const char* name) {

    UniqueLock<Mutex> w_lock(objectArenaMutex());

    PixelBuffer* temp = nullptr;
    switch (renderAPI()) {
        case RenderAPI::OpenGL:
        case RenderAPI::OpenGLES: {
            temp = new (objectArena()) glPixelBuffer(*this, type, name);
        } break;
        case RenderAPI::Vulkan: {
            temp = new (objectArena()) vkPixelBuffer(*this, type, name);
        } break;
        case RenderAPI::None: {
            temp = new (objectArena()) noPixelBuffer(*this, type, name);
        } break;
        default: {
            DIVIDE_UNEXPECTED_CALL(Locale::get(_ID("ERROR_GFX_DEVICE_API")));
        } break;
    };

    if (temp != nullptr) {
        objectArena().DTOR(temp);
    }

    return temp;
}

GenericVertexData* GFXDevice::newGVD(const U32 ringBufferLength, const char* name) {

    UniqueLock<Mutex> w_lock(objectArenaMutex());

    GenericVertexData* temp = nullptr;
    switch (renderAPI()) {
        case RenderAPI::OpenGL:
        case RenderAPI::OpenGLES: {
            temp = new (objectArena()) glGenericVertexData(*this, ringBufferLength, name);
        } break;
        case RenderAPI::Vulkan: {
            temp = new (objectArena()) vkGenericVertexData(*this, ringBufferLength, name);
        } break;
        case RenderAPI::None: {
            temp = new (objectArena()) noGenericVertexData(*this, ringBufferLength, name);
        } break;
        default: {
            DIVIDE_UNEXPECTED_CALL(Locale::get(_ID("ERROR_GFX_DEVICE_API")));
        } break;
    };

    if (temp != nullptr) {
        objectArena().DTOR(temp);
    }

    return temp;
}

Texture* GFXDevice::newTexture(size_t descriptorHash,
                               const Str128& resourceName,
                               const stringImpl& assetNames,
                               const stringImpl& assetLocations,
                               bool isFlipped,
                               bool asyncLoad,
                               const TextureDescriptor& texDescriptor) {

    UniqueLock<Mutex> w_lock(objectArenaMutex());

    // Texture is a resource! Do not use object arena!
    Texture* temp = nullptr;
    switch (renderAPI()) {
        case RenderAPI::OpenGL:
        case RenderAPI::OpenGLES: {
            temp = new (objectArena()) glTexture(*this, descriptorHash, resourceName, assetNames, assetLocations, isFlipped, asyncLoad, texDescriptor);
        } break;
        case RenderAPI::Vulkan: {
            temp = new (objectArena()) vkTexture(*this, descriptorHash, resourceName, assetNames, assetLocations, isFlipped, asyncLoad, texDescriptor);
        } break;
        case RenderAPI::None: {
            temp = new (objectArena()) noTexture(*this, descriptorHash, resourceName, assetNames, assetLocations, isFlipped, asyncLoad, texDescriptor);
        } break;
        default: {
            DIVIDE_UNEXPECTED_CALL(Locale::get(_ID("ERROR_GFX_DEVICE_API")));
        } break;
    };

    if (temp != nullptr) {
        objectArena().DTOR(temp);
    }

    return temp;
}

Pipeline* GFXDevice::newPipeline(const PipelineDescriptor& descriptor) {
    // Pipeline with no shader is no pipeline at all
    DIVIDE_ASSERT(descriptor._shaderProgramHandle != 0, "Missing shader handle during pipeline creation!");

    const size_t hash = descriptor.getHash();

    UniqueLock<Mutex> lock(_pipelineCacheLock);
    const hashMap<size_t, Pipeline, NoHash<size_t>>::iterator it = _pipelineCache.find(hash);
    if (it == std::cend(_pipelineCache)) {
        return &hashAlg::insert(_pipelineCache, hash, Pipeline(descriptor)).first->second;
    }

    return &it->second;
}

ShaderProgram* GFXDevice::newShaderProgram(size_t descriptorHash,
                                           const Str128& resourceName,
                                           const Str128& assetName,
                                           const stringImpl& assetLocation,
                                           const ShaderProgramDescriptor& descriptor,
                                           bool asyncLoad) {

    UniqueLock<Mutex> w_lock(objectArenaMutex());

    ShaderProgram* temp = nullptr;
    switch (renderAPI()) {
        case RenderAPI::OpenGL:
        case RenderAPI::OpenGLES: {
            temp = new (objectArena()) glShaderProgram(*this, descriptorHash, resourceName, assetName, assetLocation, descriptor, asyncLoad);
        } break;
        case RenderAPI::Vulkan: {
            temp = new (objectArena()) vkShaderProgram(*this, descriptorHash, resourceName, assetName, assetLocation, descriptor, asyncLoad);
        } break;
        case RenderAPI::None: {
            temp = new (objectArena()) noShaderProgram(*this, descriptorHash, resourceName, assetName, assetLocation, descriptor, asyncLoad);
        } break;
        default: {
            DIVIDE_UNEXPECTED_CALL(Locale::get(_ID("ERROR_GFX_DEVICE_API")));
        } break;
    };

    if (temp != nullptr) {
        objectArena().DTOR(temp);
    }

    return temp;
}

ShaderBuffer* GFXDevice::newSB(const ShaderBufferDescriptor& descriptor) {

    UniqueLock<Mutex> w_lock(objectArenaMutex());

    ShaderBuffer* temp = nullptr;
    switch (renderAPI()) {
        case RenderAPI::OpenGL:
        case RenderAPI::OpenGLES: {
            temp = new (objectArena()) glUniformBuffer(*this, descriptor);
        } break;
        case RenderAPI::Vulkan: {
            temp = new (objectArena()) vkUniformBuffer(*this, descriptor);
        } break;
        case RenderAPI::None: {
            temp = new (objectArena()) noUniformBuffer(*this, descriptor);
        } break;
        default: {
            DIVIDE_UNEXPECTED_CALL(Locale::get(_ID("ERROR_GFX_DEVICE_API")));
        } break;
    };

    if (temp != nullptr) {
        objectArena().DTOR(temp);
    }

    return temp;
}
#pragma endregion

ShaderComputeQueue& GFXDevice::shaderComputeQueue() {
    assert(_shaderComputeQueue != nullptr);
    return *_shaderComputeQueue;
}

const ShaderComputeQueue& GFXDevice::shaderComputeQueue() const {
    assert(_shaderComputeQueue != nullptr);
    return *_shaderComputeQueue;
}

/// Extract the pixel data from the main render target's first colour attachment and save it as a TGA image
void GFXDevice::Screenshot(const stringImpl& filename) {
    // Get the screen's resolution
    const RenderTarget& screenRT = _rtPool->screenTarget();
    const U16 width = screenRT.getWidth();
    const U16 height = screenRT.getHeight();
    // Allocate sufficiently large buffers to hold the pixel data
    const U32 bufferSize = width * height * 4;
    vectorEASTL<U8> imageData(bufferSize, 0u);
    // Read the pixels from the main render target (RGBA16F)
    screenRT.readData(GFXImageFormat::RGBA, GFXDataFormat::UNSIGNED_BYTE, (bufferPtr)(imageData.data()));
    // Save to file
    ImageTools::SaveSeries(filename,
                           vec2<U16>(width, height),
                           32,
                           imageData.data());
}

/// returns the standard state block
size_t GFXDevice::getDefaultStateBlock(bool noDepth) const noexcept {
    return noDepth ? _defaultStateNoDepthHash : RenderStateBlock::defaultHash();
}

};
