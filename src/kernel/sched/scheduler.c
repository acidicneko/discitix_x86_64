#include <kernel/sched/scheduler.h>
#include <kernel/sched/build_stack.h>
#include <kernel/elf.h>
#include <kernel/vfs/vfs.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/liballoc.h>
#include <arch/x86_64/gdt.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <drivers/pit.h>
#include <drivers/tty/tty.h>
#include <stdint.h>

task_t *task_list = NULL;
static task_t *current = NULL;
static int next_task_id = 1;

#define USER_CODE_VADDR  0x400000ULL

void init_scheduler() {
    task_list = NULL;
    current = NULL;
}

task_t *get_current_task() { 
    return current; 
}

task_t *find_task_by_id(int id) {
    if (!task_list) return NULL;
    task_t *t = task_list;
    do {
        if (t->id == id) return t;
        t = t->next;
    } while (t != task_list);
    return NULL;
}

void task_enqueue(task_t *t) {
    if (!task_list) {
        task_list = t;
        t->next = t;
    } else {
        task_t *tail = task_list;
        while (tail->next != task_list) {
            tail = tail->next;
        }
        tail->next = t;
        t->next = task_list;
    }
}


void task_exit() {
    current->state = TASK_ZOMBIE;
    log("SCHED",INFO,"task id=%d exited\n\r", current->id);
    
    if (current->is_usermode) {
        if (current->user_code) pmm_free_pages(current->user_code, current->user_code_pages);
        if (current->user_stack) pmm_free_pages(current->user_stack, 2); 
        if (current->cr3) vmm_free_user_page_table(current->cr3);
    }
    
    for (;;) asm volatile("hlt");
}

static void sweep_wakeup(void) {
    if (!task_list) return;
    uint64_t ticks = get_ticks();
    task_t *t = task_list;
    do {
        if (t->state == TASK_BLOCKED && t->sleeping && t->wake_tick <= ticks) {
            t->state = TASK_RUNNABLE;
            log("SCHED",INFO,"wake task id=%d\n\r", t->id);
        }
        t = t->next;
    } while (t != task_list);
}

void scheduler_sleep(uint64_t ticks) {
    task_t *c = get_current_task();
    if (!c) {
        uint64_t et = get_ticks() + ticks;
        while (get_ticks() < et) asm volatile("hlt");
        return;
    }
    c->wake_tick = get_ticks() + ticks;
    c->sleeping = 1;
    c->state = TASK_BLOCKED;
    while (c->state == TASK_BLOCKED) asm volatile("hlt");
    c->sleeping = 0;
}


void schedule_tick(register_t *regs) {
    if (!task_list) return;
    sweep_wakeup();
    
    if (!current) {
        task_t *main_task = (task_t *)kmalloc(sizeof(task_t));
        if (!main_task) return;
        memset(main_task, 0, sizeof(task_t));
        main_task->state = TASK_RUNNING;
        memcpy((uint8_t *)&main_task->regs, (const uint8_t *)regs, sizeof(register_t));
        task_enqueue(main_task);
        current = main_task;
        return;
    }

    memcpy((uint8_t *)&current->regs, (const uint8_t *)regs, sizeof(register_t));

    if (current->state == TASK_RUNNING) {
        current->state = TASK_RUNNABLE;
    }

    task_t *next = current->next;
    while (next && next->state != TASK_RUNNABLE) {
        if (next == current) {
            // Nothing else can run. Keep running current's context, but only
            // mark it RUNNING if it was runnable: a BLOCKED or ZOMBIE task
            // just keeps halting in its kernel loop with its state intact.
            if (current->state == TASK_RUNNABLE)
                current->state = TASK_RUNNING;
            return;
        }
        next = next->next;
    }
    
    current = next;
    current->state = TASK_RUNNING;
    
    if (current->is_usermode) {
        uint64_t kstack_top = (uint64_t)current->stack_base + current->stack_pages * 4096;
        tss_set_kernel_stack(kstack_top); 
        
        if (current->cr3 != 0) {
            vmm_switch_page_table(current->cr3);
        }
    } else {
        // Kernel tasks must not keep running on the last user task's CR3:
        // that task may exit and free its page table.
        vmm_switch_page_table(vmm_get_kernel_cr3());
    }

    memcpy((uint8_t *)regs, (const uint8_t *)&current->regs, sizeof(register_t));
}

