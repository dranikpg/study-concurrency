#include "futex_lock.h"
#include "atomicutils.h"

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>


futex_lock_t make_futex_lock() {
    return (futex_lock_t){0};
}

void lock_futex_lock(futex_lock_t* lock) {
    int expected = 0;
    while (!__atomic_compare_exchange_n(&lock->atom, &expected, 1, 
            false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        syscall(SYS_futex, &lock->atom, FUTEX_WAIT, 1, 0, 0, 0);
        expected = 0;
    }
}

void unlock_futex_lock(futex_lock_t* lock) {
    STORE(&lock->atom, 0);
    syscall(SYS_futex, &lock->atom, FUTEX_WAKE, 1, 0, 0, 0);
}