#include <cilk/cilk.h>
#include <stdio.h>

long fun(long n) {
  long w;
  w = 14;

  if (n > 2) {
    long x, y, f, h;
    x = cilk_spawn fun(n - 5);
    y = cilk_spawn fun(n - 2);
    cilk_sync;
    f = cilk_spawn fun(n - 5);
    h = cilk_spawn fun(n - 2);
    cilk_sync;
    x = 3 + x;
    y = y + 6;
    w = (x + y + f + h);
  }

  w = w + 40;
  w = w * 10;
  w = w - 6;
  return w;
}

int main() {
  int n = fun(8);
  printf("fun = %d\n", n);
  return 0;
}