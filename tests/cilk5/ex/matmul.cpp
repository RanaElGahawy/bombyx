#include "cilk_explicit.hh"
/* 
 * Rectangular matrix multiplication.
 *
 * See the paper ``Cache-Oblivious Algorithms'', by
 * Matteo Frigo, Charles E. Leiserson, Harald Prokop, and 
 * Sridhar Ramachandran, FOCS 1999, for an explanation of
 * why this algorithm is good for caches.
 *
 * Author: Matteo Frigo
 */

/*
 * Copyright (c) 2003 Massachusetts Institute of Technology
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

#include <cilk/cilk.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include "getoptions.h"

unsigned long long todval(struct timeval *tp);
int cilk_rand();
void init_vec(float *V, int n);
double maxerror_vec(float *V1, float *V2, int n);
void zero(float *A, int n);
void init(float *A, int n);
double maxerror(float *A, float *B, int n);
void iter_matmul(float *A, float *B, float *C, int n);
THREAD(rec_matmulAdd);
THREAD(rec_matmul);
void mat_vec_mul(float *A, float *R, float *P, int m, int n, int ld, int add);
int main(int argc, char **argv);
THREAD(rec_matmulAdd_cont0);
THREAD(rec_matmulAdd_cont1);
THREAD(rec_matmulAdd_cont2);
THREAD(rec_matmulAdd_cont3);
THREAD(rec_matmul_cont0);
THREAD(rec_matmul_cont1);
THREAD(rec_matmul_cont2);
THREAD(rec_matmul_cont3);
THREAD(main_cont0);

CLOSURE_DEF(rec_matmulAdd,
    float *A;
    float *B;
    float *C;
    int m;
    int n;
    int p;
    int ld;
);
CLOSURE_DEF(rec_matmul,
    float *A;
    float *B;
    float *C;
    int m;
    int n;
    int p;
    int ld;
);
CLOSURE_DEF(rec_matmulAdd_cont0,
);
CLOSURE_DEF(rec_matmulAdd_cont1,
    float *A;
    float *B;
    float *C;
    int m;
    int n;
    int p;
    int ld;
    int n1;
);
CLOSURE_DEF(rec_matmulAdd_cont2,
);
CLOSURE_DEF(rec_matmulAdd_cont3,
);
CLOSURE_DEF(rec_matmul_cont0,
);
CLOSURE_DEF(rec_matmul_cont1,
    float *A;
    float *B;
    float *C;
    int m;
    int n;
    int p;
    int ld;
    int n1;
);
CLOSURE_DEF(rec_matmul_cont2,
);
CLOSURE_DEF(rec_matmul_cont3,
);
CLOSURE_DEF(main_cont0,
    int n;
    int check;
    int rand_check;
    float *A;
    float *B;
    float *C;
    float *R;
    float *P1;
    float *P2;
    float *C2;
    struct timeval t1;
    struct timeval t2;
);



#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

#define REAL float

unsigned long rand_nxt = 0;




void zero_vec(REAL *V, int n) {

  int i;

  for(i = 0; i < n; i++) {
    V[i] = (REAL) 0.0;
  }
}





double sum_diff_vec(REAL *V1, REAL *V2, int n) {
  int i; 
  double err = 0.0, diff;

  for(i = 0; i < n; i++) {
    diff = (V1[i] - V2[i]) / V1[i];
    if(diff < 0) 
      diff = -diff;
    err += diff; 
  }

  return err;
}

void print_vec(REAL *V, int n) {
  int i;

  for(i = 0; i < n; i++) {
    printf("%f  ", V[i]);
  }    
}

void print_matrix(REAL *A, int n, int ld) {
  int i, j;

  for(i = 0; i < n; i++) {
    for(j = 0; j < n; j++) {
      printf("%f  ", A[i*ld + j]);
    }
    printf("\n");
  }    
}









/*
 * A \in M(m, n)
 * B \in M(n, p)
 * C \in M(m, p)
 */




/* 
 * ANGE:
 * recursively mutliply A (matrix) and R (vector)
 * A = matrix, in M(m, n) (size m x n)
 * R = input column vector, size n
 * P = output column vector, size n
 * ld = size of each row in A
 * add = add the result in if set 
 */


const char *specifiers[] = {"-n", "-c", "-rc", "-h", 0};
int opt_types[] = {INTARG, BOOLARG, BOOLARG, BOOLARG, 0};




