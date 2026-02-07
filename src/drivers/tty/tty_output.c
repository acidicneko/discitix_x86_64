#include <drivers/tty/tty.h>
#include <drivers/tty/psf2.h>
#include <stddef.h>
#include <stdint.h>


void tty_push(tty_t* tty, terminal_cell_t* cell){
  terminal_cell_t* buffer = tty->buffer;
  char c = cell->printable_char;
  switch (c) {
  case '\n': {
    // Clear from current cursor to end of line before moving to next line
    terminal_cell_t blank = {.printable_char = ' ', .fg = cell->fg, .bg = cell->bg};
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
    // Tab: move to next 8-column boundary, clearing cells along the way
    uint16_t target_x = (tty->x_cursor - (tty->x_cursor % 8)) + 8;
    if (target_x > tty->width) target_x = tty->width;
    terminal_cell_t blank = {.printable_char = ' ', .fg = cell->fg, .bg = cell->bg};
    while (tty->x_cursor < target_x) {
      buffer[tty->y_cursor * tty->width + tty->x_cursor] = blank;
      tty->x_cursor++;
    }
    break;
  }

  case '\b':
    if (tty->x_cursor > 0) {
      tty->x_cursor--;
    } else if (tty->y_cursor > 0) {
      tty->y_cursor--;
    }
    break;
  default:
      buffer[tty->y_cursor * tty->width + tty->x_cursor] = *cell;
      tty->x_cursor++;
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
    // Clear the last row with proper space characters
    terminal_cell_t blank = {.printable_char = ' ', .fg = tty->colors[7], .bg = tty->colors[0]};
    for (int col = 0; col < tty->width; ++col) {
      buffer[(tty->height - 1) * tty->width + col] = blank;
    }
    // Keep cursor at the last row
    tty->y_cursor = tty->height - 1;
    // Don't scroll framebuffer here - tty_write will do full repaint
  }

}

long tty_write(file_t* file, const void* data, size_t len, uint64_t off){
  (void)off;
  if (!data) {
    return -1;
  }
  const char* str = (const char*)data;
  size_t bytes_wrote = 0;
 
  tty_t* tty = &ttys[file->inode->ino - 6000];

  terminal_cell_t cell;
  cell.fg = tty->colors[7];
  cell.bg = tty->colors[0];
  
  while(bytes_wrote < len && str[bytes_wrote] != 0){
    cell.printable_char = str[bytes_wrote];
    tty_push(tty, &cell);
    bytes_wrote++;
  }
  
  // After all characters are pushed, repaint the entire screen from buffer
  if (tty->id == current_tty->id) {
    framebuffer_clear(tty->colors[0]);
    uint16_t saved_x = tty->x_cursor;
    uint16_t saved_y = tty->y_cursor;
    
    for (int row = 0; row < tty->height; ++row) {
      for (int col = 0; col < tty->width; ++col) {
        int i = row * tty->width + col;
        terminal_cell_t c = tty->buffer[i];
        if (c.printable_char == 0) {
          c.printable_char = ' ';
          c.fg = tty->colors[7];
          c.bg = tty->colors[0];
        }
        tty->x_cursor = col;
        tty->y_cursor = row;
        tty_paint_cell_psf(c, tty);
      }
    }
    
    tty->x_cursor = saved_x;
    tty->y_cursor = saved_y;
  }
  
  return (long)bytes_wrote;
}

void tty_switch(int id){
  tty_t* tty = &ttys[id];
  uint16_t saved_x = tty->x_cursor;
  uint16_t saved_y = tty->y_cursor;
  
  framebuffer_clear(tty->colors[0]);
  
  for(int row = 0; row < tty->height; ++row){
    for(int col = 0; col < tty->width; ++col){
      int i = row * tty->width + col;
      tty->x_cursor = col;
      tty->y_cursor = row;
      // If cell is empty (printable_char == 0), paint as space
      terminal_cell_t cell = tty->buffer[i];
      if (cell.printable_char == 0) {
        cell.printable_char = ' ';
        cell.fg = tty->colors[7];
        cell.bg = tty->colors[0];
      }
      tty_paint_cell_psf(cell, tty);
    }
  }
  
  tty->x_cursor = saved_x;
  tty->y_cursor = saved_y;
  current_tty = tty;
}