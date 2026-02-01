#include <libk/string.h>
#include <libk/utils.h>
#include <drivers/framebuffer.h>
#include <drivers/tty/font.h>
#include <drivers/tty/hansi_parser.h>
#include <drivers/tty/psf2.h>
#include <drivers/tty/tty.h>
#include <stddef.h>
#include <stdint.h>
#include <mm/liballoc.h>
#include <kernel/vfs/vfs.h>
#include <stdlib.h>

tty_t* current_tty = NULL;

uint32_t colors[16];

uint32_t currentBg;
uint32_t currentFg;

uint32_t x_cursor;
uint32_t y_cursor;

bool tty_initialized = false;

fb_info_t *current_fb = NULL;

tty_t ttys[4];

void tty_push(tty_t* tty, terminal_cell_t* cell){
  terminal_cell_t* buffer = tty->buffer;
  char c = cell->printable_char;
  switch (c) {
  case '\n':
    tty->x_cursor = 0;
    tty->y_cursor++;
    break;

  case '\r':
    tty->x_cursor = 0;
    break;

  case '\t':
    tty->x_cursor = (tty->x_cursor - (tty->x_cursor % 8)) + 8;
    break;

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
    // Clear the last row
    for (int col = 0; col < tty->width; ++col) {
      buffer[(tty->height - 1) * tty->width + col] = (terminal_cell_t){0};
    }
    // Keep cursor at the last row
    tty->y_cursor = tty->height - 1;
    tty->x_cursor = 0;
    
    // If this is the active TTY, scroll the framebuffer
    if (current_tty && tty->id == current_tty->id) {
      scroll_framebuffer(tty->colors[0], g_font.header->height);
    }
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

  terminal_cell_t* cell = (terminal_cell_t*)kmalloc(sizeof(terminal_cell_t));
  
  while(bytes_wrote < len && str[bytes_wrote]!=0){

    cell->printable_char = str[bytes_wrote];
    cell->fg = tty->colors[7];
    cell->bg = tty->colors[0];
    tty_push(tty, cell);
    if (tty->id == current_tty->id){ //flush to current active tty
      // Paint at the position where the character was stored (before cursor advance)
      uint16_t paint_x, paint_y;
      // Calculate the position where the character was actually stored
      if (cell->printable_char == '\n' || cell->printable_char == '\r' || 
          cell->printable_char == '\t' || cell->printable_char == '\b') {
        // Control characters don't need painting
      } else {
        // Character was stored at previous position (before x_cursor was incremented)
        if (tty->x_cursor == 0) {
          // Wrapped to new line, char was at end of previous line
          paint_x = tty->width - 1;
          paint_y = (tty->y_cursor > 0) ? tty->y_cursor - 1 : 0;
        } else {
          paint_x = tty->x_cursor - 1;
          paint_y = tty->y_cursor;
        }
        uint16_t saved_x = tty->x_cursor;
        uint16_t saved_y = tty->y_cursor;
        tty->x_cursor = paint_x;
        tty->y_cursor = paint_y;
        tty_paint_cell_psf(*cell, tty);
        tty->x_cursor = saved_x;
        tty->y_cursor = saved_y;
      }
    }

    bytes_wrote++;
  }
  
  kfree(cell);
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
      if(tty->buffer[i].printable_char == 0){
        continue;
      }
      tty->x_cursor = col;
      tty->y_cursor = row;
      tty_paint_cell_psf(tty->buffer[i], tty);
    }
  }
  
  tty->x_cursor = saved_x;
  tty->y_cursor = saved_y;
  current_tty = tty;
}

