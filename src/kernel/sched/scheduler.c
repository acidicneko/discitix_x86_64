#include <kernel/sched/scheduler.h>
#include <mm/pmm.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <drivers/pit.h>

static task_t *task_list = NULL;
static task_t *current = NULL;
static int next_task_id = 1;

void init_scheduler() {
    task_list = NULL;
    current = NULL;
}

static void sweep_wakeup(void) {
    if (!task_list) return;
    uint64_t ticks = get_ticks();
    task_t *t = task_list;
    do {
        if (t->state == TASK_BLOCKED && t->wake_tick <= ticks) {
            t->state = TASK_RUNNABLE;
            dbgln("SCHED: wake task id=%d\n\r", t->id);
        }
        t = t->next;
    } while (t != task_list);
}

task_t *get_current_task() { return current; }

static void task_enqueue(task_t *t) {
    if (!task_list) {
        task_list = t;
        t->next = t;
    } else {
        task_t *tail = task_list;
        while (tail->next != task_list)
            tail = tail->next;
        tail->next = t;
        t->next = task_list;
    }
}

static void prepare_initial_frame(task_t *t, void (*entry)(void *), void *arg) {
    memset(&t->regs, 0, sizeof(register_t));

    t->regs.rip = (uint64_t)entry;
    t->regs.cs = 0x08; 
    t->regs.rflags = 0x202; 
   
    uintptr_t stk_top = (uintptr_t)t->stack_base + t->stack_pages * 4096;
    
    stk_top &= ~0xFULL;
    
    stk_top -= 8; // for return address
    *(uint64_t *)stk_top = 0;
    stk_top -= 8; 

    t->regs.rsp = stk_top;
    t->regs.ss = 0x10; 

    t->regs.rdi = (uint64_t)arg;
}

task_t *create_task(void (*entry)(void *), void *arg, size_t stack_pages) {
    task_t *t = (task_t *)pmalloc(1);
    if (!t) return NULL;
    memset(t, 0, 4096);

    void *stack = pmalloc(stack_pages);
    if (!stack) {
        pmm_free_pages(t, 1);
        return NULL;
    }
    t->stack_base = stack;
    t->stack_pages = stack_pages;
    t->state = TASK_RUNNABLE;
    t->id = next_task_id++;
    t->wake_tick = 0;
    prepare_initial_frame(t, entry, arg);
    dbgln("SCHED: created task id=%d at 0x%xl stack=0x%xl pages=%d\n\r", t->id, t, 
        t->stack_base, (int)stack_pages);
    task_enqueue(t);
    return t;
}

void scheduler_sleep(uint64_t ticks) {
    task_t *c = get_current_task();
    if (!c) {
        uint64_t et = get_ticks() + ticks;
        while (get_ticks() < et) asm("hlt");
        return;
    }
    c->wake_tick = get_ticks() + ticks;
    c->state = TASK_BLOCKED;
    while (c->state == TASK_BLOCKED) asm("hlt");
}

void schedule_tick(register_t *regs) {
    if (!task_list) return;
    sweep_wakeup();
    if (!current) {
        task_t *main = (task_t *)pmalloc(1);
        if (!main) return;
        memset(main, 0, 4096);
        main->state = TASK_RUNNING;
        memcpy((uint8_t *)&main->regs, (const uint8_t *)regs, sizeof(register_t));
        task_enqueue(main);
        current = main;
        return;
    }

    memcpy((uint8_t *)&current->regs, (const uint8_t *)regs, sizeof(register_t));

    task_t *next = current->next;
    while (next && next->state != TASK_RUNNABLE) {
        if (next == current) break;
        next = next->next;
    }
    if (!next) return;

    // dbgln("SCHED: switch from id=%d to id=%d\n\r", current->id, next->id);
    current = next;

    memcpy((uint8_t *)regs, (const uint8_t *)&current->regs, sizeof(register_t));
}
