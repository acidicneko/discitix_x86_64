#include <arch/x86_64/syscall.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/gdt.h>
#include <libk/utils.h>
#include <libk/string.h>
#include <kernel/vfs/vfs.h>
#include <kernel/sched/scheduler.h>
#include <kernel/elf.h>
#include <drivers/tty/tty.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stdint.h>

extern void syscall_stub(void);

int64_t syscall_dispatch(register_t* regs) {
    uint64_t syscall_num = regs->rax;
    
    // Save regs pointer for fork/exec
    current_syscall_regs = regs;
    
    if (syscall_num >= MAX_SYSCALLS || !syscall_handlers[syscall_num]) {
        dbgln("Unknown syscall: %d\n\r", (int)syscall_num);
        current_syscall_regs = NULL;
        return -1;
    }
    
    // Arguments are passed in: rdi, rsi, rdx, r10, r8, r9
    // (Linux x86_64 syscall convention, but r10 instead of rcx)
    syscall_handler_t handler = syscall_handlers[syscall_num];
    int64_t result = handler(regs->rdi, regs->rsi, regs->rdx, 
                   regs->r10, regs->r8, regs->r9);
    
    current_syscall_regs = NULL;
    return result;
}

void syscall_register(uint32_t num, syscall_handler_t handler) {
    if (num < MAX_SYSCALLS) {
        syscall_handlers[num] = handler;
    }
}

void init_syscalls(void) {
    memset(syscall_handlers, 0, sizeof(syscall_handlers));
    
    syscall_register(SYS_EXIT, sys_exit);
    syscall_register(SYS_READ, sys_read);
    syscall_register(SYS_WRITE, sys_write);
    syscall_register(SYS_OPEN, sys_open);
    syscall_register(SYS_CLOSE, sys_close);
    syscall_register(SYS_FORK, sys_fork);
    syscall_register(SYS_EXEC, sys_exec);
    syscall_register(SYS_WAITPID, sys_waitpid);
    syscall_register(SYS_SPAWN, sys_spawn);
    syscall_register(SYS_BRK, sys_brk);
    syscall_register(SYS_MMAP, sys_mmap);
    syscall_register(SYS_MUNMAP, sys_munmap);
    syscall_register(SYS_GETDENTS64, sys_getdents64);
    syscall_register(SYS_STAT, sys_stat);
    syscall_register(SYS_FSTAT, sys_fstat);
    
    // Set up interrupt 0x80 for syscalls
    // Flags: 0xEE = Present(1) | DPL(11) | Type(01110) = interrupt gate accessible from Ring 3
    idt_set_gate(0x80, (uint64_t)syscall_stub, GDT_KERNEL_CODE, 0xEE);
    
    dbgln("Syscalls initialized (int 0x80)\n\r");
}
