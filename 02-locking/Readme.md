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
[Peterson lock](./clib/peterson_lock.c)

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
[Semaphore lock](./clib/semaphore_lock.c)

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

[Futex lock](./clib/futex_lock.c)

## Rust

### std::sync

The rust standard library provides:
* Atomics
* Once
* Mutex
* RwLock
* Condvar
* Barrier
* Multi producer single consumer channel
    * asynchronous (never blocking) unlimited queue
    * synchronous (blocking) queue with limit (0 is a rendezvous)
* hint::spin_loop
* thread::yield_now
* thread_local!

#### How are channels implemented?

```rust
struct State<T> {
    disconnected: bool, // Is the channel disconnected yet?
    queue: Queue,       // queue of senders waiting to send data
    blocker: Blocker,   // currently blocked thread on this channel
    buf: Buffer<T>,     // storage for buffered messages
    cap: usize,         // capacity of this channel
}
```

#### How are mutexes implemented?

[From a forum thread about internals](https://internals.rust-lang.org/t/standard-library-synchronization-primitives-and-undefined-behavior/8439)

> Currently the system primitives Mutex, RwLock, Condvar, and ReentrantMutex (internal to libstd) all wrap underlying primitives for each os (pthreads on non-Window, corresponding objects on Windows). 

> Unfortunately, though, using the OS primitives brings quite a few caveats.

> First off, once the primitive is used its memory address cannot be changed. This requires our safe wrappers to use Box to contain the primitives (as Rust values can change memory addresses through moves).

> Some primitives like Condvar have restrictions such as they can only ever be used with at most one mutex. It’s undefined behavior with pthreads (I think) to use more than one mutex with a cond var (over its entire lifespan, not necessarily at the same time).

> Reentrantly acquiring a Mutex (aka locking it twice on the same thread) is undefined behavior by default in posix, so we have to handle this with explicit initialization

The biggest problem, I guess, is that
`Mutex` holds a `Box` to a `pthread` primitive. Combined with the fact, that `Mutexes` are often boxed in `Arc`'s, this adds up to **two** abstraction levels for basic locking.

### Parking lot

> To keep synchronization primitives small, all thread queuing and suspending functionality is offloaded to the parking lot. The idea behind this is based on the Webkit WTF::ParkingLot class, which essentially consists of a hash table mapping of lock addresses to queues of parked (sleeping) threads. The Webkit parking lot was itself inspired by Linux futexes, but it is more powerful since it allows invoking callbacks while holding a queue lock.

How does a parking_lot mutex compare to std?

* No poisoning, the lock is released normally on panic.
* Only requires 1 byte of space, whereas the standard library boxes the Mutex due to platform limitations.
* Can be statically constructed (requires the const_fn nightly feature).
* Does not require any drop glue when dropped.
* Inline fast path for the uncontended case.
* Efficient handling of micro-contention using adaptive spinning.
* Allows raw locking & unlocking without a guard.
* Supports eventual fairness so that the mutex is fair on average.
* Optionally allows making the mutex fair by calling MutexGuard::unlock_fair.

Contrary to std, a parking_lot mutex uses its memory address, because a locked mutex cannot move in memory as we have a hanging mutex guard to it. **Why** does this not apply to std? This is still an open question.

[raw_mutex.rs on github](https://github.com/Amanieu/parking_lot/blob/master/src/raw_mutex.rs)

What about the hash table? It uses some kind of global table for parked threads. In the end it just boils down to some kind of abstraction over `FUTEX` calls. I've looked it up. 

Besides that parking_lot uses addaptive spinning, so this kind of lock should do well even with small critical sections.

### Crossbeam

General multithreading utility library.

* AtomicConsume
    * Use faster *Consume* memory ordering for weak architectures like ARM and AArch64 where fence can be avoided.
* AtomicCell
    * `Cell<T>` but atomic
* deque
    * Work staealing queues
* channel
    * Multi producer *Multi* Consumer
    * select! macro
* Backoff
    * Smart spin-wait lock
* CachePadded
    * Pads value to cache to prevent cache line locking (in arrays for example)
* scope
    * scoped threads
* Parker
    * thread sleeping (parking) by command
* ShardedLock
    * > This lock is equivalent to RwLock, except read operations are faster and write operations are slower.
* WaitGroup
    * dynamically sized std::sync::Barrier
* epoch 
    * smart pointer for non blocking structures
    * possibly hazard pointers in the future
* TreiberStack
    * Lock free stack
* skiplist
    * WIP lock free collections on skip lists

### Rayon

* Easy parallel iterators
* Thread pools & scopes

### Others

* scoped_threadpool
* thread_local
    * for non-static thread_locals
* jod-thread
    * Join On Drop std::thread
* spin
    * no_std sync primitives
* flurry
    * **Jon Gjengset**'s port of Java’s ConcurrentHashMap
* dashmap
    * flurry clone but more similar to std api and somewhat faster


## Links

* [ТиПМС 1. Взаимное исключение, мьютексы и спинлоки](https://youtu.be/36LMjAFbFfg)
* [Basics of Futexes. Mutex implementation](https://eli.thegreenplace.net/2018/basics-of-futexes/)
* [POSIX Semaphores](https://www.softprayog.in/programming/posix-semaphores)
* [Std sync](https://doc.rust-lang.org/std/sync/index.html)
* [Parking Lot](https://github.com/Amanieu/parking_lot)
* [Spinlocks Rust](https://matklad.github.io/2020/01/02/spinlocks-considered-harmful.html)
* [Apple Webkit parking lot](https://webkit.org/blog/6161/locking-in-webkit/)
