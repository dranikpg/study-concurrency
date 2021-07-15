#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>


class ThreadPool {
public:
    using Closure = std::function<void()>;
    constexpr static size_t kThreadCount = 2;

    ThreadPool();
    ~ThreadPool();

    void run(Closure func, Closure then = Closure());

private:
    struct Task {
        Closure func_;
        Closure then_;
    };

    std::deque<Task> queue_;
    std::vector<std::thread> threads_;
    std::mutex threads_sync_;
    std::condition_variable task_waker_;
    std::atomic_bool shutdown_;

    static void worker(ThreadPool* pool);
};

