## Fibers

Fibers are suspendable stackful tasks. It's pretty clear how to implement them for virtual machines
and interpreters. But zero cost fibers for compiled programming languages require some tricks.

Let's see fibers in action and assume `f1` is called first:

```rust
fn f1(){
    enqueue(f2); // prepare new task
    println!("1  - 1");
    suspend(); // switch to f2
    println!("1  -   2");
    // sitches to f2
}

fn f2() {
    println!(" 2 - 1");
    suspend(); // switch to f1
    println!(" 2 -   2");
    // halt
}
```

Then the output is as follows:
```
1  - 1
 2 - 1
1  -   2
 2 -   2
```

### The magic switch function

Let's see how a `switch` function can be implemented, that switches two execution contexts.
We'll need some assembly for that and I'm going to use netwide assembler.

When switching contexts all caller-reserved registers are pushed onto the current stack frame. 
The return address is located before them, as it was pushed on to the stack frame when calling the `switch` function. At the end, we just have to save the new stack pointer. This is enough to save a fully restorable execution context. 

This is how a execution context is stored:
```nasm
nasm_switch:
    ; rdi - ptr to old rsp
    ; rsi - new rsp

    ; save old
    push r15
    push r14
    push r13
    push r12
    push rbx
    push rbp
    mov [rdi], rsp
```
To restore an execution context, we just have to reverse all previous actions:
```nasm
    ; restore new
    mov rsp, rsi
    pop rbp
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15

    ; move args
    mov rdi, rdx
    mov rsi, rcx

    ret
```
Ret uses the return address from the current stackframe, so the fibers returns to the point it was suspended.

### Initializing stack frames

Initalizing stack frames is pretty simple. We just have to make sure the return address points to out start function.

```nasm
nasm_init:
    ; rdi - stack base address
    ; rsi - func address
    enter 0, 0
    mov rcx, rsp
    
    mov rsp, rdi
    push 0      ;for 16 byte align
    push rsi    ;ret addr (our function)
    push 0      ;r15
    push 0      ;r14
    push 0      ;r13
    push 0      ;r12
    push 0      ;rbx
    push rdi    ;rbp
    mov rax, rsp

    mov rsp, rcx
    leave
    ret
```

### What exactly are stack frames?

Memory regions. I'm using Vec solely for simplicity, one should use memory pages for this purpose.

``` rust
pub struct Stack {
    mem: Option<Vec<u8>>,
    top: Cell<usize>
}
```

```rust
pub fn switch(from: &Stack, to: &Stack, (arg1, arg2): (usize, usize)) {
    unsafe {
        nasm_switch(from.top.as_ptr(), to.top.get(), arg1, arg2);
    }
}
```

### The executor

The executor internally uses the same switch magic. It constructs a task on top 
of the real stack frame and uses it for switching to other tasks

```rust
fn run(&mut self) {
    while let Some(fiber) = self.queue.pop_front() {
        self.current = Some(fiber.clone());
        match self.switch_to(&fiber) { // switch to fiber and back
            FiberState::Finished => continue,
            FiberState::Suspended => self.queue.push_back(fiber),
            _ => unreachable!()
        };
    }
}
```

### The trampoline

The trampoline is where fibers execution starts and stops.
It passes the function pointer (fat pointer) through two registers when 
initializing the stackframe (I skipped this in the asm part intentionally).
It also stops the current fiber before crashing.

```rust
fn trampoline(args: (usize, usize)) -> ! {
    let func_ptr: &dyn Fn() = unsafe {std::mem::transmute(args)};
    func_ptr();
    unsafe {EXECUTOR.as_mut().unwrap().suspend(FiberState::Finished);}
    unreachable!();
}
```


## Links

* [ТиПМС, семинар 3. Файберы](https://youtu.be/OEY1StCZtpU)
* [Folly fibers](https://github.com/facebook/folly/blob/master/folly/fibers/README.md)
* [Fibers tutorial](https://graphitemaster.github.io/fibers)
