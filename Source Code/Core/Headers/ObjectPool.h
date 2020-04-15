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
#ifndef _OBJECT_POOL_H_
#define _OBJECT_POOL_H_

#include "Platform/Threading/Headers/SharedMutex.h"
#include "Core/TemplateLibraries/Headers/Vector.h"

namespace Divide {

#pragma pack(push, 1)
struct PoolHandle {
    U16 _id = 0u;
    U8  _generation = 0u;

    inline bool operator== (const PoolHandle& val) const noexcept {
        return _generation == val._generation && _id == val._id;
    }

    inline bool operator!= (const PoolHandle& val) const noexcept {
        return _generation != val._generation || _id != val._id;
    }
};
#pragma pack(pop)

template<typename T, size_t N>
class ObjectPool {
public:
    ObjectPool()
    {
        _ids.fill({ 0u });
        _pool.fill(nullptr);
    }

    template<typename... Args>
    PoolHandle allocate(void* mem, Args... args);
    void deallocate(void* mem, PoolHandle handle);

    template<typename... Args>
    PoolHandle allocate(Args... args);
    void deallocate(PoolHandle handle);

    PoolHandle registerExisting(T& object);
    void unregisterExisting(PoolHandle handle);

    T* find(PoolHandle handle) const;

protected:
    mutable SharedMutex _poolLock;
    std::array<PoolHandle, N> _ids;
    std::array<T*, N> _pool;
};

}; //namespace Divide

#endif //_OBJECT_POOL_H_

#include "ObjectPool.inl"