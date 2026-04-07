#include <cilk/cilk.h>
#include <stdio.h>

int f(int x) { return x + 1; }
int g(int x) { return x + 100; }

int original(int n) {
  int i = 0;
  int total = 0;

  // Texpr_seq_0
  total += 5;

  while (i < n) { // Texpr_c_0
    // Texpr_seq_1
    total += 10;

    if ((i % 2) == 0) { // Texpr_c_2   // inner Oexpr: Rule 5
      int a;

      // Texpr_seq_2
      total += 1;

      a = cilk_spawn f(i);
      cilk_sync;

      // Texpr_seq_3
      total += a;
    } else {
      int b;

      // Texpr_seq_4
      total += 2;

      b = cilk_spawn g(i);
      cilk_sync;

      // Texpr_seq_5
      total += b;
    }

    // Texpr_seq_7
    total += 20;
    i = i + 1;
  }

  // Texpr_seq_8
  total += 30;
  return total;
}

int main() {
  int y = original(4);
  printf("%d\n", y);
  return 0;
}