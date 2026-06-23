#include <kernel/sched/scheduler.h>
#include <kernel/elf.h>
#include <kernel/vfs/vfs.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <arch/x86_64/gdt.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <drivers/pit.h>
#include <drivers/tty/tty.h>
#include <stdint.h>

task_t *task_list = NULL;
static task_t *current = NULL;
static int next_task_id = 1;

#define USER_STACK_VADDR 0x7FFFFF000000ULL
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
    dbgln("SCHED: task id=%d exited\n\r", current->id);
    
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
        if (t->state == TASK_BLOCKED && t->wake_tick <= ticks) {
            t->state = TASK_RUNNABLE;
            dbgln("SCHED: wake task id=%d\n\r", t->id);
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
    c->state = TASK_BLOCKED;
    while (c->state == TASK_BLOCKED) asm volatile("hlt");
}


void schedule_tick(register_t *regs) {
    if (!task_list) return;
    sweep_wakeup();
    
    if (!current) {
        task_t *main_task = (task_t *)pmalloc(1);
        if (!main_task) return;
        memset(main_task, 0, 4096);
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
    }

    memcpy((uint8_t *)regs, (const uint8_t *)&current->regs, sizeof(register_t));
}

static void setup_task_stdio(task_t *t) {
    tty_t* tty = get_current_tty();
    if (!tty) return;
    
    char tty_path[16] = "/dev/tty";
    char id_str[2];
    itoa(tty->id, id_str, 10);
    strcat(tty_path, id_str);
    
    inode_t *tty_inode = NULL;
    if (vfs_lookup_path(tty_path, &tty_inode) != 0 || !tty_inode) return;
    
    for (int fd = 0; fd < 3; fd++) {
        file_t *f = NULL;
        if (vfs_open(&f, tty_inode, 0) == 0 && f) {
            t->fd_table[fd] = f;
        }
    }
}

