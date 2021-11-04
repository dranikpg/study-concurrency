#include "stack.hpp"

#include <vector>
#include <iostream>
#include <thread>

struct IntWrapper {
    int v = 0; 
    IntWrapper(int v): v(v) {}
    IntWrapper(const IntWrapper& o) { v = o.v; }
    IntWrapper(IntWrapper&& o) { 
        v = o.v; 
        o.v = -1;
    }
};

std::ostream& operator<<(std::ostream& os, const IntWrapper& w) {
    os << "IW{" << w.v << "}";
    return os;
}

void test() {
    std::vector<std::thread> ts;
    Stack<IntWrapper> stack;
    for (int i = 0; i < 5; i++) {
        ts.emplace_back(std::thread([&] {
            for (int j = 0; j < 10; j++) {
                stack.push(j + 1);
                stack.pop();
            }
        }));
    }
    for (auto& t: ts) {
        t.join();
    }
}

int main() {
    test();
    if (STACK_ALLOC_BALANCE.load() != 0) {
        std::cout << "MEMORY LEAKED: " << STACK_ALLOC_BALANCE.load() << std::endl;
    }
}