#include "stdafx.h"

#include "Platform/Threading/Headers/Task.h"
#include "Headers/TaskPool.h"
#include "Core/Headers/StringHelper.h"
#include "Platform/Headers/PlatformRuntime.h"

namespace Divide {

namespace {
    std::atomic_uint g_taskIDCounter = 0u;
    thread_local Task g_taskAllocator[Config::MAX_POOLED_TASKS];
    thread_local U64  g_allocatedTasks = 0u;
}

TaskPool::TaskPool()
    : GUIDWrapper(),
      _taskCallbacks(Config::MAX_POOLED_TASKS)
{
    _threadCount = 0u;
}

TaskPool::TaskPool(const U32 threadCount, const TaskPoolType poolType, const DELEGATE<void, const std::thread::id&>& onThreadCreate, const stringImpl& workerName)
    : TaskPool()
{
    if (!init(threadCount, poolType, onThreadCreate, workerName)) {
        DIVIDE_UNEXPECTED_CALL();
    }
}

TaskPool::~TaskPool()
{
    shutdown();
}

bool TaskPool::init(const U32 threadCount, const TaskPoolType poolType, const DELEGATE<void, const std::thread::id&>& onThreadCreate, const stringImpl& workerName) {
    if (threadCount == 0 || _poolImpl.init()) {
        return false;
    }

    _threadNamePrefix = workerName;
    _threadCreateCbk = onThreadCreate;
    _workerThreadCount = threadCount;

    switch (poolType) {
        case TaskPoolType::TYPE_LOCKFREE: {
            std::get<1>(_poolImpl._poolImpl) = MemoryManager_NEW ThreadPool<false>(*this, _workerThreadCount);
            return true;
        }
        case TaskPoolType::TYPE_BLOCKING: {
            std::get<0>(_poolImpl._poolImpl) = MemoryManager_NEW ThreadPool<true>(*this, _workerThreadCount);
            return true;
        }
        case TaskPoolType::COUNT: break;
    }

    return false;
}

void TaskPool::shutdown() {
    waitForAllTasks(true, true);
    if (_poolImpl.init()) {
        MemoryManager::SAFE_DELETE(std::get<0>(_poolImpl._poolImpl));
        MemoryManager::SAFE_DELETE(std::get<1>(_poolImpl._poolImpl));
    }
}

void TaskPool::onThreadCreate(const std::thread::id& threadID) {
    const stringImpl threadName = _threadNamePrefix + Util::to_string(_threadCount.fetch_add(1));
    if (USE_OPTICK_PROFILER) {
        OPTICK_START_THREAD(threadName.c_str());
    }

    SetThreadName(threadName.c_str());
    if (_threadCreateCbk) {
        _threadCreateCbk(threadID);
    }
}

void TaskPool::onThreadDestroy(const std::thread::id& threadID) {
    ACKNOWLEDGE_UNUSED(threadID);

    if (USE_OPTICK_PROFILER) {
        OPTICK_STOP_THREAD();
    }
}

bool TaskPool::enqueue(PoolTask&& task, const TaskPriority priority, const U32 taskIndex, const DELEGATE<void>& onCompletionFunction) {
    _runningTaskCount.fetch_add(1);

    if (priority == TaskPriority::REALTIME) {
        WAIT_FOR_CONDITION(task(false));
        if (onCompletionFunction) {
            onCompletionFunction();
        }
        return true;
    }

    if (onCompletionFunction) {
        _taskCallbacks[taskIndex].push_back(onCompletionFunction);
    }

    return _poolImpl.addTask(MOV(task));
}

void TaskPool::runCbkAndClearTask(const U32 taskIdentifier) {
    auto& cbks = _taskCallbacks[taskIdentifier];
    for (auto& cbk : cbks) {
        if (cbk) {
            cbk();
        }
    }
    cbks.resize(0);
}

void TaskPool::flushCallbackQueue() {
    if (USE_OPTICK_PROFILER) {
        OPTICK_EVENT();
    }

    constexpr I32 maxDequeueItems = 10;
    
    std::array<U32, maxDequeueItems> taskIndex = {};
    size_t count;
    do {
        count = _threadedCallbackBuffer.try_dequeue_bulk(std::begin(taskIndex), maxDequeueItems);
        for (size_t i = 0; i < count; ++i) {
            runCbkAndClearTask(taskIndex[i]);
        }
    } while (count > 0);
}

void TaskPool::waitForAllTasks(const bool yield, const bool flushCallbacks) {
    if (!_poolImpl.init()) {
        return;
    }

    bool finished = _workerThreadCount == 0u;
    while (!finished) {
        finished = _runningTaskCount.load() == 0u;
        if (!finished && yield) {
            std::this_thread::yield();
        }
    }

    if (flushCallbacks) {
        flushCallbackQueue();
    }

    _poolImpl.waitAndJoin();
}

void TaskPool::taskCompleted(const U32 taskIndex, const bool hasOnCompletionFunction) {
    if (hasOnCompletionFunction) {
        _threadedCallbackBuffer.enqueue(taskIndex);
    }

    _runningTaskCount.fetch_sub(1);
}

Task* TaskPool::allocateTask(Task* parentTask, const bool allowedInIdle) {
    Task* task = nullptr;
    do {
        Task& crtTask = g_taskAllocator[g_allocatedTasks++ & Config::MAX_POOLED_TASKS - 1u];

        U16 expected = to_U16(0u);
        if (crtTask._unfinishedJobs.compare_exchange_strong(expected, 1u)) {
            task = &crtTask;
        }
    } while (task == nullptr);

    task->_parent = parentTask;
    task->_parentPool = this;
    task->_runWhileIdle = allowedInIdle;
    if (task->_parent != nullptr) {
        task->_parent->_unfinishedJobs.fetch_add(1);
    }
    if (task->_id == 0) {
        task->_id = g_taskIDCounter.fetch_add(1u);
    }

    return task;
}

void TaskPool::threadWaiting() {
    if (Runtime::isMainThread()) {
        flushCallbackQueue();
    } else {
        _poolImpl.threadWaiting();
    }
}

void WaitForAllTasks(TaskPool& pool, const bool yield, const bool flushCallbacks) {
    pool.waitForAllTasks(yield, flushCallbacks);
}

bool TaskPool::PoolHolder::init() const noexcept {
    return _poolImpl.first != nullptr || 
           _poolImpl.second != nullptr;
}

void TaskPool::PoolHolder::waitAndJoin() const {
    if (_poolImpl.first != nullptr) {
        _poolImpl.first->wait();
        _poolImpl.first->join();
        return;
    }

    _poolImpl.second->wait();
    _poolImpl.second->join();
}

void TaskPool::PoolHolder::threadWaiting() const {
    if (_poolImpl.first != nullptr) {
        _poolImpl.first->executeOneTask(false);
    } else {
        _poolImpl.second->executeOneTask(false);
    }
}

bool TaskPool::PoolHolder::addTask(PoolTask&& job) const {
    if (_poolImpl.first != nullptr) {
        return _poolImpl.first->addTask(MOV(job));
    }

    return _poolImpl.second->addTask(MOV(job));
}

void parallel_for(TaskPool& pool, const ParallelForDescriptor& descriptor) {
    if (descriptor._iterCount == 0) {
        return;
    }

    const U32 crtPartitionSize = std::min(descriptor._partitionSize, descriptor._iterCount);
    const U32 partitionCount = descriptor._iterCount / crtPartitionSize;
    const U32 remainder = descriptor._iterCount % crtPartitionSize;
    const U32 adjustedCount = descriptor._useCurrentThread ? partitionCount - 1 : partitionCount;

    std::atomic_uint jobCount = adjustedCount + (remainder > 0 ? 1 : 0);
    const auto& cbk = descriptor._cbk;

    for (U32 i = 0; i < adjustedCount; ++i) {
        const U32 start = i * crtPartitionSize;
        const U32 end = start + crtPartitionSize;
        Task* parallel_job = pool.allocateTask(nullptr, descriptor._allowRunInIdle);
        parallel_job->_callback = [&cbk, &jobCount, start, end](Task& parentTask) {
                                      cbk(&parentTask, start, end);
                                      jobCount.fetch_sub(1);
                                  };
  
        Start(*parallel_job, descriptor._priority);
    }
    if (remainder > 0) {
        const U32 count = descriptor._iterCount;
        Task* parallel_job = pool.allocateTask(nullptr, descriptor._allowRunInIdle);
        parallel_job->_callback = [&cbk, &jobCount, count, remainder](Task& parentTask) {
                                      cbk(&parentTask, count - remainder, count);
                                      jobCount.fetch_sub(1);
                                  };
        Start(*parallel_job, descriptor._priority);
    }

    if (descriptor._useCurrentThread) {
        const U32 start = adjustedCount * crtPartitionSize;
        cbk(nullptr, start, start + crtPartitionSize);
    }

    if (descriptor._waitForFinish) {
        if (descriptor._allowPoolIdle) {
            while (jobCount.load() > 0) {
                pool.threadWaiting();
            }
        } else {
            WAIT_FOR_CONDITION(jobCount.load() == 0u);
        }
    }
}
}
