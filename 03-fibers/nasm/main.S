    global nasm_switch:function
    global nasm_init:function
    section .text

nasm_switch:
    ; rdi - ptr to old rsp
    ; rsi - new rsp
    ; rdx - arg1
    ; rcx - arg2

    ; save old
    push r15
    push r14
    push r13
    push r12
    push rbx
    push rbp
    mov [rdi], rsp

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

nasm_init:
    ; rdi - stack base address
    ; rsi - func address
    enter 0, 0
    mov rcx, rsp
    
    mov rsp, rdi
    push 0
    push rsi ;ret addr
    push 0   ;r15
    push 0   ;r14
    push 0   ;r13
    push 0   ;r12
    push 0   ;rbx
    push rdi   ;rbp
    mov rax, rsp

    mov rsp, rcx
    leave
    ret
