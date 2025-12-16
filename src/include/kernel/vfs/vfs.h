#ifndef __VFS_H__
#define __VFS_H__

#include <stddef.h>
#include <stdint.h>

#define NAME_MAX 255
#define PATH_MAX 4096

typedef struct file file_t;
typedef struct inode inode_t;
typedef struct inode_operations inode_operations_t;
typedef struct file_operations file_operations_t;

struct inode{
    uint32_t ino;
    uint32_t size;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;

    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
    
    uint32_t blocks;
    uint32_t block_size;

    inode_operations_t *i_ops;
    file_operations_t *f_ops;
    void *private;

    uint8_t is_directory;
};

typedef struct dentry {
    char name[NAME_MAX];
    inode_t *inode;
    struct dentry *parent;
    struct dentry *next;
} dentry_t;

struct inode_operations {
    inode_t* (*lookup)(inode_t*, const char*);
    int (*create)(inode_t*, const char*, uint32_t);
    int (*mkdir)(inode_t*, const char*);
    int (*unlink)(inode_t*, const char*);
};

struct file {
    inode_t *inode;
    uint32_t flags;
    file_operations_t *f_ops;
    uint64_t offset;
};

struct file_operations {
    int (*open)(inode_t*, uint32_t);
    int (*close)(inode_t*);
    long (*read)(file_t*, void*, size_t, uint64_t);
    long (*write)(file_t*, const void*, size_t, uint64_t);
};

typedef struct {
    char fs_type[16];
    dentry_t *root;
    void* private;
} superblock_t;

typedef struct vfs_mount {
	superblock_t *sb;
	char mount_point[NAME_MAX];
	struct vfs_mount *next;
} vfs_mount_t;


// File related operations
int vfs_open(file_t **file, inode_t *inode, uint32_t flags);
int vfs_close(file_t *file);
long vfs_read(file_t *file, void *buf, size_t len);
long vfs_write(file_t *file, const void *buf, size_t len);
// int vfs_mkdir(inode_t *parent, const char *name);
// int vfs_unlink(inode_t *parent, const char *name);

// Filesystem operations
int vfs_mount(superblock_t *sb, const char *mount_point);
superblock_t *vfs_get_root_superblock();
int vfs_lookup(inode_t *parent, const char *name, inode_t **result_inode);
int vfs_lookup_path(const char *path, inode_t **result_inode);

#endif /* __VFS_H__ */