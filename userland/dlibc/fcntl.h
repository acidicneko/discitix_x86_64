#ifndef _DLIBC_FCNTL_H
#define _DLIBC_FCNTL_H

#include "syscall.h"

// Open flags
#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2

// Open a file, returns fd or -1 on error
static inline int open(const char *path, int flags) {
    return (int)syscall3(SYS_OPEN, (long)path, flags, 0);
}

// Close a file descriptor
static inline int close(int fd) {
    return (int)syscall1(SYS_CLOSE, fd);
}

// Read from file descriptor
static inline long read(int fd, void *buf, long count) {
    return syscall3(SYS_READ, fd, (long)buf, count);
}

// Write to file descriptor  
static inline long write(int fd, const void *buf, long count) {
    return syscall3(SYS_WRITE, fd, (long)buf, count);
}

#endif
