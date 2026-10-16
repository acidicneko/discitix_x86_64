#include "init/limine.h"
#include "init/limine_req.h"
#include <drivers/framebuffer.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/pmm.h>
#include <stddef.h>
#include <stdint.h>

fb_info_t fb_info;

void init_framebuffer() {
  struct limine_framebuffer_response *fb_response =
      framebuffer_request.response;
  if (!fb_response || fb_response->framebuffer_count == 0) {
    log("FB",ERROR,"No framebuffer found!\n\r");
    return;
  }
  fb_info.address = (uint32_t *)(uint64_t)fb_response->framebuffers[0]->address;
  fb_info.height = fb_response->framebuffers[0]->height;
  fb_info.width = fb_response->framebuffers[0]->width;
  fb_info.bpp = fb_response->framebuffers[0]->bpp;
  fb_info.pitch = fb_response->framebuffers[0]->pitch;
}

static inline uint32_t *row(uint32_t y) {
  return (uint32_t *)((uint8_t *)fb_info.address + (uint64_t)y * fb_info.pitch);
}

void framebuffer_put_pixel(int x_pos, uint32_t y_pos, uint32_t color) {
  row(y_pos)[x_pos] = color;
}

void framebuffer_clear(uint32_t color) {
  for (uint32_t y = 0; y < fb_info.height; y++) {
    uint32_t *r = row(y);
    for (uint32_t x = 0; x < fb_info.width; x++) r[x] = color;
  }
}

fb_info_t *get_fb_info() { return &fb_info; }

void scroll_framebuffer(uint32_t color, uint8_t pixel_count) {
  uint32_t height = fb_info.height;
  if (pixel_count >= height) {
    framebuffer_clear(color);
    return;
  }
  memmove(row(0), row(pixel_count), (uint64_t)(height - pixel_count) * fb_info.pitch);
  for (uint32_t y = height - pixel_count; y < height; y++) {
    uint32_t *r = row(y);
    for (uint32_t x = 0; x < fb_info.width; x++) r[x] = color;
  }
}
