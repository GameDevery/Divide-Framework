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
#ifndef DVD_GENERIC_VERTEX_DATA_H
#define DVD_GENERIC_VERTEX_DATA_H

#include "Core/Headers/RingBuffer.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/BufferParams.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/VertexDataInterface.h"

/// This class is used to upload generic VB data to the GPU that can be rendered directly or instanced.
/// Use this class to create precise VB data with specific usage (such as particle systems)
/// Use IMPrimitive for on-the-fly geometry creation

namespace Divide {

NOINITVTABLE_CLASS(GenericVertexData) : public VertexDataInterface,
                                        public RingBuffer
{
   public:
     static constexpr size_t INVALID_INDEX_OFFSET = SIZE_MAX;

     struct IndexBuffer
     {
         bufferPtr data{ nullptr };
         size_t count{ 0u };
         bool smallIndices{ false };
         bool indicesNeedCast{ false };
         bool dynamic{ false };
         bool useRingBuffer{ false };
         vector<U16> smallIndicesTemp;
     };

     struct SetBufferParams
     {
         static constexpr size_t INVALID_ELEMENT_STRIDE = SIZE_MAX;

         struct BufferBindConfig 
         {
             U16 _bufferIdx{ 0u };
             U16 _bindIdx{ 0u };
         };

         BufferParams _bufferParams{ ._usageType = BufferUsageType::VERTEX_BUFFER };
         BufferBindConfig _bindConfig{};
         size_t _elementStride{ INVALID_ELEMENT_STRIDE };
         std::pair<bufferPtr, size_t> _initialData{nullptr, 0};
         bool _useRingBuffer{ false };
     };

   public:
    GenericVertexData(GFXDevice& context, U16 ringBufferLength, const std::string_view name);

    virtual BufferLock setIndexBuffer(const IndexBuffer& indices) = 0;

    virtual void reset() = 0; ///< Also clears GPU memory

    /// When reading and writing to the same buffer, we use a round-robin approach and
    /// offset the reading and writing to multiple copies of the data
    [[nodiscard]] virtual BufferLock setBuffer(const SetBufferParams& params) = 0;

    [[nodiscard]] virtual BufferLock updateBuffer(U32 buffer,
                                                  U32 elementCountOffset,
                                                  U32 elementCountRange,
                                                  bufferPtr data) = 0;

    [[nodiscard]] virtual BufferLock updateIndexBuffer(U32 elementCountOffset,
                                                       U32 elementCountRange,
                                                       bufferPtr data) = 0;

     PROPERTY_R_IW(size_t, firstIndexOffsetCount, 0u);
   protected:
    Str<256> _name;
};

};  // namespace Divide

#endif //DVD_GENERIC_VERTEX_DATA_H
