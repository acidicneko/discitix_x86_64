#include "arch/x86_64/gdt.h"
#include "libk/utils.h"

gdt_entry_t gdt_entries[3] = {
    {0, 0, 0, 0x00, 0x00, 0}, /* Kernel NULL Segment*/
    {0, 0, 0, 0x9a, 0xa0, 0}, /* Kernel Code Segment*/
    {0, 0, 0, 0x92, 0xa0, 0}  /* Kernel Data Segment*/
};
gdt_descriptor_t gdt;

void gdt_install(){
    gdt.size = (sizeof(gdt_entry_t)*3) - 1;
    gdt.offset = (uint64_t)&gdt_entries;
    load_gdt(&gdt);
	log(INFO, "GDT initialised\n");
}