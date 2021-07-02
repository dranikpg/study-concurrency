## Memory ordering 

Instructions do not always execute in the same order as they were written in the source code. Both the compiler and the CPU can reorder instructions.

Obvious compiler reordering example.
An external thread could observe `b` being changed earlier than `a`.
```c++
a = b + 1;
b++;
```

The x86/64 family has a *strong* memory ordering, so writes and reads to the *same* location are executed in order, but "a load is allowed to be reordered with a store to a *different* location". This is because cache lines may be blocked and the CPU may choose then to do other operations first to avoid stalling.

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

Each programming language and complex system (Linux) defines its own memory model. The Java MM is simpler than the C++ one and hides many implementation details. Go acts alike and even advises [not to be clever](https://golang.org/ref/mem) :) Rust does not formally define one and uses the proposed C++20 MM.

Atomic wrappers in many programming languages are commonly used as "synchronization points".
A common core of memory orderings applied to those points can be deduced:

* Sequentially Consistent (SeqCst) 
    * Strongest memory ordering
    * A linear sequential and consistent order exists
* Acquire-Release
    * Used in pairs for effective message passing
    * Acquire when coupled with a load, if the loaded value was written by a store operation with Release (or stronger) ordering, then all subsequent operations become ordered after that store. 
    * Release coupled with a store, all previous operations become ordered before any load of this value with Acquire (or stronger) ordering
* Relaxed
    * No constraints, only atomic operations

## Message passing

Here we use atomics primary as a synchronization point, so a strong ordering is required: 

Thread1:
```c++
write_to_buffer();
ready.set(true, std::memory_order_seq_cst);
```

Thread2:
```c++
if (ready.load(std::memory_order_seq_cst)) {
    read_from_buffer();
}
```

This pattern is called *message passing*. 
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

## Barriers

Special CPU instructions, called *memory barriers* are used by the compiler to enforce specific memory models. `mfence` is the default x86-64 memory barrier. It guarantees that every load/store instruction preceding the memory fence will be visible after the memory fence.

```c++
__atomic_thread_fence(__ATOMIC_SEQ_CST);
/* ASM: mfence */
```

`lock()` runs test-and-set in a loop:

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

`unlock()` clears the lock and throws in a `mfence` to sync previously written data.

```x86asm
movl	$0, %eax
movl	%eax, locked(%rip)
mfence
```

Another **interesting** example is a simple relaxed counter. 

```c++
int count = 0;
...
while (true) {
    __atomic_fetch_add(&count, 11, __ATOMIC_RELAXED);
}
```

It uses the `lock` instruction prefix to ensure it has exclusive ownership of the cache line.

```x86asm
.L2:
	lock addl	$11, count(%rip)
	jmp	.L2
```

## Compilation guarantees

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
