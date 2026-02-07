#include <arch/x86_64/syscall.h>
#include <kernel/sched/scheduler.h>
#include <kernel/vfs/vfs.h>
#include <kernel/elf.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <libk/utils.h>
#include <libk/string.h>
#include <arch/x86_64/regs.h>
#include <stdint.h>

int64_t sys_exit(uint64_t status, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    dbgln("Process (task %d) called exit(%d)\n\r", 
          current ? current->id : -1, (int)status);
    
    if (current) {
        // Store exit status for waitpid
        current->exit_status = (int)status;
        
        // Wake up parent if it's waiting
        if (current->parent_id > 0) {
            task_t *parent = find_task_by_id(current->parent_id);
            if (parent && parent->state == TASK_BLOCKED) {
                parent->state = TASK_RUNNABLE;
            }
        }
        
        // Mark task as zombie - scheduler will clean it up
        current->state = TASK_ZOMBIE;
    }
    
    // Return from syscall - userspace will spin in a loop
    // Scheduler will see ZOMBIE state and not switch back to us
    return 0;
}

int64_t sys_fork(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    if (!current_syscall_regs) {
        dbgln("sys_fork: no regs available\n\r");
        return -1;
    }
    
    task_t *child = fork_current_task(current_syscall_regs);
    if (!child) {
        return -1;
    }
    
    // Parent returns child PID
    return child->id;
}

int64_t sys_exec(uint64_t path_ptr, uint64_t argv_ptr, uint64_t envp_ptr,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)envp_ptr; (void)arg4; (void)arg5; (void)arg6;
    
    const char *path = (const char*)path_ptr;
    char **argv = (char**)argv_ptr;
    
    task_t *current_task = get_current_task();
    if (!current_task || !path) return -1;
    
    dbgln("sys_exec: executing '%s'\n\r", path);
    
    // Look up executable in VFS
    inode_t *inode = NULL;
    if (vfs_lookup_path(path, &inode) != 0 || !inode) {
        dbgln("sys_exec: file not found: %s\n\r", path);
        return -1;
    }
    
    // Open and read the file
    file_t *file = NULL;
    if (vfs_open(&file, inode, 0) != 0 || !file) {
        dbgln("sys_exec: failed to open file\n\r");
        return -1;
    }
    
    // Get file size and allocate buffer
    size_t file_size = inode->size;
    void *elf_data = pmalloc((file_size + 4095) / 4096);
    if (!elf_data) {
        vfs_close(file);
        return -1;
    }
    
    // Read file contents
    long bytes_read = vfs_read(file, elf_data, file_size);
    vfs_close(file);
    
    if (bytes_read != (long)file_size) {
        pmm_free_pages(elf_data, (file_size + 4095) / 4096);
        dbgln("sys_exec: read failed\n\r");
        return -1;
    }
    
    // Count argc if argv provided
    int argc = 0;
    if (argv) {
        while (argv[argc]) argc++;
    }
    
    // Load ELF
    elf_info_t elf_info;
    if (elf_load(elf_data, file_size, &elf_info) != 0) {
        pmm_free_pages(elf_data, (file_size + 4095) / 4096);
        dbgln("sys_exec: ELF load failed\n\r");
        return -1;
    }
    
    pmm_free_pages(elf_data, (file_size + 4095) / 4096);
    
    // Free old user code pages (if any)
    // Note: in a proper implementation, we'd properly clean up all old mappings
    
    // Set up new user stack
    void *new_stack = pmalloc(2);
    if (!new_stack) {
        elf_free(&elf_info);
        return -1;
    }
    
    // Map new stack
    uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
    void *stack_phys = phys_from_virt(new_stack);
    void *stack_phys2 = phys_from_virt((void*)((uint64_t)new_stack + 4096));
    vmm_map_page(new_stack, stack_phys, user_flags);
    vmm_map_page((void*)((uint64_t)new_stack + 4096), stack_phys2, user_flags);
    
    // Copy arguments to new stack
    uint64_t stack_top = (uint64_t)new_stack + 8192;
    uint8_t *kstack = (uint8_t*)new_stack;
    
    uint64_t str_ptr = stack_top;
    uint64_t arg_ptrs[64];
    
    for (int i = 0; i < argc && i < 64; i++) {
        size_t len = strlen(argv[i]) + 1;
        str_ptr -= len;
        memcpy(kstack + (str_ptr - (uint64_t)new_stack), (const uint8_t*)argv[i], len);
        arg_ptrs[i] = str_ptr;
    }
    
    str_ptr &= ~7ULL;
    str_ptr -= 8;
    *(uint64_t*)(kstack + (str_ptr - (uint64_t)new_stack)) = 0;  // NULL terminator
    
    for (int i = argc - 1; i >= 0; i--) {
        str_ptr -= 8;
        *(uint64_t*)(kstack + (str_ptr - (uint64_t)new_stack)) = arg_ptrs[i];
    }
    
    uint64_t argv_addr = str_ptr;
    str_ptr &= ~0xFULL;
    
    // Update task
    current_task->user_code = elf_info.pages;
    current_task->user_code_pages = elf_info.num_pages;
    if (current_task->user_stack) {
        pmm_free_pages(current_task->user_stack, 2);
    }
    current_task->user_stack = new_stack;
    
    // Set up new register state - this will take effect when returning from syscall
    current_syscall_regs->rip = elf_info.entry_point;
    current_syscall_regs->rsp = str_ptr;
    current_syscall_regs->rdi = argc;
    current_syscall_regs->rsi = argv_addr;
    current_syscall_regs->rax = 0;  // exec doesn't return on success
    
    dbgln("sys_exec: loaded '%s', entry=0x%xl\n\r", path, elf_info.entry_point);
    
    return 0;  // Won't actually return - registers are overwritten
}

