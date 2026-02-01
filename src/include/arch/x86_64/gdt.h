#ifndef __GDT_H__
#define __GDT_H__

#include <stdint.h>

// GDT segment selectors
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x18
#define GDT_USER_CODE   0x20
#define GDT_TSS         0x28

// Ring levels
#define RING_KERNEL 0
#define RING_USER   3

typedef struct{
    uint16_t limit0;
    uint16_t base0;
    uint8_t base1;
    uint8_t access_byte;
    uint8_t limit1;
    uint8_t base2;
} __attribute__((packed)) gdt_entry_t;

// TSS entry is 16 bytes in long mode (spans 2 GDT slots)
typedef struct {
    uint16_t limit0;
    uint16_t base0;
    uint8_t base1;
    uint8_t access;
    uint8_t limit1_flags;
    uint8_t base2;
    uint32_t base3;
    uint32_t reserved;
} __attribute__((packed)) tss_entry_t;

// Task State Segment structure for x86_64
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;      // Kernel stack pointer (used when switching from Ring 3 to Ring 0)
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;      // Interrupt Stack Table entries
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

typedef struct{
    uint16_t size;
    uint64_t offset;
} __attribute__((packed)) gdt_descriptor_t;

extern void load_gdt(gdt_descriptor_t* gdt);
extern void load_tss(uint16_t selector);
void init_gdt();
void tss_set_kernel_stack(uint64_t stack);

#endif