#include "arch/x86_64/idt.h"
#include "arch/ports.h"
#include "libk/string.h"
#include "libk/utils.h"

idt_entry_t idt_entries[256];
idt_desc_t idt;

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags){
    idt_entries[num].base_high = (base >> 32);
    idt_entries[num].base_medium = (base >> 16) & 0xFFFF;
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].sel = sel;
    idt_entries[num].null = 0;
    idt_entries[num].ist = 0;
    idt_entries[num].flags = flags;
}

void init_idt(){
    idt.limit = (sizeof(idt_entry_t) * 256) - 1;
    idt.base = (uint64_t)&idt_entries;
    memset(&idt_entries, 0, sizeof(idt_entry_t)*256);
    asm volatile("lidt %0" : : "m"(idt));
    dbgln("IDT loaded\n\r");
}
