#ifndef __STRIP_FS__
#define __STRIP_FS__

#include <init/stivale2.h>
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
} strip_fs_file_t;

void init_initrd_stripFS(struct stivale2_struct *bootinfo);
int read_initrd_stripFS();

#endif // !__STRIP_FS__
