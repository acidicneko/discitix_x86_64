#include <kernel/elf.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <libk/string.h>
#include <libk/utils.h>

int elf_validate(const void* data, size_t size) {
    if (size < sizeof(elf64_ehdr_t)) {
        dbgln("ELF: File too small for header\n\r");
        return -1;
    }
    
    const elf64_ehdr_t* ehdr = (const elf64_ehdr_t*)data;
    
    // Check magic number
    uint32_t magic = *(uint32_t*)ehdr->e_ident;
    if (magic != ELF_MAGIC) {
        dbgln("ELF: Invalid magic number: 0x%xl\n\r", magic);
        return -1;
    }
    
    // Check class (64-bit)
    if (ehdr->e_ident[4] != ELFCLASS64) {
        dbgln("ELF: Not a 64-bit ELF\n\r");
        return -1;
    }
    
    // Check endianness (little endian)
    if (ehdr->e_ident[5] != ELFDATA2LSB) {
        dbgln("ELF: Not little-endian\n\r");
        return -1;
    }
    
    // Check type (executable)
    if (ehdr->e_type != ET_EXEC) {
        dbgln("ELF: Not an executable (type=%d)\n\r", ehdr->e_type);
        return -1;
    }
    
    // Check machine (x86_64)
    if (ehdr->e_machine != EM_X86_64) {
        dbgln("ELF: Not x86_64 architecture\n\r");
        return -1;
    }
    
    // Check program header table
    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
        dbgln("ELF: No program headers\n\r");
        return -1;
    }
    
    // Verify program headers fit in file
    size_t ph_end = ehdr->e_phoff + (ehdr->e_phnum * ehdr->e_phentsize);
    if (ph_end > size) {
        dbgln("ELF: Program headers extend past end of file\n\r");
        return -1;
    }
    
    return 0;
}

static inline uint64_t page_align_up(uint64_t addr) {
    return (addr + 0xFFF) & ~0xFFFULL;
}

static inline uint64_t page_align_down(uint64_t addr) {
    return addr & ~0xFFFULL;
}

typedef struct {
    uint64_t user_vaddr;
    void*    kernel_vaddr; // Kernel-space virtual address (for copying)
} elf_page_t;


