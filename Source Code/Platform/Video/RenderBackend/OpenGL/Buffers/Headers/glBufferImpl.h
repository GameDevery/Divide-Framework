/*
Copyright (c) 2018 DIVIDE-Studio
Copyright (c) 2009 Ionut Cava

This file is part of DIVIDE Framework.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software
and associated documentation files (the "Software"), to deal in the Software
without restriction,
including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#pragma once
#ifndef _GL_BUFFER_IMPL_H_
#define _GL_BUFFER_IMPL_H_

#include "Platform/Video/RenderBackend/OpenGL/Headers/glResources.h"
namespace Divide {

struct BufferImplParams {
    GLenum _target = GL_NONE;

    /// Desired buffer size in bytes
    size_t _dataSize = 0;

    /// Buffer primitive size in bytes
    size_t _elementSize = 0;
    bool _explicitFlush = true;
    bool _unsynced = true;
    const char* _name = nullptr;
    std::pair<Byte*, size_t> _initialData = { nullptr, 0 };
    BufferStorageType _storageType = BufferStorageType::AUTO;
    BufferUpdateFrequency _frequency = BufferUpdateFrequency::ONCE;
    BufferUpdateUsage _updateUsage = BufferUpdateUsage::CPU_W_GPU_R;
};

class glBufferImpl;
struct BufferLockEntry {
    glBufferImpl* _buffer = nullptr;
    size_t _offset = 0;
    size_t _length = 0;
};

struct BufferMapRange
{
    size_t _offset = 0;
    size_t _range = 0;
};

class glBufferLockManager;
class glBufferImpl final : public glObject, public GUIDWrapper {
public:
    explicit glBufferImpl(GFXDevice& context, const BufferImplParams& params);
    virtual ~glBufferImpl();

    [[nodiscard]] bool bindRange(GLuint bindIndex, size_t offsetInBytes, size_t rangeInBytes);

    // Returns false if we encounter an error
    [[nodiscard]] bool lockRange(size_t offsetInBytes, size_t rangeInBytes, U32 frameID) const;

    // Returns false if we encounter an error
    [[nodiscard]] bool waitRange(size_t offsetInBytes, size_t rangeInBytes, bool blockClient) const;

    void writeData(size_t offsetInBytes, size_t rangeInBytes, const Byte* data);
    void readData(size_t offsetInBytes, size_t rangeInBytes, Byte* data) const;
    void zeroMem(size_t offsetInBytes, size_t rangeInBytes);

public:
    static GLenum GetBufferUsage(BufferUpdateFrequency frequency, BufferUpdateUsage usage) noexcept;
    static void CleanMemory() noexcept;

public:
    PROPERTY_R(GLuint, bufferID, 0);
    PROPERTY_R(size_t, elementSize, 0);
    PROPERTY_R(size_t, alignedSize, 0);
    PROPERTY_R(GLenum, target, GL_NONE);
    PROPERTY_R(bool, unsynced, false);
    PROPERTY_R(bool, useExplicitFlush, false);
    PROPERTY_R(BufferUpdateFrequency, updateFrequency, BufferUpdateFrequency::COUNT);
    PROPERTY_R(BufferUpdateUsage, updateUsage, BufferUpdateUsage::COUNT);
    PROPERTY_R(GLenum, usage, GL_NONE);

protected:
    void writeOrClearData(size_t offsetInBytes, size_t rangeInBytes, const Byte* data, bool zeroMem);

protected:
    GFXDevice& _context;

    std::atomic_int _flushQueueSize;
    Byte*  _mappedBuffer = nullptr;
    glBufferLockManager* _lockManager = nullptr;
    moodycamel::BlockingConcurrentQueue<BufferMapRange> _flushQueue;
};
}; //namespace Divide

#endif //_GL_BUFFER_IMPL_H_