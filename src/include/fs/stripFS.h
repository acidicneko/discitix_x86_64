#ifndef __STRIP_FS__
#define __STRIP_FS__

#include <init/stivale2.h>
#include <kernel/vfs/vfs.h>
#include <stddef.h>
#include <stdint.h>

#define MAGIC_1 0x69
#define MAGIC_2 0x42

typedef struct {
  uint8_t magic[2];
  int num_files;
} strip_fs_header_t;

typedef struct {
  char filename[256];
  int length;
  int offset;
  int executable;
} strip_fs_file_t;

void init_initrd_stripFS();

long stripfs_file_read(file_t *f, void *buf, size_t len, uint64_t off);
int stripfs_file_open(inode_t *inode, uint32_t flags);
int stripfs_file_close(inode_t *inode);
inode_t *stripfs_dir_lookup(inode_t *parent, const char *name);

#endif // !__STRIP_FS__