static int elf_load_impl(const void* data, size_t size, elf_info_t* info, uint64_t cr3_phys) {
    if (elf_validate(data, size) != 0) {
        return -1;
    }
    
    const elf64_ehdr_t* ehdr = (const elf64_ehdr_t*)data;
    const uint8_t* file_data = (const uint8_t*)data;
    
    // First pass: calculate total pages needed and address range
    uint64_t lowest_addr = ~0ULL;
    uint64_t highest_addr = 0;
    size_t total_pages = 0;
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const elf64_phdr_t* phdr = (const elf64_phdr_t*)(file_data + ehdr->e_phoff + i * ehdr->e_phentsize);
        
        if (phdr->p_type != PT_LOAD) continue;
        if (phdr->p_memsz == 0) continue;
        
        uint64_t seg_start = page_align_down(phdr->p_vaddr);
        uint64_t seg_end = page_align_up(phdr->p_vaddr + phdr->p_memsz);
        
        if (seg_start < lowest_addr) lowest_addr = seg_start;
        if (seg_end > highest_addr) highest_addr = seg_end;
        
        total_pages += (seg_end - seg_start) / 4096;
    }
    
    if (total_pages == 0) {
        dbgln("ELF: No loadable segments\n\r");
        return -1;
    }
    
    dbgln("ELF: Loading %d pages at 0x%xl - 0x%xl (cr3=0x%xl)\n\r", 
          (int)total_pages, lowest_addr, highest_addr, cr3_phys);
    
    // Allocate page tracking array (stores both user and kernel vaddrs)
    elf_page_t* page_map = (elf_page_t*)pmalloc(1);
    if (!page_map) {
        dbgln("ELF: Failed to allocate page tracking\n\r");
        return -1;
    }
    memset(page_map, 0, 4096);
    size_t num_mapped = 0;
    
    info->pages = (void**)page_map;  // Reuse pages pointer for tracking
    info->num_pages = 0;
    info->entry_point = ehdr->e_entry;  // Use ELF's entry point directly
    info->base_addr = lowest_addr;
    info->end_addr = highest_addr;
    
    // Helper to find kernel vaddr for a user vaddr
    #define find_kernel_vaddr(uva) ({ \
        void* _kva = NULL; \
        for (size_t _j = 0; _j < num_mapped; _j++) { \
            if (page_map[_j].user_vaddr <= (uva) && \
                (uva) < page_map[_j].user_vaddr + 4096) { \
                _kva = (void*)((uint64_t)page_map[_j].kernel_vaddr + ((uva) - page_map[_j].user_vaddr)); \
                break; \
            } \
        } \
        _kva; \
    })
    
    // Second pass: allocate pages and load segments
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const elf64_phdr_t* phdr = (const elf64_phdr_t*)(file_data + ehdr->e_phoff + i * ehdr->e_phentsize);
        
        if (phdr->p_type != PT_LOAD) continue;
        if (phdr->p_memsz == 0) continue;
        
        dbgln("ELF: Loading segment %d: vaddr=0x%xl filesz=%d memsz=%d flags=0x%xi\n\r",
              i, phdr->p_vaddr, (int)phdr->p_filesz, (int)phdr->p_memsz, phdr->p_flags);
        
        uint64_t seg_start = page_align_down(phdr->p_vaddr);
        uint64_t seg_end = page_align_up(phdr->p_vaddr + phdr->p_memsz);
        
        // Allocate and map pages for this segment
        for (uint64_t vaddr = seg_start; vaddr < seg_end; vaddr += 4096) {
            // Check if page already allocated (segments can overlap)
            int already_mapped = 0;
            for (size_t j = 0; j < num_mapped; j++) {
                if (page_map[j].user_vaddr == vaddr) {
                    already_mapped = 1;
                    break;
                }
            }
            
            if (!already_mapped) {
                // Allocate physical page (returns kernel vaddr)
                void* page = pmalloc(1);
                if (!page) {
                    dbgln("ELF: Failed to allocate page\n\r");
                    elf_free(info);
                    return -1;
                }
                
                // Zero the page using kernel vaddr
                memset(page, 0, 4096);
                
                // Determine page flags - always RW for simplicity
                uint64_t flags = PTE_PRESENT | PTE_USER | PTE_RW;
                
                // Map physical page to user virtual address in the specified page table
                void* phys = phys_from_virt(page);
                int map_result;
                if (cr3_phys != 0) {
                    map_result = vmm_map_page_in(cr3_phys, (void*)vaddr, phys, flags);
                } else {
                    map_result = vmm_map_page((void*)vaddr, phys, flags);
                }
                
                if (map_result != 0) {
                    dbgln("ELF: Failed to map page at 0x%xl\n\r", vaddr);
                    pmm_free_pages(page, 1);
                    elf_free(info);
                    return -1;
                }
                
                // Track this page
                page_map[num_mapped].user_vaddr = vaddr;
                page_map[num_mapped].kernel_vaddr = page;
                num_mapped++;
            }
        }
        
        // Copy segment data from file using KERNEL addresses
        if (phdr->p_filesz > 0) {
            // Validate file offset
            if (phdr->p_offset + phdr->p_filesz > size) {
                dbgln("ELF: Segment data extends past end of file\n\r");
                elf_free(info);
                return -1;
            }
            
            // Copy file data using kernel virtual addresses
            uint64_t dest_vaddr = phdr->p_vaddr;
            size_t remaining = phdr->p_filesz;
            size_t src_offset = 0;
            
            while (remaining > 0) {
                void* kva = find_kernel_vaddr(dest_vaddr);
                if (!kva) {
                    dbgln("ELF: No kernel mapping for 0x%xl\n\r", dest_vaddr);
                    elf_free(info);
                    return -1;
                }
                
                // Calculate how much to copy in this page
                size_t page_offset = dest_vaddr & 0xFFF;
                size_t copy_size = 4096 - page_offset;
                if (copy_size > remaining) copy_size = remaining;
                
                memcpy(kva, file_data + phdr->p_offset + src_offset, copy_size);
                
                dest_vaddr += copy_size;
                src_offset += copy_size;
                remaining -= copy_size;
            }
        }
        
        // Zero BSS portion (memsz > filesz) using kernel addresses
        if (phdr->p_memsz > phdr->p_filesz) {
            uint64_t bss_start = phdr->p_vaddr + phdr->p_filesz;
            size_t bss_size = phdr->p_memsz - phdr->p_filesz;
            
            while (bss_size > 0) {
                void* kva = find_kernel_vaddr(bss_start);
                if (!kva) break;  // Already zeroed during allocation
                
                size_t page_offset = bss_start & 0xFFF;
                size_t zero_size = 4096 - page_offset;
                if (zero_size > bss_size) zero_size = bss_size;
                
                memset(kva, 0, zero_size);
                
                bss_start += zero_size;
                bss_size -= zero_size;
            }
        }
    }
    
    info->num_pages = num_mapped;
    dbgln("ELF: Loaded successfully, entry point 0x%xl\n\r", info->entry_point);
    return 0;
    
    #undef find_kernel_vaddr
}

// Load ELF using current page table (legacy interface)
int elf_load(const void* data, size_t size, elf_info_t* info) {
    return elf_load_impl(data, size, info, 0);
}

// Load ELF into a specific page table
int elf_load_into(const void* data, size_t size, elf_info_t* info, uint64_t cr3_phys) {
    return elf_load_impl(data, size, info, cr3_phys);
}

void elf_free(elf_info_t* info) {
    if (!info) return;
    
    // Note: We can't easily unmap pages without more VMM support
    // For now, just free the tracking array
    if (info->pages) {
        // TODO: unmap pages from page tables
        // TODO: free physical pages
        pmm_free_pages(info->pages, 1);
        info->pages = NULL;
    }
    
    info->num_pages = 0;
    info->entry_point = 0;
}
