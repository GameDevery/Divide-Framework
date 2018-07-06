#include "Headers/ShaderBuffer.h"

namespace Divide {
    ShaderBuffer::ShaderBuffer(const stringImpl& bufferName,
                               const U32 sizeFactor,
                               bool unbound,
                               bool persistentMapped,
                               BufferUpdateFrequency frequency)
                                                      : RingBuffer(sizeFactor),
                                                        GUIDWrapper(),
                                                        _bufferSize(0),
                                                        _primitiveSize(0),
                                                        _primitiveCount(0),
                                                        _alignmentRequirement(0),
                                                        _frequency(frequency),
                                                        _unbound(unbound),
                                                        _persistentMapped(persistentMapped &&
                                                            !Config::Profile::DISABLE_PERSISTENT_BUFFER)
    {
    }

    ShaderBuffer::~ShaderBuffer()
    {
    }

    void ShaderBuffer::create(U32 primitiveCount, ptrdiff_t primitiveSize) {
        _primitiveCount = primitiveCount;
        _primitiveSize = primitiveSize;
        _bufferSize = primitiveSize * primitiveCount;
        DIVIDE_ASSERT(_bufferSize > 0, "ShaderBuffer::Create error: Invalid buffer size!");
    }

    void ShaderBuffer::setData(const bufferPtr data) {
        DIVIDE_ASSERT(_bufferSize > 0, "ShaderBuffer::SetData error: Invalid buffer size!");
        updateData(0, _primitiveCount, data);
    }

}; //namespace Divide;