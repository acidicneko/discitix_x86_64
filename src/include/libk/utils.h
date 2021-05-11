#ifndef __UTILS_H__
#define __UTILS_H__

#define	ERROR	0
#define INFO	1

#include <stdarg.h>

void sysfetch();
void log(int status, char *fmt, ...);
void dbgln(char* fmt, ...);

#endif