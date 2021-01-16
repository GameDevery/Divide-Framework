#include "stdafx.h"

#include "Headers/glBufferImpl.h"
#include "Headers/glBufferLockManager.h"
#include "Headers/glMemoryManager.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/GLWrapper.h"

namespace Divide {
namespace {
    Mutex g_createBufferLock;

    constexpr size_t g_MinZeroDataSize = 1 * 1024; //1Kb
    constexpr I32 g_maxFlushQueueLength = 16;

    vectorEASTL<Byte> g_zeroMemData(g_MinZeroDataSize, Byte{ 0 });

    struct BindConfigEntry {
        U32 _handle = 0;
        size_t _offset = 0;
        size_t _range = 0;
    };

    using BindConfig = std::array<BindConfigEntry, to_base(ShaderBufferLocation::COUNT)>;
    BindConfig g_currentBindConfig;

    bool SetIfDifferentBindRange(const U32 UBOid,
                                 const U32 bindIndex,
                                 const size_t offsetInBytes,
                                 const size_t rangeInBytes) noexcept {

        BindConfigEntry& crtConfig = g_currentBindConfig[bindIndex];

        if (crtConfig._handle != UBOid ||
            crtConfig._offset != offsetInBytes ||
            crtConfig._range != rangeInBytes)
        {
            crtConfig = { UBOid, offsetInBytes, rangeInBytes };
            return true;
        }

        return false;
    }
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

    std::atomic_init(&_flushQueueSize, 0);

    // We can't use persistent mapping with ONCE usage because we use block allocator for memory and it may have been mapped using write bits and we wouldn't know.
    // Since we don't need to keep writing to the buffer, we can just use a regular glBufferData call once and be done with it.
    const bool usePersistentMapping = _params._bufferParams._usePersistentMapping &&
                                      _params._bufferParams._updateUsage != BufferUpdateUsage::GPU_R_GPU_W &&
                                      _params._bufferParams._updateFrequency != BufferUpdateFrequency::ONCE;

    // Initial data may not fill the entire buffer
    const bool needsAdditionalData = _params._bufferParams._initialData.second < _params._dataSize;
    UniqueLock<Mutex> w_lock(g_createBufferLock);
    if (needsAdditionalData) {
        if (_params._dataSize > g_zeroMemData.size()) {
            g_zeroMemData.resize(_params._dataSize, Byte{ 0 });
        }
        std::memset(&g_zeroMemData[_params._bufferParams._initialData.second], 0, _params._dataSize - _params._bufferParams._initialData.second);
        if (_params._bufferParams._initialData.second > 0) {
            std::memcpy(g_zeroMemData.data(), _params._bufferParams._initialData.first, _params._bufferParams._initialData.second);
        }
    }

