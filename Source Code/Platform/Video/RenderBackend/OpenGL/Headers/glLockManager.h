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
#pragma once
#ifndef _GL_LOCK_MANAGER_H_
#define _GL_LOCK_MANAGER_H_

#include "glResources.h"

namespace Divide {

class glLockManager : public GUIDWrapper {
   public:
    glLockManager() noexcept;
    virtual ~glLockManager();

    void Wait(bool blockClient);
    void Lock(bool flush);

    // returns true if the sync object was signaled. retryCount is the number of retries it took to wait for the object
    // if quickCheck is true, we don't retry if the initial check fails 
    static bool wait(GLsync syncObj, bool blockClient, bool quickCheck, U8& retryCount);

    inline static bool wait(GLsync syncObj, bool blockClient, bool quickCheck = false) {
        U8 retryCount = 0;
        return wait(syncObj, blockClient, quickCheck, retryCount);
    }
   protected:
    SharedMutex _syncMutex;
    GLsync _defaultSync;
};

};  // namespace Divide

#endif  //_GL_LOCK_MANAGER_H_