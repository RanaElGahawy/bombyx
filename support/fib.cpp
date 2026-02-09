#include <cilk/cilk.h>
#include <stdio.h>

#include "cilk_explicit_copy.hh"
THREAD(fib);
int main();
THREAD(fib_cont0);
THREAD(main_cont0);

CLOSURE_DEF(fib, int n;);
CLOSURE_DEF(fib_cont0, int f1; int f2;);
CLOSURE_DEF(main_cont0, int n0;);
THREAD(fib) {
  int f1;
  int f2;
  fib_closure *largs = (fib_closure *)(args.get());
  if ((largs->n < 2)) {
    SEND_ARGUMENT(largs->k, largs->n);
  } else {
    fib_cont0_closure SN_fib_cont0c(largs->k);
    spawn_next<fib_cont0_closure> SN_fib_cont0(SN_fib_cont0c);
    cont sp0k;
    SN_BIND(SN_fib_cont0, &sp0k, f1);
    fib_closure sp0c(sp0k);
    sp0c.n = (largs->n - 1);
    spawn<fib_closure> sp0(sp0c);

    cont sp1k;
    SN_BIND(SN_fib_cont0, &sp1k, f2);
    fib_closure sp1c(sp1k);
    sp1c.n = (largs->n - 2);
    spawn<fib_closure> sp1(sp1c);

    // Original sync was here
  }
  return;
}
int main() {
  int n0;
  main_cont0_closure SN_main_cont0c(CONT_DUMMY);
  spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
  cont sp0k;
  SN_BIND(SN_main_cont0, &sp0k, n0);
  fib_closure sp0c(sp0k);
  sp0c.n = 8;
  spawn<fib_closure> sp0(sp0c);

  // Original sync was here
  return 0;
}
THREAD(fib_cont0) {
  fib_cont0_closure *largs = (fib_cont0_closure *)(args.get());
  SEND_ARGUMENT(largs->k, (largs->f1 + largs->f2));
  return;
}
THREAD(main_cont0) {
  main_cont0_closure *largs = (main_cont0_closure *)(args.get());
  printf("fib = %d\n", largs->n0);
  SEND_ARGUMENT(largs->k, 0);
  return;
}
