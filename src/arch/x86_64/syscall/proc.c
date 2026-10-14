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
#include <stdbool.h>
#include <libk/errno.h>

#define USER_STACK_SIZE      8192 // 2 Pages

extern task_t *task_list;

int64_t sys_exit(uint64_t status, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    if (!current) return -ESRCH;
    log("SYS_EXIT",INFO,"Process (task %d) called exit(%d)\n\r", current->id, (int)status);
    current->exit_status = (int)status;

    // Drop our references to open files (lets pipe readers see EOF).
    for (int fd = 0; fd < MAX_FDS; fd++) {
        if (current->fd_table[fd]) {
            vfs_close(current->fd_table[fd]);
            current->fd_table[fd] = NULL;
        }
    }

    // Reparent our children: nobody will wait for them now, so they get
    // reaped as orphans (already-dead ones by reap_orphan_zombies below).
    if (task_list) {
        task_t *t = task_list;
        do {
            if (t->parent_id == current->id) t->parent_id = 0;
            t = t->next;
        } while (t != task_list);
    }

    task_t *parent = NULL;
    if (current->parent_id > 0) {
        parent = find_task_by_id(current->parent_id);
        if (!parent || parent->state == TASK_ZOMBIE) {
            current->parent_id = 0;   // parent gone: become an orphan
            parent = NULL;
        }
    }
    // Only wake the parent if it is actually waiting for a child, not if
    // it's blocked on a sleep or a pipe.
    if (parent && parent->state == TASK_BLOCKED && parent->in_waitpid) {
        parent->state = TASK_RUNNABLE;
    }
    
    current->state = TASK_ZOMBIE;
        
    if (current->is_usermode) {
        if (current->user_code) kfree(current->user_code);

        // Never free the page table we're running on: move to the kernel's
        // CR3 first. This task keeps halting in kernel space until reaped,
        // which only needs the shared higher-half mappings.
        if (current->cr3) {
            uint64_t old_cr3 = current->cr3;
            current->cr3 = 0;
            vmm_switch_page_table(vmm_get_kernel_cr3());
            vmm_free_user_page_table(old_cr3);
        }
        
        current->user_code = NULL;
        current->user_stack = NULL;
    }

    // Free dead orphans (our own dead children included). We can't free
    // ourselves here since we're running on our kernel stack; if we're an
    // orphan, a later exit/waitpid by another task reaps us.
    reap_orphan_zombies();

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
    if (!current_task) return -ESRCH;

    int *status = (int*)status_ptr;
    int nohang = options & 1;

    reap_orphan_zombies();

    // pid == -1  -> wait for ANY child
    // pid > 0    -> wait for that specific child
    // pid == 0 or pid < -1 -> process-group waiting, not supported yet
    if (pid != (uint64_t)(int64_t)-1 && (int64_t)pid <= 0) {
        return -EINVAL;
    }

    for (;;) {
        task_t *found_zombie = NULL;
        bool has_matching_children = false;

        if ((int64_t)pid == -1) {
            task_t *t = task_list;
            if (t) {
                do {
                    if (t->parent_id == current_task->id) {
                        has_matching_children = true;
                        if (t->state == TASK_ZOMBIE) {
                            found_zombie = t;
                            break;
                        }
                    }
                    t = t->next;
                } while (t != task_list);
            }

            if (!found_zombie && !has_matching_children) {
                return -ECHILD;
            }
        } else {
            task_t *child = find_task_by_id((int)pid);
            if (!child || child->parent_id != current_task->id) {
                return -ECHILD;
            }
            has_matching_children = true;
            if (child->state == TASK_ZOMBIE) {
                found_zombie = child;
            }
        }

        if (found_zombie) {
            task_t *child = found_zombie;
            int child_id = child->id;
            // Linux/newlib encoding: WIFEXITED checks the low byte is 0,
            // WEXITSTATUS reads bits 8-15.
            if (status) *status = (child->exit_status & 0xff) << 8;

            task_reap(child);

            log("SYS_WAITPID", INFO, "collected and destroyed child %d\n\r", child_id);
            return child_id;
        }

        // No zombie found yet.
        if (nohang) {
            return 0;
        }

        // Block and wait to be woken (by the child's sys_exit setting our
        // state back to TASK_RUNNABLE, or by a timer tick). Either way,
        // loop back to the top and re-scan -- we don't know in advance
        // WHICH child (if pid == -1) will have become a zombie, so a
        // single block-then-return isn't correct; we must re-check.
        current_task->in_waitpid = 1;
        current_task->state = TASK_BLOCKED;
        current_task->wake_tick = 0xFFFFFFFFFFFFFFFF;
        while (current_task->state == TASK_BLOCKED) {
            asm volatile("sti; hlt");
        }
        current_task->in_waitpid = 0;
        // loop back around and re-scan for a zombie
    }
}

