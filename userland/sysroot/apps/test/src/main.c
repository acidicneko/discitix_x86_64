#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv){
  int* a = malloc(sizeof(int));
  printf("address: %p\n", a);
  *a = 8;
  printf("value: %d\n", *a);
  free(a);
  int* b = malloc(sizeof(int));
  printf("address: %p\n", b);
  *b = 8;
  printf("value: %d\n", *b);
  free(b);
  return EXIT_SUCCESS;
}
