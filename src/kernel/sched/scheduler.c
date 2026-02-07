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

static task_t *task_list = NULL;
static task_t *current = NULL;
static int next_task_id = 1;

// Helper to set up stdin/stdout/stderr for a new task
static void setup_task_stdio(task_t *t) {
    // Get current TTY and build path
    tty_t* tty = get_current_tty();
    if (!tty) {
        dbgln("SCHED: No current TTY for stdio setup\n\r");
        return;
    }
    
    char tty_path[8] = "/tty";
    char id_str[2];
    itoa(tty->id, id_str, 10);
    strcat(tty_path, id_str);
    
    inode_t *tty_inode = NULL;
    if (vfs_lookup_path(tty_path, &tty_inode) != 0 || !tty_inode) {
        dbgln("SCHED: Failed to find %s\n\r", tty_path);
        return;
    }
    
    // Open the TTY 3 times for stdin, stdout, stderr
    for (int fd = 0; fd < 3; fd++) {
        file_t *f = NULL;
        if (vfs_open(&f, tty_inode, 0) != 0 || !f) {
            dbgln("SCHED: Failed to open %s for fd %d\n\r", tty_path, fd);
            continue;
        }
        t->fd_table[fd] = f;
    }
    
    dbgln("SCHED: Opened %s as stdin/stdout/stderr for task %d\n\r", tty_path, t->id);
}

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
    t->user_code_pages = 0;
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
    t->user_code_pages = 1;
    
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

