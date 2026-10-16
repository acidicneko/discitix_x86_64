#ifndef __TTY_H__
#define __TTY_H__

#include <kernel/vfs/vfs.h>
#include <init/stivale2.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <libk/spinlock.h>
#include <drivers/framebuffer.h>
#include <drivers/tty/hansi_parser.h>

#define TTY_MAX_BUF 256

#define TTY_CANONICAL  0
#define TTY_RAW        1

#define TTY_NUM       4

typedef struct {
  char printable_char;
  uint32_t fg;
  uint32_t bg;
} terminal_cell_t;

// Input escape sequence parser states
#define INPUT_STATE_NORMAL  0
#define INPUT_STATE_ESC     1
#define INPUT_STATE_CSI     2

typedef struct tty {
  uint8_t id;

  terminal_cell_t *buffer;
  uint32_t *colors;
  uint16_t width;
  uint16_t height;

  uint16_t x_cursor;
  uint16_t y_cursor;

  // Output attributes and escape-sequence state, per TTY
  uint32_t fg;
  uint32_t bg;
  bool bold;
  int8_t fg_index;   // 0-7 for a basic SGR color (bold brightens it), else -1
  hansi_parser parser;

  // Line discipline modes
  int ldisc_mode;           // TTY_CANONICAL or TTY_RAW
  bool echo;                // Echo input to screen
  
  // Input ring buffer
  char input_buf[TTY_MAX_BUF];
  volatile size_t input_head;   // Write position (keyboard writes here)
  volatile size_t input_tail;   // Read position (tty_read reads from here)
  
  // Canonical mode line editing buffer
  char line_buffer[TTY_MAX_BUF];
  size_t line_length;
  size_t line_cursor;           // Cursor position within line buffer for editing
  volatile bool line_ready;     // Set when Enter is pressed in canonical mode
  volatile bool eof_pending;    // Ctrl-D on an empty line: next read returns 0
  
  // Input escape sequence parser state
  int input_esc_state;
  
  file_operations_t tty_ops;
  
  spinlock_t lock; 

} tty_t;


extern tty_t* current_tty;

extern uint32_t colors[16];

extern uint32_t currentBg;
extern uint32_t currentFg;

extern uint32_t x_cursor;
extern uint32_t y_cursor;
extern bool tty_initialized;

extern fb_info_t *current_fb;

extern tty_t ttys[4];
extern bool tty_graphics_mode;

void init_tty();
void init_colors(uint32_t black, uint32_t red, uint32_t green, uint32_t yellow,
                 uint32_t blue, uint32_t purple, uint32_t cyan, uint32_t white,
                 uint32_t blackBright, uint32_t redBright, uint32_t greenBright,
                 uint32_t yellowBright, uint32_t blueBright,
                 uint32_t purpleBright, uint32_t cyanBright,
                 uint32_t whiteBright);

void tty_putchar(char c);
void tty_write_chars(tty_t *tty, const char *s, size_t len);
void tty_toggle_cursor_visibility();
void tty_repaint(tty_t *tty);
void tty_set_graphics_mode(bool on);

// Used by the ANSI parser
void tty_put_raw(tty_t *tty, char c);
void tty_erase(tty_t *tty, size_t from, size_t to);

void tty_clear();

void tty_switch(int id);
tty_t* get_current_tty();

// Input handling
void tty_input_char(tty_t* tty, char c);
void tty_set_ldisc(tty_t* tty, int mode);

long tty_read(file_t* file, void* buf, size_t len, uint64_t off);
long tty_write(file_t* file, const void* data, size_t len, uint64_t off);
int tty_ioctl(file_t* file, uint64_t request, void* arg);

#endif
