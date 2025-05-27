#include "libk/utils.h"
#include <drivers/framebuffer.h>
#include <drivers/tty/font.h>
#include <drivers/tty/hansi_parser.h>
#include <drivers/tty/psf2.h>
#include <drivers/tty/tty.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

uint32_t colors[16];

uint32_t currentBg;
uint32_t currentFg;

uint32_t x_cursor;
uint32_t y_cursor;

bool tty_initialized = false;

fb_info_t *current_fb = NULL;

void init_tty() {
  init_framebuffer();
  x_cursor = 0;
  y_cursor = 0;
  current_fb = get_fb_info();
  load_embedded_psf2();
  tty_initialized = true;
  // tty_paint_cursor(x_cursor, y_cursor);
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

  framebuffer_clear(currentBg);
}

void tty_paint_cell(terminal_cell_t cell) {
  uint8_t iy, ix;
  // paint the background of cell
  for (iy = 0; iy < 8; iy++) {
    for (ix = 0; ix < 8; ix++) {
      if ((font[128][iy] >> ix) & 1) {
        framebuffer_put_pixel(ix + x_cursor * GLYPH_WIDTH,
                              iy + y_cursor * GLYPH_HEIGHT, cell.bg);
      }
    }
  }
  // paint the printable character
  for (iy = 0; iy < 8; iy++) {
    for (ix = 0; ix < 8; ix++) {
      if ((font[(uint8_t)cell.printable_char][iy] >> ix) & 1) {
        framebuffer_put_pixel(ix + x_cursor * GLYPH_WIDTH,
                              iy + y_cursor * GLYPH_HEIGHT, cell.fg);
      }
    }
  }
}

void tty_paint_cell_psf(terminal_cell_t cell) {
  if (g_font.header == 0) {
    dbgln("[TTY] Invalid psf font detected! Falling back to bitmap font!\n\r");
    tty_paint_cell(cell);
    return;
  }

  if (g_font.glyphBuffer == 0) {
    dbgln(
        "[TTY] Invalid font memory detected! Falling back to bitmap font!\n\r");
    tty_paint_cell(cell);
    return;
  }

  uint8_t *glyph =
      g_font.glyphBuffer + (cell.printable_char * g_font.header->bytesperglyph);

  if (glyph == 0) {
    dbgln("[TTY] Invalid symbol Falling back to bitmap font!\n\r");
    tty_paint_cell(cell);
    return;
  }

  int start_x = x_cursor * g_font.header->width;
  int start_y = y_cursor * g_font.header->height;

  for (uint32_t row = 0; row < g_font.header->height; ++row) {
    for (uint32_t col = 0; col < g_font.header->width; ++col) {
      uint8_t byte = glyph[row * ((g_font.header->width + 7) / 8) + (col / 8)];
      uint8_t bit = (byte >> (7 - (col % 8))) & 1;

      uint32_t color = bit ? currentFg : currentBg;
      framebuffer_put_pixel(start_x + col, start_y + row, color);
    }
  }
}

void tty_hide_cursor() {
  terminal_cell_t cell = {
      .printable_char = ' ', .fg = currentFg, .bg = currentBg};
  tty_paint_cell_psf(cell); // Overwrites the previous cursor cell
}

void tty_putchar_raw(char c) {
  tty_hide_cursor(); // Hide the cursor before printing
  switch (c) {
  case '\n':
    x_cursor = 0;
    y_cursor++;
    break;

  case '\r':
    x_cursor = 0;
    break;

  case '\t':
    x_cursor = (x_cursor - (x_cursor % 8)) + 8;
    break;

  case '\b':
    if (x_cursor > 0) {
      x_cursor--;
    } else if (y_cursor > 0) {
      y_cursor--;
      x_cursor = (current_fb->width / g_font.header->width) - 1;
    }

    {
      terminal_cell_t blank = {
          .printable_char = ' ', .fg = currentFg, .bg = currentBg};
      tty_paint_cell_psf(blank); // Draw blank at new cursor
    }
    break;

  default:
    terminal_cell_t cell = {
        .printable_char = c, .fg = currentFg, .bg = currentBg};
    // tty_paint_cell(cell);
    tty_paint_cell_psf(cell);
    x_cursor += 1;
    break;
  }
  if (x_cursor >= current_fb->width / g_font.header->width) {
    x_cursor = 0;
    y_cursor++;
  }
  if (y_cursor >= current_fb->height / g_font.header->height) {
    scroll_framebuffer(currentBg, g_font.header->height);
    x_cursor = 0;
    y_cursor--;
  }
  tty_paint_cursor(x_cursor, y_cursor);
}

void tty_putchar(char c) { hansi_handler(c); }

// IN CONSTRUCTION! DO NOT USE!
void tty_paint_cursor(uint32_t x, uint32_t y) {
  terminal_cell_t cell = {
      .printable_char = '_', .fg = currentFg, .bg = currentBg};
  // tty_paint_cell(cell);
  tty_paint_cell_psf(cell);
  x_cursor = x;
  y_cursor = y;
}

void tty_toggle_cursor_visibility() {
  static int cursor_visible = 1;
  if (cursor_visible) {
    tty_paint_cursor(x_cursor, y_cursor);
  } else {
    terminal_cell_t cell = {
        .printable_char = ' ', .fg = currentFg, .bg = currentBg};
    tty_paint_cell_psf(cell);
  }
  cursor_visible = !cursor_visible;
}

void set_currentFg(uint32_t value) { currentFg = value; }

void set_currentBg(uint32_t value) { currentBg = value; }

void tty_clear() {
  x_cursor = 0;
  y_cursor = 0;
  framebuffer_clear(currentBg);
}
