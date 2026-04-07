#include <cilk/cilk.h>
#include <stdio.h>

long fun(long n) {
  long w;
  w = 14;

  if (n > 4) {
    long x, y;
    x = cilk_spawn fun(n - 5);
    y = cilk_spawn fun(n - 2);
    cilk_sync;
    x = 3 + x;
    y = y + 6;
    w = (x + y);
  } else if (n > 2) {
    long x, y;
    x = cilk_spawn fun(n - 5);
    y = cilk_spawn fun(n - 2);
    cilk_sync;
    x = 3 + x;
    y = y + 6;
    w = (x + y);
  } else {
    w = w + 40;
    w = w * 10;
    w = w - 6;
  }
  w = w * 10;
  return w;
}

int main() {
  int n = fun(8);
  printf("fun = %d\n", n);
  return 0;
}