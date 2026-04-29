#include "cilk_explicit.hh"
THREAD(worker);
THREAD(fun);
int main();
THREAD(fun_exit0);
THREAD(fun_reentry0);
THREAD(main_cont0);
THREAD(fun_reentry0_cont0);

CLOSURE_DEF(worker,
    int x;
);
CLOSURE_DEF(fun,
    int rounds;
);
CLOSURE_DEF(fun_exit0,
    int rounds;
    int iter;
    int total;
    int a;
    int b;
);
CLOSURE_DEF(fun_reentry0,
    int rounds;
    int iter;
    int total;
    int a;
    int b;
);
CLOSURE_DEF(main_cont0,
);
CLOSURE_DEF(fun_reentry0_cont0,
    int rounds;
    int iter;
    int total;
    int a;
    int b;
);
#include <cilk/cilk.h>
#include <stdio.h>






THREAD(worker) {
    worker_closure *largs = (worker_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->x + 1));
    return;
}
THREAD(fun) {
    int iter;
    int total;
    int a;
    int b;
    fun_closure *largs = (fun_closure*)(args.get());
    iter = 0;
    total = 0;
    auto sp0c = std::make_shared<fun_reentry0_closure>(largs->k);
    sp0c->rounds = largs->rounds;
    sp0c->iter = iter;
    sp0c->total = total;
    sp0c->a = a;
    sp0c->b = b;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
int main() {
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    cont sp0k;
    SN_BIND_VOID(SN_main_cont0, &sp0k);
    fun_closure sp0c(sp0k);
    sp0c.rounds = 4;
    spawn<fun_closure> sp0(sp0c);

    // Original sync was here
    return 0;
}
THREAD(fun_exit0) {
    fun_exit0_closure *largs = (fun_exit0_closure*)(args.get());
    largs->total = (largs->total + 100);
    printf("fun = %d\n",largs->total);
    return;
}
THREAD(fun_reentry0) {
    fun_reentry0_closure *largs = (fun_reentry0_closure*)(args.get());
    if ((largs->iter < largs->rounds)) {
        fun_reentry0_cont0_closure SN_fun_reentry0_cont0c(largs->k);
        spawn_next<fun_reentry0_cont0_closure> SN_fun_reentry0_cont0(SN_fun_reentry0_cont0c);
        cont sp0k;
        SN_BIND(SN_fun_reentry0_cont0, &sp0k, a);
        worker_closure sp0c(sp0k);
        sp0c.x = largs->iter;
        spawn<worker_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND(SN_fun_reentry0_cont0, &sp1k, b);
        worker_closure sp1c(sp1k);
        sp1c.x = (largs->iter + 10);
        spawn<worker_closure> sp1(sp1c);

        ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->iter = largs->iter;
        ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->total = largs->total;
        ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->rounds = largs->rounds;
        // Original sync was here
    } else {
        auto sp2c = std::make_shared<fun_exit0_closure>(largs->k);
        sp2c->rounds = largs->rounds;
        sp2c->iter = largs->iter;
        sp2c->total = largs->total;
        sp2c->a = largs->a;
        sp2c->b = largs->b;
        cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
        return;
    }
    return;
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fun_reentry0_cont0) {
    fun_reentry0_cont0_closure *largs = (fun_reentry0_cont0_closure*)(args.get());
    largs->total = (largs->total + largs->a);
    largs->total = (largs->total + largs->b);
    largs->iter = (largs->iter + 1);
    auto sp0c = std::make_shared<fun_reentry0_closure>(largs->k);
    sp0c->rounds = largs->rounds;
    sp0c->iter = largs->iter;
    sp0c->total = largs->total;
    sp0c->a = largs->a;
    sp0c->b = largs->b;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
