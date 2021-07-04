#pragma once
#include <stddef.h>
#include <semaphore.h>

typedef sem_t semaphore_lock_t;

semaphore_lock_t* make_semaphore_lock();
void release_semaphore_lock(semaphore_lock_t* lock);
void lock_semaphore(semaphore_lock_t*);
void unlock_semaphore(semaphore_lock_t*);