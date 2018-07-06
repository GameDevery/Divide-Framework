/*
   Copyright (c) 2014 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy of this software
   and associated documentation files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef _D3D_GENERIC_VERTEX_DATA_H
#define _D3d_GENERIC_VERTEX_DATA_H

#include "Hardware/Video/Buffers/VertexBufferObject/Headers/GenericVertexData.h"

class d3dGenericVertexData : public GenericVertexData {

public:
    d3dGenericVertexData() : GenericVertexData()
    {
    }
    ~d3dGenericVertexData()
    {
    }

    void Clear()
    {
    }
    void Create(U8 numBuffers = 1) 
    {
    }
    void Draw(const PrimitiveType& type, U32 min, U32 max)
    {
    }
    void DrawInstanced(const PrimitiveType& type, U32 count, U32 min, U32 max)
    {
    }
    void SetBuffer(U32 buffer, size_t dataSize, void* data, bool dynamic, bool stream)
    {
    }
    void UpdateBuffer(U32 buffer, size_t dataSize, void* data, U32 offset, size_t currentSize, bool dynamic, bool steam)
    {
    }
    void SetAttribute(U32 index, U32 buffer, U32 divisor, size_t size, bool normalized, U32 stride, U32 offset, const GFXDataFormat& type) 
    {
    }
};
#endif
