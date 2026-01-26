#include <fs/stripFS.h>
#include <kernel/vfs/vfs.h>
#include <init/limine.h>
#include <init/limine_req.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/pmm.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

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
    return (ssize_t)to_read;
}

inode_t *stripfs_dir_lookup(inode_t *parent, const char *name) {
    if (!parent || !parent->private) return NULL;
    dentry_t *parent_d = (dentry_t *)parent->private;
    dentry_t *child = parent_d->next;
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
};


int stripfs_create_and_mount() {
  if (!header_strip) return -1;

  superblock_t *sb = (superblock_t *)pmalloc(1);
  if (!sb) return -1;
  memset(sb, 0, 4096);
  strncpy(sb->fs_type, "stripfs", sizeof(sb->fs_type) - 1);

  dentry_t *root = (dentry_t *)pmalloc(1);
  if (!root) return -1;
  memset(root, 0, 4096);
  strcpy(root->name, "/");

  inode_t *root_inode = (inode_t *)pmalloc(1);
  if (!root_inode) return -1;
  memset(root_inode, 0, 4096);
  root_inode->is_directory = 1;
  root_inode->i_ops = &stripfs_dir_iops;
  root_inode->f_ops = NULL;
  root_inode->private = (void *)root;

  root->inode = root_inode;
  sb->root = root;

  uint8_t *ptr = (uint8_t *)(initrd_location_strip + sizeof(strip_fs_header_t));
  uint32_t ino_counter = 1;
  for (int i = 0; i < header_strip->num_files; i++) {
      strip_fs_file_t *filemeta = (strip_fs_file_t *)pmalloc(1);
      if (!filemeta) continue;
      memcpy((uint8_t *)filemeta, ptr, sizeof(strip_fs_file_t));

      /* create inode */
      inode_t *inode = (inode_t *)pmalloc(1);
      if (!inode) {
          pmm_free_pages(filemeta, 1);
          ptr += sizeof(strip_fs_file_t);
          continue;
      }
      memset(inode, 0, 4096);
      inode->ino = ino_counter++;
      inode->size = (uint32_t)filemeta->length;
      inode->is_directory = 0;
      inode->f_ops = &stripfs_fops;
      inode->i_ops = NULL;
      inode->private = (void *)filemeta; /* store meta pointer */

      dentry_t *d = (dentry_t *)pmalloc(1);
      if (!d) {
          pmm_free_pages(inode, 1);
          pmm_free_pages(filemeta, 1);
          ptr += sizeof(strip_fs_file_t);
          continue;
      }
      memset(d, 0, 4096);
      strncpy(d->name, filemeta->filename, NAME_MAX - 1);
      d->inode = inode;
      d->parent = root;

      d->next = root->next;
      root->next = d;

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


