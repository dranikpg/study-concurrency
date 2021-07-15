#include <iostream>
#include <chrono>
#include <memory>
#include <coroutine>

#include "threadpool.hpp"
#include "sleeper.hpp"

using namespace std::chrono_literals;
ThreadPool pool;
Sleeper sleeper(&pool);

// Timer futures 

struct Promise {
    struct promise_type {
        Promise get_return_object() {
          return Promise {
            std::coroutine_handle<promise_type>::from_promise(*this)
          };
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}   
        void unhandled_exception() {}
    };
    std::coroutine_handle<promise_type> h_;
    operator std::coroutine_handle<promise_type>() const { return h_; }
};

struct Timer {
    Timer(int ms): ms(ms) {}
    int ms;    
};

struct TimerAwaiter {
    TimerAwaiter(Timer&& t) : timer(t) {}
    constexpr bool await_ready() const noexcept {
        return timer.ms == 0;
    }
    void await_suspend(std::coroutine_handle<> h) {
        sleeper.run(timer.ms, [h]{
            h.resume();
        });
    }
    constexpr int await_resume() const noexcept {
        return timer.ms;
    }
    Timer timer;
};

auto operator co_await(Timer&& t) {
    return TimerAwaiter(std::move(t));
}

// Test

std::atomic<int> counter;

Promise testSleep(int id) {
    std::cout << "  sleep " << id << std::endl;
    co_await Timer(500);
    std::cout << "      step " << id << std::endl;
    co_await Timer(500);
    std::cout << "  wakeup " << id << std::endl;

    counter.fetch_add(1);
}

void testManySleep() {
    std::cout << "== start " << std::endl;
    for (int i = 0; i < 10; i++) testSleep(i);
    std::cout << "== end" << std::endl;
}

int main() {
    testManySleep();


    while (counter.load() != 10) {
        std::this_thread::sleep_for(1s);
    }
}