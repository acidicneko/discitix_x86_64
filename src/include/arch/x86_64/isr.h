#ifndef __ISR_H__
#define __ISR_H__

#include <arch/x86_64/regs.h>

void init_isr();
void isr_install_handler(int isr, void (*handler)(register_t* regs));
void isr_uninstall_handler(int isr);
void fault_handler(register_t* regs);

#endif