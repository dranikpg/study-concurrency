#include "sleeper.hpp"

#include <iostream>
#include <algorithm>

void Sleeper::worker(Sleeper* sleeper) {
    for(;;) {
        {
            const auto tp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::scoped_lock lock(sleeper->sync_);
            while (!sleeper->queue_.empty() && sleeper->queue_.begin()->time_point_ < tp) {
                auto it = sleeper->queue_.begin();
                sleeper->pool_->run(std::move(it->func_));
                sleeper->queue_.erase(it);
            }
            if (sleeper->queue_.empty() && sleeper->shutdown_.load()) return;
        }
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(100ms);
    }
}

Sleeper::Sleeper(ThreadPool* pool): pool_(pool) {
    std::thread t(worker, this);   
    this->worker_ = std::move(t); 
}

Sleeper::~Sleeper() {
    shutdown_.store(true);
    worker_.join();
}

void Sleeper::run(int ms, Closure func) {
    {
        std::scoped_lock lock(sync_);
        queue_.emplace(ms, std::move(func));
    }
}

std::atomic<int> Sleeper::TimePoint::counter;

Sleeper::TimePoint::TimePoint(int ms, Closure func) {
    time_point_ = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() 
                                                        + std::chrono::milliseconds(ms));
    func_ = std::move(func);
    id_ = counter.fetch_add(1);
}

bool Sleeper::TimePoint::operator<(const Sleeper::TimePoint& other) const {
    if (time_point_ == other.time_point_) {
        return id_ < other.id_;
    }
    return time_point_ < other.time_point_;
}

bool Sleeper::TimePoint::operator==(const Sleeper::TimePoint& other) const {
    return this == &other;
}