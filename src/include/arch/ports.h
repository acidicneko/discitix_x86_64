#ifndef __PORTS_H__
#define __PORTS_H__

#include <stdint.h>

uint8_t inb(uint16_t _port);
void outb(uint16_t _port, uint8_t _data);

uint16_t inw(uint16_t _port);
void outw(uint16_t _port, uint16_t _data);

#endif