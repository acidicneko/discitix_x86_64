#include <mm/vmm.h>
#include <mm/pmm.h>
#include <libk/stdio.h>
#include <libk/utils.h>
#include <stdint.h>

static inline uint64_t read_cr3(void) {
  uint64_t v;
  asm volatile("mov %%cr3, %0" : "=r"(v));
  return v;
}

static inline void invlpg(void *addr) {
  asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

int init_vmm() {
  dbgln("VMM: initialized mapping helpers\n\r");
  return 0;
}

static uint64_t *ensure_table(uint64_t *table_entry, uint64_t flags) {
  if ((*table_entry) & PTE_PRESENT) {
    // If user flag requested, make sure it's set on existing entry too
    if (flags & PTE_USER) {
      *table_entry |= PTE_USER;
    }
    uint64_t phys = (*table_entry) & 0x000ffffffffff000ULL;
    return (uint64_t *)virt_from_phys((void *)phys);
  }

  void *virt = pmalloc(1);
  if (!virt)
    return NULL;
  for (size_t i = 0; i < 512; i++)
    ((uint64_t *)virt)[i] = 0ULL;

  void *phys = phys_from_virt(virt);
  uint64_t entry_flags = PTE_PRESENT | PTE_RW;
  if (flags & PTE_USER) {
    entry_flags |= PTE_USER;
  }
  *table_entry = ((uint64_t)(uintptr_t)phys & 0x000ffffffffff000ULL) | entry_flags;
  return (uint64_t *)virt;
}

int vmm_map_page(void *virt, void *phys, uint64_t flags) {
  if (!virt || !phys)
    return -1;

  uint64_t v = (uint64_t)virt;
  uint64_t p = (uint64_t)phys;

  uint64_t cr3 = read_cr3();
  uint64_t pml4_phys = cr3 & 0x000ffffffffff000ULL;
  uint64_t *pml4 = (uint64_t *)virt_from_phys((void *)pml4_phys);

  size_t i4 = (v >> 39) & 0x1FF;
  size_t i3 = (v >> 30) & 0x1FF;
  size_t i2 = (v >> 21) & 0x1FF;
  size_t i1 = (v >> 12) & 0x1FF;

  uint64_t *pdpt = ensure_table(&pml4[i4], flags);
  if (!pdpt)
    return -1;
  uint64_t *pd = ensure_table(&pdpt[i3], flags);
  if (!pd)
    return -1;
  uint64_t *pt = ensure_table(&pd[i2], flags);
  if (!pt)
    return -1;

  uint64_t entry = (p & 0x000ffffffffff000ULL) | (flags ? flags : (PTE_PRESENT | PTE_RW));
  pt[i1] = entry;

  invlpg(virt);
  return 0;
}

int vmm_map_range(void *virt, void *phys, size_t pages, uint64_t flags) {
  for (size_t i = 0; i < pages; i++) {
    void *v = (void *)((uintptr_t)virt + i * 4096);
    void *p = (void *)((uintptr_t)phys + i * 4096);
    if (vmm_map_page(v, p, flags) != 0)
      return -1;
  }
  return 0;
}
