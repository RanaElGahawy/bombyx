#include "cilk_explicit.hh"
#include <cilk/cilk.h>

struct Node {
  int value;
  int weight;
};

THREAD(process);
THREAD(compute);
int main();
THREAD(compute_cont0);
THREAD(main_cont0);

CLOSURE_DEF(process,
    Node * n;
);
CLOSURE_DEF(compute,
    Node * a;
    Node * b;
);
CLOSURE_DEF(compute_cont0,
    int x;
    int y;
);
CLOSURE_DEF(main_cont0,
    int result;
);




#include <stdio.h>


THREAD(process) {
    process_closure *largs = (process_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->n->value * largs->n->weight));
    return;
}
THREAD(compute) {
    int x;
    int y;
    compute_closure *largs = (compute_closure*)(args.get());
    compute_cont0_closure SN_compute_cont0c(largs->k);
    spawn_next<compute_cont0_closure> SN_compute_cont0(SN_compute_cont0c);
    cont sp0k;
    SN_BIND(SN_compute_cont0, &sp0k, x);
    process_closure sp0c(sp0k);
    sp0c.n = largs->a;
    spawn<process_closure> sp0(sp0c);

    cont sp1k;
    SN_BIND(SN_compute_cont0, &sp1k, y);
    process_closure sp1c(sp1k);
    sp1c.n = largs->b;
    spawn<process_closure> sp1(sp1c);

    // Original sync was here
    return;
}
int main() {
    Node a0;
    Node b0;
    int result;
    a0.value = 3;
    a0.weight = 4;
    b0.value = 5;
    b0.weight = 6;
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, result);
    compute_closure sp0c(sp0k);
    sp0c.a = &(a0);
    sp0c.b = &(b0);
    spawn<compute_closure> sp0(sp0c);

    // Original sync was here
    return 0;
}
THREAD(compute_cont0) {
    compute_cont0_closure *largs = (compute_cont0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->x + largs->y));
    return;
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    printf("result = %d\n",largs->result);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
