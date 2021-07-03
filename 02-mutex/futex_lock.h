#include <stdbool.h>

typedef struct {
    int atom;
} futex_lock_t;

futex_lock_t make_futex_lock();
void lock_futex_lock(futex_lock_t*);
void unlock_futex_lock(futex_lock_t*);