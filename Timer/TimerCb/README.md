# TimerCb

TimerCb是一个多线程定时器，主要是参考了知乎的帖子[https://zhuanlan.zhihu.com/p/650930845](https://zhuanlan.zhihu.com/p/650930845)，附带了相应的测试样例TimerCb_UnitTest.cpp。

## 设计原理

该定时器使用条件变量和互斥锁实现，思路比较简单，程序的主要API为start和stop函数。条件变量的wait等待时间间隔到或定时器停止退出等待，如果定时器没有停止，就执行传入的callback函数。

## 快速上手

我们使用定时器常常想进行跨线程的操作，我们可以这样写：

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
// 测试定时器是否能够正确调用带参数的回调

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