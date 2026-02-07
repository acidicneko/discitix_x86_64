#ifndef _DLIBC_UNISTD_H
#define _DLIBC_UNISTD_H

#include "syscall.h"

// Fork - creates a new process (NOTE: broken without per-process page tables)
// Returns: child PID to parent, 0 to child, -1 on error
static inline int fork(void) {
    return (int)syscall0(SYS_FORK);
}

// Exec - replace current process image with new program
// Returns: -1 on error, doesn't return on success
static inline int execv(const char *path, char *const argv[]) {
    return (int)syscall3(SYS_EXEC, (long)path, (long)argv, 0);
}

// Execve - exec with environment (env ignored for now)
static inline int execve(const char *path, char *const argv[], char *const envp[]) {
    return (int)syscall3(SYS_EXEC, (long)path, (long)argv, (long)envp);
}

// Spawn - create a new process from an executable (preferred over fork+exec)
// Returns: child PID to parent, -1 on error
static inline int spawn(const char *path, char *const argv[]) {
    return (int)syscall3(SYS_SPAWN, (long)path, (long)argv, 0);
}

// Waitpid flags
#define WNOHANG 1

// Waitpid - wait for child process
// Returns: PID of child that exited, 0 if WNOHANG and not ready, -1 on error
static inline int _waitpid_raw(int pid, int *status, int options) {
    return (int)syscall3(SYS_WAITPID, (long)pid, (long)status, (long)options);
}

// Blocking waitpid - polls until child exits
static inline int waitpid(int pid, int *status, int options) {
    int ret;
    while (1) {
        ret = _waitpid_raw(pid, status, options);
        if (ret != -2) {  // -2 means "try again"
            return ret;
        }
        // Small delay to let scheduler run
        for (volatile int i = 0; i < 10000; i++);
    }
}

// Wait - wait for any child process
static inline int wait(int *status) {
    return waitpid(-1, status, 0);
}

// Get process ID (not implemented yet, returns -1)
static inline int getpid(void) {
    return -1;  // TODO: implement SYS_GETPID
}

// brk - change data segment size
// Returns new program break on success, -1 on error
static inline void *brk(void *addr) {
    long ret = syscall1(SYS_BRK, (long)addr);
    if (ret == -1) return (void*)-1;
    return (void*)ret;
}

// sbrk - increment data segment size
static inline void *sbrk(long increment) {
    void *current = brk(0);
    if (current == (void*)-1) return (void*)-1;
    if (increment == 0) return current;
    void *new_brk = brk((void*)((long)current + increment));
    if (new_brk == (void*)-1) return (void*)-1;
    return current;  // Return old break
}

#endif
