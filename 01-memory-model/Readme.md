## Memory access ordering 

Instructions do not always execute in the same order as they were written in the source code. Both the compiler and the CPU can reorder instructions.

In the first example the compiler may reorder `b++` to happen first, so an another thread could observe `b` being changed earlier than `a`.

```c++
a = b + 1;
b++;
```

The CPU may reorder instructions to avoid stalls. The x86/64 family has a *strong* memory ordering, so writes and reads to the *same* location are executed in order, but a load is allowed to be reordered with a store to a *different* location. This is because cache lines might be blocked and the CPU may choose then to do other operations first.

```c++
int x, y = 0;
int r1, r2 = 0;
std::thread t1([&](){
    x = 1;
    r1 = y;
});
std::thread t2([&](){
    y = 1;
    r2 = x;
});
t1.join();
t2.join();
if (r1 == 0 && r2 == 0) {
    std::cout << "May happen" << std::endl;
}
```

## Memory models

Each programming language and complex system (e.g. Linux) defines its own memory model. It can be described as set of strong guarantees of memory access order and change visibility in defined conditions.
The Java MM is simpler than the C++ one and hides many implementation details. Go acts alike and even advises [not to be clever](https://golang.org/ref/mem) :) Rust does not formally define one and uses the proposed C++20 MM. 

## Atomics

Atomic operations are commonly used as "synchronization points" so they can impose rules on the order of writes/reads around the atomic access. A common core of memory orderings applied to those points can be deduced:

* Sequentially Consistent (SeqCst) 
    * Strongest memory ordering
    * A linear sequential and consistent order exists for all accesses
* Acquire-Release
    * Used in pairs for effective message passing
    * Acquire coupled with a load, if the loaded value was written by a store operation with Release (or stronger) ordering, then all subsequent operations become ordered after that store. 
    * Release coupled with a store, all previous operations become ordered before any load of this value with Acquire (or stronger) ordering
* Relaxed
    * No constraints, only atomic operations

SeqCst and a correctly implemented Acquire-Release are *strong* memory orderings.

## Message passing

Strong memory orderings allow us to carry out safe *message passing*. Because of a specific memory model and memory ordering being used, 
`write()` is *guaranteed* to happen before `read()`:

Thread1:
```c++
write();
ready.set(true, std::memory_order_seq_cst);
```

Thread2:
```c++
if (ready.load(std::memory_order_seq_cst)) {
    read();
}
```

A common example of this might be spin-lock. It actually doesn't require a SeqCst memory ordering, 
so a potentially faster implementation looks like:

```c++
std::atomic_flag locked = ATOMIC_FLAG_INIT;
void lock() {
    while (locked.test_and_set(std::memory_order_acquire)) { ; }
}
void unlock() {
    locked.clear(std::memory_order_release);
}
```

Whereas a weak (relaxed) ordering is enough for a simple counter:

```c++
for(auto& issue: list) {
    issues_count.inc(std::memory_order_relaxed);
}
```

## Cache coherency

CPU caches use a cache coherency protocol to synchronize reads and writes to main memory. This imposes something similar to a **RWLock for cache lines**. This means that our `test_and_set` operation in the spinlock `lock()` function puts a lot of pressure on the caches.
That is why a *test and test-and-set* spinlock is commonly used, which tries to reduce write(set) operations as much as possible:   
```c++
void lock() {
  while (true) {
    if (!lock_.test_and_set(std::memory_order_acquire)) {
      break;
    }
    while (lock_.test(std::memory_order_relaxed)) { ; }
  }
}
```


## Assembly

I'll be using the [GCC built-in atomic functions](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html) as they generate simpler assembly.

Lets look at a simple relaxed counter. What assembly does it generate?

```c++
int count = 0;
while (true) {
    __atomic_fetch_add(&count, 5, __ATOMIC_RELAXED);
}
```


It uses the `lock` instruction prefix to ensure it has exclusive ownership of the cache line.

```x86asm
.L2:
	lock addl	$5, count(%rip)
	jmp	.L2
```

What about the spinlock assembly? `lock()` runs test-and-set in a loop. 

**!!** `xchgb` doesn't require a `lock` prefix, because it is already atomic [according to the Intel docs](https://stackoverflow.com/questions/3144335/on-a-multicore-x86-is-a-lock-necessary-as-a-prefix-to-xchg).

```x86asm
.L3:
	movl	$1, %eax
	xchgb	locked(%rip), %al
	testb	%al, %al
	je	.L4
	jmp	.L3
.L4:
	ret
```

*Memory barriers* are used to enforce specific memory models. `mfence` is the default x86-64 memory barrier. It guarantees that every load/store instruction preceding the memory fence will be visible after the memory fence.

```c++
__atomic_thread_fence(__ATOMIC_SEQ_CST);
/* ASM: mfence */
```

`unlock()` clears the lock and throws in a `mfence` to sync previously written data.

```x86asm
movl	$0, %eax
movl	%eax, locked(%rip)
mfence
```


## C++

The C++ memory model is *quite* complex. In short, it states that a program can be translated to a graph of *happens-before* relations, where the nodes are synchronization points. Nodes may contain any number independent operations, which can be reordered within the node to improve performance. This results in a balanced model, which favours both performance and safety. C++ guarantees **sequential consistency for data race free programs**. 

## Links

* [ТиПМС 5. Модели памяти, часть I](https://youtu.be/Ao7qoAc9AGc)

* [ТиПМС 6. Модели памяти, часть II](https://youtu.be/q9dXzh5yBdA)

* [Crust of Rust: Memory Ordering](https://youtu.be/rMGWeSjctlY)
    
* [Rust nomicon on atomics](https://doc.rust-lang.org/nomicon/atomics.html)

* [C++ Memory order](https://en.cppreference.com/w/cpp/atomic/memory_order)

* [Test and test-and-set spinlock](https://rigtorp.se/spinlock/)

* [Linux kernel memory barriers](https://www.kernel.org/doc/Documentation/memory-barriers.txt)

* [Java memory model (§17.4)](https://docs.oracle.com/javase/specs/jls/se8/html/jls-17.html)
