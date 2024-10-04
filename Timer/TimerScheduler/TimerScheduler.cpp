#include <random>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include "TimerScheduler.h"

using std::chrono::microseconds;
using std::chrono::steady_clock;

struct ConstIntervalFunctor
{
    const microseconds constInterval;
    explicit ConstIntervalFunctor(microseconds interval) : constInterval(interval)
    {
        if (interval < microseconds::zero())
        {
            throw std::invalid_argument("TimerScheduler: time interval must be non-negative");
        }
    }
    microseconds operator()() const { return constInterval; }
};

TimerScheduler::TimerScheduler() = default;

TimerScheduler::~TimerScheduler()
{
    shutdown();
}

bool TimerScheduler::start()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (running_)
    {
        return false;
    }

    std::cout << "Starting TimerScheduler with " << functions_.size() << " functions.";
    auto now = steady_clock::now();
    // Reset the next run time. for all functions. this is needed since one can shutdown() and start() again
    for (const auto &f : functions_)
    {
        f->resetNextRunTime(now);
        std::cout << "   - func: " << (f->name.empty() ? "(anon)" : f->name.c_str())
                  << ", period = " << f->intervalDescr
                  << ", delay = " << f->startDelay.count() << "ms" << std::endl;
    }
    std::make_heap(functions_.begin(), functions_.end(), fnCmp_);

    thread_ = std::thread([&]
                          { this->run(); });
    running_ = true;

    return true;
}

bool TimerScheduler::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_)
        {
            return false;
        }

        running_ = false;
        runningCondvar_.notify_one();
    }
    thread_.join();
    return true;
}

void TimerScheduler::addFunction(std::function<void()> &&cb, microseconds interval, std::string nameID, microseconds startDelay)
{
    addFunctionToHeapChecked(std::move(cb), ConstIntervalFunctor(interval), nameID, std::to_string(interval.count()) + "us", startDelay, false /*runOnce*/);
}

void TimerScheduler::addFunctionOnce(std::function<void()> &&cb, std::string nameID, microseconds startDelay)
{
    addFunctionToHeapChecked(std::move(cb), ConstIntervalFunctor(microseconds::zero()), nameID, "once", startDelay, true /*runOnce*/);
}

template <typename IntervalFunc>
void TimerScheduler::addFunctionToHeapChecked(std::function<void()> &&cb, IntervalFunc &&fn, const std::string &nameID, const std::string &intervalDescr, microseconds startDelay, bool runOnce)
{
    if (!cb)
    {
        throw std::invalid_argument("TimerScheduler: Scheduled function must be set");
    }
    if (startDelay < microseconds::zero())
    {
        throw std::invalid_argument("TimerScheduler: start delay must be non-negative");
    }

    std::unique_lock<std::mutex> lock(mutex_);
    auto it = functionsMap_.find(nameID);
    if (it != functionsMap_.end() && it->second->isValid())
    {
        throw std::invalid_argument("TimerScheduler: a function named \"" + nameID + "\" already exists");
    }
    if (currentFunction_ && currentFunction_->name == nameID)
    {
        throw std::invalid_argument("TimerScheduler: a function named \"" + nameID + "\" already exists");
    }

    std::unique_ptr<RepeatFunc> func = std::make_unique<RepeatFunc>(std::move(cb), std::forward<IntervalFunc>(fn), nameID, intervalDescr, startDelay, runOnce);

    assert(lock.mutex() == &mutex_);
    assert(lock.owns_lock());

    functions_.push_back(std::move(func));
    functionsMap_[functions_.back()->name] = functions_.back().get();
    if (running_)
    {
        functions_.back()->resetNextRunTime(steady_clock::now());
        std::push_heap(functions_.begin(), functions_.end(), fnCmp_);

        // Signal the running thread to wake up and see if it needs to change its current scheduling decision.
        runningCondvar_.notify_one();
    }
}

bool TimerScheduler::cancelFunction(std::string nameID)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (currentFunction_ && currentFunction_->name == nameID)
    {
        functionsMap_.erase(currentFunction_->name);
        // This function is currently being run. Clear currentFunction_
        // The running thread will see this and won't reschedule the function.
        currentFunction_ = nullptr;
        cancellingCurrentFunction_ = true;
        return true;
    }
    auto it = functionsMap_.find(nameID);
    if (it != functionsMap_.end() && it->second->isValid())
    {
        functionsMap_.erase(it->second->name);
        it->second->cancel();
        return true;
    }

    return false;
}

