#ifndef __FBDEV_H__
#define __FBDEV_H__

#include <stdint.h>

#define FBIOGET_INFO 0x4600

struct fb_user_info {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;   // bytes per row
    uint32_t bpp;
};

void init_fb_device(void);

#endif
