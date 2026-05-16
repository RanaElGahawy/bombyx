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

unsigned long long todval(struct timeval * tp);
int cilk_rand();
void init_vec(float * V, int n);
double maxerror_vec(float * V1, float * V2, int n0);
void zero(float * A, int n2);
void init(float * A0, int n3);
double maxerror(float * A1, float * B, int n4);
void iter_matmul(float * A2, float * B0, float * C, int n5);
THREAD(rec_matmulAdd);
THREAD(rec_matmul);
void mat_vec_mul(float * A5, float * R, float * P, int m2, int n8, int ld1, int add);
int main(int argc, char ** argv);
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
    float * A3;
    float * B1;
    float * C0;
    int m;
    int n6;
    int p;
    int ld;
);
CLOSURE_DEF(rec_matmul,
    float * A4;
    float * B2;
    float * C1;
    int m0;
    int n7;
    int p0;
    int ld0;
);
CLOSURE_DEF(rec_matmulAdd_cont0,
);
CLOSURE_DEF(rec_matmulAdd_cont1,
    float * A3;
    float * B1;
    float * C0;
    int m;
    int n6;
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
    float * A4;
    float * B2;
    float * C1;
    int m0;
    int n7;
    int p0;
    int ld0;
    int n10;
);
CLOSURE_DEF(rec_matmul_cont2,
);
CLOSURE_DEF(rec_matmul_cont3,
);
CLOSURE_DEF(main_cont0,
    int n9;
    int check;
    int rand_check;
    float * A6;
    float * B3;
    float * C3;
    float * R0;
    float * P1;
    float * P2;
    float * C2;
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




unsigned long long todval(struct timeval * tp) {
    return (((tp->tv_sec * 1000) * 1000) + tp->tv_usec);
}
int cilk_rand() {
    int result;
    rand_nxt = rand_nxt * 1103515245 + 12345;
    result = ((rand_nxt >> 16) % (((unsigned int) 2147483647) + 1));
    return result;
}
void init_vec(float * V, int n) {
    int i;
    for (i = 0;(i < n);(i++)) {
        V[i] = ((float) cilk_rand());
    }
}
double maxerror_vec(float * V1, float * V2, int n0) {
    int i0;
    double err;
    double diff;
    err = 0.;
    for (i0 = 0;(i0 < n0);(i0++)) {
        diff = ((V1[i0] - V2[i0]) / V1[i0]);
        if ((diff < 0)) {
            diff = (-diff);
        }
        if ((diff > err)) {
            err = diff;
        }
    }
    return err;
}
void zero(float * A, int n2) {
    int i1;
    int j;
    for (i1 = 0;(i1 < n2);(i1++)) {
        for (j = 0;(j < n2);(j++)) {
            A[((i1 * n2) + j)] = 0.;
        }
    }
}
void init(float * A0, int n3) {
    int i2;
    int j0;
    for (i2 = 0;(i2 < n3);(i2++)) {
        for (j0 = 0;(j0 < n3);(j0++)) {
            A0[((i2 * n3) + j0)] = ((double) cilk_rand());
        }
    }
}
double maxerror(float * A1, float * B, int n4) {
    int i3;
    int j1;
    double error;
    double diff0;
    error = 0.;
    for (i3 = 0;(i3 < n4);(i3++)) {
        for (j1 = 0;(j1 < n4);(j1++)) {
            diff0 = ((A1[((i3 * n4) + j1)] - B[((i3 * n4) + j1)]) / A1[((i3 * n4) + j1)]);
            if ((diff0 < 0)) {
                diff0 = (-diff0);
            }
            if ((diff0 > error)) {
                error = diff0;
            }
        }
    }
    return error;
}
void iter_matmul(float * A2, float * B0, float * C, int n5) {
    int i4;
    int j2;
    int k;
    float c;
    for (i4 = 0;(i4 < n5);(i4++)) {
        for (k = 0;(k < n5);(k++)) {
            c = 0.;
            for (j2 = 0;(j2 < n5);(j2++)) {
                c = (c + (A2[((i4 * n5) + j2)] * B0[((j2 * n5) + k)]));
            }
            C[((i4 * n5) + k)] = c;
        }
    }
}
THREAD(rec_matmulAdd) {
    int i5;
    int j3;
    int k0;
    float c0;
    int m1;
    int n1;
    int p1;
    rec_matmulAdd_closure *largs = (rec_matmulAdd_closure*)(args.get());
    if ((((largs->m + largs->n6) + largs->p) <= 64)) {
        for (i5 = 0;(i5 < largs->m);(i5++)) {
            for (k0 = 0;(k0 < largs->p);(k0++)) {
                c0 = 0.;
                for (j3 = 0;(j3 < largs->n6);(j3++)) {
                    c0 = (c0 + (largs->A3[((i5 * largs->ld) + j3)] * largs->B1[((j3 * largs->ld) + k0)]));
                }
                largs->C0[i5 * largs->ld + k0] += c0;
            }
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if ((largs->n6 >= largs->p)) {
            if ((largs->m >= largs->n6)) {
                m1 = (largs->m >> 1);
                rec_matmulAdd_cont3_closure SN_rec_matmulAdd_cont3c(largs->k);
                spawn_next<rec_matmulAdd_cont3_closure> SN_rec_matmulAdd_cont3(SN_rec_matmulAdd_cont3c);
                cont sp0k;
                SN_BIND_VOID(SN_rec_matmulAdd_cont3, &sp0k);
                rec_matmulAdd_closure sp0c(sp0k);
                sp0c.A3 = largs->A3;
                sp0c.B1 = largs->B1;
                sp0c.C0 = largs->C0;
                sp0c.m = m1;
                sp0c.n6 = largs->n6;
                sp0c.p = largs->p;
                sp0c.ld = largs->ld;
                spawn<rec_matmulAdd_closure> sp0(sp0c);

                cont sp1k;
                SN_BIND_VOID(SN_rec_matmulAdd_cont3, &sp1k);
                rec_matmulAdd_closure sp1c(sp1k);
                sp1c.A3 = (largs->A3 + (m1 * largs->ld));
                sp1c.B1 = largs->B1;
                sp1c.C0 = (largs->C0 + (m1 * largs->ld));
                sp1c.m = (largs->m - m1);
                sp1c.n6 = largs->n6;
                sp1c.p = largs->p;
                sp1c.ld = largs->ld;
                spawn<rec_matmulAdd_closure> sp1(sp1c);

                // Original sync was here
            } else {
                n1 = (largs->n6 >> 1);
                rec_matmulAdd_cont1_closure SN_rec_matmulAdd_cont1c(largs->k);
                spawn_next<rec_matmulAdd_cont1_closure> SN_rec_matmulAdd_cont1(SN_rec_matmulAdd_cont1c);
                cont sp2k;
                SN_BIND_VOID(SN_rec_matmulAdd_cont1, &sp2k);
                rec_matmulAdd_closure sp2c(sp2k);
                sp2c.A3 = largs->A3;
                sp2c.B1 = largs->B1;
                sp2c.C0 = largs->C0;
                sp2c.m = largs->m;
                sp2c.n6 = n1;
                sp2c.p = largs->p;
                sp2c.ld = largs->ld;
                spawn<rec_matmulAdd_closure> sp2(sp2c);

                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->p = largs->p;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->C0 = largs->C0;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->B1 = largs->B1;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->ld = largs->ld;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->n6 = largs->n6;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->n1 = n1;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->m = largs->m;
                ((rec_matmulAdd_cont1_closure*)SN_rec_matmulAdd_cont1.cls.get())->A3 = largs->A3;
                // Original sync was here
            }
        } else {
            p1 = (largs->p >> 1);
            rec_matmulAdd_cont0_closure SN_rec_matmulAdd_cont0c(largs->k);
            spawn_next<rec_matmulAdd_cont0_closure> SN_rec_matmulAdd_cont0(SN_rec_matmulAdd_cont0c);
            cont sp3k;
            SN_BIND_VOID(SN_rec_matmulAdd_cont0, &sp3k);
            rec_matmulAdd_closure sp3c(sp3k);
            sp3c.A3 = largs->A3;
            sp3c.B1 = largs->B1;
            sp3c.C0 = largs->C0;
            sp3c.m = largs->m;
            sp3c.n6 = largs->n6;
            sp3c.p = p1;
            sp3c.ld = largs->ld;
            spawn<rec_matmulAdd_closure> sp3(sp3c);

            cont sp4k;
            SN_BIND_VOID(SN_rec_matmulAdd_cont0, &sp4k);
            rec_matmulAdd_closure sp4c(sp4k);
            sp4c.A3 = largs->A3;
            sp4c.B1 = (largs->B1 + p1);
            sp4c.C0 = (largs->C0 + p1);
            sp4c.m = largs->m;
            sp4c.n6 = largs->n6;
            sp4c.p = (largs->p - p1);
            sp4c.ld = largs->ld;
            spawn<rec_matmulAdd_closure> sp4(sp4c);

            // Original sync was here
        }
    }
    return;
}
THREAD(rec_matmul) {
    int i6;
    int j4;
    int k1;
    float c1;
    int m10;
    int n10;
    int p10;
    rec_matmul_closure *largs = (rec_matmul_closure*)(args.get());
    if ((((largs->m0 + largs->n7) + largs->p0) <= 64)) {
        for (i6 = 0;(i6 < largs->m0);(i6++)) {
            for (k1 = 0;(k1 < largs->p0);(k1++)) {
                c1 = 0.;
                for (j4 = 0;(j4 < largs->n7);(j4++)) {
                    c1 = (c1 + (largs->A4[((i6 * largs->ld0) + j4)] * largs->B2[((j4 * largs->ld0) + k1)]));
                }
                largs->C1[((i6 * largs->ld0) + k1)] = c1;
            }
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if ((largs->n7 >= largs->p0)) {
            if ((largs->m0 >= largs->n7)) {
                m10 = (largs->m0 >> 1);
                rec_matmul_cont3_closure SN_rec_matmul_cont3c(largs->k);
                spawn_next<rec_matmul_cont3_closure> SN_rec_matmul_cont3(SN_rec_matmul_cont3c);
                cont sp0k;
                SN_BIND_VOID(SN_rec_matmul_cont3, &sp0k);
                rec_matmul_closure sp0c(sp0k);
                sp0c.A4 = largs->A4;
                sp0c.B2 = largs->B2;
                sp0c.C1 = largs->C1;
                sp0c.m0 = m10;
                sp0c.n7 = largs->n7;
                sp0c.p0 = largs->p0;
                sp0c.ld0 = largs->ld0;
                spawn<rec_matmul_closure> sp0(sp0c);

                cont sp1k;
                SN_BIND_VOID(SN_rec_matmul_cont3, &sp1k);
                rec_matmul_closure sp1c(sp1k);
                sp1c.A4 = (largs->A4 + (m10 * largs->ld0));
                sp1c.B2 = largs->B2;
                sp1c.C1 = (largs->C1 + (m10 * largs->ld0));
                sp1c.m0 = (largs->m0 - m10);
                sp1c.n7 = largs->n7;
                sp1c.p0 = largs->p0;
                sp1c.ld0 = largs->ld0;
                spawn<rec_matmul_closure> sp1(sp1c);

                // Original sync was here
            } else {
                n10 = (largs->n7 >> 1);
                rec_matmul_cont1_closure SN_rec_matmul_cont1c(largs->k);
                spawn_next<rec_matmul_cont1_closure> SN_rec_matmul_cont1(SN_rec_matmul_cont1c);
                cont sp2k;
                SN_BIND_VOID(SN_rec_matmul_cont1, &sp2k);
                rec_matmul_closure sp2c(sp2k);
                sp2c.A4 = largs->A4;
                sp2c.B2 = largs->B2;
                sp2c.C1 = largs->C1;
                sp2c.m0 = largs->m0;
                sp2c.n7 = n10;
                sp2c.p0 = largs->p0;
                sp2c.ld0 = largs->ld0;
                spawn<rec_matmul_closure> sp2(sp2c);

                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->ld0 = largs->ld0;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->p0 = largs->p0;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->n7 = largs->n7;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->m0 = largs->m0;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->C1 = largs->C1;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->B2 = largs->B2;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->n10 = n10;
                ((rec_matmul_cont1_closure*)SN_rec_matmul_cont1.cls.get())->A4 = largs->A4;
                // Original sync was here
            }
        } else {
            p10 = (largs->p0 >> 1);
            rec_matmul_cont0_closure SN_rec_matmul_cont0c(largs->k);
            spawn_next<rec_matmul_cont0_closure> SN_rec_matmul_cont0(SN_rec_matmul_cont0c);
            cont sp3k;
            SN_BIND_VOID(SN_rec_matmul_cont0, &sp3k);
            rec_matmul_closure sp3c(sp3k);
            sp3c.A4 = largs->A4;
            sp3c.B2 = largs->B2;
            sp3c.C1 = largs->C1;
            sp3c.m0 = largs->m0;
            sp3c.n7 = largs->n7;
            sp3c.p0 = p10;
            sp3c.ld0 = largs->ld0;
            spawn<rec_matmul_closure> sp3(sp3c);

            cont sp4k;
            SN_BIND_VOID(SN_rec_matmul_cont0, &sp4k);
            rec_matmul_closure sp4c(sp4k);
            sp4c.A4 = largs->A4;
            sp4c.B2 = (largs->B2 + p10);
            sp4c.C1 = (largs->C1 + p10);
            sp4c.m0 = largs->m0;
            sp4c.n7 = largs->n7;
            sp4c.p0 = (largs->p0 - p10);
            sp4c.ld0 = largs->ld0;
            spawn<rec_matmul_closure> sp4(sp4c);

            // Original sync was here
        }
    }
    return;
}
void mat_vec_mul(float * A5, float * R, float * P, int m2, int n8, int ld1, int add) {
    int i7;
    int j5;
    float c2;
    float c3;
    int m11;
    int n11;
    if (((m2 + n8) <= 64)) {
        if (add) {
            for (i7 = 0;(i7 < m2);(i7++)) {
                c2 = 0;
                for (j5 = 0;(j5 < n8);(j5++)) {
                    c2 = (c2 + (A5[((i7 * ld1) + j5)] * R[j5]));
                }
                P[i7] += c2;
            }
        } else {
            for (i7 = 0;(i7 < m2);(i7++)) {
                c3 = 0;
                for (j5 = 0;(j5 < n8);(j5++)) {
                    c3 = (c3 + (A5[((i7 * ld1) + j5)] * R[j5]));
                }
                P[i7] = c3;
            }
        }
    } else {
        if ((m2 >= n8)) {
            m11 = (m2 >> 1);
            mat_vec_mul(A5,R,P,m11,n8,ld1,add);
            mat_vec_mul((A5 + (m11 * ld1)),R,(P + m11),(m2 - m11),n8,ld1,add);
        } else {
            n11 = (n8 >> 1);
            mat_vec_mul(A5,R,P,m2,n11,ld1,add);
            mat_vec_mul((A5 + n11),(R + n11),P,m2,(n8 - n11),ld1,1);
        }
    }
}
int main(int argc, char ** argv) {
    int n9;
    int check;
    int rand_check;
    int help;
    float * A6;
    float * B3;
    float * C3;
    float * R0;
    float * P1;
    float * P2;
    float * C2;
    struct timeval t1;
    n9 = 1024;
    check = 0;
    rand_check = 0;
    help = 0;
    get_options(argc,argv,specifiers,opt_types,&(n9),&(check),&(rand_check),&(help));
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    if (help) {
        fprintf(__stderrp,"Usage: matmul [-n size] [-c] [-rc] [-h] [<cilk options>]\n");
        fprintf(__stderrp,"if -c is set, check result against iterative matrix multiply O(n^3).\n");
        fprintf(__stderrp,"if -rc is set, check result against randomlized algo. due to Freivalds O(n^2).\n");
        exit(1);
    }
    A6 = ((float *) malloc(((n9 * n9) * sizeof(float))));
    B3 = ((float *) malloc(((n9 * n9) * sizeof(float))));
    C3 = ((float *) malloc(((n9 * n9) * sizeof(float))));
    if (rand_check) {
        R0 = ((float *) malloc((n9 * sizeof(float))));
        P1 = ((float *) malloc((n9 * sizeof(float))));
        P2 = ((float *) malloc((n9 * sizeof(float))));
        init_vec(R0,n9);
    } else {
        if (check) {
            C2 = ((float *) malloc(((n9 * n9) * sizeof(float))));
            zero(C2,n9);
        }
    }
    init(A6,n9);
    init(B3,n9);
    fprintf(__stderrp,"\nCalculate using recursive method ... (timing start here)\n");
    zero(C3,n9);
    gettimeofday(&(t1),0);
    cont sp0k;
    SN_BIND_VOID(SN_main_cont0, &sp0k);
    rec_matmul_closure sp0c(sp0k);
    sp0c.A4 = A6;
    sp0c.B2 = B3;
    sp0c.C1 = C3;
    sp0c.m0 = n9;
    sp0c.n7 = n9;
    sp0c.p0 = n9;
    sp0c.ld0 = n9;
    spawn<rec_matmul_closure> sp0(sp0c);

    ((main_cont0_closure*)SN_main_cont0.cls.get())->P2 = P2;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->R0 = R0;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->P1 = P1;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->C3 = C3;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->B3 = B3;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->A6 = A6;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->rand_check = rand_check;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->t1 = t1;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->C2 = C2;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->check = check;
    ((main_cont0_closure*)SN_main_cont0.cls.get())->n9 = n9;
    // Original sync was here
    return 0;
}
THREAD(rec_matmulAdd_cont0) {
    rec_matmulAdd_cont0_closure *largs = (rec_matmulAdd_cont0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(rec_matmulAdd_cont1) {
    rec_matmulAdd_cont1_closure *largs = (rec_matmulAdd_cont1_closure*)(args.get());
    rec_matmulAdd_cont2_closure SN_rec_matmulAdd_cont2c(largs->k);
    spawn_next<rec_matmulAdd_cont2_closure> SN_rec_matmulAdd_cont2(SN_rec_matmulAdd_cont2c);
    cont sp0k;
    SN_BIND_VOID(SN_rec_matmulAdd_cont2, &sp0k);
    rec_matmulAdd_closure sp0c(sp0k);
    sp0c.A3 = (largs->A3 + largs->n1);
    sp0c.B1 = (largs->B1 + (largs->n1 * largs->ld));
    sp0c.C0 = largs->C0;
    sp0c.m = largs->m;
    sp0c.n6 = (largs->n6 - largs->n1);
    sp0c.p = largs->p;
    sp0c.ld = largs->ld;
    spawn<rec_matmulAdd_closure> sp0(sp0c);

    // Original sync was here
    return;
}
THREAD(rec_matmulAdd_cont2) {
    rec_matmulAdd_cont2_closure *largs = (rec_matmulAdd_cont2_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(rec_matmulAdd_cont3) {
    rec_matmulAdd_cont3_closure *largs = (rec_matmulAdd_cont3_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(rec_matmul_cont0) {
    rec_matmul_cont0_closure *largs = (rec_matmul_cont0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(rec_matmul_cont1) {
    rec_matmul_cont1_closure *largs = (rec_matmul_cont1_closure*)(args.get());
    rec_matmul_cont2_closure SN_rec_matmul_cont2c(largs->k);
    spawn_next<rec_matmul_cont2_closure> SN_rec_matmul_cont2(SN_rec_matmul_cont2c);
    cont sp0k;
    SN_BIND_VOID(SN_rec_matmul_cont2, &sp0k);
    rec_matmulAdd_closure sp0c(sp0k);
    sp0c.A3 = (largs->A4 + largs->n10);
    sp0c.B1 = (largs->B2 + (largs->n10 * largs->ld0));
    sp0c.C0 = largs->C1;
    sp0c.m = largs->m0;
    sp0c.n6 = (largs->n7 - largs->n10);
    sp0c.p = largs->p0;
    sp0c.ld = largs->ld0;
    spawn<rec_matmulAdd_closure> sp0(sp0c);

    // Original sync was here
    return;
}
THREAD(rec_matmul_cont2) {
    rec_matmul_cont2_closure *largs = (rec_matmul_cont2_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(rec_matmul_cont3) {
    rec_matmul_cont3_closure *largs = (rec_matmul_cont3_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(main_cont0) {
    double err0;
    unsigned long long runtime_ms;
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    printf("%f\n",(runtime_ms / 1000.));
    if (largs->rand_check) {
        mat_vec_mul(largs->B3,largs->R0,largs->P1,largs->n9,largs->n9,largs->n9,0);
        mat_vec_mul(largs->A6,largs->P1,largs->P2,largs->n9,largs->n9,largs->n9,0);
        mat_vec_mul(largs->C3,largs->R0,largs->P1,largs->n9,largs->n9,largs->n9,0);
        err0 = maxerror_vec(largs->P1,largs->P2,largs->n9);
        fprintf(__stderrp,"Max error     = %g\n",err0);
    } else {
        if (largs->check) {
            iter_matmul(largs->A6,largs->B3,largs->C2,largs->n9);
            err0 = maxerror(largs->C3,largs->C2,largs->n9);
            fprintf(__stderrp,"Max error     = %g\n",err0);
        }
    }
    fprintf(__stderrp,"\nCilk Example: matmul\n");
    fprintf(__stderrp,"Options: size = %d\n",largs->n9);
    free(largs->C3);
    free(largs->B3);
    free(largs->A6);
    if (largs->rand_check) {
        free(largs->R0);
        free(largs->P1);
        free(largs->P2);
    } else {
        if (largs->check) {
            free(largs->C2);
        }
    }
    SEND_ARGUMENT(largs->k, 0);
    return;
}