bool TimerScheduler::cancelFunctionAndWait(std::string nameID)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (currentFunction_ && currentFunction_->name == nameID)
    {
        functionsMap_.erase(currentFunction_->name);
        // This function is currently being run. Clear currentFunction_
        // The running thread will see this and won't reschedule the function.
        currentFunction_ = nullptr;
        cancellingCurrentFunction_ = true;
        runningCondvar_.wait(lock, [this]()
                             { return !cancellingCurrentFunction_; });
        return true;
    }

    auto it = functionsMap_.find(nameID);
    if (it != functionsMap_.end() && it->second->isValid())
    {
        functionsMap_.erase(it->second->name);
        it->second->cancel();
        return true;
    }

    return false;
}

void TimerScheduler::run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (running_)
    {
        if (functions_.empty())
        {
            runningCondvar_.wait(lock);
            continue;
        }

        const auto now = steady_clock::now();

        /* std::cout << "before running " << std::endl;
        for (auto &func : functions_)
        {
            auto nextRunTime = func->getNextRunTime();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(nextRunTime.time_since_epoch()).count();
            std::cout << func->name << " " << duration << std::endl;
        } */
        std::pop_heap(functions_.begin(), functions_.end(), fnCmp_);
        if (!functions_.back()->isValid())
        {
            functions_.pop_back();
            continue;
        }

        const auto sleepTime = functions_.back()->getNextRunTime() - now;
        if (sleepTime < microseconds::zero())
        {
            runOneFunction(lock, now);
            runningCondvar_.notify_all();
        }
        else
        {
            // Re-add the function to the heap, and wait until we actually need to run it.
            std::push_heap(functions_.begin(), functions_.end(), fnCmp_);
            runningCondvar_.wait_for(lock, sleepTime);
        }
        /* std::cout << "after running " << std::endl;
        for (auto &func : functions_)
        {
            auto nextRunTime = func->getNextRunTime();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(nextRunTime.time_since_epoch()).count();
            std::cout << func->name << " " << duration << std::endl;
        }
        std::cout << std::endl; */
    }
}

void TimerScheduler::runOneFunction(std::unique_lock<std::mutex> &lock, steady_clock::time_point now)
{
    assert(lock.mutex() == &mutex_);
    assert(lock.owns_lock());

    // The function to run will be at the end of functions_ already.
    // Fully remove it from functions_ now.
    // We need to release mutex_ while we invoke this function, and we need to maintain the heap property on functions_ while mutex_ is unlocked.
    auto func = std::move(functions_.back());
    functions_.pop_back();
    if (!func->cb)
    {
        std::cout << func->name << "function has been canceled while waiting" << std::endl;
        return;
    }
    currentFunction_ = func.get();
    if (steady_)
    {
        // This allows scheduler to catch up
        func->setNextRunTimeSteady();
    }
    else
    {
        // Note that we set nextRunTime based on the current time where we started
        // the function call, rather than the time when the function finishes.
        // This ensures that we call the function once every time interval, as
        // opposed to waiting time interval seconds between calls.  (These can be
        // different if the function takes a significant amount of time to run.)
        func->setNextRunTimeStrict(now);
    }

    lock.unlock();

    try
    {
        std::cout << "Now running " << func->name << std::endl;
        func->cb();
    }
    catch (const std::exception &ex)
    {
        std::cout << "Error running the scheduled function <" << func->name << ">: " << ex.what() << std::endl;
    }

    lock.lock();

    if (!currentFunction_)
    {
        // The function was cancelled while we were running it. We shouldn't reschedule it;
        cancellingCurrentFunction_ = false;
        return;
    }
    if (currentFunction_->runOnce)
    {
        // Don't reschedule if the function only needed to run once.
        functionsMap_.erase(currentFunction_->name);
        currentFunction_ = nullptr;
        return;
    }

    // Re-insert the function into our functions_ heap.
    // We only maintain the heap property while running_ is set.  (running_ may have been cleared while we were invoking the user's function.)
    functions_.push_back(std::move(func));

    // Clear currentFunction_
    currentFunction_ = nullptr;

    if (running_)
    {
        std::push_heap(functions_.begin(), functions_.end(), fnCmp_);
    }
}