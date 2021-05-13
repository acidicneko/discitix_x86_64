#include "mm/pmm.h"
#include "mm/bitmap.h"
#include "libk/stdio.h"
#include "libk/string.h"
#include "libk/utils.h"
#include <stddef.h>

uint64_t usable_memory = 0;

bitmap_t page_bitmap;
uint64_t next_free_bit = 0;
uintptr_t highest_page;

size_t align_up(size_t value, size_t align_to){
    if((value % align_to) == 0) return value;
    size_t diff = value % align_to;
    value -= diff;
    value += align_to;
    return value;
}

size_t align_down(size_t value, size_t align_to){
    if((value % align_to) == 0) return value;
    size_t diff = value % align_to;
    value -= diff;
    return value;
}

void init_pmm(struct stivale2_struct *bootinfo){
    /*Get the memory map*/
    uintptr_t topmost;
    struct stivale2_struct_tag_memmap* memory_map = (struct stivale2_struct_tag_memmap*)stivale2_get_tag(bootinfo, STIVALE2_STRUCT_TAG_MEMMAP_ID);

    for(size_t i = 0; i < memory_map->entries; i++){
        struct stivale2_mmap_entry* entry = (struct stivale2_mmap_entry*)&memory_map->memmap[i];
        dbgln("PMM: entry->base: 0x%xl, entry->length: %ul, entry->type: %ul\n\r", entry->base, entry->length, entry->type);
        topmost = entry->base + entry->length;
        if(highest_page < topmost){
            highest_page = topmost;
        }
    }

    void* largest_entry = NULL;
    size_t largest_entry_size = 0;

    /*loop through each entry*/
    for(uint64_t i = 0; i < memory_map->entries; i++)
    {
        struct stivale2_mmap_entry* entry = (struct stivale2_mmap_entry*)&memory_map->memmap[i];
        if(entry->type == STIVALE2_MMAP_USABLE)
        {
            usable_memory += entry->length;
            if(entry->length > largest_entry_size)
            {
                largest_entry = (void*)(uint64_t)entry->base;
                largest_entry_size = entry->length;
            }
        }
    }

    size_t temp = highest_page / PAGE_SIZE / 8;
    size_t bitmap_size = align_up(temp, PAGE_SIZE);
    dbgln("PMM: initializing bitmap with aligned size = %ul\n\rPMM: page_bitmap.buffer: 0x%xl\n\r", (uint64_t)bitmap_size, (uint64_t)largest_entry);
    init_bitmap(bitmap_size, largest_entry);
    uint64_t bitmap_pages = page_bitmap.size / PAGE_SIZE;
    lock_pages((void*)page_bitmap.buffer, bitmap_pages);
    
    for(uint64_t i = 0; i < memory_map->entries; i++)
    {
        struct stivale2_mmap_entry* entry = (struct stivale2_mmap_entry*)&memory_map->memmap[i];
        if(entry->type == STIVALE2_MMAP_USABLE)
        {
            free_pages((void*)entry->base, entry->length/PAGE_SIZE);
        }
    }
    dbgln("PMM: initialized with %ul Bytes(%ul MB) of usable_memory\n\r", usable_memory, usable_memory/1024/1024);
    log(INFO, "PMM initialized\n");
}

void init_bitmap(size_t size, void* buffer){
    page_bitmap.size = size;
    page_bitmap.buffer = (uint8_t*)buffer;
    memset(page_bitmap.buffer, 0xff, page_bitmap.size);
}

void lock_page(void* address){
    uint64_t index = (uint64_t)address / PAGE_SIZE;
    if(find_bit(&page_bitmap, index) == true)	return;
    set_bit(&page_bitmap, index, true);
}

void lock_pages(void* address, uint64_t count){
    for(uint64_t i = 0; i < count; i++){
        lock_page((void*)((uint64_t)address + (i*PAGE_SIZE)));
    }
}

void free_page(void* address){
    uint64_t index = (uint64_t)address / PAGE_SIZE;
    if(find_bit(&page_bitmap, index) == false)	return;
    set_bit(&page_bitmap, index, false);
}

void free_pages(void* address, uint64_t count){
    for(uint64_t i = 0; i < count; i++){
        free_page((void*)((uint64_t)address + (i*PAGE_SIZE)));
    }
}

void* request_page(){
    /*This is slow, we should keep track of next free bit instead!*/
    /*FIXME!!*/
    for(uint64_t i = 0; i < page_bitmap.size * 8; i++){
        if(find_bit(&page_bitmap, i) == true) continue;
        lock_page((void*)(i*PAGE_SIZE));
        return (void*)(i*PAGE_SIZE);
    }
    return NULL;
}

uint64_t get_usable_memory(){
    return usable_memory;
}