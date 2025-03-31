.globl main

.data
hello:
    .string "Hello World! %ld\n"

.text
main:                       # Function main:
    pushq %rbp              # Push frame pointer to stack
    movq %rsp, %rbp         # Save stack pointer as frame pointer

    movq $42, %rsi          # Setting arg 2 to constant value 42
    leaq hello(%rip), %rdi  # Setting arg 1 to defined string "Hello World! %ld\n". Note that we here load the address
    call printf             # Print using the arguments in %rsi and %rdi

    leave                   # Done with main: reverting pointers and pop from stack 
    ret                     # Return from main

// Workaround for macOS prefixing symbols with _
#ifdef __APPLE__
    .set printf, _printf
    .set putchar, _putchar
    .set _main, main
    .global _main
#endif
