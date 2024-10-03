#pragma once
#include <iostream>
#include <chrono>
#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
/**************************************
From 只讲干货的攻城狮 :2023/08/18
TimerCb: c++实现一个定时器回调类
//https://zhuanlan.zhihu.com/p/650930845
start用来启动定时器
stop 用来终止定时器
***********************************************/

class TimerCb
{
public:
    TimerCb();
    ~TimerCb();
    void start(int interval_ms, std::function<void()> callback); // 启动定时器
    void stop();                                                 // 停止定时器
private:
    std::atomic<bool> is_running; // 是否在运行
    std::thread thread_;          // 定时器线程
    std::mutex mutex_;            // 互斥锁
    std::condition_variable cv_;  // 条件变量
};