    // Create all buffers with zero mem and then write the actual data that we have (If we want to initialise all memory)
    if (!usePersistentMapping) {
        _params._bufferParams._sync = false;
        _params._explicitFlush = false;
        const GLenum usage = _params._target == GL_ATOMIC_COUNTER_BUFFER ? GL_STREAM_READ : GetBufferUsage(_params._bufferParams._updateFrequency, _params._bufferParams._updateUsage);
        GLUtil::createAndAllocBuffer(_params._dataSize, 
                                     usage,
                                     _memoryBlock._bufferHandle,
                                     needsAdditionalData ? g_zeroMemData.data() : _params._bufferParams._initialData.first,
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
  
        _memoryBlock = GL_API::getMemoryAllocator().allocate(_params._dataSize, storageMask, accessMask, _params._name, needsAdditionalData ? g_zeroMemData.data() : _params._bufferParams._initialData.first);
        assert(_memoryBlock._ptr != nullptr && _memoryBlock._size == _params._dataSize && "PersistentBuffer::Create error: Can't mapped persistent buffer!");
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
    OPTICK_EVENT();

    if (_params._bufferParams._sync) {
        return _lockManager->lockRange(offsetInBytes, rangeInBytes, frameID);
    }

    return true;
}

bool glBufferImpl::waitByteRange(const size_t offsetInBytes, const size_t rangeInBytes, const bool blockClient) const {
    OPTICK_EVENT();

    if (_params._bufferParams._sync) {
        return _lockManager->waitForLockedRange(offsetInBytes, rangeInBytes, blockClient);
    }

    return true;
}

bool glBufferImpl::bindByteRange(const GLuint bindIndex, const size_t offsetInBytes, const size_t rangeInBytes) {
    assert(_memoryBlock._bufferHandle != 0 && "BufferImpl error: Tried to bind an uninitialized UBO");

    if (_params._explicitFlush) {
        size_t minOffset = std::numeric_limits<size_t>::max();
        size_t maxRange = 0u;

        BufferMapRange range;
        while (_flushQueue.try_dequeue(range)) {
            minOffset = std::min(minOffset, range._offset);
            maxRange = std::max(maxRange, range._range);
            _flushQueueSize.fetch_sub(1);
        }
        if (maxRange > 0u) {
            glFlushMappedNamedBufferRange(_memoryBlock._bufferHandle, minOffset, maxRange);
        }
    }

    bool bound = false;
    if (bindIndex == to_base(ShaderBufferLocation::CMD_BUFFER)) {
        GL_API::getStateTracker().setActiveBuffer(GL_DRAW_INDIRECT_BUFFER, _memoryBlock._bufferHandle);
        bound = true;
    } else if (SetIfDifferentBindRange(_memoryBlock._bufferHandle, bindIndex, offsetInBytes, rangeInBytes)) {
        glBindBufferRange(_params._target, bindIndex, _memoryBlock._bufferHandle, offsetInBytes, rangeInBytes);
        bound = true;
    }

    if (_params._bufferParams._sync) {
        GL_API::RegisterBufferBind({this, offsetInBytes, rangeInBytes}, !_params._bufferParams._syncEndOfCmdBuffer);
    }

    return bound;
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
        void* dst = _memoryBlock._ptr + offsetInBytes;
        if (zeroMem) {
            memset(dst, 0, rangeInBytes);
        } else {
            memcpy(dst, data, rangeInBytes);
        }

        if (_params._explicitFlush) {
            if (_flushQueue.enqueue(BufferMapRange{offsetInBytes, rangeInBytes})) {
                const I32 size = _flushQueueSize.fetch_add(1);
                if (size >= g_maxFlushQueueLength) {
                    BufferMapRange temp;
                    _flushQueue.try_dequeue(temp);
                }
            }
        }
    } else {
        const auto writeBuffer = [&](bufferPtr localData) {
            if (offsetInBytes == 0 && rangeInBytes == _memoryBlock._size) {
                const GLenum usage = _params._target == GL_ATOMIC_COUNTER_BUFFER ? GL_STREAM_READ : GetBufferUsage(_params._bufferParams._updateFrequency, _params._bufferParams._updateUsage);
                glInvalidateBufferData(_memoryBlock._bufferHandle);
                glNamedBufferData(_memoryBlock._bufferHandle, _memoryBlock._size, localData, usage);
            } else {
                glInvalidateBufferSubData(_memoryBlock._bufferHandle, offsetInBytes, rangeInBytes);
                glNamedBufferSubData(_memoryBlock._bufferHandle, offsetInBytes, rangeInBytes, localData);
            }
        };

        if (zeroMem) {
            UniqueLock<Mutex> w_lock(g_createBufferLock);
            if (rangeInBytes - offsetInBytes > g_zeroMemData.size()) {
                g_zeroMemData.resize(rangeInBytes - offsetInBytes, Byte{ 0 });
            }
            writeBuffer(g_zeroMemData.data());
        } else {
            writeBuffer(data);
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
        std::memcpy(data, _memoryBlock._ptr + offsetInBytes, rangeInBytes);
    } else {
        Byte* bufferData = (Byte*)glMapNamedBufferRange(_memoryBlock._bufferHandle, offsetInBytes, rangeInBytes, MapBufferAccessMask::GL_MAP_READ_BIT);
        if (bufferData != nullptr) {
            std::memcpy(data, &bufferData[offsetInBytes], rangeInBytes);
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

void glBufferImpl::CleanMemory() noexcept {
    UniqueLock<Mutex> w_lock(g_createBufferLock);
    const size_t crtSize = g_zeroMemData.size();
    if (crtSize > g_MinZeroDataSize) {
        // Speed things along if we are in the megabyte range. Not really needed, but still helps.
        const U8 reduceFactor = crtSize > g_MinZeroDataSize * 100 ? 4 : 2;
        g_zeroMemData.resize(std::max(g_zeroMemData.size() / reduceFactor, g_MinZeroDataSize));
    }
}

}; //namespace Divide