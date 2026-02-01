#include <arch/x86_64/gdt.h>
#include <libk/utils.h>
#include <libk/string.h>

// GDT with 7 entries: NULL, Kernel Code, Kernel Data, User Data, User Code, TSS (2 slots)
// Note: In x86_64 sysret, User Data must come BEFORE User Code
__attribute__((aligned(16)))
gdt_entry_t gdt_entries[7];

gdt_descriptor_t gdt;
tss_t tss;

static void gdt_set_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt_entries[num].base0 = base & 0xFFFF;
    gdt_entries[num].base1 = (base >> 16) & 0xFF;
    gdt_entries[num].base2 = (base >> 24) & 0xFF;
    gdt_entries[num].limit0 = limit & 0xFFFF;
    gdt_entries[num].limit1 = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt_entries[num].access_byte = access;
}

static void gdt_set_tss(int num, uint64_t base, uint32_t limit) {
    tss_entry_t* tss_entry = (tss_entry_t*)&gdt_entries[num];
    
    tss_entry->limit0 = limit & 0xFFFF;
    tss_entry->base0 = base & 0xFFFF;
    tss_entry->base1 = (base >> 16) & 0xFF;
    tss_entry->access = 0x89;           // Present, DPL=0, TSS type (available)
    tss_entry->limit1_flags = ((limit >> 16) & 0x0F);
    tss_entry->base2 = (base >> 24) & 0xFF;
    tss_entry->base3 = (base >> 32) & 0xFFFFFFFF;
    tss_entry->reserved = 0;
}

void tss_set_kernel_stack(uint64_t stack) {
    tss.rsp0 = stack;
}

void init_gdt(){
    // Initialize TSS
    memset(&tss, 0, sizeof(tss_t));
    tss.iopb_offset = sizeof(tss_t);  // No I/O permission bitmap
    
    // Entry 0: NULL segment
    gdt_set_entry(0, 0, 0, 0, 0);
    
    // Entry 1: Kernel Code (0x08) - DPL 0
    // Access: Present(1) | DPL(00) | Type(1) | Executable(1) | Conforming(0) | Readable(1) | Accessed(0)
    // = 0x9A
    // Flags: Granularity(0) | Long mode(1) | Size(0) | reserved(0) = 0xA0
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);
    
    // Entry 2: Kernel Data (0x10) - DPL 0
    // Access: Present(1) | DPL(00) | Type(1) | Executable(0) | Direction(0) | Writable(1) | Accessed(0)
    // = 0x92
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xA0);
    
    // Entry 3: User Data (0x18) - DPL 3
    // Access: Present(1) | DPL(11) | Type(1) | Executable(0) | Direction(0) | Writable(1) | Accessed(0)
    // = 0xF2
    gdt_set_entry(3, 0, 0xFFFFF, 0xF2, 0xA0);
    
    // Entry 4: User Code (0x20) - DPL 3
    // Access: Present(1) | DPL(11) | Type(1) | Executable(1) | Conforming(0) | Readable(1) | Accessed(0)
    // = 0xFA
    gdt_set_entry(4, 0, 0xFFFFF, 0xFA, 0xA0);
    
    // Entry 5-6: TSS (0x28) - spans 2 entries in long mode
    gdt_set_tss(5, (uint64_t)&tss, sizeof(tss_t) - 1);
    
    // GDT descriptor - 7 entries but TSS counts as 2
    gdt.size = (sizeof(gdt_entry_t) * 7) - 1;
    gdt.offset = (uint64_t)&gdt_entries;
    
    load_gdt(&gdt);
    load_tss(GDT_TSS);
    
    dbgln("GDT Loaded with user-mode segments and TSS\\n\\r");
}