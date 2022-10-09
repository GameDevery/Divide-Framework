#include "stdafx.h"

#include "Headers/glBufferImpl.h"
#include "Headers/glMemoryManager.h"

#include "Platform/Headers/PlatformRuntime.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/GLWrapper.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/glLockManager.h"

#include "Core/Headers/StringHelper.h"
#include "Utility/Headers/Localization.h"

namespace Divide {

glBufferImpl::glBufferImpl(GFXDevice& context, const BufferImplParams& params, const std::pair<bufferPtr, size_t>& initialData, const char* name)
    : GUIDWrapper(),
      _params(params),
      _context(context)
{
    assert(_params._bufferParams._updateFrequency != BufferUpdateFrequency::COUNT);

    // Create all buffers with zero mem and then write the actual data that we have (If we want to initialise all memory)
    if (_params._bufferParams._updateUsage == BufferUpdateUsage::GPU_R_GPU_W || _params._bufferParams._updateFrequency == BufferUpdateFrequency::ONCE) {
        GLenum usage = GL_NONE;
        switch (_params._bufferParams._updateFrequency) {
            case BufferUpdateFrequency::ONCE:
                switch (_params._bufferParams._updateUsage) {
                    case BufferUpdateUsage::CPU_W_GPU_R: usage = GL_STATIC_DRAW; break;
                    case BufferUpdateUsage::CPU_R_GPU_W: usage = GL_STATIC_READ; break;
                    case BufferUpdateUsage::GPU_R_GPU_W: usage = GL_STATIC_COPY; break;
                    default: break;
                }
                break;
            case BufferUpdateFrequency::OCASSIONAL:
                switch (_params._bufferParams._updateUsage) {
                    case BufferUpdateUsage::CPU_W_GPU_R: usage = GL_DYNAMIC_DRAW; break;
                    case BufferUpdateUsage::CPU_R_GPU_W: usage = GL_DYNAMIC_READ; break;
                    case BufferUpdateUsage::GPU_R_GPU_W: usage = GL_DYNAMIC_COPY; break;
                    default: break;
                }
                break;
            case BufferUpdateFrequency::OFTEN:
                switch (_params._bufferParams._updateUsage) {
                    case BufferUpdateUsage::CPU_W_GPU_R: usage = GL_STREAM_DRAW; break;
                    case BufferUpdateUsage::CPU_R_GPU_W: usage = GL_STREAM_READ; break;
                    case BufferUpdateUsage::GPU_R_GPU_W: usage = GL_STREAM_COPY; break;
                    default: break;
                }
                break;
            default: break;
        }

        DIVIDE_ASSERT(usage != GL_NONE);

        GLUtil::createAndAllocBuffer(_params._dataSize, 
                                     usage,
                                     _memoryBlock._bufferHandle,
                                     initialData,
                                     name);

        _memoryBlock._offset = 0u;
        _memoryBlock._size = _params._dataSize;
        _memoryBlock._free = false;
    } else {
        const BufferStorageMask storageMask = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT;
        const MapBufferAccessMask accessMask = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT;

        const size_t alignment = params._target == GL_UNIFORM_BUFFER 
                                                ? GFXDevice::GetDeviceInformation()._UBOffsetAlignmentBytes
                                                : _params._target == GL_SHADER_STORAGE_BUFFER
                                                                  ? GFXDevice::GetDeviceInformation()._SSBOffsetAlignmentBytes
                                                                  : GFXDevice::GetDeviceInformation()._VBOffsetAlignmentBytes;

        GLUtil::GLMemory::DeviceAllocator& allocator = GL_API::GetMemoryAllocator(GL_API::GetMemoryTypeForUsage(_params._target));
        _memoryBlock = allocator.allocate(_params._useChunkAllocation,
                                          _params._dataSize,
                                          alignment,
                                          storageMask,
                                          accessMask,
                                          params._target,
                                          name,
                                          initialData);
        assert(_memoryBlock._ptr != nullptr && _memoryBlock._size >= _params._dataSize && "PersistentBuffer::Create error: Can't mapped persistent buffer!");
    }

    if (!Runtime::isMainThread()) {
        _lockManager.lockRange(0u, _params._dataSize, _lockManager.createSyncObject(LockManager::DEFAULT_SYNC_FLAG_SSBO));
    }

    context.getPerformanceMetrics()._bufferVRAMUsage += params._dataSize;
}

glBufferImpl::~glBufferImpl()
{
    if (!_lockManager.waitForLockedRange(0u, _memoryBlock._size)) {
        DIVIDE_UNEXPECTED_CALL();
    }

    if (_memoryBlock._bufferHandle != GLUtil::k_invalidObjectID) {
        if (_memoryBlock._ptr != nullptr) {
            const GLUtil::GLMemory::DeviceAllocator& allocator = GL_API::GetMemoryAllocator(GL_API::GetMemoryTypeForUsage(_params._target));
            allocator.deallocate(_memoryBlock);
        } else {
            GLUtil::freeBuffer(_memoryBlock._bufferHandle);
            GLUtil::freeBuffer(_copyBufferTarget);
        }

        _context.getPerformanceMetrics()._bufferVRAMUsage -= _params._dataSize;
        _context.getPerformanceMetrics()._bufferVRAMUsage -= _copyBufferSize;
    }
}

void glBufferImpl::writeOrClearBytes(const size_t offsetInBytes, const size_t rangeInBytes, const bufferPtr data, const bool firstWrite) {

    assert(rangeInBytes > 0u && offsetInBytes + rangeInBytes <= _memoryBlock._size);

    PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );
    PROFILE_TAG("Mapped", static_cast<bool>(_memoryBlock._ptr != nullptr));
    PROFILE_TAG("Offset", to_U32(offsetInBytes));
    PROFILE_TAG("Range", to_U32(rangeInBytes));

