#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#undef errno
extern int errno;

// ============================================================================
// 1. YOUR KERNEL SYSCALL INTERFACE & NUMBERS
// ============================================================================
#define SYS_EXIT       0
#define SYS_READ       1  
#define SYS_WRITE      2
#define SYS_OPEN       3
#define SYS_CLOSE      4
#define SYS_FORK       5
#define SYS_EXEC       6
#define SYS_WAITPID    7
#define SYS_SPAWN      8
#define SYS_BRK        9
#define SYS_MMAP       10
#define SYS_MUNMAP     11
#define SYS_GETDENTS64 12
#define SYS_STAT       13
#define SYS_FSTAT      14
#define SYS_CHDIR      15
#define SYS_GETCWD     16
#define SYS_MKDIR      17
#define SYS_UNLINK     18
#define SYS_LSEEK      19

static inline long _syscall0(long num) {
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory");
    return ret;
}

static inline long _syscall1(long num, long arg1) {
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(arg1) : "rcx", "r11", "memory");
    return ret;
}

static inline long _syscall2(long num, long arg1, long arg2) {
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(arg1), "S"(arg2) : "rcx", "r11", "memory");
    return ret;
}

static inline long _syscall3(long num, long arg1, long arg2, long arg3) {
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3) : "rcx", "r11", "memory");
    return ret;
}

