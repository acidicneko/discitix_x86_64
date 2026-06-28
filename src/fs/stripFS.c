#include <fs/stripFS.h>
#include <kernel/vfs/vfs.h>
#include <init/limine.h>
#include <init/limine_req.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/liballoc.h>
#include <stdint.h>
#include <stddef.h>

uint64_t initrd_location_strip = 0;
strip_fs_header_t *header_strip = NULL;

long stripfs_file_read(file_t *f, void *buf, size_t len, uint64_t off) {
    if (!f || !f->inode || !f->inode->private) return -1;
    strip_fs_file_t *meta = (strip_fs_file_t *)f->inode->private;

    if ((uint64_t)meta->length < off) return 0;
    size_t available = meta->length - off;
    size_t to_read = len < available ? len : available;

    uint8_t *src = (uint8_t *)(initrd_location_strip + meta->offset + off);
    memcpy(buf, src, to_read);
    f->offset += to_read;
    return (size_t)to_read;
}

inode_t *stripfs_dir_lookup(inode_t *parent, const char *name) {
    if (!parent || !parent->private) return NULL;
    dentry_t *parent_d = (dentry_t *)parent->private;
    dentry_t *child = parent_d->children;
    while (child) {
        if (!strcmp(child->name, name)) {
            return child->inode;
        }
        child = child->next;
    }
    return NULL;
}

static file_operations_t stripfs_fops = {
  .open = NULL,
  .close = NULL,
  .read = stripfs_file_read,
  .write = NULL,
};

static inode_operations_t stripfs_dir_iops = {
  .lookup = stripfs_dir_lookup,
  .create = NULL,
  .mkdir = NULL,
  .unlink = NULL,
  .getdents = stripfs_dir_getdents,
};


int stripfs_create_and_mount() {
  if (!header_strip) return -1;

  superblock_t *sb = (superblock_t *)kmalloc(sizeof(superblock_t));
  if (!sb) return -1;
  memset(sb, 0, sizeof(superblock_t));
  strncpy(sb->fs_type, "stripfs", sizeof(sb->fs_type) - 1);

  dentry_t *root = (dentry_t *)kmalloc(sizeof(dentry_t));
  if (!root) return -1;
  memset(root, 0, sizeof(dentry_t));
  strcpy(root->name, "/");

  inode_t *root_inode = (inode_t *)kmalloc(sizeof(inode_t));
  if (!root_inode) return -1;
  memset(root_inode, 0, sizeof(inode_t));
  root_inode->is_directory = 1;
  root_inode->type = FT_DIR;
  root_inode->i_ops = &stripfs_dir_iops;
  root_inode->f_ops = NULL;
  root_inode->private = (void *)root;

  root->inode = root_inode;
  sb->root = root;

  uint8_t *ptr = (uint8_t *)(initrd_location_strip + sizeof(strip_fs_header_t));
  uint32_t ino_counter = 1;
  for (int i = 0; i < header_strip->num_files; i++) {
      strip_fs_file_t *filemeta = (strip_fs_file_t *)kmalloc(sizeof(strip_fs_file_t));
      if (!filemeta) continue;
      memcpy((uint8_t *)filemeta, ptr, sizeof(strip_fs_file_t));

      /* create inode */
      inode_t *inode = (inode_t *)kmalloc(sizeof(inode_t));
      if (!inode) {
          kfree(filemeta);
          ptr += sizeof(strip_fs_file_t);
          continue;
      }
      memset(inode, 0, sizeof(inode_t));
      inode->ino = 9000+ino_counter++;
      inode->size = (uint32_t)filemeta->length;
      inode->is_directory = 0;
      inode->type = FT_REG;
      inode->f_ops = &stripfs_fops;
      inode->i_ops = NULL;
      inode->private = (void *)filemeta; /* store meta pointer */
      if(filemeta->executable) {
          inode->mode = 5;
      } else {
          inode->mode = 4;
      }
      dentry_t *d = (dentry_t *)kmalloc(sizeof(dentry_t));
      if (!d) {
          kfree(inode);
          kfree(filemeta);
          ptr += sizeof(strip_fs_file_t);
          continue;
      }
      memset(d, 0, sizeof(dentry_t));
      strncpy(d->name, filemeta->filename, NAME_MAX - 1);
      d->inode = inode;
      d->parent = root;

      d->next = root->children;
      root->children = d;

      ptr += sizeof(strip_fs_file_t);
  }

  sb->private = NULL;

  vfs_mount(sb, "/");
  return 0;
}


void init_initrd_stripFS() {
  struct limine_module_response *modules = module_request.response;
  for (uint64_t i = 0; i < modules->module_count; i++) {
    // TODO: Do not hardcode this path instead get path from the kernel command
    // line
    if (modules->modules[i]->path &&
        !strcmp(modules->modules[i]->path, "/initrd.img")) {
      initrd_location_strip = (uint64_t)modules->modules[i]->address;
      header_strip = (strip_fs_header_t *)initrd_location_strip;
      if (header_strip->magic[0] != MAGIC_1 ||
          header_strip->magic[1] != MAGIC_2) {
        header_strip = NULL;
      }
      dbgln("Found initrd at: 0x%xl\n\r", initrd_location_strip);
      stripfs_create_and_mount();
      break;
    }
  }
}

// Your exact logic, just accepting the offset pointer!
long stripfs_dir_getdents(inode_t *inode, uint64_t *offset, void *buf_ptr, uint32_t count) {
    dentry_t *dir_dentry = (dentry_t*)inode->private;
    if (!dir_dentry) return -1;
    
    char *buf = (char*)buf_ptr;
    size_t pos = 0;
    
    // Skip to the current offset
    dentry_t *child = dir_dentry->children;
    uint64_t skip = *offset; 
    while (child && skip > 0) {
        child = child->next;
        skip--;
    }
    
    // Fill buffer with directory entries
    while (child && pos < count) {
        size_t name_len = strlen(child->name);
        size_t reclen = (sizeof(struct linux_dirent64) + name_len + 1 + 7) & ~7;
        
        if (pos + reclen > count) break; 
        
        struct linux_dirent64 *dirent = (struct linux_dirent64*)(buf + pos);
        dirent->d_ino = child->inode ? child->inode->ino : 0;
        dirent->d_off = *offset + 1;
        dirent->d_reclen = (uint16_t)reclen;
        
        if (child->inode && child->inode->type != 0) {
            dirent->d_type = child->inode->type;
        } else {
            dirent->d_type = (child->inode && child->inode->is_directory) ? DT_DIR : DT_REG;
        }
        memcpy((uint8_t*)dirent->d_name, (const uint8_t*)child->name, name_len + 1);
        
        pos += reclen;
        (*offset)++; // Increment the file offset for the next syscall
        child = child->next;
    }
    
    return (int64_t)pos;
}

