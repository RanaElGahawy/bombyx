#include "cilk_explicit.hh"
#include <cilk/cilk.h>
#include <stdio.h>

struct Vec2 {
  float x;
  float y;
};

THREAD(magnitude);
THREAD(compute);
int main();
THREAD(compute_cont0);
THREAD(main_cont0);

CLOSURE_DEF(magnitude,
    Vec2 *v;
);
CLOSURE_DEF(compute,
    Vec2 *a;
    Vec2 *b;
);
CLOSURE_DEF(compute_cont0,
    float m1;
    float m2;
);
CLOSURE_DEF(main_cont0,
    float result;
);





THREAD(magnitude) {
    magnitude_closure *largs = (magnitude_closure*)(args.get());
    SEND_ARGUMENT(largs->k, ((largs->v->x * largs->v->x) + (largs->v->y * largs->v->y)));
    return;
}
THREAD(compute) {
    float m1;
    float m2;
    compute_closure *largs = (compute_closure*)(args.get());
    compute_cont0_closure SN_compute_cont0c(largs->k);
    spawn_next<compute_cont0_closure> SN_compute_cont0(SN_compute_cont0c);
    cont sp0k;
    SN_BIND(SN_compute_cont0, &sp0k, m1);
    magnitude_closure sp0c(sp0k);
    sp0c.v = largs->a;
    spawn<magnitude_closure> sp0(sp0c);

    cont sp1k;
    SN_BIND(SN_compute_cont0, &sp1k, m2);
    magnitude_closure sp1c(sp1k);
    sp1c.v = largs->b;
    spawn<magnitude_closure> sp1(sp1c);

    // Original sync was here
    return;
}
int main() {
    Vec2 a0;
    Vec2 b0;
    float result;
    a0.x = 3.F;
    a0.y = 4.F;
    b0.x = 1.F;
    b0.y = 2.F;
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
    SEND_ARGUMENT(largs->k, (largs->m1 + largs->m2));
    return;
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    printf("result = %f\n",largs->result);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
