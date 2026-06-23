#include <mm/vmm.h>
#include <mm/pmm.h>
#include <libk/stdio.h>
#include <libk/utils.h>
#include <stdint.h>
#include <libk/string.h>

static inline uint64_t read_cr3(void) {
    uint64_t v;
    asm volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void write_cr3(uint64_t v) {
    asm volatile("mov %0, %%cr3" : : "r"(v) : "memory");
}

static inline void invlpg(void *addr) {
    asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static uint64_t kernel_cr3 = 0;

int init_vmm() {
    kernel_cr3 = read_cr3();
    dbgln("VMM: initialized, kernel CR3 = 0x%llx\n\r", kernel_cr3);
    return 0;
}

uint64_t vmm_get_cr3(void) {
    return read_cr3();
}

void vmm_switch_page_table(uint64_t cr3_phys) {
    write_cr3(cr3_phys);
}

static uint64_t *ensure_table(uint64_t *table_entry, uint64_t flags) {
    if ((*table_entry) & PTE_PRESENT) {
        if (flags & PTE_USER)
            *table_entry |= PTE_USER;
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
    if (flags & PTE_USER)
        entry_flags |= PTE_USER;

    *table_entry = ((uint64_t)(uintptr_t)phys & 0x000ffffffffff000ULL) | entry_flags;
    return (uint64_t *)virt;
}

int vmm_map_page(void *virt, void *phys, uint64_t flags) {
    if (!virt || !phys)
        return -1;

    uint64_t v = (uint64_t)virt;
    uint64_t p = (uint64_t)phys;

    uint64_t cr3      = read_cr3();
    uint64_t *pml4    = (uint64_t *)virt_from_phys((void *)(cr3 & 0x000ffffffffff000ULL));

    size_t i4 = (v >> 39) & 0x1FF;
    size_t i3 = (v >> 30) & 0x1FF;
    size_t i2 = (v >> 21) & 0x1FF;
    size_t i1 = (v >> 12) & 0x1FF;

    uint64_t *pdpt = ensure_table(&pml4[i4], flags);
    if (!pdpt) return -1;
    uint64_t *pd = ensure_table(&pdpt[i3], flags);
    if (!pd) return -1;
    uint64_t *pt = ensure_table(&pd[i2], flags);
    if (!pt) return -1;

    pt[i1] = (p & 0x000ffffffffff000ULL) | (flags ? flags : (PTE_PRESENT | PTE_RW));
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

int vmm_map_page_in(uint64_t cr3_phys, void *virt, void *phys, uint64_t flags) {
    if (!virt || !phys)
        return -1;

    uint64_t v        = (uint64_t)virt;
    uint64_t p        = (uint64_t)phys;
    uint64_t pml4_phys = cr3_phys & 0x000ffffffffff000ULL;
    uint64_t *pml4    = (uint64_t *)virt_from_phys((void *)pml4_phys);

    size_t i4 = (v >> 39) & 0x1FF;
    size_t i3 = (v >> 30) & 0x1FF;
    size_t i2 = (v >> 21) & 0x1FF;
    size_t i1 = (v >> 12) & 0x1FF;

    uint64_t *pdpt = ensure_table(&pml4[i4], flags);
    if (!pdpt) return -1;
    uint64_t *pd = ensure_table(&pdpt[i3], flags);
    if (!pd) return -1;
    uint64_t *pt = ensure_table(&pd[i2], flags);
    if (!pt) return -1;

    pt[i1] = (p & 0x000ffffffffff000ULL) | (flags ? flags : (PTE_PRESENT | PTE_RW));

    if ((read_cr3() & 0x000ffffffffff000ULL) == pml4_phys)
        invlpg(virt);

    return 0;
}

/*
 * vmm_unmap_page_in — remove a single PTE from an arbitrary page table.
 *
 * Returns the physical address of the frame that was mapped, so the caller
 * can free it.  Returns NULL if the page was not mapped.
 *
 * invlpg is only issued when cr3_phys matches the current address space —
 * same guard used in vmm_map_page_in.
 */
void *vmm_unmap_page_in(uint64_t cr3_phys, void *virt) {
    uint64_t va        = (uint64_t)virt;
    uint64_t pml4_phys = cr3_phys & 0x000ffffffffff000ULL;

    size_t i4 = (va >> 39) & 0x1FF;
    size_t i3 = (va >> 30) & 0x1FF;
    size_t i2 = (va >> 21) & 0x1FF;
    size_t i1 = (va >> 12) & 0x1FF;

    uint64_t *pml4 = (uint64_t *)virt_from_phys((void *)pml4_phys);
    if (!(pml4[i4] & PTE_PRESENT)) return NULL;

    uint64_t *pdpt = (uint64_t *)virt_from_phys((void *)(pml4[i4] & 0x000ffffffffff000ULL));
    if (!(pdpt[i3] & PTE_PRESENT)) return NULL;

    uint64_t *pd = (uint64_t *)virt_from_phys((void *)(pdpt[i3] & 0x000ffffffffff000ULL));
    if (!(pd[i2] & PTE_PRESENT)) return NULL;

    uint64_t *pt = (uint64_t *)virt_from_phys((void *)(pd[i2] & 0x000ffffffffff000ULL));
    if (!(pt[i1] & PTE_PRESENT)) return NULL;

    void *phys = (void *)(pt[i1] & 0x000ffffffffff000ULL);
    pt[i1] = 0;

    if ((read_cr3() & 0x000ffffffffff000ULL) == pml4_phys)
        invlpg(virt);

    return phys;
}

uint64_t vmm_create_user_page_table(void) {
    void *new_pml4_virt = pmalloc(1);
    if (!new_pml4_virt) {
        dbgln("VMM: Failed to allocate PML4\n\r");
        return 0;
    }

    for (size_t i = 0; i < 512; i++)
        ((uint64_t *)new_pml4_virt)[i] = 0ULL;

    uint64_t *kernel_pml4 = (uint64_t *)virt_from_phys(
        (void *)(kernel_cr3 & 0x000ffffffffff000ULL));
    uint64_t *new_pml4 = (uint64_t *)new_pml4_virt;

    /* Share the kernel's higher-half mappings (indices 256–511) */
    for (size_t i = 256; i < 512; i++)
        new_pml4[i] = kernel_pml4[i];

    uint64_t new_cr3 = (uint64_t)phys_from_virt(new_pml4_virt);
    dbgln("VMM: Created user page table at phys 0x%llx\n\r", new_cr3);
    return new_cr3;
}

void vmm_free_user_page_table(uint64_t cr3_phys) {
    if (cr3_phys == 0 || cr3_phys == kernel_cr3) return;

    uint64_t *pml4 = (uint64_t *)virt_from_phys(
        (void *)(cr3_phys & 0x000ffffffffff000ULL));

    for (size_t i4 = 0; i4 < 256; i4++) {
        if (!(pml4[i4] & PTE_PRESENT)) continue;

        uint64_t *pdpt = (uint64_t *)virt_from_phys(
            (void *)(pml4[i4] & 0x000ffffffffff000ULL));

        for (size_t i3 = 0; i3 < 512; i3++) {
            if (!(pdpt[i3] & PTE_PRESENT)) continue;

            /* Bug fixed: was 0x000fffffffff000 — missing one 'f' */
            uint64_t *pd = (uint64_t *)virt_from_phys(
                (void *)(pdpt[i3] & 0x000ffffffffff000ULL));

            for (size_t i2 = 0; i2 < 512; i2++) {
                if (!(pd[i2] & PTE_PRESENT)) continue;

                uint64_t *pt = (uint64_t *)virt_from_phys(
                    (void *)(pd[i2] & 0x000ffffffffff000ULL));
                pmm_free_pages(pt, 1);
                pd[i2] = 0;
            }

            pmm_free_pages(pd, 1);
            pdpt[i3] = 0;
        }

        pmm_free_pages(pdpt, 1);
        pml4[i4] = 0;
    }

    pmm_free_pages(pml4, 1);
    dbgln("VMM: Freed user page table at phys 0x%llx\n\r", cr3_phys);
}

uint64_t vmm_clone_user_page_table(uint64_t parent_cr3_phys) {
    uint64_t child_cr3_phys = vmm_create_user_page_table();
    if (!child_cr3_phys) return 0;

    uint64_t *p_pml4 = (uint64_t *)virt_from_phys((void *)parent_cr3_phys);
    uint64_t *c_pml4 = (uint64_t *)virt_from_phys((void *)child_cr3_phys);

    for (int i4 = 0; i4 < 256; i4++) {
        if (!(p_pml4[i4] & PTE_PRESENT)) continue;

        uint64_t *p_pdpt = (uint64_t *)virt_from_phys(
            (void *)(p_pml4[i4] & 0x000ffffffffff000ULL));
        uint64_t *c_pdpt = ensure_table(&c_pml4[i4], p_pml4[i4] & 0xFFF);
        if (!c_pdpt) goto fail;

        for (int i3 = 0; i3 < 512; i3++) {
            if (!(p_pdpt[i3] & PTE_PRESENT)) continue;

            uint64_t *p_pd = (uint64_t *)virt_from_phys(
                (void *)(p_pdpt[i3] & 0x000ffffffffff000ULL));
            uint64_t *c_pd = ensure_table(&c_pdpt[i3], p_pdpt[i3] & 0xFFF);
            if (!c_pd) goto fail;

            for (int i2 = 0; i2 < 512; i2++) {
                if (!(p_pd[i2] & PTE_PRESENT)) continue;

                uint64_t *p_pt = (uint64_t *)virt_from_phys(
                    (void *)(p_pd[i2] & 0x000ffffffffff000ULL));
                uint64_t *c_pt = ensure_table(&c_pd[i2], p_pd[i2] & 0xFFF);
                if (!c_pt) goto fail;

                for (int i1 = 0; i1 < 512; i1++) {
                    if (!(p_pt[i1] & PTE_PRESENT)) continue;

                    void *new_frame_virt = pmalloc(1);
                    if (!new_frame_virt) goto fail;

                    uint64_t old_frame_phys = p_pt[i1] & 0x000ffffffffff000ULL;
                    void *old_frame_virt    = virt_from_phys((void *)old_frame_phys);
                    memcpy(new_frame_virt, old_frame_virt, 4096);

                    uint64_t new_frame_phys = (uint64_t)phys_from_virt(new_frame_virt);
                    c_pt[i1] = (new_frame_phys & 0x000ffffffffff000ULL)
                               | (p_pt[i1] & 0xFFF);
                }
            }
        }
    }

    return child_cr3_phys;

fail:
    dbgln("VMM: clone OOM — freeing partial child\n\r");
    vmm_free_user_page_table(child_cr3_phys);
    return 0;
}
