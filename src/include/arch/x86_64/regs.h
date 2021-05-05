#ifndef __REGS_H__
#define __REGS_H__

#include <stdint.h>

typedef struct{               
    uint64_t rip, cs, rflags, usrsp, ss;             
    uint64_t gs, fs, es, ds;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsp, rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
} register_t;

#endif