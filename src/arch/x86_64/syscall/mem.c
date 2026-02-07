#include <kernel/sched/scheduler.h>
#include <kernel/vfs/vfs.h>
#include <mm/vmm.h>

// mmap flags
#define MAP_FAILED ((void*)-1)
#define MAP_ANONYMOUS 0x20
#define MAP_PRIVATE   0x02
#define PROT_READ     0x1
#define PROT_WRITE    0x2
#define PROT_EXEC     0x4

// brk - change data segment size (heap management)
int64_t sys_brk(uint64_t addr, uint64_t arg2, uint64_t arg3,
                uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    if (!current || !current->cr3) {
        return -1;
    }
    
    // Initialize brk if not set
    if (current->brk_start == 0) {
        // Set heap start to a fixed address after code
        // Typically after the ELF sections, we use 0x600000
        current->brk_start = 0x600000;
        current->brk_current = current->brk_start;
    }
    
    // If addr is 0, return current brk
    if (addr == 0) {
        return (int64_t)current->brk_current;
    }
    
    // Can't shrink below brk_start
    if (addr < current->brk_start) {
        return (int64_t)current->brk_current;
    }
    
    // Expand the heap
    uint64_t old_brk = current->brk_current;
    uint64_t new_brk = addr;
    
    // Align new_brk to page boundary for mapping
    uint64_t old_page = (old_brk + 4095) & ~4095ULL;
    uint64_t new_page = (new_brk + 4095) & ~4095ULL;
    
    // Map new pages if needed
    if (new_page > old_page) {
        uint64_t flags = PTE_PRESENT | PTE_RW | PTE_USER;
        for (uint64_t vaddr = old_page; vaddr < new_page; vaddr += 4096) {
            void *page = pmalloc(1);
            if (!page) {
                return (int64_t)current->brk_current;  // Allocation failed
            }
            memset(page, 0, 4096);
            void *phys = phys_from_virt(page);
            if (vmm_map_page_in(current->cr3, (void*)vaddr, phys, flags) != 0) {
                pmm_free_pages(page, 1);
                return (int64_t)current->brk_current;
            }
        }
    }
    
    current->brk_current = new_brk;
    return (int64_t)current->brk_current;
}



// mmap - map memory
int64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                 uint64_t flags, uint64_t fd, uint64_t offset) {
    (void)fd; (void)offset;  // Currently unused for anonymous mappings
    
    task_t *current = get_current_task();
    if (!current || !current->cr3) {
        return -1;
    }
    
    // Only support anonymous mappings for now
    if (!(flags & MAP_ANONYMOUS)) {
        dbgln("sys_mmap: only anonymous mappings supported\n\r");
        return -1;
    }
    
    if (length == 0) return -1;
    
    // Round up to page boundary
    size_t num_pages = (length + 4095) / 4096;
    
    // Find a free virtual address range if addr is 0
    // Use a simple allocator starting at 0x700000000000
    static uint64_t next_mmap_addr = 0x700000000000ULL;
    
    uint64_t vaddr;
    if (addr == 0) {
        vaddr = next_mmap_addr;
        next_mmap_addr += num_pages * 4096;
    } else {
        vaddr = addr & ~4095ULL;  // Page align
    }
    
    // Build page flags
    uint64_t page_flags = PTE_PRESENT | PTE_USER;
    if (prot & PROT_WRITE) page_flags |= PTE_RW;
    
    // Allocate and map pages
    for (size_t i = 0; i < num_pages; i++) {
        void *page = pmalloc(1);
        if (!page) {
            // TODO: unmap already mapped pages
            return -1;
        }
        memset(page, 0, 4096);
        void *phys = phys_from_virt(page);
        if (vmm_map_page_in(current->cr3, (void*)(vaddr + i * 4096), phys, page_flags) != 0) {
            pmm_free_pages(page, 1);
            return -1;
        }
    }
    
    dbgln("sys_mmap: mapped %d pages at 0x%xl\n\r", (int)num_pages, vaddr);
    return (int64_t)vaddr;
}

// munmap - unmap memory
int64_t sys_munmap(uint64_t addr, uint64_t length, uint64_t arg3,
                   uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    // TODO: implement proper unmapping
    // For now, just return success
    (void)addr; (void)length;
    return 0;
}
