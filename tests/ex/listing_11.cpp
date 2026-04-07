#include "cilk_explicit.hh"
THREAD(f);
THREAD(g);
THREAD(original);
int main();
THREAD(original_exit0);
THREAD(original_reentry0);
THREAD(original_reentry0_exit1);
THREAD(original_reentry0_reentry1);
THREAD(original_exit2);
THREAD(original_reentry2);
THREAD(original_cont0);
THREAD(main_cont0);
THREAD(original_reentry0_cont0);
THREAD(original_reentry0_exit1_cont0);
THREAD(original_reentry0_exit1_cont1);
THREAD(original_reentry0_exit1_cont2);
THREAD(original_reentry0_reentry1_cont0);
THREAD(original_exit2_cont0);
THREAD(original_exit2_cont1);
THREAD(original_exit2_cont2);
THREAD(original_reentry2_cont0);

CLOSURE_DEF(f,
    int x;
);
CLOSURE_DEF(g,
    int x0;
);
CLOSURE_DEF(original,
    int n;
    int m;
);
CLOSURE_DEF(original_exit0,
    int total;
);
CLOSURE_DEF(original_reentry0,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_reentry0_exit1,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_reentry0_reentry1,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_exit2,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_reentry2,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_cont0,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(main_cont0,
    int ans;
);
CLOSURE_DEF(original_reentry0_cont0,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_reentry0_exit1_cont0,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
);
CLOSURE_DEF(original_reentry0_exit1_cont1,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_reentry0_exit1_cont2,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_reentry0_reentry1_cont0,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_exit2_cont0,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
);
CLOSURE_DEF(original_exit2_cont1,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_exit2_cont2,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
CLOSURE_DEF(original_reentry2_cont0,
    int n;
    int m;
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
);
#include <cilk/cilk.h>
#include <stdio.h>








THREAD(f) {
    f_closure *largs = (f_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->x + 1));
    return;
}
THREAD(g) {
    g_closure *largs = (g_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->x0 + 100));
    return;
}
THREAD(original) {
    int total;
    int i;
    int j;
    int inner_sum;
    int x1;
    int y;
    int z;
    original_closure *largs = (original_closure*)(args.get());
    total = 0;
    i = 0;
    total = (total + 7);
    if ((i < largs->n)) {
        j = 0;
        inner_sum = 0;
        total = (total + 10);
        if ((j < largs->m)) {
            inner_sum = (inner_sum + 1);
            original_cont0_closure SN_original_cont0c(largs->k);
            spawn_next<original_cont0_closure> SN_original_cont0(SN_original_cont0c);
            cont sp0k;
            SN_BIND(SN_original_cont0, &sp0k, x1);
            f_closure sp0c(sp0k);
            sp0c.x = j;
            spawn<f_closure> sp0(sp0c);

            inner_sum = (inner_sum + 2);
            ((original_cont0_closure*)SN_original_cont0.cls.get())->z = z;
            ((original_cont0_closure*)SN_original_cont0.cls.get())->inner_sum = inner_sum;
            ((original_cont0_closure*)SN_original_cont0.cls.get())->i = i;
            ((original_cont0_closure*)SN_original_cont0.cls.get())->y = y;
            ((original_cont0_closure*)SN_original_cont0.cls.get())->total = total;
            ((original_cont0_closure*)SN_original_cont0.cls.get())->j = j;
            ((original_cont0_closure*)SN_original_cont0.cls.get())->m = largs->m;
            ((original_cont0_closure*)SN_original_cont0.cls.get())->n = largs->n;
            // Original sync was here
        } else {
            auto sp1c = std::make_shared<original_exit2_closure>(largs->k);
            sp1c->n = largs->n;
            sp1c->m = largs->m;
            sp1c->total = total;
            sp1c->i = i;
            sp1c->j = j;
            sp1c->inner_sum = inner_sum;
            sp1c->x1 = x1;
            sp1c->y = y;
            sp1c->z = z;
            cilk_spawn taskSpawn(sp1c->getTask(), sp1c);
            return;
        }
    } else {
        auto sp2c = std::make_shared<original_exit0_closure>(largs->k);
        sp2c->total = total;
        cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
        return;
    }
    return;
}
int main() {
    int ans;
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, ans);
    original_closure sp0c(sp0k);
    sp0c.n = 3;
    sp0c.m = 2;
    spawn<original_closure> sp0(sp0c);

    // Original sync was here
    return 0;
}
THREAD(original_exit0) {
    original_exit0_closure *largs = (original_exit0_closure*)(args.get());
    largs->total = (largs->total + 60);
    SEND_ARGUMENT(largs->k, largs->total);
    return;
}
THREAD(original_reentry0) {
    original_reentry0_closure *largs = (original_reentry0_closure*)(args.get());
    if ((largs->i < largs->n)) {
        largs->j = 0;
        largs->inner_sum = 0;
        largs->total = (largs->total + 10);
        if ((largs->j < largs->m)) {
            largs->inner_sum = (largs->inner_sum + 1);
            original_reentry0_cont0_closure SN_original_reentry0_cont0c(largs->k);
            spawn_next<original_reentry0_cont0_closure> SN_original_reentry0_cont0(SN_original_reentry0_cont0c);
            cont sp0k;
            SN_BIND(SN_original_reentry0_cont0, &sp0k, x1);
            f_closure sp0c(sp0k);
            sp0c.x = largs->j;
            spawn<f_closure> sp0(sp0c);

            largs->inner_sum = (largs->inner_sum + 2);
            ((original_reentry0_cont0_closure*)SN_original_reentry0_cont0.cls.get())->y = largs->y;
            ((original_reentry0_cont0_closure*)SN_original_reentry0_cont0.cls.get())->inner_sum = largs->inner_sum;
            ((original_reentry0_cont0_closure*)SN_original_reentry0_cont0.cls.get())->j = largs->j;
            ((original_reentry0_cont0_closure*)SN_original_reentry0_cont0.cls.get())->z = largs->z;
            ((original_reentry0_cont0_closure*)SN_original_reentry0_cont0.cls.get())->i = largs->i;
            ((original_reentry0_cont0_closure*)SN_original_reentry0_cont0.cls.get())->total = largs->total;
            ((original_reentry0_cont0_closure*)SN_original_reentry0_cont0.cls.get())->m = largs->m;
            ((original_reentry0_cont0_closure*)SN_original_reentry0_cont0.cls.get())->n = largs->n;
            // Original sync was here
        } else {
            auto sp1c = std::make_shared<original_reentry0_exit1_closure>(largs->k);
            sp1c->n = largs->n;
            sp1c->m = largs->m;
            sp1c->total = largs->total;
            sp1c->i = largs->i;
            sp1c->j = largs->j;
            sp1c->inner_sum = largs->inner_sum;
            sp1c->x1 = largs->x1;
            sp1c->y = largs->y;
            sp1c->z = largs->z;
            cilk_spawn taskSpawn(sp1c->getTask(), sp1c);
            return;
        }
    } else {
        auto sp2c = std::make_shared<original_exit0_closure>(largs->k);
        sp2c->total = largs->total;
        cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
        return;
    }
    return;
}
THREAD(original_reentry0_exit1) {
    original_reentry0_exit1_closure *largs = (original_reentry0_exit1_closure*)(args.get());
    largs->total = (largs->total + largs->inner_sum);
    original_reentry0_exit1_cont0_closure SN_original_reentry0_exit1_cont0c(largs->k);
    spawn_next<original_reentry0_exit1_cont0_closure> SN_original_reentry0_exit1_cont0(SN_original_reentry0_exit1_cont0c);
    ((original_reentry0_exit1_cont0_closure*)SN_original_reentry0_exit1_cont0.cls.get())->j = largs->j;
    ((original_reentry0_exit1_cont0_closure*)SN_original_reentry0_exit1_cont0.cls.get())->total = largs->total;
    ((original_reentry0_exit1_cont0_closure*)SN_original_reentry0_exit1_cont0.cls.get())->x1 = largs->x1;
    ((original_reentry0_exit1_cont0_closure*)SN_original_reentry0_exit1_cont0.cls.get())->inner_sum = largs->inner_sum;
    ((original_reentry0_exit1_cont0_closure*)SN_original_reentry0_exit1_cont0.cls.get())->i = largs->i;
    ((original_reentry0_exit1_cont0_closure*)SN_original_reentry0_exit1_cont0.cls.get())->m = largs->m;
    ((original_reentry0_exit1_cont0_closure*)SN_original_reentry0_exit1_cont0.cls.get())->n = largs->n;
    // Original sync was here
    return;
}
THREAD(original_reentry0_reentry1) {
    original_reentry0_reentry1_closure *largs = (original_reentry0_reentry1_closure*)(args.get());
    if ((largs->j < largs->m)) {
        largs->inner_sum = (largs->inner_sum + 1);
        original_reentry0_reentry1_cont0_closure SN_original_reentry0_reentry1_cont0c(largs->k);
        spawn_next<original_reentry0_reentry1_cont0_closure> SN_original_reentry0_reentry1_cont0(SN_original_reentry0_reentry1_cont0c);
        cont sp0k;
        SN_BIND(SN_original_reentry0_reentry1_cont0, &sp0k, x1);
        f_closure sp0c(sp0k);
        sp0c.x = largs->j;
        spawn<f_closure> sp0(sp0c);

        largs->inner_sum = (largs->inner_sum + 2);
        ((original_reentry0_reentry1_cont0_closure*)SN_original_reentry0_reentry1_cont0.cls.get())->z = largs->z;
        ((original_reentry0_reentry1_cont0_closure*)SN_original_reentry0_reentry1_cont0.cls.get())->inner_sum = largs->inner_sum;
        ((original_reentry0_reentry1_cont0_closure*)SN_original_reentry0_reentry1_cont0.cls.get())->total = largs->total;
        ((original_reentry0_reentry1_cont0_closure*)SN_original_reentry0_reentry1_cont0.cls.get())->m = largs->m;
        ((original_reentry0_reentry1_cont0_closure*)SN_original_reentry0_reentry1_cont0.cls.get())->y = largs->y;
        ((original_reentry0_reentry1_cont0_closure*)SN_original_reentry0_reentry1_cont0.cls.get())->j = largs->j;
        ((original_reentry0_reentry1_cont0_closure*)SN_original_reentry0_reentry1_cont0.cls.get())->i = largs->i;
        ((original_reentry0_reentry1_cont0_closure*)SN_original_reentry0_reentry1_cont0.cls.get())->n = largs->n;
        // Original sync was here
    } else {
        auto sp1c = std::make_shared<original_reentry0_exit1_closure>(largs->k);
        sp1c->n = largs->n;
        sp1c->m = largs->m;
        sp1c->total = largs->total;
        sp1c->i = largs->i;
        sp1c->j = largs->j;
        sp1c->inner_sum = largs->inner_sum;
        sp1c->x1 = largs->x1;
        sp1c->y = largs->y;
        sp1c->z = largs->z;
        cilk_spawn taskSpawn(sp1c->getTask(), sp1c);
        return;
    }
    return;
}
THREAD(original_exit2) {
    original_exit2_closure *largs = (original_exit2_closure*)(args.get());
    largs->total = (largs->total + largs->inner_sum);
    original_exit2_cont0_closure SN_original_exit2_cont0c(largs->k);
    spawn_next<original_exit2_cont0_closure> SN_original_exit2_cont0(SN_original_exit2_cont0c);
    ((original_exit2_cont0_closure*)SN_original_exit2_cont0.cls.get())->x1 = largs->x1;
    ((original_exit2_cont0_closure*)SN_original_exit2_cont0.cls.get())->i = largs->i;
    ((original_exit2_cont0_closure*)SN_original_exit2_cont0.cls.get())->inner_sum = largs->inner_sum;
    ((original_exit2_cont0_closure*)SN_original_exit2_cont0.cls.get())->total = largs->total;
    ((original_exit2_cont0_closure*)SN_original_exit2_cont0.cls.get())->j = largs->j;
    ((original_exit2_cont0_closure*)SN_original_exit2_cont0.cls.get())->m = largs->m;
    ((original_exit2_cont0_closure*)SN_original_exit2_cont0.cls.get())->n = largs->n;
    // Original sync was here
    return;
}
THREAD(original_reentry2) {
    original_reentry2_closure *largs = (original_reentry2_closure*)(args.get());
    if ((largs->j < largs->m)) {
        largs->inner_sum = (largs->inner_sum + 1);
        original_reentry2_cont0_closure SN_original_reentry2_cont0c(largs->k);
        spawn_next<original_reentry2_cont0_closure> SN_original_reentry2_cont0(SN_original_reentry2_cont0c);
        cont sp0k;
        SN_BIND(SN_original_reentry2_cont0, &sp0k, x1);
        f_closure sp0c(sp0k);
        sp0c.x = largs->j;
        spawn<f_closure> sp0(sp0c);

        largs->inner_sum = (largs->inner_sum + 2);
        ((original_reentry2_cont0_closure*)SN_original_reentry2_cont0.cls.get())->z = largs->z;
        ((original_reentry2_cont0_closure*)SN_original_reentry2_cont0.cls.get())->inner_sum = largs->inner_sum;
        ((original_reentry2_cont0_closure*)SN_original_reentry2_cont0.cls.get())->i = largs->i;
        ((original_reentry2_cont0_closure*)SN_original_reentry2_cont0.cls.get())->j = largs->j;
        ((original_reentry2_cont0_closure*)SN_original_reentry2_cont0.cls.get())->total = largs->total;
        ((original_reentry2_cont0_closure*)SN_original_reentry2_cont0.cls.get())->m = largs->m;
        ((original_reentry2_cont0_closure*)SN_original_reentry2_cont0.cls.get())->y = largs->y;
        ((original_reentry2_cont0_closure*)SN_original_reentry2_cont0.cls.get())->n = largs->n;
        // Original sync was here
    } else {
        auto sp1c = std::make_shared<original_exit2_closure>(largs->k);
        sp1c->n = largs->n;
        sp1c->m = largs->m;
        sp1c->total = largs->total;
        sp1c->i = largs->i;
        sp1c->j = largs->j;
        sp1c->inner_sum = largs->inner_sum;
        sp1c->x1 = largs->x1;
        sp1c->y = largs->y;
        sp1c->z = largs->z;
        cilk_spawn taskSpawn(sp1c->getTask(), sp1c);
        return;
    }
    return;
}
THREAD(original_cont0) {
    original_cont0_closure *largs = (original_cont0_closure*)(args.get());
    largs->inner_sum = (largs->inner_sum + largs->x1);
    largs->j = (largs->j + 1);
    auto sp0c = std::make_shared<original_reentry2_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->m = largs->m;
    sp0c->total = largs->total;
    sp0c->i = largs->i;
    sp0c->j = largs->j;
    sp0c->inner_sum = largs->inner_sum;
    sp0c->x1 = largs->x1;
    sp0c->y = largs->y;
    sp0c->z = largs->z;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    printf("%d\n",largs->ans);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(original_reentry0_cont0) {
    original_reentry0_cont0_closure *largs = (original_reentry0_cont0_closure*)(args.get());
    largs->inner_sum = (largs->inner_sum + largs->x1);
    largs->j = (largs->j + 1);
    auto sp0c = std::make_shared<original_reentry0_reentry1_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->m = largs->m;
    sp0c->total = largs->total;
    sp0c->i = largs->i;
    sp0c->j = largs->j;
    sp0c->inner_sum = largs->inner_sum;
    sp0c->x1 = largs->x1;
    sp0c->y = largs->y;
    sp0c->z = largs->z;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(original_reentry0_exit1_cont0) {
    int y;
    int z;
    original_reentry0_exit1_cont0_closure *largs = (original_reentry0_exit1_cont0_closure*)(args.get());
    largs->total = (largs->total + 20);
    if (((largs->i % 2) == 0)) {
        largs->total = (largs->total + 30);
        original_reentry0_exit1_cont2_closure SN_original_reentry0_exit1_cont2c(largs->k);
        spawn_next<original_reentry0_exit1_cont2_closure> SN_original_reentry0_exit1_cont2(SN_original_reentry0_exit1_cont2c);
        cont sp0k;
        SN_BIND(SN_original_reentry0_exit1_cont2, &sp0k, y);
        f_closure sp0c(sp0k);
        sp0c.x = largs->i;
        spawn<f_closure> sp0(sp0c);

        ((original_reentry0_exit1_cont2_closure*)SN_original_reentry0_exit1_cont2.cls.get())->x1 = largs->x1;
        ((original_reentry0_exit1_cont2_closure*)SN_original_reentry0_exit1_cont2.cls.get())->j = largs->j;
        ((original_reentry0_exit1_cont2_closure*)SN_original_reentry0_exit1_cont2.cls.get())->inner_sum = largs->inner_sum;
        ((original_reentry0_exit1_cont2_closure*)SN_original_reentry0_exit1_cont2.cls.get())->i = largs->i;
        ((original_reentry0_exit1_cont2_closure*)SN_original_reentry0_exit1_cont2.cls.get())->total = largs->total;
        ((original_reentry0_exit1_cont2_closure*)SN_original_reentry0_exit1_cont2.cls.get())->m = largs->m;
        ((original_reentry0_exit1_cont2_closure*)SN_original_reentry0_exit1_cont2.cls.get())->n = largs->n;
        // Original sync was here
    } else {
        largs->total = (largs->total + 40);
        original_reentry0_exit1_cont1_closure SN_original_reentry0_exit1_cont1c(largs->k);
        spawn_next<original_reentry0_exit1_cont1_closure> SN_original_reentry0_exit1_cont1(SN_original_reentry0_exit1_cont1c);
        cont sp1k;
        SN_BIND(SN_original_reentry0_exit1_cont1, &sp1k, z);
        g_closure sp1c(sp1k);
        sp1c.x0 = largs->i;
        spawn<g_closure> sp1(sp1c);

        ((original_reentry0_exit1_cont1_closure*)SN_original_reentry0_exit1_cont1.cls.get())->x1 = largs->x1;
        ((original_reentry0_exit1_cont1_closure*)SN_original_reentry0_exit1_cont1.cls.get())->j = largs->j;
        ((original_reentry0_exit1_cont1_closure*)SN_original_reentry0_exit1_cont1.cls.get())->inner_sum = largs->inner_sum;
        ((original_reentry0_exit1_cont1_closure*)SN_original_reentry0_exit1_cont1.cls.get())->i = largs->i;
        ((original_reentry0_exit1_cont1_closure*)SN_original_reentry0_exit1_cont1.cls.get())->total = largs->total;
        ((original_reentry0_exit1_cont1_closure*)SN_original_reentry0_exit1_cont1.cls.get())->m = largs->m;
        ((original_reentry0_exit1_cont1_closure*)SN_original_reentry0_exit1_cont1.cls.get())->n = largs->n;
        // Original sync was here
    }
    return;
}
THREAD(original_reentry0_exit1_cont1) {
    original_reentry0_exit1_cont1_closure *largs = (original_reentry0_exit1_cont1_closure*)(args.get());
    largs->total = (largs->total + largs->z);
    largs->total = (largs->total + 50);
    largs->i = (largs->i + 1);
    auto sp0c = std::make_shared<original_reentry0_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->m = largs->m;
    sp0c->total = largs->total;
    sp0c->i = largs->i;
    sp0c->j = largs->j;
    sp0c->inner_sum = largs->inner_sum;
    sp0c->x1 = largs->x1;
    sp0c->y = largs->y;
    sp0c->z = largs->z;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(original_reentry0_exit1_cont2) {
    original_reentry0_exit1_cont2_closure *largs = (original_reentry0_exit1_cont2_closure*)(args.get());
    largs->total = (largs->total + largs->y);
    largs->total = (largs->total + 50);
    largs->i = (largs->i + 1);
    auto sp0c = std::make_shared<original_reentry0_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->m = largs->m;
    sp0c->total = largs->total;
    sp0c->i = largs->i;
    sp0c->j = largs->j;
    sp0c->inner_sum = largs->inner_sum;
    sp0c->x1 = largs->x1;
    sp0c->y = largs->y;
    sp0c->z = largs->z;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(original_reentry0_reentry1_cont0) {
    original_reentry0_reentry1_cont0_closure *largs = (original_reentry0_reentry1_cont0_closure*)(args.get());
    largs->inner_sum = (largs->inner_sum + largs->x1);
    largs->j = (largs->j + 1);
    auto sp0c = std::make_shared<original_reentry0_reentry1_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->m = largs->m;
    sp0c->total = largs->total;
    sp0c->i = largs->i;
    sp0c->j = largs->j;
    sp0c->inner_sum = largs->inner_sum;
    sp0c->x1 = largs->x1;
    sp0c->y = largs->y;
    sp0c->z = largs->z;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(original_exit2_cont0) {
    int y;
    int z;
    original_exit2_cont0_closure *largs = (original_exit2_cont0_closure*)(args.get());
    largs->total = (largs->total + 20);
    if (((largs->i % 2) == 0)) {
        largs->total = (largs->total + 30);
        original_exit2_cont2_closure SN_original_exit2_cont2c(largs->k);
        spawn_next<original_exit2_cont2_closure> SN_original_exit2_cont2(SN_original_exit2_cont2c);
        cont sp0k;
        SN_BIND(SN_original_exit2_cont2, &sp0k, y);
        f_closure sp0c(sp0k);
        sp0c.x = largs->i;
        spawn<f_closure> sp0(sp0c);

        ((original_exit2_cont2_closure*)SN_original_exit2_cont2.cls.get())->n = largs->n;
        ((original_exit2_cont2_closure*)SN_original_exit2_cont2.cls.get())->i = largs->i;
        ((original_exit2_cont2_closure*)SN_original_exit2_cont2.cls.get())->total = largs->total;
        ((original_exit2_cont2_closure*)SN_original_exit2_cont2.cls.get())->x1 = largs->x1;
        ((original_exit2_cont2_closure*)SN_original_exit2_cont2.cls.get())->inner_sum = largs->inner_sum;
        ((original_exit2_cont2_closure*)SN_original_exit2_cont2.cls.get())->m = largs->m;
        ((original_exit2_cont2_closure*)SN_original_exit2_cont2.cls.get())->j = largs->j;
        // Original sync was here
    } else {
        largs->total = (largs->total + 40);
        original_exit2_cont1_closure SN_original_exit2_cont1c(largs->k);
        spawn_next<original_exit2_cont1_closure> SN_original_exit2_cont1(SN_original_exit2_cont1c);
        cont sp1k;
        SN_BIND(SN_original_exit2_cont1, &sp1k, z);
        g_closure sp1c(sp1k);
        sp1c.x0 = largs->i;
        spawn<g_closure> sp1(sp1c);

        ((original_exit2_cont1_closure*)SN_original_exit2_cont1.cls.get())->n = largs->n;
        ((original_exit2_cont1_closure*)SN_original_exit2_cont1.cls.get())->i = largs->i;
        ((original_exit2_cont1_closure*)SN_original_exit2_cont1.cls.get())->total = largs->total;
        ((original_exit2_cont1_closure*)SN_original_exit2_cont1.cls.get())->x1 = largs->x1;
        ((original_exit2_cont1_closure*)SN_original_exit2_cont1.cls.get())->inner_sum = largs->inner_sum;
        ((original_exit2_cont1_closure*)SN_original_exit2_cont1.cls.get())->m = largs->m;
        ((original_exit2_cont1_closure*)SN_original_exit2_cont1.cls.get())->j = largs->j;
        // Original sync was here
    }
    return;
}
THREAD(original_exit2_cont1) {
    original_exit2_cont1_closure *largs = (original_exit2_cont1_closure*)(args.get());
    largs->total = (largs->total + largs->z);
    largs->total = (largs->total + 50);
    largs->i = (largs->i + 1);
    auto sp0c = std::make_shared<original_reentry0_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->m = largs->m;
    sp0c->total = largs->total;
    sp0c->i = largs->i;
    sp0c->j = largs->j;
    sp0c->inner_sum = largs->inner_sum;
    sp0c->x1 = largs->x1;
    sp0c->y = largs->y;
    sp0c->z = largs->z;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(original_exit2_cont2) {
    original_exit2_cont2_closure *largs = (original_exit2_cont2_closure*)(args.get());
    largs->total = (largs->total + largs->y);
    largs->total = (largs->total + 50);
    largs->i = (largs->i + 1);
    auto sp0c = std::make_shared<original_reentry0_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->m = largs->m;
    sp0c->total = largs->total;
    sp0c->i = largs->i;
    sp0c->j = largs->j;
    sp0c->inner_sum = largs->inner_sum;
    sp0c->x1 = largs->x1;
    sp0c->y = largs->y;
    sp0c->z = largs->z;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(original_reentry2_cont0) {
    original_reentry2_cont0_closure *largs = (original_reentry2_cont0_closure*)(args.get());
    largs->inner_sum = (largs->inner_sum + largs->x1);
    largs->j = (largs->j + 1);
    auto sp0c = std::make_shared<original_reentry2_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->m = largs->m;
    sp0c->total = largs->total;
    sp0c->i = largs->i;
    sp0c->j = largs->j;
    sp0c->inner_sum = largs->inner_sum;
    sp0c->x1 = largs->x1;
    sp0c->y = largs->y;
    sp0c->z = largs->z;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
