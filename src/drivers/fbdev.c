#include <drivers/fbdev.h>
#include <drivers/framebuffer.h>
#include <drivers/tty/tty.h>
#include <fs/devfs.h>
#include <kernel/vfs/vfs.h>
#include <libk/errno.h>
#include <libk/string.h>
#include <libk/utils.h>

// Open files on /dev/fb0; the TTYs stay off the screen while any exist
static int fb_users;

static uint64_t fb_size(void) {
    fb_info_t *fb = get_fb_info();
    return (uint64_t)fb->pitch * fb->height;
}

static int fb_open(inode_t *inode, uint32_t flags) {
    (void)inode; (void)flags;
    if (!get_fb_info()->address) return -ENODEV;
    if (fb_users++ == 0) tty_set_graphics_mode(true);
    return 0;
}

static int fb_close(inode_t *inode) {
    (void)inode;
    if (fb_users > 0 && --fb_users == 0) tty_set_graphics_mode(false);
    return 0;
}

static long fb_read(file_t *file, void *buf, size_t len, uint64_t off) {
    (void)file;
    uint64_t size = fb_size();
    if (off >= size) return 0;
    if (len > size - off) len = size - off;
    memcpy(buf, (uint8_t *)get_fb_info()->address + off, len);
    return (long)len;
}

static long fb_write(file_t *file, const void *buf, size_t len, uint64_t off) {
    (void)file;
    uint64_t size = fb_size();
    if (off >= size) return -ENOSPC;
    if (len > size - off) len = size - off;
    memcpy((uint8_t *)get_fb_info()->address + off, buf, len);
    return (long)len;
}

static int fb_ioctl(file_t *file, uint64_t request, void *arg) {
    (void)file;
    if (request != FBIOGET_INFO) return -EINVAL;
    if (!arg) return -EFAULT;
    fb_info_t *fb = get_fb_info();
    struct fb_user_info *info = (struct fb_user_info *)arg;
    info->width = fb->width;
    info->height = fb->height;
    info->pitch = fb->pitch;
    info->bpp = fb->bpp;
    return 0;
}

static file_operations_t fb_ops = {
    .open = fb_open,
    .close = fb_close,
    .read = fb_read,
    .write = fb_write,
    .ioctl = fb_ioctl,
};

void init_fb_device(void) {
    if (devfs_register_device("fb0", &fb_ops, FT_CHR, NULL) == 0)
        log("FB", INFO, "Registered /dev/fb0\n\r");
}
