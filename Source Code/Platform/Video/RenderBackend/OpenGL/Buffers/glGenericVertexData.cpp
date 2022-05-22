#include "stdafx.h"

#include "Headers/glGenericVertexData.h"

#include "Platform/Headers/PlatformRuntime.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/Headers/glBufferImpl.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/Headers/glMemoryManager.h"

#include "Utility/Headers/Localization.h"

namespace Divide {

glGenericVertexData::glGenericVertexData(GFXDevice& context, const U32 ringBufferLength, const char* name)
    : GenericVertexData(context, ringBufferLength, name)
{
}

glGenericVertexData::~glGenericVertexData()
{
    reset();
}

void glGenericVertexData::reset() {
    _bufferObjects.clear();
    GLUtil::freeBuffer(_indexBufferHandle);
}

/// Submit a draw command to the GPU using this object and the specified command
void glGenericVertexData::draw(const GenericDrawCommand& command) {
    DIVIDE_ASSERT(GL_API::GetStateTracker()->_primitiveRestartEnabled == primitiveRestartRequired());
    _lockManager.wait(false);

    // Update buffer bindings
    for (const auto& buffer : _bufferObjects) {
        bindBufferInternal(buffer._bindConfig);
    }

    if (!renderIndirect() &&
        command._cmd.primCount == 1u &&
        command._drawCount > 1u)
    {
        rebuildCountAndIndexData(command._drawCount, command._cmd.indexCount, command._cmd.firstIndex, idxBuffer().count);
    }

    if (GL_API::GetStateTracker()->setActiveBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBufferHandle) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }

    // Submit the draw command
    GLUtil::SubmitRenderCommand(command,
                                renderIndirect(),
                                _idxBuffer.smallIndices ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                                _indexInfo._countData.data(),
                                (bufferPtr)_indexInfo._indexOffsetData.data());

    lockBuffersInternal(false);
}

void glGenericVertexData::setIndexBuffer(const IndexBuffer& indices) {
    if (!AreCompatible(_idxBuffer, indices) || indices.count == 0u) {
        GLUtil::freeBuffer(_indexBufferHandle);
    }

    const size_t oldCount = _idxBuffer.count;
    GenericVertexData::setIndexBuffer(indices);
    _idxBuffer.count = std::max(oldCount, _idxBuffer.count);

    if (indices.count > 0u) {
        const size_t elementSize = indices.smallIndices ? sizeof(GLushort) : sizeof(GLuint);

        vector_fast<U16> smallIndicesTemp;
        bufferPtr data = indices.data;

        if (indices.indicesNeedCast) {
            smallIndicesTemp.resize(indices.count);
            const U32* const dataIn = reinterpret_cast<U32*>(data);
            for (size_t i = 0u; i < indices.count; ++i) {
                smallIndicesTemp[i] = to_U16(dataIn[i]);
            }
            data = smallIndicesTemp.data();
        }

        if (_indexBufferHandle == GLUtil::k_invalidObjectID) {
            const GLuint newDataSize = static_cast<GLuint>(indices.count * elementSize);
            _indexBufferSize = std::max(newDataSize, _indexBufferSize);

            assert(indices.offsetCount == 0u);
            GLUtil::createBuffer(_indexBufferSize,
                                 _indexBufferHandle,
                                 _name.empty() ? nullptr : (_name + "_index").c_str());
        }

        const size_t offset = indices.offsetCount * elementSize;
        const size_t range = indices.count * elementSize;
        DIVIDE_ASSERT(offset + range <= _indexBufferSize);

        if (offset == 0u && range == _indexBufferSize) {
            glNamedBufferData(_indexBufferHandle, range, data, indices.dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW);
        } else {
            glInvalidateBufferSubData(_indexBufferHandle, offset, range);
            glNamedBufferSubData(_indexBufferHandle, offset, range, data);
        }
        if (!Runtime::isMainThread()) {
            _lockManager.lock();
        }
    }
}

/// Specify the structure and data of the given buffer
void glGenericVertexData::setBuffer(const SetBufferParams& params) {
    // Make sure we specify buffers in order.
    genericBufferImpl* impl = nullptr;
    for (auto& buffer : _bufferObjects) {
        if (buffer._bindConfig._bufferIdx == params._bindConfig._bufferIdx) {
            impl = &buffer;
            break;
        }
    }    
    if (impl == nullptr) {
        impl = &_bufferObjects.emplace_back();
    }

    const size_t ringSizeFactor = params._useRingBuffer ? queueLength() : 1;
    const size_t bufferSizeInBytes = params._bufferParams._elementCount * params._bufferParams._elementSize;

    BufferImplParams implParams;
    implParams._bufferParams = params._bufferParams;
    implParams._dataSize = bufferSizeInBytes * ringSizeFactor;
    implParams._target = GL_ARRAY_BUFFER;
    implParams._name = _name.empty() ? nullptr : _name.c_str();
    implParams._useChunkAllocation = params._useChunkAllocation;

    const size_t elementStride = params._elementStride == SetBufferParams::INVALID_ELEMENT_STRIDE
                                                    ? params._bufferParams._elementSize
                                                    : params._elementStride;
    impl->_ringSizeFactor = ringSizeFactor;
    impl->_useAutoSyncObjects = params._useAutoSyncObjects;
    impl->_bindConfig = params._bindConfig;
    impl->_elementStride = elementStride;

    bool skipUpdate = false;
    if (impl->_buffer == nullptr || impl->_buffer->params() != implParams) {
        impl->_buffer = eastl::make_unique<glBufferImpl>(_context, implParams, params._initialData);
        for (U32 i = 1u; i < ringSizeFactor; ++i) {
            impl->_buffer->writeOrClearBytes(
                i * bufferSizeInBytes,
                params._initialData.second > 0 ? params._initialData.second  : bufferSizeInBytes, 
                params._initialData.first, 
                params._initialData.first == nullptr,
                true);
        }
        skipUpdate = true;
    }

    if (!skipUpdate) {
        updateBuffer(params._bindConfig._bufferIdx, 0, params._bufferParams._elementCount, params._initialData.first);
    }
}

