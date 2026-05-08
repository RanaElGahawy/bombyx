// struct_test_2.cpp
// EXPECTED: PASS (likely)
// Tests: struct value type declared locally, dot-member access,
//        struct passed by value to spawned function.
// Risk: struct-typed CopyIRStmt — depends on whether codegen handles
//       struct-sized copies. If codegen assumes scalar types this may
//       silently produce wrong code rather than crashing.

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