task_t *create_elf_task_args(const void *elf_data, size_t elf_size, size_t stack_pages, int argc, char *argv[]) {
    uint64_t task_cr3 = vmm_create_user_page_table();
    if (task_cr3 == 0) return NULL;
    
    elf_info_t elf_info;
    if (elf_load_into(elf_data, elf_size, &elf_info, task_cr3) != 0) {
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    task_t *t = (task_t *)pmalloc(1);
    void *kernel_stack = pmalloc(stack_pages);
    void *ustack = pmalloc(2); 
    
    if (!t || !kernel_stack || !ustack) {
        if (t) pmm_free_pages(t, 1);
        if (kernel_stack) pmm_free_pages(kernel_stack, stack_pages);
        if (ustack) pmm_free_pages(ustack, 2);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    memset(t, 0, 4096);
    
    uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
    void *ustack_phys1 = phys_from_virt(ustack);
    void *ustack_phys2 = phys_from_virt((void*)((uint64_t)ustack + 4096));
    
    if (vmm_map_page_in(task_cr3, (void*)USER_STACK_VADDR, ustack_phys1, user_flags) != 0 ||
        vmm_map_page_in(task_cr3, (void*)(USER_STACK_VADDR + 4096), ustack_phys2, user_flags) != 0) {
        // [Cleanup omitted for brevity...]
        if (t) pmm_free_pages(t, 1);
        if (kernel_stack) pmm_free_pages(kernel_stack, stack_pages);
        if (ustack) pmm_free_pages(ustack, 2);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    uint64_t offset = 8192; // Start exactly at the top of the 2-page stack block
    uint8_t *kstack_base = (uint8_t*)ustack;
    uint64_t arg_user_ptrs[64];
    
    // 1. Push Strings
    for (int i = 0; i < argc && i < 64; i++) {
        size_t len = strlen(argv[i]) + 1;
        offset -= len;
        memcpy(kstack_base + offset, (uint8_t*)argv[i], len);
        arg_user_ptrs[i] = USER_STACK_VADDR + offset;
    }
    
    // 2. Align & Push Array
    offset &= ~7ULL; // 8-byte boundary
    offset -= 8;
    *(uint64_t*)(kstack_base + offset) = 0; // NULL terminator
    
    for (int i = argc - 1; i >= 0; i--) {
        offset -= 8;
        *(uint64_t*)(kstack_base + offset) = arg_user_ptrs[i];
    }
    
    uint64_t argv_user_vaddr = USER_STACK_VADDR + offset;
    
    // 3. Align RSP to strict 16-byte boundary
    offset &= ~0xFULL; 
    uint64_t final_rsp = USER_STACK_VADDR + offset;

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
    t->regs.rsp = final_rsp;        // SAFE, PROTECTED STACK POINTER!
    t->regs.ss = 0x18 | 3;   
    
    t->regs.rdi = argc;         
    t->regs.rsi = argv_user_vaddr;  // CORRECTLY CALCULATED USER POINTER!
    
    setup_task_stdio(t);
    task_enqueue(t);
    return t;
}

task_t *fork_current_task(register_t *parent_regs) {
    task_t *parent = current;
    if (!parent || !parent->is_usermode) return NULL;
    
    uint64_t child_cr3 = vmm_create_user_page_table();
    if (child_cr3 == 0) return NULL;
    
    task_t *child = (task_t *)pmalloc(1);
    void *kernel_stack = pmalloc(parent->stack_pages);
    void *child_user_stack = pmalloc(2); 

    if (!child || !kernel_stack || !child_user_stack) {
        if (child) pmm_free_pages(child, 1);
        if (kernel_stack) pmm_free_pages(kernel_stack, parent->stack_pages);
        if (child_user_stack) pmm_free_pages(child_user_stack, 2);
        vmm_free_user_page_table(child_cr3);
        return NULL;
    }
    memset(child, 0, 4096);
    
    memcpy(child_user_stack, parent->user_stack, 8192);
    
    uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
    void *ustack_phys1 = phys_from_virt(child_user_stack);
    void *ustack_phys2 = phys_from_virt((void*)((uint64_t)child_user_stack + 4096));
    
    if (vmm_map_page_in(child_cr3, (void*)USER_STACK_VADDR, ustack_phys1, user_flags) != 0 ||
        vmm_map_page_in(child_cr3, (void*)(USER_STACK_VADDR + 4096), ustack_phys2, user_flags) != 0) {
        pmm_free_pages(child_user_stack, 2);
        pmm_free_pages(kernel_stack, parent->stack_pages);
        pmm_free_pages(child, 1);
        vmm_free_user_page_table(child_cr3);
        return NULL;
    }
    
    elf_page_t *parent_pages = (elf_page_t *)parent->user_code;
    
    elf_page_t *child_pages = (elf_page_t *)pmalloc(1);
    if (!child_pages) {
        // TODO: Handle severe out-of-memory error
        return NULL;
    }
    memset(child_pages, 0, 4096);
    
    // Iterate through every single ELF page the parent owns
    for (size_t i = 0; i < parent->user_code_pages; i++) {
        uint64_t u_vaddr = parent_pages[i].user_vaddr;
        void *p_kaddr = parent_pages[i].kernel_vaddr;
        
        // Allocate a completely new, unique physical page for the child
        void *c_kaddr = pmalloc(1);
        if (!c_kaddr) continue; 
        
        // Deep copy the actual executable machine code / data into the child's page
        memcpy(c_kaddr, p_kaddr, 4096);
        
        // Save the metadata so the kernel can free it later when the child dies
        child_pages[i].user_vaddr = u_vaddr;
        child_pages[i].kernel_vaddr = c_kaddr;
        
        // Map the executable page into the child's CR3
        void *c_phys = phys_from_virt(c_kaddr);
        vmm_map_page_in(child_cr3, (void*)u_vaddr, c_phys, PTE_PRESENT | PTE_RW | PTE_USER); 
    }
    
    child->stack_base = kernel_stack;
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
    
    for (int i = 0; i < MAX_FDS; i++) {
        child->fd_table[i] = parent->fd_table[i];
    }
    
    dbgln("SCHED: forked task %d -> child %d with distinct CR3\n\r", parent->id, child->id);
    task_enqueue(child);
    
    return child;
}

