#include <arch/x86_64/syscall.h>
#include <drivers/tty/tty.h>
#include <kernel/sched/scheduler.h>
#include <libk/utils.h>
#include <libk/string.h>
#include <kernel/vfs/vfs.h>
#include <arch/x86_64/regs.h>
#include <libk/errno.h>

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
    
    task_t *current = get_current_task();
    if (!current || fd >= MAX_FDS || !current->fd_table[fd]) {
        return -EBADF;
    }
    file_t *f = current->fd_table[fd];
    return vfs_write(f, user_buf, count);
}

int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    char* user_buf = (char*)buf;

    task_t *current = get_current_task();
    if (!current) return -ESRCH;
    if (fd >= MAX_FDS || !current->fd_table[fd]) return -EBADF;

    file_t *f = current->fd_table[fd];
    return vfs_read(f, user_buf, count);
}


int64_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t mode,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    const char *path = (const char*)path_ptr;
    task_t *current = get_current_task();

    if (!current) return -ESRCH;
    if (!path) return -EFAULT;
    log("SYS_OPEN",INFO,"path='%s', flags=0x%xl\n\r", path, flags);

    int fd = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (!current->fd_table[i]) {
            fd = i;
            break;
        }
    }
    if (fd < 0) return -EMFILE;

    inode_t *inode = NULL;
    int err = vfs_lookup_path(path, &inode);
    if (err == 0 && (flags & O_CREAT) && (flags & O_EXCL)) return -EEXIST;
    if (err == -ENOENT && (flags & O_CREAT)) {
        err = vfs_create(path, mode);
        if (err == 0) err = vfs_lookup_path(path, &inode);
    }
    if (err != 0) {
        log("SYS_OPEN", ERROR, "open '%s' failed: %d\n\r", path, err);
        return err;
    }

    int writing = (flags & O_ACCMODE) != O_RDONLY;
    if (inode->type == FT_DIR && writing) return -EISDIR;
    if ((flags & O_DIRECTORY) && inode->type != FT_DIR) return -ENOTDIR;

    if ((flags & O_TRUNC) && writing && inode->type == FT_REG) {
        err = vfs_truncate(inode, 0);
        if (err != 0) return err;
    }

    file_t *f = NULL;
    err = vfs_open(&f, inode, (uint32_t)flags);
    if (err != 0) return err;

    current->fd_table[fd] = f;
    current->fd_cloexec &= ~(1u << fd);
    return fd;
}

int64_t sys_ftruncate(uint64_t fd, uint64_t length, uint64_t arg3,
                      uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    task_t *current = get_current_task();
    if (!current) return -ESRCH;
    if (fd >= MAX_FDS || !current->fd_table[fd]) return -EBADF;
    if ((int64_t)length < 0) return -EINVAL;

    file_t *f = current->fd_table[fd];
    if ((f->flags & O_ACCMODE) == O_RDONLY) return -EINVAL;
    return vfs_truncate(f->inode, length);
}

int64_t sys_close(uint64_t fd, uint64_t arg2, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    
    if (!current) return -ESRCH;
    
    // fds 0-2 can be closed like any other: files are refcounted, so this
    // only drops this process's reference to the terminal.
    if (fd >= MAX_FDS || !current->fd_table[fd]) {
        return -EBADF;
    }
    
    file_t *f = current->fd_table[fd];
    vfs_close(f);
    current->fd_table[fd] = NULL;
    current->fd_cloexec &= ~(1u << fd);
    
    return 0;
}

int64_t sys_mkdir(uint64_t path_ptr, uint64_t mode, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)mode; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    const char *path = (const char*)path_ptr;
    if (!path) return -EFAULT;

    log("SYS_MKDIR", INFO, "path='%s'\n\r", path);
    return vfs_mkdir(path);
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
    
    if (!path || !buf) return -EFAULT;

    inode_t *inode = NULL;
    int err = vfs_lookup_path(path, &inode);
    if (err != 0) return err;
    
    fill_stat_from_inode(buf, inode);
    return 0;
}

// fstat - get file status by file descriptor
int64_t sys_fstat(uint64_t fd, uint64_t buf_ptr, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    struct user_stat *buf = (struct user_stat*)buf_ptr;
    if (!buf) return -EFAULT;

    task_t *current = get_current_task();
    if (!current || fd >= MAX_FDS || !current->fd_table[fd]) return -EBADF;

    file_t *f = current->fd_table[fd];
    if (!f->inode) return -EBADF;
    
    fill_stat_from_inode(buf, f->inode);
    return 0;
}

