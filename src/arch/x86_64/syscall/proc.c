#include <arch/x86_64/syscall.h>
#include <kernel/sched/build_stack.h>
#include <kernel/sched/scheduler.h>
#include <kernel/vfs/vfs.h>
#include <kernel/elf.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/liballoc.h>
#include <libk/utils.h>
#include <libk/string.h>
#include <arch/x86_64/regs.h>
#include <stdint.h>

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
            size_t meta_pages = (current->user_code_pages * sizeof(elf_page_t) + 4095) / 4096;
            if (meta_pages == 0) meta_pages = 1;
            pmm_free_pages(current->user_code, meta_pages);
        }

        if (current->cr3) vmm_free_user_page_table(current->cr3);
        
        current->user_code = NULL;
        current->user_stack = NULL;
        current->cr3 = 0;
    }
    log("SYS_EXIT", INFO, "Free memory: %d\r\n", get_free_physical_memory()); 
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
        
        log("SYS_WAITPID", INFO, "collected and destroyed child %d\n\r", child_id);
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
  (void)arg4; (void)arg5; (void)arg6;
  log("SYS_EXEC", INFO, "Free memory: %d\r\n", get_free_physical_memory()); 

  const char *path = (const char*)path_ptr;
  char **argv = (char**)argv_ptr;
  char **envp = (char**)envp_ptr;
  task_t *current_task = get_current_task();

  if (!current_task || !path) return -1;
  log("SYS_EXEC",INFO,"loading '%s' into task %d\n\r", path, current_task->id);

  int argc = 0;
  if (argv) { while (argv[argc] && argc < MAX_EXEC_ARGS) argc++; }
  int envc = 0;
  if (envp) { while (envp[envc] && envc < MAX_EXEC_ARGS) envc++; }

  char *argv_snap[MAX_EXEC_ARGS];
  char *envp_snap[MAX_EXEC_ARGS];
  int argv_snapped = 0, envp_snapped = 0;

  for (int i = 0; i < argc; i++) {
      size_t len = strlen(argv[i]) + 1;
      argv_snap[i] = (char*)kmalloc(len);
      if (!argv_snap[i]) goto snapshot_oom;
      memcpy((uint8_t*)argv_snap[i], (uint8_t*)argv[i], len);
      argv_snapped++;
  }
  for (int i = 0; i < envc; i++) {
      size_t len = strlen(envp[i]) + 1;
      envp_snap[i] = (char*)kmalloc(len);
      if (!envp_snap[i]) goto snapshot_oom;
      memcpy((uint8_t*)envp_snap[i], (uint8_t*)envp[i], len);
      envp_snapped++;
  }

  {
  inode_t *inode = NULL;
  if (vfs_lookup_path(path, &inode) != 0 || !inode) goto snapshot_oom_ret_neg1;
  file_t *file = NULL;
  if (vfs_open(&file, inode, 0) != 0 || !file) goto snapshot_oom_ret_neg1;
  size_t file_size = inode->size;
  void *elf_data = pmalloc((file_size + 4095) / 4096);
  if (!elf_data) { vfs_close(file); goto snapshot_oom_ret_neg1; }
  if (vfs_read(file, elf_data, file_size) != (long)file_size) {
      pmm_free_pages(elf_data, (file_size + 4095) / 4096);
      vfs_close(file);
      goto snapshot_oom_ret_neg1;
  }
  vfs_close(file);

  elf_info_t elf_info;
  if (elf_load_into(elf_data, file_size, &elf_info, current_task->cr3) != 0) {
      pmm_free_pages(elf_data, (file_size + 4095) / 4096);
      goto snapshot_oom_ret_neg1;
  }
  pmm_free_pages(elf_data, (file_size + 4095) / 4096);

  void *new_stack_kaddr = pmalloc(2);
  if (!new_stack_kaddr) {
      elf_free(&elf_info);
      goto snapshot_oom_ret_neg1;
  }

  user_stack_result_t stack_res;
  if (build_user_stack((uint8_t*)new_stack_kaddr, 8192,
                        USER_STACK_TOP_VADDR - 8192,
                        argc, argv_snap, envc, envp_snap, &stack_res) != 0) {
      pmm_free_pages(new_stack_kaddr, 2);
      elf_free(&elf_info);
      goto snapshot_oom_ret_neg1; 
  }

  uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
  if (vmm_map_page_in(current_task->cr3, (void*)(USER_STACK_TOP_VADDR - 8192),
                       phys_from_virt(new_stack_kaddr), user_flags) != 0 ||
      vmm_map_page_in(current_task->cr3, (void*)(USER_STACK_TOP_VADDR - 4096),
                       phys_from_virt((void*)((uint64_t)new_stack_kaddr + 4096)), user_flags) != 0) {
      pmm_free_pages(new_stack_kaddr, 2);
      elf_free(&elf_info);
      goto snapshot_oom_ret_neg1;
  }

  // 4. POINT OF NO RETURN — everything above succeeded, safe to tear down
  //    the old image now and commit the new one.
  current_task->brk_start   = elf_info.end_addr;
  current_task->brk_current = elf_info.end_addr;

  if (current_task->user_code) {
      elf_page_t *pages = (elf_page_t *)current_task->user_code;
      for (size_t i = 0; i < current_task->user_code_pages; i++) {
          if (pages[i].kernel_vaddr) {
              pmm_free_pages(pages[i].kernel_vaddr, 1);
          }
      }
      size_t meta_pages = (current_task->user_code_pages * sizeof(elf_page_t) + 4095) / 4096;
      if (meta_pages == 0) meta_pages = 1;
      pmm_free_pages(current_task->user_code, meta_pages);
  }
  if (current_task->user_stack) {
      pmm_free_pages(current_task->user_stack, 2);
  }

  current_task->user_code       = elf_info.pages;
  current_task->user_code_pages = elf_info.num_pages;
  current_task->user_stack      = new_stack_kaddr;

 
  current_syscall_regs->rip = elf_info.entry_point;
  current_syscall_regs->rsp = stack_res.rsp;
  current_syscall_regs->rdi = argc;
  current_syscall_regs->rsi = stack_res.argv_uvaddr;
  current_syscall_regs->rdx = stack_res.envp_uvaddr;
  current_syscall_regs->rax = 0; // Success!

  vmm_switch_page_table(current_task->cr3);

  for (int i = 0; i < argv_snapped; i++) kfree(argv_snap[i]);
  for (int i = 0; i < envp_snapped; i++) kfree(envp_snap[i]);

  return 0;
  }

snapshot_oom_ret_neg1:
  for (int i = 0; i < argv_snapped; i++) kfree(argv_snap[i]);
  for (int i = 0; i < envp_snapped; i++) kfree(envp_snap[i]);
  return -1;

snapshot_oom:
  for (int i = 0; i < argv_snapped; i++) kfree(argv_snap[i]);
  for (int i = 0; i < envp_snapped; i++) kfree(envp_snap[i]);
  return -1;
}

