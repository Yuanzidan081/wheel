#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "TimerScheduler.h"

using namespace std::chrono;

// 一个简单的计数器类，用于测试
// 计数器类，用于测试
class Counter
{
public:
    void increment()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
    }

    int count() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    mutable std::mutex mutex_;
    int count_ = 0;
};

// 测试添加和运行单个函数
TEST(TimerSchedulerTest, SingleFunction)
{
    TimerScheduler scheduler;
    Counter counter;

    scheduler.addFunction([&]
                          { counter.increment(); }, milliseconds(100), "increment");

    scheduler.start();
    std::this_thread::sleep_for(milliseconds(350)); // 等待足够时间让函数运行几次
    scheduler.shutdown();
    printf("call_count = %d\n", counter.count());
}
// 测试一次性函数
TEST(TimerSchedulerTest, SingleRunFunction)
{
    TimerScheduler scheduler;
    Counter counter;

    scheduler.addFunctionOnce([&]
                              { counter.increment(); }, "incrementOnce", milliseconds(100));

    scheduler.start();
    std::this_thread::sleep_for(milliseconds(200)); // 等待足够时间让函数运行一次
    scheduler.shutdown();

    printf("call_count = %d\n", counter.count());
}

// 测试取消函数
TEST(TimerSchedulerTest, CancelFunction)
{
    TimerScheduler scheduler;
    Counter counter;

    scheduler.addFunction([&]
                          { counter.increment(); }, milliseconds(100), "increment");

    scheduler.start();
    std::this_thread::sleep_for(milliseconds(150)); // 运行一次
    scheduler.cancelFunction("increment");
    std::this_thread::sleep_for(milliseconds(200)); // 等待，确保函数不再运行
    scheduler.shutdown();

    printf("call_count = %d\n", counter.count());
}

// 测试取消并等待函数
TEST(TimerSchedulerTest, CancelFunctionAndWait)
{
    TimerScheduler scheduler;
    Counter counter;

    scheduler.addFunction([&]
                          { counter.increment(); }, milliseconds(100), "increment");

    scheduler.start();
    std::this_thread::sleep_for(milliseconds(150)); // 运行一次
    scheduler.cancelFunctionAndWait("increment");
    std::this_thread::sleep_for(milliseconds(200)); // 等待，确保函数不再运行
    scheduler.shutdown();

    printf("call_count = %d\n", counter.count());
}

// 测试多个函数在堆里执行
TEST(TimerSchedulerTest, MultipleFunctions)
{
    TimerScheduler scheduler;
    Counter counter1, counter2, counter3;

    scheduler.addFunction([&]
                          { counter1.increment(); }, milliseconds(100), "increment1", milliseconds(50));
    scheduler.addFunction([&]
                          { counter2.increment(); }, milliseconds(200), "increment2", milliseconds(100));
    scheduler.addFunction([&]
                          { counter3.increment(); }, milliseconds(150), "increment3", milliseconds(150));

    scheduler.start();
    std::this_thread::sleep_for(milliseconds(500)); // 等待足够时间让函数运行几次
    scheduler.addFunction([&]
                          { counter3.increment(); }, milliseconds(200), "increment4", milliseconds(150));
    std::this_thread::sleep_for(milliseconds(500)); // 等待足够时间让函数运行几次

    scheduler.shutdown();
    printf("call_count = %d\n", counter1.count());
    printf("call_count = %d\n", counter2.count());
    printf("call_count = %d\n", counter3.count());
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    int status = RUN_ALL_TESTS();
    return 0;
}