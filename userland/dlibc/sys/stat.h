#ifndef _DLIBC_SYS_STAT_H
#define _DLIBC_SYS_STAT_H

#include "../syscall.h"
#include <stdint.h>

/* basic types */
typedef uint32_t mode_t;
typedef uint32_t ino_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef uint32_t off_t;

/* File type bits */
#define S_IFMT    0170000
#define S_IFDIR   0040000
#define S_IFCHR   0020000
#define S_IFBLK   0060000
#define S_IFREG   0100000
#define S_IFIFO   0010000
#define S_IFLNK   0120000
#define S_IFSOCK  0140000

/* File type tests */
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* Permission bits */
#define S_IRWXU  0700
#define S_IRUSR  0400
#define S_IWUSR  0200
#define S_IXUSR  0100

#define S_IRWXG  0070
#define S_IRGRP  0040
#define S_IWGRP  0020
#define S_IXGRP  0010

#define S_IRWXO  0007
#define S_IROTH  0004
#define S_IWOTH  0002
#define S_IXOTH  0001

/* stat structure (ABI-stable) */
struct stat {
    ino_t   st_ino;
    mode_t  st_mode;
    uid_t   st_uid;
    gid_t   st_gid;
    off_t   st_size;
    uint32_t st_atime;
    uint32_t st_mtime;
    uint32_t st_ctime;
    uint32_t st_blocks;
    uint32_t st_blksize;
};

/* syscalls */
static inline int stat(const char *path, struct stat *buf) {
    return (int)syscall2(SYS_STAT, (long)path, (long)buf);
}

static inline int fstat(int fd, struct stat *buf) {
    return (int)syscall2(SYS_FSTAT, (long)fd, (long)buf);
}

#endif
