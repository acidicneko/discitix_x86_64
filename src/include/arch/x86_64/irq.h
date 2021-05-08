#ifndef __IRQ_H__
#define __IRQ_H__

#include "arch/x86_64/regs.h"

void init_irq();
void irq_install_handler(int irq, void (*handler)(register_t* regs));
void irq_uninstall_handler(int irq);
void send_eoi(int irq);
void irq_handler(register_t* regs);

#endif