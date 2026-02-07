#ifndef _DLIBC_STDLIB_H
#define _DLIBC_STDLIB_H

#include "syscall.h"

static inline void exit(int code) {
    syscall1(SYS_EXIT, code);
    __builtin_unreachable();
}

#endif
