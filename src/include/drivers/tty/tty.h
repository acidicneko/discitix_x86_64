#ifndef __TTY_H__
#define __TTY_H__

#include <init/stivale2.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  char printable_char;
  uint32_t fg;
  uint32_t bg;
} terminal_cell_t;

extern uint32_t colors[];
extern bool tty_initialized;

void init_tty();
void init_colors(uint32_t black, uint32_t red, uint32_t green, uint32_t yellow,
                 uint32_t blue, uint32_t purple, uint32_t cyan, uint32_t white,
                 uint32_t blackBright, uint32_t redBright, uint32_t greenBright,
                 uint32_t yellowBright, uint32_t blueBright,
                 uint32_t purpleBright, uint32_t cyanBright,
                 uint32_t whiteBright);

void tty_paint_cell(terminal_cell_t cell);
void tty_paint_cell_psf(terminal_cell_t cell);
void tty_putchar_raw(char c);
void tty_putchar(char c);
void tty_paint_cursor(uint32_t x, uint32_t y);
void tty_toggle_cursor_visibility();

void set_currentFg(uint32_t value);
void set_currentBg(uint32_t value);

void tty_clear();

#endif