/// Update the elementCount worth of data contained in the buffer starting from elementCountOffset size offset
void glGenericVertexData::updateBuffer(const U32 buffer,
                                       const U32 elementCountOffset,
                                       const U32 elementCountRange,
                                       const bufferPtr data) {
    genericBufferImpl* impl = nullptr;
    for (auto& bufferImpl : _bufferObjects) {
        if (bufferImpl._bindConfig._bufferIdx == buffer) {
            impl = &bufferImpl;
            break;
        }
    }

    DIVIDE_ASSERT(impl != nullptr && "glGenericVertexData error: set buffer called for invalid buffer index!");

    const BufferParams& bufferParams = impl->_buffer->params()._bufferParams;

    // Calculate the size of the data that needs updating
    const size_t dataCurrentSizeInBytes = elementCountRange * bufferParams._elementSize;
    // Calculate the offset in the buffer in bytes from which to start writing
    size_t offsetInBytes = elementCountOffset * bufferParams._elementSize;

    if (impl->_ringSizeFactor > 1u) {
        offsetInBytes += bufferParams._elementCount * bufferParams._elementSize * queueIndex();
    }

    impl->_buffer->writeOrClearBytes(offsetInBytes, dataCurrentSizeInBytes, data, false);
    Merge(impl->_writtenRange, {offsetInBytes, dataCurrentSizeInBytes});
}

void glGenericVertexData::bindBufferInternal(const SetBufferParams::BufferBindConfig& bindConfig) {
    genericBufferImpl* impl = nullptr;
    for (auto& bufferImpl : _bufferObjects) {
        if (bufferImpl._bindConfig._bufferIdx == bindConfig._bufferIdx) {
            impl = &bufferImpl;
            break;
        }
    }

    if (impl == nullptr) {
        return;
    }

    const BufferParams& bufferParams = impl->_buffer->params()._bufferParams;
    size_t offsetInBytes = impl->_buffer->memoryBlock()._offset;

    if (impl->_ringSizeFactor > 1) {
        offsetInBytes += bufferParams._elementCount * bufferParams._elementSize * queueIndex();
    }

    const GLStateTracker::BindResult ret = 
        GL_API::GetStateTracker()->bindActiveBuffer(bindConfig._bindIdx,
                                                    impl->_buffer->memoryBlock()._bufferHandle,
                                                    offsetInBytes,
                                                    impl->_elementStride);

    if (ret == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }

    impl->_usedAfterWrite = impl->_writtenRange._length > 0u;
}

void glGenericVertexData::insertFencesIfNeeded() {
    lockBuffersInternal(true);
}

void glGenericVertexData::lockBuffersInternal(const bool force) {
    SyncObject* sync = nullptr;
    for (const auto& buffer : _bufferObjects) {
        if ((buffer._useAutoSyncObjects || force) && buffer._usedAfterWrite) {
            sync = glLockManager::CreateSyncObject();
            break;
        }
    }

    if (sync != nullptr) {
        for (auto& buffer : _bufferObjects) {
            if ((buffer._useAutoSyncObjects || force) && buffer._usedAfterWrite) {
                DIVIDE_ASSERT(buffer._writtenRange._length > 0u);

                if (!buffer._buffer->lockByteRange(buffer._writtenRange, sync)) {
                    DIVIDE_UNEXPECTED_CALL();
                }
                buffer._writtenRange = {};
                buffer._usedAfterWrite = false;
            }
        }
    }
}

void glGenericVertexData::rebuildCountAndIndexData(const U32 drawCount, const  U32 indexCount, const U32 firstIndex, const size_t indexBufferSize) {
    if (_lastDrawCount == drawCount && _lastIndexCount == indexCount && _lastFirstIndex == firstIndex) {
        return;
    }

    const size_t idxCountInternal = drawCount * indexBufferSize;
    if (idxCountInternal >= _indexInfo._indexOffsetData.size()) {
        _indexInfo._indexOffsetData.resize(idxCountInternal, _lastFirstIndex);
    }

    if (_lastDrawCount != drawCount) {
        if (drawCount >= _indexInfo._countData.size()) {
            // No need to resize down. Cheap to keep in memory.
            _indexInfo._countData.resize(drawCount, _lastIndexCount);
        }
        _lastDrawCount = drawCount;
    }

    if (_lastIndexCount != indexCount) {
        eastl::fill(begin(_indexInfo._countData), begin(_indexInfo._countData) + drawCount, indexCount);
        _lastIndexCount = indexCount;
    }

    if (_lastFirstIndex != firstIndex) {
        if (idxCountInternal > 0u) {
            eastl::fill(begin(_indexInfo._indexOffsetData), begin(_indexInfo._indexOffsetData) + idxCountInternal, firstIndex);
        }
        _lastFirstIndex = firstIndex;
    }
}

};
