[bits 64]

global load_gdt
global load_tss

load_gdt:
    lgdt [rdi]
    mov ax, 0x10        ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    pop rdi
    mov rax, 0x08       ; Kernel code segment
    push rax
    push rdi
    retfq

; Load TSS - selector in di
load_tss:
    ltr di
    ret