int64_t sys_fork(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current_task = get_current_task();
    if (!current_task || !current_task->syscall_regs) return -ESRCH;
    
    task_t *child = fork_current_task(current_task->syscall_regs);
    if (!child) return -EAGAIN;
    
    child->regs.rax = 0; 
    
    return child->id;
}

#define SHEBANG_MAX   256
#define SHEBANG_DEPTH 4

static char *kstrdup(const char *s) {
  size_t len = strlen(s) + 1;
  char *d = (char *)kmalloc(len);
  if (d) memcpy((uint8_t *)d, (const uint8_t *)s, len);
  return d;
}

// Splits "#!interp [arg]" (buf holds len bytes, has room for a terminator).
// Everything after the interpreter is one argument, as on Linux.
static int parse_shebang(char *buf, size_t len, char **interp, char **arg) {
  size_t end = 2;
  while (end < len && buf[end] != '\n') end++;
  buf[end] = '\0';

  char *p = buf + 2;
  while (*p == ' ' || *p == '\t') p++;
  if (!*p || *p == '\r') return -ENOEXEC;
  *interp = p;
  while (*p && *p != ' ' && *p != '\t' && *p != '\r') p++;
  if (*p) *p++ = '\0';

  while (*p == ' ' || *p == '\t') p++;
  char *q = p + strlen(p);
  while (q > p && (q[-1] == ' ' || q[-1] == '\t' || q[-1] == '\r')) *--q = '\0';
  *arg = *p ? p : NULL;
  return 0;
}

