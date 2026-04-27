#include "cilk_explicit.hh"
THREAD(worker);
THREAD(fun);
int main();
THREAD(fun_exit0);
THREAD(fun_reentry0);
THREAD(fun_reentry0_exit1);
THREAD(fun_reentry0_reentry1);
THREAD(fun_exit0_exit2);
THREAD(fun_exit0_reentry2);
THREAD(fun_exit3);
THREAD(fun_reentry3);
THREAD(fun_cont0);
THREAD(main_cont0);
THREAD(fun_exit0_cont0);
THREAD(fun_reentry0_cont0);
THREAD(fun_reentry0_reentry1_cont0);
THREAD(fun_exit0_reentry2_cont0);
THREAD(fun_reentry3_cont0);

CLOSURE_DEF(worker,
    unsigned long long n;
);
CLOSURE_DEF(fun,
    long n0;
);
CLOSURE_DEF(fun_exit0,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_reentry0,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_reentry0_exit1,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_reentry0_reentry1,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_exit0_exit2,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_exit0_reentry2,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_exit3,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_reentry3,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_cont0,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(main_cont0,
    unsigned long long n1;
);
CLOSURE_DEF(fun_exit0_cont0,
    long n0;
    unsigned long long w;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_reentry0_cont0,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_reentry0_reentry1_cont0,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_exit0_reentry2_cont0,
    long n0;
    unsigned long long w;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
CLOSURE_DEF(fun_reentry3_cont0,
    long n0;
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
);
#include <cilk/cilk.h>
#include <stdio.h>






THREAD(worker) {
    worker_closure *largs = (worker_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->n * largs->n));
    return;
}
THREAD(fun) {
    unsigned long long w;
    unsigned long long result;
    unsigned long long sum;
    unsigned long long val_0;
    unsigned long long val_1;
    unsigned long long x;
    int v;
    unsigned long long z;
    unsigned long long y;
    fun_closure *largs = (fun_closure*)(args.get());
    w = 14;
    result = 2;
    if ((w > 0)) {
        if ((w > 0)) {
            if ((largs->n0 > 0)) {
                sum = (w + largs->n0);
                fun_cont0_closure SN_fun_cont0c(largs->k);
                spawn_next<fun_cont0_closure> SN_fun_cont0(SN_fun_cont0c);
                cont sp0k;
                SN_BIND(SN_fun_cont0, &sp0k, val_0);
                worker_closure sp0c(sp0k);
                sp0c.n = largs->n0;
                spawn<worker_closure> sp0(sp0c);

                cont sp1k;
                SN_BIND(SN_fun_cont0, &sp1k, val_1);
                worker_closure sp1c(sp1k);
                sp1c.n = sum;
                spawn<worker_closure> sp1(sp1c);

                ((fun_cont0_closure*)SN_fun_cont0.cls.get())->y = y;
                ((fun_cont0_closure*)SN_fun_cont0.cls.get())->z = z;
                ((fun_cont0_closure*)SN_fun_cont0.cls.get())->v = v;
                ((fun_cont0_closure*)SN_fun_cont0.cls.get())->result = result;
                ((fun_cont0_closure*)SN_fun_cont0.cls.get())->x = x;
                ((fun_cont0_closure*)SN_fun_cont0.cls.get())->w = w;
                ((fun_cont0_closure*)SN_fun_cont0.cls.get())->sum = sum;
                ((fun_cont0_closure*)SN_fun_cont0.cls.get())->n0 = largs->n0;
                // Original sync was here
            } else {
                auto sp2c = std::make_shared<fun_exit3_closure>(largs->k);
                sp2c->n0 = largs->n0;
                sp2c->w = w;
                sp2c->result = result;
                sp2c->sum = sum;
                sp2c->val_0 = val_0;
                sp2c->val_1 = val_1;
                sp2c->x = x;
                sp2c->v = v;
                sp2c->z = z;
                sp2c->y = y;
                cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
                return;
            }
        } else {
            auto sp3c = std::make_shared<fun_exit0_closure>(largs->k);
            sp3c->n0 = largs->n0;
            sp3c->w = w;
            sp3c->result = result;
            sp3c->sum = sum;
            sp3c->val_0 = val_0;
            sp3c->val_1 = val_1;
            sp3c->x = x;
            sp3c->v = v;
            sp3c->z = z;
            sp3c->y = y;
            cilk_spawn taskSpawn(sp3c->getTask(), sp3c);
            return;
        }
    } else {
        w = 0;
        auto sp4c = std::make_shared<fun_exit0_closure>(largs->k);
        sp4c->n0 = largs->n0;
        sp4c->w = w;
        sp4c->result = result;
        sp4c->sum = sum;
        sp4c->val_0 = val_0;
        sp4c->val_1 = val_1;
        sp4c->x = x;
        sp4c->v = v;
        sp4c->z = z;
        sp4c->y = y;
        cilk_spawn taskSpawn(sp4c->getTask(), sp4c);
        return;
    }
    return;
}
int main() {
    unsigned long long n1;
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, n1);
    fun_closure sp0c(sp0k);
    sp0c.n0 = 10;
    spawn<fun_closure> sp0(sp0c);

    // Original sync was here
    return 0;
}
THREAD(fun_exit0) {
    fun_exit0_closure *largs = (fun_exit0_closure*)(args.get());
    largs->x = largs->result;
    if ((largs->x > 3)) {
        largs->v = 0;
        if ((largs->v < 3)) {
            fun_exit0_cont0_closure SN_fun_exit0_cont0c(largs->k);
            spawn_next<fun_exit0_cont0_closure> SN_fun_exit0_cont0(SN_fun_exit0_cont0c);
            cont sp0k;
            SN_BIND(SN_fun_exit0_cont0, &sp0k, z);
            worker_closure sp0c(sp0k);
            sp0c.n = largs->x;
            spawn<worker_closure> sp0(sp0c);

            largs->y = 3;
            cont sp1k;
            SN_BIND(SN_fun_exit0_cont0, &sp1k, y);
            worker_closure sp1c(sp1k);
            sp1c.n = largs->y;
            spawn<worker_closure> sp1(sp1c);

            ((fun_exit0_cont0_closure*)SN_fun_exit0_cont0.cls.get())->v = largs->v;
            ((fun_exit0_cont0_closure*)SN_fun_exit0_cont0.cls.get())->x = largs->x;
            ((fun_exit0_cont0_closure*)SN_fun_exit0_cont0.cls.get())->sum = largs->sum;
            ((fun_exit0_cont0_closure*)SN_fun_exit0_cont0.cls.get())->val_0 = largs->val_0;
            ((fun_exit0_cont0_closure*)SN_fun_exit0_cont0.cls.get())->val_1 = largs->val_1;
            ((fun_exit0_cont0_closure*)SN_fun_exit0_cont0.cls.get())->w = largs->w;
            ((fun_exit0_cont0_closure*)SN_fun_exit0_cont0.cls.get())->n0 = largs->n0;
            // Original sync was here
        } else {
            auto sp2c = std::make_shared<fun_exit0_exit2_closure>(largs->k);
            sp2c->n0 = largs->n0;
            sp2c->w = largs->w;
            sp2c->result = largs->result;
            sp2c->sum = largs->sum;
            sp2c->val_0 = largs->val_0;
            sp2c->val_1 = largs->val_1;
            sp2c->x = largs->x;
            sp2c->v = largs->v;
            sp2c->z = largs->z;
            sp2c->y = largs->y;
            cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
            return;
        }
    } else {
        largs->w = 0;
        auto sp3c = std::make_shared<fun_exit0_exit2_closure>(largs->k);
        sp3c->n0 = largs->n0;
        sp3c->w = largs->w;
        sp3c->result = largs->result;
        sp3c->sum = largs->sum;
        sp3c->val_0 = largs->val_0;
        sp3c->val_1 = largs->val_1;
        sp3c->x = largs->x;
        sp3c->v = largs->v;
        sp3c->z = largs->z;
        sp3c->y = largs->y;
        cilk_spawn taskSpawn(sp3c->getTask(), sp3c);
        return;
    }
    return;
}
THREAD(fun_reentry0) {
    fun_reentry0_closure *largs = (fun_reentry0_closure*)(args.get());
    if ((largs->w > 0)) {
        if ((largs->n0 > 0)) {
            largs->sum = (largs->w + largs->n0);
            fun_reentry0_cont0_closure SN_fun_reentry0_cont0c(largs->k);
            spawn_next<fun_reentry0_cont0_closure> SN_fun_reentry0_cont0(SN_fun_reentry0_cont0c);
            cont sp0k;
            SN_BIND(SN_fun_reentry0_cont0, &sp0k, val_0);
            worker_closure sp0c(sp0k);
            sp0c.n = largs->n0;
            spawn<worker_closure> sp0(sp0c);

            cont sp1k;
            SN_BIND(SN_fun_reentry0_cont0, &sp1k, val_1);
            worker_closure sp1c(sp1k);
            sp1c.n = largs->sum;
            spawn<worker_closure> sp1(sp1c);

            ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->v = largs->v;
            ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->x = largs->x;
            ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->y = largs->y;
            ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->z = largs->z;
            ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->sum = largs->sum;
            ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->result = largs->result;
            ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->w = largs->w;
            ((fun_reentry0_cont0_closure*)SN_fun_reentry0_cont0.cls.get())->n0 = largs->n0;
            // Original sync was here
        } else {
            auto sp2c = std::make_shared<fun_reentry0_exit1_closure>(largs->k);
            sp2c->n0 = largs->n0;
            sp2c->w = largs->w;
            sp2c->result = largs->result;
            sp2c->sum = largs->sum;
            sp2c->val_0 = largs->val_0;
            sp2c->val_1 = largs->val_1;
            sp2c->x = largs->x;
            sp2c->v = largs->v;
            sp2c->z = largs->z;
            sp2c->y = largs->y;
            cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
            return;
        }
    } else {
        auto sp3c = std::make_shared<fun_exit0_closure>(largs->k);
        sp3c->n0 = largs->n0;
        sp3c->w = largs->w;
        sp3c->result = largs->result;
        sp3c->sum = largs->sum;
        sp3c->val_0 = largs->val_0;
        sp3c->val_1 = largs->val_1;
        sp3c->x = largs->x;
        sp3c->v = largs->v;
        sp3c->z = largs->z;
        sp3c->y = largs->y;
        cilk_spawn taskSpawn(sp3c->getTask(), sp3c);
        return;
    }
    return;
}
THREAD(fun_reentry0_exit1) {
    fun_reentry0_exit1_closure *largs = (fun_reentry0_exit1_closure*)(args.get());
    largs->w = (largs->w - 1);
    auto sp0c = std::make_shared<fun_reentry0_closure>(largs->k);
    sp0c->n0 = largs->n0;
    sp0c->w = largs->w;
    sp0c->result = largs->result;
    sp0c->sum = largs->sum;
    sp0c->val_0 = largs->val_0;
    sp0c->val_1 = largs->val_1;
    sp0c->x = largs->x;
    sp0c->v = largs->v;
    sp0c->z = largs->z;
    sp0c->y = largs->y;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fun_reentry0_reentry1) {
    fun_reentry0_reentry1_closure *largs = (fun_reentry0_reentry1_closure*)(args.get());
    if ((largs->n0 > 0)) {
        largs->sum = (largs->w + largs->n0);
        fun_reentry0_reentry1_cont0_closure SN_fun_reentry0_reentry1_cont0c(largs->k);
        spawn_next<fun_reentry0_reentry1_cont0_closure> SN_fun_reentry0_reentry1_cont0(SN_fun_reentry0_reentry1_cont0c);
        cont sp0k;
        SN_BIND(SN_fun_reentry0_reentry1_cont0, &sp0k, val_0);
        worker_closure sp0c(sp0k);
        sp0c.n = largs->n0;
        spawn<worker_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND(SN_fun_reentry0_reentry1_cont0, &sp1k, val_1);
        worker_closure sp1c(sp1k);
        sp1c.n = largs->sum;
        spawn<worker_closure> sp1(sp1c);

        ((fun_reentry0_reentry1_cont0_closure*)SN_fun_reentry0_reentry1_cont0.cls.get())->z = largs->z;
        ((fun_reentry0_reentry1_cont0_closure*)SN_fun_reentry0_reentry1_cont0.cls.get())->v = largs->v;
        ((fun_reentry0_reentry1_cont0_closure*)SN_fun_reentry0_reentry1_cont0.cls.get())->x = largs->x;
        ((fun_reentry0_reentry1_cont0_closure*)SN_fun_reentry0_reentry1_cont0.cls.get())->sum = largs->sum;
        ((fun_reentry0_reentry1_cont0_closure*)SN_fun_reentry0_reentry1_cont0.cls.get())->result = largs->result;
        ((fun_reentry0_reentry1_cont0_closure*)SN_fun_reentry0_reentry1_cont0.cls.get())->y = largs->y;
        ((fun_reentry0_reentry1_cont0_closure*)SN_fun_reentry0_reentry1_cont0.cls.get())->w = largs->w;
        ((fun_reentry0_reentry1_cont0_closure*)SN_fun_reentry0_reentry1_cont0.cls.get())->n0 = largs->n0;
        // Original sync was here
    } else {
        auto sp2c = std::make_shared<fun_reentry0_exit1_closure>(largs->k);
        sp2c->n0 = largs->n0;
        sp2c->w = largs->w;
        sp2c->result = largs->result;
        sp2c->sum = largs->sum;
        sp2c->val_0 = largs->val_0;
        sp2c->val_1 = largs->val_1;
        sp2c->x = largs->x;
        sp2c->v = largs->v;
        sp2c->z = largs->z;
        sp2c->y = largs->y;
        cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
        return;
    }
    return;
}
THREAD(fun_exit0_exit2) {
    fun_exit0_exit2_closure *largs = (fun_exit0_exit2_closure*)(args.get());
    SEND_ARGUMENT(largs->k, largs->result);
    return;
}
THREAD(fun_exit0_reentry2) {
    fun_exit0_reentry2_closure *largs = (fun_exit0_reentry2_closure*)(args.get());
    if ((largs->v < 3)) {
        fun_exit0_reentry2_cont0_closure SN_fun_exit0_reentry2_cont0c(largs->k);
        spawn_next<fun_exit0_reentry2_cont0_closure> SN_fun_exit0_reentry2_cont0(SN_fun_exit0_reentry2_cont0c);
        cont sp0k;
        SN_BIND(SN_fun_exit0_reentry2_cont0, &sp0k, z);
        worker_closure sp0c(sp0k);
        sp0c.n = largs->x;
        spawn<worker_closure> sp0(sp0c);

        largs->y = 3;
        cont sp1k;
        SN_BIND(SN_fun_exit0_reentry2_cont0, &sp1k, y);
        worker_closure sp1c(sp1k);
        sp1c.n = largs->y;
        spawn<worker_closure> sp1(sp1c);

        ((fun_exit0_reentry2_cont0_closure*)SN_fun_exit0_reentry2_cont0.cls.get())->v = largs->v;
        ((fun_exit0_reentry2_cont0_closure*)SN_fun_exit0_reentry2_cont0.cls.get())->x = largs->x;
        ((fun_exit0_reentry2_cont0_closure*)SN_fun_exit0_reentry2_cont0.cls.get())->sum = largs->sum;
        ((fun_exit0_reentry2_cont0_closure*)SN_fun_exit0_reentry2_cont0.cls.get())->val_0 = largs->val_0;
        ((fun_exit0_reentry2_cont0_closure*)SN_fun_exit0_reentry2_cont0.cls.get())->val_1 = largs->val_1;
        ((fun_exit0_reentry2_cont0_closure*)SN_fun_exit0_reentry2_cont0.cls.get())->w = largs->w;
        ((fun_exit0_reentry2_cont0_closure*)SN_fun_exit0_reentry2_cont0.cls.get())->n0 = largs->n0;
        // Original sync was here
    } else {
        auto sp2c = std::make_shared<fun_exit0_exit2_closure>(largs->k);
        sp2c->n0 = largs->n0;
        sp2c->w = largs->w;
        sp2c->result = largs->result;
        sp2c->sum = largs->sum;
        sp2c->val_0 = largs->val_0;
        sp2c->val_1 = largs->val_1;
        sp2c->x = largs->x;
        sp2c->v = largs->v;
        sp2c->z = largs->z;
        sp2c->y = largs->y;
        cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
        return;
    }
    return;
}
THREAD(fun_exit3) {
    fun_exit3_closure *largs = (fun_exit3_closure*)(args.get());
    largs->w = (largs->w - 1);
    auto sp0c = std::make_shared<fun_reentry0_closure>(largs->k);
    sp0c->n0 = largs->n0;
    sp0c->w = largs->w;
    sp0c->result = largs->result;
    sp0c->sum = largs->sum;
    sp0c->val_0 = largs->val_0;
    sp0c->val_1 = largs->val_1;
    sp0c->x = largs->x;
    sp0c->v = largs->v;
    sp0c->z = largs->z;
    sp0c->y = largs->y;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fun_reentry3) {
    fun_reentry3_closure *largs = (fun_reentry3_closure*)(args.get());
    if ((largs->n0 > 0)) {
        largs->sum = (largs->w + largs->n0);
        fun_reentry3_cont0_closure SN_fun_reentry3_cont0c(largs->k);
        spawn_next<fun_reentry3_cont0_closure> SN_fun_reentry3_cont0(SN_fun_reentry3_cont0c);
        cont sp0k;
        SN_BIND(SN_fun_reentry3_cont0, &sp0k, val_0);
        worker_closure sp0c(sp0k);
        sp0c.n = largs->n0;
        spawn<worker_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND(SN_fun_reentry3_cont0, &sp1k, val_1);
        worker_closure sp1c(sp1k);
        sp1c.n = largs->sum;
        spawn<worker_closure> sp1(sp1c);

        ((fun_reentry3_cont0_closure*)SN_fun_reentry3_cont0.cls.get())->z = largs->z;
        ((fun_reentry3_cont0_closure*)SN_fun_reentry3_cont0.cls.get())->x = largs->x;
        ((fun_reentry3_cont0_closure*)SN_fun_reentry3_cont0.cls.get())->v = largs->v;
        ((fun_reentry3_cont0_closure*)SN_fun_reentry3_cont0.cls.get())->sum = largs->sum;
        ((fun_reentry3_cont0_closure*)SN_fun_reentry3_cont0.cls.get())->result = largs->result;
        ((fun_reentry3_cont0_closure*)SN_fun_reentry3_cont0.cls.get())->w = largs->w;
        ((fun_reentry3_cont0_closure*)SN_fun_reentry3_cont0.cls.get())->y = largs->y;
        ((fun_reentry3_cont0_closure*)SN_fun_reentry3_cont0.cls.get())->n0 = largs->n0;
        // Original sync was here
    } else {
        auto sp2c = std::make_shared<fun_exit3_closure>(largs->k);
        sp2c->n0 = largs->n0;
        sp2c->w = largs->w;
        sp2c->result = largs->result;
        sp2c->sum = largs->sum;
        sp2c->val_0 = largs->val_0;
        sp2c->val_1 = largs->val_1;
        sp2c->x = largs->x;
        sp2c->v = largs->v;
        sp2c->z = largs->z;
        sp2c->y = largs->y;
        cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
        return;
    }
    return;
}
THREAD(fun_cont0) {
    fun_cont0_closure *largs = (fun_cont0_closure*)(args.get());
    largs->result = ((largs->result + largs->val_0) + largs->val_1);
    largs->n0 = (largs->n0 - 1);
    auto sp0c = std::make_shared<fun_reentry3_closure>(largs->k);
    sp0c->n0 = largs->n0;
    sp0c->w = largs->w;
    sp0c->result = largs->result;
    sp0c->sum = largs->sum;
    sp0c->val_0 = largs->val_0;
    sp0c->val_1 = largs->val_1;
    sp0c->x = largs->x;
    sp0c->v = largs->v;
    sp0c->z = largs->z;
    sp0c->y = largs->y;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    printf("fun = %llu\n",largs->n1);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fun_exit0_cont0) {
    unsigned long long result;
    fun_exit0_cont0_closure *largs = (fun_exit0_cont0_closure*)(args.get());
    result = ((largs->x + largs->y) + largs->z);
    largs->v = (largs->v + 1);
    auto sp0c = std::make_shared<fun_exit0_reentry2_closure>(largs->k);
    sp0c->n0 = largs->n0;
    sp0c->w = largs->w;
    sp0c->result = result;
    sp0c->sum = largs->sum;
    sp0c->val_0 = largs->val_0;
    sp0c->val_1 = largs->val_1;
    sp0c->x = largs->x;
    sp0c->v = largs->v;
    sp0c->z = largs->z;
    sp0c->y = largs->y;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fun_reentry0_cont0) {
    fun_reentry0_cont0_closure *largs = (fun_reentry0_cont0_closure*)(args.get());
    largs->result = ((largs->result + largs->val_0) + largs->val_1);
    largs->n0 = (largs->n0 - 1);
    auto sp0c = std::make_shared<fun_reentry0_reentry1_closure>(largs->k);
    sp0c->n0 = largs->n0;
    sp0c->w = largs->w;
    sp0c->result = largs->result;
    sp0c->sum = largs->sum;
    sp0c->val_0 = largs->val_0;
    sp0c->val_1 = largs->val_1;
    sp0c->x = largs->x;
    sp0c->v = largs->v;
    sp0c->z = largs->z;
    sp0c->y = largs->y;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fun_reentry0_reentry1_cont0) {
    fun_reentry0_reentry1_cont0_closure *largs = (fun_reentry0_reentry1_cont0_closure*)(args.get());
    largs->result = ((largs->result + largs->val_0) + largs->val_1);
    largs->n0 = (largs->n0 - 1);
    auto sp0c = std::make_shared<fun_reentry0_reentry1_closure>(largs->k);
    sp0c->n0 = largs->n0;
    sp0c->w = largs->w;
    sp0c->result = largs->result;
    sp0c->sum = largs->sum;
    sp0c->val_0 = largs->val_0;
    sp0c->val_1 = largs->val_1;
    sp0c->x = largs->x;
    sp0c->v = largs->v;
    sp0c->z = largs->z;
    sp0c->y = largs->y;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fun_exit0_reentry2_cont0) {
    unsigned long long result;
    fun_exit0_reentry2_cont0_closure *largs = (fun_exit0_reentry2_cont0_closure*)(args.get());
    result = ((largs->x + largs->y) + largs->z);
    largs->v = (largs->v + 1);
    auto sp0c = std::make_shared<fun_exit0_reentry2_closure>(largs->k);
    sp0c->n0 = largs->n0;
    sp0c->w = largs->w;
    sp0c->result = result;
    sp0c->sum = largs->sum;
    sp0c->val_0 = largs->val_0;
    sp0c->val_1 = largs->val_1;
    sp0c->x = largs->x;
    sp0c->v = largs->v;
    sp0c->z = largs->z;
    sp0c->y = largs->y;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fun_reentry3_cont0) {
    fun_reentry3_cont0_closure *largs = (fun_reentry3_cont0_closure*)(args.get());
    largs->result = ((largs->result + largs->val_0) + largs->val_1);
    largs->n0 = (largs->n0 - 1);
    auto sp0c = std::make_shared<fun_reentry3_closure>(largs->k);
    sp0c->n0 = largs->n0;
    sp0c->w = largs->w;
    sp0c->result = largs->result;
    sp0c->sum = largs->sum;
    sp0c->val_0 = largs->val_0;
    sp0c->val_1 = largs->val_1;
    sp0c->x = largs->x;
    sp0c->v = largs->v;
    sp0c->z = largs->z;
    sp0c->y = largs->y;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
