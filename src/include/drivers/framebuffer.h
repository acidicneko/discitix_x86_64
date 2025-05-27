#ifndef __FRAMEBUFFER_H__
#define __FRAMEBUFFER_H__

#include <init/stivale2.h>
#include <stdint.h>

typedef struct {
  uint32_t *address;
  uint32_t width;
  uint32_t height;
  uint32_t bpp;
} fb_info_t;

void init_framebuffer();
void framebuffer_put_pixel(int x_pos, uint32_t y_pos, uint32_t color);
void framebuffer_clear(uint32_t color);
fb_info_t *get_fb_info();
void scroll_framebuffer(uint32_t color, uint8_t pixel_count);

#endif
