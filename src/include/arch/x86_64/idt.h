#ifndef __IDT_H__
#define __IDT_H__

#include "arch/x86_64/regs.h"
#include <stdint.h>

typedef struct{
    uint16_t base_low;
    uint16_t sel;
    uint8_t ist;
    uint8_t flags;
    uint16_t base_medium;
    uint32_t base_high;
    uint32_t null;
} __attribute__((packed)) idt_entry_t;

typedef struct{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_desc_t;

extern void load_idt(idt_desc_t* idtptr);
void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist);
void init_idt();

void irq_install_handler(int irq, void (*handler)(register_t* regs));
void irq_uninstall_handler(int irq);
void send_eoi(int irq);
void irq_handler(register_t* regs);

void fault_handler(register_t* regs);

#endif