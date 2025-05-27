#ifndef PSF2_FONT
#define PSF2_FONT

#include <stdint.h>

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

#define PSF2_MAGIC0 0x72
#define PSF2_MAGIC1 0xb5
#define PSF2_MAGIC2 0x4a
#define PSF2_MAGIC3 0x86

typedef struct {
  uint8_t magic[4];
  uint32_t version;
  uint32_t headersize;
  uint32_t flags;
  uint32_t length;
  uint32_t bytesperglyph;
  uint32_t height;
  uint32_t width;
} psf2_header_t;

typedef struct {
  uint8_t magic[2];
  uint8_t mode;
  uint8_t charsize;
} psf1_header_t;

typedef struct {
  psf2_header_t *header;
  uint8_t *glyphBuffer;
} psf2_font_t;

extern psf2_font_t g_font;

void initial_psf_setup();
void load_embedded_psf2();
void print_font_details();

#endif // !PSF2_FONT
