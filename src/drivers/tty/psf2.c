#include <libk/utils.h>
#include <drivers/tty/psf2.h>
#include <drivers/tty/tty.h>
#include <fs/stripFS.h>
#include <libk/stdio.h>
#include <kernel/vfs/vfs.h>
#include <mm/pmm.h>
#include <stdbool.h>
#include <stdint.h>

psf2_font_t g_font;
static bool psf2_loaded = false;

void initial_psf_setup() {
  g_font.header = 0;
  g_font.glyphBuffer = 0;
}

void load_embedded_psf2() {
  file_t* font = NULL;
  inode_t *font_inode = NULL;
  if(vfs_lookup_path("/font.psf", &font_inode) != 0) {
    dbgln("Font file not found in /font.psf\n\r");
    return;
  }
  if (vfs_open(&font, font_inode, 0) != 0) {
    dbgln("Failed to open font file!\n\r");
    return;
  }
  uint8_t *font_buffer = pmalloc((font->inode->size + PAGE_SIZE - 1) / PAGE_SIZE);
  long read_bytes = vfs_read(font, font_buffer, font->inode->size);
  if (read_bytes != (long)font->inode->size) {
    dbgln("Failed to read entire font file!\n\r");
    pmm_free_pages(font_buffer, (font->inode->size + PAGE_SIZE - 1) / PAGE_SIZE);
    return;
  }
  vfs_close(font);
  dbgln("Font file size: %ul bytes\n\r",font->inode->size);
  psf2_header_t *font_header = (psf2_header_t *)(void *)font_buffer;

  // Validate magic number
  if (font_header->magic[0] == PSF2_MAGIC0 &&
      font_header->magic[1] == PSF2_MAGIC1 &&
      font_header->magic[2] == PSF2_MAGIC2 &&
      font_header->magic[3] == PSF2_MAGIC3) {
    dbgln("Provided font is PSF2!\n\r");
  } else if (font_header->magic[0] == PSF1_MAGIC0 &&
             font_header->magic[1] == PSF1_MAGIC1) {
    dbgln("Provided font is PSF1!\n\r");
    return;
  } else {
    dbgln(
        "Invalid font type provided!\n\rKernel wouldn't be able to boot!\n\r");
    return;
  }

  // Calculate glyph data pointer
  void *glyph_data = (void *)font_buffer + font_header->headersize;
  
  g_font.header = font_header;
  g_font.glyphBuffer = glyph_data;
  psf2_loaded = true;
}

void print_font_details() {
  if (!psf2_loaded)
    return;
  printf("Header size: %ui\n", g_font.header->headersize);
  printf("Number of glyphs: %ul\n", g_font.header->length);
  printf("Dimension: %ulx%ul\n", g_font.header->width, g_font.header->height);
}
