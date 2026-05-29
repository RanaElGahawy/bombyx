#include "cilk_explicit.hh"
#include <cilk/cilk.h>
#include <stdio.h>

struct Counter {
  int val;
};

int counter_get(Counter *c);
THREAD(worker);
THREAD(compute);
int main();
THREAD(compute_cont0);
THREAD(main_cont0);

CLOSURE_DEF(worker,
    int x;
);
CLOSURE_DEF(compute,
    int a;
    int b;
);
CLOSURE_DEF(compute_cont0,
    int x;
    int y;
);
CLOSURE_DEF(main_cont0,
    int result;
);







int counter_get(Counter *c) {
    return c->val;
}
THREAD(worker) {
    Counter c;
    worker_closure *largs = (worker_closure*)(args.get());
    c.val = largs->x;
    SEND_ARGUMENT(largs->k, counter_get(&(c)));
}
THREAD(compute) {
    int x;
    int y;
    compute_closure *largs = (compute_closure*)(args.get());
    compute_cont0_closure SN_compute_cont0c(largs->k);
    spawn_next<compute_cont0_closure> SN_compute_cont0(SN_compute_cont0c);
    cont sp0k;
    SN_BIND(SN_compute_cont0, &sp0k, x);
    worker_closure sp0c(sp0k);
    sp0c.x = largs->a;
    spawn<worker_closure> sp0(sp0c);

    cont sp1k;
    SN_BIND(SN_compute_cont0, &sp1k, y);
    worker_closure sp1c(sp1k);
    sp1c.x = largs->b;
    spawn<worker_closure> sp1(sp1c);

    // Original sync was here
    return;
}
int main() {
    int result;
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, result);
    compute_closure sp0c(sp0k);
    sp0c.a = 10;
    sp0c.b = 20;
    spawn<compute_closure> sp0(sp0c);

    // Original sync was here
    return 0;
}
THREAD(compute_cont0) {
    compute_cont0_closure *largs = (compute_cont0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->x + largs->y));
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    printf("result = %d\n",largs->result);
    SEND_ARGUMENT(largs->k, 0);
}
