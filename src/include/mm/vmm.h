#ifndef __VMM_H__
#define __VMM_H__

#include <stddef.h>
#include <stdint.h>

#define PTE_PRESENT (1ULL << 0)
#define PTE_RW (1ULL << 1)
#define PTE_USER (1ULL << 2)
#define PTE_PWT (1ULL << 3)
#define PTE_PCD (1ULL << 4)
#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY (1ULL << 6)
#define PTE_PSE (1ULL << 7)

int init_vmm();

// Map a single 4KiB page: virtual -> physical with given pte flags (or 0 for default rw).
int vmm_map_page(void *virt, void *phys, uint64_t flags);

// Map a contiguous range of pages
int vmm_map_range(void *virt, void *phys, size_t pages, uint64_t flags);

#endif
