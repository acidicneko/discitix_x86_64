#ifndef __PMM_H__
#define __PMM_H__

#define PAGE_SIZE   0x1000

#include <stdint.h>
#include "init/stivale2.h"
#include <stddef.h>

extern uintptr_t highest_page;

size_t align_up(size_t value, size_t align_to);
size_t align_down(size_t value, size_t align_to);

void init_pmm(struct stivale2_struct* bootinfo);
void init_bitmap(size_t size, void* buffer);

void lock_page(void* address);
void lock_pages(void* address, uint64_t count);

void free_page(void* address);
void free_pages(void* address, uint64_t count);

void* request_page();

uint64_t get_usable_memory();

#endif