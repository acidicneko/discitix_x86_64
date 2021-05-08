#ifndef __PIT_H__
#define __PIT_H__

#include "arch/x86_64/regs.h"
#include <stdint.h>

void pit_handler(register_t* regs);
void pit_install();
void pit_wait(int ticks);
uint64_t get_ticks();

#endif