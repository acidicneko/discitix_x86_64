// usr/include/arpa/inet.h
#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#include <stdint.h>

static inline uint16_t htons(uint16_t x) {
    return (uint16_t)((x << 8) | (x >> 8));
}
static inline uint16_t ntohs(uint16_t x) {
    return htons(x);
}
static inline uint32_t htonl(uint32_t x) {
    return ((x & 0x000000FFU) << 24) |
           ((x & 0x0000FF00U) << 8)  |
           ((x & 0x00FF0000U) >> 8)  |
           ((x & 0xFF000000U) >> 24);
}
static inline uint32_t ntohl(uint32_t x) {
    return htonl(x);
}

#endif
