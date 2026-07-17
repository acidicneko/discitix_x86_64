#ifndef STACK_BUILD_H
#define STACK_BUILD_H

#include <stdint.h>
#include <stddef.h>

#define MAX_EXEC_ARGS 64

typedef struct {
    uint64_t rsp; 
    uint64_t argv_uvaddr; 
    uint64_t envp_uvaddr;  
} user_stack_result_t;

// from OSDev notes:
// Lays out argc/argv/envp (strings + pointer arrays) onto a user stack
// buffer, SysV-ABI-aligned, ready to hand to a fresh entry point.
//
// kstack_base : kernel vaddr of the START (lowest address) of the stack block
// stack_bytes : total size of that block (e.g. 8192 for a 2-page stack)
// vaddr_base  : user-space vaddr corresponding to kstack_base+0
// argc/argv   : args to lay out (argv may be NULL if argc == 0)
// envc/envp   : env vars to lay out (envp may be NULL if envc == 0)
// out         : receives final rsp/argv_uvaddr/envp_uvaddr on success
//
// Returns 0 on success, -1 on failure (too many args, or stack too small).
int build_user_stack(uint8_t *kstack_base, size_t stack_bytes,
                      uint64_t vaddr_base,
                      int argc, char *argv[],
                      int envc, char *envp[],
                      user_stack_result_t *out);

#endif
