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
    dbgln("No framebuffer found!\n\r");
    return;
  }
  fb_info.address = (uint32_t *)(uint64_t)fb_response->framebuffers[0]->address;
  fb_info.height = fb_response->framebuffers[0]->height;
  fb_info.width = fb_response->framebuffers[0]->width;
  fb_info.bpp = fb_response->framebuffers[0]->bpp;
}

void framebuffer_put_pixel(int x_pos, uint32_t y_pos, uint32_t color) {
  *(uint32_t *)(x_pos + y_pos * fb_info.width + fb_info.address) = color;
}

void framebuffer_clear(uint32_t color) {
  for (uint32_t i = 0; i < fb_info.height * fb_info.width; i++) {
    fb_info.address[i] = color;
  }
}

fb_info_t *get_fb_info() { return &fb_info; }

void scroll_framebuffer(uint32_t color, uint8_t pixel_count) {
  int pixels_per_row = fb_info.width;
  int row_count = fb_info.height;

  for (int y = 0; y < row_count - pixel_count; y++) {
    for (int x = 0; x < pixels_per_row; x++) {
      fb_info.address[y * pixels_per_row + x] =
          fb_info.address[(y + pixel_count) * pixels_per_row + x];
    }
  }

  for (int y = row_count - pixel_count; y < row_count; y++) {
    for (int x = 0; x < pixels_per_row; x++) {
      fb_info.address[y * pixels_per_row + x] = color;
    }
  }
}
