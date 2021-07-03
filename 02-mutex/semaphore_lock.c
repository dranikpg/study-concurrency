#include "semaphore_lock.h"
#include "malloc.h"
#include "stdbool.h"

semaphore_lock_t* make_semaphore_lock() {
    sem_t* sem = malloc(sizeof(sem));
    sem_init(sem, false, 1);
    return sem;
}

void release_semaphore_lock(semaphore_lock_t* lock) {
    sem_destroy(lock);
    free(lock);
}

void lock_semaphore(semaphore_lock_t* lock) {
    sem_wait(lock);
}

void unlock_semaphore(semaphore_lock_t* lock) {
    sem_post(lock);
}