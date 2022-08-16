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

#include "Core/Headers/ObjectPool.h"

#include "Platform/Video/Headers/GraphicsResource.h"
#include "Platform/Video/Headers/RenderAPIEnums.h"

namespace Divide {

struct RenderStagePass;
struct GenericDrawCommand;

struct BufferParams
{
    U32 _elementCount{ 0u };
    size_t _elementSize{ 0u };     ///< Buffer primitive size in bytes
    BufferUpdateFrequency _updateFrequency{ BufferUpdateFrequency::COUNT };
    BufferUpdateUsage _updateUsage{ BufferUpdateUsage::COUNT };
};

inline bool operator==(const BufferParams& lhs, const BufferParams& rhs) noexcept {
    return lhs._elementCount == rhs._elementCount &&
           lhs._elementSize == rhs._elementSize &&
           lhs._updateFrequency == rhs._updateFrequency &&
           lhs._updateUsage == rhs._updateUsage;
}

inline bool operator!=(const BufferParams& lhs, const BufferParams& rhs) noexcept {
    return lhs._elementCount != rhs._elementCount ||
           lhs._elementSize != rhs._elementSize ||
           lhs._updateFrequency != rhs._updateFrequency ||
           lhs._updateUsage != rhs._updateUsage;
}

struct BufferRange {
    size_t _startOffset{ 0u };
    size_t _length{ 0u };

    FORCE_INLINE [[nodiscard]] size_t endOffset() const noexcept { return _startOffset + _length; }
};

inline bool operator==(const BufferRange& lhs, const BufferRange& rhs) noexcept {
    return lhs._startOffset == rhs._startOffset &&
           lhs._length == rhs._length;
}

inline bool operator!=(const BufferRange& lhs, const BufferRange& rhs) noexcept {
    return lhs._startOffset != rhs._startOffset ||
           lhs._length == rhs._length;
}

[[nodiscard]] inline bool Overlaps(const BufferRange& lhs, const BufferRange& rhs) noexcept {
    return lhs._startOffset < rhs.endOffset() &&
           rhs._startOffset < lhs.endOffset();
}

inline void Merge(BufferRange& lhs, const BufferRange& rhs) noexcept {
    const size_t lhsEndOffset = lhs.endOffset();
    const size_t rhsEndOffset = rhs.endOffset();
    lhs._startOffset = std::min(lhs._startOffset, rhs._startOffset);
    lhs._length = std::max(lhsEndOffset, rhsEndOffset) - lhs._startOffset;
}

class ShaderBuffer;
struct BufferLock {
    const ShaderBuffer* _targetBuffer = nullptr;
    BufferRange _range{};
};

class GenericVertexData;

using BufferLocks = eastl::fixed_vector<BufferLock, 3, true, eastl::dvd_allocator>;
using FenceLocks = eastl::fixed_vector<GenericVertexData*, 3, true, eastl::dvd_allocator>;

inline [[nodiscard]] bool IsEmpty(const BufferLocks& locks) noexcept {
    if (locks.empty()) {
        return true;
    }

    for (auto& it : locks) {
        if (it._targetBuffer != nullptr) {
            return false;
        }
    }

    return true;
}

inline [[nodiscard]] bool IsEmpty(const FenceLocks& locks) noexcept {
    if (locks.empty()) {
        return true;
    }

    for (auto& it : locks) {
        if (it != nullptr) {
            return false;
        }
    }

    return true;
}

struct VDIUserData {};

class NOINITVTABLE VertexDataInterface : public GUIDWrapper, public GraphicsResource {
   public:
    using Handle = PoolHandle;
    static constexpr Handle INVALID_VDI_HANDLE{U16_MAX, 0u};

    explicit VertexDataInterface(GFXDevice& context, const char* name);
    virtual ~VertexDataInterface();

    virtual void draw(const GenericDrawCommand& command, VDIUserData* data) = 0;

    PROPERTY_R(Handle, handle);
    PROPERTY_RW(bool, primitiveRestartRequired, false);

    using VDIPool = ObjectPool<VertexDataInterface, 4096>;
    // We only need this pool in order to get a valid handle to pass around to command buffers instead of using raw pointers
    static VDIPool s_VDIPool;
};

};  // namespace Divide


#endif