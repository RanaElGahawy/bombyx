#include "cilk_explicit.hh"
#include <cilk/cilk.h>
#include <stdio.h>

THREAD(f);
THREAD(g);
THREAD(original);
int main();
THREAD(original_exit0);
THREAD(original_reentry0);
THREAD(original_reentry0_afterif0);
THREAD(main_cont0);
THREAD(original_reentry0_cont0);
THREAD(original_reentry0_cont1);

struct f_data {
    int x;
};
struct original_exit0_data {
    int n;
    int i;
    int total;
    int a;
    int b;
};

CLOSURE_DEF_SHARED(f, f_data);
CLOSURE_DEF_SHARED(g, f_data);
CLOSURE_DEF(original,
    int n;
);
CLOSURE_DEF_SHARED(original_exit0, original_exit0_data);
CLOSURE_DEF_SHARED(original_reentry0, original_exit0_data);
CLOSURE_DEF_SHARED(original_reentry0_afterif0, original_exit0_data);
CLOSURE_DEF(main_cont0,
    int y;
);
CLOSURE_DEF_SHARED(original_reentry0_cont0, original_exit0_data);
CLOSURE_DEF_SHARED(original_reentry0_cont1, original_exit0_data);






THREAD(f) {
    f_closure *largs = (f_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->x + 1));
}
THREAD(g) {
    g_closure *largs = (g_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->x + 100));
}
THREAD(original) {
    int i;
    int total;
    int a;
    int b;
    original_closure *largs = (original_closure*)(args.get());
    i = 0;
    total = 0;
    total = (total + 5);
    auto sp0c = std::make_shared<original_reentry0_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->i = i;
    sp0c->total = total;
    sp0c->a = a;
    sp0c->b = b;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
int main() {
    int y;
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, y);
    original_closure sp0c(sp0k);
    sp0c.n = 4;
    spawn<original_closure> sp0(sp0c);

    // Original sync was here
    return 0;
}
THREAD(original_exit0) {
    original_exit0_closure *largs = (original_exit0_closure*)(args.get());
    largs->total = (largs->total + 30);
    SEND_ARGUMENT(largs->k, largs->total);
}
THREAD(original_reentry0) {
    original_reentry0_closure *largs = (original_reentry0_closure*)(args.get());
    if ((largs->i < largs->n)) {
        largs->total = (largs->total + 10);
        if (((largs->i % 2) == 0)) {
            largs->total = (largs->total + 1);
            original_reentry0_cont1_closure SN_original_reentry0_cont1c(largs->k);
            spawn_next<original_reentry0_cont1_closure> SN_original_reentry0_cont1(SN_original_reentry0_cont1c);
            cont sp0k;
            SN_BIND(SN_original_reentry0_cont1, &sp0k, a);
            f_closure sp0c(sp0k);
            sp0c.x = largs->i;
            spawn<f_closure> sp0(sp0c);

            *static_cast<original_exit0_data*>(SN_original_reentry0_cont1.cls.get()) = *largs;
            // Original sync was here
        } else {
            largs->total = (largs->total + 2);
            original_reentry0_cont0_closure SN_original_reentry0_cont0c(largs->k);
            spawn_next<original_reentry0_cont0_closure> SN_original_reentry0_cont0(SN_original_reentry0_cont0c);
            cont sp1k;
            SN_BIND(SN_original_reentry0_cont0, &sp1k, b);
            g_closure sp1c(sp1k);
            sp1c.x = largs->i;
            spawn<g_closure> sp1(sp1c);

            *static_cast<original_exit0_data*>(SN_original_reentry0_cont0.cls.get()) = *largs;
            // Original sync was here
        }
    } else {
        auto sp2c = std::make_shared<original_exit0_closure>(largs->k, *largs);
        cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
        return;
    }
}
THREAD(original_reentry0_afterif0) {
    original_reentry0_afterif0_closure *largs = (original_reentry0_afterif0_closure*)(args.get());
    largs->total = (largs->total + 20);
    largs->i = (largs->i + 1);
    auto sp0c = std::make_shared<original_reentry0_closure>(largs->k, *largs);
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    printf("%d\n",largs->y);
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(original_reentry0_cont0) {
    original_reentry0_cont0_closure *largs = (original_reentry0_cont0_closure*)(args.get());
    largs->total = (largs->total + largs->b);
    auto sp0c = std::make_shared<original_reentry0_afterif0_closure>(largs->k, *largs);
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(original_reentry0_cont1) {
    original_reentry0_cont1_closure *largs = (original_reentry0_cont1_closure*)(args.get());
    largs->total = (largs->total + largs->a);
    auto sp0c = std::make_shared<original_reentry0_afterif0_closure>(largs->k, *largs);
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
