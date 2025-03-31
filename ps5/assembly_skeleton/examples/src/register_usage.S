.globl main

.data
hello:
    .string "I sure hope my register hasn't been overwritten!\n"
register_message:
    .string "Register %ld contains %ld\n"
push_pop_message:
    .string "\n\nThis time pushing r8 to the stack and popping it afterwards.\n\n"

/*
    Caller-save (i.e., "volatile"): %rax, %rcx, %rdx, %rdi, %rsi, %rsp, and %r8-r11

    Callee-save (i.e., "global"): %rbx, %rbp, and %r12-r15
*/

.text
main:
    pushq %rbp
    movq %rsp, %rbp

    movq $42, %r8           # r8  is caller-save
    movq $42, %r12          # r12 is callee-save

    # Print value of r8
    leaq register_message(%rip), %rdi
    movq $8, %rsi
    movq %r8, %rdx
    call printf

    # Print value of r12
    leaq register_message(%rip), %rdi
    movq $12, %rsi
    movq %r12, %rdx
    call printf

    # Print message
    # Any function may disrupt r8 - not only prints
    leaq hello(%rip), %rdi
    call printf

    # Print value of r8
    leaq register_message(%rip), %rdi
    movq $8, %rsi
    movq %r8, %rdx
    call printf

    # Print value of r12
    leaq register_message(%rip), %rdi
    movq $12, %rsi
    movq %r12, %rdx
    call printf

    # We may store temporary values on the stack and pop them afterwards

    leaq push_pop_message(%rip), %rdi
    call printf

    movq $42, %r8
    movq $42, %r12

    # Note that we push TWO values to the stack
    # Usually, we wouldn't need to do this,
    # but printf specifically expects the stack to be 16-byte alligned
    # and may throw a segmentation fault if we don't always have an even number of quads on the stack...

    pushq %r8               # Pushing the value of r8 to the stack
    pushq $0                
    
    # Print value of r8
    leaq register_message(%rip), %rdi
    movq $8, %rsi
    movq %r8, %rdx
    call printf

    # Print value of r12
    leaq register_message(%rip), %rdi
    movq $12, %rsi
    movq %r12, %rdx
    call printf

    # Print message
    leaq hello(%rip), %rdi
    call printf

    popq %rax               # Popping the top of the stack - 0 - to a random unused register
    popq %r8                # Popping the top of the stack - 42 - to r8

    # Print value of r8
    leaq register_message(%rip), %rdi
    movq $8, %rsi
    movq %r8, %rdx
    call printf

    # Print value of r12
    leaq register_message(%rip), %rdi
    movq $12, %rsi
    movq %r12, %rdx
    call printf

    movq $0, %rax
    leave
    ret

// Workaround for macOS prefixing symbols with _
#ifdef __APPLE__
    .set printf, _printf
    .set putchar, _putchar
    .set _main, main
    .global _main
#endif
