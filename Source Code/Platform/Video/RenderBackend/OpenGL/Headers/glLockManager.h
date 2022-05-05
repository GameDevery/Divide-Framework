/*Copyright (c) 2018 DIVIDE-Studio
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
#ifndef _GL_LOCK_MANAGER_H_
#define _GL_LOCK_MANAGER_H_

#include "glResources.h"

// https://github.com/nvMcJohn/apitest
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------


namespace Divide {

constexpr U8 g_MaxLockWaitRetries = 5u;

struct BufferRange {
    size_t _startOffset = 0u;
    size_t _length = 0u;
};

inline bool operator==(const BufferRange& lhs, const BufferRange& rhs) noexcept {
    return lhs._startOffset == rhs._startOffset &&
        lhs._length == rhs._length;
}

[[nodiscard]] inline bool Overlaps(const BufferRange& lhs, const BufferRange& rhs) noexcept {
    return lhs._startOffset < (rhs._startOffset + rhs._length) && rhs._startOffset < (lhs._startOffset + lhs._length);
}

// --------------------------------------------------------------------------------------------------------------------
enum class BufferLockState : U8 {
    ACTIVE = 0,
    EXPIRED,
    DELETED,
    ERROR
};

struct SyncObject {
    ~SyncObject();
    void reset();

    Mutex _fenceLock;
    GLsync _fence = nullptr;
    U32 _frameID = 0u;
};

FWD_DECLARE_MANAGED_STRUCT(SyncObject);

struct BufferLock {
    BufferLock() = default;
    explicit BufferLock(const BufferRange range, SyncObject* syncObj) noexcept
        : _range(range), _syncObj(syncObj)
    {
    }

    BufferRange _range{};
    SyncObject* _syncObj = nullptr;
    BufferLockState _state = BufferLockState::ACTIVE;
};

class glLockManager : public GUIDWrapper {
   public:
    using BufferLockPool = eastl::fixed_vector<SyncObject_uptr, 1024, true>;

    static [[nodiscard]] SyncObject* CreateSyncObject(bool isRetry = false);
    static void Clear();

   public:
    virtual ~glLockManager();

    void wait(bool blockClient);
    void lock();

    /// Returns false if we encountered an error
    bool waitForLockedRange(size_t lockBeginBytes, size_t lockLength, bool blockClient, bool quickCheck = false);
    /// Returns false if we encountered an error
    bool lockRange(size_t lockBeginBytes, size_t lockLength, SyncObject* syncObj);

  protected:
    /// Returns true if the sync object was signaled. retryCount is the number of retries it took to wait for the object
    /// if quickCheck is true, we don't retry if the initial check fails 
    static bool Wait(GLsync syncObj, bool blockClient, bool quickCheck, U8& retryCount);

   protected:
     mutable SharedMutex _lock;
     vector<BufferLock> _bufferLocks;
     vector<BufferLock> _swapLocks;

     static BufferLockPool s_bufferLockPool;
};

};  // namespace Divide

#endif  //_GL_LOCK_MANAGER_H_