static void setup_task_stdio(task_t *t) {
    tty_t* tty = get_current_tty();
    if (!tty) return;
    
    char tty_path[16] = "/dev/tty";
    char id_str[8]; 
    itoa(tty->id, id_str, 10);
    strcat(tty_path, id_str);
    
    inode_t *tty_inode = NULL;
    if (vfs_lookup_path(tty_path, &tty_inode) != 0 || !tty_inode) {
        log("SCHED",ERROR,"setup_task_stdio: Could not find %s", tty_path);
        return;
    }
    
    for (int fd = 0; fd < 3; fd++) {
        file_t *f = NULL;
        if (vfs_open(&f, tty_inode, 2) == 0 && f) {
            t->fd_table[fd] = f;
        }
    }
}
task_t *create_elf_task_args(const void *elf_data, size_t elf_size, size_t stack_pages,
                              int argc, char *argv[], int envc, char *envp[]) {
    uint64_t task_cr3 = vmm_create_user_page_table();
    if (task_cr3 == 0) return NULL;
    
    elf_info_t elf_info;
    if (elf_load_into(elf_data, elf_size, &elf_info, task_cr3) != 0) {
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    task_t *t = (task_t *)kmalloc(sizeof(task_t));
    void *kernel_stack = pmalloc(stack_pages);
    void *ustack = pmalloc(2); 
    
    if (!t || !kernel_stack || !ustack) {
        if (t) kfree(t);
        if (kernel_stack) pmm_free_pages(kernel_stack, stack_pages);
        if (ustack) pmm_free_pages(ustack, 2);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    memset(t, 0, sizeof(task_t));
    t->brk_start   = elf_info.end_addr;
    t->brk_current = elf_info.end_addr;

    // Build the stack contents before mapping, so a failure here only has to
    // free the unmapped pages.
    user_stack_result_t stack_res;
    if (build_user_stack((uint8_t*)ustack, USER_STACK_TOP_VADDR - USER_STACK_VADDR,
                          USER_STACK_VADDR, argc, argv, envc, envp, &stack_res) != 0) {
        kfree(t);
        pmm_free_pages(kernel_stack, stack_pages);
        pmm_free_pages(ustack, 2);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }

    // Stack occupies [USER_STACK_VADDR, USER_STACK_TOP_VADDR), same as sys_exec.
    uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
    void *ustack_phys1 = phys_from_virt(ustack);
    void *ustack_phys2 = phys_from_virt((void*)((uint64_t)ustack + 4096));
    
    if (vmm_map_page_in(task_cr3, (void*)USER_STACK_VADDR, ustack_phys1, user_flags) != 0 ||
        vmm_map_page_in(task_cr3, (void*)(USER_STACK_VADDR + 4096), ustack_phys2, user_flags) != 0) {
        // Unmap whatever got mapped so the page table free doesn't free it too.
        vmm_unmap_page_in(task_cr3, (void*)USER_STACK_VADDR);
        vmm_unmap_page_in(task_cr3, (void*)(USER_STACK_VADDR + 4096));
        kfree(t);
        pmm_free_pages(kernel_stack, stack_pages);
        pmm_free_pages(ustack, 2);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }

    t->stack_base = kernel_stack;
    t->stack_pages = stack_pages;
    t->state = TASK_RUNNABLE;
    t->id = next_task_id++;
    t->cr3 = task_cr3;
    t->is_usermode = 1;
    t->user_code = elf_info.pages;
    t->user_stack = ustack;
    t->user_code_pages = elf_info.num_pages;

    memset(&t->regs, 0, sizeof(register_t));
    t->regs.rip = elf_info.entry_point;
    t->regs.cs = 0x20 | 3;   
    t->regs.rflags = 0x202;
    t->regs.rsp = stack_res.rsp;
    t->regs.ss = 0x18 | 3;   
     
    t->regs.rdi = argc;         
    t->regs.rsi = stack_res.argv_uvaddr;
    t->regs.rdx = stack_res.envp_uvaddr;
    
    task_t *current_task = get_current_task();
    if (current_task && current_task->cwd) {
        t->cwd = current_task->cwd; 
    } else {
        t->cwd = (void*)vfs_get_root_dentry(); 
    }
     
    setup_task_stdio(t);
    task_enqueue(t);
    return t;
}
task_t *fork_current_task(register_t *parent_regs) {
    task_t *parent = current;
    if (!parent || !parent->is_usermode) return NULL;

    uint64_t child_cr3 = vmm_clone_user_page_table(parent->cr3);
    if (child_cr3 == 0) return NULL;

    task_t *child = (task_t *)kmalloc(sizeof(task_t));
    void *kernel_stack = pmalloc(parent->stack_pages);
    if (!child || !kernel_stack) {
        if (child) kfree(child);
        if (kernel_stack) pmm_free_pages(kernel_stack, parent->stack_pages);
        vmm_free_user_page_table(child_cr3);
        return NULL;
    }
    memset(child, 0, sizeof(task_t));

    elf_page_t *parent_pages = (elf_page_t *)parent->user_code;
    size_t meta_bytes = parent->user_code_pages * sizeof(elf_page_t);
    if (meta_bytes == 0) meta_bytes = sizeof(elf_page_t);
    elf_page_t *child_pages = (elf_page_t *)kmalloc(meta_bytes);
    if (!child_pages) {
        pmm_free_pages(kernel_stack, parent->stack_pages);
        kfree(child);
        vmm_free_user_page_table(child_cr3);
        return NULL;
    }
    memset(child_pages, 0, meta_bytes);

    for (size_t i = 0; i < parent->user_code_pages; i++) {
        uint64_t u_vaddr = parent_pages[i].user_vaddr;
        uint64_t pte = vmm_get_pte(child_cr3, u_vaddr);
        child_pages[i].user_vaddr   = u_vaddr;
        child_pages[i].kernel_vaddr = (pte & PTE_PRESENT)
            ? virt_from_phys((void*)(pte & 0x000ffffffffff000ULL)) : NULL;
    }

    // Informational only: the cloned stack frames aren't contiguous, and they
    // are freed with the page table, never through this pointer.
    uint64_t stack_pte = vmm_get_pte(child_cr3, USER_STACK_VADDR);
    void *child_user_stack = (stack_pte & PTE_PRESENT)
        ? virt_from_phys((void*)(stack_pte & 0x000ffffffffff000ULL)) : NULL;

    child->stack_base = kernel_stack;
    child->brk_start   = parent->brk_start;
    child->brk_current = parent->brk_current;
    child->mmap_base   = parent->mmap_base;
    child->stack_pages = parent->stack_pages;
    child->state = TASK_RUNNABLE;
    child->id = next_task_id++;
    child->parent_id = parent->id;
    child->cr3 = child_cr3;
    child->is_usermode = 1;
    child->user_code = child_pages;
    child->user_stack = child_user_stack;
    child->user_code_pages = parent->user_code_pages;
    child->cwd = parent->cwd;
    memcpy((uint8_t*)&child->regs, (const uint8_t*)parent_regs, sizeof(register_t));

    child->regs.rax = 0;

    // The child shares the parent's open files; each share is a reference.
    for (int i = 0; i < MAX_FDS; i++) {
        child->fd_table[i] = parent->fd_table[i];
        if (child->fd_table[i]) child->fd_table[i]->refcount++;
    }
    child->fd_cloexec = parent->fd_cloexec;

    log("SCHED",INFO, "forked task %d -> child %d with distinct CR3\n\r", parent->id, child->id);
    task_enqueue(child);

    return child;
}

// Unlink a zombie from the task list and free what sys_exit couldn't
// (its kernel stack and the task struct). Must not be called on `current`,
// and must not be called from IRQ context (kfree takes the allocator lock).
void task_reap(task_t *t) {
    if (!t || t == current || !task_list) return;

    if (t == task_list) {
        if (t->next == t) {
            task_list = NULL;
        } else {
            task_t *tail = task_list;
            while (tail->next != t) tail = tail->next;
            task_list = t->next;
            tail->next = task_list;
        }
    } else {
        task_t *prev = task_list;
        while (prev->next != t) {
            prev = prev->next;
            if (prev == task_list) return; // not in the list
        }
        prev->next = t->next;
    }

    if (t->stack_base) pmm_free_pages(t->stack_base, t->stack_pages);
    kfree(t);
}

// Free zombies nobody will wait for (parent_id 0: no parent, or reparented
// because the parent exited first).
void reap_orphan_zombies(void) {
    if (!task_list) return;
    int found;
    do {
        found = 0;
        task_t *t = task_list;
        do {
            if (t->state == TASK_ZOMBIE && t->parent_id == 0 && t != current) {
                task_reap(t);
                found = 1;
                break;  // list changed, rescan
            }
            t = t->next;
        } while (task_list && t != task_list);
    } while (found && task_list);
}

void sched_wakeup(void *chan) {
    if (!task_list) return;
    task_t *t = task_list;
    do {
        if (t->state == TASK_BLOCKED && t->wait_chan == chan) {
            t->wait_chan = NULL;
            t->state = TASK_RUNNABLE;
        }
        t = t->next;
    } while (t != task_list);
}