task_t *create_elf_task(const void *elf_data, size_t elf_size, size_t stack_pages) {
    // Create per-process page table
    uint64_t task_cr3 = vmm_create_user_page_table();
    if (task_cr3 == 0) {
        dbgln("SCHED: Failed to create page table for ELF task\n\r");
        return NULL;
    }
    
    // Load the ELF file into the new page table
    elf_info_t elf_info;
    if (elf_load_into(elf_data, elf_size, &elf_info, task_cr3) != 0) {
        dbgln("SCHED: Failed to load ELF\n\r");
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    task_t *t = (task_t *)pmalloc(1);
    if (!t) {
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    memset(t, 0, 4096);
    
    // Allocate kernel stack (for syscalls/interrupts)
    void *kernel_stack = pmalloc(stack_pages);
    if (!kernel_stack) {
        pmm_free_pages(t, 1);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    // Allocate user stack
    void *ustack = pmalloc(1);
    if (!ustack) {
        pmm_free_pages(kernel_stack, stack_pages);
        pmm_free_pages(t, 1);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    // Map user stack as user-accessible in the process's page table
    uint64_t user_stack_vaddr = 0x7FFFFF000000ULL;  // High user-space address
    uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
    void *ustack_phys = phys_from_virt(ustack);
    if (vmm_map_page_in(task_cr3, (void*)user_stack_vaddr, ustack_phys, user_flags) != 0) {
        pmm_free_pages(ustack, 1);
        pmm_free_pages(kernel_stack, stack_pages);
        pmm_free_pages(t, 1);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    t->stack_base = kernel_stack;
    t->stack_pages = stack_pages;
    t->state = TASK_RUNNABLE;
    t->id = next_task_id++;
    t->wake_tick = 0;
    t->cr3 = task_cr3;  // Per-process page table
    t->is_usermode = 1;
    t->user_code = elf_info.pages;  // Store page tracking array
    t->user_stack = ustack;
    t->user_code_pages = elf_info.num_pages;
    
    // Set up usermode register frame
    memset(&t->regs, 0, sizeof(register_t));
    t->regs.rip = elf_info.entry_point;
    t->regs.cs = 0x20 | 3;   // GDT_USER_CODE | RPL 3
    t->regs.rflags = 0x202;  // IF enabled
    
    // User stack pointer (top of page, 16-byte aligned)
    uint64_t user_stack_top = (user_stack_vaddr + 4096) & ~0xFULL;
    t->regs.rsp = user_stack_top;
    t->regs.ss = 0x18 | 3;   // GDT_USER_DATA | RPL 3
    
    // Default: argc=0, argv=NULL
    t->regs.rdi = 0;  // argc
    t->regs.rsi = 0;  // argv
    
    // Set up stdin/stdout/stderr
    setup_task_stdio(t);
    
    dbgln("SCHED: created ELF task id=%d cr3=0x%xl entry=0x%xl ustack=0x%xl kstack=0x%xl pages=%d\n\r", 
        t->id, task_cr3, elf_info.entry_point, user_stack_top, (uint64_t)kernel_stack, (int)elf_info.num_pages);
    task_enqueue(t);
    return t;
}

// Create ELF task with command-line arguments
task_t *create_elf_task_args(const void *elf_data, size_t elf_size, size_t stack_pages,
                              int argc, char *argv[]) {
    // Create per-process page table
    uint64_t task_cr3 = vmm_create_user_page_table();
    if (task_cr3 == 0) {
        dbgln("SCHED: Failed to create page table for ELF task\n\r");
        return NULL;
    }
    
    // Load the ELF file into the new page table
    elf_info_t elf_info;
    if (elf_load_into(elf_data, elf_size, &elf_info, task_cr3) != 0) {
        dbgln("SCHED: Failed to load ELF\n\r");
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    task_t *t = (task_t *)pmalloc(1);
    if (!t) {
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    memset(t, 0, 4096);
    
    // Allocate kernel stack (for syscalls/interrupts)
    void *kernel_stack = pmalloc(stack_pages);
    if (!kernel_stack) {
        pmm_free_pages(t, 1);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    // Allocate user stack (2 pages for args)
    void *ustack = pmalloc(2);
    if (!ustack) {
        pmm_free_pages(kernel_stack, stack_pages);
        pmm_free_pages(t, 1);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    // Map user stack pages as user-accessible in the process's page table
    // User stack will be at a fixed virtual address (e.g., 0x7FFFFF000000)
    uint64_t user_stack_vaddr = 0x7FFFFF000000ULL;  // High user-space address
    uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
    void *ustack_phys = phys_from_virt(ustack);
    void *ustack_phys2 = phys_from_virt((void*)((uint64_t)ustack + 4096));
    
    // Map both pages into the process's page table
    if (vmm_map_page_in(task_cr3, (void*)user_stack_vaddr, ustack_phys, user_flags) != 0 ||
        vmm_map_page_in(task_cr3, (void*)(user_stack_vaddr + 4096), ustack_phys2, user_flags) != 0) {
        pmm_free_pages(ustack, 2);
        pmm_free_pages(kernel_stack, stack_pages);
        pmm_free_pages(t, 1);
        elf_free(&elf_info);
        vmm_free_user_page_table(task_cr3);
        return NULL;
    }
    
    // Set up arguments on user stack (using kernel virtual address)
    // Stack layout (grows down):
    //   [strings: "arg0\0arg1\0..."]
    //   [argv[n] = NULL]
    //   [argv[n-1] = ptr to argn-1]
    //   ...
    //   [argv[0] = ptr to arg0]  <-- argv points here
    //   [argc]                    <-- not needed, passed in rdi
    //   [return addr = 0]         <-- rsp points here
    
    uint64_t stack_top = user_stack_vaddr + 8192;  // 2 pages (user virtual address)
    uint8_t *kstack = (uint8_t*)ustack;  // Kernel vaddr for writing
    
    // First, copy all argument strings to the top of the stack
    uint64_t str_ptr = stack_top;
    uint64_t arg_ptrs[64];  // Max 64 args
    
    for (int i = 0; i < argc && i < 64; i++) {
        size_t len = strlen(argv[i]) + 1;
        str_ptr -= len;
        // Copy string using kernel address - offset from user_stack_vaddr
        memcpy(kstack + (str_ptr - user_stack_vaddr), (const uint8_t*)argv[i], len);
        arg_ptrs[i] = str_ptr;  // User-space pointer
    }
    
    // Align to 8 bytes
    str_ptr &= ~7ULL;
    
    // Push NULL terminator for argv
    str_ptr -= 8;
    *(uint64_t*)(kstack + (str_ptr - user_stack_vaddr)) = 0;
    
    // Push argv pointers in reverse order
    for (int i = argc - 1; i >= 0; i--) {
        str_ptr -= 8;
        *(uint64_t*)(kstack + (str_ptr - user_stack_vaddr)) = arg_ptrs[i];
    }
    
    uint64_t argv_ptr = str_ptr;  // This is where argv points (user virtual address)
    
    // Debug: print what we set up
    dbgln("SCHED: argv setup: argc=%d argv_ptr=0x%xl\n\r", argc, argv_ptr);
    for (int i = 0; i < argc; i++) {
        dbgln("SCHED:   argv[%d] ptr=0x%xl\n\r", i, arg_ptrs[i]);
    }
    
    // Align stack to 16 bytes
    str_ptr &= ~0xFULL;
    
    t->stack_base = kernel_stack;
    t->stack_pages = stack_pages;
    t->state = TASK_RUNNABLE;
    t->id = next_task_id++;
    t->wake_tick = 0;
    t->cr3 = task_cr3;  // Per-process page table
    t->is_usermode = 1;
    t->user_code = elf_info.pages;
    t->user_stack = ustack;
    t->user_code_pages = elf_info.num_pages;
    
    // Set up usermode register frame
    memset(&t->regs, 0, sizeof(register_t));
    t->regs.rip = elf_info.entry_point;
    t->regs.cs = 0x20 | 3;   // GDT_USER_CODE | RPL 3
    t->regs.rflags = 0x202;  // IF enabled
    t->regs.rsp = str_ptr;
    t->regs.ss = 0x18 | 3;   // GDT_USER_DATA | RPL 3
    
    // Pass argc and argv via registers (System V ABI)
    t->regs.rdi = argc;       // First argument: argc
    t->regs.rsi = argv_ptr;   // Second argument: argv
    
    // Set up stdin/stdout/stderr
    setup_task_stdio(t);
    
    dbgln("SCHED: created ELF task id=%d cr3=0x%xl argc=%d rdi=0x%xl rsi=0x%xl rsp=0x%xl\n\r", 
        t->id, task_cr3, argc, t->regs.rdi, t->regs.rsi, t->regs.rsp);
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
        
        // Switch to process's page table
        if (current->cr3 != 0) {
            vmm_switch_page_table(current->cr3);
        }
    }

    memcpy((uint8_t *)regs, (const uint8_t *)&current->regs, sizeof(register_t));
}

// Find a task by its ID
task_t *find_task_by_id(int id) {
    if (!task_list) return NULL;
    task_t *t = task_list;
    do {
        if (t->id == id) return t;
        t = t->next;
    } while (t != task_list);
    return NULL;
}

// Fork the current task - creates a copy with same state
// Returns the new child task (caller sets return values appropriately)
task_t *fork_current_task(register_t *parent_regs) {
    task_t *parent = current;
    if (!parent || !parent->is_usermode) {
        dbgln("SCHED: fork failed - no usermode parent\n\r");
        return NULL;
    }
    
    // Allocate child task struct
    task_t *child = (task_t *)pmalloc(1);
    if (!child) return NULL;
    memset(child, 0, 4096);
    
    // Allocate kernel stack for child
    void *kernel_stack = pmalloc(parent->stack_pages);
    if (!kernel_stack) {
        pmm_free_pages(child, 1);
        return NULL;
    }
    
    // Allocate user stack for child (2 pages like parent)
    void *user_stack = pmalloc(2);
    if (!user_stack) {
        pmm_free_pages(kernel_stack, parent->stack_pages);
        pmm_free_pages(child, 1);
        return NULL;
    }
    
    // Copy parent's user stack contents
    memcpy(user_stack, parent->user_stack, 8192);
    
    // Map child's user stack
    uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
    void *ustack_phys = phys_from_virt(user_stack);
    void *ustack_phys2 = phys_from_virt((void*)((uint64_t)user_stack + 4096));
    
    if (vmm_map_page(user_stack, ustack_phys, user_flags) != 0 ||
        vmm_map_page((void*)((uint64_t)user_stack + 4096), ustack_phys2, user_flags) != 0) {
        pmm_free_pages(user_stack, 2);
        pmm_free_pages(kernel_stack, parent->stack_pages);
        pmm_free_pages(child, 1);
        return NULL;
    }
    
    // Set up child task structure
    child->stack_base = kernel_stack;
    child->stack_pages = parent->stack_pages;
    child->state = TASK_RUNNABLE;
    child->id = next_task_id++;
    child->parent_id = parent->id;
    child->wake_tick = 0;
    child->exit_status = 0;
    child->is_usermode = 1;
    child->user_code = parent->user_code;  // Share code pages (read-only)
    child->user_stack = user_stack;
    child->user_code_pages = parent->user_code_pages;
    
    // Copy register state from parent
    memcpy((uint8_t*)&child->regs, (const uint8_t*)parent_regs, sizeof(register_t));
    
    // Child's stack pointer needs adjustment to point to new stack
    // Calculate offset within parent's stack
    uint64_t parent_stack_base = (uint64_t)parent->user_stack;
    uint64_t child_stack_base = (uint64_t)user_stack;
    uint64_t stack_offset = child->regs.rsp - parent_stack_base;
    child->regs.rsp = child_stack_base + stack_offset;
    
    // Child returns 0 from fork
    child->regs.rax = 0;
    
    // Copy file descriptor table (share the same file objects)
    for (int i = 0; i < MAX_FDS; i++) {
        child->fd_table[i] = parent->fd_table[i];
        // Note: In a real OS, we'd increment reference counts on files
    }
    
    dbgln("SCHED: forked task %d -> child %d\n\r", parent->id, child->id);
    task_enqueue(child);
    
    return child;
}
