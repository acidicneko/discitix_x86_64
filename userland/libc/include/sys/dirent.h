/* <dirent.h> includes <sys/dirent.h>, which is this file.  On a
   system which supports <dirent.h>, this file is overridden by
   dirent.h in the libc/sys/.../sys directory.  On a system which does
   not support <dirent.h>, we will get this file which uses #error to force
   an error.  */

#ifdef __cplusplus
extern "C" {
#endif
#ifndef _SYS_DIRENT_H_
#define _SYS_DIRENT_H_

#include <stdint.h>
#include <sys/types.h>

#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK      10
#define DT_SOCK     12
#define DT_WHT      14

/* Linux dirent64 structure - matches kernel's struct linux_dirent64
 * Layout: d_ino(8) + d_off(8) + d_reclen(2) + d_type(1) + d_name[] */
struct kernel_dirent {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
};

/* Simplified dirent for readdir() compatibility */
struct dirent {
    uint64_t d_ino;
    uint8_t  d_type;
    char     d_name[256];
};

/* Directory stream handle. newlib's dirent.h (the file that includes
 * this one) only forward-references DIR as an opaque type via pointers
 * (DIR *), so defining the full struct here is sufficient -- no separate
 * typedef needed elsewhere. */
typedef struct {
    int fd;
    char buf[1024];
    size_t buf_pos;
    size_t buf_len;
} DIR;

/* Not part of newlib's own dirent.h declarations -- this is our raw
 * syscall wrapper that readdir() is built on top of, declared here
 * since sys/dirent.h is the OS-specific half of this header pair. */
long getdents64(int fd, void *buf, size_t count);

#endif /* _SYS_DIRENT_H_ */
#ifdef __cplusplus
}
#endif
