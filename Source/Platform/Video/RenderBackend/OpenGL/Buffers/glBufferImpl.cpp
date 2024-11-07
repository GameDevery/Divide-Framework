

#include "Headers/glBufferImpl.h"
#include "Headers/glMemoryManager.h"

#include "Platform/Headers/PlatformRuntime.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/GLWrapper.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/glLockManager.h"

#include "Core/Headers/StringHelper.h"
#include "Utility/Headers/Localization.h"

namespace Divide
{
    glBufferImpl::glBufferImpl( GFXDevice& context, const BufferImplParams& params, const std::pair<const bufferPtr, size_t>& initialData, const std::string_view name )
        : _params( params )
        , _context( context )
    {
        DIVIDE_ASSERT(_params._bufferParams._updateFrequency != BufferUpdateFrequency::COUNT );

        const size_t alignment = _params._target == gl46core::GL_UNIFORM_BUFFER
                                            ? GFXDevice::GetDeviceInformation()._offsetAlignmentBytesUBO
                                            : _params._target == gl46core::GL_SHADER_STORAGE_BUFFER
                                                            ? GFXDevice::GetDeviceInformation()._offsetAlignmentBytesSSBO
                                                            : _params._target == gl46core::GL_ELEMENT_ARRAY_BUFFER
                                                                ? GFXDevice::GetDeviceInformation()._offsetAlignmentBytesIBO
                                                                : GFXDevice::GetDeviceInformation()._offsetAlignmentBytesVBO;

        const U32 flags = _params._target == gl46core::GL_ELEMENT_ARRAY_BUFFER ? to_U32(params._bufferParams._elementSize) : 0u;

        _allocator = &GL_API::GetMemoryAllocator(GL_API::GetMemoryTypeForUsage(_params._target));

        gl46core::BufferStorageMask storageMask = gl46core::GL_MAP_PERSISTENT_BIT | gl46core::GL_MAP_WRITE_BIT | gl46core::GL_MAP_COHERENT_BIT;
        gl46core::BufferAccessMask  accessMask  = gl46core::GL_MAP_PERSISTENT_BIT | gl46core::GL_MAP_WRITE_BIT | gl46core::GL_MAP_COHERENT_BIT;

        switch ( _params._bufferParams._updateFrequency )
        {
            case BufferUpdateFrequency::ONCE:
            {
                storageMask |= gl46core::GL_DYNAMIC_STORAGE_BIT;
            } break;

            default: DebugBreak();
            case BufferUpdateFrequency::OFTEN:
            case BufferUpdateFrequency::OCASSIONAL:
            {

                // We will also need to be careful to not step on our own toes here
                _lockManager = std::make_unique<glLockManager>();
            } break;
        }

        if (_params._bufferParams._hostVisible)
        {
            storageMask |= gl46core::GL_MAP_READ_BIT;
            accessMask  |= gl46core::GL_MAP_READ_BIT;
        }

        // We can't offset bind a command buffer easily in OpenGL, so just use individual allocations for these
        const bool poolAllocations = _params._bufferParams._usageType != BufferUsageType::COMMAND_BUFFER;
        const size_t alignedSize = Util::GetAlignmentCorrected(_params._dataSize, alignment);// Code for the worst case?
        _memoryBlock = _allocator->allocate( poolAllocations,
                                             alignedSize,
                                             storageMask,
                                             accessMask,
                                             flags,
                                             initialData);
        DIVIDE_ASSERT( _memoryBlock._ptr != nullptr && _memoryBlock._size >= _params._dataSize && "PersistentBuffer::Create error: Can't mapped persistent buffer!" );

        if ( !Runtime::isMainThread() && _lockManager != nullptr )
        {
            auto sync = LockManager::CreateSyncObject( RenderAPI::OpenGL, LockManager::DEFAULT_SYNC_FLAG_SSBO );
            if ( !lockRange( { 0u, _params._dataSize }, sync ) )
            {
                DIVIDE_UNEXPECTED_CALL();
            }
        }

        context.getPerformanceMetrics()._bufferVRAMUsage += _memoryBlock._size;
    }

    glBufferImpl::~glBufferImpl()
    {
        if (!waitForLockedRange({0u, _memoryBlock._size})) [[unlikely]]
        {
            DIVIDE_UNEXPECTED_CALL();
        }

        _context.getPerformanceMetrics()._bufferVRAMUsage -= _memoryBlock._size;
        _allocator->deallocate( _memoryBlock );

        _context.getPerformanceMetrics()._bufferVRAMUsage -= _copyBufferSize;
        GLUtil::freeBuffer( _copyBufferTarget );
    }

