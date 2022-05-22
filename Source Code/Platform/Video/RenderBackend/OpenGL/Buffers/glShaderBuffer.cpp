#include "stdafx.h"

#include "Headers/glShaderBuffer.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/Headers/glBufferImpl.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/glResources.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/GLWrapper.h"

#include "Utility/Headers/Localization.h"

#include <iomanip>

namespace Divide {

glShaderBuffer::glShaderBuffer(GFXDevice& context, const ShaderBufferDescriptor& descriptor)
    : ShaderBuffer(context, descriptor)
{
    _maxSize = _usage == Usage::CONSTANT_BUFFER ? GFXDevice::GetDeviceInformation()._UBOMaxSizeBytes : GFXDevice::GetDeviceInformation()._SSBOMaxSizeBytes;

    const size_t targetElementSize = Util::GetAlignmentCorrected(_params._elementSize, AlignmentRequirement(_usage));
    if (targetElementSize > _params._elementSize) {
        DIVIDE_ASSERT((_params._elementSize * _params._elementCount) % AlignmentRequirement(_usage) == 0u,
            "ERROR: glShaderBuffer - element size and count combo is less than the minimum alignment requirement for current hardware! Pad the element size and or count a bit");
    } else {
        DIVIDE_ASSERT(_params._elementSize == targetElementSize,
                        "ERROR: glShaderBuffer - element size is less than the minimum alignment requirement for current hardware! Pad the element size a bit");
    }
    _alignedBufferSize = _params._elementCount * _params._elementSize;
    _alignedBufferSize = static_cast<ptrdiff_t>(realign_offset(_alignedBufferSize, AlignmentRequirement(_usage)));

    BufferImplParams implParams;
    implParams._bufferParams = _params;
    implParams._target = (_usage == Usage::UNBOUND_BUFFER || _usage == Usage::COMMAND_BUFFER)
                                 ? GL_SHADER_STORAGE_BUFFER
                                 : GL_UNIFORM_BUFFER;

    implParams._dataSize = _alignedBufferSize * queueLength();
    implParams._name = _name.empty() ? nullptr : _name.c_str();
    implParams._useChunkAllocation = _usage != Usage::COMMAND_BUFFER;

    _bufferImpl = eastl::make_unique<glBufferImpl>(context, implParams, descriptor._initialData);

    // Just to avoid issues with reading undefined or zero-initialised memory.
    // This is quite fast so far so worth it for now.
    if (descriptor._separateReadWrite && descriptor._initialData.second > 0) {
        for (U32 i = 1u; i < descriptor._ringBufferLength; ++i) {
            bufferImpl()->writeOrClearBytes(_alignedBufferSize * i, descriptor._initialData.second, descriptor._initialData.first, false, true);
        }
    }
}

BufferLock glShaderBuffer::clearBytes(BufferRange range) {
    DIVIDE_ASSERT(range._length > 0);
    OPTICK_EVENT();

    DIVIDE_ASSERT(range._startOffset == Util::GetAlignmentCorrected((range._startOffset), AlignmentRequirement(_usage)));
    assert(range._startOffset + range._length <= _alignedBufferSize && "glShaderBuffer::UpdateData error: was called with an invalid range (buffer overflow)!");

    range._startOffset += queueWriteIndex() * _alignedBufferSize;

    bufferImpl()->writeOrClearBytes(range._startOffset, range._length, nullptr, true);
    return { this, range };
}

void glShaderBuffer::readBytes(BufferRange range, std::pair<bufferPtr, size_t> outData) const {
    if (range._length > 0) {
        OPTICK_EVENT();
        
        DIVIDE_ASSERT(range._startOffset == Util::GetAlignmentCorrected(range._startOffset, AlignmentRequirement(_usage)));
        range._startOffset += queueReadIndex() * _alignedBufferSize;

        bufferImpl()->readBytes(range._startOffset, range._length, outData);
    }
}

BufferLock glShaderBuffer::writeBytes(BufferRange range, bufferPtr data) {
    DIVIDE_ASSERT(range._length > 0);
    OPTICK_EVENT();

    DIVIDE_ASSERT(range._startOffset == Util::GetAlignmentCorrected(range._startOffset, AlignmentRequirement(_usage)));
    range._startOffset += queueWriteIndex() * _alignedBufferSize;

    bufferImpl()->writeOrClearBytes(range._startOffset, range._length, data, false);
    return { this, range };
}

bool glShaderBuffer::lockByteRange(const BufferRange range, SyncObject* sync) const {
    DIVIDE_ASSERT(sync != nullptr);
    DIVIDE_ASSERT(range._startOffset == Util::GetAlignmentCorrected(range._startOffset, AlignmentRequirement(_usage)));
    return bufferImpl()->lockByteRange(range._startOffset, range._length, sync);
}

bool glShaderBuffer::bindByteRange(const U8 bindIndex, BufferRange range) {
    OPTICK_EVENT();

    GLStateTracker::BindResult result = GLStateTracker::BindResult::FAILED;
    if (bindIndex == to_base(ShaderBufferLocation::CMD_BUFFER)) {
        result = GL_API::GetStateTracker()->setActiveBuffer(GL_DRAW_INDIRECT_BUFFER,
                                                            bufferImpl()->memoryBlock()._bufferHandle);
    } else if (range._length > 0) {
        DIVIDE_ASSERT(to_size(range._length) <= _maxSize && "glShaderBuffer::bindByteRange: attempted to bind a larger shader block than is allowed on the current platform");
        DIVIDE_ASSERT(range._startOffset == Util::GetAlignmentCorrected(range._startOffset, AlignmentRequirement(_usage)));

        const size_t offset = bufferImpl()->memoryBlock()._offset + range._startOffset + (queueReadIndex() * _alignedBufferSize);

        // If we bind the entire buffer, offset == 0u and range == 0u is a hack to bind the entire thing instead of a subrange
        const size_t bindRange = Util::GetAlignmentCorrected((offset == 0u && to_size(range._length) == bufferImpl()->memoryBlock()._size) ? 0u : range._length, AlignmentRequirement(_usage));

        result = GL_API::GetStateTracker()->setActiveBufferIndexRange(bufferImpl()->params()._target,
                                                                      bufferImpl()->memoryBlock()._bufferHandle,
                                                                      bindIndex,
                                                                      offset,
                                                                      bindRange);
    }

    if (result == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }

    return result == GLStateTracker::BindResult::JUST_BOUND;
}

}  // namespace Divide
