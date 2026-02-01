#include <arch/x86_64/syscall.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/gdt.h>
#include <libk/utils.h>
#include <libk/string.h>
#include <kernel/vfs/vfs.h>
#include <kernel/sched/scheduler.h>
#include <drivers/tty/tty.h>

// Syscall handler table
static syscall_handler_t syscall_handlers[MAX_SYSCALLS];

// External assembly handler
extern void syscall_stub(void);

// Syscall implementations
int64_t sys_exit(uint64_t status, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    dbgln("Process (task %d) called exit(%d)\n\r", 
          current ? current->id : -1, (int)status);
    
    if (current) {
        // Mark task as zombie - scheduler will clean it up
        current->state = TASK_ZOMBIE;
    }
    
    // Enable interrupts and halt - scheduler will switch us out
    for (;;) {
        asm volatile("sti; hlt");
    }
    
    return 0;
}

int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    // TODO: validate user pointer
    const char* user_buf = (const char*)buf;
    
    if (fd == 1 || fd == 2) {
        // stdout or stderr - write to current TTY
        tty_t* tty = get_current_tty();
        if (tty) {
            for (size_t i = 0; i < count; i++) {
                tty_putchar(user_buf[i]);
            }
            return (int64_t)count;
        }
        return -1;
    }
    
    // TODO: handle other file descriptors via VFS
    return -1;
}

int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    (void)fd; (void)buf; (void)count;
    
    // TODO: implement reading from stdin/files
    return -1;
}

// Dispatch syscall based on syscall number in rax
int64_t syscall_dispatch(register_t* regs) {
    uint64_t syscall_num = regs->rax;
    
    if (syscall_num >= MAX_SYSCALLS || !syscall_handlers[syscall_num]) {
        dbgln("Unknown syscall: %d\n\r", (int)syscall_num);
        return -1;
    }
    
    // Arguments are passed in: rdi, rsi, rdx, r10, r8, r9
    // (Linux x86_64 syscall convention, but r10 instead of rcx)
    syscall_handler_t handler = syscall_handlers[syscall_num];
    return handler(regs->rdi, regs->rsi, regs->rdx, 
                   regs->r10, regs->r8, regs->r9);
}

void syscall_register(uint32_t num, syscall_handler_t handler) {
    if (num < MAX_SYSCALLS) {
        syscall_handlers[num] = handler;
    }
}

void init_syscalls(void) {
    // Clear handler table
    memset(syscall_handlers, 0, sizeof(syscall_handlers));
    
    // Register built-in syscalls
    syscall_register(SYS_EXIT, sys_exit);
    syscall_register(SYS_READ, sys_read);
    syscall_register(SYS_WRITE, sys_write);
    
    // Set up interrupt 0x80 for syscalls
    // Flags: 0xEE = Present(1) | DPL(11) | Type(01110) = interrupt gate accessible from Ring 3
    idt_set_gate(0x80, (uint64_t)syscall_stub, GDT_KERNEL_CODE, 0xEE);
    
    dbgln("Syscalls initialized (int 0x80)\n\r");
}
