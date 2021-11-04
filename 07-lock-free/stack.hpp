#include <atomic>
#include <array>
#include <optional>
#include <functional>
#include <thread>

#include <iostream>

std::atomic<int> STACK_ALLOC_BALANCE = 0;

namespace {

constexpr int HAZARD_N = 10;
constexpr int HAZARD_R = 6;
constexpr int HAZARD_T = 5;

struct hazard_ptr {
    std::atomic<void*> memory_ = nullptr;
    std::function<void(void*)> deleter_;
    void clear() {
        deleter_(memory_.load());
        memory_.store(nullptr);
    }
};

struct dummy_hazard_context {
    dummy_hazard_context() {
        for (int i = 0; i < HAZARD_T; i++) ids_[i] = std::thread::id();
        for (int t = 0; t < HAZARD_T; t++) {
            for (int i = 0; i < HAZARD_N; i++) safe_[t][i] = nullptr;
            for (int i = 0; i < HAZARD_R; i++) reclaim_[t][i].memory_ = nullptr;
        }
    }
    ~dummy_hazard_context() {
        for (int i = 0; i < HAZARD_T; i++) {
            if (ids_[i] != std::thread::id()) {
                hazard_scan(i);
            }
        }
    }
    int thread_id(std::thread::id id) {
        for (int i = 0; i < HAZARD_T; i++) {
            if (ids_[i].load() == id) return i;
        }
        // no id - create id
        for (int i = 0; i < HAZARD_T; i++) {
            std::thread::id empty_id;
            if (ids_[i].compare_exchange_strong(empty_id, id)) {
                return i;
            }
        }
        std::cout << "##failed to store thread_id" << std::endl;
        std::exit(-1); // failed to store thread id
    }
    void hazard_scan(int tid) {
        // std::cout << "scanning! " << std::endl;
        for (int di = 0; di < HAZARD_R; di++) {
            void* needle = reclaim_[tid][di].memory_.load();
            if (needle == nullptr) continue;
            for (int otid = 0; otid < HAZARD_T; otid++) {
                for (int i = 0; i < HAZARD_N; i++) {
                    if (safe_[otid][i].load() == needle) goto SKIP;
                }
            }
            // found something to delete - let's delete
            reclaim_[tid][di].clear();
            SKIP:;
        }
    }
    template<typename T>
    void hazard_delete(T* ptr) {
        if (ptr == nullptr) return;
        void* mem = (void*) ptr;
        int tid = thread_id(std::this_thread::get_id());
        for (int i = 0; i < HAZARD_R; i++) {
            void* nullptr_v = nullptr;
            if (reclaim_[tid][i].memory_.compare_exchange_strong(nullptr_v, mem)) {
                reclaim_[tid][i].deleter_ = create_deleter(ptr);
                return;
            }
        }
        hazard_scan(tid); // failed to delete -> scan
        hazard_delete(std::move(ptr)); // try again
    }
    template<typename T, typename D = std::default_delete<T>>
    std::function<void(void*)> create_deleter(T* ptr) {
        return [](void* mem){
            D{}((T*) mem);
        };
    }
    void hazard_safe(void* mem) {
        if (mem == nullptr) return;
        int tid = thread_id(std::this_thread::get_id());
        for (int i = 0; i < HAZARD_N; i++) {
            if (safe_[tid][i].load() == mem) return;
        }
        for (int i = 0; i < HAZARD_N; i++) {
            void* nullptr_v = nullptr;
            if (safe_[tid][i].compare_exchange_strong(nullptr_v, mem)) {
                return;
            }
        }
        std::cout << "##failed to safe" << std::endl;
        std::exit(-1);
    }
    void hazard_unsafe(void* mem) {
        if (mem == nullptr) return;
        int tid = thread_id(std::this_thread::get_id());
        for (int i = 0; i < HAZARD_N; i++) {
            void* mem_v = mem;
            if (safe_[tid][i].compare_exchange_strong(mem_v, nullptr)) {
                return;
            }
        }
    }
    std::array<std::atomic<std::thread::id>, HAZARD_T> ids_;
    std::atomic<void*> safe_[HAZARD_T][HAZARD_N];
    hazard_ptr reclaim_[HAZARD_T][HAZARD_R];
};


template<typename T>
struct Node  {
    T value_;
    Node<T>* next_;
    Node(T&& value): value_(std::move(value)), next_(nullptr) {
        STACK_ALLOC_BALANCE.fetch_add(1);
    }
    ~Node() {
        STACK_ALLOC_BALANCE.fetch_sub(1);
    }
};

};

template<typename T>
class Stack {
public:
    void push(T&& value) {
        auto node = new Node<T>(std::move(value));
        node->next_ = head_.load();
        context_.hazard_safe(node->next_);
        Node<T>* real_v = nullptr;
        while (!head_.compare_exchange_weak(real_v, node)) {
            if (real_v != node->next_) {
                context_.hazard_unsafe(node->next_);
                node->next_ = real_v;
                context_.hazard_safe(node->next_);
            }
            __builtin_ia32_pause();
        }
        context_.hazard_unsafe(real_v);
    }
    std::optional<T> pop() {
        while (true) {
            Node<T>* top = head_.load();
            if (top == nullptr) return std::nullopt;
            context_.hazard_safe(top);
            if (head_.compare_exchange_strong(top, top->next_)) {
                T value = std::move(top->value_);
                context_.hazard_delete(top);
                context_.hazard_unsafe(top);
                return value;
            }
        }
    }
    ~Stack() {
        Node<T>* cur = head_.load();
        while (cur != nullptr) {
            Node<T>* next = cur->next_;
            delete cur;
            cur = next;
        }
    }
private:
    dummy_hazard_context context_;
    std::atomic<Node<T>*> head_ = nullptr;
};