#include "libk/utils.h"
#include <drivers/tty/psf2.h>
#include <drivers/tty/tty.h>
#include <mm/pmm.h>
#include <stdint.h>

psf2_font_t g_font;

void initial_psf_setup() {
  g_font.header = 0;
  g_font.glyphBuffer = 0;
}

void load_embedded_psf2() {
  psf2_header_t *font_header =
      (psf2_header_t *)(void *)&_binary_misc_default_psf_start;

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
    dbgln("Invalid font type provided!\n\r");
    return;
  }

  // Calculate glyph data pointer
  void *glyph_data =
      (void *)&_binary_misc_default_psf_start + font_header->headersize;
  // Store the glyph data globally if needed for rendering
  g_font.header = font_header;
  g_font.glyphBuffer = glyph_data;
}
