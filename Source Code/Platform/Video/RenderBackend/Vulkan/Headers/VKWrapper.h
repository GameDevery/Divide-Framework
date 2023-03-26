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
#ifndef _VK_WRAPPER_H_
#define _VK_WRAPPER_H_

#include "vkDevice.h"
#include "vkSwapChain.h"
#include "vkMemAllocatorInclude.h"

#include "Platform/Video/Headers/CommandsImpl.h"
#include "Platform/Video/Headers/RenderAPIWrapper.h"
#include "Platform/Video/Shaders/Headers/ShaderProgram.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/VertexDataInterface.h"

#include "vkDescriptors.h"

struct FONScontext;

namespace Divide {

class Pipeline;
enum class ShaderResult : U8;

struct vkUserData : VDIUserData
{
    VkCommandBuffer* _cmdBuffer = nullptr;
};

class RenderStateBlock;
class VK_API final : public RenderAPIWrapper {
  public:
  constexpr static VkPipelineStageFlagBits2 ALL_SHADER_STAGES = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                                                                VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |
                                                                VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT |
                                                                VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT |
                                                                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                                                                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  public:
 
    VK_API(GFXDevice& context) noexcept;

    [[nodiscard]] VKDevice* getDevice() { return _device.get(); }

    [[nodiscard]] GFXDevice& context() noexcept { return _context; };
    [[nodiscard]] const GFXDevice& context() const noexcept { return _context; };

  protected:
      [[nodiscard]] VkCommandBuffer getCurrentCommandBuffer() const noexcept;

      void idle(bool fast) noexcept override;

      [[nodiscard]] bool drawToWindow( DisplayWindow& window ) override;
                    void flushWindow( DisplayWindow& window ) override;
      [[nodiscard]] bool frameStarted() override;
      [[nodiscard]] bool frameEnded() override;

      [[nodiscard]] ErrorCode initRenderingAPI(I32 argc, char** argv, Configuration& config) noexcept override;
      void closeRenderingAPI() override;
      void preFlushCommandBuffer(const GFX::CommandBuffer& commandBuffer) override;
      void flushCommand(GFX::CommandBase* cmd) noexcept override;
      void postFlushCommandBuffer(const GFX::CommandBuffer& commandBuffer) noexcept override;
      bool setViewportInternal(const Rect<I32>& newViewport) noexcept override;
      bool setScissorInternal(const Rect<I32>& newScissor) noexcept override;

      void onThreadCreated(const std::thread::id& threadID) noexcept override;
      void initDescriptorSets() override;

private:
    void initStatePerWindow( VKPerWindowState& windowState );
    void destroyStatePerWindow( VKPerWindowState& windowState );
    void recreateSwapChain( VKPerWindowState& windowState );

    bool draw(const GenericDrawCommand& cmd, VkCommandBuffer cmdBuffer) const;
    bool setViewportInternal( const Rect<I32>& newViewport, VkCommandBuffer cmdBuffer ) noexcept;
    bool setScissorInternal( const Rect<I32>& newScissor, VkCommandBuffer cmdBuffer ) noexcept;
    void destroyPipelineCache();
    void flushPushConstantsLocks();
    VkDescriptorSetLayout createLayoutFromBindings( const DescriptorSetUsage usage, const ShaderProgram::BindingsPerSetArray& bindings );

    ShaderResult bindPipeline(const Pipeline& pipeline, VkCommandBuffer cmdBuffer);
    void bindDynamicState(const VKDynamicState& currentState, VkCommandBuffer cmdBuffer) noexcept;
    [[nodiscard]] bool bindShaderResources(DescriptorSetUsage usage, const DescriptorSet& bindings, bool isDirty) override;

public:
    static [[nodiscard]] VKStateTracker& GetStateTracker() noexcept;
    static [[nodiscard]] VkSampler GetSamplerHandle(size_t samplerHash);

    static void RegisterCustomAPIDelete(DELEGATE<void, VkDevice>&& cbk, bool isResourceTransient);
    static void RegisterTransferRequest(const VKTransferQueue::TransferRequest& request);
    static void FlushBufferTransferRequests( VkCommandBuffer cmdBuffer );
    static void FlushBufferTransferRequests( );
    static void SubmitTransferRequest(const VKTransferQueue::TransferRequest& request, VkCommandBuffer cmd);

    static void InsertDebugMessage(VkCommandBuffer cmdBuffer, const char* message, U32 id = U32_MAX);
    static void PushDebugMessage(VkCommandBuffer cmdBuffer, const char* message, U32 id = U32_MAX);
    static void PopDebugMessage(VkCommandBuffer cmdBuffer);

private:
    GFXDevice& _context;
    vkb::Instance _vkbInstance;
    VKDevice_uptr _device{ nullptr };
    VmaAllocator _allocator{VK_NULL_HANDLE};
    VkPipelineCache _pipelineCache{ VK_NULL_HANDLE };

    GFX::MemoryBarrierCommand _pushConstantsMemCommand{};
    DescriptorLayoutCache_uptr _descriptorLayoutCache{nullptr};

    hashMap<I64, VKPerWindowState> _perWindowState;
    hashMap<size_t, CompiledPipeline> _compiledPipelines;

    std::array<VkDescriptorSet, to_base( DescriptorSetUsage::COUNT )> _descriptorSets;
    std::array<VkDescriptorSetLayout, to_base( DescriptorSetUsage::COUNT )> _descriptorSetLayouts;

    U8 _currentFrameIndex{ 0u };
    bool _pushConstantsNeedLock{ false };
private:
    using SamplerObjectMap = hashMap<size_t, VkSampler, NoHash<size_t>>;

    static SharedMutex s_samplerMapLock;
    static SamplerObjectMap s_samplerMap;
    static VKStateTracker s_stateTracker;
    static VKDeletionQueue s_transientDeleteQueue;
    static VKDeletionQueue s_deviceDeleteQueue;
    static VKTransferQueue s_transferQueue;

public:
    struct DepthFormatInformation
    {
        bool _d24s8Supported{false};
        bool _d32s8Supported{false};
        bool _d24x8Supported{false};
        bool _d32FSupported{false};
    };

    static bool s_hasDebugMarkerSupport;
    static bool s_hasValidationFeaturesSupport;
    static DepthFormatInformation s_depthFormatInformation;
};

};  // namespace Divide

#endif //_VK_WRAPPER_H_