// Forward declaration for tty_read
long tty_read(file_t* file, void* buf, size_t len, uint64_t off);

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
  superblock_t* root_sb = vfs_get_root_superblock();
  for (int i = 0; i < TTY_NUM; ++i) {
    tty_node = (inode_t*)kmalloc(sizeof(inode_t));  
    memset(tty_node, 0, sizeof(inode_t));
    tty_node->atime = 0;
    tty_node->ctime = 0;
    tty_node->is_directory = 0;
    tty_node->size = 0;
    tty_node->ino = 6000+i;
    tty_node->f_ops = &tty_file_ops;
    tty_node->mode = 6;
    
    dentry_t *d = (dentry_t *)kmalloc(sizeof(dentry_t));
    memset(d, 0, sizeof(dentry_t));
    
    char id_str[2];
    itoa(i, id_str, 10);
    char name[5] = "tty";
    strcat(name, id_str);
    strncpy(d->name, name, 5);
    
    d->name[NAME_MAX - 1] = '\0';
    d->inode = tty_node;
    d->parent = root_sb->root;
    d->next = root_sb->root->next;
    root_sb->root->next = d;
    
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
  dbgln("TTYs registered - Defaulting to /tty0\n\r");
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
  
  tty_hide_cursor(); // Hide the cursor before printing
  switch (c) {
  case '\n':
    tty->x_cursor = 0;
    tty->y_cursor++;
    break;

  case '\r':
    tty->x_cursor = 0;
    break;

  case '\t':
    tty->x_cursor = (tty->x_cursor - (tty->x_cursor % 8)) + 8;
    break;

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
      tty->buffer[idx] = (terminal_cell_t){0};
      tty_paint_cell_psf(blank, tty); // Draw blank at new cursor
    }
    break;

  default:
    {
      terminal_cell_t cell = {
          .printable_char = c, .fg = currentFg, .bg = currentBg};
      // Store in buffer so it can be restored when cursor moves
      int idx = tty->y_cursor * tty->width + tty->x_cursor;
      tty->buffer[idx] = cell;
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
    scroll_framebuffer(currentBg, g_font.header->height);
    tty->x_cursor = 0;
    tty->y_cursor--;
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
    // Clear the TTY buffer so content doesn't reappear on switch
    memset(current_tty->buffer, 0, sizeof(terminal_cell_t) * current_tty->width * current_tty->height);
  }
  framebuffer_clear(currentBg);
}

tty_t* get_current_tty(){
  return current_tty;
}

// Set line discipline mode
void tty_set_ldisc(tty_t* tty, int mode) {
  tty->ldisc_mode = mode;
}

