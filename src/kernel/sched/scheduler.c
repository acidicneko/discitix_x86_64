#include <kernel/sched/scheduler.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <arch/x86_64/gdt.h>
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

static void cleanup_zombies() {
    if (!task_list) return;
    task_t *t = task_list;
    do {
        task_t *next = t->next;
        if (t->state == TASK_ZOMBIE) {
            // Remove from list
            if (t == task_list) {
                if (t->next == t) {
                    task_list = NULL;
                } else {
                    task_list = t->next;
                    task_t *tail = task_list;
                    while (tail->next != t) tail = tail->next;
                    tail->next = task_list;
                }
            } else {
                task_t *prev = task_list;
                while (prev->next != t) prev = prev->next;
                prev->next = t->next;
                if (t == task_list) task_list = t->next;
            }
            // Free memory
            pmm_free_pages(t->stack_base, t->stack_pages);
            pmm_free_pages(t, 1);
            dbgln("SCHED: cleaned up task id=%d\n\r", t->id);
        }
        t = next;
    } while (t != task_list && task_list);
}

static void task_exit() {
    current->state = TASK_ZOMBIE;
    dbgln("SCHED: task id=%d exited\n\r", current->id);
    // Free usermode memory if applicable
    if (current->is_usermode) {
        if (current->user_code) pmm_free_pages(current->user_code, 1);
        if (current->user_stack) pmm_free_pages(current->user_stack, 1);
    }
    cleanup_zombies();
    for (;;) asm("hlt");
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
    *(uint64_t *)stk_top = (uint64_t)task_exit;

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
    t->is_usermode = 0;
    t->user_code = NULL;
    t->user_stack = NULL;
    prepare_initial_frame(t, entry, arg);
    dbgln("SCHED: created task id=%d at 0x%xl stack=0x%xl pages=%d\n\r", t->id, t, 
        t->stack_base, (int)stack_pages);
    task_enqueue(t);
    return t;
}

task_t *create_user_task(void *user_code, size_t code_size, size_t stack_pages) {
    task_t *t = (task_t *)pmalloc(1);
    if (!t) return NULL;
    memset(t, 0, 4096);

    // Allocate kernel stack (for syscalls/interrupts)
    void *kernel_stack = pmalloc(stack_pages);
    if (!kernel_stack) {
        pmm_free_pages(t, 1);
        return NULL;
    }
    
    // Allocate user code page
    void *ucode = pmalloc(1);
    if (!ucode) {
        pmm_free_pages(kernel_stack, stack_pages);
        pmm_free_pages(t, 1);
        return NULL;
    }
    
    // Allocate user stack page
    void *ustack = pmalloc(1);
    if (!ustack) {
        pmm_free_pages(ucode, 1);
        pmm_free_pages(kernel_stack, stack_pages);
        pmm_free_pages(t, 1);
        return NULL;
    }
    
    // Copy user code
    memcpy((uint8_t*)ucode, (const uint8_t*)user_code, code_size);
    
    // Map user code and stack as user-accessible
    uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
    void *ucode_phys = phys_from_virt(ucode);
    void *ustack_phys = phys_from_virt(ustack);
    
    if (vmm_map_page(ucode, ucode_phys, user_flags) != 0) {
        pmm_free_pages(ustack, 1);
        pmm_free_pages(ucode, 1);
        pmm_free_pages(kernel_stack, stack_pages);
        pmm_free_pages(t, 1);
        return NULL;
    }
    
    if (vmm_map_page(ustack, ustack_phys, user_flags) != 0) {
        pmm_free_pages(ustack, 1);
        pmm_free_pages(ucode, 1);
        pmm_free_pages(kernel_stack, stack_pages);
        pmm_free_pages(t, 1);
        return NULL;
    }
    
    t->stack_base = kernel_stack;
    t->stack_pages = stack_pages;
    t->state = TASK_RUNNABLE;
    t->id = next_task_id++;
    t->wake_tick = 0;
    t->is_usermode = 1;
    t->user_code = ucode;
    t->user_stack = ustack;
    
    // Set up usermode register frame
    memset(&t->regs, 0, sizeof(register_t));
    t->regs.rip = (uint64_t)ucode;
    t->regs.cs = 0x20 | 3;   // GDT_USER_CODE | RPL 3
    t->regs.rflags = 0x202;  // IF enabled
    
    // User stack pointer (top of page, 16-byte aligned)
    uint64_t user_stack_top = ((uint64_t)ustack + 4096) & ~0xFULL;
    t->regs.rsp = user_stack_top;
    t->regs.ss = 0x18 | 3;   // GDT_USER_DATA | RPL 3
    
    dbgln("SCHED: created user task id=%d code=0x%xl ustack=0x%xl kstack=0x%xl\n\r", 
        t->id, (uint64_t)ucode, user_stack_top, (uint64_t)kernel_stack);
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

    // Mark current task as runnable (unless it's blocked/zombie)
    if (current->state == TASK_RUNNING) {
        current->state = TASK_RUNNABLE;
    }

    task_t *next = current->next;
    while (next && next->state != TASK_RUNNABLE) {
        if (next == current) {
            // No other runnable task, keep running current
            current->state = TASK_RUNNING;
            return;
        }
        next = next->next;
    }
    if (!next || next == current) {
        current->state = TASK_RUNNING;
        return;
    }

    // dbgln("SCHED: switch from id=%d to id=%d\n\r", current->id, next->id);
    current = next;
    current->state = TASK_RUNNING;
    
    // Update TSS kernel stack for usermode tasks
    if (current->is_usermode) {
        uint64_t kstack_top = (uint64_t)current->stack_base + current->stack_pages * 4096;
        tss_set_kernel_stack(kstack_top);
    }

    memcpy((uint8_t *)regs, (const uint8_t *)&current->regs, sizeof(register_t));
}
