﻿#include "stdafx.h"

#include "Headers/VKWrapper.h"

#include "Core/Headers/Application.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"

#include "Utility/Headers/Localization.h"

#include "Platform/Headers/SDLEventManager.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/Headers/GFXRTPool.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Platform/Video/Headers/RenderStateBlock.h"
#include "Platform/Video/Textures/Headers/SamplerDescriptor.h"

#include "Buffers/Headers/vkRenderTarget.h"
#include "Buffers/Headers/vkShaderBuffer.h"
#include "Buffers/Headers/vkGenericVertexData.h"

#include "Shaders/Headers/vkShaderProgram.h"

#include "Textures/Headers/vkTexture.h"
#include "Textures/Headers/vkSamplerObject.h"

#include <sdl/include/SDL_vulkan.h>

#define VMA_IMPLEMENTATION
#include "Headers/vkMemAllocatorInclude.h"

namespace
{
    inline VKAPI_ATTR VkBool32 VKAPI_CALL divide_debug_callback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                                 VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                                 void* )
    {

        const auto to_string_message_severity = []( VkDebugUtilsMessageSeverityFlagBitsEXT s ) -> const char*
        {
            switch ( s )
            {
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                    return "VERBOSE";
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                    return "ERROR";
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                    return "WARNING";
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                    return "INFO";
                default:
                    return "UNKNOWN";
            }
        };

        const auto to_string_message_type = []( VkDebugUtilsMessageTypeFlagsEXT s ) -> const char*
        {
            if ( s == 7 ) return "General | Validation | Performance";
            if ( s == 6 ) return "Validation | Performance";
            if ( s == 5 ) return "General | Performance";
            if ( s == 4 /*VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT*/ ) return "Performance";
            if ( s == 3 ) return "General | Validation";
            if ( s == 2 /*VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT*/ ) return "Validation";
            if ( s == 1 /*VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT*/ ) return "General";
            return "Unknown";
        };

        Divide::Console::errorfn( "[%s: %s]\n%s\n",
                                  to_string_message_severity( messageSeverity ),
                                  to_string_message_type( messageType ),
                                  pCallbackData->pMessage );
        if ( messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT )
        {
            Divide::DIVIDE_UNEXPECTED_CALL();
        }
        return VK_FALSE; // Applications must return false here
    }

}

namespace Divide
{
    namespace
    {
        [[nodiscard]] VkShaderStageFlags GetFlagsForStageVisibility( const BaseType<ShaderStageVisibility> mask ) noexcept
        {
            VkShaderStageFlags ret = 0u;

            if ( mask != to_base( ShaderStageVisibility::NONE ) )
            {
                if ( mask == to_base( ShaderStageVisibility::ALL ) )
                {
                    ret = VK_SHADER_STAGE_ALL;
                }
                else if ( mask == to_base( ShaderStageVisibility::ALL_DRAW ) )
                {
                    ret = VK_SHADER_STAGE_ALL_GRAPHICS;
                }
                else if ( mask == to_base( ShaderStageVisibility::COMPUTE ) )
                {
                    ret = VK_SHADER_STAGE_COMPUTE_BIT;
                }
                else
                {
                    if ( TestBit( mask, ShaderStageVisibility::VERTEX ) )
                    {
                        ret |= VK_SHADER_STAGE_VERTEX_BIT;
                    }
                    if ( TestBit( mask, ShaderStageVisibility::GEOMETRY ) )
                    {
                        ret |= VK_SHADER_STAGE_GEOMETRY_BIT;
                    }
                    if ( TestBit( mask, ShaderStageVisibility::TESS_CONTROL ) )
                    {
                        ret |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
                    }
                    if ( TestBit( mask, ShaderStageVisibility::TESS_EVAL ) )
                    {
                        ret |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                    }
                    if ( TestBit( mask, ShaderStageVisibility::FRAGMENT ) )
                    {
                        ret |= VK_SHADER_STAGE_FRAGMENT_BIT;
                    }
                    if ( TestBit( mask, ShaderStageVisibility::COMPUTE ) )
                    {
                        ret |= VK_SHADER_STAGE_COMPUTE_BIT;
                    }
                }
            }

            return ret;
        }
    }

    constexpr U32 VK_VENDOR_ID_AMD = 0x1002;
    constexpr U32 VK_VENDOR_ID_IMGTECH = 0x1010;
    constexpr U32 VK_VENDOR_ID_NVIDIA = 0x10DE;
    constexpr U32 VK_VENDOR_ID_ARM = 0x13B5;
    constexpr U32 VK_VENDOR_ID_QUALCOMM = 0x5143;
    constexpr U32 VK_VENDOR_ID_INTEL = 0x8086;

    bool VK_API::s_hasDebugMarkerSupport = false;
    VKDeletionQueue VK_API::s_transientDeleteQueue;
    VKDeletionQueue VK_API::s_deviceDeleteQueue;
    VKTransferQueue VK_API::s_transferQueue;
    VKStateTracker VK_API::s_stateTracker;
    SharedMutex VK_API::s_samplerMapLock;
    VK_API::SamplerObjectMap VK_API::s_samplerMap{};
    VK_API::DepthFormatInformation VK_API::s_depthFormatInformation;

    VkPipeline PipelineBuilder::build_pipeline( VkDevice device, VkRenderPass pass, const bool graphics )
    {
        if ( graphics )
        {
            return build_graphics_pipeline( device, pass );
        }

        return build_compute_pipeline( device, pass );
    }

    VkPipeline PipelineBuilder::build_compute_pipeline( VkDevice device, VkRenderPass pass )
    {
        VkComputePipelineCreateInfo pipelineInfo = vk::computePipelineCreateInfo( _pipelineLayout );
        pipelineInfo.stage = _shaderStages.front();
        pipelineInfo.layout = _pipelineLayout;

        //it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
        VkPipeline newPipeline;
        if ( vkCreateComputePipelines( device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline ) != VK_SUCCESS )
        {
            Console::errorfn( "Failed to create compute pipeline!" );
            return VK_NULL_HANDLE; // failed to create graphics pipeline
        }

        return newPipeline;
    }

    VkPipeline PipelineBuilder::build_graphics_pipeline( VkDevice device, VkRenderPass pass )
    {
        //make viewport state from our stored viewport and scissor.
        //at the moment we won't support multiple viewports or scissors
        VkPipelineViewportStateCreateInfo viewportState = vk::pipelineViewportStateCreateInfo( 1, 1 );
        viewportState.pViewports = &_viewport;
        viewportState.pScissors = &_scissor;

        const VkPipelineColorBlendStateCreateInfo colorBlending = vk::pipelineColorBlendStateCreateInfo( to_U32( _colorBlendAttachments.size() ), _colorBlendAttachments.data() );

        const VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
            VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
            VK_DYNAMIC_STATE_STENCIL_REFERENCE,
            VK_DYNAMIC_STATE_DEPTH_BIAS,

            //ToDo:
             /*VK_DYNAMIC_STATE_CULL_MODE,
             VK_DYNAMIC_STATE_FRONT_FACE,
             VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
             VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
             VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
             VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
             VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
             VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
             VK_DYNAMIC_STATE_STENCIL_OP,
             VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
             VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
             VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,*/
        };

        constexpr U32 stateCount = to_U32( std::size( dynamicStates ) );
        const VkPipelineDynamicStateCreateInfo dynamicState = vk::pipelineDynamicStateCreateInfo( dynamicStates, stateCount );

        //build the actual pipeline
        //we now use all of the info structs we have been writing into into this one to create the pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo = vk::pipelineCreateInfo( _pipelineLayout, pass );
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.stageCount = to_U32( _shaderStages.size() );
        pipelineInfo.pStages = _shaderStages.data();
        pipelineInfo.pVertexInputState = &_vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &_inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &_rasterizer;
        pipelineInfo.pMultisampleState = &_multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDepthStencilState = &_depthStencil;
        pipelineInfo.pTessellationState = &_tessellation;
        pipelineInfo.subpass = 0;
        if ( VK_API::GetStateTracker()._pipelineRenderingCreateInfo.sType == VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO )
        {
            pipelineInfo.renderPass = nullptr;
            pipelineInfo.pNext = &VK_API::GetStateTracker()._pipelineRenderingCreateInfo;
        }

