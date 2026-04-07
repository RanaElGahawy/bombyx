#include <cilk/cilk.h>
#include <stdio.h>

int original(int n) {
  // Texpr_seq_0
  //   printf("Start n = %d\n", n);

  // base case
  if (n <= 1) {
    // Texpr_seq_1
    // printf("Base case n = %d\n", n);
    return 1;
  }

  // Texpr_seq_2
  //   printf("Spawning for n = %d\n", n);

  // two recursive spawns
  int left = cilk_spawn original(n - 1);
  int right = cilk_spawn original(n - 2);

  cilk_sync;

  // Texpr_seq_3
  int result = left + right;
  //   printf("Done n = %d -> %d\n", n, result);

  return result;
}

int main() {
  int res = original(5);
  printf("Final result = %d\n", res);
  return 0;
}