#include "semaphore_lock.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <pthread.h>

#include <unistd.h>
#include <sys/time.h>

#include "peterson_lock.h"
#include "semaphore_lock.h"
#include "futex_lock.h"

const int TEST_COUNT = 200;
const int LOOP_COUNT = 500;

#define SLEEP 100

int state = 0;
int counter = 0;

void crit_1() {
    if (state == 0) counter += 5;
    else counter -= 5;
#ifdef SLEEP
    usleep(SLEEP);
#endif
}

void crit_2() {
    if (state == 0) state = 1;
    else state = 0;
}

// ============ LOCK IMPLEMENTATIONS ======================

// ================ NO LOCK ===============================

void* no_lock(void* args) {
    for (int i = 0; i < LOOP_COUNT; i++) {
        crit_1();
        crit_2();
    }
}

// =============== PETERSON LOCK ==========================

#ifdef peterson

typedef struct  {
    peterson_lock_t* lock;
    int index;
} peterson_args;

int ga = 0;
void* peterson_lock(void* args_raw) {
    peterson_args* args = args_raw;
    peterson_lock_t* lock = args->lock;
    int index = args->index;
    for (int i = 0; i < LOOP_COUNT; i++) {
        lock_peterson(lock, index);
        crit_1();
        crit_2();
        unlock_peterson(lock, index);
    }
}

#endif

// =============== SEMAPHORE LOCK =========================

#ifdef semaphore

void* semaphore_lock(void* args_raw) {
    semaphore_lock_t* lock = args_raw;
    for (int i = 0; i < LOOP_COUNT; i++) {
        lock_semaphore(lock);
        crit_1();
        crit_2();
        unlock_semaphore(lock);
    }
}

#endif

// =============== FUTEX LOCK =============================

#ifdef futex

void* futex_lock(void* args_raw) {
    futex_lock_t* lock = args_raw;
    for (int i = 0; i < LOOP_COUNT; i++) {
        lock_futex_lock(lock);
        crit_1();
        crit_2();
        unlock_futex_lock(lock);
    }
}

#endif

// =======================================================

bool test() {
    __atomic_store_n(&state, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&counter, 0, __ATOMIC_SEQ_CST);
    pthread_t t1, t2;
#ifdef no
    pthread_create(&t1, NULL, no_lock, NULL);
    pthread_create(&t2, NULL, no_lock, NULL);
#endif
#ifdef peterson
    peterson_lock_t lock = make_peterson_lock();
    peterson_args args1 = {&lock, 0};
    peterson_args args2 = {&lock, 1};
    pthread_create(&t1, NULL, peterson_lock, &args1);
    pthread_create(&t2, NULL, peterson_lock, &args2);
#endif
#ifdef semaphore
    semaphore_lock_t* lock = make_semaphore_lock();
    pthread_create(&t1, NULL, semaphore_lock, lock);
    pthread_create(&t2, NULL, semaphore_lock, lock);
#endif
#ifdef futex
    futex_lock_t lock = make_futex_lock();
    pthread_create(&t1, NULL, futex_lock, &lock);
    pthread_create(&t2, NULL, futex_lock, &lock);
#endif
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

#ifdef semaphore
    release_semaphore_lock(lock);
#endif
    return __atomic_load_n(&counter, __ATOMIC_SEQ_CST) == 0;
}

int main() {
    double mint = 1000;
    double maxt = 0;
    for (int i = 0; i < TEST_COUNT; i++) {
        struct timeval t1, t2;

        gettimeofday(&t1, NULL);
        if (!test()) {
            puts("Failed");
            return 0;
        }
        gettimeofday(&t2, NULL);
        
        double took;
        took = (t2.tv_sec - t1.tv_sec) * 1000.0;
        took += (t2.tv_usec - t1.tv_usec) / 1000.0;
        if (i > TEST_COUNT / 2) {
            if (took < mint) mint = took;
            if (took > maxt) maxt = took;
        }
    }
    printf("Min %f, Max %f \n", mint, maxt);
    puts("Passed");
}