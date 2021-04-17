#ifndef __FRAMEBUFFER_H__
#define __FRAMEBUFFER_H__

#include "init/stivale2.h"

extern uint32_t* framebuffer_address;
extern uint32_t framebuffer_width;
extern uint32_t framebuffer_height;
extern uint32_t framebuffer_bpp;

void init_framebuffer(struct stivale2_struct* bootinfo);
void framebuffer_put_pixel(int x_pos, uint32_t y_pos, uint32_t color);
void framebuffer_clear(uint32_t color);

#endif