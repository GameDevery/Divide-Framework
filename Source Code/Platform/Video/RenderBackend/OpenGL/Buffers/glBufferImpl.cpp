#include "stdafx.h"

#include "Headers/glBufferImpl.h"
#include "Headers/glBufferLockManager.h"
#include "Headers/glMemoryManager.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/GLWrapper.h"

namespace Divide {
namespace {
    constexpr I32 g_maxFlushQueueLength = 16;
};

glBufferImpl::glBufferImpl(GFXDevice& context, const BufferImplParams& params)
    : glObject(glObjectType::TYPE_BUFFER, context),
      GUIDWrapper(),
      _params(params),
      _context(context),
      _lockManager(eastl::make_unique<glBufferLockManager>())
{
    if (_params._target == GL_ATOMIC_COUNTER_BUFFER) {
        _params._explicitFlush = false;
    }

    assert(_params._bufferParams._updateFrequency != BufferUpdateFrequency::ONCE ||
           (_params._bufferParams._initialData.second > 0 && _params._bufferParams._initialData.first != nullptr) ||
            _params._bufferParams._updateUsage == BufferUpdateUsage::GPU_R_GPU_W);

    // We can't use persistent mapping with ONCE usage because we use block allocator for memory and it may have been mapped using write bits and we wouldn't know.
    // Since we don't need to keep writing to the buffer, we can just use a regular glBufferData call once and be done with it.
    const bool usePersistentMapping = _params._bufferParams._usePersistentMapping &&
                                      _params._bufferParams._updateUsage != BufferUpdateUsage::GPU_R_GPU_W &&
                                      _params._bufferParams._updateFrequency != BufferUpdateFrequency::ONCE;

    // Initial data may not fill the entire buffer
    const bool needsAdditionalData = _params._bufferParams._initialData.second < _params._dataSize;

    // Create all buffers with zero mem and then write the actual data that we have (If we want to initialise all memory)
    if (!usePersistentMapping) {
        _params._bufferParams._sync = false;
        _params._explicitFlush = false;
        const GLenum usage = _params._target == GL_ATOMIC_COUNTER_BUFFER ? GL_STREAM_READ : GetBufferUsage(_params._bufferParams._updateFrequency, _params._bufferParams._updateUsage);
        GLUtil::createAndAllocBuffer(_params._dataSize, 
                                     usage,
                                     _memoryBlock._bufferHandle,
                                     needsAdditionalData ? nullptr : _params._bufferParams._initialData.first,
                                     _params._name);

        _memoryBlock._offset = 0;
        _memoryBlock._size = _params._dataSize;
        _memoryBlock._free = false;
    } else {
        BufferStorageMask storageMask = GL_MAP_PERSISTENT_BIT;
        MapBufferAccessMask accessMask = GL_MAP_PERSISTENT_BIT;

        assert(_params._bufferParams._updateFrequency != BufferUpdateFrequency::COUNT && _params._bufferParams._updateFrequency != BufferUpdateFrequency::ONCE);

        storageMask |= GL_MAP_WRITE_BIT;
        accessMask |= GL_MAP_WRITE_BIT;
        if (_params._explicitFlush) {
            accessMask |= GL_MAP_FLUSH_EXPLICIT_BIT;
        } else {
            storageMask |= GL_MAP_COHERENT_BIT;
            accessMask |= GL_MAP_COHERENT_BIT;
        }
  
        _memoryBlock = GL_API::getMemoryAllocator().allocate(_params._dataSize, storageMask, accessMask, _params._name, needsAdditionalData ? nullptr : _params._bufferParams._initialData.first);
        assert(_memoryBlock._ptr != nullptr && _memoryBlock._size >= _params._dataSize && "PersistentBuffer::Create error: Can't mapped persistent buffer!");
    }

    // In this scenario, we have storage allocated but our contents are undefined so we need to do 2 writes to the buffer:
    if (needsAdditionalData) {
        const BufferUpdateFrequency crtFrequency = _params._bufferParams._updateFrequency;
        _params._bufferParams._updateFrequency = BufferUpdateFrequency::OFTEN;
        //1) Zero the buffer memory
        writeOrClearBytes(0, _params._dataSize, nullptr, true);
        //2) Write the data at the start (if any)
        if (_params._bufferParams._initialData.second > 0) {
            writeOrClearBytes(0, _params._bufferParams._initialData.second, _params._bufferParams._initialData.first, false);
        }
        _params._bufferParams._updateFrequency = crtFrequency;
    }
}

glBufferImpl::~glBufferImpl()
{
    if (_memoryBlock._bufferHandle > 0) {
        if (!waitByteRange(0, _memoryBlock._size, true)) {
            DIVIDE_UNEXPECTED_CALL();
        }

        if (_memoryBlock._ptr != nullptr) {
            GL_API::getMemoryAllocator().deallocate(_memoryBlock);
        } else {
            glInvalidateBufferData(_memoryBlock._bufferHandle);
            GLUtil::freeBuffer(_memoryBlock._bufferHandle, nullptr);
        }
    }
}


bool glBufferImpl::lockByteRange(const size_t offsetInBytes, const size_t rangeInBytes, const U32 frameID) const {
    if (_params._bufferParams._sync) {
        return _lockManager->lockRange(offsetInBytes, rangeInBytes, frameID);
    }

    return true;
}

bool glBufferImpl::waitByteRange(const size_t offsetInBytes, const size_t rangeInBytes, const bool blockClient) const {
    if (_params._bufferParams._sync) {
        return _lockManager->waitForLockedRange(offsetInBytes, rangeInBytes, blockClient);
    }

    return true;
}

bool glBufferImpl::bindByteRange(const GLuint bindIndex, const size_t offsetInBytes, const size_t rangeInBytes) {
    assert(_memoryBlock._bufferHandle != 0 && "BufferImpl error: Tried to bind an uninitialized UBO");

    if (_params._explicitFlush) {
        bool queueEmpty = true;
        {
            SharedLock<SharedMutex> r_lock(_flushQueueLock);
            queueEmpty = _flushQueue.empty();
        }
        if (!queueEmpty) {
            ScopedLock<SharedMutex> w_lock(_flushQueueLock);
            size_t minOffset = std::numeric_limits<size_t>::max();
            size_t maxRange = 0u;

            while (!_flushQueue.empty()) {
                const BufferMapRange& range = _flushQueue.front();
                minOffset = std::min(minOffset, range._offset);
                maxRange = std::max(maxRange, range._range);
                _flushQueue.pop_front();
            }
            if (maxRange > 0u) {
                glFlushMappedNamedBufferRange(_memoryBlock._bufferHandle, minOffset, maxRange);
            }
        }
    }

    bool bound = true;
    if (bindIndex == to_base(ShaderBufferLocation::CMD_BUFFER)) {
        GL_API::getStateTracker().setActiveBuffer(GL_DRAW_INDIRECT_BUFFER, _memoryBlock._bufferHandle);
    } else {
        bound = GL_API::getStateTracker().setActiveBufferIndexRange(_params._target, _memoryBlock._bufferHandle, bindIndex, offsetInBytes, rangeInBytes);
    }

    if (_params._bufferParams._sync) {
        GL_API::RegisterBufferBind({this, offsetInBytes, rangeInBytes}, !_params._bufferParams._syncAtEndOfCmdBuffer);
    }

    return bound;
}

[[nodiscard]] inline bool Overlaps(const BufferMapRange& lhs, const BufferMapRange& rhs) noexcept {
    return lhs._offset < (rhs._offset + rhs._range) && rhs._offset < (lhs._offset + lhs._range);
}

void glBufferImpl::writeOrClearBytes(const size_t offsetInBytes, const size_t rangeInBytes, const bufferPtr data, const bool zeroMem) {

    OPTICK_EVENT();
    OPTICK_TAG("Mapped", static_cast<bool>(_memoryBlock._ptr != nullptr));
    OPTICK_TAG("Offset", to_U32(offsetInBytes));
    OPTICK_TAG("Range", to_U32(rangeInBytes));
    assert(rangeInBytes > 0);
    assert(offsetInBytes + rangeInBytes <= _memoryBlock._size);
    assert(_params._bufferParams._updateFrequency != BufferUpdateFrequency::ONCE);

    if (!waitByteRange(offsetInBytes, rangeInBytes, true)) {
        DIVIDE_UNEXPECTED_CALL();
    }

    if (_memoryBlock._ptr != nullptr) {
        if (zeroMem) {
            memset(&_memoryBlock._ptr[offsetInBytes], 0, rangeInBytes);
        } else {
            memcpy(&_memoryBlock._ptr[offsetInBytes], data, rangeInBytes);
        }

        if (_params._explicitFlush) {
            ScopedLock<SharedMutex> w_lock(_flushQueueLock);
            const BufferMapRange newRange{ offsetInBytes, rangeInBytes };

            bool matched = false;
            for (BufferMapRange& crtRange : _flushQueue) {
                if (Overlaps(crtRange, newRange)) {
                    crtRange._offset = std::min(crtRange._offset, newRange._offset);
                    crtRange._range = std::max(crtRange._range, newRange._range);
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                _flushQueue.push_back(newRange);
                if (_flushQueue.size() >= g_maxFlushQueueLength) {
                    // We completely abandon the first range and pray that it was already signaled
                    _flushQueue.pop_front();
                }
            }
        }
    } else {
        if (zeroMem) {
            constexpr GLfloat zero = 0.f;
            glClearNamedBufferData(_memoryBlock._bufferHandle, GL_R32F, GL_RED, GL_FLOAT, &zero);
        } else {
            if (offsetInBytes == 0u && rangeInBytes == _memoryBlock._size) {
                const GLenum usage = _params._target == GL_ATOMIC_COUNTER_BUFFER ? GL_STREAM_READ : GetBufferUsage(_params._bufferParams._updateFrequency, _params._bufferParams._updateUsage);
                glInvalidateBufferData(_memoryBlock._bufferHandle);
                glNamedBufferData(_memoryBlock._bufferHandle, _memoryBlock._size, data, usage);
            } else {
                glInvalidateBufferSubData(_memoryBlock._bufferHandle, offsetInBytes, rangeInBytes);
                glNamedBufferSubData(_memoryBlock._bufferHandle, offsetInBytes, rangeInBytes, data);
            }
        }
    }
}

void glBufferImpl::readBytes(const size_t offsetInBytes, const size_t rangeInBytes, bufferPtr data) const {

    if (_params._target == GL_ATOMIC_COUNTER_BUFFER) {
        glMemoryBarrier(MemoryBarrierMask::GL_ATOMIC_COUNTER_BARRIER_BIT);
    }

    if (!waitByteRange(offsetInBytes, rangeInBytes, true)) {
        DIVIDE_UNEXPECTED_CALL();
    }

    if (_memoryBlock._ptr != nullptr) {
        if (_params._target != GL_ATOMIC_COUNTER_BUFFER) {
            glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
        }
        memcpy(data, _memoryBlock._ptr + offsetInBytes, rangeInBytes);
    } else {
        const Byte* bufferData = (Byte*)glMapNamedBufferRange(_memoryBlock._bufferHandle, offsetInBytes, rangeInBytes, MapBufferAccessMask::GL_MAP_READ_BIT);
        if (bufferData != nullptr) {
            memcpy(data, &bufferData[offsetInBytes], rangeInBytes);
        }
        glUnmapNamedBuffer(_memoryBlock._bufferHandle);
    }
}

GLenum glBufferImpl::GetBufferUsage(const BufferUpdateFrequency frequency, const BufferUpdateUsage usage) noexcept {
    switch (frequency) {
        case BufferUpdateFrequency::ONCE:
            switch (usage) {
                case BufferUpdateUsage::CPU_W_GPU_R: return GL_STATIC_DRAW;
                case BufferUpdateUsage::CPU_R_GPU_W: return GL_STATIC_READ;
                case BufferUpdateUsage::GPU_R_GPU_W: return GL_STATIC_COPY;
                default: break;
            }
            break;
        case BufferUpdateFrequency::OCASSIONAL:
            switch (usage) {
                case BufferUpdateUsage::CPU_W_GPU_R: return GL_DYNAMIC_DRAW;
                case BufferUpdateUsage::CPU_R_GPU_W: return GL_DYNAMIC_READ;
                case BufferUpdateUsage::GPU_R_GPU_W: return GL_DYNAMIC_COPY;
                default: break;
            }
            break;
        case BufferUpdateFrequency::OFTEN:
            switch (usage) {
                case BufferUpdateUsage::CPU_W_GPU_R: return GL_STREAM_DRAW;
                case BufferUpdateUsage::CPU_R_GPU_W: return GL_STREAM_READ;
                case BufferUpdateUsage::GPU_R_GPU_W: return GL_STREAM_COPY;
                default: break;
            }
            break;
        default: break;
    }

    DIVIDE_UNEXPECTED_CALL();
    return GL_NONE;
}

}; //namespace Divide