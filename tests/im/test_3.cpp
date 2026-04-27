#include <cilk/cilk.h>
#include <stdio.h>

unsigned long long worker(unsigned long long n) { return n * n; }

unsigned long long fun(long n) {
  unsigned long long w, result;
  w = 14;
  result = 2;

  if (w > 0) {

    if (n > 8) {
      if (w < 15) {
        unsigned long long y = cilk_spawn worker(n - 5);
        unsigned long long z = cilk_spawn worker(n - 3);
        cilk_sync;
        result = (n + y + z);
      } else {
        w = 0;
      }
    } else {
      w = 0;
    }

    if (w > 0) {
      while (n > 0) {
        unsigned long long sum = w + n;
        unsigned long long val_0 = cilk_spawn worker(n);
        unsigned long long val_1 = cilk_spawn worker(sum);
        cilk_sync;
        result = result + val_0 + val_1;
        n = n - 1;
      }
      w = w - 1;
    } else {
      w = 0;
    }

    unsigned long long x = result;
    if (x > 3) {
      int v = 0;
      while (v < 3) {
        unsigned long long z = cilk_spawn worker(x);
        unsigned long long y = 3;
        y = cilk_spawn worker(y);
        cilk_sync;
        result = (x + y + z);
        v = v + 1;
      }
    } else {
      w = 0;
    }
  } else {
    w = 0;
  }

  return result;
}

int main() {
  unsigned long long n = fun(15);
  printf("fun = %llu\n", n);
  return 0;
}