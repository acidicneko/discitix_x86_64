// doomgeneric backend for Discitix: draws through /dev/fb0, reads raw
// PS/2 set-1 scancodes from /dev/kbd.

#include "doomkeys.h"
#include "doomgeneric.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define FBIOGET_INFO 0x4600

struct fb_user_info {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
};

static int fb_fd = -1, kbd_fd = -1;
static struct fb_user_info fb;

// Doom's frame, scaled to fit the screen keeping its aspect ratio
static uint32_t *frame;          // fb.pitch * out_h bytes, written in one go
static uint32_t out_w, out_h, off_x, off_y;
static uint16_t *src_x;          // output column -> DG_ScreenBuffer column

#define KEYQUEUE_SIZE 64
static uint16_t key_queue[KEYQUEUE_SIZE];
static unsigned key_read, key_write;
static int extended;

static const char scancode_ascii[0x3A] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', 0, 0, '/', 0, '*', 0, 0,
};

static unsigned char to_doom_key(uint8_t code, int ext) {
    if (ext) {
        switch (code) {
        case 0x48: return KEY_UPARROW;
        case 0x50: return KEY_DOWNARROW;
        case 0x4B: return KEY_LEFTARROW;
        case 0x4D: return KEY_RIGHTARROW;
        case 0x1D: return KEY_FIRE;      // right ctrl
        case 0x38: return KEY_RALT;      // right alt
        case 0x1C: return KEY_ENTER;     // keypad enter
        case 0x47: return KEY_HOME;
        case 0x4F: return KEY_END;
        case 0x49: return KEY_PGUP;
        case 0x51: return KEY_PGDN;
        case 0x52: return KEY_INS;
        case 0x53: return KEY_DEL;
        default:   return 0;
        }
    }

    switch (code) {
    case 0x01: return KEY_ESCAPE;
    case 0x0E: return KEY_BACKSPACE;
    case 0x0F: return KEY_TAB;
    case 0x1C: return KEY_ENTER;
    case 0x1D: return KEY_FIRE;          // ctrl
    case 0x2A: case 0x36: return KEY_RSHIFT;
    case 0x38: return KEY_RALT;          // alt
    case 0x39: return KEY_USE;           // space
    case 0x33: return KEY_STRAFE_L;      // ,
    case 0x34: return KEY_STRAFE_R;      // .
    case 0x57: return KEY_F11;
    case 0x58: return KEY_F12;
    }
    if (code >= 0x3B && code <= 0x44) return KEY_F1 + (code - 0x3B);
    if (code < sizeof scancode_ascii) return (unsigned char)scancode_ascii[code];
    return 0;
}

static void poll_keyboard(void) {
    uint8_t buf[64];
    long n;
    while ((n = read(kbd_fd, buf, sizeof buf)) > 0) {
        for (long i = 0; i < n; i++) {
            if (buf[i] == 0xE0) {
                extended = 1;
                continue;
            }
            int pressed = !(buf[i] & 0x80);
            unsigned char key = to_doom_key(buf[i] & 0x7F, extended);
            extended = 0;
            if (!key) continue;

            unsigned next = (key_write + 1) % KEYQUEUE_SIZE;
            if (next == key_read) continue;   // queue full
            key_queue[key_write] = (uint16_t)((pressed << 8) | key);
            key_write = next;
        }
    }
}

void DG_Init(void) {
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        perror("doom: /dev/fb0");
        exit(1);
    }
    if (ioctl(fb_fd, FBIOGET_INFO, &fb) < 0 || fb.bpp != 32) {
        fprintf(stderr, "doom: unsupported framebuffer\n");
        exit(1);
    }

    kbd_fd = open("/dev/kbd", O_RDONLY);
    if (kbd_fd < 0) {
        perror("doom: /dev/kbd");
        exit(1);
    }

    // Largest size with Doom's aspect ratio that fits the screen
    out_w = fb.width;
    out_h = (uint32_t)((uint64_t)fb.width * DOOMGENERIC_RESY / DOOMGENERIC_RESX);
    if (out_h > fb.height) {
        out_h = fb.height;
        out_w = (uint32_t)((uint64_t)fb.height * DOOMGENERIC_RESX / DOOMGENERIC_RESY);
    }
    off_x = (fb.width - out_w) / 2;
    off_y = (fb.height - out_h) / 2;

    frame = calloc(out_h, fb.pitch);
    src_x = malloc(out_w * sizeof *src_x);
    if (!frame || !src_x) {
        fprintf(stderr, "doom: out of memory\n");
        exit(1);
    }
    for (uint32_t x = 0; x < out_w; x++)
        src_x[x] = (uint16_t)((uint64_t)x * DOOMGENERIC_RESX / out_w);

    // Black out the whole screen once; frames only cover the middle rows
    uint8_t *black = calloc(1, fb.pitch);
    if (black) {
        lseek(fb_fd, 0, SEEK_SET);
        for (uint32_t y = 0; y < fb.height; y++) write(fb_fd, black, fb.pitch);
        free(black);
    }
}

void DG_DrawFrame(void) {
    uint32_t stride = fb.pitch / 4;
    for (uint32_t y = 0; y < out_h; y++) {
        const uint32_t *src = DG_ScreenBuffer + (uint64_t)(y * DOOMGENERIC_RESY / out_h) * DOOMGENERIC_RESX;
        uint32_t *dst = frame + (uint64_t)y * stride + off_x;
        for (uint32_t x = 0; x < out_w; x++) dst[x] = src[src_x[x]];
    }
    lseek(fb_fd, (off_t)off_y * fb.pitch, SEEK_SET);
    write(fb_fd, frame, (size_t)out_h * fb.pitch);

    poll_keyboard();
}

void DG_SleepMs(uint32_t ms) {
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int DG_GetKey(int *pressed, unsigned char *key) {
    if (key_read == key_write) return 0;
    uint16_t data = key_queue[key_read];
    key_read = (key_read + 1) % KEYQUEUE_SIZE;
    *pressed = data >> 8;
    *key = data & 0xFF;
    return 1;
}

void DG_SetWindowTitle(const char *title) {
    (void)title;
}

#define DEFAULT_IWAD "/usr/share/games/doom/doom1.wad"

int main(int argc, char **argv) {
    char *args[argc + 3];
    int n = 0;
    int has_iwad = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-iwad")) has_iwad = 1;
        args[n++] = argv[i];
    }
    if (!has_iwad) {
        args[n++] = "-iwad";
        args[n++] = DEFAULT_IWAD;
    }
    args[n] = NULL;

    doomgeneric_Create(n, args);
    for (;;) doomgeneric_Tick();
    return 0;
}