unsigned long long todval(struct timeval *tp) {
    return (((tp->tv_sec * 1000) * 1000) + tp->tv_usec);
}
int cilk_rand() {
    int result;
    rand_nxt = rand_nxt * 1103515245 + 12345;
    result = ((rand_nxt >> 16) % (((unsigned int) 2147483647) + 1));
    return result;
}
void init_vec(float *V, int n) {
    int i;
    for (i = 0;(i < n);(i++)) {
        V[i] = ((float) cilk_rand());
    }
}
double maxerror_vec(float *V1, float *V2, int n) {
    int i;
    double err;
    double diff;
    err = 0.;
    for (i = 0;(i < n);(i++)) {
        diff = ((V1[i] - V2[i]) / V1[i]);
        if ((diff < 0)) {
            diff = (-diff);
        }
        if ((diff > err)) {
            err = diff;
        }
    }
    return err;
}
void zero(float *A, int n) {
    int i;
    int j;
    for (i = 0;(i < n);(i++)) {
        for (j = 0;(j < n);(j++)) {
            A[((i * n) + j)] = 0.;
        }
    }
}
void init(float *A, int n) {
    int i;
    int j;
    for (i = 0;(i < n);(i++)) {
        for (j = 0;(j < n);(j++)) {
            A[((i * n) + j)] = ((double) cilk_rand());
        }
    }
}
double maxerror(float *A, float *B, int n) {
    int i;
    int j;
    double error;
    double diff;
    error = 0.;
    for (i = 0;(i < n);(i++)) {
        for (j = 0;(j < n);(j++)) {
            diff = ((A[((i * n) + j)] - B[((i * n) + j)]) / A[((i * n) + j)]);
            if ((diff < 0)) {
                diff = (-diff);
            }
            if ((diff > error)) {
                error = diff;
            }
        }
    }
    return error;
}
void iter_matmul(float *A, float *B, float *C, int n) {
    int i;
    int j;
    int k0;
    float c;
    for (i = 0;(i < n);(i++)) {
        for (k0 = 0;(k0 < n);(k0++)) {
            c = 0.;
            for (j = 0;(j < n);(j++)) {
                c = (c + (A[((i * n) + j)] * B[((j * n) + k0)]));
            }
            C[((i * n) + k0)] = c;
        }
    }
}
THREAD(rec_matmulAdd) {
    int i;
    int j;
    int k0;
    float c;
    int m1;
    int n1;
    int p1;
    rec_matmulAdd_closure *largs = (rec_matmulAdd_closure*)(args.get());
    if ((((largs->m + largs->n) + largs->p) <= 64)) {
        for (i = 0;(i < largs->m);(i++)) {
            for (k0 = 0;(k0 < largs->p);(k0++)) {
                c = 0.;
                for (j = 0;(j < largs->n);(j++)) {
                    c = (c + (largs->A[((i * largs->ld) + j)] * largs->B[((j * largs->ld) + k0)]));
                }
                largs->C[i * largs->ld + k0] += c;
            }
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if ((largs->n >= largs->p)) {
            if ((largs->m >= largs->n)) {
                m1 = (largs->m >> 1);
                rec_matmulAdd_cont3_closure SN_rec_matmulAdd_cont3c(largs->k);
                spawn_next<rec_matmulAdd_cont3_closure> SN_rec_matmulAdd_cont3(SN_rec_matmulAdd_cont3c);
                cont sp0k;
                SN_BIND_VOID(SN_rec_matmulAdd_cont3, &sp0k);
                rec_matmulAdd_closure sp0c(sp0k);
                sp0c.A = largs->A;
                sp0c.B = largs->B;
                sp0c.C = largs->C;
                sp0c.m = m1;
                sp0c.n = largs->n;
                sp0c.p = largs->p;
                sp0c.ld = largs->ld;
                spawn<rec_matmulAdd_closure> sp0(sp0c);

                cont sp1k;
                SN_BIND_VOID(SN_rec_matmulAdd_cont3, &sp1k);
                rec_matmulAdd_closure sp1c(sp1k);
                sp1c.A = (largs->A + (m1 * largs->ld));
                sp1c.B = largs->B;
                sp1c.C = (largs->C + (m1 * largs->ld));
                sp1c.m = (largs->m - m1);
                sp1c.n = largs->n;
                sp1c.p = largs->p;
                sp1c.ld = largs->ld;
                spawn<rec_matmulAdd_closure> sp1(sp1c);

                // Original sync was here
            } else {
                n1 = (largs->n >> 1);
                rec_matmulAdd_cont1_closure SN_rec_matmulAdd_cont1c(largs->k);
                spawn_next<rec_matmulAdd_cont1_closure> SN_rec_matmulAdd_cont1(SN_rec_matmulAdd_cont1c);
                cont sp2k;
                SN_BIND_VOID(SN_rec_matmulAdd_cont1, &sp2k);
                rec_matmulAdd_closure sp2c(sp2k);
                sp2c.A = largs->A;
                sp2c.B = largs->B;
                sp2c.C = largs->C;
                sp2c.m = largs->m;
                sp2c.n = n1;
                sp2c.p = largs->p;
                sp2c.ld = largs->ld;
                spawn<rec_matmulAdd_closure> sp2(sp2c);

                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->n1 = n1;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->ld = largs->ld;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->p = largs->p;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->n = largs->n;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->B = largs->B;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->m = largs->m;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->C = largs->C;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->A = largs->A;
                // Original sync was here
            }
        } else {
            p1 = (largs->p >> 1);
            rec_matmulAdd_cont0_closure SN_rec_matmulAdd_cont0c(largs->k);
            spawn_next<rec_matmulAdd_cont0_closure> SN_rec_matmulAdd_cont0(SN_rec_matmulAdd_cont0c);
            cont sp3k;
            SN_BIND_VOID(SN_rec_matmulAdd_cont0, &sp3k);
            rec_matmulAdd_closure sp3c(sp3k);
            sp3c.A = largs->A;
            sp3c.B = largs->B;
            sp3c.C = largs->C;
            sp3c.m = largs->m;
            sp3c.n = largs->n;
            sp3c.p = p1;
            sp3c.ld = largs->ld;
            spawn<rec_matmulAdd_closure> sp3(sp3c);

            cont sp4k;
            SN_BIND_VOID(SN_rec_matmulAdd_cont0, &sp4k);
            rec_matmulAdd_closure sp4c(sp4k);
            sp4c.A = largs->A;
            sp4c.B = (largs->B + p1);
            sp4c.C = (largs->C + p1);
            sp4c.m = largs->m;
            sp4c.n = largs->n;
            sp4c.p = (largs->p - p1);
            sp4c.ld = largs->ld;
            spawn<rec_matmulAdd_closure> sp4(sp4c);

            // Original sync was here
        }
    }
}
THREAD(rec_matmul) {
    int i;
    int j;
    int k0;
    float c;
    int m1;
    int n1;
    int p1;
    rec_matmul_closure *largs = (rec_matmul_closure*)(args.get());
    if ((((largs->m + largs->n) + largs->p) <= 64)) {
        for (i = 0;(i < largs->m);(i++)) {
            for (k0 = 0;(k0 < largs->p);(k0++)) {
                c = 0.;
                for (j = 0;(j < largs->n);(j++)) {
                    c = (c + (largs->A[((i * largs->ld) + j)] * largs->B[((j * largs->ld) + k0)]));
                }
                largs->C[((i * largs->ld) + k0)] = c;
            }
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if ((largs->n >= largs->p)) {
            if ((largs->m >= largs->n)) {
                m1 = (largs->m >> 1);
                rec_matmul_cont3_closure SN_rec_matmul_cont3c(largs->k);
                spawn_next<rec_matmul_cont3_closure> SN_rec_matmul_cont3(SN_rec_matmul_cont3c);
                cont sp0k;
                SN_BIND_VOID(SN_rec_matmul_cont3, &sp0k);
                rec_matmul_closure sp0c(sp0k);
                sp0c.A = largs->A;
                sp0c.B = largs->B;
                sp0c.C = largs->C;
                sp0c.m = m1;
                sp0c.n = largs->n;
                sp0c.p = largs->p;
                sp0c.ld = largs->ld;
                spawn<rec_matmul_closure> sp0(sp0c);

                cont sp1k;
                SN_BIND_VOID(SN_rec_matmul_cont3, &sp1k);
                rec_matmul_closure sp1c(sp1k);
                sp1c.A = (largs->A + (m1 * largs->ld));
                sp1c.B = largs->B;
                sp1c.C = (largs->C + (m1 * largs->ld));
                sp1c.m = (largs->m - m1);
                sp1c.n = largs->n;
                sp1c.p = largs->p;
                sp1c.ld = largs->ld;
                spawn<rec_matmul_closure> sp1(sp1c);

                // Original sync was here
            } else {
                n1 = (largs->n >> 1);
                rec_matmul_cont1_closure SN_rec_matmul_cont1c(largs->k);
                spawn_next<rec_matmul_cont1_closure> SN_rec_matmul_cont1(SN_rec_matmul_cont1c);
                cont sp2k;
                SN_BIND_VOID(SN_rec_matmul_cont1, &sp2k);
                rec_matmul_closure sp2c(sp2k);
                sp2c.A = largs->A;
                sp2c.B = largs->B;
                sp2c.C = largs->C;
                sp2c.m = largs->m;
                sp2c.n = n1;
                sp2c.p = largs->p;
                sp2c.ld = largs->ld;
                spawn<rec_matmul_closure> sp2(sp2c);

                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->p = largs->p;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->C = largs->C;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->n = largs->n;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->ld = largs->ld;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->B = largs->B;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->m = largs->m;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->n1 = n1;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->A = largs->A;
                // Original sync was here
            }
        } else {
            p1 = (largs->p >> 1);
            rec_matmul_cont0_closure SN_rec_matmul_cont0c(largs->k);
            spawn_next<rec_matmul_cont0_closure> SN_rec_matmul_cont0(SN_rec_matmul_cont0c);
            cont sp3k;
            SN_BIND_VOID(SN_rec_matmul_cont0, &sp3k);
            rec_matmul_closure sp3c(sp3k);
            sp3c.A = largs->A;
            sp3c.B = largs->B;
            sp3c.C = largs->C;
            sp3c.m = largs->m;
            sp3c.n = largs->n;
            sp3c.p = p1;
            sp3c.ld = largs->ld;
            spawn<rec_matmul_closure> sp3(sp3c);

            cont sp4k;
            SN_BIND_VOID(SN_rec_matmul_cont0, &sp4k);
            rec_matmul_closure sp4c(sp4k);
            sp4c.A = largs->A;
            sp4c.B = (largs->B + p1);
            sp4c.C = (largs->C + p1);
            sp4c.m = largs->m;
            sp4c.n = largs->n;
            sp4c.p = (largs->p - p1);
            sp4c.ld = largs->ld;
            spawn<rec_matmul_closure> sp4(sp4c);

            // Original sync was here
        }
    }
}
void mat_vec_mul(float *A, float *R, float *P, int m, int n, int ld, int add) {
    int i;
    int j;
    float c;
    float c0;
    int m1;
    int n1;
    if (((m + n) <= 64)) {
        if (add) {
            for (i = 0;(i < m);(i++)) {
                c = 0;
                for (j = 0;(j < n);(j++)) {
                    c = (c + (A[((i * ld) + j)] * R[j]));
                }
                P[i] += c;
            }
        } else {
            for (i = 0;(i < m);(i++)) {
                c0 = 0;
                for (j = 0;(j < n);(j++)) {
                    c0 = (c0 + (A[((i * ld) + j)] * R[j]));
                }
                P[i] = c0;
            }
        }
    } else {
        if ((m >= n)) {
            m1 = (m >> 1);
            mat_vec_mul(A,R,P,m1,n,ld,add);
            mat_vec_mul((A + (m1 * ld)),R,(P + m1),(m - m1),n,ld,add);
        } else {
            n1 = (n >> 1);
            mat_vec_mul(A,R,P,m,n1,ld,add);
            mat_vec_mul((A + n1),(R + n1),P,m,(n - n1),ld,1);
        }
    }
}
int main(int argc, char **argv) {
    int n;
    int check;
    int rand_check;
    int help;
    float *A;
    float *B;
    float *C;
    float *R;
    float *P1;
    float *P2;
    float *C2;
    struct timeval t1;
    n = 1024;
    check = 0;
    rand_check = 0;
    help = 0;
    get_options(argc,argv,specifiers,opt_types,&(n),&(check),&(rand_check),&(help));
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    if (help) {
        fprintf(__stderrp,"Usage: matmul [-n size] [-c] [-rc] [-h] [<cilk options>]\n");
        fprintf(__stderrp,"if -c is set, check result against iterative matrix multiply O(n^3).\n");
        fprintf(__stderrp,"if -rc is set, check result against randomlized algo. due to Freivalds O(n^2).\n");
        exit(1);
    }
    A = ((float *) malloc(((n * n) * sizeof(float))));
    B = ((float *) malloc(((n * n) * sizeof(float))));
    C = ((float *) malloc(((n * n) * sizeof(float))));
    if (rand_check) {
        R = ((float *) malloc((n * sizeof(float))));
        P1 = ((float *) malloc((n * sizeof(float))));
        P2 = ((float *) malloc((n * sizeof(float))));
        init_vec(R,n);
    } else {
        if (check) {
            C2 = ((float *) malloc(((n * n) * sizeof(float))));
            zero(C2,n);
        }
    }
    init(A,n);
    init(B,n);
    fprintf(__stderrp,"\nCalculate using recursive method ... (timing start here)\n");
    zero(C,n);
    gettimeofday(&(t1),0);
    cont sp0k;
    SN_BIND_VOID(SN_main_cont0, &sp0k);
    rec_matmul_closure sp0c(sp0k);
    sp0c.A = A;
    sp0c.B = B;
    sp0c.C = C;
    sp0c.m = n;
    sp0c.n = n;
    sp0c.p = n;
    sp0c.ld = n;
    spawn<rec_matmul_closure> sp0(sp0c);

    ((main_cont0_closure*)SN_main_cont0.cls.get())->P1 = P1;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->R = R;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->C = C;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->t1 = t1;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->B = B;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->check = check;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->A = A;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->C2 = C2;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->P2 = P2;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->rand_check = rand_check;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->n = n;
    // Original sync was here
    return 0;
}
THREAD(rec_matmulAdd_cont0) {
    rec_matmulAdd_cont0_closure *largs = (rec_matmulAdd_cont0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(rec_matmulAdd_cont1) {
    rec_matmulAdd_cont1_closure *largs = (rec_matmulAdd_cont1_closure*)(args.get());
    rec_matmulAdd_cont2_closure SN_rec_matmulAdd_cont2c(largs->k);
    spawn_next<rec_matmulAdd_cont2_closure> SN_rec_matmulAdd_cont2(SN_rec_matmulAdd_cont2c);
    cont sp0k;
    SN_BIND_VOID(SN_rec_matmulAdd_cont2, &sp0k);
    rec_matmulAdd_closure sp0c(sp0k);
    sp0c.A = (largs->A + largs->n1);
    sp0c.B = (largs->B + (largs->n1 * largs->ld));
    sp0c.C = largs->C;
    sp0c.m = largs->m;
    sp0c.n = (largs->n - largs->n1);
    sp0c.p = largs->p;
    sp0c.ld = largs->ld;
    spawn<rec_matmulAdd_closure> sp0(sp0c);

    // Original sync was here
    return;
}
THREAD(rec_matmulAdd_cont2) {
    rec_matmulAdd_cont2_closure *largs = (rec_matmulAdd_cont2_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(rec_matmulAdd_cont3) {
    rec_matmulAdd_cont3_closure *largs = (rec_matmulAdd_cont3_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(rec_matmul_cont0) {
    rec_matmul_cont0_closure *largs = (rec_matmul_cont0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(rec_matmul_cont1) {
    rec_matmul_cont1_closure *largs = (rec_matmul_cont1_closure*)(args.get());
    rec_matmul_cont2_closure SN_rec_matmul_cont2c(largs->k);
    spawn_next<rec_matmul_cont2_closure> SN_rec_matmul_cont2(SN_rec_matmul_cont2c);
    cont sp0k;
    SN_BIND_VOID(SN_rec_matmul_cont2, &sp0k);
    rec_matmulAdd_closure sp0c(sp0k);
    sp0c.A = (largs->A + largs->n1);
    sp0c.B = (largs->B + (largs->n1 * largs->ld));
    sp0c.C = largs->C;
    sp0c.m = largs->m;
    sp0c.n = (largs->n - largs->n1);
    sp0c.p = largs->p;
    sp0c.ld = largs->ld;
    spawn<rec_matmulAdd_closure> sp0(sp0c);

    // Original sync was here
    return;
}
THREAD(rec_matmul_cont2) {
    rec_matmul_cont2_closure *largs = (rec_matmul_cont2_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(rec_matmul_cont3) {
    rec_matmul_cont3_closure *largs = (rec_matmul_cont3_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(main_cont0) {
    double err;
    unsigned long long runtime_ms;
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    printf("%f\n",(runtime_ms / 1000.));
    if (largs->rand_check) {
        mat_vec_mul(largs->B,largs->R,largs->P1,largs->n,largs->n,largs->n,0);
        mat_vec_mul(largs->A,largs->P1,largs->P2,largs->n,largs->n,largs->n,0);
        mat_vec_mul(largs->C,largs->R,largs->P1,largs->n,largs->n,largs->n,0);
        err = maxerror_vec(largs->P1,largs->P2,largs->n);
        fprintf(__stderrp,"Max error     = %g\n",err);
    } else {
        if (largs->check) {
            iter_matmul(largs->A,largs->B,largs->C2,largs->n);
            err = maxerror(largs->C,largs->C2,largs->n);
            fprintf(__stderrp,"Max error     = %g\n",err);
        }
    }
    fprintf(__stderrp,"\nCilk Example: matmul\n");
    fprintf(__stderrp,"Options: size = %d\n",largs->n);
    free(largs->C);
    free(largs->B);
    free(largs->A);
    if (largs->rand_check) {
        free(largs->R);
        free(largs->P1);
        free(largs->P2);
    } else {
        if (largs->check) {
            free(largs->C2);
        }
    }
    SEND_ARGUMENT(largs->k, 0);
}
