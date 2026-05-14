#include <cilk/cilk.h>
#include <stdio.h>

struct Counter {
  int val;
};

int counter_get(Counter *c) { return c->val; }

int worker(int x) {
  Counter c;
  c.val = x;
  return counter_get(&c);
}

int compute(int a, int b) {
  int x = cilk_spawn worker(a);
  int y = cilk_spawn worker(b);
  cilk_sync;
  return x + y;
}

int main() {
  int result = cilk_spawn compute(10, 20);
  cilk_sync;
  printf("result = %d\n", result);
  return 0;
}