#ifndef __STRING_H__
#define __STRING_H__

#include <stdint.h>

char* utoa(uint64_t value, char *str, uint8_t base);
char* itoa(int value, char* str, int base);

#endif
