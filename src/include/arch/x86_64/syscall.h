#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stdint.h>
#include <arch/x86_64/regs.h>
#include <stddef.h>

#define SYS_EXIT        0
#define SYS_READ        1
#define SYS_WRITE       2
#define SYS_OPEN        3
#define SYS_CLOSE       4
#define SYS_FORK        5
#define SYS_EXEC        6
#define SYS_WAITPID     7
#define SYS_SPAWN       8
#define SYS_BRK         9
#define SYS_MMAP        10
#define SYS_MUNMAP      11
#define SYS_GETDENTS64  12
#define SYS_STAT        13
#define SYS_FSTAT       14


#define MAX_SYSCALLS 32

static syscall_handler_t syscall_handlers[MAX_SYSCALLS];

static register_t *current_syscall_regs = NULL;

// Syscall handler function type
typedef int64_t (*syscall_handler_t)(uint64_t arg1, uint64_t arg2, uint64_t arg3, 
                                      uint64_t arg4, uint64_t arg5, uint64_t arg6);


void init_syscalls(void);


void syscall_register(uint32_t num, syscall_handler_t handler);

int64_t syscall_dispatch(register_t* regs);

// Jump to user mode (Ring 3)
// entry: user code entry point
// user_stack: top of user stack
extern void jump_to_usermode(uint64_t entry, uint64_t user_stack);

// Built-in syscall handlers
int64_t sys_exit(uint64_t status, uint64_t arg2, uint64_t arg3, 
                 uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_open(uint64_t path, uint64_t flags, uint64_t mode,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_close(uint64_t fd, uint64_t arg2, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_fork(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_exec(uint64_t path, uint64_t argv, uint64_t envp,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_waitpid(uint64_t pid, uint64_t status_ptr, uint64_t options,
                    uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_spawn(uint64_t path, uint64_t argv, uint64_t envp,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_brk(uint64_t addr, uint64_t arg2, uint64_t arg3,
                uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                 uint64_t flags, uint64_t fd, uint64_t offset);
int64_t sys_munmap(uint64_t addr, uint64_t length, uint64_t arg3,
                   uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_getdents64(uint64_t fd, uint64_t buf_ptr, uint64_t count,
                       uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_stat(uint64_t path_ptr, uint64_t buf_ptr, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6);
int64_t sys_fstat(uint64_t fd, uint64_t buf_ptr, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6);

#endif
