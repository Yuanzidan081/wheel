#include "TimerCb.h"
#include <gtest/gtest.h>
#include <stdio.h>
using namespace std;

// 测试定时器是否能够正确启动并调用回调
TEST(TimerCbTest, TimerCallsCallback)
{
    TimerCb timer;
    bool callback_called = false;
    timer.start(100, [&]()
                { callback_called = true; });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    timer.stop();

    EXPECT_TRUE(callback_called);
}

// 测试定时器在停止后不再调用回调
TEST(TimerCbTest, TimerStopsCorrectly)
{
    TimerCb timer;

    int call_count = 0;
    timer.start(100, [&]()
                { call_count++; });

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    timer.stop();

    int call_count_after_stop = call_count;

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    // call_count = 2
    printf("call_count = %d\n", call_count);
    EXPECT_EQ(call_count, call_count_after_stop);
}

// 测试定时器在重复调用 start 时不重复启动
TEST(TimerCbTest, TimerDoesNotRestart)
{
    TimerCb timer;

    int call_count = 0;
    timer.start(100, [&]()
                { call_count++; });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // 尝试再次启动
    timer.start(100, [&]()
                { call_count++; });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    timer.stop();
    //  call_count = 3
    printf("call_count = %d\n", call_count);

    EXPECT_LE(call_count, 3); // 确保回调调用次数不超过3次
}

void callbackWithParam(int &counter)
{
    counter++;
}

// 测试定时器是否能够正确调用带参数的回调
TEST(TimerCbTest, TimerCallsCallbackWithParam)
{
    TimerCb timer;
    int call_count = 0;

    // 使用 std::bind 绑定参数
    auto boundCallback = std::bind(callbackWithParam, std::ref(call_count));

    timer.start(100, boundCallback);

    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    timer.stop();
    printf("call_count = %d\n", call_count);
    EXPECT_GT(call_count, 0); // 确保回调至少被调用了一次
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    int status = RUN_ALL_TESTS();
    return 0;
}