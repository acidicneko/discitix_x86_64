#include <drivers/keyboard.h>
#include <drivers/pit.h>
#include <drivers/tty/tty.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <stdarg.h>

void putchar(char c) { tty_putchar(c); }

void puts(const char *str) {
  while (*str != 0) {
    putchar(*str);
    str++;
  }
}

int __vsprintf__(char *fmt, va_list args, void (*putchar_func)(char c),
                 void (*puts_func)(const char *str)) {
  int integer;
  uint64_t unsigned_long;
  uint32_t unsigned_int;
  uint16_t unsigned_short;
  uint8_t unsigned_char;
  char character;
  char *str = NULL;

  char buffer[32]; // Local buffer for conversions

  while (*fmt != 0) {
    if (*fmt == '%') {
      fmt++;
      switch (*fmt) {
      case 'c':
        character = va_arg(args, int);
        putchar_func(character);
        break;
      case 's':
        str = va_arg(args, char *);
        puts_func(str ? str : "(null)");
        break;
      case 'd':
      case 'i':
        integer = va_arg(args, int);
        itoa(integer, buffer, 10);
        puts_func(buffer);
        break;
      case 'b':
        integer = va_arg(args, int);
        itoa(integer, buffer, 2);
        puts_func(buffer);
        break;
      case 'x':
        fmt++;
        switch (*fmt) {
        case 'l':
          unsigned_long = va_arg(args, uint64_t);
          utoa(unsigned_long, buffer, 16);
          puts_func(buffer);
          break;
        case 'i':
          unsigned_int = va_arg(args, uint32_t);
          utoa((uint64_t)unsigned_int, buffer, 16);
          puts_func(buffer);
          break;
        case 's':
          unsigned_short = va_arg(args, int);
          utoa((uint64_t)unsigned_short, buffer, 16);
          puts_func(buffer);
          break;
        case 'd':
          integer = va_arg(args, int);
          itoa(integer, buffer, 16);
          puts_func(buffer);
          break;
        case 'c':
          character = va_arg(args, int);
          itoa((int)character, buffer, 16);
          puts_func(buffer);
          break;
        case 'h':
          unsigned_char = va_arg(args, int);
          utoa((uint64_t)unsigned_char, buffer, 16);
          puts_func(buffer);
          break;
        default:
          putchar_func(*fmt);
          break;
        }
        break;
      case 'u':
        fmt++;
        switch (*fmt) {
        case 'l':
          unsigned_long = va_arg(args, uint64_t);
          utoa(unsigned_long, buffer, 10);
          puts_func(buffer);
          break;
        case 'i':
          unsigned_int = va_arg(args, uint32_t);
          utoa((uint64_t)unsigned_int, buffer, 10);
          puts_func(buffer);
          break;
        case 's':
          unsigned_short = va_arg(args, int);
          utoa((uint64_t)unsigned_short, buffer, 10);
          puts_func(buffer);
          break;
        case 'h':
          unsigned_char = va_arg(args, int);
          utoa((uint64_t)unsigned_char, buffer, 10);
          puts_func(buffer);
          break;
        default:
          putchar_func(*fmt);
          break;
        }
        break;
      default:
        putchar_func(*fmt);
        break;
      }
    } else {
      putchar_func(*fmt);
    }
    fmt++;
  }
  return 0;
}

int printf(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  __vsprintf__(fmt, args, putchar, puts);
  va_end(args);
  return 0;
}

void gets(char *to) {
  char c;
  int index = 0;
  while (1) {
    c = keyboard_read();
    if (c == '\b') {
      if (index != 0) {
        index--;
        putchar(c);
      }
    } else if (c == '\n') {
      to[index] = '\0';
      putchar(c);
      return;
    } else {
      to[index++] = c;
      putchar(c);
    }
  }
}

void wait(uint16_t ms) {
  int required_ticks = ms * get_ticks();
  pit_wait(required_ticks);
}
