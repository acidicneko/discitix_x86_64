#ifndef __TTY_H__
#define __TTY_H__

#include <kernel/vfs/vfs.h>
#include <init/stivale2.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <libk/spinlock.h>

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

typedef struct {
  uint8_t id;

  terminal_cell_t *buffer;
  uint32_t *colors;
  uint16_t width;
  uint16_t height;

  uint16_t x_cursor;
  uint16_t y_cursor;

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
  
  // Input escape sequence parser state
  int input_esc_state;
  
  file_operations_t tty_ops;
  
  spinlock_t lock; 

} tty_t;


static tty_t* current_tty = NULL;

static uint32_t colors[16];

static uint32_t currentBg;
static uint32_t currentFg;

static uint32_t x_cursor;
static uint32_t y_cursor;

static bool tty_initialized = false;

static fb_info_t *current_fb = NULL;

static tty_t ttys[4];

void init_tty();
void init_colors(uint32_t black, uint32_t red, uint32_t green, uint32_t yellow,
                 uint32_t blue, uint32_t purple, uint32_t cyan, uint32_t white,
                 uint32_t blackBright, uint32_t redBright, uint32_t greenBright,
                 uint32_t yellowBright, uint32_t blueBright,
                 uint32_t purpleBright, uint32_t cyanBright,
                 uint32_t whiteBright);

void tty_paint_cell(terminal_cell_t cell);
void tty_paint_cell_psf(terminal_cell_t cell, tty_t* tty);
void tty_putchar_raw(char c);
void tty_putchar(char c);
void tty_paint_cursor(uint32_t x, uint32_t y);
void tty_toggle_cursor_visibility();

void set_currentFg(uint32_t value);
void set_currentBg(uint32_t value);

void tty_clear();

void tty_switch(int id);
tty_t* get_current_tty();

// Input handling
void tty_input_char(tty_t* tty, char c);
void tty_set_ldisc(tty_t* tty, int mode);

long tty_read(file_t* file, void* buf, size_t len, uint64_t off);
long tty_write(file_t* file, const void* data, size_t len, uint64_t off);

#endif
