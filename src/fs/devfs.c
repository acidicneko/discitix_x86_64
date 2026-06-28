#include <fs/devfs.h>
#include <kernel/vfs/vfs.h>
#include <libk/string.h>
#include <mm/liballoc.h>

static superblock_t devfs_sb;
static dentry_t devfs_root_dentry;
static inode_t devfs_root_inode;

inode_t* devfs_lookup(inode_t *parent, const char *name);
long devfs_getdents(inode_t *dir_inode, uint64_t *offset, void *buf, uint32_t count);

static inode_operations_t devfs_dir_iops = {
    .lookup = devfs_lookup,
    .create = NULL, 
    .mkdir = NULL, 
    .unlink = NULL,
    .getdents = devfs_getdents
};

#define ALIGN_UP(val, align) (((val) + (align) - 1) & ~((align) - 1))

// Look up a device (e.g., "tty1") inside /dev
inode_t* devfs_lookup(inode_t *parent, const char *name) {
    if (!parent || !parent->private) return NULL;
    
    dentry_t *parent_d = (dentry_t *)parent->private;
    dentry_t *child = parent_d->children;
    
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child->inode;
        }
        child = child->next;
    }
    return NULL;
}

long devfs_getdents(inode_t *inode, uint64_t *offset, void *buf_ptr, uint32_t count) {
    if (!inode || inode->type != FT_DIR || !inode->private) return -1;

    dentry_t *parent_d = (dentry_t *)inode->private;
    dentry_t *child = parent_d->children;
    
    // Fast-forward to the current offset
    for (uint64_t i = 0; i < *offset && child; i++) {
        child = child->next;
    }

    char *buf = (char *)buf_ptr;
    uint32_t bytes_written = 0;

    while (child) {
        uint32_t name_len = strlen(child->name);
        uint16_t reclen = ALIGN_UP(sizeof(struct linux_dirent64) + name_len + 1, 8);

        if (bytes_written + reclen > count) break;

        struct linux_dirent64 *dirent = (struct linux_dirent64 *)(buf + bytes_written);
        dirent->d_ino = child->inode->ino;
        dirent->d_off = *offset + 1;
        dirent->d_reclen = reclen;
        
        // Tag it as a Character/Block device or Directory
        if (child->inode->type == FT_DIR) dirent->d_type = 4; // DT_DIR
        else if (child->inode->type == FT_CHR) dirent->d_type = 2; // DT_CHR (Character Device)
        else dirent->d_type = 8; // DT_REG fallback
        
        memcpy((uint8_t*)dirent->d_name, (const uint8_t*)child->name, name_len + 1);

        bytes_written += reclen;
        (*offset)++;
        child = child->next;
    }

    return bytes_written;
}


int devfs_register_device(const char *name, file_operations_t *fops, uint8_t type) {
    if (!name || !fops) return -1;

    inode_t *dev_inode = (inode_t *)kmalloc(sizeof(inode_t));
    memset(dev_inode, 0, sizeof(inode_t));
    
    dev_inode->type = type; 
    dev_inode->is_directory = 0;
    dev_inode->f_ops = fops;
    dev_inode->i_ops = NULL; 
    
    static uint32_t dev_ino_counter = 1000;
    dev_inode->ino = dev_ino_counter++;

    dentry_t *new_dev = (dentry_t *)kmalloc(sizeof(dentry_t));
    memset(new_dev, 0, sizeof(dentry_t));
    strncpy(new_dev->name, name, NAME_MAX - 1);
    
    new_dev->inode = dev_inode;
    new_dev->parent = &devfs_root_dentry;

    new_dev->next = devfs_root_dentry.children;
    devfs_root_dentry.children = new_dev;

    return 0;
}

int devfs_init(void) {
    memset(&devfs_root_inode, 0, sizeof(inode_t));
    devfs_root_inode.ino = 1; 
    devfs_root_inode.type = FT_DIR;
    devfs_root_inode.is_directory = 1;
    devfs_root_inode.i_ops = &devfs_dir_iops;
    devfs_root_inode.private = &devfs_root_dentry; 

    memset(&devfs_root_dentry, 0, sizeof(dentry_t));
    strcpy(devfs_root_dentry.name, "dev");
    devfs_root_dentry.inode = &devfs_root_inode;

    memset(&devfs_sb, 0, sizeof(superblock_t));
    strcpy(devfs_sb.fs_type, "devfs");
    devfs_sb.root = &devfs_root_dentry;

    vfs_mkdir("/dev"); 
    return vfs_mount(&devfs_sb, "/dev");
}
