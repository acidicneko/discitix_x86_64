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
    
    uint64_t cr3;          // Physical address of PML4 for this process
    
    uint64_t brk_start;    // Start of heap (end of BSS)
    uint64_t brk_current;  // Current program break
    
    // Usermode support
    uint8_t is_usermode;   // 1 if this is a usermode task
    void *user_code;       // User code page (for cleanup) - or ELF pages array
    void *user_stack;      // User stack page (for cleanup)
    size_t user_code_pages; // Number of user code pages (for ELF)
    uint64_t mmap_base;
    struct file *fd_table[MAX_FDS];
    
    void *cwd;  
} task_t;

#define USER_STACK_TOP_VADDR 0x7FFFF0000000ULL
#define USER_STACK_VADDR     (USER_STACK_TOP_VADDR - 8192)

void init_scheduler();
task_t *create_elf_task_args(const void *elf_data, size_t elf_size, size_t stack_pages,
                              int argc, char *argv[], int envc, char *envp[]);
task_t *fork_current_task(register_t *parent_regs);
task_t *find_task_by_id(int id);
void schedule_tick(register_t *regs);
task_t *get_current_task();
void scheduler_sleep(uint64_t ticks);

#endif
