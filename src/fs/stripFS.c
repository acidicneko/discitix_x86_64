#include <fs/stripFS.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <mm/pmm.h>
#include <stdint.h>
#include <sys/types.h>

uint64_t initrd_location_strip = 0;
strip_fs_header_t *header_strip = NULL;

void init_initrd_stripFS(struct stivale2_struct *bootinfo) {
  struct stivale2_struct_tag_modules *modules =
      (struct stivale2_struct_tag_modules *)stivale2_get_tag(
          bootinfo, STIVALE2_STRUCT_TAG_MODULES_ID);
  if (modules->module_count == 0) {
    printf("ERROR! No modules loaded! Cannot find initrd...\n");
    return;
  }
  initrd_location_strip = modules->modules[0].begin;
  header_strip = (strip_fs_header_t *)initrd_location_strip;
  if (header_strip->magic[0] != MAGIC_1 && header_strip->magic[1] != MAGIC_2) {
    header_strip = NULL;
  }
}

int read_initrd_stripFS() {
  uint8_t *ptr = (uint8_t *)(initrd_location_strip + sizeof(strip_fs_header_t));

  for (int i = 0; i < header_strip->num_files; i++) {

    strip_fs_file_t *file = (strip_fs_file_t *)pmalloc(1);
    memcpy((uint8_t *)file, ptr, sizeof(strip_fs_file_t));

    printf("Filename: %s\nLength: %d\nOffset: %d\n", file->filename,
           file->length, file->offset);

    char *buffer = pmalloc(
        4); // TODO: Calulate number of pages required based on file size
    uint8_t *contents = (uint8_t *)(initrd_location_strip + file->offset);
    memcpy((uint8_t *)buffer, contents, file->length);
    printf("Contents: \n%s\n\n", buffer);

    memset(buffer, 0, file->length);
    pmm_free_pages(buffer, 4);

    memset(file, 0, sizeof(strip_fs_file_t));
    pmm_free_pages(file, 1);

    ptr += sizeof(strip_fs_file_t);
  }
  return 0;
}