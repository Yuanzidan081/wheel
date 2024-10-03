
#include "TimerCb.h"

TimerCb::TimerCb() : is_running(false)
{
}

TimerCb::~TimerCb()
{
    stop(); // 注意,析构对象的时候停止定时器,对象不存在了,定时器自然要销毁
}

// 启动定时器 传入一个时间参数和一个用户需要调用的函数,这个函数长什么样可以自己改
void TimerCb::start(int interval_ms, std::function<void()> callback)
{
    if (is_running)
        return; // 如果定时器已经在运行,返回,代表该定时器对象已经在运行,不需要再启动了

    is_running = true; // 设置定时器运行状态
    // 创建定时器线程
    thread_ = std::thread([this, interval_ms, callback]()
                          {
        while (is_running) { //如果定时器是运行状态
            //这里要说一下,下面使用了互斥锁和条件变量, 因为条件变量需要和互斥锁一起使用,大家知道使用条件变量需要
            //三个操作,先抢锁,等待条件,条件满足解锁,程序往下执行,这里的条件变量等待的条件就是interval_ms时间,这个
            //时间单位是ms,也就是在interval_ms的时间内会给条件变量发送一个信号,条件满足,然后解锁,程序往下执行,如果
            //时间没有到,会一直阻塞在这里,
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(interval_ms), [this] { return !is_running; }); //条件变量等待

            if (is_running) {
                callback(); //调用回调
            }
        } });
}

// 结束定时器
void TimerCb::stop()
{
    if (is_running)
    {                       // 如果在运行状态才去结束
        is_running = false; // 改变状态为没有在运行
        // 发送信号唤醒条件变量,不需要等待了,这一步非常重要,暂停定时器的时候一定要先结束线程,避免线程还在运行,
        // 导致崩溃,或继续调用回调函数
        cv_.notify_one();
        if (thread_.joinable())
        {                   // 等待线程结束
            thread_.join(); // 回收线程资源
        }
    }
}
