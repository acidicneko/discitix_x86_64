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

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);
void init_idt();

#endif