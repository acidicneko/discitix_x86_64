#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <stddef.h>
#include <stdint.h>
#include <arch/x86_64/regs.h>

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
    void *stack_base;
    size_t stack_pages;
    int id;
    uint64_t wake_tick;
} task_t;

void init_scheduler();
task_t *create_task(void (*entry)(void *), void *arg, size_t stack_pages);
void schedule_tick(register_t *regs);
task_t *get_current_task();
void scheduler_sleep(uint64_t ticks);

#endif
