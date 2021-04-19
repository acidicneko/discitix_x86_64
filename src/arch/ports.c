#include "arch/ports.h"

inline uint8_t inb(uint16_t _port) {
  unsigned char rv;
  __asm__ __volatile__("inb %1, %0" : "=a"(rv) : "dN"(_port));
  return rv;
}

// Send out byte to adress
inline void outb(uint16_t _port, uint8_t _data) {
  __asm__ __volatile__("outb %1, %0" : : "dN"(_port), "a"(_data));
}

// Read in word from adress
inline uint16_t inw(uint16_t _port) {
  uint16_t rv;
  __asm__ __volatile__("inw %1, %0" : "=a"(rv) : "dN"(_port));
  return rv;
}

// Send out word to adress
inline void outw(unsigned short _port, uint16_t _data) {
  __asm__ __volatile__("outw %1, %0" : : "dN"(_port), "a"(_data));
}