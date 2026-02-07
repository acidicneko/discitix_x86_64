#ifndef _DLIBC_DIRENT_H
#define _DLIBC_DIRENT_H

#include "syscall.h"
#include <stdint.h>
#include <stddef.h>

// Directory entry types (must match kernel DT_* values)
#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK      10
#define DT_SOCK     12
#define DT_WHT      14

// Linux dirent64 structure - matches kernel's struct linux_dirent64
// Layout: d_ino(8) + d_off(8) + d_reclen(2) + d_type(1) + d_name[]
struct dirent64 {
    uint64_t d_ino;      // Inode number
    int64_t  d_off;      // Offset to next dirent  
    uint16_t d_reclen;   // Length of this dirent (including name)
    uint8_t  d_type;     // File type (DT_*)
    char     d_name[];   // Filename (null-terminated, flexible array)
};

// Simplified dirent for readdir() compatibility
struct dirent {
    uint64_t d_ino;
    uint8_t  d_type;
    char     d_name[256];
};

// Directory stream
typedef struct {
    int fd;
    char buf[1024];
    size_t buf_pos;
    size_t buf_len;
} DIR;

// getdents64 - raw syscall wrapper
static inline long getdents64(int fd, void *buf, size_t count) {
    return syscall3(SYS_GETDENTS64, (long)fd, (long)buf, (long)count);
}

// opendir - open a directory stream
static inline DIR *opendir(const char *name) {
    int fd = (int)syscall2(SYS_OPEN, (long)name, 0);  // O_RDONLY
    if (fd < 0) return 0;
    
    // Simple static allocation (only one DIR at a time)
    static DIR dir_buf;
    dir_buf.fd = fd;
    dir_buf.buf_pos = 0;
    dir_buf.buf_len = 0;
    return &dir_buf;
}

// readdir - read next directory entry
static inline struct dirent *readdir(DIR *dirp) {
    if (!dirp) return 0;
    
    static struct dirent result;
    
    // Refill buffer if exhausted
    if (dirp->buf_pos >= dirp->buf_len) {
        long nread = getdents64(dirp->fd, dirp->buf, sizeof(dirp->buf));
        if (nread <= 0) return 0;
        dirp->buf_len = (size_t)nread;
        dirp->buf_pos = 0;
    }
    
    // Parse current dirent64 entry from buffer
    struct dirent64 *d64 = (struct dirent64*)(dirp->buf + dirp->buf_pos);
    
    result.d_ino = d64->d_ino;
    result.d_type = d64->d_type;
    
    // Clear the name buffer first to avoid leftover characters
    for (size_t i = 0; i < 256; i++) {
        result.d_name[i] = '\0';
    }
    
    // Copy the name (null-terminated by kernel)
    for (size_t i = 0; i < 255 && d64->d_name[i]; i++) {
        result.d_name[i] = d64->d_name[i];
    }
    
    // Advance to next entry
    dirp->buf_pos += d64->d_reclen;
    return &result;
}

// closedir - close directory stream
static inline int closedir(DIR *dirp) {
    if (!dirp) return -1;
    int ret = (int)syscall1(SYS_CLOSE, dirp->fd);
    dirp->fd = -1;
    dirp->buf_pos = 0;
    dirp->buf_len = 0;
    return ret;
}

#endif
