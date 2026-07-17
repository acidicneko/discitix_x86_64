#include <kernel/sched/scheduler.h>
#include <kernel/vfs/vfs.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <arch/x86_64/syscall.h>
#include <libk/utils.h>
#include <stdint.h>
#include <string.h>
#define EPERM    1
#define ENOENT   2
#define ESRCH    3
#define ENOMEM  12
#define EINVAL  22
#define ENOSYS  38
/* mmap protection flags */
#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

/* mmap flags */
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20

#define MAP_FAILED    ((void *)-1)

// The below was obtained from multiple sources such as osdev wiki
/*
 * Virtual address space layout (user-space):
 *
 *   0x400000             ELF load base (typical)
 *   brk_start            set by ELF loader to page-aligned end of BSS
 *   brk_current          moves up as malloc/brk expands the heap
 *   ...
 *   0x00007F0000000000   mmap region grows downward from here
 *   0x00007FFFFFFFFFFF   top of canonical user space
 *
 * brk_start is stored in task_t and set during ELF loading.
 * USER_HEAP_FALLBACK is only used if the ELF loader didn't set it
 * (e.g. a hand-crafted test binary).
 *
 * task_t fields required:
 *   uint64_t  brk_start;    -- set by ELF loader, page-aligned end of BSS
 *   uint64_t  brk_current;  -- current break pointer
 *   uint64_t  mmap_base;    -- per-process mmap bump pointer
 */
#define USER_HEAP_FALLBACK  0x0000000001000000ULL   /* 16 MiB, well clear of ELF */
#define MMAP_BASE           0x00007F0000000000ULL   /* top of user mmap region */

/* Page helpers */
#define PAGE_MASK           (~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x)    (((uint64_t)(x) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_ALIGN_DOWN(x)  ((uint64_t)(x) & PAGE_MASK)

/*
 * NX bit (bit 63) marks a page non-executable on x86-64.
 * Requires IA32_EFER.NXE = 1, which your early boot should set.
 */
#define PTE_NX (1ULL << 63)

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/*
 * prot_to_flags — convert POSIX prot bits to x86-64 PTE flags.
 *
 * PROT_NONE results in a present-but-inaccessible page (no RW, no user
 * without PTE_USER — but we always set PTE_USER here since these are
 * user mappings). For true PROT_NONE you'd want to not set PTE_USER either;
 * keep this simple for now and document the limitation.
 */
static uint64_t prot_to_flags(uint64_t prot)
{
    uint64_t flags = PTE_PRESENT | PTE_USER;

    if (prot & PROT_WRITE)
        flags |= PTE_RW;

    /*
     * Set NX on every mapping that doesn't explicitly request EXEC.
     * This enforces W^X at the page level.
     */
    if (!(prot & PROT_EXEC))
        flags |= PTE_NX;

    return flags;
}

static int map_pages(uint64_t cr3, uint64_t vaddr, size_t num_pages, uint64_t flags)
{
    size_t mapped = 0;

    for (size_t i = 0; i < num_pages; i++) {
        void *page = pmalloc(1);
        if (!page)
            goto oom;

        memset(page, 0, PAGE_SIZE);

        void *phys = phys_from_virt(page);
        if (vmm_map_page_in(cr3,
                            (void *)(vaddr + i * PAGE_SIZE),
                            phys, flags) != 0) {
            pmm_free_pages(page, 1);
            goto oom;
        }

        mapped++;
    }

    return 0;

oom:
    /* Roll back all successfully mapped pages */
    for (size_t i = 0; i < mapped; i++) {
        void *phys = vmm_unmap_page_in(cr3,
                                       (void *)(vaddr + i * PAGE_SIZE));
        if (phys)
            pmm_free_pages(virt_from_phys(phys), 1);
    }
    return -ENOMEM;
}

static void unmap_pages(uint64_t cr3, uint64_t vaddr, size_t num_pages)
{
    for (size_t i = 0; i < num_pages; i++) {
        void *phys = vmm_unmap_page_in(cr3,
                                       (void *)(vaddr + i * PAGE_SIZE));
        if (phys)
            pmm_free_pages(virt_from_phys(phys), 1);
    }
}

