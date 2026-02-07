#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <stddef.h>
#include <stdint.h>
#include <arch/x86_64/regs.h>

struct file;

// Maximum open files per process TODO: maybe increase it in future
#define MAX_FDS 16

typedef enum { 
    TASK_RUNNABLE, 
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_ZOMBIE 
} task_state_t;

typedef struct task {
    struct task *next;
    task_state_t state;
    register_t regs;
    void *stack_base;      // Kernel stack base
    size_t stack_pages;
    int id;
    uint64_t wake_tick;
    
    int parent_id;         // Parent task ID (0 if orphan/init)
    int exit_status;       // Exit status for waitpid
    
    // Per-process page table (physical address for CR3)
    uint64_t cr3;          // Physical address of PML4 for this process
    
    // Heap management (brk)
    uint64_t brk_start;    // Start of heap (end of BSS)
    uint64_t brk_current;  // Current program break
    
    // Usermode support
    uint8_t is_usermode;   // 1 if this is a usermode task
    void *user_code;       // User code page (for cleanup) - or ELF pages array
    void *user_stack;      // User stack page (for cleanup)
    size_t user_code_pages; // Number of user code pages (for ELF)
    
    // File descriptor table (fd 0=stdin, 1=stdout, 2=stderr)
    struct file *fd_table[MAX_FDS];
} task_t;

void init_scheduler();
task_t *create_task(void (*entry)(void *), void *arg, size_t stack_pages);
task_t *create_user_task(void *user_code, size_t code_size, size_t stack_pages);
task_t *create_elf_task(const void *elf_data, size_t elf_size, size_t stack_pages);
task_t *create_elf_task_args(const void *elf_data, size_t elf_size, size_t stack_pages,
                              int argc, char *argv[]);
task_t *fork_current_task(register_t *parent_regs);
task_t *find_task_by_id(int id);
void schedule_tick(register_t *regs);
task_t *get_current_task();
void scheduler_sleep(uint64_t ticks);

#endif
