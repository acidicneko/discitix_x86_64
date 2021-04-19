/*	Shamelessly copied from https://github.com/MandelbrotOS/MandelbrotOS/blob/master/src/arch/x86_64/idt.c
	all credit goes to original creator!!
*/

#include "arch/x86_64/idt.h"
#include "libk/utils.h"

idt_entry_t idt_entries[256];
idt_ptr_t idt_ptr;

void idt_set_entry(idt_entry_t *entry, int user_space, void (*func)(void)) {
  entry->base_low = (uint16_t)((uint64_t)(func));
  entry->base_mid = (uint16_t)((uint64_t)(func) >> 16);
  entry->base_high = (uint32_t)((uint64_t)(func) >> 32);
  entry->sel = 8;
  entry->flags = idt_a_present | (user_space ? idt_a_ring_3 : idt_a_ring_0) | idt_a_type_interrupt;
}

void idt_install(){
    idt_ptr.limit = (sizeof(idt_entry_t)*256) - 1;
	idt_ptr.base = (uint64_t)&idt_entries;

	__asm__ volatile("lidt %0" : : "m"(idt_ptr));
	log(INFO, "IDT initialised\n");
}