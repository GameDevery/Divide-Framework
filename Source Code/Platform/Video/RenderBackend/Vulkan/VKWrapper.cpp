﻿#include "stdafx.h"

#include "Headers/VKWrapper.h"

namespace Divide {
    VK_API::VK_API(GFXDevice& context)
    {
        ACKNOWLEDGE_UNUSED(context);
    }

    void VK_API::idle(const bool fast) {
        ACKNOWLEDGE_UNUSED(fast);
    }

    void VK_API::beginFrame(DisplayWindow& window, bool global) {
        ACKNOWLEDGE_UNUSED(window);
        ACKNOWLEDGE_UNUSED(global);
    }

    void VK_API::endFrame(DisplayWindow& window, bool global) {
        ACKNOWLEDGE_UNUSED(window);
        ACKNOWLEDGE_UNUSED(global);
    }

    size_t VK_API::queueTextureResidency(U64 textureAddress, bool makeResident) {
        ACKNOWLEDGE_UNUSED(textureAddress);
        ACKNOWLEDGE_UNUSED(makeResident);

        return 0;
    }

    ErrorCode VK_API::initRenderingAPI(I32 argc, char** argv, Configuration& config) {
        ACKNOWLEDGE_UNUSED(argc);
        ACKNOWLEDGE_UNUSED(argv);
        ACKNOWLEDGE_UNUSED(config);

        return ErrorCode::NO_ERR;
    }

    void VK_API::closeRenderingAPI() {
    }

    F32 VK_API::getFrameDurationGPU() const noexcept {
        return 0.f;
    }

    void VK_API::flushCommand(const GFX::CommandBuffer::CommandEntry& entry, const GFX::CommandBuffer& commandBuffer) {
        ACKNOWLEDGE_UNUSED(entry);
        ACKNOWLEDGE_UNUSED(commandBuffer);
    }

    void VK_API::postFlushCommandBuffer(const GFX::CommandBuffer& commandBuffer) {
        ACKNOWLEDGE_UNUSED(commandBuffer);
    }

    vec2<U16> VK_API::getDrawableSize(const DisplayWindow& window) const {
        ACKNOWLEDGE_UNUSED(window);

        return vec2<U16>(1);
    }

    U32 VK_API::getHandleFromCEGUITexture(const CEGUI::Texture& textureIn) const {
        ACKNOWLEDGE_UNUSED(textureIn);

        return 0u;
    }

    bool VK_API::setViewport(const Rect<I32>& newViewport) {
        ACKNOWLEDGE_UNUSED(newViewport);

        return true;
    }

    void VK_API::onThreadCreated(const std::thread::id& threadID) {
        ACKNOWLEDGE_UNUSED(threadID);
    }

}; //namespace Divide