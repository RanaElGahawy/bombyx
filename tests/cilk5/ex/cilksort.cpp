#include "cilk_explicit.hh"
/*
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Matteo Frigo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * this program uses an algorithm that we call `cilksort'.
 * The algorithm is essentially mergesort:
 *
 *   cilksort(in[1..n]) =
 *       spawn cilksort(in[1..n/2], tmp[1..n/2])
 *       spawn cilksort(in[n/2..n], tmp[n/2..n])
 *       sync
 *       spawn cilkmerge(tmp[1..n/2], tmp[n/2..n], in[1..n])
 *
 *
 * The procedure cilkmerge does the following:
 *
 *       cilkmerge(A[1..n], B[1..m], C[1..(n+m)]) =
 *          find the median of A \union B using binary
 *          search.  The binary search gives a pair
 *          (ma, mb) such that ma + mb = (n + m)/2
 *          and all elements in A[1..ma] are smaller than
 *          B[mb..m], and all the B[1..mb] are smaller
 *          than all elements in A[ma..n].
 *
 *          spawn cilkmerge(A[1..ma], B[1..mb], C[1..(n+m)/2])
 *          spawn cilkmerge(A[ma..m], B[mb..n], C[(n+m)/2 .. (n+m)])
 *          sync
 *
 * The algorithm appears for the first time (AFAIK) in S. G. Akl and
 * N. Santoro, "Optimal Parallel Merging and Sorting Without Memory
 * Conflicts", IEEE Trans. Comp., Vol. C-36 No. 11, Nov. 1987 .  The
 * paper does not express the algorithm using recursion, but the
 * idea of finding the median is there.
 *
 * For cilksort of n elements, T_1 = O(n log n) and
 * T_\infty = O(log^3 n).  There is a way to shave a
 * log factor in the critical path (left as homework).
 */

#include <cilk/cilk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "getoptions.h"

#if CILKSAN
#include "cilksan.h"
#endif

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

typedef long ELM;

/* MERGESIZE must be >= 2 */
#define KILO 1024
#define MERGESIZE (2 * KILO)
#define QUICKSIZE (2 * KILO)
#define INSERTIONSIZE 20

static unsigned long rand_nxt = 0;

unsigned long long todval(struct timeval *tp);
unsigned long my_rand();
void my_srand(unsigned long seed);
ELM med3(ELM a, ELM b, ELM c);
ELM choose_pivot(ELM *low, ELM *high);
ELM * seqpart(ELM *low, ELM *high);
void insertion_sort(ELM *low, ELM *high);
void seqquick(ELM *low, ELM *high);
void seqmerge(ELM *low1, ELM *high1, ELM *low2, ELM *high2, ELM *lowdest);
ELM * binsplit(ELM val, ELM *low, ELM *high);
THREAD(cilkmerge);
THREAD(cilksort);
void scramble_array(ELM *arr, unsigned long size);
void fill_array(ELM *arr, unsigned long size);
int usage();
int main(int argc, char **argv);
THREAD(cilkmerge_cont0);
THREAD(cilkmerge_cont1);
THREAD(cilksort_cont0);
THREAD(cilksort_cont1);
THREAD(cilksort_cont2);
THREAD(cilksort_cont3);
THREAD(cilksort_cont4);
THREAD(main_cont0);

