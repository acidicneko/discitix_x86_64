#ifndef __ISR_H__
#define __ISR_H__

#include "arch/x86_64/regs.h"

void init_isr();
void fault_handler(register_t* regs);

#endif