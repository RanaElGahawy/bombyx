#include <cilk/cilk.h>
#include <stdio.h>

int worker(int x) { return x + 1; }

void fun(int rounds) {
  int iter = 0;
  int total = 0;

  while (iter < rounds) {
    int a, b;
    a = cilk_spawn worker(iter);
    b = cilk_spawn worker(iter + 10);
    cilk_sync;

    total = total + a;
    total = total + b;
    iter = iter + 1;
  }

  total = total + 100;
  printf("fun = %d\n", total);
}

int main() {
  fun(4);
  return 0;
}