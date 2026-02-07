#ifndef _DLIBC_SYS_MMAN_H
#define _DLIBC_SYS_MMAN_H

#include "../syscall.h"
#include <stddef.h>

// Protection flags
#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

// Map flags
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON      MAP_ANONYMOUS

#define MAP_FAILED ((void*)-1)

// mmap - map files or devices into memory
static inline void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    long ret = syscall6(SYS_MMAP, (long)addr, (long)length, (long)prot, 
                        (long)flags, (long)fd, offset);
    if (ret < 0) return MAP_FAILED;
    return (void*)ret;
}

// munmap - unmap a mapped memory region
static inline int munmap(void *addr, size_t length) {
    return (int)syscall2(SYS_MUNMAP, (long)addr, (long)length);
}

#endif
