#ifndef __IDT_H__
#define __IDT_H__

#include <stdint.h>

#define IDT_TA_IG   0b10001110
#define IDT_TA_CG   0b10001100
#define IDT_TA_TG   0b10001111

typedef struct{
    uint16_t offset0;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attribute;
    uint16_t offset1;
    uint32_t offset2;
    uint32_t ignore;
} idt_entry_t;

typedef struct{
    uint16_t limit;
    uint64_t offset;
} __attribute__((packed)) idt_descriptor_t;

extern void load_idt(idt_descriptor_t* idt);

void set_offset(idt_entry_t* entry, uint64_t offset);
uint64_t get_offset(idt_entry_t* entry);

#endif