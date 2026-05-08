#include <cilk/cilk.h>
#include <stdio.h>

struct Accum {
  int value;
  int count;
};

int worker(int x) { return x * x; }

int reduce(int *arr, int n) {
  Accum acc;
  acc.value = 0;
  acc.count = 0;
  int i = 0;
  while (i < n) {
    int r = cilk_spawn worker(arr[i]);
    cilk_sync;
    acc.value = acc.value + r;
    acc.count = acc.count + 1;
    i = i + 1;
  }
  return acc.value;
}

int main() {
  int arr[4];

  arr[0] = 1;
  arr[1] = 2;
  arr[2] = 3;
  arr[3] = 4;

  int result = reduce(arr, 4);

  printf("result = %d\n", result);

  return 0;
}