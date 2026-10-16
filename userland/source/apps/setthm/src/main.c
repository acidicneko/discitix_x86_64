#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define COLOR_COUNT 16
#define TCSETTHM 0x5696

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <colors.txt>\n", argv[0]);
    return EXIT_FAILURE;
  }

  FILE *file = fopen(argv[1], "r");
  if (!file) {
    perror("fopen");
    return EXIT_FAILURE;
  }

  uint32_t colors[COLOR_COUNT];
  char line[32];
  size_t count = 0;

  while (count < COLOR_COUNT && fgets(line, sizeof(line), file)) {
    char *end;
    unsigned long value = strtoul(line, &end, 16);

    if (end == line || value > 0xFFFFFF) {
      fprintf(stderr, "Invalid color on line %zu\n", count + 1);
      fclose(file);
      return EXIT_FAILURE;
    }

    colors[count++] = (uint32_t)value;
  }

  fclose(file);

  if (count != COLOR_COUNT) {
    fprintf(stderr, "Expected %d colors, got %zu\n", COLOR_COUNT, count);
    return EXIT_FAILURE;
  }

  int tty = open("/dev/tty0", O_RDWR);
  if (tty < 0) {
    perror("open tty");
    return EXIT_FAILURE;
  }

  if (ioctl(tty, TCSETTHM, colors) < 0) {
    perror("ioctl TTY_SET_COLORS");
    close(tty);
    return EXIT_FAILURE;
  }

  close(tty);
  return EXIT_SUCCESS;
}
