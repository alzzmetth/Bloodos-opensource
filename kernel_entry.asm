[BITS 32]
[GLOBAL _start]
[EXTERN kernel_main]

section .text
_start:
    ; Set up stack
    mov esp, kernel_stack + KERNEL_STACK_SIZE
    
    ; Call main kernel
    call kernel_main
    
    ; If kernel returns, halt
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
kernel_stack:
    resb KERNEL_STACK_SIZE
KERNEL_STACK_SIZE equ 4096
