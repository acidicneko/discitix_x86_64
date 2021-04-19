#include "arch/x86_64/idt.h"
#include "arch/x86_64/isr.h"
#include "libk/utils.h"

extern void isr_0();
extern void isr_1();
extern void isr_2();
extern void isr_3();
extern void isr_4();
extern void isr_5();
extern void isr_6();
extern void isr_7();
extern void isr_8();
extern void isr_9();
extern void isr_10();
extern void isr_11();
extern void isr_12();
extern void isr_13();
extern void isr_14();
extern void isr_15();
extern void isr_16();
extern void isr_17();
extern void isr_18();
extern void isr_19();
extern void isr_20();
extern void isr_21();
extern void isr_22();
extern void isr_23();
extern void isr_24();
extern void isr_25();
extern void isr_26();
extern void isr_27();
extern void isr_28();
extern void isr_29();
extern void isr_30();
extern void isr_31();

const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "Device not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault Exception",
    "General Protection Fault",
    "Page Fault",
    "[RESERVED]",
    "Floating Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point Exception",
    "Virtualization Exception",
    "[RESERVED]",
    "[RESERVED]",
    "[RESERVED]",
    "[RESERVED]",
    "[RESERVED]",
    "[RESERVED]",
    "[RESERVED]",
    "[RESERVED]",
    "[RESERVED]",
    "[RESERVED]",
    "[RESERVED]",
    "Security Exception",
};

// Initializes ISR's
void init_isr() {
  	idt_set_entry(&idt_entries[0], 0, isr_0);
  	idt_set_entry(&idt_entries[1], 0, isr_1);
  	idt_set_entry(&idt_entries[2], 0, isr_2);
  	idt_set_entry(&idt_entries[3], 0, isr_3);
  	idt_set_entry(&idt_entries[4], 0, isr_4);
  	idt_set_entry(&idt_entries[5], 0, isr_5);
  	idt_set_entry(&idt_entries[6], 0, isr_6);
  	idt_set_entry(&idt_entries[7], 0, isr_7);
  	idt_set_entry(&idt_entries[8], 0, isr_8);
  	idt_set_entry(&idt_entries[9], 0, isr_9);
  	idt_set_entry(&idt_entries[10], 0, isr_10);
  	idt_set_entry(&idt_entries[11], 0, isr_11);
  	idt_set_entry(&idt_entries[12], 0, isr_12);
  	idt_set_entry(&idt_entries[13], 0, isr_13);
  	idt_set_entry(&idt_entries[14], 0, isr_14);
  	idt_set_entry(&idt_entries[15], 0, isr_15);
  	idt_set_entry(&idt_entries[16], 0, isr_16);
  	idt_set_entry(&idt_entries[17], 0, isr_17);
  	idt_set_entry(&idt_entries[18], 0, isr_18);
  	idt_set_entry(&idt_entries[19], 0, isr_19);
  	idt_set_entry(&idt_entries[20], 0, isr_20);
  	idt_set_entry(&idt_entries[21], 0, isr_21);
  	idt_set_entry(&idt_entries[22], 0, isr_22);
  	idt_set_entry(&idt_entries[23], 0, isr_23);
  	idt_set_entry(&idt_entries[24], 0, isr_24);
  	idt_set_entry(&idt_entries[25], 0, isr_25);
  	idt_set_entry(&idt_entries[26], 0, isr_26);
  	idt_set_entry(&idt_entries[27], 0, isr_27);
  	idt_set_entry(&idt_entries[28], 0, isr_28);
  	idt_set_entry(&idt_entries[29], 0, isr_29);
  	idt_set_entry(&idt_entries[30], 0, isr_30);
  	idt_set_entry(&idt_entries[31], 0, isr_31);
	log(INFO, "ISRs initliased\n");
}

void fault_handler(int ex_no) {
	log(ERROR, "Exeception rasied! %s raised! Error code: %d\n", exception_messages[ex_no], ex_no);
  	for (;;)
    	;
}