int64_t sys_waitpid(uint64_t pid, uint64_t status_ptr, uint64_t options,
                    uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current_task = get_current_task();
    if (!current_task) return -1;
    
    int *status = (int*)status_ptr;
    int nohang = options & 1;  // WNOHANG
    
    // If pid > 0, wait for specific child
    if (pid <= 0) {
        // TODO: implement waiting for any child
        return -1;
    }
    
    task_t *child = find_task_by_id((int)pid);
    if (!child || child->parent_id != current_task->id) {
        dbgln("waitpid: task %d is not our child\n\r", (int)pid);
        return -1;  // Not our child
    }
    
    // Check if child is zombie
    if (child->state == TASK_ZOMBIE) {
        int child_id = child->id;
        if (status) {
            *status = child->exit_status;
        }
        dbgln("waitpid: collected child %d, status=%d\n\r", child_id, child->exit_status);
        return child_id;
    }
    
    // Child not done yet
    if (nohang) {
        return 0;  // WNOHANG: return 0 if child not done
    }
    
    // Blocking wait: return -2 to indicate "try again"
    // Userspace should loop
    return -2;
}

int64_t sys_spawn(uint64_t path_ptr, uint64_t argv_ptr, uint64_t envp_ptr,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)envp_ptr; (void)arg4; (void)arg5; (void)arg6;
    
    const char *path = (const char*)path_ptr;
    char **argv = (char**)argv_ptr;
    
    task_t *parent = get_current_task();
    if (!path) return -1;
    
    dbgln("sys_spawn: spawning '%s'\n\r", path);
    
    // Count argc and print args for debugging
    int argc = 0;
    if (argv) {
        while (argv[argc]) {
            dbgln("sys_spawn: argv[%d] = '%s'\n\r", argc, argv[argc]);
            argc++;
        }
    }
    dbgln("sys_spawn: argc = %d\n\r", argc);
    
    // Look up executable in VFS
    inode_t *inode = NULL;
    if (vfs_lookup_path(path, &inode) != 0 || !inode) {
        dbgln("sys_spawn: file not found: %s\n\r", path);
        return -1;
    }
    
    // Open and read the file
    file_t *file = NULL;
    if (vfs_open(&file, inode, 0) != 0 || !file) {
        dbgln("sys_spawn: failed to open file\n\r");
        return -1;
    }
    
    // Get file size and allocate buffer
    size_t file_size = inode->size;
    size_t file_pages = (file_size + 4095) / 4096;
    void *elf_data = pmalloc(file_pages);
    if (!elf_data) {
        vfs_close(file);
        return -1;
    }
    
    // Read file contents
    long bytes_read = vfs_read(file, elf_data, file_size);
    vfs_close(file);
    
    if (bytes_read != (long)file_size) {
        pmm_free_pages(elf_data, file_pages);
        dbgln("sys_spawn: read failed\n\r");
        return -1;
    }
    
    // Create the new task from ELF
    task_t *child = create_elf_task_args(elf_data, file_size, 4, argc, argv);
    pmm_free_pages(elf_data, file_pages);
    
    if (!child) {
        dbgln("sys_spawn: failed to create task\n\r");
        return -1;
    }
    
    // Set parent relationship
    child->parent_id = parent ? parent->id : 0;
    
    dbgln("sys_spawn: created child %d for '%s'\n\r", child->id, path);
    return child->id;
}
