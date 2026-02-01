#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stdint.h>
#include <arch/x86_64/regs.h>

// Syscall numbers
#define SYS_EXIT    0
#define SYS_READ    1
#define SYS_WRITE   2
#define SYS_OPEN    3
#define SYS_CLOSE   4

// Maximum syscall number
#define MAX_SYSCALLS 32

// Syscall handler function type
typedef int64_t (*syscall_handler_t)(uint64_t arg1, uint64_t arg2, uint64_t arg3, 
                                      uint64_t arg4, uint64_t arg5, uint64_t arg6);

// Initialize syscall handling (sets up int 0x80)
void init_syscalls(void);

// Register a syscall handler
void syscall_register(uint32_t num, syscall_handler_t handler);

// Syscall dispatcher (called from assembly)
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

#endif
