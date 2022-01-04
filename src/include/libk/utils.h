#ifndef __UTILS_H__
#define __UTILS_H__

#define	ERROR	0
#define INFO	1

#include <stdarg.h>
#include <init/stivale2.h>

void sysfetch();
void log(int status, char *fmt, ...);
void dbgln(char* fmt, ...);
void init_arg_parser(struct stivale2_struct* bootinfo);
int arg_exist(char* arg);

#endif