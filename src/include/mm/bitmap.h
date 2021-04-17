#ifndef __BITMAP_H__
#define __BITMAP_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct{
    size_t size;
    uint8_t* buffer;
} bitmap_t;

bool find_bit(bitmap_t* bitmap, uint64_t index);
bool set_bit(bitmap_t* bitmap, uint64_t index, bool value);

#endif