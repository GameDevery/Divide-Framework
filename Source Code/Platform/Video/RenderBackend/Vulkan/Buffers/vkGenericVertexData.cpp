#include "stdafx.h"

#include "Headers/vkGenericVertexData.h"

#include "Platform/Video/Headers/GenericDrawCommand.h"
#include "Platform/Video/RenderBackend/Vulkan/Headers/VKWrapper.h"

#include "Core/Headers/StringHelper.h"
#include "Utility/Headers/Localization.h"

namespace Divide {
    vkGenericVertexData::vkGenericVertexData(GFXDevice& context, const U32 ringBufferLength, const char* name)
        : GenericVertexData(context, ringBufferLength, name)
    {
    }

    void vkGenericVertexData::reset()
    {
        _bufferObjects.clear();

        LockGuard<SharedMutex> w_lock( _idxBufferLock );
        _idxBuffers.clear();
    }

    void vkGenericVertexData::draw(const GenericDrawCommand& command, VDIUserData* userData) noexcept {
        vkUserData* vkData = static_cast<vkUserData*>(userData);

        for (const auto& buffer : _bufferObjects) 
        
        {
            bindBufferInternal(buffer._bindConfig, *vkData->_cmdBuffer);
        }

        SharedLock<SharedMutex> w_lock( _idxBufferLock );
        const auto& idxBuffer = _idxBuffers[command._bufferFlag];
        if (idxBuffer._buffer != nullptr) {
            vkCmdBindIndexBuffer(*vkData->_cmdBuffer, idxBuffer._buffer->_buffer, 0u, idxBuffer._data.smallIndices ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
        }

        // Submit the draw command
        VKUtil::SubmitRenderCommand( command, *vkData->_cmdBuffer, idxBuffer._buffer != nullptr, renderIndirect() );
    }

    void vkGenericVertexData::bindBufferInternal(const SetBufferParams::BufferBindConfig& bindConfig, VkCommandBuffer& cmdBuffer) {
        GenericBufferImpl* impl = nullptr;
        for (auto& bufferImpl : _bufferObjects) {
            if (bufferImpl._bindConfig._bufferIdx == bindConfig._bufferIdx) {
                impl = &bufferImpl;
                break;
            }
        }

        if (impl == nullptr) {
            return;
        }

        const BufferParams& bufferParams = impl->_buffer->_params;
        VkDeviceSize offsetInBytes = 0u;

        if (impl->_ringSizeFactor > 1) {
            offsetInBytes += bufferParams._elementCount * bufferParams._elementSize * queueIndex();
        }

        vkCmdBindVertexBuffers(cmdBuffer, bindConfig._bindIdx, 1, &impl->_buffer->_buffer, &offsetInBytes);
    }

    BufferLock vkGenericVertexData::setBuffer(const SetBufferParams& params) noexcept
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        DIVIDE_ASSERT( params._bufferParams._flags._usageType != BufferUsageType::COUNT );

        // Make sure we specify buffers in order.
        GenericBufferImpl* impl = nullptr;
        for (auto& buffer : _bufferObjects) {
            if (buffer._bindConfig._bufferIdx == params._bindConfig._bufferIdx) {
                impl = &buffer;
                break;
            }
        }

        if (impl == nullptr)
        {
            impl = &_bufferObjects.emplace_back();
        }

        const size_t ringSizeFactor = params._useRingBuffer ? queueLength() : 1;
        const size_t bufferSizeInBytes = params._bufferParams._elementCount * params._bufferParams._elementSize;
        const size_t dataSize = bufferSizeInBytes * ringSizeFactor;

        const size_t elementStride = params._elementStride == SetBufferParams::INVALID_ELEMENT_STRIDE
                                                            ? params._bufferParams._elementSize
                                                            : params._elementStride;
        impl->_ringSizeFactor = ringSizeFactor;
        impl->_bindConfig = params._bindConfig;
        impl->_elementStride = elementStride;

        if ( impl->_buffer != nullptr && impl->_buffer->_params == params._bufferParams )

        {
            return updateBuffer( params._bindConfig._bufferIdx, 0, params._bufferParams._elementCount, params._initialData.first);
        }

        const string bufferName = _name.empty() ? Util::StringFormat("DVD_GENERAL_VTX_BUFFER_%d", handle()._id) : (_name + "_VTX_BUFFER");
        impl->_buffer = eastl::make_unique<vkAllocatedLockableBuffer>(params._bufferParams,
                                                                      impl->_lockManager.get(),
                                                                      bufferSizeInBytes,
                                                                      ringSizeFactor,
                                                                      params._initialData,
                                                                      bufferName.c_str());
        return BufferLock
        {
            ._range = {0u, dataSize},
            ._type = BufferSyncUsage::CPU_WRITE_TO_GPU_READ,
            ._buffer = impl->_buffer.get()
        };
    }

