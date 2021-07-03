## Message passing

Message passing is the base for mutual exclusion. Its correctness was mentioned in the memory model section.

## Spin lock

Spin locks together with their assembly are shown in the memory model section. The process of spinning is called **busy-waiting**. 
Busy waiting loops should use the `pause` instruction to give hints to the processor.
The `sched_yield` syscall is used to return CPU control to the system. 

Spinlocks are often used in cases where threads don't block for a long time. This saves the context switches and interrupts that happen with regular mutexes. Sometimes special `spin-wait` locks are used that first spin for a short time, and then wait if they didn't acquire the lock.

### Peterson lock 

A very simple spin lock for only two threads. It uses only load/set operations, whereas a generic spin lock uses test-and-set.

```c
void lock_peterson(peterson_lock_t* lock, int index) {
    STORE(&lock->want[index], 1);
    STORE(&lock->victim, index);
    while(LOAD(&lock->want[1-index]) && LOAD(&lock->victim) == index) {}
}
```
[Peterson lock](./peterson_lock.cpp)

### Ticket lock

A thread may be neglected by the scheduler and gain no access to a spinlock for a long time, whereas other threads continuously acquire the lock. In this case a ticket lock can be used to enqueue all waiting threads in a strict order, hence the name.

## Sleep lock

A sleep lock performs no busy waiting and instead suspends the current thread. Sleep locks are system dependant.

### POSIX semaphore

Linux semaphores allow proccesses and threads to synchronize. Semaphores can be named, so other processes can locate them. They usually live in `/dev/shm`. Unnamed semaphores for multiple processes can be constructed in shared memory. Unnamed semaphores are referenced with pointers.

```c
semaphore_lock_t* make_semaphore_lock() {
    sem_t* sem = malloc(sizeof(sem));
    sem_init(sem, false, 1);
    return sem;
}
void lock_semaphore(semaphore_lock_t* lock) {
    sem_wait(lock);
}
```
[Semaphore lock](./semaphore_lock.cpp)

### Futex

`FUTEX` is a kernel system call for building mutexes that store their data in user space, so the number of context switches is minimal. The kernel builds a wait-queue for each futex to suspend and resume threads. Most modern mutex libraries use `FUTEX` internally.

A very simple, but not optimal futex-mutex implementation:
```c
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
```
An optimal mutex would use three atomic states to skip redundant `FUTEX_WAKE` calls.

[Futex lock](./futex_lock.cpp)

## Links

* [ТиПМС 1. Взаимное исключение, мьютексы и спинлоки](https://youtu.be/36LMjAFbFfg)
* [Basics of Futexes. Mutex implementation](https://eli.thegreenplace.net/2018/basics-of-futexes/)
* [POSIX Semaphores](https://www.softprayog.in/programming/posix-semaphores)