int64_t sys_getdents64(uint64_t fd, uint64_t buf_ptr, uint64_t count,
                       uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    if (!current || fd >= MAX_FDS || !current->fd_table[fd]) return -EBADF;
    if (!buf_ptr) return -EFAULT;

    file_t *f = current->fd_table[fd];
    inode_t *inode = f->inode;

    if (!inode || !inode->is_directory) return -ENOTDIR;
    if (!inode->i_ops || !inode->i_ops->getdents) return -EINVAL;

    return inode->i_ops->getdents(inode, &f->offset, (void*)buf_ptr, count);
}


int64_t sys_chdir(uint64_t path_ptr, uint64_t arg2, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    const char *path = (const char *)path_ptr;
    if (!path) return -EFAULT;

    return vfs_chdir(path);
}

int64_t sys_getcwd(uint64_t buf_ptr, uint64_t size, uint64_t arg3,
                   uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    char *buf = (char *)buf_ptr;
    int err = vfs_getcwd(buf, size);
    if (err != 0) return err;
    
    return (int64_t)buf_ptr;  // Return pointer to buffer on success (like Linux)
}

int64_t sys_unlink(uint64_t path_ptr, uint64_t arg2, uint64_t arg3,
                   uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    const char *path = (const char*)path_ptr;
    if (!path) return -EFAULT;

    return vfs_unlink(path);
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    task_t *current = get_current_task();
    if (!current) return -ESRCH;
    if (fd >= MAX_FDS || !current->fd_table[fd]) return -EBADF;

    file_t *f = current->fd_table[fd];
    if (f->inode && (f->inode->type == FT_FIFO || f->inode->type == FT_CHR)) return -ESPIPE;
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
    f->offset = (uint64_t)new_offset;
    
    return new_offset;
}


int64_t sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t arg3,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;

    task_t *current = get_current_task();
    if (!current) return -ESRCH;

    if (oldfd >= MAX_FDS || newfd >= MAX_FDS) return -EBADF;
    if (!current->fd_table[oldfd]) return -EBADF;

    if (oldfd == newfd) return (int64_t)newfd; // no-op, POSIX-correct

    // Drop our reference to whatever newfd held. vfs_close only runs the
    // driver close hook (and frees the file) when this was the last ref.
    if (current->fd_table[newfd]) {
        vfs_close(current->fd_table[newfd]);
        current->fd_table[newfd] = NULL;
    }

    current->fd_table[newfd] = current->fd_table[oldfd];
    current->fd_table[newfd]->refcount++; 
    current->fd_cloexec &= ~(1u << newfd);  // dup2 never copies FD_CLOEXEC

    return (int64_t)newfd;
}

// fcntl command numbers as defined by newlib (userland ABI)
#define FCNTL_F_DUPFD          0
#define FCNTL_F_GETFD          1
#define FCNTL_F_SETFD          2
#define FCNTL_F_GETFL          3
#define FCNTL_F_SETFL          4
#define FCNTL_F_DUPFD_CLOEXEC  14
#define FCNTL_FD_CLOEXEC       1

int64_t sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;

    task_t *current = get_current_task();
    if (!current) return -EBADF;
    if (fd >= MAX_FDS || !current->fd_table[fd]) return -EBADF;

    switch (cmd) {
    case FCNTL_F_DUPFD:
    case FCNTL_F_DUPFD_CLOEXEC: {
        // Lowest free descriptor >= arg
        if (arg >= MAX_FDS) return -EINVAL;
        for (uint64_t newfd = arg; newfd < MAX_FDS; newfd++) {
            if (current->fd_table[newfd]) continue;
            current->fd_table[newfd] = current->fd_table[fd];
            current->fd_table[newfd]->refcount++;
            if (cmd == FCNTL_F_DUPFD_CLOEXEC)
                current->fd_cloexec |= (1u << newfd);
            else
                current->fd_cloexec &= ~(1u << newfd);
            return (int64_t)newfd;
        }
        return -EMFILE;
    }
    case FCNTL_F_GETFD:
        return (current->fd_cloexec & (1u << fd)) ? FCNTL_FD_CLOEXEC : 0;
    case FCNTL_F_SETFD:
        if (arg & FCNTL_FD_CLOEXEC) current->fd_cloexec |= (1u << fd);
        else                        current->fd_cloexec &= ~(1u << fd);
        return 0;
    case FCNTL_F_GETFL:
        return (int64_t)current->fd_table[fd]->flags;
    case FCNTL_F_SETFL:
        return 0;  // O_NONBLOCK/O_APPEND not supported yet; accept silently
    default:
        return -EINVAL;
    }
}

int64_t sys_ioctl(uint64_t fd, uint64_t request, uint64_t arg,
                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    task_t *current = get_current_task();
    if (!current) return -ESRCH;
    if (fd >= MAX_FDS || !current->fd_table[fd]) return -EBADF;

    file_t *f = current->fd_table[fd];
    if (!f->inode || !f->inode->f_ops || !f->inode->f_ops->ioctl) return -ENOTTY;
    return f->inode->f_ops->ioctl(f, request, (void *)arg);
}
