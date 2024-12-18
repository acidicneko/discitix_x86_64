#ifndef __PMM_H__
#define __PMM_H__

#include <init/stivale2.h>
#include <stddef.h>

#define PAGE_SIZE 0x1000
// #define PHYS_MEM_OFFSET 0xffff800000000000

void *pmalloc(size_t pages);
void *pcalloc(size_t pages);
void pmm_free_pages(void *adr, size_t page_count);
int init_pmm(struct stivale2_struct *bootinfo);

#endif
