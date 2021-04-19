#include "libk/string.h"

char * itoa(int value, char *str, int base){
	char *rc;
	char *ptr;
	char *low;
	if(base < 2 || base > 36){
		*str = '\0';
		return str;
	}
	rc = ptr = str;
	if(value < 0 && base == 10){
		*ptr++ = '-';
	}
	low = ptr;
	do{
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
		value /= base;
	} while (value);
	*ptr-- = '\0';
	while(low < ptr){
		char temp = *low;
		*low++ = *ptr;
		*ptr-- = temp;
	}
	return rc;
}

char * utoa(uint64_t value, char *str, uint8_t base){
	char *rc;
	char *ptr;
	char *low;
	if(base < 2 || base > 36){
		*str = '\0';
		return str;
	}
	rc = ptr = str;
	low = ptr;
	do{
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
		value /= base;
	} while (value);
	*ptr-- = '\0';
	while(low < ptr){
		char temp = *low;
		*low++ = *ptr;
		*ptr-- = temp;
	}
	return rc;
}

void* memset(void* bufptr, int value, size_t size) {
    unsigned char* buf = (unsigned char*) bufptr;
    for (size_t i = 0; i < size; i++)
        buf[i] = (unsigned char) value;
    return bufptr;
}

uint16_t *memsetw(uint16_t *dest, uint16_t val, size_t count){
    uint16_t *temp = dest;
    while(count--){   
        *temp++ = val;
    }
    return dest;
}

uint8_t *memcpy(uint8_t *dest, const uint8_t *src, size_t n){
    uint8_t *d = dest;
    const uint8_t *s = src;
    while(n--){
        *d++=*s++;
    }
    return dest;
}