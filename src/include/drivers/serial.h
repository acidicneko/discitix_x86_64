#ifndef __SERIAL_H__
#define __SERIAL_H__

#define COM1_PORT 0x3F8
#define COM2_PORT 0x2F8

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <kernel/vfs/vfs.h>

int serial_init(uint16_t port);
bool is_serial_initialized();
void serial_putchar(char c);
void serial_puts(const char* str);
void serial_printf(char* fmt, ...);
long serial_write(file_t* file, const void* buf, size_t count, uint64_t off);
void init_serial_device();

#endif