    BufferLock glBufferImpl::writeOrClearBytes( const size_t offsetInBytes, const size_t rangeInBytes, const bufferPtr data )
    {
        DIVIDE_ASSERT(_memoryBlock._ptr != nullptr);
        DIVIDE_ASSERT( rangeInBytes > 0u && offsetInBytes + rangeInBytes <= _memoryBlock._size );

        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );
        PROFILE_TAG( "Mapped", static_cast<bool>(_memoryBlock._ptr != nullptr) );
        PROFILE_TAG( "Offset", to_U32( offsetInBytes ) );
        PROFILE_TAG( "Range", to_U32( rangeInBytes ) );

        if ( !waitForLockedRange({offsetInBytes, rangeInBytes} ) ) [[unlikely]]
        {
            Console::errorfn( LOCALE_STR( "ERROR_BUFFER_LOCK_MANAGER_WAIT" ) );
        }

        BufferLock ret
        {
            ._type = BufferSyncUsage::CPU_WRITE_TO_GPU_READ,
            ._buffer = this,
        };

        if ( _params._bufferParams._updateFrequency != BufferUpdateFrequency::ONCE) [[likely]]
        {
            LockGuard<Mutex> w_lock(_dataLock);
        
            if ( data == nullptr )
            {
                memset( &_memoryBlock._ptr[offsetInBytes], 0, rangeInBytes );
            }
            else
            {
                memcpy( &_memoryBlock._ptr[offsetInBytes], data, rangeInBytes );
            }
            ret._range = { offsetInBytes, rangeInBytes };
        }
        else
        {
            gl46core::glInvalidateBufferSubData(_memoryBlock._bufferHandle, _memoryBlock._offset + offsetInBytes, rangeInBytes);
            gl46core::glNamedBufferSubData(_memoryBlock._bufferHandle, _memoryBlock._offset + offsetInBytes, rangeInBytes, data == nullptr ? GLUtil::GLMemory::GetZeroData(rangeInBytes) : data);
        }

        return ret;
    }

    void glBufferImpl::readBytes( const size_t offsetInBytes, const size_t rangeInBytes, std::pair<bufferPtr, size_t> outData )
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        DIVIDE_ASSERT(_params._bufferParams._hostVisible);
        DIVIDE_ASSERT(outData.second >= rangeInBytes && _memoryBlock._size >= offsetInBytes + rangeInBytes);

        if ( !waitForLockedRange( {offsetInBytes, rangeInBytes} ) ) [[unlikely]]
        {
            DIVIDE_UNEXPECTED_CALL();
        }

        LockGuard<Mutex> w_lock(_dataLock);
        if (_params._bufferParams._updateFrequency != BufferUpdateFrequency::ONCE) [[likely]]
        {
            DIVIDE_ASSERT(rangeInBytes + offsetInBytes <= _memoryBlock._size );
            memcpy( outData.first, _memoryBlock._ptr + offsetInBytes, rangeInBytes);
        }
        else
        {
            if ( _copyBufferTarget == GL_NULL_HANDLE || _copyBufferSize < rangeInBytes )
            {
                GLUtil::createAndAllocateBuffer(_copyBufferTarget,
                                                Util::StringFormat("COPY_BUFFER_{}", _memoryBlock._bufferHandle).c_str(),
                                                gl46core::GL_DYNAMIC_STORAGE_BIT,
                                                rangeInBytes,
                                                {nullptr, 0u});

                _context.getPerformanceMetrics()._bufferVRAMUsage += (rangeInBytes - _copyBufferSize);
                _copyBufferSize = rangeInBytes;
            }

            gl46core::glCopyNamedBufferSubData( _memoryBlock._bufferHandle, _copyBufferTarget, _memoryBlock._offset + offsetInBytes, 0u, rangeInBytes );

            const Byte* bufferData = (Byte*)gl46core::glMapNamedBufferRange( _copyBufferTarget, 0u, rangeInBytes, gl46core::BufferAccessMask::GL_MAP_READ_BIT );
            assert( bufferData != nullptr );
            memcpy( outData.first, bufferData, rangeInBytes );
            gl46core::glUnmapNamedBuffer( _copyBufferTarget );
        }
    }

    size_t glBufferImpl::getDataOffset() const
    {
        return _memoryBlock._offset;
    }

    size_t glBufferImpl::getDataSize() const
    {
        return _memoryBlock._size;
    }

    bufferPtr glBufferImpl::getDataPtr() const
    {
        return  _memoryBlock._ptr + getDataOffset();
    }

    gl46core::GLuint glBufferImpl::getBufferHandle() const
    {
        return _memoryBlock._bufferHandle;
    }

}; //namespace Divide
