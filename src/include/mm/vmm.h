#ifndef __VMM_H__
#define __VMM_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define KERNEL_OFFSET 0xffffffff80000000
#define VIRT_OFFSET 0xffff800000000000

typedef enum {
    present = 0,
    rw = 1,
    usersuper = 2,
    write_through = 3,
    cache_disabled = 4,
    accessed = 5,
    larger_pages = 7,
    custom0 = 9,
    custom1 = 10,
    custom2 = 11,
    no_exec = 63 // only if supported
} page_flags;

typedef struct{
    uint64_t value;
} page_dir_entry_t;

typedef struct{
    page_dir_entry_t entries[512];
} __attribute__((aligned(0x1000))) page_table_t;

typedef struct{
    uint64_t page_index;
    uint64_t page_table_index;
    uint64_t page_dir_index;
    uint64_t page_dir_ptr_index;
} indexer_t;

void set_flag(page_dir_entry_t* entry, page_flags flag, bool status);
bool get_flag(page_dir_entry_t* entry, page_flags flag);

void set_address(page_dir_entry_t* entry, uint64_t address);
uint64_t get_address(page_dir_entry_t* entry);

void make_index(indexer_t* indexer, uint64_t virtual_addr);

void switch_page_map(page_table_t* page_map);
void init_vmm();
void map_page(void* physical_addr, void* virtual_addr);

#endif