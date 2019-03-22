#include <stdio.h>
#include <stdlib.h>
#include "vtbl64.h"

int get_bit(int n, int position){
  return (n & ( 1 << position )) >> position;
}

int main(int argc, char **argv) {

  int i, n = 1203;
  printf("%d : ", n);
  for(i = sizeof(n) * 8 - 1; i >= 0; i--) { 
    printf("%d", get_bit(n, i));
  }
  printf("\n");
  return 0;
}
