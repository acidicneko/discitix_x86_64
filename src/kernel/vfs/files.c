#include <kernel/vfs/vfs.h>
#include <libk/string.h>
#include <kernel/sched/scheduler.h>
#include <mm/pmm.h>
#include <mm/liballoc.h>
#include <libk/errno.h>

int vfs_open(file_t **file, inode_t *inode, uint32_t flags) {
    if (!inode || !file) return -EINVAL;
    file_t *f = (file_t *)kmalloc(sizeof(file_t));
    if (!f) return -ENOMEM;
    memset(f, 0, sizeof(file_t));
    f->inode = inode;
    f->flags = flags;
    f->f_ops = inode->f_ops;
    f->offset = 0;
    f->refcount = 1;
    *file = f;
    return 0;
}

int vfs_close(file_t *file) {
    if (!file || !file->inode) return -EBADF;
    if (--file->refcount > 0) return 0;
    if (file->inode->f_ops && file->inode->f_ops->close) {
        file->inode->f_ops->close(file->inode);
    }
    kfree(file);
    return 0;
}

long vfs_read(file_t *file, void *buf, size_t len) {
    if (!file || !file->inode) return -EBADF;
    if ((file->flags & O_ACCMODE) == O_WRONLY) return -EBADF;
    if (file->inode->type == FT_DIR) return -EISDIR;
    if (!file->inode->f_ops || !file->inode->f_ops->read) return -EINVAL;

    long ret = file->inode->f_ops->read(file, buf, len, file->offset);
    if (ret > 0)
        file->offset += ret;
    return ret;
}

long vfs_write(file_t *file, const void *buf, size_t len) {
    if (!file || !file->inode) return -EBADF;
    if ((file->flags & O_ACCMODE) == O_RDONLY) return -EBADF;
    if (file->inode->type == FT_DIR) return -EISDIR;
    if (!file->inode->f_ops || !file->inode->f_ops->write) return -EINVAL;
    if (len == 0) return 0;

    if (file->flags & O_APPEND)
        file->offset = file->inode->size;

    long ret = file->inode->f_ops->write(file, buf, len, file->offset);
    if (ret > 0)
        file->offset += (uint64_t)ret;
    return ret;
}

int vfs_truncate(inode_t *inode, uint64_t length) {
    if (!inode) return -EINVAL;
    if (inode->type == FT_DIR) return -EISDIR;
    if (inode->type != FT_REG) return -EINVAL;
    if (!inode->f_ops || !inode->f_ops->truncate) return -EROFS;
    return inode->f_ops->truncate(inode, length);
}

int vfs_create(const char *path, uint32_t mode) {
    char buf[PATH_MAX];
    char *file_name;
    int err = 0;
    dentry_t *parent_dentry = vfs_resolve_parent(path, buf, &file_name, &err);
    if (!parent_dentry) return err;
    if (!parent_dentry->inode->i_ops || !parent_dentry->inode->i_ops->create) return -EROFS;

    return parent_dentry->inode->i_ops->create(parent_dentry->inode, file_name, mode);
}
