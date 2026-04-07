#include "cilk_explicit.hh"
THREAD(fun);
int main();
THREAD(fun_cont0);
THREAD(fun_cont1);
THREAD(main_cont0);

CLOSURE_DEF(fun,
    long n;
);
CLOSURE_DEF(fun_cont0,
    long x0;
    long y0;
);
CLOSURE_DEF(fun_cont1,
    long x;
    long y;
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
    long x0;
    long y0;
    fun_closure *largs = (fun_closure*)(args.get());
    w = 14;
    if ((largs->n > 4)) {
        fun_cont1_closure SN_fun_cont1c(largs->k);
        spawn_next<fun_cont1_closure> SN_fun_cont1(SN_fun_cont1c);
        cont sp0k;
        SN_BIND(SN_fun_cont1, &sp0k, x);
        fun_closure sp0c(sp0k);
        sp0c.n = (largs->n - 5);
        spawn<fun_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND(SN_fun_cont1, &sp1k, y);
        fun_closure sp1c(sp1k);
        sp1c.n = (largs->n - 2);
        spawn<fun_closure> sp1(sp1c);

        // Original sync was here
    } else {
        if ((largs->n > 2)) {
            fun_cont0_closure SN_fun_cont0c(largs->k);
            spawn_next<fun_cont0_closure> SN_fun_cont0(SN_fun_cont0c);
            cont sp2k;
            SN_BIND(SN_fun_cont0, &sp2k, x0);
            fun_closure sp2c(sp2k);
            sp2c.n = (largs->n - 5);
            spawn<fun_closure> sp2(sp2c);

            cont sp3k;
            SN_BIND(SN_fun_cont0, &sp3k, y0);
            fun_closure sp3c(sp3k);
            sp3c.n = (largs->n - 2);
            spawn<fun_closure> sp3(sp3c);

            // Original sync was here
        } else {
            w = (w + 40);
            w = (w * 10);
            w = (w - 6);
            w = (w * 10);
            SEND_ARGUMENT(largs->k, w);
        }
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
THREAD(fun_cont0) {
    long w;
    fun_cont0_closure *largs = (fun_cont0_closure*)(args.get());
    largs->x0 = (3 + largs->x0);
    largs->y0 = (largs->y0 + 6);
    w = (largs->x0 + largs->y0);
    w = (w * 10);
    SEND_ARGUMENT(largs->k, w);
    return;
}
THREAD(fun_cont1) {
    long w;
    fun_cont1_closure *largs = (fun_cont1_closure*)(args.get());
    largs->x = (3 + largs->x);
    largs->y = (largs->y + 6);
    w = (largs->x + largs->y);
    w = (w * 10);
    SEND_ARGUMENT(largs->k, w);
    return;
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    printf("fun = %d\n",largs->n0);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
