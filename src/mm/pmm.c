#include <init/limine.h>
#include <init/limine_req.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/pmm.h>
#include <stdint.h>

#define ALIGN_UP(__number) (((__number) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define ALIGN_DOWN(__number) ((__number) & ~(PAGE_SIZE - 1))

#define BIT_SET(__bit) (pmm_bitmap[(__bit) / 8] |= (1 << ((__bit) % 8)))
#define BIT_CLEAR(__bit) (pmm_bitmap[(__bit) / 8] &= ~(1 << ((__bit) % 8)))
#define BIT_TEST(__bit) ((pmm_bitmap[(__bit) / 8] >> ((__bit) % 8)) & 1)

static uint8_t *pmm_bitmap = 0;
static uintptr_t highest_page = 0;
static uint32_t total_mem = 0;
static uint32_t free_mem = 0;

void *get_physical_address(void *adr) {
  if ((uintptr_t)adr < PHYS_MEM_OFFSET)
    return adr;

  return (void *)((uintptr_t)adr - PHYS_MEM_OFFSET);
}

void *get_virtual_address(void *adr) {
  if ((uintptr_t)adr >= PHYS_MEM_OFFSET)
    return adr;

  return (void *)((uintptr_t)adr + PHYS_MEM_OFFSET);
}

void pmm_free_page(void *adr) {
  BIT_CLEAR((size_t)get_physical_address(adr) / PAGE_SIZE);
}

void pmm_alloc_page(void *adr) { BIT_SET((size_t)adr / PAGE_SIZE); }

void pmm_free_pages(void *adr, size_t page_count) {
  for (size_t i = 0; i < page_count; i++) {
    pmm_free_page((void *)(adr + (i * PAGE_SIZE)));
    free_mem += page_count * PAGE_SIZE;
  }
}

void pmm_alloc_pages(void *adr, size_t page_count) {
  for (size_t i = 0; i < page_count; i++) {
    pmm_alloc_page((void *)(adr + (i * PAGE_SIZE)));
    free_mem -= page_count * PAGE_SIZE;
  }
}

void *pmalloc(size_t pages) {
  for (size_t i = 0; i < highest_page / PAGE_SIZE; i++)
    for (size_t j = 0; j < pages; j++) {
      if (BIT_TEST(i))
        break;
      else if (j == pages - 1) {
        pmm_alloc_pages((void *)(i * PAGE_SIZE), pages);
        // return (void *)(i * PAGE_SIZE) + PHYS_MEM_OFFSET;
        return get_virtual_address((void *)(i * PAGE_SIZE));
      }
    }

  dbgln("PMM: Ran out of memory! Halting!\n\r");
  while (1)
    ;
  return NULL;
}

void *pcalloc(size_t pages) {
  char *ret = (char *)pmalloc(pages);

  if (ret == NULL)
    return NULL;

  memset((void *)((uint64_t)ret /*+ PHYS_MEM_OFFSET*/), 0, pages * PAGE_SIZE);

  return ret;
}

int init_pmm() {
  struct limine_memmap_response *memory_info = memmap_request.response;

  uintptr_t top;

  for (uint64_t i = 0; i < memory_info->entry_count; i++) {
    struct limine_memmap_entry *entry = memory_info->entries[i];

    if (entry->type != LIMINE_MEMMAP_USABLE &&
        entry->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE &&
        entry->type != LIMINE_MEMMAP_KERNEL_AND_MODULES)
      continue;

    top = entry->base + entry->length;

    if (top > highest_page)
      highest_page = top;
  }

  size_t bitmap_size = ALIGN_UP(ALIGN_DOWN(highest_page) / PAGE_SIZE / 8);

  for (uint64_t i = 0; i < memory_info->entry_count; i++) {
    struct limine_memmap_entry *entry = memory_info->entries[i];

    if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
      // TODO: Check if i need to add PHYS_MEM_OFFSET here
      pmm_bitmap = (uint8_t *)(entry->base + PHYS_MEM_OFFSET);
      entry->base += bitmap_size;
      entry->length -= bitmap_size;
      break;
    }
  }
  dbgln("Allocated bitmap at: 0x%xh\n\r", pmm_bitmap);
  memset(pmm_bitmap, 0xff, bitmap_size);

  for (uint64_t i = 0; i < memory_info->entry_count; i++) {
    if (memory_info->entries[i]->type == LIMINE_MEMMAP_USABLE) {
      pmm_free_pages((void *)memory_info->entries[i]->base,
                     memory_info->entries[i]->length / PAGE_SIZE);
      total_mem += memory_info->entries[i]->length;
    }
  }
  free_mem = total_mem;
  return 0;
}

uint32_t get_total_physical_memory() { return total_mem; }
uint32_t get_free_physical_memory() { return free_mem; }
