#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#define LEFT_SHIFT_PRESSED 		0x2A
#define LEFT_SHIFT_RELEASED 	0xAA
#define RIGHT_SHIFT_PRESSED 	0x36
#define RIGHT_SHIFT_RELEASED 	0xB6
#define CAPS_LOCK 				0x3A

#define LEFT_CTRL_PRESSED       0x1D
#define LEFT_CTRL_RELEASED      0x9D
#define LEFT_ALT_PRESSED        0x38
#define LEFT_ALT_RELEASED       0xB8

// Number key scancodes (1-4 for TTY switching)
#define KEY_1                   0x02
#define KEY_2                   0x03
#define KEY_3                   0x04
#define KEY_4                   0x05

// Extended scancode prefix
#define EXTENDED_SCANCODE       0xE0

// Arrow key scancodes (after 0xE0 prefix)
#define KEY_UP                  0x48
#define KEY_DOWN                0x50
#define KEY_LEFT                0x4B
#define KEY_RIGHT               0x4D

// Home/End/PgUp/PgDn (after 0xE0 prefix)
#define KEY_HOME                0x47
#define KEY_END                 0x4F
#define KEY_PGUP                0x49
#define KEY_PGDN                0x51
#define KEY_INSERT              0x52
#define KEY_DELETE              0x53

#include <arch/x86_64/regs.h>
#include <stdint.h>

void keyboard_install(void);
void keyboard_handler(register_t* regs);
void handleKey_normal(uint8_t scancode);
char keyboard_read();

#endif