#include "threadpool.hpp"

#include <iostream>

void ThreadPool::worker(ThreadPool* pool) {
    for(;;) {
        std::unique_lock lk(pool->threads_sync_);
        pool->task_waker_.wait(lk, [pool]{
            return !pool->queue_.empty() || pool->shutdown_.load();
        });
        if (pool->queue_.empty() && pool->shutdown_.load()) {
            lk.unlock();
            return;
        }
        Task task = std::move(pool->queue_.front());
        pool->queue_.pop_front();
        lk.unlock();

        task.func_();
        if (task.then_) task.then_();
    }
}

ThreadPool::ThreadPool() {
    for (int i = 0; i < kThreadCount; i++) {
        std::thread t(worker, this);
        threads_.push_back(std::move(t));
    }
}

ThreadPool::~ThreadPool() {
    shutdown_.store(true);
    task_waker_.notify_all();
    for (auto& t: threads_) {
        t.join();
    }
}

void ThreadPool::run(Closure func, Closure then){
    {
        std::scoped_lock lock(threads_sync_);
        if (then) {
            queue_.push_back({std::move(func), std::move(then)});
        } else {
            queue_.push_back({std::move(func), Closure()});
        }
    }
    task_waker_.notify_one();
}