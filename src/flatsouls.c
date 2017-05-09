#include "flatsouls_platform.h"
#include <stdio.h>

int main_loop(Bitmap bitmap) {
  int i,j,I,J;
  unsigned char* b = bitmap.buf;
  for (i = 0, I = bitmap.h; i < I; ++i)
  for (j = 0, J = bitmap.w; j < J; ++j) {
    *b++ = 0;
    *b++ = 255;
    *b++ = 0;
  }
  puts("Main loop called");
  return 0;
}