int64_t sys_brk(uint64_t addr, uint64_t arg2, uint64_t arg3,
                uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    task_t *current = get_current_task();
    if (!current || !current->cr3)
        return -ESRCH;
    if (current->brk_start == 0) {
        current->brk_start   = USER_HEAP_FALLBACK;
        current->brk_current = USER_HEAP_FALLBACK;
    }
    log("SYS_BRK", INFO, "task %d: addr=0x%xl brk_start=0x%xl brk_current=0x%xl\n\r",
        current->id, addr, current->brk_start, current->brk_current);

    if (addr == 0)
        return (int64_t)current->brk_current;
    if (addr < current->brk_start) {
        log("SYS_BRK", ERROR, "task %d: REJECTED addr=0x%xl < brk_start=0x%xl\n\r",
            current->id, addr, current->brk_start);
        return (int64_t)current->brk_current;
    }
    uint64_t old_brk = current->brk_current;
    uint64_t new_brk = addr;
    if (new_brk > old_brk) {
        uint64_t map_start = PAGE_ALIGN_UP(old_brk);
        uint64_t map_end   = PAGE_ALIGN_UP(new_brk);
        if (map_end > map_start) {
            size_t   n     = (map_end - map_start) / PAGE_SIZE;
            uint64_t flags = prot_to_flags(PROT_READ | PROT_WRITE);
            log("SYS_BRK", INFO, "task %d: mapping %ul pages [0x%xl - 0x%xl)\n\r",
                current->id, n, map_start, map_end);
            if (map_pages(current->cr3, map_start, n, flags) != 0) {
                log("SYS_BRK", ERROR, "task %d: map_pages FAILED\n\r", current->id);
                return (int64_t)old_brk;
            }
        }
    } else if (new_brk < old_brk) {
        uint64_t unmap_start = PAGE_ALIGN_UP(new_brk);
        uint64_t unmap_end   = PAGE_ALIGN_UP(old_brk);
        if (unmap_end > unmap_start) {
            size_t n = (unmap_end - unmap_start) / PAGE_SIZE;
            unmap_pages(current->cr3, unmap_start, n);
        }
    }
    current->brk_current = new_brk;
    log("SYS_BRK", INFO, "task %d: SUCCESS new brk_current=0x%xl\n\r",
        current->id, current->brk_current);
    return (int64_t)new_brk;
}

/* -------------------------------------------------------------------------
 * sys_mmap - map memory into the process address space.
 *
 * Currently only anonymous mappings are supported (MAP_ANONYMOUS).
 * The per-process bump pointer lives in task->mmap_base (add this field
 * to task_t; it is initialized to MMAP_BASE on first use).
 * ---------------------------------------------------------------------- */
int64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                 uint64_t flags, uint64_t fd, uint64_t offset)
{
    (void)fd; (void)offset;

    task_t *current = get_current_task();
    if (!current || !current->cr3)
        return -ESRCH;

    if (length == 0)
        return -EINVAL;

    if (!(flags & MAP_ANONYMOUS)) {
        log("SYS_MMAP", ERROR ,"file-backed mappings not yet supported\n\r");
        return -ENOSYS;
    }

    if (!(flags & (MAP_PRIVATE | MAP_SHARED)))
        return -EINVAL;

    size_t   num_pages  = PAGE_ALIGN_UP(length) / PAGE_SIZE;
    uint64_t page_flags = prot_to_flags(prot);
    uint64_t vaddr;

    if (flags & MAP_FIXED) {
        /*
         * TODO: unmap any existing mapping in [addr, addr + length).
         */
        if (addr == 0 || (addr & ~PAGE_MASK))
            return -EINVAL;

        vaddr = addr;

    } else if (addr != 0) {
        vaddr = PAGE_ALIGN_DOWN(addr);

    } else {
        if (current->mmap_base == 0)
            current->mmap_base = MMAP_BASE;

        vaddr               = current->mmap_base;
        current->mmap_base += num_pages * PAGE_SIZE;
    }

    int ret = map_pages(current->cr3, vaddr, num_pages, page_flags);
    if (ret != 0)
        return ret;   /* -ENOMEM, with all partial pages already cleaned up */

    log("SYS_MMAP",INFO, "mapped %ul pages at 0x%xl\n\r", num_pages, vaddr);
    return (int64_t)vaddr;
}

int64_t sys_munmap(uint64_t addr, uint64_t length, uint64_t arg3,
                   uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;

    task_t *current = get_current_task();
    if (!current || !current->cr3)
        return -ESRCH;

    /* addr must be page-aligned and non-zero; length must be non-zero */
    if (addr == 0 || (addr & ~PAGE_MASK) || length == 0)
        return -EINVAL;

    size_t num_pages = PAGE_ALIGN_UP(length) / PAGE_SIZE;
    unmap_pages(current->cr3, addr, num_pages);

    return 0;
}