        //it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
        VkPipeline newPipeline;
        if ( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline ) != VK_SUCCESS )
        {
            Console::errorfn( "Failed to create graphics pipeline!" );
            return VK_NULL_HANDLE; // failed to create graphics pipeline
        }

        return newPipeline;
    }

    void VKDeletionQueue::push( DELEGATE<void, VkDevice>&& function )
    {
        ScopedLock<Mutex> w_lock( _deletionLock );
        _deletionQueue.push_back( MOV( function ) );
    }

    void VKDeletionQueue::flush( VkDevice device )
    {
        ScopedLock<Mutex> w_lock( _deletionLock );
        // reverse iterate the deletion queue to delete all the buffers
        for ( auto it = _deletionQueue.rbegin(); it != _deletionQueue.rend(); it++ )
        {
            (*it)(device);
        }

        _deletionQueue.clear();
    }

    bool VKDeletionQueue::empty() const
    {
        ScopedLock<Mutex> w_lock( _deletionLock );
        return _deletionQueue.empty();
    }

    VKImmediateCmdContext::VKImmediateCmdContext( VKDevice& context )
        : _context( context )
    {
        const VkFenceCreateInfo fenceCreateInfo = vk::fenceCreateInfo();
        vkCreateFence( _context.getVKDevice(), &fenceCreateInfo, nullptr, &_submitFence );
        _commandPool = _context.createCommandPool( _context.getQueueIndex( vkb::QueueType::graphics ), 0 );

        const VkCommandBufferAllocateInfo cmdBufAllocateInfo = vk::commandBufferAllocateInfo( _commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 );
        VK_CHECK( vkAllocateCommandBuffers( _context.getVKDevice(), &cmdBufAllocateInfo, &_commandBuffer ) );
    }

    VKImmediateCmdContext::~VKImmediateCmdContext()
    {
        vkDestroyCommandPool( _context.getVKDevice(), _commandPool, nullptr );
        vkDestroyFence( _context.getVKDevice(), _submitFence, nullptr );
    }

    void VKImmediateCmdContext::flushCommandBuffer( std::function<void( VkCommandBuffer cmd )>&& function )
    {
        UniqueLock<Mutex> w_lock( _submitLock );

        // Begin the command buffer recording.
        // We will use this command buffer exactly once before resetting, so we tell Vulkan that
        const VkCommandBufferBeginInfo cmdBeginInfo = vk::commandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

        VkCommandBuffer cmd = _commandBuffer;
        VK_CHECK( vkBeginCommandBuffer( cmd, &cmdBeginInfo ) );

        // Execute the function
        function( cmd );

        VK_CHECK( vkEndCommandBuffer( cmd ) );

        VkSubmitInfo submitInfo = vk::submitInfo();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        _context.submitToQueueAndWait( vkb::QueueType::graphics, submitInfo, _submitFence );

        // Reset the command buffers inside the command pool
        vkResetCommandPool( _context.getVKDevice(), _commandPool, 0 );
    }

    void VK_API::RegisterCustomAPIDelete( DELEGATE<void, VkDevice>&& cbk, const bool isResourceTransient )
    {
        if ( isResourceTransient )
        {
            s_transientDeleteQueue.push( MOV( cbk ) );
        }
        else
        {
            s_deviceDeleteQueue.push( MOV( cbk ) );
        }
    }

    void VK_API::RegisterTransferRequest( const VKTransferQueue::TransferRequest& request )
    {
        if ( request._immediate )
        {
            GetStateTracker()._cmdContext->flushCommandBuffer( [&request]( VkCommandBuffer cmd )
                                                               {
                                                                   if ( request.srcBuffer != VK_NULL_HANDLE )
                                                                   {
                                                                       VkBufferCopy copy;
                                                                       copy.dstOffset = request.dstOffset;
                                                                       copy.srcOffset = request.srcOffset;
                                                                       copy.size = request.size;
                                                                       vkCmdCopyBuffer( cmd, request.srcBuffer, request.dstBuffer, 1, &copy );
                                                                   }

                                                                   VkBufferMemoryBarrier2 memoryBarrier = vk::bufferMemoryBarrier2();
                                                                   memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                                                                   memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
                                                                   memoryBarrier.dstStageMask = request.dstStageMask;
                                                                   memoryBarrier.dstAccessMask = request.dstAccessMask;
                                                                   memoryBarrier.offset = request.dstOffset;
                                                                   memoryBarrier.size = request.size;
                                                                   memoryBarrier.buffer = request.dstBuffer;

                                                                   VkDependencyInfo dependencyInfo = vk::dependencyInfo();
                                                                   dependencyInfo.bufferMemoryBarrierCount = 1;
                                                                   dependencyInfo.pBufferMemoryBarriers = &memoryBarrier;

                                                                   //ToDo: batch these up! -Ionut
                                                                   vkCmdPipelineBarrier2( cmd, &dependencyInfo );
                                                               } );
        }
        else
        {
            UniqueLock<Mutex> w_lock( s_transferQueue._lock );
            s_transferQueue._requests.push_back( request );
        }
    }

    VK_API::VK_API( GFXDevice& context ) noexcept
        : _context( context )
    {
    }

    VkCommandBuffer VK_API::getCurrentCommandBuffer() const
    {
        return _commandBuffers[_currentFrameIndex];
    }

    void VK_API::idle( [[maybe_unused]] const bool fast ) noexcept
    {
        vkShaderProgram::Idle( _context.context() );
    }

    void VKStateTracker::setDefaultState()
    {
        _dynamicState = {};
        _activeShaderProgram = nullptr;
        _activePipeline = VK_NULL_HANDLE;
        _activePipelineLayout = VK_NULL_HANDLE;
        _activeTopology = PrimitiveTopology::COUNT;
        _activeMSAASamples = 0u;
        _activeRenderTargetID = INVALID_RENDER_TARGET_ID;
        _pipelineRenderingCreateInfo = {};
        _pipelineRenderingCreateInfo.colorAttachmentCount = 1u;
        _lastSyncedFrameNumber = GFXDevice::FrameCount();
        _drawIndirectBuffer = VK_NULL_HANDLE;
    }

    bool VK_API::beginFrame( DisplayWindow& window, [[maybe_unused]] bool global ) noexcept
    {
        // Set dynamic state to default
        GetStateTracker().setDefaultState();

        const auto& windowDimensions = window.getDrawableSize();
        const VkExtent2D windowExtents{ windowDimensions.width, windowDimensions.height };

        if ( _windowExtents.width != windowExtents.width || _windowExtents.height != windowExtents.height )
        {
            recreateSwapChain( window );
        }

        const VkResult result = _swapChain->beginFrame();
        if ( result != VK_SUCCESS )
        {
            if ( result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR )
            {
                Console::errorfn( "Detected Vulkan error: %s\n", VKErrorString( result ).c_str() );
                DIVIDE_UNEXPECTED_CALL();
            }
            recreateSwapChain( window );
            _skipEndFrame = true;
            return false;
        }

        _descriptorAllocatorPools[to_base( DescriptorAllocatorUsage::PER_DRAW )]->Flip();
        GetStateTracker()._perDrawDescriptorAllocator = _descriptorAllocatorPools[to_base( DescriptorAllocatorUsage::PER_DRAW )]->GetAllocator();

        _defaultRenderPass.renderPass = _swapChain->getRenderPass();
        _defaultRenderPass.framebuffer = _swapChain->getCurrentFrameBuffer();
        _defaultRenderPass.renderArea.extent = windowExtents;
        _descriptorSets.fill( VK_NULL_HANDLE );

        //begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
        VkCommandBufferBeginInfo cmdBeginInfo = vk::commandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        VK_CHECK( vkBeginCommandBuffer( getCurrentCommandBuffer(), &cmdBeginInfo ) );

        _context.setViewport( { 0, 0, windowDimensions.width, windowDimensions.height } );
        setScissor( { 0, 0, windowDimensions.width, windowDimensions.height } );
        vkLockManager::CleanExpiredSyncObjects( VK_API::GetStateTracker()._lastSyncedFrameNumber );

        return true;
    }

    void VK_API::endFrame( DisplayWindow& window, [[maybe_unused]] bool global ) noexcept
    {
        if ( _skipEndFrame )
        {
            _skipEndFrame = false;
            return;
        }

        VkCommandBuffer cmd = getCurrentCommandBuffer();
        //finalize the command buffer (we can no longer add commands, but it can now be executed)
        VK_CHECK( vkEndCommandBuffer( cmd ) );
        const VkResult result = _swapChain->endFrame( vkb::QueueType::graphics, cmd );

        if ( result != VK_SUCCESS )
        {
            if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR )
            {
                recreateSwapChain( window );
            }
            else
            {
                Console::errorfn( "Detected Vulkan error: %s\n", VKErrorString( result ).c_str() );
                DIVIDE_UNEXPECTED_CALL();
            }
        }

        _currentFrameIndex = ++_currentFrameIndex % MAX_FRAMES_IN_FLIGHT;
    }

    ErrorCode VK_API::initRenderingAPI( [[maybe_unused]] I32 argc, [[maybe_unused]] char** argv, [[maybe_unused]] Configuration& config ) noexcept
    {
        _descriptorSets.fill( VK_NULL_HANDLE );

        s_transientDeleteQueue.flags( s_transientDeleteQueue.flags() | to_base( VKDeletionQueue::Flags::TREAT_AS_TRANSIENT ) );

        const DisplayWindow& window = *_context.context().app().windowManager().mainWindow();

        auto systemInfoRet = vkb::SystemInfo::get_system_info();
        if ( !systemInfoRet )
        {
            Console::errorfn( Locale::Get( _ID( "ERROR_VK_INIT" ) ), systemInfoRet.error().message().c_str() );
            return ErrorCode::VK_OLD_HARDWARE;
        }

        //make the Vulkan instance, with basic debug features
        vkb::InstanceBuilder builder{};
        builder.set_app_name( window.title() )
            .set_engine_name( Config::ENGINE_NAME )
            .set_engine_version( Config::ENGINE_VERSION_MAJOR, Config::ENGINE_VERSION_MINOR, Config::ENGINE_VERSION_PATCH )
            .request_validation_layers( Config::ENABLE_GPU_VALIDATION )
            .require_api_version( 1, Config::DESIRED_VULKAN_MINOR_VERSION, 0 )
            .set_debug_callback( divide_debug_callback )
            .set_debug_callback_user_data_pointer( this );

        vkb::SystemInfo& systemInfo = systemInfoRet.value();
        if ( Config::ENABLE_GPU_VALIDATION && systemInfo.is_extension_available( VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) )
        {
            builder.enable_extension( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
            s_hasDebugMarkerSupport = true;
        }
        else
        {
            s_hasDebugMarkerSupport = false;
        }

        if constexpr ( Config::ENABLE_GPU_VALIDATION )
        {
            if ( systemInfo.validation_layers_available )
            {
                builder.enable_validation_layers();
            }
        }

        auto instanceRet = builder.build();
        if ( !instanceRet )
        {
            Console::errorfn( Locale::Get( _ID( "ERROR_VK_INIT" ) ), instanceRet.error().message().c_str() );
            return ErrorCode::VK_OLD_HARDWARE;
        }

        _vkbInstance = instanceRet.value();

        //store the debug messenger
        _debugMessenger = _vkbInstance.debug_messenger;

        // get the surface of the window we opened with SDL
        SDL_Vulkan_CreateSurface( window.getRawWindow(), _vkbInstance.instance, &_surface );
        if ( _surface == nullptr )
        {
            return ErrorCode::VK_SURFACE_CREATE;
        }

        _device = eastl::make_unique<VKDevice>( *this, _vkbInstance, _surface );
        if ( _device->getVKDevice() == nullptr )
        {
            return ErrorCode::VK_DEVICE_CREATE_FAILED;
        }
        VKUtil::fillEnumTables( _device->getVKDevice() );

        VkFormatProperties2 properties{};
        properties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;

        vkGetPhysicalDeviceFormatProperties2( _device->getPhysicalDevice(), VK_FORMAT_D24_UNORM_S8_UINT, &properties );
        s_depthFormatInformation._d24s8Supported = properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        vkGetPhysicalDeviceFormatProperties2( _device->getPhysicalDevice(), VK_FORMAT_D32_SFLOAT_S8_UINT, &properties );
        s_depthFormatInformation._d32s8Supported = properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        DIVIDE_ASSERT( s_depthFormatInformation._d24s8Supported || s_depthFormatInformation._d32s8Supported );


        vkGetPhysicalDeviceFormatProperties2( _device->getPhysicalDevice(), VK_FORMAT_X8_D24_UNORM_PACK32, &properties );
        s_depthFormatInformation._d24x8Supported = properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        vkGetPhysicalDeviceFormatProperties2( _device->getPhysicalDevice(), VK_FORMAT_D32_SFLOAT, &properties );
        s_depthFormatInformation._d32FSupported = properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        DIVIDE_ASSERT( s_depthFormatInformation._d24x8Supported || s_depthFormatInformation._d32FSupported );

        VkPhysicalDeviceProperties deviceProperties{};
        vkGetPhysicalDeviceProperties( _device->getPhysicalDevice().physical_device, &deviceProperties );

        DeviceInformation deviceInformation{};
        deviceInformation._renderer = GPURenderer::UNKNOWN;
        switch ( deviceProperties.vendorID )
        {
            case VK_VENDOR_ID_NVIDIA:
                deviceInformation._vendor = GPUVendor::NVIDIA;
                deviceInformation._renderer = GPURenderer::GEFORCE;
                break;
            case VK_VENDOR_ID_INTEL:
                deviceInformation._vendor = GPUVendor::INTEL;
                deviceInformation._renderer = GPURenderer::INTEL;
                break;
            case VK_VENDOR_ID_AMD:
                deviceInformation._vendor = GPUVendor::AMD;
                deviceInformation._renderer = GPURenderer::RADEON;
                break;
            case VK_VENDOR_ID_ARM:
                deviceInformation._vendor = GPUVendor::ARM;
                deviceInformation._renderer = GPURenderer::MALI;
                break;
            case VK_VENDOR_ID_QUALCOMM:
                deviceInformation._vendor = GPUVendor::QUALCOMM;
                deviceInformation._renderer = GPURenderer::ADRENO;
                break;
            case VK_VENDOR_ID_IMGTECH:
                deviceInformation._vendor = GPUVendor::IMAGINATION_TECH;
                deviceInformation._renderer = GPURenderer::POWERVR;
                break;
            case VK_VENDOR_ID_MESA:
                deviceInformation._vendor = GPUVendor::MESA;
                deviceInformation._renderer = GPURenderer::SOFTWARE;
                break;
            default:
                deviceInformation._vendor = GPUVendor::OTHER;
                break;
        }

        Console::printfn( Locale::Get( _ID( "VK_VENDOR_STRING" ) ),
                          deviceProperties.deviceName,
                          deviceProperties.vendorID,
                          deviceProperties.deviceID,
                          deviceProperties.driverVersion,
                          deviceProperties.apiVersion );

        deviceInformation._versionInfo._major = 1u;
        deviceInformation._versionInfo._minor = to_U8( VK_API_VERSION_MINOR( deviceProperties.apiVersion ) );

        deviceInformation._maxTextureUnits = deviceProperties.limits.maxDescriptorSetSampledImages;
        deviceInformation._maxVertAttributeBindings = deviceProperties.limits.maxVertexInputBindings;
        deviceInformation._maxVertAttributes = deviceProperties.limits.maxVertexInputAttributes;
        deviceInformation._maxRTColourAttachments = deviceProperties.limits.maxColorAttachments;
        deviceInformation._shaderCompilerThreads = 0xFFFFFFFF;
        CLAMP( config.rendering.maxAnisotropicFilteringLevel,
               to_U8( 0 ),
               to_U8( deviceProperties.limits.maxSamplerAnisotropy ) );
        deviceInformation._maxAnisotropy = config.rendering.maxAnisotropicFilteringLevel;

        DIVIDE_ASSERT( PushConstantsStruct::Size() <= deviceProperties.limits.maxPushConstantsSize );

        const VkSampleCountFlags counts = deviceProperties.limits.framebufferColorSampleCounts & deviceProperties.limits.framebufferDepthSampleCounts;
        U8 maxMSAASamples = 0u;
        if ( counts & VK_SAMPLE_COUNT_2_BIT )
        {
            maxMSAASamples = 2u;
        }
        if ( counts & VK_SAMPLE_COUNT_4_BIT )
        {
            maxMSAASamples = 4u;
        }
        if ( counts & VK_SAMPLE_COUNT_8_BIT )
        {
            maxMSAASamples = 8u;
        }
        if ( counts & VK_SAMPLE_COUNT_16_BIT )
        {
            maxMSAASamples = 16u;
        }
        if ( counts & VK_SAMPLE_COUNT_32_BIT )
        {
            maxMSAASamples = 32u;
        }
        if ( counts & VK_SAMPLE_COUNT_64_BIT )
        {
            maxMSAASamples = 64u;
        }
        // If we do not support MSAA on a hardware level for whatever reason, override user set MSAA levels
        config.rendering.MSAASamples = std::min( config.rendering.MSAASamples, maxMSAASamples );
        config.rendering.shadowMapping.csm.MSAASamples = std::min( config.rendering.shadowMapping.csm.MSAASamples, maxMSAASamples );
        config.rendering.shadowMapping.spot.MSAASamples = std::min( config.rendering.shadowMapping.spot.MSAASamples, maxMSAASamples );
        _context.gpuState().maxMSAASampleCount( maxMSAASamples );
        // How many workgroups can we have per compute dispatch
        for ( U8 i = 0u; i < 3; ++i )
        {
            deviceInformation._maxWorgroupCount[i] = deviceProperties.limits.maxComputeWorkGroupCount[i];
            deviceInformation._maxWorgroupSize[i] = deviceProperties.limits.maxComputeWorkGroupSize[i];
        }
        deviceInformation._maxWorgroupInvocations = deviceProperties.limits.maxComputeWorkGroupInvocations;
        deviceInformation._maxComputeSharedMemoryBytes = deviceProperties.limits.maxComputeSharedMemorySize;
        Console::printfn( Locale::Get( _ID( "MAX_COMPUTE_WORK_GROUP_INFO" ) ),
                          deviceInformation._maxWorgroupCount[0], deviceInformation._maxWorgroupCount[1], deviceInformation._maxWorgroupCount[2],
                          deviceInformation._maxWorgroupSize[0], deviceInformation._maxWorgroupSize[1], deviceInformation._maxWorgroupSize[2],
                          deviceInformation._maxWorgroupInvocations );
        Console::printfn( Locale::Get( _ID( "MAX_COMPUTE_SHARED_MEMORY_SIZE" ) ), deviceInformation._maxComputeSharedMemoryBytes / 1024 );

        // Maximum number of varying components supported as outputs in the vertex shader
        deviceInformation._maxVertOutputComponents = deviceProperties.limits.maxVertexOutputComponents;
        Console::printfn( Locale::Get( _ID( "MAX_VERTEX_OUTPUT_COMPONENTS" ) ), deviceInformation._maxVertOutputComponents );

        deviceInformation._UBOffsetAlignmentBytes = deviceProperties.limits.minUniformBufferOffsetAlignment;
        deviceInformation._UBOMaxSizeBytes = deviceProperties.limits.maxUniformBufferRange;
        deviceInformation._SSBOffsetAlignmentBytes = deviceProperties.limits.minStorageBufferOffsetAlignment;
        deviceInformation._SSBOMaxSizeBytes = deviceProperties.limits.maxStorageBufferRange;
        deviceInformation._maxSSBOBufferBindings = deviceProperties.limits.maxPerStageDescriptorStorageBuffers;

        const bool UBOSizeOver1Mb = deviceInformation._UBOMaxSizeBytes / 1024 > 1024;
        Console::printfn( Locale::Get( _ID( "GL_VK_UBO_INFO" ) ),
                          deviceProperties.limits.maxDescriptorSetUniformBuffers,
                          (deviceInformation._UBOMaxSizeBytes / 1024) / (UBOSizeOver1Mb ? 1024 : 1),
                          UBOSizeOver1Mb ? "Mb" : "Kb",
                          deviceInformation._UBOffsetAlignmentBytes );
        Console::printfn( Locale::Get( _ID( "GL_VK_SSBO_INFO" ) ),
                          deviceInformation._maxSSBOBufferBindings,
                          deviceInformation._SSBOMaxSizeBytes / 1024 / 1024,
                          deviceProperties.limits.maxDescriptorSetStorageBuffers,
                          deviceInformation._SSBOffsetAlignmentBytes );

        deviceInformation._maxClipAndCullDistances = deviceProperties.limits.maxCombinedClipAndCullDistances;
        deviceInformation._maxClipDistances = deviceProperties.limits.maxClipDistances;
        deviceInformation._maxCullDistances = deviceProperties.limits.maxCullDistances;

        GFXDevice::OverrideDeviceInformation( deviceInformation );

        if ( _device->getQueueIndex( vkb::QueueType::graphics ) == INVALID_VK_QUEUE_INDEX )
        {
            return ErrorCode::VK_NO_GRAHPICS_QUEUE;
        }

        _swapChain = eastl::make_unique<VKSwapChain>( *this, *_device, window );
        _cmdContext = eastl::make_unique<VKImmediateCmdContext>( *_device );

        VK_API::s_stateTracker._device = _device.get();
        VK_API::s_stateTracker._swapChain = _swapChain.get();
        VK_API::s_stateTracker._cmdContext = _cmdContext.get();

        _descriptorAllocatorPools[to_base( DescriptorAllocatorUsage::PER_DRAW )].reset( vke::DescriptorAllocatorPool::Create( _device->getVKDevice(), MAX_FRAMES_IN_FLIGHT ) );
        _descriptorAllocatorPools[to_base( DescriptorAllocatorUsage::PER_FRAME )].reset( vke::DescriptorAllocatorPool::Create( _device->getVKDevice(), 1u ) );
        for ( auto& pool : _descriptorAllocatorPools )
        {
            pool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f );
            pool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f );
            pool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f );
            pool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f );
            pool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f );
            pool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f );
            pool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f );
            pool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f );
            pool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f );
            pool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f );
        }
        _descriptorLayoutCache = eastl::make_unique<DescriptorLayoutCache>( _device->getVKDevice() );
        GetStateTracker()._perFrameDescriptorAllocator = _descriptorAllocatorPools[to_base( DescriptorAllocatorUsage::PER_FRAME )]->GetAllocator();

        recreateSwapChain( window );

        DIVIDE_ASSERT( Config::MINIMUM_VULKAN_MINOR_VERSION > 2 );

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = _device->getPhysicalDevice();
        allocatorInfo.device = _device->getDevice();
        allocatorInfo.instance = _vkbInstance.instance;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

        vmaCreateAllocator( &allocatorInfo, &_allocator );
        GetStateTracker()._allocatorInstance._allocator = &_allocator;

        _commandBuffers.resize( MAX_FRAMES_IN_FLIGHT );

        //allocate the default command buffer that we will use for rendering
        const VkCommandBufferAllocateInfo cmdAllocInfo = vk::commandBufferAllocateInfo( _device->graphicsCommandPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, MAX_FRAMES_IN_FLIGHT );
        VK_CHECK( vkAllocateCommandBuffers( _device->getVKDevice(), &cmdAllocInfo, _commandBuffers.data() ) );

        s_stateTracker.setDefaultState();

        return ErrorCode::NO_ERR;
    }

    void VK_API::recreateSwapChain( const DisplayWindow& window )
    {
        while ( window.minimized() )
        {
            idle( false );
            SDLEventManager::pollEvents();
        }

        vkDeviceWaitIdle( _device->getVKDevice() );

        s_deviceDeleteQueue.flush( _device->getVKDevice() );

        const ErrorCode err = _swapChain->create( TestBit( window.flags(), WindowFlags::VSYNC ),
                                                  _context.context().config().runtime.adaptiveSync,
                                                  _surface );

        DIVIDE_ASSERT( err == ErrorCode::NO_ERR );

        const auto& windowDimensions = window.getDrawableSize();
        _windowExtents = VkExtent2D{ windowDimensions.width, windowDimensions.height };

        _defaultRenderPass = vk::renderPassBeginInfo();
        _defaultRenderPass.renderArea.offset.x = 0;
        _defaultRenderPass.renderArea.offset.y = 0;
        _defaultRenderPass.clearValueCount = 1;
    }

    void VK_API::closeRenderingAPI()
    {
        vkShaderProgram::DestroyStaticData();

        // Destroy sampler objects
        {
            for ( auto& sampler : s_samplerMap )
            {
                vkSamplerObject::Destruct( sampler.second );
            }
            s_samplerMap.clear();
        }

        vkLockManager::Clear();
        if ( _device != nullptr )
        {
            if ( _device->getVKDevice() != VK_NULL_HANDLE )
            {
                vkDeviceWaitIdle( _device->getVKDevice() );
                s_transientDeleteQueue.flush( _device->getVKDevice() );
                s_deviceDeleteQueue.flush( _device->getVKDevice() );
                GetStateTracker()._perDrawDescriptorAllocator = {};
                GetStateTracker()._perFrameDescriptorAllocator = {};
                _descriptorLayoutCache.reset();
                _descriptorSetLayouts.fill( VK_NULL_HANDLE );
                _descriptorSets.fill( VK_NULL_HANDLE );
                for ( auto& pool : _descriptorAllocatorPools )
                {
                    pool.reset();
                }
            }
            if ( _allocator != VK_NULL_HANDLE )
            {
                vmaDestroyAllocator( _allocator );
                _allocator = VK_NULL_HANDLE;
            }
            _commandBuffers.clear();
            _cmdContext.reset();
            _swapChain.reset();
            _device.reset();
        }

        s_stateTracker.setDefaultState();

        if ( _vkbInstance.instance != nullptr )
        {
            vkDestroySurfaceKHR( _vkbInstance.instance, _surface, nullptr );
        }
        vkb::destroy_instance( _vkbInstance );
        _vkbInstance = {};
    }

    bool VK_API::loadShaderModule( const char* filePath, VkShaderModule* outShaderModule )
    {
        //open the file. With cursor at the end
        std::ifstream file( filePath, std::ios::ate | std::ios::binary );

        if ( !file.is_open() )
        {
            return false;
        }

        //find what the size of the file is by looking up the location of the cursor
        //because the cursor is at the end, it gives the size directly in bytes
        size_t fileSize = (size_t)file.tellg();

        //spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
        std::vector<uint32_t> buffer( fileSize / sizeof( uint32_t ) );

        //put file cursor at beginning
        file.seekg( 0 );

        //load the entire file into the buffer
        file.read( (char*)buffer.data(), fileSize );

        //now that the file is loaded into the buffer, we can close it
        file.close();


        //create a new shader module, using the buffer we loaded
        //codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
        const VkShaderModuleCreateInfo createInfo = vk::shaderModuleCreateInfo( buffer.size() * sizeof( uint32_t ), buffer.data() );

        //check that the creation goes well.
        VkShaderModule shaderModule;
        if ( vkCreateShaderModule( _device->getVKDevice(), &createInfo, nullptr, &shaderModule ) != VK_SUCCESS )
        {
            return false;
        }
        *outShaderModule = shaderModule;
        return true;
    }

    void VK_API::drawText( const TextElementBatch& batch )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        BlendingSettings textBlend{};
        textBlend.blendSrc( BlendProperty::SRC_ALPHA );
        textBlend.blendDest( BlendProperty::INV_SRC_ALPHA );
        textBlend.blendOp( BlendOperation::ADD );
        textBlend.blendSrcAlpha( BlendProperty::ONE );
        textBlend.blendDestAlpha( BlendProperty::ZERO );
        textBlend.blendOpAlpha( BlendOperation::COUNT );
        textBlend.enabled( true );

        [[maybe_unused]] const I32 width = _context.renderingResolution().width;
        [[maybe_unused]] const I32 height = _context.renderingResolution().height;

        size_t drawCount = 0;
        size_t previousStyle = 0;

        for ( const TextElement& entry : batch.data() )
        {
            if ( previousStyle != entry.textLabelStyleHash() )
            {
                previousStyle = entry.textLabelStyleHash();
            }

            const TextElement::TextType& text = entry.text();
            const size_t lineCount = text.size();
            for ( size_t i = 0; i < lineCount; ++i )
            {
            }
            drawCount += lineCount;

            // Register each label rendered as a draw call
            _context.registerDrawCalls( to_U32( drawCount ) );
        }
    }

    bool VK_API::draw( const GenericDrawCommand& cmd, VkCommandBuffer& cmdBuffer ) const
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        if ( cmd._sourceBuffer._id == 0 )
        {
            U32 indexCount = 0u;
            switch ( VK_API::GetStateTracker()._activeTopology )
            {
                case PrimitiveTopology::COMPUTE:
                case PrimitiveTopology::COUNT: DIVIDE_UNEXPECTED_CALL();         break;
                case PrimitiveTopology::TRIANGLES: indexCount = cmd._drawCount * 3;  break;
                case PrimitiveTopology::POINTS: indexCount = cmd._drawCount * 1;  break;
                default: indexCount = cmd._cmd.indexCount; break;
            }

            vkCmdDraw( cmdBuffer, indexCount, cmd._cmd.primCount, cmd._cmd.firstIndex, 0u );

        }
        else
        {
            // Because this can only happen on the main thread, try and avoid costly lookups for hot-loop drawing
            static VertexDataInterface::Handle s_lastID = { U16_MAX, 0u };
            static VertexDataInterface* s_lastBuffer = nullptr;

            if ( s_lastID != cmd._sourceBuffer )
            {
                s_lastID = cmd._sourceBuffer;
                s_lastBuffer = VertexDataInterface::s_VDIPool.find( s_lastID );
            }

            DIVIDE_ASSERT( s_lastBuffer != nullptr );
            vkUserData userData{};
            userData._cmdBuffer = &cmdBuffer;

            s_lastBuffer->draw( cmd, &userData );
        }

        return true;
    }

    namespace
    {
        [[nodiscard]] bool IsEmpty( const ShaderProgram::BindingsPerSetArray& bindings ) noexcept
        {
            for ( const auto& binding : bindings )
            {
                if ( binding._type != DescriptorSetBindingType::COUNT && binding._visibility != 0u )
                {
                    return false;
                }
            }

            return true;
        }
    }

    bool VK_API::bindShaderResources( const DescriptorSetUsage usage, const DescriptorSet& bindings, const bool isDirty )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );
        const BaseType<DescriptorSetUsage> usageIdx = to_base( usage );

        vke::DescriptorAllocatorHandle& handle = (usage == DescriptorSetUsage::PER_DRAW)
            ? GetStateTracker()._perDrawDescriptorAllocator
            : GetStateTracker()._perFrameDescriptorAllocator;

        DescriptorBuilder builder = DescriptorBuilder::Begin( _descriptorLayoutCache.get(), &handle );

        static std::array<VkDescriptorBufferInfo, ShaderProgram::MAX_SLOTS_PER_DESCRIPTOR_SET> BufferInfoStructs;
        static std::array<VkDescriptorImageInfo, ShaderProgram::MAX_SLOTS_PER_DESCRIPTOR_SET>  ImageInfoStructs;
        static std::array<VkImageMemoryBarrier2, ShaderProgram::MAX_SLOTS_PER_DESCRIPTOR_SET> MemBarriers{};
        U8 memBarrierCount = 0u;
        U8 bufferInfoStructIndex = 0u;
        U8 imageInfoStructIndex = 0u;

        DIVIDE_ASSERT( GetStateTracker()._activeShaderProgram != nullptr );
        auto& drawDescriptor = GetStateTracker()._activeShaderProgram->perDrawDescriptorSetLayout();
        const bool targetDescriptorEmpty = IsEmpty( drawDescriptor );

        VkImageMemoryBarrier2 memBarrier{};

        for ( auto& srcBinding : bindings )
        {
            if ( usage == DescriptorSetUsage::PER_DRAW )
            {
                if ( targetDescriptorEmpty )
                {
                    break;
                }

                if ( drawDescriptor[srcBinding._slot]._type == DescriptorSetBindingType::COUNT )
                {
                    continue;
                }
            }
            else if ( !isDirty )
            {
                return true;
            }

            const DescriptorSetBindingType type = Type( srcBinding._data );
            const VkShaderStageFlags stageFlags = GetFlagsForStageVisibility( srcBinding._shaderStageVisibility );

            switch ( type )
            {
                case DescriptorSetBindingType::UNIFORM_BUFFER:
                case DescriptorSetBindingType::SHADER_STORAGE_BUFFER:
                {
                    if ( !Has<ShaderBufferEntry>( srcBinding._data ) ||
                         As<ShaderBufferEntry>( srcBinding._data )._buffer == nullptr ||
                         As<ShaderBufferEntry>( srcBinding._data )._range._length == 0u )
                    {
                        continue;
                    }
                    const ShaderBufferEntry& bufferEntry = As<ShaderBufferEntry>( srcBinding._data );
                    DIVIDE_ASSERT( bufferEntry._buffer != nullptr );
                    const ShaderBuffer::Usage bufferUsage = bufferEntry._buffer->getUsage();
                    VkBuffer buffer = static_cast<vkShaderBuffer*>(bufferEntry._buffer)->bufferImpl()->_buffer;

                    if ( usage == DescriptorSetUsage::PER_BATCH && srcBinding._slot == 0 )
                    {
                        // Draw indirect buffer!
                        DIVIDE_ASSERT( bufferUsage == ShaderBuffer::Usage::COMMAND_BUFFER );
                        GetStateTracker()._drawIndirectBuffer = buffer;
                    }
                    else
                    {
                        VkDescriptorBufferInfo& bufferInfo = BufferInfoStructs[bufferInfoStructIndex++];
                        bufferInfo.buffer = buffer;
                        bufferInfo.offset = bufferEntry._range._startOffset * bufferEntry._buffer->getPrimitiveSize();
                        if ( bufferEntry._bufferQueueReadIndex == -1 )
                        {
                            bufferInfo.offset += bufferEntry._bufferQueueReadIndex * bufferEntry._buffer->alignedBufferSize();
                        }
                        else
                        {
                            bufferInfo.offset += bufferEntry._buffer->getStartOffset( true );
                        }
                        bufferInfo.range = bufferEntry._range._length * bufferEntry._buffer->getPrimitiveSize();

                        builder.bindBuffer( srcBinding._slot, &bufferInfo, VKUtil::vkDescriptorType( type ), stageFlags );
                    }
                } break;
                case DescriptorSetBindingType::COMBINED_IMAGE_SAMPLER:
                {
                    if ( srcBinding._slot == INVALID_TEXTURE_BINDING )
                    {
                        continue;
                    }
                    const DescriptorCombinedImageSampler& imageSampler = As<DescriptorCombinedImageSampler>( srcBinding._data );
                    if ( imageSampler._image._srcTexture._ceguiTex == nullptr && imageSampler._image._srcTexture._internalTexture == nullptr )
                    {
                        DIVIDE_ASSERT( imageSampler._image._usage == ImageUsage::UNDEFINED );
                        //unbind request;
                    }
                    else
                    {
                        DIVIDE_ASSERT( imageSampler._image.targetType() != TextureType::COUNT );
                        if ( imageSampler._image._srcTexture._internalTexture == nullptr )
                        {
                            DIVIDE_ASSERT( imageSampler._image._srcTexture._ceguiTex != nullptr );
                            continue;
                        }
                        const VkSampler samplerHandle = GetSamplerHandle( imageSampler._samplerHash );

                        VkDescriptorImageInfo& imageInfo = ImageInfoStructs[imageInfoStructIndex++];

                        vkTexture* vkTex = static_cast<vkTexture*>(imageSampler._image._srcTexture._internalTexture);

                        vkTexture::CachedImageView::Descriptor descriptor{};
                        descriptor._usage = ImageUsage::SHADER_READ;
                        descriptor._format = vkTex->vkFormat();
                        descriptor._type = vkTex->descriptor().texType();

                        if ( imageSampler._image._srcTexture._internalTexture->descriptor().hasUsageFlagSet( ImageUsage::RT_DEPTH_STENCIL_ATTACHMENT ) )
                        {
                            descriptor._aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
                        }

                        const VkImageLayout targetLayout = IsDepthTexture( vkTex->descriptor().baseFormat() ) ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        imageInfo = vk::descriptorImageInfo( samplerHandle, vkTex->getImageView( descriptor ), targetLayout );
                        if ( GetStateTracker()._activeRenderTargetID == SCREEN_TARGET_ID )
                        {
                            if ( vkTex->transitionLayout( descriptor._usage, ImageUsage::COUNT, descriptor._subRange, memBarrier ) )
                            {
                                MemBarriers[memBarrierCount++] = memBarrier;
                            }
                        }
                        else
                        {
                            DIVIDE_ASSERT( vkTex->imageUsage( descriptor._subRange.getHash() ) == descriptor._usage );
                        }
                        builder.bindImage( srcBinding._slot, &imageInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stageFlags );
                    }
                } break;
                case DescriptorSetBindingType::IMAGE:
                {
                    if ( !Has<ImageView>( srcBinding._data ) )
                    {
                        continue;
                    }
                    const ImageView& image = As<ImageView>( srcBinding._data );
                    if ( image._srcTexture._internalTexture == nullptr && image._srcTexture._ceguiTex != nullptr )
                    {
                        continue;
                    }

                    DIVIDE_ASSERT( image._srcTexture._internalTexture != nullptr && image._subRange._mipLevels.count == 1u );

                    vkTexture* vkTex = static_cast<vkTexture*>(image._srcTexture._internalTexture);

                    vkTexture::CachedImageView::Descriptor descriptor{};
                    descriptor._usage = image._usage;
                    descriptor._format = vkTex->vkFormat();
                    descriptor._type = image.targetType();
                    descriptor._subRange = image._subRange;

                    const size_t crtSubRangeHash = descriptor._subRange.getHash();
                    const ImageUsage crtUsage = image._srcTexture._internalTexture->imageUsage( crtSubRangeHash );
                    DIVIDE_ASSERT( crtUsage != ImageUsage::RT_COLOUR_ATTACHMENT &&
                                   crtUsage != ImageUsage::RT_DEPTH_ATTACHMENT &&
                                   crtUsage != ImageUsage::RT_DEPTH_STENCIL_ATTACHMENT );

                    VkDescriptorImageInfo& imageInfo = ImageInfoStructs[imageInfoStructIndex++];
                    imageInfo = vk::descriptorImageInfo( VK_NULL_HANDLE, vkTex->getImageView( descriptor ), VK_IMAGE_LAYOUT_GENERAL );

                    if ( GetStateTracker()._activeRenderTargetID == SCREEN_TARGET_ID )
                    {
                        if ( vkTex->transitionLayout( image._usage, ImageUsage::COUNT, descriptor._subRange, memBarrier ) )
                        {
                            MemBarriers[memBarrierCount++] = memBarrier;
                        }
                    }
                    else
                    {
                        DIVIDE_ASSERT( vkTex->imageUsage( crtSubRangeHash ) == image._usage );
                    }
                    builder.bindImage( srcBinding._slot, &imageInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stageFlags );
                } break;
                case DescriptorSetBindingType::COUNT:
                {
                    DIVIDE_UNEXPECTED_CALL();
                } break;
            };
        }

        if ( usage == DescriptorSetUsage::PER_DRAW )
        {
            
            builder.buildSetAndLayout( _descriptorSets[usageIdx], _descriptorSetLayouts[usageIdx], _device->getVKDevice() );
        }
        else
        {
            builder.buildSetFromLayout( _descriptorSets[usageIdx], _descriptorSetLayouts[usageIdx], _device->getVKDevice() );
        }

        if ( memBarrierCount > 0u )
        {
            VkDependencyInfo dependencyInfo = vk::dependencyInfo();
            dependencyInfo.imageMemoryBarrierCount = memBarrierCount;
            dependencyInfo.pImageMemoryBarriers = MemBarriers.data();

            vkCmdPipelineBarrier2( getCurrentCommandBuffer(), &dependencyInfo );
        }
        GetStateTracker()._descriptorsUpdated = true;

        return true;
    }

    void VK_API::bindDynamicState( const RenderStateBlock& currentState, VkCommandBuffer& cmdBuffer ) const
    {
        auto& stateTrackerDS = GetStateTracker()._dynamicState;
        if ( stateTrackerDS.stencilMask() != currentState.stencilMask() )
        {
            vkCmdSetStencilCompareMask( cmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, currentState.stencilMask() );
            stateTrackerDS.stencilMask( currentState.stencilMask() );
        }
        if ( stateTrackerDS.stencilWriteMask() != currentState.stencilWriteMask() )
        {
            vkCmdSetStencilWriteMask( cmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, currentState.stencilWriteMask() );
            stateTrackerDS.stencilWriteMask( currentState.stencilWriteMask() );
        }
        if ( stateTrackerDS.stencilRef() != currentState.stencilRef() )
        {
            vkCmdSetStencilReference( cmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, currentState.stencilRef() );
            stateTrackerDS.stencilRef( currentState.stencilRef() );
        }
        if ( !COMPARE( stateTrackerDS.zUnits(), currentState.zUnits() ) || !COMPARE( stateTrackerDS.zBias(), currentState.zBias() ) )
        {
            vkCmdSetDepthBias( cmdBuffer, currentState.zUnits(), 0.f, currentState.zBias() );
            stateTrackerDS.zUnits( currentState.zUnits() );
            stateTrackerDS.zBias( currentState.zBias() );
        }
    }

    ShaderResult VK_API::bindPipeline( const Pipeline& pipeline, VkCommandBuffer& cmdBuffer )
    {
        const PipelineDescriptor& pipelineDescriptor = pipeline.descriptor();
        ShaderProgram* program = ShaderProgram::FindShaderProgram( pipelineDescriptor._shaderProgramHandle );

        GetStateTracker()._activePipeline = VK_NULL_HANDLE;
        GetStateTracker()._activeTopology = pipelineDescriptor._primitiveTopology;

        if ( program == nullptr )
        {
            Console::errorfn( Locale::Get( _ID( "ERROR_GLSL_INVALID_HANDLE" ) ), pipelineDescriptor._shaderProgramHandle );
            return ShaderResult::Failed;
        }

        size_t stateBlockHash = pipelineDescriptor._stateHash == 0u ? _context.getDefaultStateBlock( false ) : pipelineDescriptor._stateHash;
        if ( stateBlockHash == 0 )
        {
            stateBlockHash = RenderStateBlock::DefaultHash();
        }
        bool currentStateValid = false;
        const RenderStateBlock& currentState = RenderStateBlock::Get( stateBlockHash, currentStateValid );
        DIVIDE_ASSERT( currentStateValid, "VK_API error: Invalid state blocks detected on activation!" );

        GetStateTracker()._activeShaderProgram = static_cast<vkShaderProgram*>(program);
        const vkShaderProgram& vkProgram = *GetStateTracker()._activeShaderProgram;
        VkPushConstantRange push_constant;
        push_constant.offset = 0u;
        push_constant.size = to_U32( PushConstantsStruct::Size() );
        push_constant.stageFlags = vkProgram.stageMask();

        VkPipelineLayoutCreateInfo pipeline_layout_info = vk::pipelineLayoutCreateInfo( 0u );
        pipeline_layout_info.pPushConstantRanges = &push_constant;
        pipeline_layout_info.pushConstantRangeCount = 1;

        eastl::fixed_vector<VkDescriptorSetLayoutBinding, ShaderProgram::MAX_SLOTS_PER_DESCRIPTOR_SET, false> layoutBinding{};
        const ShaderProgram::BindingsPerSetArray& drawLayout = vkProgram.perDrawDescriptorSetLayout();
        for ( U8 slot = 0u; slot < ShaderProgram::MAX_SLOTS_PER_DESCRIPTOR_SET; ++slot )
        {
            if ( drawLayout[slot]._type == DescriptorSetBindingType::COUNT )
            {
                continue;
            }
            VkDescriptorSetLayoutBinding& newBinding = layoutBinding.emplace_back();
            newBinding.descriptorCount = 1u;
            newBinding.descriptorType = VKUtil::vkDescriptorType( drawLayout[slot]._type );
            newBinding.stageFlags = GetFlagsForStageVisibility( drawLayout[slot]._visibility );
            newBinding.binding = slot;
            newBinding.pImmutableSamplers = nullptr;
        }

        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = vk::descriptorSetLayoutCreateInfo( layoutBinding.data(), to_U32( layoutBinding.size() ) );
        _descriptorSetLayouts[to_base( DescriptorSetUsage::PER_DRAW )] = _descriptorLayoutCache->createDescriptorLayout( &layoutCreateInfo );

        pipeline_layout_info.pSetLayouts = _descriptorSetLayouts.data();
        pipeline_layout_info.setLayoutCount = to_U32( _descriptorSetLayouts.size() );

        VkPipelineLayout tempPipelineLayout{VK_NULL_HANDLE};
        VK_CHECK( vkCreatePipelineLayout( _device->getVKDevice(), &pipeline_layout_info, nullptr, &tempPipelineLayout ) );
        VK_API::RegisterCustomAPIDelete( [tempPipelineLayout](VkDevice device)
                                         {
                                             vkDestroyPipelineLayout( device, tempPipelineLayout, nullptr);
                                         }, false );

        GetStateTracker()._activePipelineLayout = tempPipelineLayout;

        //build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage

        const auto& shaderStages = vkProgram.shaderStages();

        PipelineBuilder pipelineBuilder;

        GetStateTracker()._isGraphicsPipeline = false;
        for ( const auto& stage : shaderStages )
        {
            pipelineBuilder._shaderStages.push_back( vk::pipelineShaderStageCreateInfo( stage->stageMask(), stage->handle() ) );
            GetStateTracker()._isGraphicsPipeline = GetStateTracker()._isGraphicsPipeline || stage->stageMask() != VK_SHADER_STAGE_COMPUTE_BIT;
        }

        //vertex input controls how to read vertices from vertex buffers. We aren't using it yet
        pipelineBuilder._vertexInputInfo = vk::pipelineVertexInputStateCreateInfo();
        //connect the pipeline builder vertex input info to the one we get from Vertex
        const VertexInputDescription vertexDescription = getVertexDescription( pipelineDescriptor._vertexFormat );
        pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
        pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = to_U32( vertexDescription.attributes.size() );
        pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
        pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = to_U32( vertexDescription.bindings.size() );

        //input assembly is the configuration for drawing triangle lists, strips, or individual points.
        //we are just going to draw triangle list
        pipelineBuilder._inputAssembly = vk::pipelineInputAssemblyStateCreateInfo( vkPrimitiveTypeTable[to_base( pipelineDescriptor._primitiveTopology )], 0u, pipelineDescriptor._primitiveRestartEnabled ? VK_TRUE : VK_FALSE );
        //configure the rasterizer to draw filled triangles
        pipelineBuilder._rasterizer = vk::pipelineRasterizationStateCreateInfo(
            vkFillModeTable[to_base( currentState.fillMode() )],
            vkCullModeTable[to_base( currentState.cullMode() )],
            currentState.frontFaceCCW()
            ? VK_FRONT_FACE_CLOCKWISE
            : VK_FRONT_FACE_COUNTER_CLOCKWISE );

        VkSampleCountFlagBits msaaSampleFlags = VK_SAMPLE_COUNT_1_BIT;
        const U8 msaaSamples = GetStateTracker()._activeMSAASamples;
        if ( msaaSamples > 0u )
        {
            assert( isPowerOfTwo( msaaSamples ) );
            msaaSampleFlags = static_cast<VkSampleCountFlagBits>(msaaSamples);
        }
        pipelineBuilder._multisampling = vk::pipelineMultisampleStateCreateInfo( msaaSampleFlags );
        pipelineBuilder._multisampling.minSampleShading = 1.f;
        pipelineBuilder._multisampling.alphaToCoverageEnable = GetStateTracker()._alphaToCoverage ? VK_TRUE : VK_FALSE;
        if ( msaaSamples > 0u )
        {
            pipelineBuilder._multisampling.sampleShadingEnable = VK_TRUE;
        }
        VkStencilOpState stencilOpState{};
        stencilOpState.failOp = vkStencilOpTable[to_base( currentState.stencilFailOp() )];
        stencilOpState.passOp = vkStencilOpTable[to_base( currentState.stencilPassOp() )];
        stencilOpState.depthFailOp = vkStencilOpTable[to_base( currentState.stencilZFailOp() )];
        stencilOpState.compareOp = vkCompareFuncTable[to_base( currentState.stencilFunc() )];

        pipelineBuilder._depthStencil = vk::pipelineDepthStencilStateCreateInfo( currentState.depthTestEnabled(), true, vkCompareFuncTable[to_base( currentState.zFunc() )] );
        pipelineBuilder._depthStencil.stencilTestEnable = currentState.stencilEnable();
        pipelineBuilder._depthStencil.front = stencilOpState;
        pipelineBuilder._depthStencil.back = stencilOpState;
        pipelineBuilder._rasterizer.depthBiasEnable = !IS_ZERO( currentState.zBias() );

        //a single blend attachment with no blending and writing to RGBA
        const P32 cWrite = currentState.colourWrite();
        VkPipelineColorBlendAttachmentState blend = vk::pipelineColorBlendAttachmentState(
            (cWrite.b[0] == 1 ? VK_COLOR_COMPONENT_R_BIT : 0) |
            (cWrite.b[1] == 1 ? VK_COLOR_COMPONENT_G_BIT : 0) |
            (cWrite.b[2] == 1 ? VK_COLOR_COMPONENT_B_BIT : 0) |
            (cWrite.b[3] == 1 ? VK_COLOR_COMPONENT_A_BIT : 0),
            VK_FALSE );

        for ( U8 i = 0u; i < to_base( RTColourAttachmentSlot::COUNT ); ++i )
        {
            const BlendingSettings& blendState = pipelineDescriptor._blendStates._settings[i];

            blend.blendEnable = blendState.enabled() ? VK_TRUE : VK_FALSE;
            if ( blendState.blendOpAlpha() != BlendOperation::COUNT )
            {
                blend.alphaBlendOp = vkBlendOpTable[to_base( blendState.blendOpAlpha() )];
                blend.dstAlphaBlendFactor = vkBlendTable[to_base( blendState.blendDestAlpha() )];
                blend.srcAlphaBlendFactor = vkBlendTable[to_base( blendState.blendSrcAlpha() )];
            }
            else
            {
                blend.alphaBlendOp = VK_BLEND_OP_ADD;
                blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            }
            blend.colorBlendOp = vkBlendOpTable[to_base( blendState.blendOp() )];
            blend.srcColorBlendFactor = vkBlendTable[to_base( blendState.blendSrc() )];
            blend.dstColorBlendFactor = vkBlendTable[to_base( blendState.blendDest() )];
            pipelineBuilder._colorBlendAttachments.emplace_back( blend );

            if ( GetStateTracker()._activeRenderTargetID == SCREEN_TARGET_ID )
            {
                break;
            }
        }

        //use the triangle layout we created
        pipelineBuilder._pipelineLayout = tempPipelineLayout;
        pipelineBuilder._tessellation = vk::pipelineTessellationStateCreateInfo( currentState.tessControlPoints() );

        VkPipeline vkPipeline = pipelineBuilder.build_pipeline( _device->getVKDevice(), _swapChain->getRenderPass(), GetStateTracker()._isGraphicsPipeline );
        VK_API::RegisterCustomAPIDelete( [vkPipeline, tempPipelineLayout]( VkDevice device )
                                         {
                                             vkDestroyPipeline( device, vkPipeline, nullptr );
                                         }, false );

        vkCmdBindPipeline( cmdBuffer, GetStateTracker()._isGraphicsPipeline ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline );
        GetStateTracker()._activePipeline = vkPipeline;

        bindDynamicState( currentState, cmdBuffer );

        return ShaderResult::OK;
    }

    void VK_API::flushCommand( GFX::CommandBase* cmd ) noexcept
    {
        static GFX::MemoryBarrierCommand pushConstantsMemCommand{};
        static bool pushConstantsNeedLock = false;

        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        VkCommandBuffer cmdBuffer = getCurrentCommandBuffer();
        const GFX::CommandType cmdType = cmd->Type();
        PROFILE_TAG( "Type", to_base( cmdType ) );

        PROFILE_SCOPE( GFX::Names::commandType[to_base( cmdType )], Profiler::Category::Graphics );
        if ( !s_transferQueue._requests.empty() && GFXDevice::IsSubmitCommand( cmdType ) )
        {
            UniqueLock<Mutex> w_lock( s_transferQueue._lock );
            // Check again
            if ( !s_transferQueue._requests.empty() )
            {
                static eastl::fixed_vector<VkBufferMemoryBarrier2, 32, true> s_barriers{};
                // ToDo: Use a semaphore here and insert a wait dependency on it into the main command buffer - Ionut
                VK_API::GetStateTracker()._cmdContext->flushCommandBuffer( []( VkCommandBuffer cmd )
                {
                    while ( !s_transferQueue._requests.empty() )
                    {
                        const VKTransferQueue::TransferRequest& request = s_transferQueue._requests.front();
                        if ( request.srcBuffer != VK_NULL_HANDLE )
                        {
                            VkBufferCopy copy;
                            copy.dstOffset = request.dstOffset;
                            copy.srcOffset = request.srcOffset;
                            copy.size = request.size;
                            vkCmdCopyBuffer( cmd, request.srcBuffer, request.dstBuffer, 1, &copy );
                        }

                        VkBufferMemoryBarrier2 memoryBarrier = vk::bufferMemoryBarrier2();
                        memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                        memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
                        memoryBarrier.dstStageMask = request.dstStageMask;
                        memoryBarrier.dstAccessMask = request.dstAccessMask;
                        memoryBarrier.offset = request.dstOffset;
                        memoryBarrier.size = request.size;
                        memoryBarrier.buffer = request.dstBuffer;

                        if ( request.srcBuffer == VK_NULL_HANDLE )
                        {
                            VkDependencyInfo dependencyInfo = vk::dependencyInfo();
                            dependencyInfo.bufferMemoryBarrierCount = 1;
                            dependencyInfo.pBufferMemoryBarriers = &memoryBarrier;

                            vkCmdPipelineBarrier2( cmd, &dependencyInfo );
                        }
                        else
                        {
                            s_barriers.emplace_back( memoryBarrier );
                        }
                        s_transferQueue._requests.pop_front();
                    }

                    if ( !s_barriers.empty() )
                    {
                        VkDependencyInfo dependencyInfo = vk::dependencyInfo();
                        dependencyInfo.bufferMemoryBarrierCount = to_U32( s_barriers.size() );
                        dependencyInfo.pBufferMemoryBarriers = s_barriers.data();

                        vkCmdPipelineBarrier2( cmd, &dependencyInfo );
                        efficient_clear( s_barriers );
                    }

                } );
            }
        }


        if ( GFXDevice::IsSubmitCommand( cmdType ) && GetStateTracker()._descriptorsUpdated )
        {
            vkCmdBindDescriptorSets( cmdBuffer,
                                     GetStateTracker()._isGraphicsPipeline ? VK_PIPELINE_BIND_POINT_GRAPHICS
                                     : VK_PIPELINE_BIND_POINT_COMPUTE,
                                     GetStateTracker()._activePipelineLayout,
                                     0,
                                     to_base( DescriptorSetUsage::COUNT ),
                                     _descriptorSets.data(),
                                     0,
                                     nullptr );
            GetStateTracker()._descriptorsUpdated = false;
        }

        switch ( cmdType )
        {
            case GFX::CommandType::BEGIN_RENDER_PASS:
            {
                const GFX::BeginRenderPassCommand* crtCmd = cmd->As<GFX::BeginRenderPassCommand>();
                VK_API::GetStateTracker()._activeRenderTargetID = crtCmd->_target;
                if ( crtCmd->_target == SCREEN_TARGET_ID )
                {
                    VkClearValue clearValue{};
                    clearValue.color =
                    {
                        DefaultColours::DIVIDE_BLUE.r,
                        DefaultColours::DIVIDE_BLUE.g,
                        DefaultColours::DIVIDE_BLUE.b,
                        DefaultColours::DIVIDE_BLUE.a
                    };

                    _defaultRenderPass.pClearValues = &clearValue;
                    vkCmdBeginRenderPass( cmdBuffer, &_defaultRenderPass, VK_SUBPASS_CONTENTS_INLINE );
                    GetStateTracker()._pipelineRenderingCreateInfo.colorAttachmentCount = 1u;
                    GetStateTracker()._activeMSAASamples = 1u;
                }
                else
                {
                    vkRenderTarget* rt = static_cast<vkRenderTarget*>(_context.renderTargetPool().getRenderTarget( crtCmd->_target ));
                    Attorney::VKAPIRenderTarget::begin( *rt, cmdBuffer, crtCmd->_descriptor, crtCmd->_clearDescriptor, GetStateTracker()._pipelineRenderingCreateInfo );

                    GetStateTracker()._alphaToCoverage = crtCmd->_descriptor._alphaToCoverage;
                    GetStateTracker()._activeMSAASamples = rt->getSampleCount();
                    if ( crtCmd->_descriptor._setViewport )
                    {
                        _context.setViewport( { 0, 0, rt->getWidth(), rt->getHeight() } );
                    }
                }

                PushDebugMessage( cmdBuffer, crtCmd->_name.c_str() );
            } break;
            case GFX::CommandType::END_RENDER_PASS:
            {
                PopDebugMessage( cmdBuffer );
                GetStateTracker()._alphaToCoverage = false;
                GetStateTracker()._activeMSAASamples = _context.context().config().rendering.MSAASamples;

                if ( GetStateTracker()._activeRenderTargetID == SCREEN_TARGET_ID )
                {
                    vkCmdEndRenderPass( cmdBuffer );
                }
                else
                {
                    vkRenderTarget* rt = static_cast<vkRenderTarget*>(_context.renderTargetPool().getRenderTarget( GetStateTracker()._activeRenderTargetID ));
                    Attorney::VKAPIRenderTarget::end( *rt, cmdBuffer );
                    GetStateTracker()._activeRenderTargetID = SCREEN_TARGET_ID;
                }
                GetStateTracker()._pipelineRenderingCreateInfo = {};
            }break;
            case GFX::CommandType::BLIT_RT:
            {
                PROFILE_SCOPE( "BLIT_RT", Profiler::Category::Graphics );

                const GFX::BlitRenderTargetCommand* crtCmd = cmd->As<GFX::BlitRenderTargetCommand>();
                vkRenderTarget* source = static_cast<vkRenderTarget*>(_context.renderTargetPool().getRenderTarget( crtCmd->_source ));
                vkRenderTarget* destination = static_cast<vkRenderTarget*>(_context.renderTargetPool().getRenderTarget( crtCmd->_destination ));
                Attorney::VKAPIRenderTarget::blitFrom( *destination, cmdBuffer, source, crtCmd->_params );

            } break;
            case GFX::CommandType::BEGIN_GPU_QUERY:
            {
            }break;
            case GFX::CommandType::END_GPU_QUERY:
            {
            }break;
            case GFX::CommandType::COPY_TEXTURE:
            {
            }break;
            case GFX::CommandType::BIND_PIPELINE:
            {
                if ( pushConstantsNeedLock )
                {
                    flushCommand( &pushConstantsMemCommand );
                    pushConstantsMemCommand._bufferLocks.clear();
                    pushConstantsNeedLock = false;
                }

                const Pipeline* pipeline = cmd->As<GFX::BindPipelineCommand>()->_pipeline;
                assert( pipeline != nullptr );
                if ( bindPipeline( *pipeline, cmdBuffer ) == ShaderResult::Failed )
                {
                    Console::errorfn( Locale::Get( _ID( "ERROR_GLSL_INVALID_BIND" ) ), pipeline->descriptor()._shaderProgramHandle );
                }
            } break;
            case GFX::CommandType::SEND_PUSH_CONSTANTS:
            {
                if ( GetStateTracker()._activePipeline != VK_NULL_HANDLE )
                {
                    const PushConstants& pushConstants = cmd->As<GFX::SendPushConstantsCommand>()->_constants;
                    if ( GetStateTracker()._activeShaderProgram->uploadUniformData( pushConstants, _context.descriptorSet( DescriptorSetUsage::PER_DRAW ).impl(), pushConstantsMemCommand ) )
                    {
                        _context.descriptorSet( DescriptorSetUsage::PER_DRAW ).dirty( true );
                    }
                    if ( pushConstants.fastData()._set )
                    {
                        const F32* constantData = pushConstants.fastData().dataPtr();
                        vkCmdPushConstants( cmdBuffer,
                                            GetStateTracker()._activePipelineLayout,
                                            GetStateTracker()._activeShaderProgram->stageMask(),
                                            0,
                                            to_U32( PushConstantsStruct::Size() ),
                                            &constantData );
                    }

                    pushConstantsNeedLock = !pushConstantsMemCommand._bufferLocks.empty();
                }
            } break;
            case GFX::CommandType::SET_SCISSOR:
            {
                if ( !setScissor( cmd->As<GFX::SetScissorCommand>()->_rect ) )
                {
                    NOP();
                }
            }break;
            case GFX::CommandType::BEGIN_DEBUG_SCOPE:
            {
                const GFX::BeginDebugScopeCommand* crtCmd = cmd->As<GFX::BeginDebugScopeCommand>();
                PushDebugMessage( cmdBuffer, crtCmd->_scopeName.c_str(), crtCmd->_scopeId );
            } break;
            case GFX::CommandType::END_DEBUG_SCOPE:
            {
                PopDebugMessage( cmdBuffer );
            } break;
            case GFX::CommandType::ADD_DEBUG_MESSAGE:
            {
                const GFX::AddDebugMessageCommand* crtCmd = cmd->As<GFX::AddDebugMessageCommand>();
                InsertDebugMessage( cmdBuffer, crtCmd->_msg.c_str(), crtCmd->_msgId );
            }break;
            case GFX::CommandType::COMPUTE_MIPMAPS:
            {
                const GFX::ComputeMipMapsCommand* crtCmd = cmd->As<GFX::ComputeMipMapsCommand>();

                if ( crtCmd->_layerRange.x == 0 && crtCmd->_layerRange.y == crtCmd->_texture->descriptor().layerCount() )
                {
                    PROFILE_SCOPE( "VK: In-place computation - Full", Profiler::Category::Graphics );
                }
                else
                {
                    PROFILE_SCOPE( "VK: View - based computation", Profiler::Category::Graphics );
                }
            }break;
            case GFX::CommandType::DRAW_TEXT:
            {
                const GFX::DrawTextCommand* crtCmd = cmd->As<GFX::DrawTextCommand>();
                drawText( crtCmd->_batch );
            }break;
            case GFX::CommandType::DRAW_COMMANDS:
            {
                const GFX::DrawCommand::CommandContainer& drawCommands = cmd->As<GFX::DrawCommand>()->_drawCommands;

                if ( GetStateTracker()._activePipeline != VK_NULL_HANDLE )
                {
                    U32 drawCount = 0u;
                    for ( const GenericDrawCommand& currentDrawCommand : drawCommands )
                    {
                        if ( draw( currentDrawCommand, cmdBuffer ) )
                        {
                            drawCount += isEnabledOption( currentDrawCommand, CmdRenderOptions::RENDER_WIREFRAME )
                                ? 2
                                : isEnabledOption( currentDrawCommand, CmdRenderOptions::RENDER_GEOMETRY ) ? 1 : 0;
                        }
                    }
                    _context.registerDrawCalls( drawCount );
                }
            }break;
            case GFX::CommandType::DISPATCH_COMPUTE:
            {
                assert( GetStateTracker()._activeTopology == PrimitiveTopology::COMPUTE );
                if ( GetStateTracker()._activePipeline != VK_NULL_HANDLE )
                {
                }
            } break;
            case GFX::CommandType::SET_CLIPING_STATE:
            {
            } break;
            case GFX::CommandType::MEMORY_BARRIER:
            {
                const GFX::MemoryBarrierCommand* crtCmd = cmd->As<GFX::MemoryBarrierCommand>();

                if ( !crtCmd->_bufferLocks.empty() )
                {
                    NOP();
                }
                for ( [[maybe_unused]] auto it : crtCmd->_fenceLocks )
                {
                    NOP();
                }

                static std::array<VkImageMemoryBarrier2, to_base( RTColourAttachmentSlot::COUNT ) + 1> memBarriers{};
                U8 memBarrierCount = 0u;
                VkImageMemoryBarrier2 memBarrier{};
                for ( const auto& it : crtCmd->_textureLayoutChanges )
                {
                    if ( static_cast<vkTexture*>(it._targetView._srcTexture._internalTexture)->transitionLayout( it._layout, it._prevLayoutOverride, it._targetView._subRange, memBarrier ) )
                    {
                        memBarriers[memBarrierCount++] = memBarrier;
                    }
                }
                if ( memBarrierCount > 0u )
                {
                    VkDependencyInfo dependencyInfo = vk::dependencyInfo();
                    dependencyInfo.imageMemoryBarrierCount = memBarrierCount;
                    dependencyInfo.pImageMemoryBarriers = memBarriers.data();

                    vkCmdPipelineBarrier2( cmdBuffer, &dependencyInfo );
                }
            } break;
            default: break;
        }

        if ( GFXDevice::IsSubmitCommand( cmd->Type() ) )
        {
            if ( pushConstantsNeedLock )
            {
                flushCommand( &pushConstantsMemCommand );
                pushConstantsMemCommand._bufferLocks.clear();
                pushConstantsNeedLock = false;
            }
        }
    }

    void VK_API::preFlushCommandBuffer( [[maybe_unused]] const GFX::CommandBuffer& commandBuffer )
    {
        GetStateTracker()._activeRenderTargetID = SCREEN_TARGET_ID;
    }

    void VK_API::postFlushCommandBuffer( [[maybe_unused]] const GFX::CommandBuffer& commandBuffer ) noexcept
    {
        s_transientDeleteQueue.flush( _device->getDevice() );
        GetStateTracker()._activeRenderTargetID = INVALID_RENDER_TARGET_ID;
    }

    vec2<U16> VK_API::getDrawableSize( const DisplayWindow& window ) const noexcept
    {
        int w = 1, h = 1;
        SDL_Vulkan_GetDrawableSize( window.getRawWindow(), &w, &h );
        return vec2<U16>( w, h );
    }

    bool VK_API::setViewport( const Rect<I32>& newViewport ) noexcept
    {
        VkCommandBuffer cmdBuffer = getCurrentCommandBuffer();
        const VkViewport targetViewport{ to_F32( newViewport.offsetX ),
                                        to_F32( newViewport.sizeY ) - to_F32( newViewport.offsetY ),
                                        to_F32( newViewport.sizeX ),
                                        -to_F32( newViewport.sizeY ),
                                        0.f,
                                        1.f };
        if ( GetStateTracker()._dynamicState._activeViewport != targetViewport )
        {
            vkCmdSetViewport( cmdBuffer, 0, 1, &targetViewport );
            GetStateTracker()._dynamicState._activeViewport = targetViewport;
            return true;
        }

        return false;
    }

    bool VK_API::setScissor( const Rect<I32>& newScissor ) noexcept
    {
        VkCommandBuffer cmdBuffer = getCurrentCommandBuffer();

        const VkOffset2D offset{ std::max( 0, newScissor.offsetX ), std::max( 0, newScissor.offsetY ) };
        const VkExtent2D extent{ to_U32( newScissor.sizeX ),to_U32( newScissor.sizeY ) };
        const VkRect2D targetScissor{ offset, extent };
        if ( GetStateTracker()._dynamicState._activeScissor != targetScissor )
        {
            vkCmdSetScissor( cmdBuffer, 0, 1, &targetScissor );
            return true;
        }

        return false;
    }

    void VK_API::initDescriptorSets()
    {
        const ShaderProgram::BindingSetData& bindingData = ShaderProgram::GetBindingSetData();
        eastl::fixed_vector<VkDescriptorSetLayoutBinding, ShaderProgram::MAX_SLOTS_PER_DESCRIPTOR_SET, false> layoutBinding{};

        for ( U8 i = 0u; i < to_base( DescriptorSetUsage::COUNT ); ++i )
        {
            if ( i == to_base( DescriptorSetUsage::PER_DRAW ) )
            {
                continue;
            }

            auto& bindings = bindingData[i];
            for ( U8 slot = 0u; slot < ShaderProgram::MAX_SLOTS_PER_DESCRIPTOR_SET; ++slot )
            {
                if ( bindings[slot]._type == DescriptorSetBindingType::COUNT || (slot == 0 && i == to_base( DescriptorSetUsage::PER_BATCH )) )
                {
                    continue;
                }

                VkDescriptorSetLayoutBinding& newBinding = layoutBinding.emplace_back();
                newBinding.descriptorCount = 1u;
                newBinding.descriptorType = VKUtil::vkDescriptorType( bindings[slot]._type );
                newBinding.stageFlags = GetFlagsForStageVisibility( bindings[slot]._visibility );
                newBinding.binding = slot;
                newBinding.pImmutableSamplers = nullptr;
            }
            VkDescriptorSetLayoutCreateInfo layoutCreateInfo = vk::descriptorSetLayoutCreateInfo( layoutBinding.data(), to_U32( layoutBinding.size() ) );
            _descriptorSetLayouts[i] = _descriptorLayoutCache->createDescriptorLayout( &layoutCreateInfo );
            efficient_clear( layoutBinding );
        }
    }

    void VK_API::onThreadCreated( [[maybe_unused]] const std::thread::id& threadID ) noexcept
    {
    }

    VKStateTracker& VK_API::GetStateTracker() noexcept
    {
        return s_stateTracker;
    }

    void VK_API::InsertDebugMessage( VkCommandBuffer cmdBuffer, const char* message, [[maybe_unused]] const U32 id )
    {
        if ( s_hasDebugMarkerSupport )
        {
            static F32 color[4] = { 0.0f, 1.0f, 0.0f, 1.f };

            VkDebugUtilsLabelEXT labelInfo{};
            labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            labelInfo.pLabelName = message;
            memcpy( labelInfo.color, &color[0], sizeof( F32 ) * 4 );

            Debug::vkCmdInsertDebugUtilsLabelEXT( cmdBuffer, &labelInfo );
        }
    }

    void VK_API::PushDebugMessage( VkCommandBuffer cmdBuffer, const char* message, const U32 id )
    {
        if ( s_hasDebugMarkerSupport )
        {
            static F32 color[4] = { 0.5f, 0.5f, 0.5f, 1.f };
            VkDebugUtilsLabelEXT labelInfo{};
            labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            labelInfo.pLabelName = message;
            memcpy( labelInfo.color, &color[0], sizeof( F32 ) * 4 );
            Debug::vkCmdBeginDebugUtilsLabelEXT( cmdBuffer, &labelInfo );
        }

        assert( GetStateTracker()._debugScopeDepth < GetStateTracker()._debugScope.size() );
        GetStateTracker()._debugScope[GetStateTracker()._debugScopeDepth++] = { message, id };
    }

    void VK_API::PopDebugMessage( VkCommandBuffer cmdBuffer )
    {
        if ( s_hasDebugMarkerSupport )
        {
            Debug::vkCmdEndDebugUtilsLabelEXT( cmdBuffer );
        }

        GetStateTracker()._debugScope[GetStateTracker()._debugScopeDepth--] = { "", U32_MAX };
    }

    /// Return the Vulkan sampler object's handle for the given hash value
    VkSampler VK_API::GetSamplerHandle( const size_t samplerHash )
    {
        // If the hash value is 0, we assume the code is trying to unbind a sampler object
        if ( samplerHash > 0 )
        {
            {
                SharedLock<SharedMutex> r_lock( s_samplerMapLock );
                // If we fail to find the sampler object for the given hash, we print an error and return the default OpenGL handle
                const SamplerObjectMap::const_iterator it = s_samplerMap.find( samplerHash );
                if ( it != std::cend( s_samplerMap ) )
                {
                    // Return the OpenGL handle for the sampler object matching the specified hash value
                    return it->second;
                }
            }
            {
                ScopedLock<SharedMutex> w_lock( s_samplerMapLock );
                // Check again
                const SamplerObjectMap::const_iterator it = s_samplerMap.find( samplerHash );
                if ( it == std::cend( s_samplerMap ) )
                {
                    // Cache miss. Create the sampler object now.
                    // Create and store the newly created sample object. GL_API is responsible for deleting these!
                    const VkSampler sampler = vkSamplerObject::Construct( SamplerDescriptor::Get( samplerHash ) );
                    emplace( s_samplerMap, samplerHash, sampler );
                    return sampler;
                }
            }
        }

        return 0u;
    }
}; //namespace Divide