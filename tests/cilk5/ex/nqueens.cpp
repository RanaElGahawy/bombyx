#include "cilk_explicit.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <cilk/cilk.h>

#if CILKSAN
#include "cilksan.h"
#endif

unsigned long long todval(struct timeval *tp);
int ok(int n, char *a);
THREAD(nqueens);
int main(int argc, char **argv);
THREAD(nqueens_cont0);
THREAD(main_cont0);

CLOSURE_DEF(nqueens,
    int n;
    int j;
    char *a;
);
CLOSURE_DEF(nqueens_cont0,
    int n;
    int *count;
    int solNum;
);
CLOSURE_DEF(main_cont0,
    int res;
    struct timeval t1;
    struct timeval t2;
);


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







unsigned long long todval(struct timeval *tp) {
    return (((tp->tv_sec * 1000) * 1000) + tp->tv_usec);
}
int ok(int n, char *a) {
    int i;
    int j;
    char p;
    char q;
    for (i = 0;(i < n);(i++)) {
        p = a[i];
        for (j = (i + 1);(j < n);(j++)) {
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
    char *b;
    int i;
    int *count;
    int solNum;
    nqueens_closure *largs = (nqueens_closure*)(args.get());
    solNum = 0;
    if ((largs->n == largs->j)) {
        SEND_ARGUMENT(largs->k, 1);
    } else {
        count = ((int *) __builtin_alloca((largs->n * sizeof(int))));
        ((void) memset(count,0,(largs->n * sizeof(int))));
        nqueens_cont0_closure SN_nqueens_cont0c(largs->k);
        spawn_next<nqueens_cont0_closure> SN_nqueens_cont0(SN_nqueens_cont0c);
        for (i = 0;(i < largs->n);(i++)) {
            b = ((char *) __builtin_alloca(((largs->j + 1) * sizeof(char))));
            memcpy(b,largs->a,(largs->j * sizeof(char)));
            b[largs->j] = i;
            if (ok((largs->j + 1),b)) {
                cont sp0k;
                SN_BIND_EXT(SN_nqueens_cont0, &sp0k, &(count[i]));
                nqueens_closure sp0c(sp0k);
                sp0c.n = largs->n;
                sp0c.j = (largs->j + 1);
                sp0c.a = b;
                spawn<nqueens_closure> sp0(sp0c);

            }
        }
        ((nqueens_cont0_closure*)SN_nqueens_cont0.cls.get())->solNum = solNum;
        ((nqueens_cont0_closure*)SN_nqueens_cont0.cls.get())->count = count;
        ((nqueens_cont0_closure*)SN_nqueens_cont0.cls.get())->n = largs->n;
        // Original sync was here
    }
}
int main(int argc, char **argv) {
    int n;
    char *a;
    int res;
    struct timeval t1;
    n = 13;
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    if ((argc < 2)) {
        fprintf(stderr,"Usage: %s [<cilk-options>] <n>\n",argv[0]);
        fprintf(stderr,"Use default board size, n = 13.\n");
    } else {
        n = atoi(argv[1]);
        fprintf(stderr,"Running %s with n = %d.\n",argv[0],n);
    }
    a = ((char *) __builtin_alloca((n * sizeof(char))));
    res = 0;
    gettimeofday(&(t1),0);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, res);
    nqueens_closure sp0c(sp0k);
    sp0c.n = n;
    sp0c.j = 0;
    sp0c.a = a;
    spawn<nqueens_closure> sp0(sp0c);

    ((main_cont0_closure*)SN_main_cont0.cls.get())->t1 = t1;
    // Original sync was here
    return 0;
}
THREAD(nqueens_cont0) {
    int i;
    nqueens_cont0_closure *largs = (nqueens_cont0_closure*)(args.get());
    for (i = 0;(i < largs->n);(i++)) {
        largs->solNum = (largs->solNum + largs->count[i]);
    }
    SEND_ARGUMENT(largs->k, largs->solNum);
}
THREAD(main_cont0) {
    unsigned long long runtime_ms;
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    printf("%f\n",(runtime_ms / 1000.));
    if ((largs->res == 0)) {
        fprintf(stderr,"No solution found.\n");
    } else {
        fprintf(stderr,"Total number of solutions : %d\n",largs->res);
    }
    SEND_ARGUMENT(largs->k, 0);
}
