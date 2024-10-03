#include "TimerCnt.h"

TimerCnt::TimerCnt()
{
    start = std::chrono::high_resolution_clock::now();
}

TimerCnt::~TimerCnt()
{
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    float ms = duration.count() * 1000.0f;
    std::cout << "Timer took " << ms << "ms" << std::endl;
}
