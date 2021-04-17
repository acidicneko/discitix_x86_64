#include "init/stivale2.h"
#include <stdint.h>
#include <stddef.h>

uint32_t* framebuffer_address = NULL;
uint32_t framebuffer_width;
uint32_t framebuffer_height;
uint32_t framebuffer_bpp;

void init_framebuffer(struct stivale2_struct* bootinfo){
    struct stivale2_struct_tag_framebuffer* fbtag = (struct stivale2_struct_tag_framebuffer*)stivale2_get_tag(bootinfo, STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID);
    framebuffer_address = (uint32_t*)(uint64_t)fbtag->framebuffer_addr;
    framebuffer_height = fbtag->framebuffer_height;
    framebuffer_width = fbtag->framebuffer_width;
    framebuffer_bpp = fbtag->framebuffer_bpp;
}

void framebuffer_put_pixel(int x_pos, uint32_t y_pos, uint32_t color){
    *(uint32_t*)(x_pos +  y_pos*framebuffer_width + framebuffer_address) = color;
}

void framebuffer_clear(uint32_t color){
    for(int i = 0; i < framebuffer_height*framebuffer_width; i++){
        framebuffer_address[i] = color;
    }
}