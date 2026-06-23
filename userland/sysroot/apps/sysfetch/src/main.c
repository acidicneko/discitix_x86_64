#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  int fd = open("/proc/meminfo", O_RDONLY);
  if (fd < 0) {
      printf("Failed to open /proc/meminfo\n");
      return EXIT_FAILURE;
  }

  char buf[128];
  int bytes_read = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  
  if (bytes_read < 0) return EXIT_FAILURE;
  buf[bytes_read] = '\0'; 
  char* token = strtok(buf, "/");
  int avail = token ? atoi(token) : 0;
  avail /= 1024;
  avail /= 1024;
  token = strtok(NULL, ",");
  int total = token ? atoi(token) : 0;
  total /= 1024;
  total /= 1024;

  printf("\033[1;34m '---' \t\033[1;37mKernel: \033[0mDiscitix Kernel\n");
  printf("\033[1;34m (O,O) \t\033[1;37mBuild:  \033[0m%s\n", __DATE__);
  
  printf("\033[1;34m /)_)\t\033[1;37mMemory: \033[0m%d/%d MB\n", avail, total);
  printf("\033[1;34m  \"\"  \n\033[0m\n");

  printf("\033[40m  \033[41m  \033[42m  \033[43m  \033[44m  \033[45m  \033[46m  "
         "\033[47m  \033[0m\n");
  printf("\033[21;40m  \033[21;41m  \033[21;42m  \033[21;43m  \033[21;44m  "
         "\033[21;45m  \033[21;46m  \033[21;47m  \033[0m\n");

  return EXIT_SUCCESS;
}
