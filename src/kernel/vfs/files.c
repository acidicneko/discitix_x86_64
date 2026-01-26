#include <kernel/vfs/vfs.h>
#include <libk/string.h>
#include <mm/pmm.h>

int vfs_open(file_t **file, inode_t *inode, uint32_t flags) {
    if (!inode || !file) return -1;
    file_t *f = (file_t *)pmalloc(1);
    if (!f) return -1;
    memset(f, 0, sizeof(file_t));
    f->inode = inode;
    f->flags = flags;
    f->f_ops = inode->f_ops;
    f->offset = 0;
    *file = f;
    return 0;
}

int vfs_close(file_t *file) {
    if (!file || !file->inode) return -1;
    pmm_free_pages(file, 1);
    return 0;
}

long vfs_read(file_t *file, void *buf, size_t len) {
    if (!file || !file->inode) return -1;
    if (!file->inode->f_ops || !file->inode->f_ops->read) return -1;
    return file->inode->f_ops->read(file, buf, len, file->offset);
}

long vfs_write(file_t *file, const void *buf, size_t len) {
    if (!file || !file->inode) return -1;
    if (!file->inode->f_ops || !file->inode->f_ops->write) return -1;
    return file->inode->f_ops->write(file, buf, len, file->offset);
}