CLOSURE_DEF(cilkmerge,
    ELM *low1;
    ELM *high1;
    ELM *low2;
    ELM *high2;
    ELM *lowdest;
);
CLOSURE_DEF(cilksort,
    ELM *low;
    ELM *tmp;
    long size;
);
CLOSURE_DEF(cilkmerge_cont0,
);
CLOSURE_DEF(cilkmerge_cont1,
);
CLOSURE_DEF(cilksort_cont0,
    ELM *low;
    long size;
    long quarter;
    ELM *A;
    ELM *B;
    ELM *C;
    ELM *D;
    ELM *tmpA;
    ELM *tmpC;
);
CLOSURE_DEF(cilksort_cont1,
    ELM *low;
    long size;
    long quarter;
    ELM *A;
    ELM *B;
    ELM *C;
    ELM *D;
    ELM *tmpA;
    ELM *tmpC;
);
CLOSURE_DEF(cilksort_cont2,
    long size;
    ELM *A;
    ELM *tmpA;
    ELM *tmpC;
);
CLOSURE_DEF(cilksort_cont3,
    long size;
    ELM *A;
    ELM *tmpA;
    ELM *tmpC;
);
CLOSURE_DEF(cilksort_cont4,
);
CLOSURE_DEF(main_cont0,
    long size;
    ELM *array;
    ELM *tmp;
    int check;
    struct timeval t1;
    struct timeval t2;
);








/*
 * simple approach for now; a better median-finding
 * may be preferable
 */




#define swap(a, b)                                                             \
  {                                                                            \
    ELM tmp;                                                                   \
    tmp = a;                                                                   \
    a = b;                                                                     \
    b = tmp;                                                                   \
  }



/*
 * tail-recursive quicksort, almost unrecognizable :-)
 */




#define swap_indices(a, b)                                                     \
  {                                                                            \
    ELM *tmp;                                                                  \
    tmp = a;                                                                   \
    a = b;                                                                     \
    b = tmp;                                                                   \
  }













const char *specifiers[] = {"-n", "-c", "-benchmark", "-h", 0};
int opt_types[] = {LONGARG, BOOLARG, BENCHMARK, BOOLARG, 0};



