#ifndef _DLIBC_SYSCALL_H
#define _DLIBC_SYSCALL_H

// Syscall numbers (matching kernel)
#define SYS_EXIT    0
#define SYS_READ    1  
#define SYS_WRITE   2
#define SYS_OPEN    3
#define SYS_CLOSE   4
#define SYS_FORK    5
#define SYS_EXEC    6
#define SYS_WAITPID 7
#define SYS_SPAWN   8
#define SYS_BRK     9
#define SYS_MMAP    10
#define SYS_MUNMAP  11
#define SYS_GETDENTS64 12
#define SYS_STAT    13
#define SYS_FSTAT   14

static inline long syscall0(long num) {
    long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num)
        : "memory"
    );
    return ret;
}

static inline long syscall1(long num, long arg1) {
    long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "memory"
    );
    return ret;
}

static inline long syscall2(long num, long arg1, long arg2) {
    long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2)
        : "memory"
    );
    return ret;
}

static inline long syscall3(long num, long arg1, long arg2, long arg3) {
    long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

static inline long syscall4(long num, long arg1, long arg2, long arg3, long arg4) {
    long ret;
    register long r10 asm("r10") = arg4;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "memory"
    );
    return ret;
}

static inline long syscall6(long num, long arg1, long arg2, long arg3, 
                            long arg4, long arg5, long arg6) {
    long ret;
    register long r10 asm("r10") = arg4;
    register long r8 asm("r8") = arg5;
    register long r9 asm("r9") = arg6;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
        : "memory"
    );
    return ret;
}

#endif
