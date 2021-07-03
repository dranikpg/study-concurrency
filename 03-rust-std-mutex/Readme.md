## std::sync

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

### How are channels implemented?

```rust
struct State<T> {
    disconnected: bool, // Is the channel disconnected yet?
    queue: Queue,       // queue of senders waiting to send data
    blocker: Blocker,   // currently blocked thread on this channel
    buf: Buffer<T>,     // storage for buffered messages
    cap: usize,         // capacity of this channel
}
```

### How are mutexes implemented?

[From a forum thread about internals](https://internals.rust-lang.org/t/standard-library-synchronization-primitives-and-undefined-behavior/8439)

> Currently the system primitives Mutex, RwLock, Condvar, and ReentrantMutex (internal to libstd) all wrap underlying primitives for each os (pthreads on non-Window, corresponding objects on Windows). 

> Unfortunately, though, using the OS primitives brings quite a few caveats.

> First off, once the primitive is used its memory address cannot be changed. This requires our safe wrappers to use Box to contain the primitives (as Rust values can change memory addresses through moves).

> Some primitives like Condvar have restrictions such as they can only ever be used with at most one mutex. It’s undefined behavior with pthreads (I think) to use more than one mutex with a cond var (over its entire lifespan, not necessarily at the same time).

> Reentrantly acquiring a Mutex (aka locking it twice on the same thread) is undefined behavior by default in posix, so we have to handle this with explicit initialization

The biggest problem, I guess, is that
`Mutex` holds a `Box` to a `pthread` primitive. Combined with the fact, that `Mutexes` are often boxed in `Arc`'s, this adds up to **two** abstraction levels for basic locking.

## Parking Lot

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

## Links
* [Std sync](https://doc.rust-lang.org/std/sync/index.html)
* [Parking Lot](https://github.com/Amanieu/parking_lot)
* [Apple Webkit parking lot](https://webkit.org/blog/6161/locking-in-webkit/)