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

#define USER_STACK_TOP_VADDR 0x7FFFF0000000ULL 
#define USER_STACK_SIZE      8192 // 2 Pages

extern task_t *task_list;

int64_t sys_exit(uint64_t status, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    if (!current) return -1;

    log("SYS_EXIT",INFO,"Process (task %d) called exit(%d)\n\r", current->id, (int)status);
    current->exit_status = (int)status;
    
    if (current->parent_id > 0) {
        task_t *parent = find_task_by_id(current->parent_id);
        if (parent && parent->state == TASK_BLOCKED) {
            parent->state = TASK_RUNNABLE;
        }
    }
    
    current->state = TASK_ZOMBIE;
        
    if (current->is_usermode) {
        if (current->user_code) {
            elf_page_t *pages = (elf_page_t *)current->user_code;
            for (size_t i = 0; i < current->user_code_pages; i++) {
                if (pages[i].kernel_vaddr) {
                    pmm_free_pages(pages[i].kernel_vaddr, 1);
                }
            }
            size_t meta_pages = (current->user_code_pages * sizeof(elf_page_t) + 4095) / 4096;
            if (meta_pages == 0) meta_pages = 1;
            pmm_free_pages(current->user_code, meta_pages);
        }
        
        if (current->user_stack) pmm_free_pages(current->user_stack, 2); 
        if (current->cr3) vmm_free_user_page_table(current->cr3);
        
        current->user_code = NULL;
        current->user_stack = NULL;
        current->cr3 = 0;
    }
    for (;;) {
        asm volatile("sti; hlt");
    }
    
    return 0; 
}

int64_t sys_waitpid(uint64_t pid, uint64_t status_ptr, uint64_t options,
                    uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current_task = get_current_task();
    if (!current_task) return -1;
    
    int *status = (int*)status_ptr;
    int nohang = options & 1; 
    
    if (pid <= 0) return -1; 
    
    task_t *child = find_task_by_id((int)pid);
    if (!child || child->parent_id != current_task->id) {
        return -1; 
    }
    
    if (child->state == TASK_ZOMBIE) {
        int child_id = child->id;
        if (status) *status = child->exit_status;
        
        if (child == task_list) {
            if (child->next == child) {
                task_list = NULL;
            } else {
                task_list = child->next;
                task_t *tail = task_list;
                while (tail->next != child) tail = tail->next;
                tail->next = task_list;
            }
        } else {
            task_t *prev = task_list;
            while (prev->next != child) prev = prev->next;
            prev->next = child->next;
        }
        
        pmm_free_pages(child->stack_base, child->stack_pages);
        pmm_free_pages(child, 1);
        
        log("SYSWAITPID", INFO, "collected and destroyed child %d\n\r", child_id);
        return child_id;
    }
    
    if (nohang) {
        return 0; 
    }
    
    current_task->state = TASK_BLOCKED;
    current_task->wake_tick = 0xFFFFFFFFFFFFFFFF;
    // Yield to the PIT Timer so the child can execute
    while (current_task->state == TASK_BLOCKED) {
        asm volatile("sti; hlt");

    }
    return -2; 
}

int64_t sys_fork(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    if (!current_syscall_regs) return -1;
    
    task_t *child = fork_current_task(current_syscall_regs);
    if (!child) return -1;
    
    child->regs.rax = 0; 
    
    return child->id;
}





