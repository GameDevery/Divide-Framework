/*
   Copyright (c) 2018 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software
   and associated documentation files (the "Software"), to deal in the Software
   without restriction,
   including without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
   PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#pragma once
#ifndef _TASKS_H_
#define _TASKS_H_

#include "Platform/Threading/Headers/ThreadPool.h"
#include "Platform/Headers/PlatformDefines.h"

namespace Divide {

struct Task;
class TaskPool;

enum class TaskPriority : U8 {
    DONT_CARE = 0,
    REALTIME = 1, //<= not threaded
    COUNT
};

struct alignas(64) Task {
    DELEGATE<void, Task&> _callback;
    TaskPool* _parentPool = nullptr;
    Task* _parent = nullptr;
    U32 _id = 0;
    std::atomic_ushort _unfinishedJobs = 0u;
    bool _runWhileIdle = true;
};

Task& Start(Task& task, TaskPriority prio = TaskPriority::DONT_CARE, DELEGATE<void>&& onCompletionFunction = {});
void Wait(const Task& task);
bool Finished(const Task& task) noexcept;
void TaskYield(const Task& task);

};  // namespace Divide

#endif
