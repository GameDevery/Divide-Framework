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
#ifndef _VERTEX_DATA_INTERFACE_H_
#define _VERTEX_DATA_INTERFACE_H_

#include "BufferLocks.h"
#include "Core/Headers/ObjectPool.h"
#include "Platform/Video/Headers/GraphicsResource.h"

namespace Divide {

struct RenderStagePass;
struct GenericDrawCommand;

struct VDIUserData{};

class NOINITVTABLE VertexDataInterface : public GUIDWrapper, public GraphicsResource {
   public:
    using Handle = PoolHandle;
    static constexpr Handle INVALID_VDI_HANDLE{U16_MAX, 0u};

    explicit VertexDataInterface(GFXDevice& context, const Str256& name);
    virtual ~VertexDataInterface();

    virtual void draw(const GenericDrawCommand& command, VDIUserData* data) = 0;

    PROPERTY_R(Handle, handle);
    PROPERTY_RW(bool, primitiveRestartRequired, false);

    using VDIPool = ObjectPool<VertexDataInterface, 4096>;
    // We only need this pool in order to get a valid handle to pass around to command buffers instead of using raw pointers
    static VDIPool s_VDIPool;
};

};  // namespace Divide

#endif //_VERTEX_DATA_INTERFACE_H_

#include "VertexDataInterface.inl"
