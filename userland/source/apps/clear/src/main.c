#include <stdio.h>

int main(int argc, char *argv[])
{
  printf("\033[2J\033[H");
  fflush(stdout);
  return 0;
}
