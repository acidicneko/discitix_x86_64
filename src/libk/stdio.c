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

int sprintf(char *out_buffer, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char *out_ptr = out_buffer; // Pointer to track our position in the output buffer
    
    int integer;
    uint64_t unsigned_long;
    uint32_t unsigned_int;
    uint16_t unsigned_short;
    uint8_t unsigned_char;
    char character;
    char *str_arg = NULL;
    char temp_buffer[32]; // Local buffer for numeric conversions

    while (*fmt != '\0') {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'c':
                    character = (char)va_arg(args, int);
                    *out_ptr++ = character;
                    break;
                case 's':
                    str_arg = va_arg(args, char *);
                    if (!str_arg) str_arg = "(null)";
                    while (*str_arg) {
                        *out_ptr++ = *str_arg++;
                    }
                    break;
                case 'd':
                case 'i':
                    integer = va_arg(args, int);
                    itoa(integer, temp_buffer, 10);
                    for (int i = 0; temp_buffer[i] != '\0'; i++) {
                        *out_ptr++ = temp_buffer[i];
                    }
                    break;
                case 'b':
                    integer = va_arg(args, int);
                    itoa(integer, temp_buffer, 2);
                    for (int i = 0; temp_buffer[i] != '\0'; i++) {
                        *out_ptr++ = temp_buffer[i];
                    }
                    break;
                case 'x':
                    fmt++;
                    temp_buffer[0] = '\0'; // Reset temp buffer
                    switch (*fmt) {
                        case 'l':
                            unsigned_long = va_arg(args, uint64_t);
                            utoa(unsigned_long, temp_buffer, 16);
                            break;
                        case 'i':
                            unsigned_int = va_arg(args, uint32_t);
                            utoa((uint64_t)unsigned_int, temp_buffer, 16);
                            break;
                        case 's':
                            unsigned_short = (uint16_t)va_arg(args, int);
                            utoa((uint64_t)unsigned_short, temp_buffer, 16);
                            break;
                        case 'd':
                            integer = va_arg(args, int);
                            itoa(integer, temp_buffer, 16);
                            break;
                        case 'c':
                            character = (char)va_arg(args, int);
                            itoa((int)character, temp_buffer, 16);
                            break;
                        case 'h':
                            unsigned_char = (uint8_t)va_arg(args, int);
                            utoa((uint64_t)unsigned_char, temp_buffer, 16);
                            break;
                        default:
                            *out_ptr++ = *fmt;
                            break;
                    }
                    // Copy the converted hex string to the output buffer
                    for (int i = 0; temp_buffer[i] != '\0'; i++) {
                        *out_ptr++ = temp_buffer[i];
                    }
                    break;
                case 'u':

                    fmt++;
                    temp_buffer[0] = '\0'; // Reset temp buffer
                    switch (*fmt) {
                        case 'l':
                            unsigned_long = va_arg(args, uint64_t);
                            utoa(unsigned_long, temp_buffer, 10);
                            break;
                        case 'i':
                            unsigned_int = va_arg(args, uint32_t);
                            utoa((uint64_t)unsigned_int, temp_buffer, 10);
                            break;
                        case 's':
                            unsigned_short = (uint16_t)va_arg(args, int);
                            utoa((uint64_t)unsigned_short, temp_buffer, 10);
                            break;
                        case 'h':
                            unsigned_char = (uint8_t)va_arg(args, int);
                            utoa((uint64_t)unsigned_char, temp_buffer, 10);
                            break;
                        default:
                            *out_ptr++ = *fmt;
                            break;
                    }
                    // Copy the converted unsigned string to the output buffer
                    for (int i = 0; temp_buffer[i] != '\0'; i++) {
                        *out_ptr++ = temp_buffer[i];
                    }
                    break;
                default:
                    *out_ptr++ = *fmt;
                    break;
            }
        } else {
            // Normal characters just get copied over
            *out_ptr++ = *fmt;
        }
        fmt++;
    }

    *out_ptr = '\0'; // Crucial: Null-terminate the final string

    va_end(args);

    // Return the number of characters written (excluding the null terminator)
    return (out_ptr - out_buffer); 
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
