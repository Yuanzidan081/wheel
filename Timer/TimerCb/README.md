# TimerCb

TimerCb是一个多线程定时器，主要是参考了知乎的帖子[https://zhuanlan.zhihu.com/p/650930845](https://zhuanlan.zhihu.com/p/650930845)，附带了相应的单元测试TimerCb_UnitTest.cpp。

## 设计原理

该定时器使用条件变量和互斥锁实现，思路比较简单，程序的主要API为start和stop函数。

在start函数中，会检查定时器是否在running，如果在running直接返回，在start内会开辟一个线程执行while循环，当时间间隔（作为一个参数传入start函数）还没有到或定时器还在running，条件变量wait等待，直到时间间隔到或定时器停止退出等待，如果定时器还在running，就执行传入的callback函数。

```cpp
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
```
在stop函数中，会检查定时器是否还在running，如果在running，首先改变状态为停止(is_running设置为false)，然后条件变量通知可能在wait的线程，让它退出start函数里线程的运行，最后使用join等待线程结束回收资源。

```cpp
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
```

## 快速上手

在测试样例里我定义了一个变量call_count，然后以lambda表达式的方式传入到timer的start的callback的参数中实现在timer的线程里进行call_count自增。

```cpp
int main()
{
    TimerCb timer;

    int call_count = 0;
    timer.start(100, [&]()
                { call_count++; });

    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    timer.stop();
    printf("call_count = %d\n", call_count);//  call_count = 3
}
```

由于程序的call_count是通过引用捕获进入lambda表达式的，所以便可以跨线程修改call_count，我们设计定时器的计时的间隔是100ms，模拟程序等待350ms，这里的call_count最后为3，即增加了三次。

如果不使用lambda表达式，我们也可以使用bind来绑定参数（这里使用了std::ref函数将变量转换为引用类型）：

```cpp
void callbackWithParam(int &counter)
{
    counter++;
}
int main()
{
    TimerCb timer;
    int call_count = 0;

    // 使用 std::bind 绑定参数
    auto boundCallback = std::bind(callbackWithParam, std::ref(call_count));

    timer.start(100, boundCallback);

    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    timer.stop();
    printf("call_count = %d\n", call_count);
}
```