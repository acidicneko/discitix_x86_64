#ifndef __ELF_H__
#define __ELF_H__

#include <stdint.h>
#include <stddef.h>

// ELF Magic
#define ELF_MAGIC 0x464C457F  // "\x7FELF"

// ELF Class
#define ELFCLASS64 2

// ELF Data encoding
#define ELFDATA2LSB 1  // Little endian

// ELF Type
#define ET_EXEC 2  // Executable file

// ELF Machine
#define EM_X86_64 62

// Program header types
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_PHDR    6

// Program header flags
#define PF_X 0x1  // Executable
#define PF_W 0x2  // Writable
#define PF_R 0x4  // Readable

// ELF64 Header
typedef struct {
    uint8_t  e_ident[16];    // Magic number and other info
    uint16_t e_type;         // Object file type
    uint16_t e_machine;      // Architecture
    uint32_t e_version;      // Object file version
    uint64_t e_entry;        // Entry point virtual address
    uint64_t e_phoff;        // Program header table file offset
    uint64_t e_shoff;        // Section header table file offset
    uint32_t e_flags;        // Processor-specific flags
    uint16_t e_ehsize;       // ELF header size in bytes
    uint16_t e_phentsize;    // Program header table entry size
    uint16_t e_phnum;        // Program header table entry count
    uint16_t e_shentsize;    // Section header table entry size
    uint16_t e_shnum;        // Section header table entry count
    uint16_t e_shstrndx;     // Section header string table index
} __attribute__((packed)) elf64_ehdr_t;

// ELF64 Program Header
typedef struct {
    uint32_t p_type;         // Segment type
    uint32_t p_flags;        // Segment flags
    uint64_t p_offset;       // Segment file offset
    uint64_t p_vaddr;        // Segment virtual address
    uint64_t p_paddr;        // Segment physical address
    uint64_t p_filesz;       // Segment size in file
    uint64_t p_memsz;        // Segment size in memory
    uint64_t p_align;        // Segment alignment
} __attribute__((packed)) elf64_phdr_t;

// ELF loading result
typedef struct {
    uint64_t entry_point;    // Program entry point
    uint64_t base_addr;      // Lowest loaded address
    uint64_t end_addr;       // Highest loaded address
    void**   pages;          // Array of allocated pages (for cleanup)
    size_t   num_pages;      // Number of allocated pages
} elf_info_t;

int elf_validate(const void* data, size_t size);

// Load ELF into memory using current page table
// Allocates user-accessible pages and copies segments
int elf_load(const void* data, size_t size, elf_info_t* info);

// Load ELF into memory using a specific page table (for per-process address spaces)
// cr3_phys: physical address of the target PML4
int elf_load_into(const void* data, size_t size, elf_info_t* info, uint64_t cr3_phys);

// Free ELF loaded pages
void elf_free(elf_info_t* info);

#endif
