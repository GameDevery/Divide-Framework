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
#ifndef VK_GENERIC_VERTEX_DATA_H
#define VK_GENERIC_VERTEX_DATA_H

#include "Platform/Video/RenderBackend/Vulkan/Headers/vkResources.h"
#include "Platform/Video/Buffers/VertexBuffer/GenericBuffer/Headers/GenericVertexData.h"

namespace Divide {

FWD_DECLARE_MANAGED_STRUCT(vkBufferImpl);

class vkGenericVertexData final : public GenericVertexData {
    struct GenericBufferImpl;

    public:
        vkGenericVertexData(GFXDevice& context, const U16 ringBufferLength, const std::string_view name);
        ~vkGenericVertexData();

        void reset() override;

        void draw(const GenericDrawCommand& command, VDIUserData* data) noexcept override;

        [[nodiscard]] BufferLock setIndexBuffer( const IndexBuffer& indices ) override;

        [[nodiscard]] BufferLock setBuffer(const SetBufferParams& params) noexcept override;

        [[nodiscard]] BufferLock updateBuffer(U32 buffer,
                                              U32 elementCountOffset,
                                              U32 elementCountRange,
                                              bufferPtr data) noexcept override;

        [[nodiscard]] BufferLock updateIndexBuffer(U32 elementCountOffset,
                                                   U32 elementCountRange,
                                                   bufferPtr data) noexcept override;

    private:
        void bindBufferInternal(const SetBufferParams::BufferBindConfig& bindConfig,
                                VkCommandBuffer& cmdBuffer);

        [[nodiscard]] BufferLock updateBuffer(GenericBufferImpl& buffer,
                                              U32 elementCountOffset,
                                              U32 elementCountRange,
                                              VkAccessFlags2 dstAccessMask,
                                              VkPipelineStageFlags2 dstStageMask,
                                              bufferPtr data);

    private:
        struct GenericBufferImpl
        {
            vkBufferImpl_uptr _buffer{ nullptr };
            size_t _ringSizeFactor{ 1u };
            size_t _elementStride{ 0u };
        };

        struct GenericBindableBufferImpl : GenericBufferImpl
        {
            SetBufferParams::BufferBindConfig _bindConfig{};
        };

        struct GenericIndexBufferImpl : GenericBufferImpl
        {
            IndexBuffer _data{};
            size_t _bufferSize{ 0u };
        };

        GenericIndexBufferImpl _indexBuffer;
        eastl::fixed_vector<GenericBindableBufferImpl,1,true> _bufferObjects;
        SharedMutex _idxBufferLock;
    };

} //namespace Divide

#endif //VK_GENERIC_VERTEX_DATA_H
