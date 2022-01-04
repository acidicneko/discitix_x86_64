#ifndef __STDIO_H__
#define __STDIO_H__

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

void putchar(char c);
void puts(const char* str);
int __vsprintf__(char *fmt, va_list args, void (*putchar_func)(char c), void (*puts_func)(const char *str));
int printf(char *fmt, ...);
void wait(uint16_t ms);
void gets(char* to);

#endif
