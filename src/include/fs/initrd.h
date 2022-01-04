#ifndef __INITRD_H__
#define __INITRD_H__

#include <stdint.h>
#include <stddef.h>
#include "init/stivale2.h"

#define ARRU_MAGIC 0x69

typedef struct{
    int num;
} __attribute__((packed)) initrd_header_t;

typedef struct{
    uint8_t magic;
    char name[64];
    uint32_t offset;
    uint32_t length;
} __attribute__((packed)) initrd_file_t;

void init_initrd(struct stivale2_struct* bootinfo);
int read_initrd();

#endif