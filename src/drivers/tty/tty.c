#include "fs/devfs.h"
#include <libk/string.h>
#include <libk/utils.h>
#include <drivers/framebuffer.h>
#include <drivers/tty/font.h>
#include <drivers/tty/hansi_parser.h>
#include <drivers/tty/psf2.h>
#include <drivers/tty/tty.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <mm/liballoc.h>
#include <kernel/vfs/vfs.h>
#include <stdlib.h>

tty_t* current_tty;

uint32_t colors[16];

uint32_t currentBg;
uint32_t currentFg;

uint32_t x_cursor;
uint32_t y_cursor;
bool tty_initialized;

fb_info_t *current_fb;
tty_t ttys[4];

static file_operations_t tty_file_ops = {
  .read = tty_read,
  .write = tty_write,
  .open = NULL,
  .close = NULL,
  .ioctl = tty_ioctl
};

static bool cursor_visible = true;

void init_tty() {
  init_framebuffer();
  x_cursor = 0;
  y_cursor = 0;
  current_fb = get_fb_info();
  load_embedded_psf2();

  for (int i = 0; i < TTY_NUM; ++i) {
    char name[16] = "tty";
    char id_str[8];
    itoa(i, id_str, 10);
    strcat(name, id_str);
    log("TTY", INFO, "Registering /dev/%s\n\r", name);
    ttys[i] = (tty_t) {
      .x_cursor = 0,
      .y_cursor = 0,
      .colors = colors,
      .height = current_fb->height/g_font.header->height,
      .width = current_fb->width/g_font.header->width,
      .id = i,
      .fg = colors[7],
      .bg = colors[0],
      .fg_index = 7,
      .parser = { .state = HTERM_ESC },
      .tty_ops = tty_file_ops,
      .ldisc_mode = TTY_CANONICAL,
      .echo = true,
      .input_esc_state = INPUT_STATE_NORMAL,
    };
    ttys[i].buffer = (terminal_cell_t*)kmalloc(sizeof(terminal_cell_t)*ttys[i].width*ttys[i].height);
    memset(ttys[i].buffer, 0, sizeof(terminal_cell_t)*ttys[i].height*ttys[i].width);
    devfs_register_device(name, &tty_file_ops, FT_CHR, &ttys[i]);
  }
  current_tty = &ttys[0];
  tty_initialized = true;
}

void init_colors(uint32_t black, uint32_t red, uint32_t green, uint32_t yellow,
                 uint32_t blue, uint32_t purple, uint32_t cyan, uint32_t white,
                 uint32_t blackBright, uint32_t redBright, uint32_t greenBright,
                 uint32_t yellowBright, uint32_t blueBright,
                 uint32_t purpleBright, uint32_t cyanBright,
                 uint32_t whiteBright) {

  // normal colors
  colors[0] = black;
  colors[1] = red;
  colors[2] = green;
  colors[3] = yellow;
  colors[4] = blue;
  colors[5] = purple;
  colors[6] = cyan;
  colors[7] = white;

  // bright colors
  colors[8] = blackBright;
  colors[9] = redBright;
  colors[10] = greenBright;
  colors[11] = yellowBright;
  colors[12] = blueBright;
  colors[13] = purpleBright;
  colors[14] = cyanBright;
  colors[15] = whiteBright;

  currentBg = colors[0];
  currentFg = colors[7];
  for (int i = 0; i < TTY_NUM; i++) {
    ttys[i].fg = currentFg;
    ttys[i].bg = currentBg;
  }

  framebuffer_clear(currentBg);
}

static void paint_bitmap_cell(terminal_cell_t cell, uint32_t x, uint32_t y) {
  for (uint8_t iy = 0; iy < 8; iy++) {
    for (uint8_t ix = 0; ix < 8; ix++) {
      bool on = (font[(uint8_t)cell.printable_char & 0x7F][iy] >> ix) & 1;
      framebuffer_put_pixel(ix + x * GLYPH_WIDTH, iy + y * GLYPH_HEIGHT, on ? cell.fg : cell.bg);
    }
  }
}

static void paint_cell(terminal_cell_t cell, uint32_t x, uint32_t y) {
  if (!g_font.header || !g_font.glyphBuffer) {
    paint_bitmap_cell(cell, x, y);
    return;
  }

  uint32_t index = (uint8_t)cell.printable_char;
  if (index >= g_font.header->length) index = '?';
  uint8_t *glyph = g_font.glyphBuffer + index * g_font.header->bytesperglyph;

  uint32_t w = g_font.header->width, h = g_font.header->height;
  uint32_t stride = (w + 7) / 8;
  uint32_t start_x = x * w, start_y = y * h;

  for (uint32_t row = 0; row < h; ++row) {
    for (uint32_t col = 0; col < w; ++col) {
      uint8_t bit = (glyph[row * stride + col / 8] >> (7 - (col % 8))) & 1;
      framebuffer_put_pixel(start_x + col, start_y + row, bit ? cell.fg : cell.bg);
    }
  }
}

