#ifndef _DLIBC_STDIO_H
#define _DLIBC_STDIO_H

#include "syscall.h"
#include "fcntl.h"

#define STDIN  0
#define STDOUT 1
#define STDERR 2

static inline long strlen(const char *s) {
    long len = 0;
    while (*s++) len++;
    return len;
}

static inline long strchr(const char *s, char c) {
    const char *p = s;
    while (*p) {
        if (*p == c) return (long)(p - s);
        p++;
    }
    return -1;
}

static inline long strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static inline void print(const char *msg) {
    write(STDOUT, msg, strlen(msg));
}

static inline void println(const char *msg) {
    print(msg);
    write(STDOUT, "\n", 1);
}

// Read a single character from stdin
static inline int getchar(void) {
    char c;
    if (read(STDIN, &c, 1) == 1) {
        return (unsigned char)c;
    }
    return -1;
}

// Read a line from stdin into buffer (max size-1 chars + null terminator)
// Returns buffer on success, NULL on error/EOF
static inline char* gets(char *buf, int size) {
    if (!buf || size <= 0) return (char*)0;
    
    int i = 0;
    while (i < size - 1) {
        char c;
        long n = read(STDIN, &c, 1);
        if (n <= 0) {
            if (i == 0) return (char*)0;  // EOF with no data
            break;
        }
        if (c == '\n') {
            break;
        }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return buf;
}



// Print a character
static inline void putchar(char c) {
    write(STDOUT, &c, 1);
}

#endif
