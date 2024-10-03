#include <iostream>
#include <thread>
#include <chrono> // 适用于多种平台

class TimerCnt
{
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
    std::chrono::duration<float> duration;

public:
    TimerCnt();

    ~TimerCnt();
};