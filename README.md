My notes while studying concurrency in depth:

[The first part](https://github.com/dranikpg/study-concurrency/tree/main/01-memory-model) explores the notion of memory models and why we need synchronization. How do atomics work. What is cache coherency.

[The second part](https://github.com/dranikpg/study-concurrency/tree/main/02-locking) is about different kinds of locks and their bare C implementation. Takes a brief look on C++/Rust standart libraries and popular crates

[The third part](https://github.com/dranikpg/study-concurrency/tree/main/03-fibers) is my implementation of **stackful** cooperative coroutines (fibers) in Rust

The fourth part is about the GO scheduler (TODO)

[The fifth part](https://github.com/dranikpg/study-concurrency/tree/main/05-async-cpp) explores C++ async by implementing a custom runtime with timer futures based on effective wakeups

[The sixth part](https://github.com/dranikpg/study-concurrency/tree/main/06-async-rust) is a Rust version of the fifth part

[The seventh part](https://github.com/dranikpg/study-concurrency/tree/main/07-lock-free) Takes a look the lock free Treiber stack and explores ways of managing memory manually and concurrently - with simple hazard pointers.

[The eight part](https://github.com/dranikpg/study-concurrency/tree/main/08-concurrent-gc) is a simple STW concurrent mark-and-sweep garbage collector in C++.
