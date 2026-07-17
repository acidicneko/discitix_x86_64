#include <kernel/sched/build_stack.h>
#include <libk/string.h>
#include <libk/utils.h>

int build_user_stack(uint8_t *kstack_base, size_t stack_bytes,
                      uint64_t vaddr_base,
                      int argc, char *argv[],
                      int envc, char *envp[],
                      user_stack_result_t *out)
{
    log("BUILD_STACK", INFO, "enter: argc=%d envc=%d\n\r", argc, envc);
    if (argc < 0 || envc < 0) return -1;
    if (argc > MAX_EXEC_ARGS || envc > MAX_EXEC_ARGS) return -1;

    uint64_t offset = (uint64_t)stack_bytes;
    uint64_t arg_user_ptrs[MAX_EXEC_ARGS];
    uint64_t env_user_ptrs[MAX_EXEC_ARGS];

    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        if (len > offset) return -1;
        offset -= len;
        memcpy(kstack_base + offset, (uint8_t*)argv[i], len);
        arg_user_ptrs[i] = vaddr_base + offset;
    }

    for (int i = 0; i < envc; i++) {
        size_t len = strlen(envp[i]) + 1;
        if (len > offset) return -1;
        offset -= len;
        memcpy(kstack_base + offset, (uint8_t*)envp[i], len);
        env_user_ptrs[i] = vaddr_base + offset;
    }

    // Space needed: envp NULL(8) + envp ptrs(8*envc) + argv NULL(8) + argv ptrs(8*argc) + argc(8)
    uint64_t array_size = 24 + (8ULL * (uint64_t)argc) + (8ULL * (uint64_t)envc);
    if (array_size > offset) return -1;

    uint64_t target_rsp = (vaddr_base + offset - array_size) & ~0xFULL;
    if (target_rsp < vaddr_base) return -1; // underflow guard

    offset = target_rsp - vaddr_base + array_size;
    if (offset > stack_bytes) return -1; // sanity after realignment

    // 4. Push arrays, top to bottom

    // envp NULL terminator
    offset -= 8;
    *(uint64_t*)(kstack_base + offset) = 0;
    // envp pointers
    for (int i = envc - 1; i >= 0; i--) {
        offset -= 8;
        *(uint64_t*)(kstack_base + offset) = env_user_ptrs[i];
    }
    uint64_t envp_uvaddr = vaddr_base + offset;

    // argv NULL terminator
    offset -= 8;
    *(uint64_t*)(kstack_base + offset) = 0;
    // argv pointers
    for (int i = argc - 1; i >= 0; i--) {
        offset -= 8;
        *(uint64_t*)(kstack_base + offset) = arg_user_ptrs[i];
    }
    uint64_t argv_uvaddr = vaddr_base + offset;

    // argc
    offset -= 8;
    *(uint64_t*)(kstack_base + offset) = (uint64_t)argc;

    out->rsp         = vaddr_base + offset;
    out->argv_uvaddr = argv_uvaddr;
    out->envp_uvaddr = envp_uvaddr;
    log("BUILD_STACK", INFO, "exit OK: rsp=0x%xl argv=0x%xl envp=0x%xl\n\r",
           out->rsp, out->argv_uvaddr, out->envp_uvaddr);
    return 0;
}
