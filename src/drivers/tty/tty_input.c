#include <drivers/tty/tty.h>
#include <drivers/tty/psf2.h>
#include <drivers/framebuffer.h>
#include <libk/errno.h>
#include <stddef.h>
#include <stdint.h>

#define TCGETS      0x5401
#define TCSETS      0x5402
#define TIOCGWINSZ  0x5413
#define LFLAG_ICANON 0000002   // same bits as userland's <termios.h>
#define LFLAG_ECHO   0000010

struct winsize {
  uint16_t ws_row, ws_col, ws_xpixel, ws_ypixel;
};

void tty_set_ldisc(tty_t* tty, int mode) {
  tty->ldisc_mode = mode;
}

static void echo(tty_t *tty, char c) {
  if (tty->echo) tty_write_chars(tty, &c, 1);
}

static void echo_seq(tty_t *tty, const char *s) {
  while (*s) echo(tty, *s++);
}

// Hand the edited line to readers (Enter, or Ctrl-D on a non-empty line)
static void flush_line(tty_t *tty) {
  for (size_t i = 0; i < tty->line_length; i++) {
    size_t next_head = (tty->input_head + 1) % TTY_MAX_BUF;
    if (next_head == tty->input_tail) break;
    tty->input_buf[tty->input_head] = tty->line_buffer[i];
    tty->input_head = next_head;
  }
  tty->line_length = 0;
  tty->line_cursor = 0;
  tty->line_ready = true;
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
    if (c >= 32 && c < 127) echo(tty, c);
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
            echo_seq(tty, "\033[C");
          }
          return;
        case 'D': // Left arrow - move cursor left
          if (tty->line_cursor > 0) {
            tty->line_cursor--;
            echo_seq(tty, "\033[D");
          }
          return;
        case 'H': // Home - move to beginning
          while (tty->line_cursor > 0) {
            tty->line_cursor--;
            echo_seq(tty, "\033[D");
          }
          return;
        case 'F': // End - move to end
          while (tty->line_cursor < tty->line_length) {
            tty->line_cursor++;
            echo_seq(tty, "\033[C");
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
      tty->line_buffer[tty->line_length++] = '\n';
      flush_line(tty);
      echo(tty, '\n');
    } else if (c == 0x04) {
      // Ctrl+D: EOF on an empty line, otherwise pass the line on as-is
      if (tty->line_length == 0) {
        tty->eof_pending = true;
        tty->line_ready = true;
      } else {
        flush_line(tty);
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
        // Move back, redraw the rest of the line, blank the old last char,
        // then move back to the cursor
        echo(tty, '\b');
        for (size_t i = tty->line_cursor; i < tty->line_length; i++) {
          echo(tty, tty->line_buffer[i]);
        }
        echo(tty, ' ');
        for (size_t i = tty->line_cursor; i <= tty->line_length; i++) {
          echo(tty, '\b');
        }
      }
    } else if (c == 0x03) {
      // Ctrl+C - clear line buffer (simple handling)
      tty->line_length = 0;
      tty->line_cursor = 0;
      echo_seq(tty, "^C\n");
    } else if (c == 0x0C) {
      // Ctrl+L - clear screen
      if (tty == current_tty) {
        tty_clear();
      }
    } else if (c >= 32 && c < 127) {
      // Printable character - insert at cursor position
      if (tty->line_length < TTY_MAX_BUF - 2) {
        // Shift characters right to make room
        for (size_t i = tty->line_length; i > tty->line_cursor; i--) {
          tty->line_buffer[i] = tty->line_buffer[i - 1];
        }
        tty->line_buffer[tty->line_cursor] = c;
        tty->line_length++;
        tty->line_cursor++;
        for (size_t i = tty->line_cursor - 1; i < tty->line_length; i++) {
          echo(tty, tty->line_buffer[i]);
        }
        for (size_t i = tty->line_cursor; i < tty->line_length; i++) {
          echo(tty, '\b');
        }
      }
    }
  }
}

// Read from TTY - implements line discipline
long tty_read(file_t* file, void* buf, size_t len, uint64_t off) {
  (void)off;
  if (!buf) return -EFAULT;
  if (len == 0) return 0;

  tty_t* tty = (tty_t *)file->inode->private;
  if (!tty) return -ENXIO;
  char* dest = (char*)buf;
  size_t bytes_read = 0;

  if (tty->ldisc_mode == TTY_CANONICAL) {
    while (!tty->line_ready) {
      asm volatile("sti; hlt; cli");
    }

    if (tty->input_tail == tty->input_head && tty->eof_pending) {
      tty->eof_pending = false;
      tty->line_ready = false;
      return 0;
    }

    while (bytes_read < len && tty->input_tail != tty->input_head) {
      char c = tty->input_buf[tty->input_tail];
      tty->input_tail = (tty->input_tail + 1) % TTY_MAX_BUF;
      dest[bytes_read++] = c;
      if (c == '\n') break;
    }

    if (tty->input_tail == tty->input_head && !tty->eof_pending) {
      tty->line_ready = false;
    }
  } else {
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

int tty_ioctl(file_t* file, uint64_t request, void* arg) {
  tty_t *tty = (tty_t *)file->inode->private;
  if (!tty) return -ENOTTY;
  if (!arg) return -EFAULT;

  switch (request) {
  case TIOCGWINSZ: {
    struct winsize *ws = (struct winsize *)arg;
    ws->ws_row = tty->height;
    ws->ws_col = tty->width;
    ws->ws_xpixel = current_fb ? current_fb->width : 0;
    ws->ws_ypixel = current_fb ? current_fb->height : 0;
    return 0;
  }
  case TCGETS:
    *(uint32_t *)arg = (tty->ldisc_mode == TTY_CANONICAL ? LFLAG_ICANON : 0) |
                       (tty->echo ? LFLAG_ECHO : 0);
    return 0;
  case TCSETS: {
    uint32_t lflag = *(uint32_t *)arg;
    int mode = (lflag & LFLAG_ICANON) ? TTY_CANONICAL : TTY_RAW;
    if (mode == TTY_RAW && tty->ldisc_mode == TTY_CANONICAL && tty->line_length)
      flush_line(tty);
    tty_set_ldisc(tty, mode);
    tty->echo = (lflag & LFLAG_ECHO) != 0;
    return 0;
  }
  default:
    return -EINVAL;
  }
}
