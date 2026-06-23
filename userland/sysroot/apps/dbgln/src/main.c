#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char** argv){
  if(argc < 2){
    printf("Send debug messages to serial device. Usually /dev/sr0\n");
    printf("Usage: dbgln <message>\n");
  }
  int fd = open("/dev/sr0", 1);
  if(fd < 0) {
    printf("Failed to open serial device\n");
    return 1;
  }
  for(int i = 1; i < argc; i++) {
      const char* msg = argv[i];
      write(fd, msg, strlen(msg));
      write(fd, " ", 1);
  }
  write(fd, "\n", 1);
  close(fd);
  return EXIT_SUCCESS;
}
