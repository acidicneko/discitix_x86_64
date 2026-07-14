#include <kernel/vfs/vfs.h>
#include <libk/string.h>
#include <kernel/sched/scheduler.h>
#include <mm/pmm.h>
#include <mm/liballoc.h>

int vfs_open(file_t **file, inode_t *inode, uint32_t flags) {
    if (!inode || !file) return -1;
    file_t *f = (file_t *)kmalloc(sizeof(file_t));
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
    kfree(file);
    return 0;
}

long vfs_read(file_t *file, void *buf, size_t len) {
    if (!file || !file->inode) return -1;
    if (!file->inode->f_ops || 
        !file->inode->f_ops->read) return -1;
    long ret = file->inode->f_ops->read(
        file,
        buf,
        len,
        file->offset
    );

    if (ret > 0)
        file->offset += ret;

    return ret;
}

long vfs_write(file_t *file, const void *buf, size_t len) {
    if (!file || !file->inode) return -1;
    if (!file->inode->f_ops || 
        !file->inode->f_ops->write) return -1;

    long ret = file->inode->f_ops->write(
        file,
        buf,
        len,
        file->offset
    );

    if (ret > 0)
        file->offset += (uint64_t)ret;

    return ret;
}


int vfs_create(const char *path, uint32_t mode) {
    if (!path) return -1;

    char path_copy[PATH_MAX];
    strncpy(path_copy, path, PATH_MAX - 1);
    path_copy[PATH_MAX - 1] = '\0';

    char *last_slash = NULL;
    for (int i = strlen(path_copy) - 1; i >= 0; i--) {
        if (path_copy[i] == '/') {
            last_slash = &path_copy[i];
            break;
        }
    }

    char *file_name = path_copy;
    dentry_t *parent_dentry = NULL;

    if (last_slash) {
        *last_slash = '\0';
        file_name = last_slash + 1;
        if (last_slash == path_copy) {
            parent_dentry = vfs_get_root_dentry();
        } else {
            parent_dentry = vfs_get_dentry(path_copy);
        }
    } else {
        task_t *task = get_current_task();
        parent_dentry = (task && task->cwd) ? (dentry_t *)task->cwd : vfs_get_root_dentry();
    }

    if (!parent_dentry || !parent_dentry->inode || !parent_dentry->inode->i_ops->create) {
        return -1; // Parent directory invalid or filesystem is read-only
    }

    // Pass the request down to the FAT32 driver to write the sectors!
    return parent_dentry->inode->i_ops->create(parent_dentry->inode, file_name, mode);
}
