// reference: https://github.com/facebook/folly/blob/master/folly/experimental/TimerScheduler.h
#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

/**
 * Schedules any number of functions to run at various intervals. E.g.,
 *
 *   TimerScheduler fs;
 *
 *   fs.addFunction([&] { LOG(INFO) << "tick..."; }, seconds(1), "ticker");
 *   fs.addFunction(std::bind(&TestClass::doStuff, this), minutes(5), "stuff");
 *   fs.start();
 *   ........
 *   fs.cancelFunction("ticker");
 *   fs.addFunction([&] { LOG(INFO) << "tock..."; }, minutes(3), "tocker");
 *   ........
 *   fs.shutdown();
 *
 *
 * Note: the class uses only one thread - if you want to use more than one
 *       thread, either use multiple TimerScheduler objects, or check out
 *       ThreadedRepeatingFunctionRunner.h for a much simpler contract of
 *       "run each function periodically in its own thread".
 *
 * start() schedules the functions, while shutdown() terminates further
 * scheduling.
 */

// A type alias for function that is called to determine the time interval for the next scheduled run.
using IntervalDistributionFunc = std::function<std::chrono::microseconds()>;

// A type alias for function that returns the next run time, given the current start time.
using NextRunTimeFunc = std::function<std::chrono::steady_clock::time_point(std::chrono::steady_clock::time_point)>;

struct RepeatFunc
{
    std::function<void()> cb;
    NextRunTimeFunc nextRunTimeFunc;
    std::chrono::steady_clock::time_point nextRunTime;
    std::string name;
    std::chrono::microseconds startDelay;
    std::string intervalDescr;
    bool runOnce;

    RepeatFunc(std::function<void()> &&cback, IntervalDistributionFunc &&intervalFn, const std::string &nameID,
               const std::string &intervalDistDescription, std::chrono::microseconds delay, bool once) : cb(std::move(cback)),
                                                                                                         nextRunTimeFunc(getNextRunTimeFunc(std::move(intervalFn))),
                                                                                                         name(nameID),
                                                                                                         intervalDescr(intervalDistDescription),
                                                                                                         startDelay(delay),
                                                                                                         runOnce(once) {}

    static NextRunTimeFunc getNextRunTimeFunc(IntervalDistributionFunc &&intervalFn)
    {
        return [intervalFn = std::move(intervalFn)](std::chrono::steady_clock::time_point curTime) mutable
        {
            return curTime + intervalFn();
        };
    }

    std::chrono::steady_clock::time_point getNextRunTime() const
    {
        return nextRunTime;
    }
    void setNextRunTimeSteady()
    {
        nextRunTime = nextRunTimeFunc(nextRunTime);
    }
    void setNextRunTimeStrict(std::chrono::steady_clock::time_point curTime)
    {
        nextRunTime = nextRunTimeFunc(curTime);
    }
    void resetNextRunTime(std::chrono::steady_clock::time_point curTime)
    {
        nextRunTime = curTime + startDelay;
    }
    void cancel()
    {
        // Simply reset cb to an empty function.
        cb = {};
    }
    bool isValid() const
    {
        return bool(cb);
    }
};

class TimerScheduler
{
public:
    TimerScheduler();
    ~TimerScheduler();

    /**
     * Starts the scheduler.
     * Returns false if the scheduler was already running.
     */
    bool start();

    /**
     * Stops the TimerScheduler.
     * It may be restarted later by calling start() again.
     *	Returns false if the scheduler was not running.
     */
    bool shutdown();

    /**
     * By default steady is false, meaning schedules may lag behind overtime.
     * This could be due to long running tasks or time drift because of randomness
     * in thread wakeup time.
     * By setting steady to true, TimerScheduler will attempt to catch up.
     * i.e. more like a cronjob
     *
     * NOTE: it's only safe to set this before calling start()
     */
    void setSteady(bool steady) { steady_ = steady; }

    /**
     * Adds a new function to the TimerScheduler.
     * Functions will not be run until start() is called.  When start() is called, each function will be run after its specified startDelay.
     * Functions may also be added after start() has been called, in which case startDelay is still honored.
     * Throws an exception on error.  In particular, each function must have a unique name--two functions cannot be added with the same name.
     */
    void addFunction(std::function<void()> &&cb, std::chrono::microseconds interval, std::string nameID, std::chrono::microseconds startDelay = std::chrono::microseconds(0));

    // Adds a new function to the TimerScheduler to run only once.
    void addFunctionOnce(std::function<void()> &&cb, std::string nameID, std::chrono::microseconds startDelay = std::chrono::microseconds(0));

    /**
     * Cancels the function with the specified name, so it will no longer be run.
     * Returns false if no function exists with the specified name.
     */
    bool cancelFunction(std::string nameID);
    bool cancelFunctionAndWait(std::string nameID);

private:
    struct RunTimeOrder
    {
        bool operator()(const std::unique_ptr<RepeatFunc> &f1, const std::unique_ptr<RepeatFunc> &f2) const
        {
            return f1->getNextRunTime() > f2->getNextRunTime();
        }
    };

    typedef std::vector<std::unique_ptr<RepeatFunc>> FunctionHeap;
    typedef std::unordered_map<std::string, RepeatFunc *> FunctionMap;

    void run();
    void runOneFunction(std::unique_lock<std::mutex> &lock, std::chrono::steady_clock::time_point now);

    template <typename IntervalFunc>
    void addFunctionToHeapChecked(std::function<void()> &&cb, IntervalFunc &&fn, const std::string &nameID,
                                  const std::string &intervalDescr, std::chrono::microseconds startDelay, bool runOnce);

    std::thread thread_;
    std::mutex mutex_;
    bool running_{false};

    FunctionHeap functions_; // This is a heap, ordered by next run time.
    FunctionMap functionsMap_;
    RunTimeOrder fnCmp_;

    // The function currently being invoked by the running thread.This is null when the running thread is idle
    RepeatFunc *currentFunction_{nullptr};

    // Condition variable that is signalled whenever a new function is added or when the TimerScheduler is stopped.
    std::condition_variable runningCondvar_;

    bool steady_{false};
    bool cancellingCurrentFunction_{false};
};