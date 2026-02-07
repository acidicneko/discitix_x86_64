#include <drivers/tty/tty.h>
#include <drivers/tty/psf2.h>
#include <drivers/framebuffer.h>
#include <stddef.h>
#include <stdint.h>

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
        case 'A': // Up arrow - TODO: could implement history later
          return;
        case 'B': // Down arrow - TODO: could implement history later
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
