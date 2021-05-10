#include "mm/pmm.h"
#include "mm/bitmap.h"
#include "libk/stdio.h"
#include "libk/utils.h"
#include <stddef.h>

uint64_t total_memory = 0;

uint64_t free_memory = 0;
uint64_t used_memory = 0;
uint64_t reserved_memory = 0;

bitmap_t page_bitmap;
uint64_t next_free_bit = 0;

void init_pmm(struct stivale2_struct *bootinfo){
    /*Get the memory map*/
    struct stivale2_struct_tag_memmap* memory_map = (struct stivale2_struct_tag_memmap*)stivale2_get_tag(bootinfo, STIVALE2_STRUCT_TAG_MEMMAP_ID);
    
    void* largest_entry = NULL;
    size_t largest_entry_size = 0;

    /*loop through each entry*/
    for(uint64_t i = 0; i < memory_map->entries; i++){
        struct stivale2_mmap_entry* entry = (struct stivale2_mmap_entry*)&memory_map->memmap[i];
        if(entry->type == STIVALE2_MMAP_USABLE ||
            entry->type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE ||
            entry->type == STIVALE2_MMAP_KERNEL_AND_MODULES){
            total_memory += entry->length;
            if(entry->length > largest_entry_size){
                largest_entry = (void*)(uint64_t)entry->base;
                largest_entry_size = entry->length;
            }
        }
    }

    free_memory = total_memory;
    size_t bitmap_size = (total_memory / 4096 / 8) + 1;
    init_bitmap(bitmap_size, largest_entry);
    uint64_t bitmap_pages = (page_bitmap.size / 4096) + 1;
    lock_pages(page_bitmap.buffer, bitmap_pages);
    reserve_pages(0, 512);
    log(INFO, "PMM initialised\n");
}

void init_bitmap(size_t size, void* buffer){
    page_bitmap.size = size;
    page_bitmap.buffer = (uint8_t*)buffer;
    for(size_t i = 0; i < size; i++){
        page_bitmap.buffer[i] = 0;
    }
}

void lock_page(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if(find_bit(&page_bitmap, index) == true)	return;
    set_bit(&page_bitmap, index, true);
    free_memory -= 4096;
    used_memory += 4096;
}

void lock_pages(void* address, uint64_t count){
    for(uint64_t i = 0; i < count; i++){
        lock_page((void*)((uint64_t)address + (i*4096)));
    }
}

void free_page(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if(find_bit(&page_bitmap, index) == false)	return;
    set_bit(&page_bitmap, index, false);
    free_memory += 4096;
    used_memory -= 4096;
}

void free_pages(void* address, uint64_t count){
    for(uint64_t i = 0; i < count; i++){
        free_page((void*)((uint64_t)address + (i*4096)));
    }
}

void reserve_page(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if(find_bit(&page_bitmap, index) == true)	return;
    set_bit(&page_bitmap, index, true);
    reserved_memory += 4096;
    free_memory -= 4096;
}

void reserve_pages(void* address, uint64_t count){
    for(uint64_t i = 0; i < count; i++){
        reserve_page((void*)((uint64_t)address + (i*4096)));
    }
}

void unreserve_page(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if(find_bit(&page_bitmap, index) == true)	return;
    set_bit(&page_bitmap, index, true);
    reserved_memory -= 4096;
    free_memory += 4096;
}

void unreserve_pages(void* address, uint64_t count){
    for(uint64_t i = 0; i < count; i++){
        unreserve_page((void*)((uint64_t)address + (i*4096)));
    }
}

void* request_page(){
    /*This is slow, we should keep track of next free bit instead!*/
    /*FIXME!!*/
    for(uint64_t i = 0; i < page_bitmap.size * 8; i++){
        if(find_bit(&page_bitmap, i) == true) continue;
        lock_page((void*)(i*4096));
        return (void*)(i*4096);
    }
    return NULL;
}

uint64_t get_total_memory(){
    return total_memory;
}

uint64_t get_free_memory(){
    return free_memory;
}

uint64_t get_used_memory(){
    return used_memory;
}

uint64_t get_reserved_memory(){
    return reserved_memory;
}