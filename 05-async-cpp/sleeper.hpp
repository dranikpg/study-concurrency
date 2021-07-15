#pragma once

#include <chrono>
#include <set>
#include <mutex>
#include <condition_variable>

#include "threadpool.hpp"

class Sleeper {
public:
    Sleeper(ThreadPool* pool);
    ~Sleeper();
    using Closure = std::function<void()>;
    void run(int ms, Closure func);
private:   
    struct TimePoint {
        int id_;
        std::time_t time_point_;
        Closure func_;
        TimePoint(int ms, Closure func);
        bool operator <(const TimePoint& other) const;
        bool operator ==(const TimePoint& other) const;
        static std::atomic<int> counter;
    };
    
    ThreadPool* pool_;
    std::set<TimePoint> queue_;
    std::mutex sync_;
    std::thread worker_;
    std::atomic_bool shutdown_;

    static void worker(Sleeper* sleeper);
};