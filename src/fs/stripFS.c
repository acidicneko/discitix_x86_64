#include <fs/stripFS.h>
#include <init/limine.h>
#include <init/limine_req.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/pmm.h>
#include <stdint.h>
#include <sys/types.h>

uint64_t initrd_location_strip = 0;
strip_fs_header_t *header_strip = NULL;

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
      break;
    }
  }
}

int read_initrd_stripFS() {
  uint8_t *ptr = (uint8_t *)(initrd_location_strip + sizeof(strip_fs_header_t));

  for (int i = 0; i < header_strip->num_files; i++) {

    strip_fs_file_t *file = (strip_fs_file_t *)pmalloc(1);
    memcpy((uint8_t *)file, ptr, sizeof(strip_fs_file_t));

    printf("Filename: %s\nLength: %d\nOffset: %d\n\n", file->filename,
           file->length, file->offset);

    memset(file, 0, sizeof(strip_fs_file_t));
    pmm_free_pages(file, 1);

    ptr += sizeof(strip_fs_file_t);
  }
  ptr = NULL;
  return 0;
}

int stat_file_stripFS(const char *filename, strip_fs_file_t *fp) {
  uint8_t *ptr = (uint8_t *)(initrd_location_strip + sizeof(strip_fs_header_t));

  for (int i = 0; i < header_strip->num_files; i++) {

    strip_fs_file_t *file = (strip_fs_file_t *)pmalloc(1);
    memcpy((uint8_t *)file, ptr, sizeof(strip_fs_file_t));

    if (!strcmp(filename, file->filename)) {
      strcpy(fp->filename, file->filename);
      fp->length = file->length;
      fp->offset = file->offset;
      memset(file, 0, sizeof(strip_fs_file_t));
      pmm_free_pages(file, 1);
      return 0;
    }

    memset(file, 0, sizeof(strip_fs_file_t));
    pmm_free_pages(file, 1);

    ptr += sizeof(strip_fs_file_t);
  }
  ptr = NULL;
  return -1;
}

int read_file_stripFS(strip_fs_file_t *fp, uint8_t *buffer) {
  uint8_t *contents = (uint8_t *)(initrd_location_strip + fp->offset);
  memcpy((uint8_t *)buffer, contents, fp->length);
  return 0;
}
