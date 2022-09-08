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
#ifndef VK_FRAME_BUFFER_H
#define VK_FRAME_BUFFER_H

#include "Platform/Video/Buffers/RenderTarget/Headers/RenderTarget.h"

#include <Vulkan/vulkan_core.h>

namespace Divide {

    class GFXDevice;
    class vkRenderTarget final : public RenderTarget {
    public:
        vkRenderTarget(GFXDevice& context, const RenderTargetDescriptor& descriptor);
        ~vkRenderTarget();

        bool create() override;
        void destroy();

        void clear(const RTClearDescriptor& descriptor) noexcept override;

        void setDefaultState(const RTDrawDescriptor& drawPolicy) noexcept override;

        void readData(const vec4<U16>& rect, GFXImageFormat imageFormat, GFXDataFormat dataType, std::pair<bufferPtr, size_t> outData) const noexcept override;

        void blitFrom(const RTBlitParams& params) noexcept override;

        const VkRenderPassBeginInfo& getRenderPassInfo();

        PROPERTY_R_IW(VkRenderPass, renderPass, VK_NULL_HANDLE);
        PROPERTY_R_IW(VkFramebuffer, framebuffer, VK_NULL_HANDLE);

        PROPERTY_INTERNAL(VkRenderPassBeginInfo, renderPassBeginInfo);
    };
} //namespace Divide

#endif //VK_FRAME_BUFFER_H