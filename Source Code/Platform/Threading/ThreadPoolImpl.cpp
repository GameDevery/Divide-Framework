#include "stdafx.h"

#include "Headers/ThreadPool.h"

namespace Divide {

// --------------------------- POOL TASK -------------------------------//
PoolTask::PoolTask(std::function<void()> task)
    : PoolTask(0, task)
{
}

PoolTask::PoolTask(I32 priority, std::function<void()> task)
    : _priority(priority),
    _task(task)
{
}

void PoolTask::operator()() {
    if (_task) {
        _task();
    }
}

// --------------------------- THREAD POOL -------------------------------//
ThreadPool::ThreadPool(I32 numThreads)
    : _numThreads(numThreads)
{
}

ThreadPool::~ThreadPool()
{
}

I32 ThreadPool::numThreads() const {
    return _numThreads;
}


// --------------------------- THREAD POOL C++11-------------------------------//
ThreadPoolC11::ThreadPoolC11(I32 numThreads)
    : ThreadPool(numThreads),
      _pool(numThreads)
{

}

ThreadPoolC11::~ThreadPoolC11()
{
    stopAll();
    waitAll();
}

bool ThreadPoolC11::enqueue(const PoolTask& task) {
    _pool.push([task](I32 id) { task._task(); });
    return true;
}

void ThreadPoolC11::stopAll() {
    _pool.clear_queue();
}

void ThreadPoolC11::waitAll() {
    while (_pool.n_idle() != _pool.size()) {
        std::this_thread::yield();
    }
}

// --------------------------- THREAD POOL Boost Prio-------------------------------//
ThreadPoolBoostPrio::ThreadPoolBoostPrio(I32 numThreads)
    : ThreadPool(numThreads),
      _pool(numThreads)
{

}

ThreadPoolBoostPrio::~ThreadPoolBoostPrio()
{
    stopAll();
    waitAll();
}

bool ThreadPoolBoostPrio::enqueue(const PoolTask& task) {
    const I32 failCountLimit = 5;
    I32 failCount = 0;
    while (!_pool.schedule(
        boost::threadpool::prio_task_func(task._priority, task._task))) {
        if (++failCount > failCountLimit) {
            return false;
        }
    }

    return true;
}

void ThreadPoolBoostPrio::stopAll() {
    _pool.clear();
}

void ThreadPoolBoostPrio::waitAll() {
    _pool.wait();
}


// --------------------------- THREAD POOL Boost FIFO-------------------------------//
ThreadPoolBoostFifo::ThreadPoolBoostFifo(I32 numThreads)
    : ThreadPool(numThreads),
    _pool(numThreads)
{

}

ThreadPoolBoostFifo::~ThreadPoolBoostFifo()
{
    stopAll();
    waitAll();
}

bool ThreadPoolBoostFifo::enqueue(const PoolTask& task) {
    const I32 failCountLimit = 5;
    I32 failCount = 0;
    while (!_pool.schedule(
        boost::threadpool::task_func(task._task))) {
        if (++failCount > failCountLimit) {
            return false;
        }
    }

    return true;
}

void ThreadPoolBoostFifo::stopAll() {
    _pool.clear();
}

void ThreadPoolBoostFifo::waitAll() {
    _pool.wait();
}
}; //namespace Divide
