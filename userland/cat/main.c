// a simple cat program
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    println("Usage: cat <filename>");
    return 1;
  }

  for (int i = 1; i < argc; ++i) {
    int fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      print("cat: cannot open ");
      println(argv[i]);
      return 1;
    }

    char buf[512];
    long n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
      write(1, buf, n); // write to stdout
    }
    close(fd);
  }
  return 0;
}
