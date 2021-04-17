#ifndef __STDIO_H__
#define __STDIO_H__

#include <stdarg.h>

void putchar(char c);
void puts(const char* str);
int vsprintf(char *fmt, va_list args);
int printf(char *fmt, ...);


#endif