int64_t sys_exec(uint64_t path_ptr, uint64_t argv_ptr, uint64_t envp_ptr,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
  (void)envp_ptr; (void)arg4; (void)arg5; (void)arg6;
  
  const char *path = (const char*)path_ptr;
  char **argv = (char**)argv_ptr;
  task_t *current_task = get_current_task();
  
  if (!current_task || !path) return -1;
  log("SYS_EXEC",INFO,"loading '%s' into task %d\n\r", path, current_task->id);

  // 1. VFS LOOKUP & FILE LOADING
  inode_t *inode = NULL;
  if (vfs_lookup_path(path, &inode) != 0 || !inode) return -1;

  file_t *file = NULL;
  if (vfs_open(&file, inode, 0) != 0 || !file) return -1;

  size_t file_size = inode->size;
  void *elf_data = pmalloc((file_size + 4095) / 4096);
  if (!elf_data) { vfs_close(file); return -1; }

  if (vfs_read(file, elf_data, file_size) != (long)file_size) {
      pmm_free_pages(elf_data, (file_size + 4095) / 4096);
      vfs_close(file);
      return -1;
  }
  vfs_close(file);

  elf_info_t elf_info;
  if (elf_load_into(elf_data, file_size, &elf_info, current_task->cr3) != 0) {
      pmm_free_pages(elf_data, (file_size + 4095) / 4096);
      return -1;
  }
  current_task->brk_start   = elf_info.end_addr;
  current_task->brk_current = elf_info.end_addr;
  pmm_free_pages(elf_data, (file_size + 4095) / 4096);

  if (current_task->user_code) {
        elf_page_t *pages = (elf_page_t *)current_task->user_code;
        
        // A. Free the actual physical frames holding the executable code
        for (size_t i = 0; i < current_task->user_code_pages; i++) {
            if (pages[i].kernel_vaddr) {
                pmm_free_pages(pages[i].kernel_vaddr, 1);
            }
        }
        
        // B. Free the metadata array itself 
        // (Calculate how many pages the array took up. Usually 1 for small apps)
        size_t meta_pages = (current_task->user_code_pages * sizeof(elf_page_t) + 4095) / 4096;
        if (meta_pages == 0) meta_pages = 1;
        pmm_free_pages(current_task->user_code, meta_pages);
    }
  if (current_task->user_stack) {
      pmm_free_pages(current_task->user_stack, 2);
  }
  current_task->user_code = elf_info.pages;
  current_task->user_code_pages = elf_info.num_pages;

  void *new_stack_kaddr = pmalloc(2);
  uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
  
  vmm_map_page_in(current_task->cr3, (void*)(USER_STACK_TOP_VADDR - 8192), phys_from_virt(new_stack_kaddr), user_flags);
  vmm_map_page_in(current_task->cr3, (void*)(USER_STACK_TOP_VADDR - 4096), phys_from_virt((void*)((uint64_t)new_stack_kaddr + 4096)), user_flags);
  
  int argc = 0;
  if (argv) { while (argv[argc]) argc++; }

  uint64_t offset = 8192;
  uint8_t *kstack_base = (uint8_t*)new_stack_kaddr;
  uint64_t arg_user_ptrs[64];
  
  for (int i = 0; i < argc && i < 64; i++) {
      size_t len = strlen(argv[i]) + 1;
      offset -= len;
      memcpy(kstack_base + offset, (uint8_t*)argv[i], len);
      arg_user_ptrs[i] = (USER_STACK_TOP_VADDR - 8192) + offset;
  }
  
  offset &= ~7ULL; 
  offset -= 8;
  *(uint64_t*)(kstack_base + offset) = 0; // NULL terminator
  
  for (int i = argc - 1; i >= 0; i--) {
      offset -= 8;
      *(uint64_t*)(kstack_base + offset) = arg_user_ptrs[i];
  }
  
  uint64_t final_user_argv_ptr = (USER_STACK_TOP_VADDR - 8192) + offset;
  offset &= ~0xFULL; 
  uint64_t final_user_rsp = (USER_STACK_TOP_VADDR - 8192) + offset;
  
  current_task->user_stack = new_stack_kaddr;
  
  // OVERWRITE registers so that when we return, we jump to the NEW binary
  current_syscall_regs->rip = elf_info.entry_point;
  current_syscall_regs->rsp = final_user_rsp;
  current_syscall_regs->rdi = argc;
  current_syscall_regs->rsi = final_user_argv_ptr;
  current_syscall_regs->rax = 0; // Success!
  vmm_switch_page_table(current_task->cr3);
  return 0; 
}