static inline long _syscall6(long num, long arg1, long arg2, long arg3, 
                             long arg4, long arg5, long arg6) {
    long ret;
    register long r10 asm("r10") = arg4;
    register long r8 asm("r8") = arg5;
    register long r9 asm("r9") = arg6;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// ============================================================================
// 2. FILE TYPE & STATUS INTERFACING (ABI-Stable)
// ============================================================================

// Undefine Newlib's time macros so they don't clobber our unique struct field names
#undef st_atime
#undef st_mtime
#undef st_ctime

struct kernel_stat {
    uint32_t st_ino;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_size;
    uint32_t st_atime;
    uint32_t st_mtime;
    uint32_t st_ctime;
    uint32_t st_blocks;
    uint32_t st_blksize;
};

static void _translate_stat(struct stat *dst, struct kernel_stat *src) {
    dst->st_ino     = src->st_ino;
    dst->st_mode    = src->st_mode;
    dst->st_uid     = src->st_uid;
    dst->st_gid     = src->st_gid;
    dst->st_size    = src->st_size;
    
    // Assign explicitly to Newlib's inner struct timespec fields
    dst->st_atim.tv_sec = src->st_atime;
    dst->st_atim.tv_nsec = 0;
    
    dst->st_mtim.tv_sec = src->st_mtime;
    dst->st_mtim.tv_nsec = 0;
    
    dst->st_ctim.tv_sec = src->st_ctime;
    dst->st_ctim.tv_nsec = 0;
    
    dst->st_blocks  = src->st_blocks;
    dst->st_blksize = src->st_blksize;
}
int stat(const char *path, struct stat *buf) {
    struct kernel_stat kst;
    long ret = _syscall2(SYS_STAT, (long)path, (long)&kst);
    if (ret < 0) { errno = (int)-ret; return -1; }
    _translate_stat(buf, &kst);
    return 0;
}

int fstat(int fd, struct stat *buf) {
    struct kernel_stat kst;
    long ret = _syscall2(SYS_FSTAT, (long)fd, (long)&kst);
    if (ret < 0) { errno = (int)-ret; return -1; }
    _translate_stat(buf, &kst);
    return 0;
}



// ============================================================================
// 3. CORE FILE I/O STUBS
// ============================================================================
int open(const char *path, int flags, ...) {
    long ret = _syscall3(SYS_OPEN, (long)path, (long)flags, 0);
    if (ret < 0) { errno = (int)-ret; return -1; }
    return (int)ret;
}

int close(int fd) {
    long ret = _syscall1(SYS_CLOSE, (long)fd);
    if (ret < 0) { errno = (int)-ret; return -1; }
    return (int)ret;
}

int read(int fd, void *buf, size_t count) {
    long ret = _syscall3(SYS_READ, (long)fd, (long)buf, (long)count);
    if (ret < 0) { errno = (int)-ret; return -1; }
    return (int)ret;
}

int write(int fd, const void *buf, size_t count) {
    long ret = _syscall3(SYS_WRITE, (long)fd, (long)buf, (long)count);
    if (ret < 0) { errno = (int)-ret; return -1; }
    return (int)ret;
}

// ============================================================================
// 4. PROCESS HOOKS & SCHEDULER LOOPS
// ============================================================================
void _exit(int status) {
    _syscall1(SYS_EXIT, (long)status);
    while (1) {
        asm volatile("hlt");
    }
}

int fork(void) {
    long ret = _syscall0(SYS_FORK);
    if (ret < 0) { errno = (int)-ret; return -1; }
    return (int)ret;
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    long ret = _syscall3(SYS_EXEC, (long)path, (long)argv, (long)envp);
    if (ret < 0) { errno = (int)-ret; return -1; }
    return (int)ret;
}
int waitpid(int pid, int *status, int options) {
    long ret;
    while (1) {
        ret = _syscall3(SYS_WAITPID, (long)pid, (long)status, (long)options);
        if (ret != -2) {
            if (ret < 0) return -1;
            return (int)ret;
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}

int getpid(void) {
    return 1; // Default fallback representation until SYS_GETPID implementation
}

// Custom Extension Hook (Optional utility preservation)
int spawn(const char *path, char *const argv[]) {
    long ret = _syscall3(SYS_SPAWN, (long)path, (long)argv, 0);
    if (ret < 0) { errno = (int)-ret; return -1; }
    return (int)ret;
}

// ============================================================================
// 5. MEMORY ALLOCATION CONTROL LAYER (sbrk/mmap)
// ============================================================================
extern char _end; // Bound by binary target linker scripts at end of BSS
static char *current_heap_break = &_end;

void *brk(void *addr) {
    long ret = _syscall1(SYS_BRK, (long)addr);
    if (ret == -1 || ret < 0) return (void *)-1;
    return (void *)ret;
}

caddr_t sbrk(int incr) {
    char *prev_break = current_heap_break;
    if (incr == 0) return (caddr_t)prev_break;

    char *next_break = (char *)((long)current_heap_break + incr);
    void *result = brk(next_break);
    if (result == (void *)-1) {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    current_heap_break = next_break;
    return (caddr_t)prev_break;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    long ret = _syscall6(SYS_MMAP, (long)addr, (long)length, (long)prot, 
                        (long)flags, (long)fd, (long)offset);
    if (ret < 0) return (void *)-1; // Maps to MAP_FAILED
    return (void *)ret;
}

int munmap(void *addr, size_t length) {
    long ret = _syscall2(SYS_MUNMAP, (long)addr, (long)length);
    if (ret < 0) { errno = (int)-ret; return -1; }
    return (int)ret;
}

// ============================================================================
// 6. DIRECTORY MANAGEMENT LAYERS
// ============================================================================
int chdir(const char *path) {
    long ret = _syscall1(SYS_CHDIR, (long)path);
    if (ret < 0) { errno = (int)-ret; return -1; }
    return 0;
}

char *getcwd(char *buf, size_t size) {
    long ret = _syscall2(SYS_GETCWD, (long)buf, (long)size);
    if (ret < 0) { errno = (int)-ret; return NULL; }
    return buf;
}

// long getdents64(int fd, void *buf, size_t count) {
//     return _syscall3(SYS_GETDENTS64, (long)fd, (long)buf, (long)count);
// }

// ============================================================================
// 7. COMPULSORY FALLBACK IMPLEMENTATIONS
// ============================================================================
int isatty(int file) {
    if (file >= 0 && file <= 2) return 1;
    errno = ENOTTY;
    return 0;
}


int kill(int pid, int sig) {
    errno = EINVAL;
    return -1;
}

int link(char *old, char *new) {
    errno = EMLINK;
    return -1;
}

clock_t times(struct tms *buf) {
    return (clock_t)-1;
}


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
long getdents64(int fd, void *buf, size_t count) {
    return _syscall3(SYS_GETDENTS64, (long)fd, (long)buf, (long)count);
}

// opendir - open a directory stream
DIR *opendir(const char *name) {
    int fd = (int)_syscall2(SYS_OPEN, (long)name, 0);  // O_RDONLY
    if (fd < 0) return 0;
    
    // Simple static allocation (only one DIR at a time)
    static DIR dir_buf;
    dir_buf.fd = fd;
    dir_buf.buf_pos = 0;
    dir_buf.buf_len = 0;
    return &dir_buf;
}

// readdir - read next directory entry
struct dirent *readdir(DIR *dirp) {
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
int closedir(DIR *dirp) {
    if (!dirp) return -1;
    int ret = (int)_syscall1(SYS_CLOSE, dirp->fd);
    dirp->fd = -1;
    dirp->buf_pos = 0;
    dirp->buf_len = 0;
    return ret;
}

int mkdir(const char *path, mode_t mode) {
    long ret = _syscall6(SYS_MKDIR, (long)path, mode, 0, 0, 0, 0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

int unlink(const char *name) {
    long ret = _syscall6(SYS_UNLINK, (long)name, 0, 0, 0, 0, 0); // Use your SYS_UNLINK number
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}


off_t lseek(int fd, off_t offset, int whence) {
    int64_t ret = _syscall3(SYS_LSEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence);

    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return (off_t)ret;
}
