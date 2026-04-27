#include "cilk_explicit.hh"
THREAD(worker);
THREAD(fun);
int main();
THREAD(fun_loop_body0);
THREAD(fun_loop_cont0);
THREAD(fun_exit0);
THREAD(fun_if_cont0);
THREAD(main_cont0);

CLOSURE_DEF(worker, long n;);
CLOSURE_DEF(fun, long n0;);
CLOSURE_DEF(fun_loop_body0, long n0; long w; long result;);
CLOSURE_DEF(fun_loop_cont0, long n0; long w; long result; long val0; long val1;);
CLOSURE_DEF(fun_exit0, long result;);
CLOSURE_DEF(fun_if_cont0, long x; long y_out; long z_out;);
CLOSURE_DEF(main_cont0, long n1;);

#include <cilk/cilk.h>
#include <memory>
#include <stdio.h>

THREAD(worker) {
  worker_closure *largs = (worker_closure *)(args.get());
  SEND_ARGUMENT(largs->k, (largs->n * largs->n));
  return;
}

THREAD(fun) {
  long w;
  long result;
  fun_closure *largs = (fun_closure *)(args.get());

  w = 14;
  result = 2;

  if ((w > 0)) {
    auto next = std::make_shared<fun_loop_body0_closure>(largs->k);
    next->n0 = largs->n0;
    next->w = w;
    next->result = result;
    fun_loop_body0(next);
  } else {
    SEND_ARGUMENT(largs->k, result);
  }
  return;
}

THREAD(fun_loop_body0) {
  long sum;
  fun_loop_body0_closure *largs = (fun_loop_body0_closure *)(args.get());

  if ((largs->w > 0)) {
    if ((largs->n0 > 0)) {
      sum = (largs->w + largs->n0);

      fun_loop_cont0_closure SN_fun_loop_cont0c(largs->k);
      spawn_next<fun_loop_cont0_closure> SN_fun_loop_cont0(SN_fun_loop_cont0c);

      ((fun_loop_cont0_closure *)SN_fun_loop_cont0.cls.get())->n0 = largs->n0;
      ((fun_loop_cont0_closure *)SN_fun_loop_cont0.cls.get())->w = largs->w;
      ((fun_loop_cont0_closure *)SN_fun_loop_cont0.cls.get())->result = largs->result;

      cont sp0k;
      SN_BIND(SN_fun_loop_cont0, &sp0k, val0);
      worker_closure sp0c(sp0k);
      sp0c.n = largs->n0;
      spawn<worker_closure> sp0(sp0c);

      cont sp1k;
      SN_BIND(SN_fun_loop_cont0, &sp1k, val1);
      worker_closure sp1c(sp1k);
      sp1c.n = sum;
      spawn<worker_closure> sp1(sp1c);

      // Original sync was here
    } else {
      auto next = std::make_shared<fun_loop_body0_closure>(largs->k);
      next->n0 = largs->n0;
      next->w = (largs->w - 1);
      next->result = largs->result;
      fun_loop_body0(next);
    }
  } else {
    auto next = std::make_shared<fun_exit0_closure>(largs->k);
    next->result = largs->result;
    fun_exit0(next);
  }
  return;
}

THREAD(fun_loop_cont0) {
  fun_loop_cont0_closure *largs = (fun_loop_cont0_closure *)(args.get());

  largs->result = ((largs->result + largs->val0) + largs->val1);

  auto next = std::make_shared<fun_loop_body0_closure>(largs->k);
  next->n0 = (largs->n0 - 1);
  next->w = largs->w;
  next->result = largs->result;
  fun_loop_body0(next);

  return;
}

THREAD(fun_exit0) {
  long x;
  long y_init;
  fun_exit0_closure *largs = (fun_exit0_closure *)(args.get());

  x = largs->result;

  if ((x > 3)) {
    fun_if_cont0_closure SN_fun_if_cont0c(largs->k);
    spawn_next<fun_if_cont0_closure> SN_fun_if_cont0(SN_fun_if_cont0c);

    ((fun_if_cont0_closure *)SN_fun_if_cont0.cls.get())->x = x;

    cont sp0k;
    SN_BIND(SN_fun_if_cont0, &sp0k, z_out);
    worker_closure sp0c(sp0k);
    sp0c.n = x;
    spawn<worker_closure> sp0(sp0c);

    y_init = 3;
    cont sp1k;
    SN_BIND(SN_fun_if_cont0, &sp1k, y_out);
    worker_closure sp1c(sp1k);
    sp1c.n = y_init;
    spawn<worker_closure> sp1(sp1c);

    // Original sync was here
  } else {
    SEND_ARGUMENT(largs->k, largs->result);
  }
  return;
}

THREAD(fun_if_cont0) {
  long result;
  fun_if_cont0_closure *largs = (fun_if_cont0_closure *)(args.get());

  result = ((largs->x + largs->y_out) + largs->z_out);
  SEND_ARGUMENT(largs->k, result);
  return;
}

int main() {
  long n1;
  main_cont0_closure SN_main_cont0c(CONT_DUMMY);
  spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);

  cont sp0k;
  SN_BIND(SN_main_cont0, &sp0k, n1);
  fun_closure sp0c(sp0k);
  sp0c.n0 = 8;
  spawn<fun_closure> sp0(sp0c);

  // Original sync was here
  return 0;
}

THREAD(main_cont0) {
  main_cont0_closure *largs = (main_cont0_closure *)(args.get());
  printf("fun = %ld\n", largs->n1);
  SEND_ARGUMENT(largs->k, 0);
  return;
}