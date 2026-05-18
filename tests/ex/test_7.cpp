#include "cilk_explicit.hh"
#include <cilk/cilk.h>
#include <stdio.h>

struct Accum {
  int value;
  int count;
};

THREAD(worker);
THREAD(reduce);
int main();
THREAD(reduce_exit0);
THREAD(reduce_reentry0);
THREAD(main_cont0);
THREAD(reduce_reentry0_cont0);

CLOSURE_DEF(worker,
    int x;
);
CLOSURE_DEF(reduce,
    int *arr;
    int n;
);
CLOSURE_DEF(reduce_exit0,
    int *arr;
    int n;
    Accum acc;
    int i;
    int r;
);
CLOSURE_DEF(reduce_reentry0,
    int *arr;
    int n;
    Accum acc;
    int i;
    int r;
);
CLOSURE_DEF(main_cont0,
    int result;
);
CLOSURE_DEF(reduce_reentry0_cont0,
    int *arr;
    int n;
    Accum acc;
    int i;
    int r;
);





THREAD(worker) {
    worker_closure *largs = (worker_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->x * largs->x));
}
THREAD(reduce) {
    Accum acc;
    int i;
    int r;
    reduce_closure *largs = (reduce_closure*)(args.get());
    acc.value = 0;
    acc.count = 0;
    i = 0;
    auto sp0c = std::make_shared<reduce_reentry0_closure>(largs->k);
    sp0c->arr = largs->arr;
    sp0c->n = largs->n;
    sp0c->acc = acc;
    sp0c->i = i;
    sp0c->r = r;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
int main() {
    int arr[4];
    int result;
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    arr[3] = 4;
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, result);
    reduce_closure sp0c(sp0k);
    sp0c.arr = arr;
    sp0c.n = 4;
    spawn<reduce_closure> sp0(sp0c);

    // Original sync was here
    return 0;
}
THREAD(reduce_exit0) {
    reduce_exit0_closure *largs = (reduce_exit0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, largs->acc.value);
}
THREAD(reduce_reentry0) {
    reduce_reentry0_closure *largs = (reduce_reentry0_closure*)(args.get());
    if ((largs->i < largs->n)) {
        reduce_reentry0_cont0_closure SN_reduce_reentry0_cont0c(largs->k);
        spawn_next<reduce_reentry0_cont0_closure> SN_reduce_reentry0_cont0(SN_reduce_reentry0_cont0c);
        cont sp0k;
        SN_BIND(SN_reduce_reentry0_cont0, &sp0k, r);
        worker_closure sp0c(sp0k);
        sp0c.x = largs->arr[largs->i];
        spawn<worker_closure> sp0(sp0c);

        ((reduce_reentry0_cont0_closure*)SN_reduce_reentry0_cont0.cls.get())->i = largs->i;
        ((reduce_reentry0_cont0_closure*)SN_reduce_reentry0_cont0.cls.get())->acc = largs->acc;
        ((reduce_reentry0_cont0_closure*)SN_reduce_reentry0_cont0.cls.get())->n = largs->n;
        ((reduce_reentry0_cont0_closure*)SN_reduce_reentry0_cont0.cls.get())->arr = largs->arr;
        // Original sync was here
    } else {
        auto sp1c = std::make_shared<reduce_exit0_closure>(largs->k);
        sp1c->arr = largs->arr;
        sp1c->n = largs->n;
        sp1c->acc = largs->acc;
        sp1c->i = largs->i;
        sp1c->r = largs->r;
        cilk_spawn taskSpawn(sp1c->getTask(), sp1c);
        return;
    }
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    printf("result = %d\n",largs->result);
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(reduce_reentry0_cont0) {
    reduce_reentry0_cont0_closure *largs = (reduce_reentry0_cont0_closure*)(args.get());
    largs->acc.value = (largs->acc.value + largs->r);
    largs->acc.count = (largs->acc.count + 1);
    largs->i = (largs->i + 1);
    auto sp0c = std::make_shared<reduce_reentry0_closure>(largs->k);
    sp0c->arr = largs->arr;
    sp0c->n = largs->n;
    sp0c->acc = largs->acc;
    sp0c->i = largs->i;
    sp0c->r = largs->r;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
