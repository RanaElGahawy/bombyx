#include "cilk_explicit.hh"
unsigned long long todval(struct timeval * tp);
int ok(int n, char * a);
THREAD(nqueens);
int main(int argc, char ** argv);
THREAD(nqueens_cont0);
THREAD(main_cont0);

CLOSURE_DEF(nqueens,
    int n0;
    int j0;
    char * a0;
);
CLOSURE_DEF(nqueens_cont0,
    int n0;
    int * count;
    int solNum;
);
CLOSURE_DEF(main_cont0,
    int res;
    struct timeval t1;
    struct timeval t2;
);
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <cilk/cilk.h>

#if CILKSAN
#include "cilksan.h"
#endif

#ifdef SERIAL
#include <cilk/cilk_stub.h>
#endif

pthread_attr_t *mutex;



// int * count;

/*
 * nqueen  4 = 2
 * nqueen  5 = 10
 * nqueen  6 = 4
 * nqueen  7 = 40
 * nqueen  8 = 92
 * nqueen  9 = 352
 * nqueen 10 = 724
 * nqueen 11 = 2680
 * nqueen 12 = 14200
 * nqueen 13 = 73712
 * nqueen 14 = 365596
 * nqueen 15 = 2279184
 */

/*
 * <a> contains array of <n> queen positions.  Returns 1
 * if none of the queens conflict, and returns 0 otherwise.
 */





unsigned long long todval(struct timeval * tp) {
    return (((tp->tv_sec * 1000) * 1000) + tp->tv_usec);
}
int ok(int n, char * a) {
    int i;
    int j;
    char p;
    char q;
    for (i = 0;(i < n);i = (i + 1)) {
        p = a[i];
        for (j = (i + 1);(j < n);j = (j + 1)) {
            q = a[j];
            if ((((q == p) || (q == (p - (j - i)))) || (q == (p + (j - i))))) {
                return 0;
            } else {
            }
        }
    }
    return 1;
}
THREAD(nqueens) {
    char * b;
    int i0;
    int * count;
    int solNum;
    char * b_alloc;
    nqueens_closure *largs = (nqueens_closure*)(args.get());
    solNum = 0;
    if ((largs->n0 == largs->j0)) {
        SEND_ARGUMENT(largs->k, 1);
    } else {
        count = ((int *) __builtin_alloca((largs->n0 * sizeof(int))));
        ((void) __builtin___memset_chk(count,0,(largs->n0 * sizeof(int)),__builtin_object_size(count,0)));
        nqueens_cont0_closure SN_nqueens_cont0c(largs->k);
        spawn_next<nqueens_cont0_closure> SN_nqueens_cont0(SN_nqueens_cont0c);
        for (i0 = 0;(i0 < largs->n0);i0 = (i0 + 1)) {
            b_alloc = ((char *) __builtin_alloca((((largs->j0 + 1) * sizeof(char)) + 31)));
            b = ((char *) ((((uintptr_t) b_alloc) + 31) & (~31)));
            __builtin___memcpy_chk(b,largs->a0,(largs->j0 * sizeof(char)),__builtin_object_size(b,0));
            b[largs->j0] = i0;
            if (ok((largs->j0 + 1),b)) {
                cont sp0k;
                SN_BIND_EXT(SN_nqueens_cont0, &sp0k, &(count[i0]));
                nqueens_closure sp0c(sp0k);
                sp0c.n0 = largs->n0;
                sp0c.j0 = (largs->j0 + 1);
                sp0c.a0 = b;
                spawn<nqueens_closure> sp0(sp0c);

            }
        }
        ((nqueens_cont0_closure*)SN_nqueens_cont0.cls.get())->solNum = solNum;
        ((nqueens_cont0_closure*)SN_nqueens_cont0.cls.get())->count = count;
        ((nqueens_cont0_closure*)SN_nqueens_cont0.cls.get())->n0 = largs->n0;
        // Original sync was here
    }
    return;
}
int main(int argc, char ** argv) {
    int n1;
    char * a1;
    int res;
    struct timeval t1;
    n1 = 13;
    a1 = ((char *) __builtin_alloca((n1 * sizeof(char))));
    res = 0;
    gettimeofday(&(t1),0);
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, res);
    nqueens_closure sp0c(sp0k);
    sp0c.n0 = n1;
    sp0c.j0 = 0;
    sp0c.a0 = a1;
    spawn<nqueens_closure> sp0(sp0c);

    ((main_cont0_closure*)SN_main_cont0.cls.get())->t1 = t1;
    // Original sync was here
    return 0;
}
THREAD(nqueens_cont0) {
    int i0;
    nqueens_cont0_closure *largs = (nqueens_cont0_closure*)(args.get());
    for (i0 = 0;(i0 < largs->n0);i0 = (i0 + 1)) {
        largs->solNum = (largs->solNum + largs->count[i0]);
    }
    SEND_ARGUMENT(largs->k, largs->solNum);
    return;
}
THREAD(main_cont0) {
    unsigned long long runtime_ms;
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    if ((largs->res == 0)) {
        fprintf(__stderrp,"No solution found.\n");
    } else {
        fprintf(__stderrp,"Total number of solutions : %d\n",largs->res);
    }
    SEND_ARGUMENT(largs->k, 0);
    return;
}
