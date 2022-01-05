#ifndef __VMM_H__
#define __VMM_H__

#include <stddef.h>
#include <stdint.h>

#define KERNEL_MEM_OFFSET 0xffffffff80000000

typedef struct pagemap {
  uint64_t *top_level;
} pagemap_t;

extern pagemap_t kernel_pagemap;

void vmm_load_pagemap(pagemap_t *pagemap);
pagemap_t *vmm_create_new_pagemap();
void vmm_map_page(pagemap_t *pagemap, uintptr_t physical_address,
                  uintptr_t virtual_address, uint64_t flags);
void vmm_unmap_page(pagemap_t *pagemap, uintptr_t virtual_address);
void vmm_memcpy(pagemap_t *pagemap_1, uintptr_t virtual_address_1,
                pagemap_t *pagemap_2, uintptr_t virtual_address_2,
                size_t count);
uintptr_t vmm_virt_to_phys(pagemap_t *pagemap, uintptr_t virtual_address);
uintptr_t vmm_get_kernel_address(pagemap_t *pagemap, uintptr_t virtual_address);
int init_vmm();

#endif