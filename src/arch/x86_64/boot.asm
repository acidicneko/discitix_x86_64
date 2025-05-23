section .stivale2hdr write
align 4096
stivale2_header:
    .entry_point: dq _start
    .stack: dq stack.top
    .flags: dq 0
    .tags: dq fb_tag

section .bss nobits align=16
stack:
    resb 4096
.top:

section .data
align 16
fb_tag:
    .id: dq 0x3ecc1bc43d0f7971
    .next: dq fb_mtrr ;define it later
    .fb_width: dw 0 ;best find
    .fb_height: dw 0 ;best find
    .fb_bpp: dw 32 ;not less than 32 bpp

fb_mtrr:
    .id: dq 0x4c7bb07731282e00
    .next: dq 0

section .text
global _start
extern kmain
_start:
    cli
    mov rsp, stack.top + 0xFFFF800000000000
    call kmain
    hlt
    jmp loop

loop:
    cli
    hlt
    jmp $