unsigned long long todval(struct timeval *tp) {
    return (((tp->tv_sec * 1000) * 1000) + tp->tv_usec);
}
unsigned long my_rand() {
    rand_nxt = rand_nxt * 1103515245 + 12345;
    return rand_nxt;
}
void my_srand(unsigned long seed) {
    rand_nxt = seed;
}
ELM med3(ELM a, ELM b, ELM c) {
    if ((a < b)) {
        if ((b < c)) {
            return b;
        } else {
            if ((a < c)) {
                return c;
            } else {
                return a;
            }
        }
    } else {
        if ((b > c)) {
            return b;
        } else {
            if ((a > c)) {
                return c;
            } else {
                return a;
            }
        }
    }
}
ELM choose_pivot(ELM *low, ELM *high) {
    return med3(*(low),*(high),low[((high - low) / 2)]);
}
ELM * seqpart(ELM *low, ELM *high) {
    ELM pivot;
    ELM h;
    ELM l;
    ELM *curr_low;
    ELM *curr_high;
    curr_low = low;
    curr_high = high;
    pivot = choose_pivot(low,high);
    while (1) {
        while (1) {
            h = *(curr_high);
            if ((!(h > pivot))) {
                break;
            } else {
                (curr_high--);
            }
        }
        while (1) {
            l = *(curr_low);
            if ((!(l < pivot))) {
                break;
            } else {
                (curr_low++);
            }
        }
        if ((curr_low >= curr_high)) {
            break;
        }
        *((curr_high--)) = l;
        *((curr_low++)) = h;
    }
    if ((curr_high < high)) {
        return curr_high;
    } else {
        return (curr_high - 1);
    }
}
void insertion_sort(ELM *low, ELM *high) {
    ELM *p;
    ELM *q;
    ELM a;
    ELM b;
    for (q = (low + 1);(q <= high);(++q)) {
        a = q[0];
        for (p = (q - 1);(p >= low);(p--)) {
            b = p[0];
            if ((!(b > a))) {
                break;
            } else {
                p[1] = b;
            }
        }
        p[1] = a;
    }
}
void seqquick(ELM *low, ELM *high) {
    ELM *p;
    while (((high - low) >= 20)) {
        p = seqpart(low,high);
        seqquick(low,p);
        low = (p + 1);
    }
    insertion_sort(low,high);
}
void seqmerge(ELM *low1, ELM *high1, ELM *low2, ELM *high2, ELM *lowdest) {
    ELM a1;
    ELM a2;
    if (((low1 < high1) && (low2 < high2))) {
        a1 = *(low1);
        a2 = *(low2);
        while (1) {
            if ((a1 < a2)) {
                *((lowdest++)) = a1;
                a1 = *((++low1));
                if ((low1 >= high1)) {
                    break;
                }
            } else {
                *((lowdest++)) = a2;
                a2 = *((++low2));
                if ((low2 >= high2)) {
                    break;
                }
            }
        }
    }
    if (((low1 <= high1) && (low2 <= high2))) {
        a1 = *(low1);
        a2 = *(low2);
        while (1) {
            if ((a1 < a2)) {
                *((lowdest++)) = a1;
                (++low1);
                if ((low1 > high1)) {
                    break;
                }
                a1 = *(low1);
            } else {
                *((lowdest++)) = a2;
                (++low2);
                if ((low2 > high2)) {
                    break;
                }
                a2 = *(low2);
            }
        }
    }
    if ((low1 > high1)) {
        memcpy(lowdest,low2,(sizeof(ELM) * ((high2 - low2) + 1)));
    } else {
        memcpy(lowdest,low1,(sizeof(ELM) * ((high1 - low1) + 1)));
    }
}
ELM * binsplit(ELM val, ELM *low, ELM *high) {
    ELM *mid;
    while ((low != high)) {
        mid = (low + (((high - low) + 1) >> 1));
        if ((val <= *(mid))) {
            high = (mid - 1);
        } else {
            low = mid;
        }
    }
    if ((*(low) > val)) {
        return (low - 1);
    } else {
        return low;
    }
}
THREAD(cilkmerge) {
    ELM *split1;
    ELM *split2;
    long lowsize;
    ELM *tmp;
    ELM *tmp0;
    cilkmerge_closure *largs = (cilkmerge_closure*)(args.get());
    if (((largs->high2 - largs->low2) > (largs->high1 - largs->low1))) {
        tmp = largs->low1;
        largs->low1 = largs->low2;
        largs->low2 = tmp;
        tmp0 = largs->high1;
        largs->high1 = largs->high2;
        largs->high2 = tmp0;
    }
    if ((largs->high1 < largs->low1)) {
        memcpy(largs->lowdest,largs->low2,(sizeof(ELM) * (largs->high2 - largs->low2)));
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if (((largs->high2 - largs->low2) < (2 * 1024))) {
            seqmerge(largs->low1,largs->high1,largs->low2,largs->high2,largs->lowdest);
            SEND_ARGUMENT(largs->k, 0);
        } else {
            split1 = ((((largs->high1 - largs->low1) + 1) / 2) + largs->low1);
            split2 = binsplit(*(split1),largs->low2,largs->high2);
            lowsize = (((split1 - largs->low1) + split2) - largs->low2);
            *(((largs->lowdest + lowsize) + 1)) = *(split1);
            cilkmerge_cont0_closure SN_cilkmerge_cont0c(largs->k);
            spawn_next<cilkmerge_cont0_closure> SN_cilkmerge_cont0(SN_cilkmerge_cont0c);
            cont sp0k;
            SN_BIND_VOID(SN_cilkmerge_cont0, &sp0k);
            cilkmerge_closure sp0c(sp0k);
            sp0c.low1 = largs->low1;
            sp0c.high1 = (split1 - 1);
            sp0c.low2 = largs->low2;
            sp0c.high2 = split2;
            sp0c.lowdest = largs->lowdest;
            spawn<cilkmerge_closure> sp0(sp0c);

            cont sp1k;
            SN_BIND_VOID(SN_cilkmerge_cont0, &sp1k);
            cilkmerge_closure sp1c(sp1k);
            sp1c.low1 = (split1 + 1);
            sp1c.high1 = largs->high1;
            sp1c.low2 = (split2 + 1);
            sp1c.high2 = largs->high2;
            sp1c.lowdest = ((largs->lowdest + lowsize) + 2);
            spawn<cilkmerge_closure> sp1(sp1c);

            // Original sync was here
        }
    }
}
THREAD(cilksort) {
    long quarter;
    ELM *A;
    ELM *B;
    ELM *C;
    ELM *D;
    ELM *tmpA;
    ELM *tmpB;
    ELM *tmpC;
    ELM *tmpD;
    cilksort_closure *largs = (cilksort_closure*)(args.get());
    quarter = (largs->size / 4);
    if ((largs->size < (2 * 1024))) {
        seqquick(largs->low,((largs->low + largs->size) - 1));
        SEND_ARGUMENT(largs->k, 0);
    } else {
        A = largs->low;
        tmpA = largs->tmp;
        B = (A + quarter);
        tmpB = (tmpA + quarter);
        C = (B + quarter);
        tmpC = (tmpB + quarter);
        D = (C + quarter);
        tmpD = (tmpC + quarter);
        cilksort_cont0_closure SN_cilksort_cont0c(largs->k);
        spawn_next<cilksort_cont0_closure> SN_cilksort_cont0(SN_cilksort_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_cilksort_cont0, &sp0k);
        cilksort_closure sp0c(sp0k);
        sp0c.low = A;
        sp0c.tmp = tmpA;
        sp0c.size = quarter;
        spawn<cilksort_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_cilksort_cont0, &sp1k);
        cilksort_closure sp1c(sp1k);
        sp1c.low = B;
        sp1c.tmp = tmpB;
        sp1c.size = quarter;
        spawn<cilksort_closure> sp1(sp1c);

        cont sp2k;
        SN_BIND_VOID(SN_cilksort_cont0, &sp2k);
        cilksort_closure sp2c(sp2k);
        sp2c.low = C;
        sp2c.tmp = tmpC;
        sp2c.size = quarter;
        spawn<cilksort_closure> sp2(sp2c);

        cont sp3k;
        SN_BIND_VOID(SN_cilksort_cont0, &sp3k);
        cilksort_closure sp3c(sp3k);
        sp3c.low = D;
        sp3c.tmp = tmpD;
        sp3c.size = (largs->size - (3 * quarter));
        spawn<cilksort_closure> sp3(sp3c);

        ((cilksort_cont0_closure*)SN_cilksort_cont0.cls.get())->tmpA = tmpA;
        ((cilksort_cont0_closure*)SN_cilksort_cont0.cls.get())->D = D;
        ((cilksort_cont0_closure*)SN_cilksort_cont0.cls.get())->tmpC = tmpC;
        ((cilksort_cont0_closure*)SN_cilksort_cont0.cls.get())->A = A;
        ((cilksort_cont0_closure*)SN_cilksort_cont0.cls.get())->C = C;
        ((cilksort_cont0_closure*)SN_cilksort_cont0.cls.get())->quarter = quarter;
        ((cilksort_cont0_closure*)SN_cilksort_cont0.cls.get())->size = largs->size;
        ((cilksort_cont0_closure*)SN_cilksort_cont0.cls.get())->B = B;
        ((cilksort_cont0_closure*)SN_cilksort_cont0.cls.get())->low = largs->low;
        // Original sync was here
    }
}
void scramble_array(ELM *arr, unsigned long size) {
    unsigned long i;
    unsigned long j;
    ELM tmp;
    for (i = 0;(i < size);(++i)) {
        j = my_rand();
        j = (j % size);
        tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}
void fill_array(ELM *arr, unsigned long size) {
    unsigned long i;
    my_srand(1);
    for (i = 0;(i < size);(++i)) {
        arr[i] = i;
    }
    scramble_array(arr,size);
}
int usage() {
    fprintf(__stderrp,"\nUsage: cilksort [<cilk-options>] [-n size] [-c] [-benchmark] [-h]\n\n");
    fprintf(__stderrp,"Cilksort is a parallel sorting algorithm, donned \"Multisort\", which\n");
    fprintf(__stderrp,"is a variant of ordinary mergesort.  Multisort begins by dividing an\n");
    fprintf(__stderrp,"array of elements in half and sorting each half.  It then merges the\n");
    fprintf(__stderrp,"two sorted halves back together, but in a divide-and-conquer approach\n");
    fprintf(__stderrp,"rather than the usual serial merge.\n\n");
    return (-1);
}
int main(int argc, char **argv) {
    long size;
    ELM *array;
    ELM *tmp;
    int benchmark;
    int help;
    int check;
    struct timeval t1;
    check = 0;
    size = 3000000;
    get_options(argc,argv,specifiers,opt_types,&(size),&(check),&(benchmark),&(help));
    if (help) {
        return usage();
    } else {
        main_cont0_closure SN_main_cont0c(CONT_DUMMY);
        spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
        if (benchmark) {
            switch (benchmark) {
  case 1:
    size = 10000;
    break;
  case 2:
    size = 3000000;
    break;
  case 3:
    size = 4100000;
    break;
}
;
        }
        array = ((ELM *) malloc((size * sizeof(ELM))));
        tmp = ((ELM *) malloc((size * sizeof(ELM))));
        fill_array(array,size);
        gettimeofday(&(t1),0);
        cont sp0k;
        SN_BIND_VOID(SN_main_cont0, &sp0k);
        cilksort_closure sp0c(sp0k);
        sp0c.low = array;
        sp0c.tmp = tmp;
        sp0c.size = size;
        spawn<cilksort_closure> sp0(sp0c);

        ((main_cont0_closure*)SN_main_cont0.cls.get())->t1 = t1;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->check = check;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->tmp = tmp;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->array = array;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->size = size;
        // Original sync was here
    }
}
THREAD(cilkmerge_cont0) {
    cilkmerge_cont0_closure *largs = (cilkmerge_cont0_closure*)(args.get());
    cilkmerge_cont1_closure SN_cilkmerge_cont1c(largs->k);
    spawn_next<cilkmerge_cont1_closure> SN_cilkmerge_cont1(SN_cilkmerge_cont1c);
    // Original sync was here
    return;
}
THREAD(cilkmerge_cont1) {
    cilkmerge_cont1_closure *largs = (cilkmerge_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(cilksort_cont0) {
    cilksort_cont0_closure *largs = (cilksort_cont0_closure*)(args.get());
    cilksort_cont1_closure SN_cilksort_cont1c(largs->k);
    spawn_next<cilksort_cont1_closure> SN_cilksort_cont1(SN_cilksort_cont1c);
    ((cilksort_cont1_closure*)SN_cilksort_cont1.cls.get())->tmpA = largs->tmpA;
    ((cilksort_cont1_closure*)SN_cilksort_cont1.cls.get())->C = largs->C;
    ((cilksort_cont1_closure*)SN_cilksort_cont1.cls.get())->A = largs->A;
    ((cilksort_cont1_closure*)SN_cilksort_cont1.cls.get())->B = largs->B;
    ((cilksort_cont1_closure*)SN_cilksort_cont1.cls.get())->quarter = largs->quarter;
    ((cilksort_cont1_closure*)SN_cilksort_cont1.cls.get())->size = largs->size;
    ((cilksort_cont1_closure*)SN_cilksort_cont1.cls.get())->tmpC = largs->tmpC;
    ((cilksort_cont1_closure*)SN_cilksort_cont1.cls.get())->D = largs->D;
    ((cilksort_cont1_closure*)SN_cilksort_cont1.cls.get())->low = largs->low;
    // Original sync was here
    return;
}
THREAD(cilksort_cont1) {
    cilksort_cont1_closure *largs = (cilksort_cont1_closure*)(args.get());
    cilksort_cont2_closure SN_cilksort_cont2c(largs->k);
    spawn_next<cilksort_cont2_closure> SN_cilksort_cont2(SN_cilksort_cont2c);
    cont sp0k;
    SN_BIND_VOID(SN_cilksort_cont2, &sp0k);
    cilkmerge_closure sp0c(sp0k);
    sp0c.low1 = largs->A;
    sp0c.high1 = ((largs->A + largs->quarter) - 1);
    sp0c.low2 = largs->B;
    sp0c.high2 = ((largs->B + largs->quarter) - 1);
    sp0c.lowdest = largs->tmpA;
    spawn<cilkmerge_closure> sp0(sp0c);

    cont sp1k;
    SN_BIND_VOID(SN_cilksort_cont2, &sp1k);
    cilkmerge_closure sp1c(sp1k);
    sp1c.low1 = largs->C;
    sp1c.high1 = ((largs->C + largs->quarter) - 1);
    sp1c.low2 = largs->D;
    sp1c.high2 = ((largs->low + largs->size) - 1);
    sp1c.lowdest = largs->tmpC;
    spawn<cilkmerge_closure> sp1(sp1c);

    ((cilksort_cont2_closure*)SN_cilksort_cont2.cls.get())->tmpA = largs->tmpA;
    ((cilksort_cont2_closure*)SN_cilksort_cont2.cls.get())->A = largs->A;
    ((cilksort_cont2_closure*)SN_cilksort_cont2.cls.get())->tmpC = largs->tmpC;
    ((cilksort_cont2_closure*)SN_cilksort_cont2.cls.get())->size = largs->size;
    // Original sync was here
    return;
}
THREAD(cilksort_cont2) {
    cilksort_cont2_closure *largs = (cilksort_cont2_closure*)(args.get());
    cilksort_cont3_closure SN_cilksort_cont3c(largs->k);
    spawn_next<cilksort_cont3_closure> SN_cilksort_cont3(SN_cilksort_cont3c);
    ((cilksort_cont3_closure*)SN_cilksort_cont3.cls.get())->tmpC = largs->tmpC;
    ((cilksort_cont3_closure*)SN_cilksort_cont3.cls.get())->tmpA = largs->tmpA;
    ((cilksort_cont3_closure*)SN_cilksort_cont3.cls.get())->A = largs->A;
    ((cilksort_cont3_closure*)SN_cilksort_cont3.cls.get())->size = largs->size;
    // Original sync was here
    return;
}
THREAD(cilksort_cont3) {
    cilksort_cont3_closure *largs = (cilksort_cont3_closure*)(args.get());
    cilksort_cont4_closure SN_cilksort_cont4c(largs->k);
    spawn_next<cilksort_cont4_closure> SN_cilksort_cont4(SN_cilksort_cont4c);
    cont sp0k;
    SN_BIND_VOID(SN_cilksort_cont4, &sp0k);
    cilkmerge_closure sp0c(sp0k);
    sp0c.low1 = largs->tmpA;
    sp0c.high1 = (largs->tmpC - 1);
    sp0c.low2 = largs->tmpC;
    sp0c.high2 = ((largs->tmpA + largs->size) - 1);
    sp0c.lowdest = largs->A;
    spawn<cilkmerge_closure> sp0(sp0c);

    // Original sync was here
    return;
}
THREAD(cilksort_cont4) {
    cilksort_cont4_closure *largs = (cilksort_cont4_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(main_cont0) {
    long i;
    int success;
    unsigned long long runtime_ms;
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    printf("%f\n",(runtime_ms / 1000.));
    if (largs->check) {
        printf("Now check result ... \n");
        success = 1;
        for (i = 0;(i < largs->size);(++i)) {
            if ((largs->array[i] != i)) {
                success = 0;
            }
        }
        if ((!success)) {
            fprintf(__stderrp,"SORTING FAILURE!");
        } else {
            fprintf(__stderrp,"Sorting successful.");
        }
    }
    fprintf(__stderrp,"\nCilk Example: cilksort\n");
    fprintf(__stderrp,"options: number of elements = %ld\n\n",largs->size);
    free(largs->array);
    free(largs->tmp);
    SEND_ARGUMENT(largs->k, 0);
}
