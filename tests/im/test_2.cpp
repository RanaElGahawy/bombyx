#include <cilk/cilk.h>
#include <stdio.h>

unsigned long long square(unsigned long long n) { return n * n; }
unsigned long long cube(unsigned long long n) { return n * n * n; }

unsigned long long compute(long depth, long width, long threshold) {
  unsigned long long result = 0;
  unsigned long long acc = 1;

  if (depth > 0) {
    if (width > threshold) {
      unsigned long long pre_a = cilk_spawn square(depth);
      unsigned long long pre_b = cilk_spawn cube(width);
      cilk_sync;
      acc = pre_a + pre_b;

      long d = 0;
      while (d < depth) {
        long w = 0;
        while (w < width) {
          unsigned long long a = cilk_spawn square(d + w);
          unsigned long long b = cilk_spawn cube(w + threshold);
          cilk_sync;

          if (a > b) {
            long i = 0;
            while (i < threshold) {
              unsigned long long p = cilk_spawn square(a + i);
              unsigned long long q = cilk_spawn cube(b + i);
              cilk_sync;
              acc = acc + p + q;
              i = i + 1;
            }
          } else {
            acc = acc + a + b;
          }

          w = w + 1;
        }
        result = result + acc;
        d = d + 1;
      }

    } else {
      unsigned long long pre_c = cilk_spawn cube(threshold);
      cilk_sync;
      acc = pre_c;

      long i = 0;
      while (i < threshold) {
        unsigned long long a = cilk_spawn square(i + depth);
        unsigned long long b = cilk_spawn cube(i + width);
        cilk_sync;

        if (a > threshold) {
          long j = 0;
          while (j < depth) {
            unsigned long long p = cilk_spawn square(a + j);
            unsigned long long q = cilk_spawn cube(b + j);
            cilk_sync;
            acc = acc + p + q;
            j = j + 1;
          }
        } else {
          acc = acc + a + b;
        }

        i = i + 1;
      }
      result = result + acc;
    }

  } else {
    unsigned long long x = cilk_spawn square(width + threshold);
    cilk_sync;
    result = x;
  }

  return result;
}

int main() {
  unsigned long long r0 = compute(3, 5, 2);
  unsigned long long r1 = compute(2, 1, 4);
  printf("r0 = %llu\n", r0);
  printf("r1 = %llu\n", r1);
  return 0;
}