    BufferLock vkGenericVertexData::setIndexBuffer(const IndexBuffer& indices)
    {
        PROFILE_SCOPE_AUTO( Profiler::Category::Graphics );

        IndexBufferEntry* impl = nullptr;

        LockGuard<SharedMutex> w_lock( _idxBufferLock );
        bool found = false;
        for (auto& idxBuffer : _idxBuffers) {
            if (idxBuffer._data.id == indices.id)
            {
                impl = &idxBuffer;
                found = true;
                break;
            }
        }

        if (!found)
        {
            impl = &_idxBuffers.emplace_back();
        }
        else if ( impl->_buffer != nullptr )
        {
            DIVIDE_ASSERT(impl->_buffer->_buffer != VK_NULL_HANDLE);

            if ( indices.count == 0u || // We don't need indices anymore
                 impl->_data.dynamic != indices.dynamic || // Buffer usage mode changed
                 impl->_data.count < indices.count ) // Buffer not big enough
            {
                if ( !impl->_lockManager->waitForLockedRange( 0, U32_MAX ) )
                {
                    DIVIDE_UNEXPECTED_CALL();
                }
                impl->_buffer.reset();
            }
        }

        if ( indices.count == 0u )
        {
            // That's it. We have a buffer entry but no VK buffer associated with it, so it won't be used
            return {};
        }

        bufferPtr data = indices.data;
        if ( indices.indicesNeedCast )
        {
            impl->_data._smallIndicesTemp.resize( indices.count );
            const U32* const dataIn = reinterpret_cast<U32*>(data);
            for ( size_t i = 0u; i < indices.count; ++i )
            {
                impl->_data._smallIndicesTemp[i] = to_U16( dataIn[i] );
            }
            data = impl->_data._smallIndicesTemp.data();
        }

        const size_t elementSize = indices.smallIndices ? sizeof( U16 ) : sizeof( U32 );
        const size_t range = indices.count * elementSize;

        if ( impl->_buffer == nullptr )
        {
            impl->_bufferSize = range;
            impl->_data = indices;

            BufferParams params{};
            params._flags._updateFrequency = indices.dynamic ? BufferUpdateFrequency::OFTEN : BufferUpdateFrequency::OCASSIONAL;
            params._flags._updateUsage = BufferUpdateUsage::CPU_TO_GPU;
            params._flags._usageType = BufferUsageType::INDEX_BUFFER;
            params._elementSize = elementSize;
            params._elementCount = to_U32(indices.count);

            const std::pair<bufferPtr, size_t> initialData = { data, range };

            const string bufferName = _name.empty() ? Util::StringFormat( "DVD_GENERAL_IDX_BUFFER_%d", handle()._id ) : (_name + "_IDX_BUFFER");
            impl->_buffer = eastl::make_unique<vkAllocatedLockableBuffer>( params, 
                                                                           impl->_lockManager.get(),
                                                                           impl->_bufferSize,
                                                                           1u,
                                                                           initialData,
                                                                           bufferName.c_str());
        }
        else
        {
            DIVIDE_ASSERT( range <= impl->_bufferSize );
            impl->_buffer->writeBytes( { 0u, range }, VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, data );
        }

        impl->_data._smallIndicesTemp.clear();

        BufferLock ret{};
        if (indices.count > 0u )
        {
            ret = 
            {
                ._range = {0u, indices.count},
                ._type = BufferSyncUsage::CPU_WRITE_TO_GPU_READ,
                ._buffer = impl->_buffer.get()
            };
        }

        return ret;
    }

    BufferLock vkGenericVertexData::updateBuffer(U32 buffer,
                                                 U32 elementCountOffset,
                                                 U32 elementCountRange,
                                                 bufferPtr data) noexcept
    {
        GenericBufferImpl* impl = nullptr;
        for (auto& bufferImpl : _bufferObjects) {
            if (bufferImpl._bindConfig._bufferIdx == buffer) {
                impl = &bufferImpl;
                break;
            }
        }

        const BufferParams& bufferParams = impl->_buffer->_params;
        DIVIDE_ASSERT(bufferParams._flags._updateFrequency != BufferUpdateFrequency::ONCE);

        DIVIDE_ASSERT(impl != nullptr, "vkGenericVertexData error: set buffer called for invalid buffer index!");

        // Calculate the size of the data that needs updating
        const size_t dataCurrentSizeInBytes = elementCountRange * bufferParams._elementSize;
        // Calculate the offset in the buffer in bytes from which to start writing
        size_t offsetInBytes = elementCountOffset * bufferParams._elementSize;
        const size_t bufferSizeInBytes = bufferParams._elementCount * bufferParams._elementSize;
        DIVIDE_ASSERT(offsetInBytes + dataCurrentSizeInBytes <= bufferSizeInBytes);

        if (impl->_ringSizeFactor > 1u)
        {
            offsetInBytes += bufferParams._elementCount * bufferParams._elementSize * queueIndex();
        }

        if (impl->_buffer->_isMemoryMappable &&
           !impl->_lockManager->waitForLockedRange(offsetInBytes, dataCurrentSizeInBytes))
        {
            DIVIDE_UNEXPECTED_CALL();
        }

        const BufferRange range = { offsetInBytes , dataCurrentSizeInBytes};
        impl->_buffer->writeBytes(range, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, data);

        BufferLock ret{};
        if ( impl->_buffer->_isMemoryMappable )
        {
            ret = 
            {
                ._range = {0u, dataCurrentSizeInBytes},
                ._type = BufferSyncUsage::CPU_WRITE_TO_GPU_READ,
                ._buffer = impl->_buffer.get()
            };
        }

        return ret;
    }
}; //namespace Divide
