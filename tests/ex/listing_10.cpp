#include "cilk_explicit.hh"
THREAD(original);
int main();
THREAD(original_cont0);
THREAD(main_cont0);

CLOSURE_DEF(original,
    int n;
);
CLOSURE_DEF(original_cont0,
    int left;
    int right;
);
CLOSURE_DEF(main_cont0,
    int res;
);
#include <cilk/cilk.h>
#include <stdio.h>




THREAD(original) {
    int left;
    int right;
    original_closure *largs = (original_closure*)(args.get());
    if ((largs->n <= 1)) {
        SEND_ARGUMENT(largs->k, 1);
    } else {
        original_cont0_closure SN_original_cont0c(largs->k);
        spawn_next<original_cont0_closure> SN_original_cont0(SN_original_cont0c);
        cont sp0k;
        SN_BIND(SN_original_cont0, &sp0k, left);
        original_closure sp0c(sp0k);
        sp0c.n = (largs->n - 1);
        spawn<original_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND(SN_original_cont0, &sp1k, right);
        original_closure sp1c(sp1k);
        sp1c.n = (largs->n - 2);
        spawn<original_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
int main() {
    int res;
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, res);
    original_closure sp0c(sp0k);
    sp0c.n = 5;
    spawn<original_closure> sp0(sp0c);

    // Original sync was here
    return 0;
}
THREAD(original_cont0) {
    int result;
    original_cont0_closure *largs = (original_cont0_closure*)(args.get());
    result = (largs->left + largs->right);
    SEND_ARGUMENT(largs->k, result);
    return;
}
THREAD(main_cont0) {
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    printf("Final result = %d\n",largs->res);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