int64_t sys_exec(uint64_t path_ptr, uint64_t argv_ptr, uint64_t envp_ptr,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
  (void)arg4; (void)arg5; (void)arg6;
  log("SYS_EXEC", INFO, "Free memory: %d\r\n", get_free_physical_memory()); 

  const char *path = (const char*)path_ptr;
  char **argv = (char**)argv_ptr;
  char **envp = (char**)envp_ptr;
  task_t *current_task = get_current_task();

  if (!current_task || !current_task->syscall_regs) return -ESRCH;
  if (!path) return -EFAULT;
  log("SYS_EXEC",INFO,"loading '%s' into task %d\n\r", path, current_task->id);

  int argc = 0;
  if (argv) { while (argv[argc] && argc < MAX_EXEC_ARGS) argc++; }
  int envc = 0;
  if (envp) { while (envp[envc] && envc < MAX_EXEC_ARGS) envc++; }
  if ((argv && argv[argc]) || (envp && envp[envc])) return -E2BIG;

  int err = -ENOMEM;

  char *argv_snap[MAX_EXEC_ARGS];
  char *envp_snap[MAX_EXEC_ARGS];
  int argv_snapped = 0, envp_snapped = 0;

  for (int i = 0; i < argc; i++) {
      size_t len = strlen(argv[i]) + 1;
      argv_snap[i] = (char*)kmalloc(len);
      if (!argv_snap[i]) goto fail;
      memcpy((uint8_t*)argv_snap[i], (uint8_t*)argv[i], len);
      argv_snapped++;
  }
  for (int i = 0; i < envc; i++) {
      size_t len = strlen(envp[i]) + 1;
      envp_snap[i] = (char*)kmalloc(len);
      if (!envp_snap[i]) goto fail;
      memcpy((uint8_t*)envp_snap[i], (uint8_t*)envp[i], len);
      envp_snapped++;
  }

  {
  // Resolve #! scripts: each round replaces the program with its
  // interpreter and rewrites argv to "interp [arg] script args...".
  char header[SHEBANG_MAX + 1];
  char interp_buf[SHEBANG_MAX + 1];
  const char *exec_path = path;
  inode_t *inode = NULL;
  file_t *file = NULL;

  for (int depth = 0; ; depth++) {
      err = vfs_lookup_path(exec_path, &inode);
      if (err != 0) goto fail;
      err = -EACCES;
      if (inode->type != FT_REG) goto fail;
      err = -ENOEXEC;
      if (inode->size < 2) goto fail;
      err = vfs_open(&file, inode, O_RDONLY);
      if (err != 0) goto fail;

      long n = vfs_read(file, header, SHEBANG_MAX);
      if (n < 0) { vfs_close(file); err = (int)n; goto fail; }
      if (n < 2 || header[0] != '#' || header[1] != '!') {
          file->offset = 0;
          break;
      }
      vfs_close(file);
      file = NULL;

      err = -ELOOP;
      if (depth == SHEBANG_DEPTH) goto fail;

      char *interp, *iarg;
      err = parse_shebang(header, (size_t)n, &interp, &iarg);
      if (err != 0) goto fail;

      int extra = iarg ? 3 : 2;
      int rest = argv_snapped > 0 ? argv_snapped - 1 : 0;
      err = -E2BIG;
      if (extra + rest > MAX_EXEC_ARGS) goto fail;

      err = -ENOMEM;
      char *s_interp = kstrdup(interp);
      char *s_arg = iarg ? kstrdup(iarg) : NULL;
      char *s_script = kstrdup(exec_path);
      if (!s_interp || (iarg && !s_arg) || !s_script) {
          if (s_interp) kfree(s_interp);
          if (s_arg) kfree(s_arg);
          if (s_script) kfree(s_script);
          goto fail;
      }

      if (argv_snapped > 0) kfree(argv_snap[0]);
      memmove(&argv_snap[extra], &argv_snap[1], (size_t)rest * sizeof(char *));
      argv_snap[0] = s_interp;
      if (s_arg) argv_snap[1] = s_arg;
      argv_snap[extra - 1] = s_script;
      argv_snapped = extra + rest;
      argc = argv_snapped;

      strcpy(interp_buf, interp);
      exec_path = interp_buf;
      log("SYS_EXEC", INFO, "script, running interpreter '%s'\n\r", exec_path);
  }

  err = -ENOMEM;
  size_t file_size = inode->size;
  void *elf_data = pmalloc((file_size + 4095) / 4096);
  if (!elf_data) { vfs_close(file); goto fail; }
  if (vfs_read(file, elf_data, file_size) != (long)file_size) {
      pmm_free_pages(elf_data, (file_size + 4095) / 4096);
      vfs_close(file);
      err = -EIO;
      goto fail;
  }
  vfs_close(file);

  // Build the new image in a fresh address space. The old one stays intact
  // until everything has succeeded, so a failed exec returns to a working
  // process.
  uint64_t new_cr3 = vmm_create_user_page_table();
  if (!new_cr3) {
      pmm_free_pages(elf_data, (file_size + 4095) / 4096);
      goto fail;
  }

  elf_info_t elf_info;
  if (elf_load_into(elf_data, file_size, &elf_info, new_cr3) != 0) {
      pmm_free_pages(elf_data, (file_size + 4095) / 4096);
      vmm_free_user_page_table(new_cr3);
      err = elf_validate(elf_data, file_size) != 0 ? -ENOEXEC : -ENOMEM;
      goto fail;
  }
  pmm_free_pages(elf_data, (file_size + 4095) / 4096);

  void *new_stack_kaddr = pmalloc(2);
  if (!new_stack_kaddr) {
      elf_free(&elf_info);
      vmm_free_user_page_table(new_cr3);
      goto fail;
  }

  user_stack_result_t stack_res;
  if (build_user_stack((uint8_t*)new_stack_kaddr, USER_STACK_SIZE,
                        USER_STACK_VADDR,
                        argc, argv_snap, envc, envp_snap, &stack_res) != 0) {
      pmm_free_pages(new_stack_kaddr, 2);
      elf_free(&elf_info);
      vmm_free_user_page_table(new_cr3);
      goto fail; 
  }

  uint64_t user_flags = PTE_PRESENT | PTE_RW | PTE_USER;
  if (vmm_map_page_in(new_cr3, (void*)USER_STACK_VADDR,
                       phys_from_virt(new_stack_kaddr), user_flags) != 0 ||
      vmm_map_page_in(new_cr3, (void*)(USER_STACK_VADDR + 4096),
                       phys_from_virt((void*)((uint64_t)new_stack_kaddr + 4096)), user_flags) != 0) {
      // Unmap whatever got mapped so the page table free doesn't free it too.
      vmm_unmap_page_in(new_cr3, (void*)USER_STACK_VADDR);
      vmm_unmap_page_in(new_cr3, (void*)(USER_STACK_VADDR + 4096));
      pmm_free_pages(new_stack_kaddr, 2);
      elf_free(&elf_info);
      vmm_free_user_page_table(new_cr3);
      goto fail;
  }

  // POINT OF NO RETURN — switch to the new address space, then drop the old
  // one. Freeing the old page table releases every frame it mapped (code,
  // stack, brk heap, mmap regions), so none of them are freed individually.
  uint64_t old_cr3 = current_task->cr3;
  current_task->cr3 = new_cr3;
  vmm_switch_page_table(new_cr3);
  if (old_cr3) vmm_free_user_page_table(old_cr3);

  // Old ELF tracking array (metadata only — its frames went with the old table)
  if (current_task->user_code) kfree(current_task->user_code);

  // Close FD_CLOEXEC descriptors
  for (int fd = 0; fd < MAX_FDS; fd++) {
      if ((current_task->fd_cloexec & (1u << fd)) && current_task->fd_table[fd]) {
          vfs_close(current_task->fd_table[fd]);
          current_task->fd_table[fd] = NULL;
      }
  }
  current_task->fd_cloexec = 0;

  current_task->brk_start       = elf_info.end_addr;
  current_task->brk_current     = elf_info.end_addr;
  current_task->mmap_base       = 0;
  current_task->user_code       = elf_info.pages;
  current_task->user_code_pages = elf_info.num_pages;
  current_task->user_stack      = new_stack_kaddr;

 
  register_t *regs = current_task->syscall_regs;
  regs->rip = elf_info.entry_point;
  regs->rsp = stack_res.rsp;
  regs->rdi = argc;
  regs->rsi = stack_res.argv_uvaddr;
  regs->rdx = stack_res.envp_uvaddr;
  regs->rax = 0; // Success!

  for (int i = 0; i < argv_snapped; i++) kfree(argv_snap[i]);
  for (int i = 0; i < envp_snapped; i++) kfree(envp_snap[i]);

  return 0;
  }

fail:
  for (int i = 0; i < argv_snapped; i++) kfree(argv_snap[i]);
  for (int i = 0; i < envp_snapped; i++) kfree(envp_snap[i]);
  return err;
}

int64_t sys_getpid(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6){
  (void)arg1;(void)arg2;(void)arg3;(void)arg4;(void)arg5;(void)arg6;
  task_t* current_task = get_current_task();
  if(!current_task) return -ESRCH;

  int pid = current_task->id;
  return pid;
}
