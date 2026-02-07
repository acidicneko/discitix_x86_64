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

inode_t* tty_node = NULL;
static file_operations_t tty_file_ops = {
  .read = tty_read,
  .write = tty_write,
  .open = NULL,
  .close = NULL
};


void init_tty() {
  init_framebuffer();
  x_cursor = 0;
  y_cursor = 0;
  current_fb = get_fb_info();
  load_embedded_psf2();
  
  
  for (int i = 0; i < TTY_NUM; ++i) {
    tty_node = (inode_t*)kmalloc(sizeof(inode_t));  
    memset(tty_node, 0, sizeof(inode_t));
    tty_node->atime = 0;
    tty_node->ctime = 0;
    tty_node->is_directory = 0;
    tty_node->type = FT_CHR;  // Character device
    tty_node->size = 0;
    tty_node->ino = 6000+i;
    tty_node->f_ops = &tty_file_ops;
    tty_node->mode = 6;
    
    char path[16] = "/dev/tty";
    char id_str[2];
    itoa(i, id_str, 10);
    strcat(path, id_str);
    
    vfs_register_device(path, tty_node);
    tty_node = NULL;
    
    ttys[i] = (tty_t) {
      .x_cursor = 0,
      .y_cursor = 0,
      .colors = colors,
      .height = current_fb->height/g_font.header->height,
      .width = current_fb->width/g_font.header->width,
      .id = i,
      .tty_ops = tty_file_ops,
      .ldisc_mode = TTY_CANONICAL,
      .echo = true,
      .input_head = 0,
      .input_tail = 0,
      .line_length = 0,
      .line_cursor = 0,
      .line_ready = false,
      .input_esc_state = INPUT_STATE_NORMAL,
    };
    ttys[i].buffer = (terminal_cell_t*)kmalloc(sizeof(terminal_cell_t)*ttys[i].width*ttys[i].height);
    memset(ttys[i].buffer, 0, sizeof(terminal_cell_t)*ttys[i].height*ttys[i].width);
  }
  current_tty = &ttys[0]; 
  dbgln("TTYs registered at /dev/tty0-3\n\r");
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

void tty_paint_cell_psf(terminal_cell_t cell, tty_t* tty) {
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

  int start_x = tty->x_cursor * g_font.header->width;
  int start_y = tty->y_cursor * g_font.header->height;

  for (uint32_t row = 0; row < g_font.header->height; ++row) {
    for (uint32_t col = 0; col < g_font.header->width; ++col) {
      uint8_t byte = glyph[row * ((g_font.header->width + 7) / 8) + (col / 8)];
      uint8_t bit = (byte >> (7 - (col % 8))) & 1;

      // Use the cell's fg/bg colors instead of global colors
      uint32_t color = bit ? cell.fg : cell.bg;
      framebuffer_put_pixel(start_x + col, start_y + row, color);
    }
  }
}

void tty_hide_cursor() {
  if (!current_tty) return;
  
  // Get the actual character at cursor position from buffer, or space if empty
  int idx = current_tty->y_cursor * current_tty->width + current_tty->x_cursor;
  terminal_cell_t cell;
  
  if (current_tty->buffer[idx].printable_char != 0) {
    cell = current_tty->buffer[idx];
  } else {
    cell = (terminal_cell_t){.printable_char = ' ', .fg = currentFg, .bg = currentBg};
  }
  
  tty_paint_cell_psf(cell, current_tty); // Restore the actual character
}

void tty_putchar_raw(char c) {
  tty_t* tty = current_tty;
  if (!tty) return;
  
  terminal_cell_t* buffer = tty->buffer;
  
  tty_hide_cursor(); // Hide the cursor before printing
  switch (c) {
  case '\n': {
    // Clear rest of line in buffer
    terminal_cell_t blank = {.printable_char = ' ', .fg = currentFg, .bg = currentBg};
    for (uint16_t x = tty->x_cursor; x < tty->width; x++) {
      buffer[tty->y_cursor * tty->width + x] = blank;
    }
    tty->x_cursor = 0;
    tty->y_cursor++;
    break;
  }

  case '\r':
    tty->x_cursor = 0;
    break;

  case '\t': {
    // Clear cells for tab
    terminal_cell_t blank = {.printable_char = ' ', .fg = currentFg, .bg = currentBg};
    uint16_t target_x = (tty->x_cursor - (tty->x_cursor % 8)) + 8;
    if (target_x > tty->width) target_x = tty->width;
    while (tty->x_cursor < target_x) {
      buffer[tty->y_cursor * tty->width + tty->x_cursor] = blank;
      tty_paint_cell_psf(blank, tty);
      tty->x_cursor++;
    }
    break;
  }

  case '\b':
    if (tty->x_cursor > 0) {
      tty->x_cursor--;
    } else if (tty->y_cursor > 0) {
      tty->y_cursor--;
      tty->x_cursor = tty->width - 1;
    }

    {
      terminal_cell_t blank = {
          .printable_char = ' ', .fg = currentFg, .bg = currentBg};
      // Clear the buffer entry too
      int idx = tty->y_cursor * tty->width + tty->x_cursor;
      buffer[idx] = blank;
      tty_paint_cell_psf(blank, tty); // Draw blank at new cursor
    }
    break;

  default:
    {
      terminal_cell_t cell = {
          .printable_char = c, .fg = currentFg, .bg = currentBg};
      // Store in buffer so it can be restored when cursor moves
      int idx = tty->y_cursor * tty->width + tty->x_cursor;
      buffer[idx] = cell;
      tty_paint_cell_psf(cell, tty);
      tty->x_cursor++;
    }
    break;
  }
  if (tty->x_cursor >= tty->width) {
    tty->x_cursor = 0;
    tty->y_cursor++;
  }
  if (tty->y_cursor >= tty->height) {
    // Scroll buffer: shift all rows up by one
    for (int row = 1; row < tty->height; ++row) {
      for (int col = 0; col < tty->width; ++col) {
        buffer[(row - 1) * tty->width + col] = buffer[row * tty->width + col];
      }
    }
    // Clear the last row
    terminal_cell_t blank = {.printable_char = ' ', .fg = currentFg, .bg = currentBg};
    for (int col = 0; col < tty->width; ++col) {
      buffer[(tty->height - 1) * tty->width + col] = blank;
    }
    
    scroll_framebuffer(currentBg, g_font.header->height);
    tty->x_cursor = 0;
    tty->y_cursor = tty->height - 1;
  }
  tty_paint_cursor(tty->x_cursor, tty->y_cursor);
}

void tty_putchar(char c) { hansi_handler(c); }

// Paint cursor at current TTY's cursor position
void tty_paint_cursor(uint32_t x, uint32_t y) {
  if (!current_tty) return;
  // Temporarily set cursor position for painting
  uint16_t saved_x = current_tty->x_cursor;
  uint16_t saved_y = current_tty->y_cursor;
  current_tty->x_cursor = x;
  current_tty->y_cursor = y;
  
  terminal_cell_t cell = {
      .printable_char = '_', .fg = currentFg, .bg = currentBg};
  tty_paint_cell_psf(cell, current_tty);
  
  current_tty->x_cursor = saved_x;
  current_tty->y_cursor = saved_y;
}

void tty_toggle_cursor_visibility() {
  if (!current_tty) return;
  static int cursor_visible = 1;
  if (cursor_visible) {
    tty_paint_cursor(current_tty->x_cursor, current_tty->y_cursor);
  } else {
    // Restore the actual character from buffer instead of painting space
    int idx = current_tty->y_cursor * current_tty->width + current_tty->x_cursor;
    terminal_cell_t cell;
    if (current_tty->buffer[idx].printable_char != 0) {
      cell = current_tty->buffer[idx];
    } else {
      cell = (terminal_cell_t){.printable_char = ' ', .fg = currentFg, .bg = currentBg};
    }
    tty_paint_cell_psf(cell, current_tty);
  }
  cursor_visible = !cursor_visible;
}

void set_currentFg(uint32_t value) { currentFg = value; }

void set_currentBg(uint32_t value) { currentBg = value; }

void tty_clear() {
  if (current_tty) {
    current_tty->x_cursor = 0;
    current_tty->y_cursor = 0;
    memset(current_tty->buffer, 0, sizeof(terminal_cell_t) * current_tty->width * current_tty->height);
  }
  framebuffer_clear(currentBg);
}

tty_t* get_current_tty(){
  return current_tty;
}