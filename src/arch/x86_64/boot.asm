section .stivale2hdr
stivale2_header:
    .entry_point: dq 0
    .stack: dq stack.top
    .flags: dq 0
    .tags: dq fb_tag

section .bss
stack:
    resb 4096
.top:

section .data
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
    call kmain
    hlt
    jmp loop

loop:
    cli
    hlt
    jmp $