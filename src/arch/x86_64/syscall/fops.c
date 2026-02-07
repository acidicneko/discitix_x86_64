#include <arch/x86_64/syscall.h>
#include <drivers/tty/tty.h>
#include <kernel/sched/scheduler.h>
#include <libk/utils.h>
#include <libk/string.h>
#include <kernel/vfs/vfs.h>
#include <arch/x86_64/regs.h>

struct user_stat {
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


#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_IFCHR  0020000
#define S_IFBLK  0060000
#define S_IFLNK  0120000
#define S_IFIFO  0010000
#define S_IFSOCK 0140000

struct linux_dirent64 {
    uint64_t d_ino;      // Inode number
    int64_t  d_off;      // Offset to next dirent
    uint16_t d_reclen;   // Length of this dirent
    uint8_t  d_type;     // File type
    char     d_name[];   // Filename (null-terminated)
};

#define DT_UNKNOWN 0
#define DT_REG     8   // Regular file
#define DT_DIR     4   // Directory


int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    // TODO: validate user pointer
    const char* user_buf = (const char*)buf;
    
    // FIXME: this is probably redundant as we can just write to /ttyX files
    if (fd == 1 || fd == 2) {
        // stdout or stderr - write to current TTY
        tty_t* tty = get_current_tty();
        if (tty) {
            for (size_t i = 0; i < count; i++) {
                tty_putchar(user_buf[i]);
            }
            return (int64_t)count;
        }
        return -1;
    }
    
    // Handle other file descriptors via VFS
    task_t *current = get_current_task();
    if (!current || fd >= MAX_FDS || !current->fd_table[fd]) {
        return -1;
    }
    
    file_t *f = current->fd_table[fd];
    return vfs_write(f, user_buf, count);
}

int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    char* user_buf = (char*)buf;
    task_t *current = get_current_task();
    
    if (!current) return -1;
    
    // Handle all file descriptors via fd_table (0=stdin, 1=stdout, 2=stderr, 3+=files)
    if (fd >= MAX_FDS || !current->fd_table[fd]) {
        return -1;
    }
    
    file_t *f = current->fd_table[fd];
    return vfs_read(f, user_buf, count);
}

int64_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t mode,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)mode; (void)arg4; (void)arg5; (void)arg6;
    
    const char *path = (const char*)path_ptr;
    task_t *current = get_current_task();
    
    if (!current || !path) return -1;
    
    // Find free file descriptor (start from 3, 0-2 are std streams)
    int fd = -1;
    for (int i = 3; i < MAX_FDS; i++) {
        if (!current->fd_table[i]) {
            fd = i;
            break;
        }
    }
    
    if (fd < 0) {
        dbgln("sys_open: no free fd\n\r");
        return -1;  // No free file descriptors
    }
    
    inode_t *inode = NULL;
    if (vfs_lookup_path(path, &inode) != 0 || !inode) {
        dbgln("sys_open: file not found: %s\n\r", path);
        return -1;
    }
    
    file_t *f = NULL;
    if (vfs_open(&f, inode, (uint32_t)flags) != 0 || !f) {
        dbgln("sys_open: vfs_open failed\n\r");
        return -1;
    }
    
    current->fd_table[fd] = f;
    
    dbgln("sys_open: opened '%s' as fd %d\n\r", path, fd);
    return fd;
}

int64_t sys_close(uint64_t fd, uint64_t arg2, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    
    if (!current) return -1;
    
    // Don't allow closing stdin/stdout/stderr this way
    if (fd < 3) return -1;
    
    if (fd >= MAX_FDS || !current->fd_table[fd]) {
        return -1;  // Invalid fd
    }
    
    file_t *f = current->fd_table[fd];
    vfs_close(f);
    current->fd_table[fd] = NULL;
    
    return 0;
}


