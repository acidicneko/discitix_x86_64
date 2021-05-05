#include "arch/x86_64/idt.h"
#include "arch/ports.h"
#include "libk/utils.h"

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

idt_entry_t idt_entries[256];
idt_desc_t idt;

void* interrupt_handlers[16] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist){
    idt_entries[num].base_high = (base >> 32);
    idt_entries[num].base_medium = (base >> 16) & 0xFFFF;
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].sel = sel;
    idt_entries[num].null = 0;
    idt_entries[num].ist = ist & 0x7;
    idt_entries[num].flags = flags;
}

void init_idt(){
    idt.limit = (sizeof(idt_entry_t) * 256) - 1;
    idt.base = (uint64_t)&idt_entries;

    for(int i = 0; i < 265; i++){
        idt_set_gate(i, 0, 0x08, 0x8E, 0);
    }

    idt_set_gate(0, (uint64_t)isr0, 0x08, 0x8E, 0);
    idt_set_gate(1, (uint64_t)isr1, 0x08, 0x8E, 0);
    idt_set_gate(2, (uint64_t)isr2, 0x08, 0x8E, 0);
    idt_set_gate(3, (uint64_t)isr3, 0x08, 0x8E, 0);
    idt_set_gate(4, (uint64_t)isr4, 0x08, 0x8E, 0);
    idt_set_gate(5, (uint64_t)isr5, 0x08, 0x8E, 0);
    idt_set_gate(6, (uint64_t)isr6, 0x08, 0x8E, 0);
    idt_set_gate(7, (uint64_t)isr7, 0x08, 0x8E, 0);
    idt_set_gate(8, (uint64_t)isr8, 0x08, 0x8E, 2);
    idt_set_gate(9, (uint64_t)isr9, 0x08, 0x8E, 0);
    idt_set_gate(10, (uint64_t)isr10, 0x08, 0x8E, 0);
    idt_set_gate(11, (uint64_t)isr11, 0x08, 0x8E, 0);
    idt_set_gate(12, (uint64_t)isr12, 0x08, 0x8E, 0);
    idt_set_gate(13, (uint64_t)isr13, 0x08, 0x8E, 0);
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E, 0);
    idt_set_gate(15, (uint64_t)isr15, 0x08, 0x8E, 0);
    idt_set_gate(16, (uint64_t)isr16, 0x08, 0x8E, 0);
    idt_set_gate(17, (uint64_t)isr17, 0x08, 0x8E, 0);
    idt_set_gate(18, (uint64_t)isr18, 0x08, 0x8E, 0);
    idt_set_gate(19, (uint64_t)isr19, 0x08, 0x8E, 0);
    idt_set_gate(20, (uint64_t)isr20, 0x08, 0x8E, 0);
    idt_set_gate(21, (uint64_t)isr21, 0x08, 0x8E, 0);
    idt_set_gate(22, (uint64_t)isr22, 0x08, 0x8E, 0);
    idt_set_gate(23, (uint64_t)isr23, 0x08, 0x8E, 0);
    idt_set_gate(24, (uint64_t)isr24, 0x08, 0x8E, 0);
    idt_set_gate(25, (uint64_t)isr25, 0x08, 0x8E, 0);
    idt_set_gate(26, (uint64_t)isr26, 0x08, 0x8E, 0);
    idt_set_gate(27, (uint64_t)isr27, 0x08, 0x8E, 0);
    idt_set_gate(28, (uint64_t)isr28, 0x08, 0x8E, 0);
    idt_set_gate(29, (uint64_t)isr29, 0x08, 0x8E, 0);
    idt_set_gate(30, (uint64_t)isr30, 0x08, 0x8E, 0);
    idt_set_gate(31, (uint64_t)isr31, 0x08, 0x8E, 0);

    load_idt(&idt);
    log(INFO, "IDT initialised\n");
    outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);

    idt_set_gate(32, (uint64_t)irq0, 0x08, 0x8E, 0);
	idt_set_gate(33, (uint64_t)irq1, 0x08, 0x8E, 0);
	idt_set_gate(34, (uint64_t)irq2, 0x08, 0x8E, 0);
	idt_set_gate(35, (uint64_t)irq3, 0x08, 0x8E, 0);
	idt_set_gate(36, (uint64_t)irq4, 0x08, 0x8E, 0);
	idt_set_gate(37, (uint64_t)irq5, 0x08, 0x8E, 0);
	idt_set_gate(38, (uint64_t)irq6, 0x08, 0x8E, 0);
	idt_set_gate(39, (uint64_t)irq7, 0x08, 0x8E, 0);
	idt_set_gate(40, (uint64_t)irq8, 0x08, 0x8E, 0);
	idt_set_gate(41, (uint64_t)irq9, 0x08, 0x8E, 0);
	idt_set_gate(42, (uint64_t)irq10, 0x08, 0x8E, 0);
	idt_set_gate(43, (uint64_t)irq11, 0x08, 0x8E, 0);
	idt_set_gate(44, (uint64_t)irq12, 0x08, 0x8E, 0);
	idt_set_gate(45, (uint64_t)irq13, 0x08, 0x8E, 0);
	idt_set_gate(46, (uint64_t)irq14, 0x08, 0x8E, 0);
	idt_set_gate(47, (uint64_t)irq15, 0x08, 0x8E, 0);
    asm("sti");
    log(INFO, "Interrupts initialised\n");
}

void irq_install_handler(int irq, void (*handler)(register_t* regs)){
    interrupt_handlers[irq] = handler;
}

/*nuke the handlers*/
void irq_uninstall_handler(int irq){
    interrupt_handlers[irq] = 0;
}

void send_eoi(int irq){
    if (irq >= 40){       
        outb(0xA0, 0x20);   
    }
    outb(0x20, 0x20);
}

void irq_handler(register_t* regs){
    void (*handler)(register_t* regs);
    handler = interrupt_handlers[regs->int_no - 32];
    if(handler){
        handler(regs);
    }
    else{
        send_eoi(regs->int_no);
    }
}

const char *exception_messages[] =
{
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",

    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",

    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",

    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

void fault_handler(register_t* regs)
{
    if (regs->int_no < 32){    
        log(ERROR, "Exception Raised! %s exception. Error code: %d\nSystem Halted!\n", exception_messages[regs->int_no], regs->int_no);/*raise an error*/
        for (;;);   /*halt the system*/
    }
}