// Handle a character input from keyboard to a specific TTY
void tty_input_char(tty_t* tty, char c) {
  if (tty->ldisc_mode == TTY_RAW) {
    // Raw mode: just put char directly into input buffer
    size_t next_head = (tty->input_head + 1) % TTY_MAX_BUF;
    if (next_head != tty->input_tail) { // Buffer not full
      tty->input_buf[tty->input_head] = c;
      tty->input_head = next_head;
    }
    // Don't echo in raw mode for escape sequences
    if (tty->echo && tty->id == current_tty->id && c >= 32 && c < 127) {
      tty_putchar(c);
    }
  } else {
    // Canonical mode: line editing with escape sequence handling
    
    // Handle escape sequence parsing
    if (tty->input_esc_state == INPUT_STATE_ESC) {
      if (c == '[') {
        tty->input_esc_state = INPUT_STATE_CSI;
        return;
      } else {
        // Not a CSI sequence, reset
        tty->input_esc_state = INPUT_STATE_NORMAL;
        return;
      }
    }
    
    if (tty->input_esc_state == INPUT_STATE_CSI) {
      tty->input_esc_state = INPUT_STATE_NORMAL;
      switch (c) {
        case 'A': // Up arrow - could implement history later
          return;
        case 'B': // Down arrow - could implement history later
          return;
        case 'C': // Right arrow - move cursor right
          if (tty->line_cursor < tty->line_length) {
            tty->line_cursor++;
            if (tty->echo && tty->id == current_tty->id) {
              tty_putchar('\033');
              tty_putchar('[');
              tty_putchar('C');
            }
          }
          return;
        case 'D': // Left arrow - move cursor left
          if (tty->line_cursor > 0) {
            tty->line_cursor--;
            if (tty->echo && tty->id == current_tty->id) {
              tty_putchar('\033');
              tty_putchar('[');
              tty_putchar('D');
            }
          }
          return;
        case 'H': // Home - move to beginning
          while (tty->line_cursor > 0) {
            tty->line_cursor--;
            if (tty->echo && tty->id == current_tty->id) {
              tty_putchar('\033');
              tty_putchar('[');
              tty_putchar('D');
            }
          }
          return;
        case 'F': // End - move to end
          while (tty->line_cursor < tty->line_length) {
            tty->line_cursor++;
            if (tty->echo && tty->id == current_tty->id) {
              tty_putchar('\033');
              tty_putchar('[');
              tty_putchar('C');
            }
          }
          return;
        case '3': // Could be Delete (ESC[3~)
          // For simplicity, just reset state - delete needs ~ after
          return;
        default:
          return;
      }
    }
    
    // Check for escape character
    if (c == '\033') {
      tty->input_esc_state = INPUT_STATE_ESC;
      return;
    }
    
    if (c == '\n' || c == '\r') {
      // End of line - copy line buffer to input buffer
      if (tty->line_length < TTY_MAX_BUF - 1) {
        tty->line_buffer[tty->line_length++] = '\n';
      }
      // Copy line to input ring buffer
      for (size_t i = 0; i < tty->line_length; i++) {
        size_t next_head = (tty->input_head + 1) % TTY_MAX_BUF;
        if (next_head != tty->input_tail) {
          tty->input_buf[tty->input_head] = tty->line_buffer[i];
          tty->input_head = next_head;
        }
      }
      tty->line_length = 0;
      tty->line_cursor = 0;
      tty->line_ready = true;
      // Echo newline
      if (tty->echo && tty->id == current_tty->id) {
        tty_putchar('\n');
      }
    } else if (c == '\b' || c == 127) {
      // Backspace - delete character before cursor
      if (tty->line_cursor > 0) {
        // Shift characters left
        for (size_t i = tty->line_cursor - 1; i < tty->line_length - 1; i++) {
          tty->line_buffer[i] = tty->line_buffer[i + 1];
        }
        tty->line_length--;
        tty->line_cursor--;
        // Echo: move back, redraw rest of line, clear last char, move cursor back
        if (tty->echo && tty->id == current_tty->id) {
          tty_putchar('\b');
          for (size_t i = tty->line_cursor; i < tty->line_length; i++) {
            tty_putchar(tty->line_buffer[i]);
          }
          tty_putchar(' ');  // Clear last character
          // Move cursor back to position
          for (size_t i = tty->line_cursor; i <= tty->line_length; i++) {
            tty_putchar('\b');
          }
        }
      }
    } else if (c == 0x03) {
      // Ctrl+C - clear line buffer (simple handling)
      tty->line_length = 0;
      tty->line_cursor = 0;
      if (tty->echo && tty->id == current_tty->id) {
        tty_putchar('^');
        tty_putchar('C');
        tty_putchar('\n');
      }
    } else if (c == 0x0C) {
      // Ctrl+L - clear screen
      if (tty->id == current_tty->id) {
        tty_clear();
      }
    } else if (c >= 32 && c < 127) {
      // Printable character - insert at cursor position
      if (tty->line_length < TTY_MAX_BUF - 1) {
        // Shift characters right to make room
        for (size_t i = tty->line_length; i > tty->line_cursor; i--) {
          tty->line_buffer[i] = tty->line_buffer[i - 1];
        }
        tty->line_buffer[tty->line_cursor] = c;
        tty->line_length++;
        tty->line_cursor++;
        // Echo
        if (tty->echo && tty->id == current_tty->id) {
          // Print from cursor position to end
          for (size_t i = tty->line_cursor - 1; i < tty->line_length; i++) {
            tty_putchar(tty->line_buffer[i]);
          }
          // Move cursor back to correct position
          for (size_t i = tty->line_cursor; i < tty->line_length; i++) {
            tty_putchar('\b');
          }
        }
      }
    }
  }
}

// Read from TTY - implements line discipline
long tty_read(file_t* file, void* buf, size_t len, uint64_t off) {
  (void)off;
  if (!buf || len == 0) {
    return -1;
  }
  
  tty_t* tty = &ttys[file->inode->ino - 6000];
  char* dest = (char*)buf;
  size_t bytes_read = 0;
  
  if (tty->ldisc_mode == TTY_CANONICAL) {
    // Canonical mode: wait for a complete line
    while (!tty->line_ready) {
      // Wait for input (allow interrupts)
      asm volatile("sti; hlt; cli");
    }
    
    // Read from input buffer until newline or len reached
    while (bytes_read < len && tty->input_tail != tty->input_head) {
      char c = tty->input_buf[tty->input_tail];
      tty->input_tail = (tty->input_tail + 1) % TTY_MAX_BUF;
      dest[bytes_read++] = c;
      if (c == '\n') {
        break;  // Return at end of line in canonical mode
      }
    }
    
    // Check if more data available, if not reset line_ready
    if (tty->input_tail == tty->input_head) {
      tty->line_ready = false;
    }
  } else {
    // Raw mode: return whatever is available, or wait for at least one char
    while (tty->input_tail == tty->input_head) {
      asm volatile("sti; hlt; cli");
    }
    
    while (bytes_read < len && tty->input_tail != tty->input_head) {
      dest[bytes_read++] = tty->input_buf[tty->input_tail];
      tty->input_tail = (tty->input_tail + 1) % TTY_MAX_BUF;
    }
  }
  
  return (long)bytes_read;
}