static void fill_stat_from_inode(struct user_stat *st, inode_t *inode) {
    st->st_ino = inode->ino;
    st->st_mode = inode->mode & 0777;  // Permission bits only
    
    // Set file type bits based on inode->type
    switch (inode->type) {
        case FT_DIR:  st->st_mode |= S_IFDIR;  break;
        case FT_CHR:  st->st_mode |= S_IFCHR;  break;
        case FT_BLK:  st->st_mode |= S_IFBLK;  break;
        case FT_LNK:  st->st_mode |= S_IFLNK;  break;
        case FT_FIFO: st->st_mode |= S_IFIFO;  break;
        case FT_SOCK: st->st_mode |= S_IFSOCK; break;
        case FT_REG:
        default:
            // Fall back to is_directory for legacy inodes without type set
            if (inode->is_directory) {
                st->st_mode |= S_IFDIR;
            } else {
                st->st_mode |= S_IFREG;
            }
            break;
    }
    
    st->st_uid = inode->uid;
    st->st_gid = inode->gid;
    st->st_size = inode->size;
    st->st_atime = inode->atime;
    st->st_mtime = inode->mtime;
    st->st_ctime = inode->ctime;
    st->st_blocks = inode->blocks;
    st->st_blksize = inode->block_size ? inode->block_size : 4096;
}

// stat - get file status by path
int64_t sys_stat(uint64_t path_ptr, uint64_t buf_ptr, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    const char *path = (const char*)path_ptr;
    struct user_stat *buf = (struct user_stat*)buf_ptr;
    
    if (!path || !buf) return -1;
    
    // Look up path in VFS
    inode_t *inode = NULL;
    if (vfs_lookup_path(path, &inode) != 0 || !inode) {
        return -1;
    }
    
    fill_stat_from_inode(buf, inode);
    return 0;
}

// fstat - get file status by file descriptor
int64_t sys_fstat(uint64_t fd, uint64_t buf_ptr, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    struct user_stat *buf = (struct user_stat*)buf_ptr;
    
    task_t *current = get_current_task();
    if (!current || fd >= MAX_FDS || !current->fd_table[fd]) {
        return -1;
    }
    
    file_t *f = current->fd_table[fd];
    if (!f->inode) {
        return -1;
    }
    
    fill_stat_from_inode(buf, f->inode);
    return 0;
}

int64_t sys_getdents64(uint64_t fd, uint64_t buf_ptr, uint64_t count,
                       uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    if (!current || fd >= MAX_FDS || !current->fd_table[fd]) {
        return -1;
    }
    
    file_t *f = current->fd_table[fd];
    inode_t *inode = f->inode;
    
    if (!inode || !inode->is_directory) {
        dbgln("sys_getdents64: fd %d is not a directory\n\r", (int)fd);
        return -1;
    }
    
    // For stripfs, the directory entries are stored as dentries
    dentry_t *dir_dentry = (dentry_t*)inode->private;
    if (!dir_dentry) return -1;
    
    char *buf = (char*)buf_ptr;
    size_t pos = 0;
    
    // Skip to the current offset (f->offset = number of entries already returned)
    dentry_t *child = dir_dentry->children;
    uint64_t skip = f->offset;
    while (child && skip > 0) {
        child = child->next;
        skip--;
    }
    
    // Fill buffer with directory entries
    while (child && pos < count) {
        size_t name_len = strlen(child->name);
        size_t reclen = (sizeof(struct linux_dirent64) + name_len + 1 + 7) & ~7;  // 8-byte align
        
        if (pos + reclen > count) break;  // No more space
        
        struct linux_dirent64 *dirent = (struct linux_dirent64*)(buf + pos);
        dirent->d_ino = child->inode ? child->inode->ino : 0;
        dirent->d_off = f->offset + 1;
        dirent->d_reclen = (uint16_t)reclen;
        // Use inode->type if set, otherwise fall back to is_directory check
        if (child->inode && child->inode->type != 0) {
            dirent->d_type = child->inode->type;
        } else {
            dirent->d_type = (child->inode && child->inode->is_directory) ? DT_DIR : DT_REG;
        }
        memcpy((uint8_t*)dirent->d_name, (const uint8_t*)child->name, name_len + 1);
        
        pos += reclen;
        f->offset++;
        child = child->next;
    }
    
    return (int64_t)pos;
}

int64_t sys_chdir(uint64_t path_ptr, uint64_t arg2, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    const char *path = (const char *)path_ptr;
    if (!path) return -1;
    
    return vfs_chdir(path);
}

int64_t sys_getcwd(uint64_t buf_ptr, uint64_t size, uint64_t arg3,
                   uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    char *buf = (char *)buf_ptr;
    if (!buf || size == 0) return -1;
    
    if (vfs_getcwd(buf, size) != 0) {
        return -1;
    }
    
    return (int64_t)buf_ptr;  // Return pointer to buffer on success (like Linux)
}
