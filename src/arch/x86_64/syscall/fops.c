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
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_APPEND    0x0008
#define O_CREAT     0x0200  // 512 in decimal
#define O_TRUNC     0x0400  // 1024 in decimal

#define ENOENT 2


int64_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t mode,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)mode; (void)arg4; (void)arg5; (void)arg6;
    log("SYS_OPEN",INFO,"path='%s', flags=0x%xl (decimal %d)\n\r", (const char*)path_ptr, flags, flags);
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
        log("SYS_OPEN",ERROR,"no free fd\n\r");
        return -1;  // No free file descriptors
    }
    
    inode_t *inode = NULL;
    if (vfs_lookup_path(path, &inode) != 0 || !inode) {
      if (flags & O_CREAT) {
            if (vfs_create(path, mode) != 0) {
                log("SYS_OPEN",ERROR, "vfs_create failed for %s\n\r", path);
                return -1;
            }
            if (vfs_lookup_path(path, &inode) != 0 || !inode) {
                return -1;
            }
        } else {
            log("SYS_OPEN", ERROR,"file not found: %s\n\r", path);
            return -ENOENT; 
        }
    } 
    file_t *f = NULL;
    if (vfs_open(&f, inode, (uint32_t)flags) != 0 || !f) {
        log("SYS_OPEN",ERROR, "vfs_open failed\n\r");
        return -1;
    }
    
    current->fd_table[fd] = f;
    
    log("SYS_OPEN",INFO, "opened '%s' as fd %d\n\r", path, fd);
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

int64_t sys_mkdir(uint64_t path_ptr, uint64_t mode, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)mode; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    const char *path = (const char*)path_ptr;
    task_t *current = get_current_task();
    
    if (!current || !path) return -1;
    
    log("SYS_MKDIR", INFO, "path='%s'", path);
    
    if (vfs_mkdir(path) != 0) {
        return -1;     
    }

    
    return 0; // Success!
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
        log("SYS_GETDETNS64", ERROR, "fd %d is not a directory\n\r", (int)fd);
        return -1;
    }
    if (!inode->i_ops || !inode->i_ops->getdents) {
        log("SYS_GETDENTS64",ERROR ,"filesystem does not support getdents\n\r");
        return -1;
    }

    return inode->i_ops->getdents(inode, &f->offset, (void*)buf_ptr, count);
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

int64_t sys_unlink(uint64_t path_ptr, uint64_t arg2, uint64_t arg3,
                   uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    const char *path = (const char*)path_ptr;
    if (!path) return -1;
    
    if (vfs_unlink(path) != 0) {
        return -ENOENT;
    }
    return 0;
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define EINVAL 22

int64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    if (!current) return -1;
    
    if (fd >= MAX_FDS || !current->fd_table[fd]) {
        return -1;
    }
    
    file_t *f = current->fd_table[fd];
    int64_t new_offset = f->offset;
    
    switch (whence) {
        case SEEK_SET:
            new_offset = (int64_t)offset;
            break;
        case SEEK_CUR:
            new_offset += (int64_t)offset;
            break;
        case SEEK_END:
            if (f->inode) {
                new_offset = f->inode->size + (int64_t)offset;
            } else {
                return -EINVAL;
            }
            break;
        default:
            return -EINVAL;
    }
    
    // Prevent seeking before the start of the file
    if (new_offset < 0) {
        return -EINVAL;
    }
    
    // Update the file descriptor's offset
    f->offset = (uint32_t)new_offset;
    
    return new_offset;
}
