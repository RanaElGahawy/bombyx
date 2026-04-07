#include <cilk/cilk.h>
#include <stdio.h>

int f(int x) { return x + 1; }

int g(int x) { return x + 100; }

int original(int n, int m) {
  int total = 0;
  int i = 0;

  // Texpr_seq_0
  total += 7;

  while (i < n) { // Texpr_c_0
    int j = 0;
    int inner_sum = 0;

    // Texpr_seq_1
    total += 10;

    while (j < m) { // Texpr_c_1
      int x;

      // Texpr_seq_2
      inner_sum += 1;

      x = cilk_spawn f(j);

      // Texpr_seq_3
      inner_sum += 2;

      cilk_sync;
      inner_sum += x;
      j = j + 1;
    }

    // Texpr_seq_4
    total += inner_sum;

    cilk_sync;

    // Texpr_seq_5
    total += 20;

    if ((i % 2) == 0) { // Texpr_c_2
      int y;

      // Texpr_seq_6
      total += 30;

      y = cilk_spawn f(i);
      cilk_sync;

      // Texpr_seq_7
      total += y;
    } else {
      int z;

      // Texpr_seq_8
      total += 40;

      z = cilk_spawn g(i);
      cilk_sync;

      // Texpr_seq_9
      total += z;
    }

    // Texpr_seq_10
    total += 50;
    i = i + 1;
  }

  // Texpr_seq_11
  total += 60;
  return total;
}

int main() {
  int ans = original(3, 2);
  printf("%d\n", ans);
  return 0;
}