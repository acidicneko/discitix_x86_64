#ifndef __STRING_H__
#define __STRING_H__

#include <stdint.h>
#include <stddef.h>

char* utoa(uint64_t value, char *str, uint8_t base);
char* itoa(int value, char* str, int base);

void* memset(void* bufptr, int value, size_t size);
uint16_t *memsetw(uint16_t *dest, uint16_t val, size_t count);
uint8_t *memcpy(uint8_t *dest, const uint8_t *src, size_t n);

#endif
