#ifndef __UTILS_H__
#define __UTILS_H__

#define ERROR 0
#define INFO 1

#include <init/stivale2.h>
#include <stdarg.h>

void sysfetch();
void log(int status, char *fmt, ...);
void dbgln(char *fmt, ...);
void init_arg_parser();
int arg_exist(char *arg);

#endif
