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

// HHDM offset (higher-half direct map) provided by Limine if available.
// Populated during init_pmm(). Use this to convert between physical and
// virtual addresses inside the PMM implementation.
static uintptr_t hhdm_offset = 0ULL;

static void *get_physical_address(void *adr) {
  if (hhdm_offset == 0)
    return adr; // assume already physical if offset unknown

  if ((uintptr_t)adr < hhdm_offset)
    return adr;

  return (void *)((uintptr_t)adr - hhdm_offset);
}

static void *get_virtual_address(void *adr) {
  if (hhdm_offset == 0)
    return adr;

  if ((uintptr_t)adr >= hhdm_offset)
    return adr;

  return (void *)((uintptr_t)adr + hhdm_offset);
}

void pmm_free_page(void *adr) {
  BIT_CLEAR((size_t)get_physical_address(adr) / PAGE_SIZE);
}

void pmm_alloc_page(void *adr) { BIT_SET((size_t)get_physical_address(adr) / PAGE_SIZE); }

void pmm_free_pages(void *adr, size_t page_count) {
  for (size_t i = 0; i < page_count; i++) {
    pmm_free_page((void *)((uintptr_t)adr + (i * PAGE_SIZE)));
  }
  free_mem += page_count * PAGE_SIZE;
}

void pmm_alloc_pages(void *adr, size_t page_count) {
  for (size_t i = 0; i < page_count; i++) {
    pmm_alloc_page((void *)((uintptr_t)adr + (i * PAGE_SIZE)));
  }
  free_mem -= page_count * PAGE_SIZE;
}

void *pmalloc(size_t pages) {
  size_t max_pages = highest_page / PAGE_SIZE;

  for (size_t i = 0; i < max_pages; i++) {
    for (size_t j = 0; j < pages; j++) {
      if (BIT_TEST(i + j))
        break;
      else if (j == pages - 1) {
        uintptr_t phys_addr = (uintptr_t)(i * PAGE_SIZE);
        pmm_alloc_pages((void *)phys_addr, pages);
        return get_virtual_address((void *)phys_addr);
      }
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

  memset((void *)ret, 0, pages * PAGE_SIZE);

  return ret;
}

void *phys_from_virt(void *virt) { return get_physical_address(virt); }
void *virt_from_phys(void *phys) { return get_virtual_address(phys); }

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
      // Use Limine's HHDM offset (if provided) to get a usable virtual address
      // to place the bitmap. If no HHDM is provided, assume the kernel has
      // already been mapped appropriately and use the physical address as-is.
      if (hhdm_request.response)
        hhdm_offset = hhdm_request.response->offset;

      pmm_bitmap = (uint8_t *)get_virtual_address((void *)entry->base);
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