    if (!_lockManager.waitForLockedRange(offsetInBytes, rangeInBytes)) {
        Console::errorfn(Locale::Get(_ID("ERROR_BUFFER_LOCK_MANAGER_WAIT")));
    }

    UniqueLock<Mutex> w_lock(_mapLock);
    if (_memoryBlock._ptr != nullptr) {
        if (data == nullptr) {
            memset(&_memoryBlock._ptr[offsetInBytes], 0, rangeInBytes);
        } else {
            memcpy(&_memoryBlock._ptr[offsetInBytes], data, rangeInBytes);
        }
    } else {
        DIVIDE_ASSERT(data == nullptr || firstWrite, "glBufferImpl: trying to write to a buffer create with BufferUpdateFrequency::ONCE");

        Byte* ptr = (Byte*)glMapNamedBufferRange(_memoryBlock._bufferHandle, _memoryBlock._offset + offsetInBytes, rangeInBytes, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
        if (data == nullptr) {
            memset(ptr, 0, rangeInBytes);
        } else {
            memcpy(ptr, data, rangeInBytes);
        }
        glUnmapNamedBuffer(_memoryBlock._bufferHandle);
    }
}

void glBufferImpl::readBytes(const size_t offsetInBytes, const size_t rangeInBytes, std::pair<bufferPtr, size_t> outData) {
    PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

    if (!_lockManager.waitForLockedRange(offsetInBytes, rangeInBytes)) {
        DIVIDE_UNEXPECTED_CALL();
    }

    UniqueLock<Mutex> w_lock(_mapLock);
    if (_memoryBlock._ptr != nullptr) {
        memcpy(outData.first,
               _memoryBlock._ptr + offsetInBytes,
               std::min(std::min(rangeInBytes, outData.second), _memoryBlock._size));
    } else {
        if (_copyBufferTarget == GLUtil::k_invalidObjectID || _copyBufferSize < rangeInBytes) {
            GLUtil::freeBuffer(_copyBufferTarget);
            _copyBufferSize = rangeInBytes;
            GLUtil::createAndAllocBuffer(_copyBufferSize,
                                         GL_STREAM_READ,
                                         _copyBufferTarget,
                                         {nullptr, 0u},
                                         Util::StringFormat("COPY_BUFFER_%d", _memoryBlock._bufferHandle).c_str());

            _context.getPerformanceMetrics()._bufferVRAMUsage += _copyBufferSize;
        }
        glCopyNamedBufferSubData(_memoryBlock._bufferHandle, _copyBufferTarget, _memoryBlock._offset + offsetInBytes, 0u, rangeInBytes);

        const Byte* bufferData = (Byte*)glMapNamedBufferRange(_copyBufferTarget, 0u, rangeInBytes, MapBufferAccessMask::GL_MAP_READ_BIT);
        assert(bufferData != nullptr);
        memcpy(outData.first, bufferData, std::min(rangeInBytes, outData.second));
        glUnmapNamedBuffer(_copyBufferTarget);
    }
}

}; //namespace Divide