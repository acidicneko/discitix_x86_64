#include "mm/vmm.h"
#include "mm/pmm.h"
#include "libk/string.h"
#include "libk/utils.h"

page_table_t* PML4 = NULL;

void set_flag(page_dir_entry_t* entry, page_flags flag, bool status){
    uint64_t bitSelector = (uint64_t)1 << flag;
    entry->value &= ~bitSelector;
    if (status){
        entry->value |= bitSelector;
    }
}

bool get_flag(page_dir_entry_t* entry, page_flags flag){
    uint64_t bitSelector = (uint64_t)1 << flag;
    return (entry->value & bitSelector) > 0 ? true : false;
}

void set_address(page_dir_entry_t* entry, uint64_t address){
    address &= 0x000000ffffffffff;
    entry->value &= 0xfff0000000000fff;
    entry->value |= (address << 12);
}

uint64_t get_address(page_dir_entry_t* entry){
    return (entry->value & 0x000ffffffffff000) >> 12;
}

void make_index(indexer_t* indexer, uint64_t virtual_addr){
    virtual_addr >>= 12;
    indexer->page_index = virtual_addr & 0x1ff;
    virtual_addr >>= 9;
    indexer->page_table_index = virtual_addr & 0x1ff;
    virtual_addr >>= 9;
    indexer->page_dir_index = virtual_addr & 0x1ff;
    virtual_addr >>= 9;
    indexer->page_dir_ptr_index = virtual_addr & 0x1ff;
}

void switch_page_map(uint64_t page_map){
   asm("mov %0, %%cr3" : : "r"(page_map));
   
}

void map_page(void* physical_addr, void* virtual_addr){
    indexer_t indexer;
    make_index(&indexer, (uint64_t)virtual_addr);
    page_dir_entry_t PDE;

    PDE = PML4->entries[indexer.page_dir_ptr_index];
    page_table_t* PDP;
    if (!get_flag(&PDE, present)){
        PDP = (page_table_t*)request_page();
        memset(PDP, 0, 0x1000);
        set_address(&PDE, (uint64_t)PDP >> 12);
        set_flag(&PDE, present, true);
        set_flag(&PDE, rw, true);
        PML4->entries[indexer.page_dir_ptr_index] = PDE;
    }
    else
    {
        PDP = (page_table_t*)((uint64_t)get_address(&PDE) << 12);
    }
    
    
    PDE = PDP->entries[indexer.page_dir_index];
    page_table_t* PD;
    if (!get_flag(&PDE, present)){
        PD = (page_table_t*)request_page();
        memset(PD, 0, 0x1000);
        set_address(&PDE, (uint64_t)PD >> 12);
        set_flag(&PDE, present, true);
        set_flag(&PDE, rw, true);
        PDP->entries[indexer.page_dir_index] = PDE;
    }
    else
    {
        PD = (page_table_t*)((uint64_t)get_address(&PDE) << 12);
    }

    PDE = PD->entries[indexer.page_table_index];
    page_table_t* PT;
    if (!get_flag(&PDE, present)){
        PT = (page_table_t*)request_page();
        memset(PT, 0, 0x1000);
        set_address(&PDE, (uint64_t)PT >> 12);
        set_flag(&PDE, present, true);
        set_flag(&PDE, rw, true);
        PD->entries[indexer.page_table_index] = PDE;
    }
    else
    {
        PT = (page_table_t*)((uint64_t)get_address(&PDE) << 12);
    }

    PDE = PT->entries[indexer.page_index];
    set_address(&PDE, (uint64_t)physical_addr >> 12);
    set_flag(&PDE, present, true);
    set_flag(&PDE, rw, true);
    PT->entries[indexer.page_index] = PDE;
}

void init_vmm(){
    PML4 = (page_table_t*)request_page();
    memset(PML4, 0, PAGE_SIZE);

    dbgln("VMM: mapping physical memory to virtual at offset = 0x%xl\n\r", (uint64_t)VIRT_OFFSET);
    for(uintptr_t i = 0; i < align_up(highest_page, PAGE_SIZE) - PAGE_SIZE; i += PAGE_SIZE){
        map_page((void*)i, (void*)(i + VIRT_OFFSET));
    }

    dbgln("VMM: switching pagemap on CR3\n\r");
    switch_page_map((uint64_t)PML4);

    dbgln("VMM: initalized\n\r");
}