static terminal_cell_t cell_at(tty_t *tty, uint16_t x, uint16_t y) {
  terminal_cell_t c = tty->buffer[y * tty->width + x];
  if (c.printable_char == 0)
    c = (terminal_cell_t){ .printable_char = ' ', .fg = tty->colors[7], .bg = tty->colors[0] };
  return c;
}

static void redraw_cell(tty_t *tty, uint16_t x, uint16_t y) {
  if (tty != current_tty) return;
  paint_cell(cell_at(tty, x, y), x, y);
}

static void hide_cursor(tty_t *tty) {
  redraw_cell(tty, tty->x_cursor, tty->y_cursor);
}

static void show_cursor(tty_t *tty) {
  if (tty != current_tty) return;
  terminal_cell_t c = cell_at(tty, tty->x_cursor, tty->y_cursor);
  c.printable_char = '_';
  paint_cell(c, tty->x_cursor, tty->y_cursor);
  cursor_visible = true;
}

static void scroll(tty_t *tty) {
  size_t row = tty->width;
  memmove(tty->buffer, tty->buffer + row, sizeof(terminal_cell_t) * row * (tty->height - 1));
  terminal_cell_t blank = { .printable_char = ' ', .fg = tty->fg, .bg = tty->bg };
  for (size_t col = 0; col < row; ++col)
    tty->buffer[(tty->height - 1) * row + col] = blank;

  if (tty == current_tty)
    scroll_framebuffer(tty->bg, g_font.header ? g_font.header->height : GLYPH_HEIGHT);
}

void tty_erase(tty_t *tty, size_t from, size_t to) {
  size_t cells = (size_t)tty->width * tty->height;
  if (to > cells) to = cells;
  terminal_cell_t blank = { .printable_char = ' ', .fg = tty->fg, .bg = tty->bg };
  for (size_t i = from; i < to; i++) {
    tty->buffer[i] = blank;
    redraw_cell(tty, i % tty->width, i / tty->width);
  }
}

// One character, no escape-sequence processing and no cursor painting
void tty_put_raw(tty_t *tty, char c) {
  switch (c) {
  case '\n':
    tty->x_cursor = 0;
    tty->y_cursor++;
    break;
  case '\r':
    tty->x_cursor = 0;
    break;
  case '\t':
    tty->x_cursor = (tty->x_cursor + 8) & ~7;
    if (tty->x_cursor >= tty->width) tty->x_cursor = tty->width - 1;
    break;
  case '\b':
    if (tty->x_cursor > 0) {
      tty->x_cursor--;
    } else if (tty->y_cursor > 0) {
      tty->y_cursor--;
      tty->x_cursor = tty->width - 1;
    }
    break;
  default: {
    if ((uint8_t)c < 32 || c == 127) break;
    terminal_cell_t cell = { .printable_char = c, .fg = tty->fg, .bg = tty->bg };
    tty->buffer[tty->y_cursor * tty->width + tty->x_cursor] = cell;
    redraw_cell(tty, tty->x_cursor, tty->y_cursor);
    tty->x_cursor++;
    break;
  }
  }

  if (tty->x_cursor >= tty->width) {
    tty->x_cursor = 0;
    tty->y_cursor++;
  }
  if (tty->y_cursor >= tty->height) {
    scroll(tty);
    tty->y_cursor = tty->height - 1;
  }
}

void tty_write_chars(tty_t *tty, const char *s, size_t len) {
  if (!tty || !tty->buffer) return;
  uint64_t flags;
  asm volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");

  hide_cursor(tty);
  for (size_t i = 0; i < len; i++)
    hansi_handler(tty, s[i]);
  show_cursor(tty);

  asm volatile("push %0; popfq" :: "r"(flags) : "memory", "cc");
}

void tty_repaint(tty_t *tty) {
  if (tty != current_tty) return;
  framebuffer_clear(tty->colors[0]);
  for (uint16_t y = 0; y < tty->height; y++)
    for (uint16_t x = 0; x < tty->width; x++)
      redraw_cell(tty, x, y);
  show_cursor(tty);
}

void tty_putchar(char c) {
  tty_write_chars(current_tty, &c, 1);
}

void tty_toggle_cursor_visibility() {
  tty_t *tty = current_tty;
  if (!tty || !tty->buffer) return;
  if (cursor_visible) {
    hide_cursor(tty);
    cursor_visible = false;
  } else {
    show_cursor(tty);
  }
}

void tty_clear() {
  tty_t *tty = current_tty;
  if (tty) {
    tty->x_cursor = 0;
    tty->y_cursor = 0;
    memset(tty->buffer, 0, sizeof(terminal_cell_t) * tty->width * tty->height);
  }
  framebuffer_clear(tty ? tty->bg : currentBg);
}

tty_t* get_current_tty(){
  return current_tty;
}
