#pragma once

#include <stdbool.h>
 
void testc();

typedef struct {
    bool want[2];
    int victim;
} peterson_lock_t;

peterson_lock_t make_peterson_lock();
void lock_peterson(peterson_lock_t* lock, int index);
void unlock_peterson(peterson_lock_t* lock, int index);