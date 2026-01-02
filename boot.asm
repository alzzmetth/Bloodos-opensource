; BloodOS Bootloader - Safe for real hardware
bits 16
org 0x7c00

section .text
start:
    ; Disable interrupts, setup segments
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti
    
    ; Save drive number
    mov [boot_drive], dl
    
    ; Clear screen
    mov ax, 0x0003
    int 0x10
    
    ; Load kernel (sector 2-50)
    mov bx, KERNEL_OFFSET
    mov dh, 50        ; Load 50 sectors (25KB)
    mov dl, [boot_drive]
    call disk_load
    
    ; Switch to protected mode
    cli
    lgdt [gdt_descriptor]
    
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    
    jmp CODE_SEG:init_pm

; === DISK LOAD FUNCTION ===
disk_load:
    pusha
    push dx
    
    mov ah, 0x02      ; BIOS read sectors
    mov al, dh        ; Sectors to read
    mov ch, 0x00      ; Cylinder 0
    mov dh, 0x00      ; Head 0
    mov cl, 0x02      ; Start from sector 2
    int 0x13
    
    jc disk_error     ; Carry flag = error
    
    pop dx
    cmp al, dh        ; AL = sectors read, DH = sectors expected
    jne disk_error
    
    popa
    ret

disk_error:
    mov si, error_msg
    call print_string
    jmp $

; === PRINT STRING FUNCTION ===
print_string:
    pusha
    mov ah, 0x0e
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

; === 32-BIT PROTECTED MODE ===
bits 32
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Setup stack
    mov ebp, 0x90000
    mov esp, ebp
    
    ; Jump to kernel
    jmp KERNEL_OFFSET

; === DATA ===
boot_drive db 0
error_msg db "Boot Error", 0

; === GDT ===
gdt_start:
    dq 0x0
gdt_code:
    dw 0xffff       ; Limit
    dw 0x0          ; Base
    db 0x0          ; Base
    db 10011010b    ; Access byte
    db 11001111b    ; Flags + Limit
    db 0x0          ; Base
gdt_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start
KERNEL_OFFSET equ 0x1000

; Boot signature
times 510-($-$$) db 0
dw 0xaa55
