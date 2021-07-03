#define STORE(ptr, value) __atomic_store_n(ptr, value, __ATOMIC_SEQ_CST)
#define LOAD(ptr) __atomic_load_n(ptr, __ATOMIC_SEQ_CST)