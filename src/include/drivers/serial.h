#ifndef __SERIAL_H__
#define __SERIAL_H__

#define COM1_PORT 0x3F8
#define COM2_PORT 0x2F8

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

int serial_init(uint16_t port);
bool is_serial_initialized();
void serial_write(const char* data);
void serial_putchar(char c);
void serial_puts(const char* str);
void serial_printf(char* fmt, ...);

#endif