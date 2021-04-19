#ifndef __IDT_H__
#define __IDT_H__

#include <stdint.h>

enum idt_attr {
  idt_a_present = 1 << 7,
  idt_a_ring_0 = 0 << 5,
  idt_a_ring_1 = 1 << 5,
  idt_a_ring_2 = 2 << 5,
  idt_a_ring_3 = 3 << 5,
  idt_a_type_interrupt = 0xE,
  idt_a_type_trap = 0xF
};

typedef struct idt_entry_t {
  uint16_t base_low;
  uint16_t sel;
  uint8_t ist;
  uint8_t flags;
  uint16_t base_mid;
  uint32_t base_high;
  uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr_t {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed)) idt_ptr_t;

extern idt_entry_t idt_entries[];

void idt_set_entry(idt_entry_t *entry, int user_space, void (*func)(void));
void idt_install();

#endif