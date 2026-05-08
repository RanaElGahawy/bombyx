#include "cilk_explicit.hh"
#include <cilk/cilk.h>
#include <stdio.h>

struct Pair {
  int x;
  int y;
};

THREAD(sum_pair);
THREAD(dot_product);
int main();
THREAD(dot_product_cont0);
THREAD(main_cont0);

CLOSURE_DEF(sum_pair, Pair p;);
CLOSURE_DEF(dot_product, Pair a; Pair b;);
CLOSURE_DEF(dot_product_cont0, int s1; int s2;);
CLOSURE_DEF(main_cont0, int result;);
// struct_test_2.cpp
// EXPECTED: PASS (likely)
// Tests: struct value type declared locally, dot-member access,
//        struct passed by value to spawned function.
// Risk: struct-typed CopyIRStmt — depends on whether codegen handles
//       struct-sized copies. If codegen assumes scalar types this may
//       silently produce wrong code rather than crashing.

THREAD(sum_pair) {
  sum_pair_closure *largs = (sum_pair_closure *)(args.get());
  SEND_ARGUMENT(largs->k, (largs->p.x + largs->p.y));
  return;
}
THREAD(dot_product) {
  int s1;
  int s2;
  dot_product_closure *largs = (dot_product_closure *)(args.get());
  dot_product_cont0_closure SN_dot_product_cont0c(largs->k);
  spawn_next<dot_product_cont0_closure> SN_dot_product_cont0(
      SN_dot_product_cont0c);
  cont sp0k;
  SN_BIND(SN_dot_product_cont0, &sp0k, s1);
  sum_pair_closure sp0c(sp0k);
  sp0c.p = largs->a;
  spawn<sum_pair_closure> sp0(sp0c);

  cont sp1k;
  SN_BIND(SN_dot_product_cont0, &sp1k, s2);
  sum_pair_closure sp1c(sp1k);
  sp1c.p = largs->b;
  spawn<sum_pair_closure> sp1(sp1c);

  // Original sync was here
  return;
}
int main() {
  Pair a0;
  Pair b0;
  int result;
  a0.x = 1;
  a0.y = 2;
  b0.x = 3;
  b0.y = 4;
  main_cont0_closure SN_main_cont0c(CONT_DUMMY);
  spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
  cont sp0k;
  SN_BIND(SN_main_cont0, &sp0k, result);
  dot_product_closure sp0c(sp0k);
  sp0c.a = a0;
  sp0c.b = b0;
  spawn<dot_product_closure> sp0(sp0c);

  // Original sync was here
  return 0;
}
THREAD(dot_product_cont0) {
  dot_product_cont0_closure *largs = (dot_product_cont0_closure *)(args.get());
  SEND_ARGUMENT(largs->k, (largs->s1 * largs->s2));
  return;
}
THREAD(main_cont0) {
  main_cont0_closure *largs = (main_cont0_closure *)(args.get());
  printf("result = %d\n", largs->result);
  SEND_ARGUMENT(largs->k, 0);
  return;
}
