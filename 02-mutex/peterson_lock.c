#include "peterson_lock.h"
#include "atomicutils.h"

void testc(){}

peterson_lock_t make_peterson_lock() {
    return (peterson_lock_t){
        .want = {0, 0},
        .victim = 0
    };
}

void lock_peterson(peterson_lock_t* lock, int index) {
    STORE(&lock->want[index], 1);
    STORE(&lock->victim, index);
    while(LOAD(&lock->want[1-index]) && LOAD(&lock->victim) == index) {}
}

void unlock_peterson(peterson_lock_t* lock, int index) {
    STORE(&lock->want[index], 0);
}