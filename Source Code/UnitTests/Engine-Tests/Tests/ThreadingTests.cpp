#include "stdafx.h"

#include "Headers/Defines.h"
#include "Core/Headers/Console.h"
#include "Core/Headers/TaskPool.h"

#include <atomic>

namespace Divide {

TEST(TaskPoolContructionTest)
{
    Console::toggleErrorStream(false);
    TaskPool test;

    // Not enough workers
    bool init = test.init(0);
    CHECK_FALSE(init);

    // Valid
    init = test.init(1);
    CHECK_TRUE(init);

    // Double init
    init = test.init(HARDWARE_THREAD_COUNT());
    CHECK_FALSE(init);
}

TEST(ParallelForTest)
{
    Console::toggleErrorStream(false);

    TaskPool test;
    bool init = test.init(HARDWARE_THREAD_COUNT());
    CHECK_TRUE(init);

    const U32 partitionSize = 4;
    const U32 loopCount = partitionSize * 4 + 2;

    std::atomic_uint loopCounter = 0;
    std::atomic_uint totalCounter = 0;
    auto loop = [&totalCounter, &loopCounter](const Task& parentTask, U32 start, U32 end) {
        ++loopCounter;
        for (U32 i = start; i < end; ++i) {
            ++totalCounter;
        }
    };

    parallel_for(test, loop, loopCount, partitionSize);

    CHECK_EQUAL(loopCounter, 5u);
    CHECK_EQUAL(totalCounter, 18u);
}


TEST(TaskCallbackTest)
{
    TaskPool test;
    bool init = test.init(HARDWARE_THREAD_COUNT());
    CHECK_TRUE(init);

    std::atomic_bool testValue = false;

    auto task = [](const Task& parentTask) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    };

    auto callback = [&testValue]() {
        testValue = true;
    };

    TaskHandle job = CreateTask(test, task, callback);
    job.startTask();

    CHECK_FALSE(testValue);

    job.wait();

    CHECK_FALSE(testValue);

    test.flushCallbackQueue();

    CHECK_TRUE(testValue);
}

namespace {
    class ThreadedTest {
      public:
        ThreadedTest()
        {
            _testValue = false;
        }

        void setTestValue(bool state) {
            _testValue = state;
        }

        bool getTestValue() {
            return _testValue;
        }

        void threadedFunction(const Task& parentTask) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            _testValue = true;
        }
      private:
        std::atomic_bool _testValue;
    };
};

TEST(TaskClassMemberCallbackTest)
{
    TaskPool test;
    bool init = test.init(HARDWARE_THREAD_COUNT());
    CHECK_TRUE(init);

    ThreadedTest testObj;

    TaskHandle job = CreateTask(test,
        DELEGATE_BIND(&ThreadedTest::threadedFunction, &testObj, std::placeholders::_1),
        DELEGATE_BIND(&ThreadedTest::setTestValue, &testObj, false));
    job.startTask();

    CHECK_FALSE(testObj.getTestValue());

    job.wait();

    CHECK_TRUE(testObj.getTestValue());

    test.flushCallbackQueue();

    CHECK_FALSE(testObj.getTestValue());
}

TEST(TaskPriorityTest)
{
    TaskPool test;
    bool init = test.init(HARDWARE_THREAD_COUNT());
    CHECK_TRUE(init);

    U32 callbackValue = 0;
    auto testFunction = [&callbackValue](const Task& parentTask) {
        callbackValue++;
    };

    auto callback = [&callbackValue]() {
        callbackValue++;
    };

    TaskHandle job = CreateTask(test, testFunction, callback);
    job.startTask();
    job.wait();
    CHECK_EQUAL(callbackValue, 1u);
    test.flushCallbackQueue();
    CHECK_EQUAL(callbackValue, 2u);

    job.startTask(TaskPriority::REALTIME);
    job.wait();
    CHECK_EQUAL(callbackValue, 3u);
    test.flushCallbackQueue();
    CHECK_EQUAL(callbackValue, 3u);

    job = CreateTask(test, testFunction, callback);
    job.startTask(TaskPriority::REALTIME);
    job.wait();
    CHECK_EQUAL(callbackValue, 5u);
}

}; //namespace Divide
