#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#define LEFT_SHIFT_PRESSED 		0x2A
#define LEFT_SHIFT_RELEASED 	0xAA
#define RIGHT_SHIFT_PRESSED 	0x36
#define RIGHT_SHIFT_RELEASED 	0xB6
#define CAPS_LOCK 				0x3A

#include "arch/x86_64/regs.h"
#include <stdint.h>

void keyboard_install(void);
void keyboard_handler(register_t* regs);
void handleKey_normal(uint8_t scancode);
char keyboard_read();

#endif