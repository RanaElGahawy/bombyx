#include "cilk_explicit.hh"
THREAD(fun);
int main();
THREAD(fun_afterif0);
THREAD(fun_cont0);
THREAD(fun_cont1);
THREAD(main_cont0);

CLOSURE_DEF(fun,
    long n;
);
CLOSURE_DEF(fun_afterif0,
    long n;
    long w;
    long x;
    long y;
    long f;
    long h;
);
CLOSURE_DEF(fun_cont0,
    long n;
    long x;
    long y;
);
CLOSURE_DEF(fun_cont1,
    long n;
    long x;
    long y;
    long f;
    long h;
);
CLOSURE_DEF(main_cont0,
    int n0;
);
#include <cilk/cilk.h>
#include <stdio.h>




THREAD(fun) {
    long w;
    long x;
    long y;
    long f;
    long h;
    fun_closure *largs = (fun_closure*)(args.get());
    w = 14;
    if ((largs->n > 2)) {
        fun_cont0_closure SN_fun_cont0c(largs->k);
        spawn_next<fun_cont0_closure> SN_fun_cont0(SN_fun_cont0c);
        cont sp0k;
        SN_BIND(SN_fun_cont0, &sp0k, x);
        fun_closure sp0c(sp0k);
        sp0c.n = (largs->n - 5);
        spawn<fun_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND(SN_fun_cont0, &sp1k, y);
        fun_closure sp1c(sp1k);
        sp1c.n = (largs->n - 2);
        spawn<fun_closure> sp1(sp1c);

        ((fun_cont0_closure*)SN_fun_cont0.cls.get())->n = largs->n;
        // Original sync was here
    } else {
        auto sp2c = std::make_shared<fun_afterif0_closure>(largs->k);
        sp2c->n = largs->n;
        sp2c->w = w;
        sp2c->x = x;
        sp2c->y = y;
        sp2c->f = f;
        sp2c->h = h;
        cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
        return;
    }
    return;
}
int main() {
    int n0;
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, n0);
    fun_closure sp0c(sp0k);
    sp0c.n = 8;
    spawn<fun_closure> sp0(sp0c);

    // Original sync was here
    return 0;
}
THREAD(fun_afterif0) {
    fun_afterif0_closure *largs = (fun_afterif0_closure*)(args.get());
    largs->w = (largs->w + 40);
    largs->w = (largs->w * 10);
    largs->w = (largs->w - 6);
    SEND_ARGUMENT(largs->k, largs->w);
    return;
}
THREAD(fun_cont0) {
    long f;
    long h;
    fun_cont0_closure *largs = (fun_cont0_closure*)(args.get());
    fun_cont1_closure SN_fun_cont1c(largs->k);
    spawn_next<fun_cont1_closure> SN_fun_cont1(SN_fun_cont1c);
    cont sp0k;
    SN_BIND(SN_fun_cont1, &sp0k, f);
    fun_closure sp0c(sp0k);
    sp0c.n = (largs->n - 5);
    spawn<fun_closure> sp0(sp0c);

    cont sp1k;
    SN_BIND(SN_fun_cont1, &sp1k, h);
    fun_closure sp1c(sp1k);
    sp1c.n = (largs->n - 2);
    spawn<fun_closure> sp1(sp1c);

    ((fun_cont1_closure*)SN_fun_cont1.cls.get())->y = largs->y;
    ((fun_cont1_closure*)SN_fun_cont1.cls.get())->x = largs->x;
    ((fun_cont1_closure*)SN_fun_cont1.cls.get())->n = largs->n;
    // Original sync was here
    return;
}
THREAD(fun_cont1) {
    long w;
    fun_cont1_closure *largs = (fun_cont1_closure*)(args.get());
    largs->x = (3 + largs->x);
    largs->y = (largs->y + 6);
    w = (((largs->x + largs->y) + largs->f) + largs->h);
    auto sp0c = std::make_shared<fun_afterif0_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->w = w;
    sp0c->x = largs->x;
    sp0c->y = largs->y;
    sp0c->f = largs->f;
    sp0c->h = largs->h;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    printf("fun = %d\n",largs->n0);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
