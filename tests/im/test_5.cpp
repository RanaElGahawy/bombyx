#include <cilk/cilk.h>
#include <stdio.h>

struct Pair {
  int x;
  int y;
};

int sum_pair(Pair p) { return p.x + p.y; }

int dot_product(Pair a, Pair b) {
  int s1 = cilk_spawn sum_pair(a);
  int s2 = cilk_spawn sum_pair(b);
  cilk_sync;
  return s1 * s2;
}

int main() {
  Pair a;
  Pair b;

  a.x = 1;
  a.y = 2;

  b.x = 3;
  b.y = 4;

  int result = cilk_spawn dot_product(a, b);
  cilk_sync;
  printf("result = %d\n", result);

  return 0;
}