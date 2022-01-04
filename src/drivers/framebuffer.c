#include <drivers/framebuffer.h>
#include <stdint.h>
#include <stddef.h>
#include <libk/utils.h>

fb_info_t fb_info;


void init_framebuffer(struct stivale2_struct* bootinfo){
    struct stivale2_struct_tag_framebuffer* fbtag = (struct stivale2_struct_tag_framebuffer*)stivale2_get_tag
                                                        (bootinfo, STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID);
    fb_info.address = (uint32_t*)(uint64_t)fbtag->framebuffer_addr;
    fb_info.height = fbtag->framebuffer_height;
    fb_info.width = fbtag->framebuffer_width;
    fb_info.bpp = fbtag->framebuffer_bpp;
    dbgln("Framebuffer: address: 0x%xl\n\rFramebuffer: Height: %ui\n\rFramebuffer: Widht: %ui\n\rFramebuffer: BPP: %ui\n\r", 
            fb_info.address, fb_info.height, fb_info.width, fb_info.bpp);
}

void framebuffer_put_pixel(int x_pos, uint32_t y_pos, uint32_t color){
    *(uint32_t*)(x_pos +  y_pos*fb_info.width + fb_info.address) = color;
}

void framebuffer_clear(uint32_t color){
    for(uint32_t i = 0; i < fb_info.height*fb_info.width; i++){
        fb_info.address[i] = color;
    }
}

fb_info_t* get_fb_info(){
    return &fb_info;
}