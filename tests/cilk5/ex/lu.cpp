#include "cilk_explicit.hh"
/****************************************************************************\
 * LU decomposition - Cilk.
 * lu.cilk
 * Robert Blumofe
 *
 * Copyright (c) 1996, Robert Blumofe.  All rights reserved.
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
\****************************************************************************/
#include <cilk/cilk.h>

#include "getoptions.h"
#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#if CILKSAN
#include "cilksan.h"
#endif

/* Define the size of a block. */
#ifndef BLOCK_SIZE
#define BLOCK_SIZE 16
#endif

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

/* Define the default matrix size. */
#ifndef DEFAULT_SIZE
#define DEFAULT_SIZE (16 * BLOCK_SIZE)
#endif

/* A block is a 2D array of doubles. */
typedef double Block[BLOCK_SIZE][BLOCK_SIZE];
#define BLOCK(B, I, J) (B[I][J])

/* A matrix is a 1D array of blocks. */
typedef Block *Matrix;
#define MATRIX(M, I, J) ((M)[(I) * nBlocks + (J)])

/* Matrix size in blocks. */
static int nBlocks;

void elem_daxmy(double a, double *x, double *y, int n);
void block_lu(Block B);
void block_lower_solve(Block B, Block L);
void block_upper_solve(Block B, Block U);
void block_schur(Block B, Block A, Block C);
THREAD(schur);
THREAD(aux_lower_solve);
THREAD(lower_solve);
THREAD(aux_upper_solve);
void upper_solve(Matrix M, Matrix U, int nb);
void lu(Matrix M, int nb);
THREAD(schur_cont0);
THREAD(schur_cont1);
THREAD(schur_cont2);
THREAD(schur_cont3);
THREAD(aux_lower_solve_cont0);
THREAD(aux_lower_solve_cont1);
THREAD(aux_lower_solve_cont2);
THREAD(lower_solve_cont0);
THREAD(lower_solve_cont1);
THREAD(aux_upper_solve_cont0);
THREAD(upper_solve_cont0);
THREAD(upper_solve_cont1);
THREAD(lu_cont0);
THREAD(lu_cont1);

CLOSURE_DEF(schur,
    Matrix M;
    Matrix V;
    Matrix W;
    int nb;
);
CLOSURE_DEF(aux_lower_solve,
    Matrix Ma;
    Matrix Mb;
    Matrix L;
    int nb;
);
CLOSURE_DEF(lower_solve,
    Matrix M;
    Matrix L;
    int nb;
);
CLOSURE_DEF(aux_upper_solve,
    Matrix Ma;
    Matrix Mb;
    Matrix U;
    int nb;
);
CLOSURE_DEF(schur_cont0,
    Matrix M00;
    Matrix M01;
    Matrix M10;
    Matrix M11;
    Matrix V01;
    Matrix V11;
    Matrix W10;
    Matrix W11;
    int hnb;
);
CLOSURE_DEF(schur_cont1,
    Matrix M00;
    Matrix M01;
    Matrix M10;
    Matrix M11;
    Matrix V01;
    Matrix V11;
    Matrix W10;
    Matrix W11;
    int hnb;
);
CLOSURE_DEF(schur_cont2,
);
CLOSURE_DEF(schur_cont3,
);
CLOSURE_DEF(aux_lower_solve_cont0,
    Matrix Ma;
    Matrix Mb;
    int nb;
    Matrix L10;
    Matrix L11;
);
CLOSURE_DEF(aux_lower_solve_cont1,
    Matrix Mb;
    int nb;
    Matrix L11;
);
CLOSURE_DEF(aux_lower_solve_cont2,
);
CLOSURE_DEF(lower_solve_cont0,
);
CLOSURE_DEF(lower_solve_cont1,
);
CLOSURE_DEF(aux_upper_solve_cont0,
    Matrix Mb;
    int nb;
    Matrix U11;
);
CLOSURE_DEF(upper_solve_cont0,
);
CLOSURE_DEF(upper_solve_cont1,
);
CLOSURE_DEF(lu_cont0,
    Matrix M01;
    Matrix M10;
    Matrix M11;
    int hnb;
);
CLOSURE_DEF(lu_cont1,
    Matrix M11;
    int hnb;
);
unsigned long long todval(struct timeval *tp) {
  return tp->tv_sec * 1000 * 1000 + tp->tv_usec;
}
/****************************************************************************\
 * Utility routines.
\****************************************************************************/

/*
 * init_matrix - Fill in matrix M with random values.
 */
static void init_matrix(Matrix M, int nb) {

  int I, J, K, i, j, k;

  /* Initialize random number generator. */
  srand(1);

  /* For each element of each block, fill in random value. */
  for (I = 0; I < nb; I++)
    for (J = 0; J < nb; J++)
      for (i = 0; i < BLOCK_SIZE; i++)
        for (j = 0; j < BLOCK_SIZE; j++)
          BLOCK(MATRIX(M, I, J), i, j) = ((double)rand()) / (double)RAND_MAX;

  /* Inflate diagonal entries. */
  for (K = 0; K < nb; K++)
    for (k = 0; k < BLOCK_SIZE; k++)
      BLOCK(MATRIX(M, K, K), k, k) *= 10.0;
}

/*
 * print_matrix - Print matrix M.
 */
static void print_matrix(Matrix M, int nb) {

  int i, j;

  /* Print out matrix. */
  for (i = 0; i < nb * BLOCK_SIZE; i++) {
    for (j = 0; j < nb * BLOCK_SIZE; j++)
      printf(" %6.4f", BLOCK(MATRIX(M, i / BLOCK_SIZE, j / BLOCK_SIZE),
                             i % BLOCK_SIZE, j % BLOCK_SIZE));
    printf("\n");
  }
}

/*
 * test_result - Check that matrix LU contains LU decomposition of M.
 */
static int test_result(Matrix LU, Matrix M, int nb) {

  int I, J, K, i, j, k;
  double diff, max_diff;
  double v;

  /* Initialize test. */
  max_diff = 0.0;

  printf("Now check result ...\n");

  /* Find maximum difference between any element of LU and M. */
  for (i = 0; i < nb * BLOCK_SIZE; i++)
    for (j = 0; j < nb * BLOCK_SIZE; j++) {
      I = i / BLOCK_SIZE;
      J = j / BLOCK_SIZE;
      v = 0.0;
      for (k = 0; k < i && k <= j; k++) {
        K = k / BLOCK_SIZE;
        v += BLOCK(MATRIX(LU, I, K), i % BLOCK_SIZE, k % BLOCK_SIZE) *
             BLOCK(MATRIX(LU, K, J), k % BLOCK_SIZE, j % BLOCK_SIZE);
      }
      if (k == i && k <= j) {
        K = k / BLOCK_SIZE;
        v += BLOCK(MATRIX(LU, K, J), k % BLOCK_SIZE, j % BLOCK_SIZE);
      }
      diff = fabs(BLOCK(MATRIX(M, I, J), i % BLOCK_SIZE, j % BLOCK_SIZE) - v);
      if (diff > 0.000001)
        return 0;
    }

  return 1;
}

/*
 * count_flops - Return number of flops to perform LU decomposition.  Unused.
 *
static double count_flops(int n) {
  return ((4.0 * (double) n - 3.0) * (double) n - 1.0) * (double) n / 6.0;
}
*/

/****************************************************************************\
 * Element operations.
 \****************************************************************************/
/*
 * elem_daxmy - Compute y' = y - ax where a is a double and x and y are
 * vectors of doubles.
 */


/****************************************************************************\
 * Block operations.
 \****************************************************************************/

/*
 * block_lu - Factor block B.
 */


/*
 * block_lower_solve - Perform forward substitution to solve for B' in
 * LB' = B.
 */


/*
 * block_upper_solve - Perform forward substitution to solve for B' in
 * B'U = B.
 */


/*
 * block_schur - Compute Schur complement B' = B - AC.
 */


/****************************************************************************\
 * Divide-and-conquer matrix LU decomposition.
 \****************************************************************************/

/*
 * schur - Compute M' = M - VW.
 */



/*
 * lower_solve - Compute M' where LM' = M.
 */

void lower_solve(Matrix M, Matrix L, int nb);





/*
 * upper_solve - Compute M' where M'U = M.
 */

void upper_solve(Matrix M, Matrix U, int nb);





/*
 * lu - Perform LU decomposition of matrix M.
 */



/****************************************************************************\
 * Mainline.
 \****************************************************************************/

/*
 * check_input - Check that the input is valid.
 */
int usage(void) {

  printf("\nUsage: lu <options>\n\n");
  printf("Options:\n");
  printf("  -n N : Decompose NxN matrix, where N is at least 16 "
         "and power of 2.\n");
  printf("  -o   : Print matrix before and after decompose.\n");
  printf("  -c   : Check result.\n\n");
  printf("Default: lu -n %d\n\n", DEFAULT_SIZE);

  return 1;
}

int invalid_input(int n) {

  int v = n;

  /* Check that matrix is not smaller than a block. */
  if (n < BLOCK_SIZE)
    return usage();

  /* Check that matrix is power-of-2 sized. */
  while (!((unsigned)v & (unsigned)1))
    v >>= 1;
  if (v != 1)
    return usage();

  return 0;
}

/*
 * main
 */

const char *specifiers[] = {"-n", "-o", "-c", "-benchmark", "-h", 0};
int opt_types[] = {INTARG, BOOLARG, BOOLARG, BENCHMARK, BOOLARG, 0};

int main(int argc, char *argv[]) {

  int print, test, n, benchmark, help, failed;
  Matrix M, Msave = 0;

  n = DEFAULT_SIZE;
  print = 0;
  test = 0;

  /* Parse arguments. */
  get_options(argc, argv, specifiers, opt_types, &n, &print, &test, &benchmark,
              &help);

  if (help)
    return usage();

  if (benchmark) {
    switch (benchmark) {
    case 1: /* short benchmark options -- a little work */
      n = 16;
      break;
    case 2: /* standard benchmark options */
      n = DEFAULT_SIZE;
      break;
    case 3: /* long benchmark options -- a lot of work */
      n = 2048;
      break;
    }
  }

  if (invalid_input(n))
    return 1;
  nBlocks = n / BLOCK_SIZE;

  /* Allocate matrix. */
  M = (Matrix)malloc(n * n * sizeof(double));
  if (!M) {
    fprintf(stderr, "Allocation failed.\n");
    return 1;
  }

  /* Initialize matrix. */
  init_matrix(M, nBlocks);

  if (print)
    print_matrix(M, nBlocks);

  Msave = (Matrix)malloc(n * n * sizeof(double));
  if (!Msave) {
    fprintf(stderr, "Allocation failed.\n");
    return 1;
  }
  memcpy((void *)Msave, (void *)M, n * n * sizeof(double));

  struct timeval t1, t2;
  gettimeofday(&t1, 0);
  lu(M, nBlocks);

  gettimeofday(&t2, 0);
  unsigned long long runtime_ms = (todval(&t2) - todval(&t1)) / 1000;
  printf("%f\n", runtime_ms / 1000.0);

  /* Test result. */
  if (print)
    print_matrix(M, nBlocks);

  failed = ((test) && (!(test_result(M, Msave, nBlocks))));

  if (failed)
    printf("WRONG ANSWER!\n");
  else {
    fprintf(stderr, "\nCilk Example: lu\n");
    fprintf(stderr, "Options: (n x n matrix) n = %d\n\n", n);
  }

  /* Free matrix. */
  free(M);
  free(Msave);

  return 0;
}

void elem_daxmy(double a, double *x, double *y, int n) {
    for ((n--);(n >= 0);(n--)) {
        y[n] -= a * x[n];
    }
}
void block_lu(Block B) {
    int i;
    int k0;
    for (k0 = 0;(k0 < 16);(k0++)) {
        for (i = (k0 + 1);(i < 16);(i++)) {
            (B[i][k0]) /= (B[k0][k0]);
            elem_daxmy(B[i][k0],&(B[k0][(k0 + 1)]),&(B[i][(k0 + 1)]),((16 - k0) - 1));
        }
    }
}
void block_lower_solve(Block B, Block L) {
    int i;
    int k0;
    for (i = 1;(i < 16);(i++)) {
        for (k0 = 0;(k0 < i);(k0++)) {
            elem_daxmy(L[i][k0],&(B[k0][0]),&(B[i][0]),16);
        }
    }
}
void block_upper_solve(Block B, Block U) {
    int i;
    int k0;
    for (i = 0;(i < 16);(i++)) {
        for (k0 = 0;(k0 < 16);(k0++)) {
            (B[i][k0]) /= (U[k0][k0]);
            elem_daxmy(B[i][k0],&(U[k0][(k0 + 1)]),&(B[i][(k0 + 1)]),((16 - k0) - 1));
        }
    }
}
void block_schur(Block B, Block A, Block C) {
    int i;
    int k0;
    for (i = 0;(i < 16);(i++)) {
        for (k0 = 0;(k0 < 16);(k0++)) {
            elem_daxmy(A[i][k0],&(C[k0][0]),&(B[i][0]),16);
        }
    }
}
THREAD(schur) {
    Matrix M00;
    Matrix M01;
    Matrix M10;
    Matrix M11;
    Matrix V00;
    Matrix V01;
    Matrix V10;
    Matrix V11;
    Matrix W00;
    Matrix W01;
    Matrix W10;
    Matrix W11;
    int hnb;
    schur_closure *largs = (schur_closure*)(args.get());
    if ((largs->nb == 1)) {
        block_schur(*(largs->M),*(largs->V),*(largs->W));
        SEND_ARGUMENT(largs->k, 0);
    } else {
        hnb = (largs->nb / 2);
        M00 = &(largs->M[((0 * nBlocks) + 0)]);
        M01 = &(largs->M[((0 * nBlocks) + hnb)]);
        M10 = &(largs->M[((hnb * nBlocks) + 0)]);
        M11 = &(largs->M[((hnb * nBlocks) + hnb)]);
        V00 = &(largs->V[((0 * nBlocks) + 0)]);
        V01 = &(largs->V[((0 * nBlocks) + hnb)]);
        V10 = &(largs->V[((hnb * nBlocks) + 0)]);
        V11 = &(largs->V[((hnb * nBlocks) + hnb)]);
        W00 = &(largs->W[((0 * nBlocks) + 0)]);
        W01 = &(largs->W[((0 * nBlocks) + hnb)]);
        W10 = &(largs->W[((hnb * nBlocks) + 0)]);
        W11 = &(largs->W[((hnb * nBlocks) + hnb)]);
        schur_cont0_closure SN_schur_cont0c(largs->k);
        spawn_next<schur_cont0_closure> SN_schur_cont0(SN_schur_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_schur_cont0, &sp0k);
        schur_closure sp0c(sp0k);
        sp0c.M = M00;
        sp0c.V = V00;
        sp0c.W = W00;
        sp0c.nb = hnb;
        spawn<schur_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_schur_cont0, &sp1k);
        schur_closure sp1c(sp1k);
        sp1c.M = M01;
        sp1c.V = V00;
        sp1c.W = W01;
        sp1c.nb = hnb;
        spawn<schur_closure> sp1(sp1c);

        cont sp2k;
        SN_BIND_VOID(SN_schur_cont0, &sp2k);
        schur_closure sp2c(sp2k);
        sp2c.M = M10;
        sp2c.V = V10;
        sp2c.W = W00;
        sp2c.nb = hnb;
        spawn<schur_closure> sp2(sp2c);

        cont sp3k;
        SN_BIND_VOID(SN_schur_cont0, &sp3k);
        schur_closure sp3c(sp3k);
        sp3c.M = M11;
        sp3c.V = V10;
        sp3c.W = W01;
        sp3c.nb = hnb;
        spawn<schur_closure> sp3(sp3c);

        ((schur_cont0_closure*)SN_schur_cont0.cls.get())->hnb = hnb;
        ((schur_cont0_closure*)SN_schur_cont0.cls.get())->W11 = W11;
        ((schur_cont0_closure*)SN_schur_cont0.cls.get())->M11 = M11;
        ((schur_cont0_closure*)SN_schur_cont0.cls.get())->M10 = M10;
        ((schur_cont0_closure*)SN_schur_cont0.cls.get())->V11 = V11;
        ((schur_cont0_closure*)SN_schur_cont0.cls.get())->V01 = V01;
        ((schur_cont0_closure*)SN_schur_cont0.cls.get())->W10 = W10;
        ((schur_cont0_closure*)SN_schur_cont0.cls.get())->M01 = M01;
        ((schur_cont0_closure*)SN_schur_cont0.cls.get())->M00 = M00;
        // Original sync was here
    }
}
THREAD(aux_lower_solve) {
    Matrix L00;
    Matrix L01;
    Matrix L10;
    Matrix L11;
    aux_lower_solve_closure *largs = (aux_lower_solve_closure*)(args.get());
    L00 = &(largs->L[((0 * nBlocks) + 0)]);
    L01 = &(largs->L[((0 * nBlocks) + largs->nb)]);
    L10 = &(largs->L[((largs->nb * nBlocks) + 0)]);
    L11 = &(largs->L[((largs->nb * nBlocks) + largs->nb)]);
    aux_lower_solve_cont0_closure SN_aux_lower_solve_cont0c(largs->k);
    spawn_next<aux_lower_solve_cont0_closure> SN_aux_lower_solve_cont0(SN_aux_lower_solve_cont0c);
    cont sp0k;
    SN_BIND_VOID(SN_aux_lower_solve_cont0, &sp0k);
    lower_solve_closure sp0c(sp0k);
    sp0c.M = largs->Ma;
    sp0c.L = L00;
    sp0c.nb = largs->nb;
    spawn<lower_solve_closure> sp0(sp0c);

    ((aux_lower_solve_cont0_closure*)SN_aux_lower_solve_cont0.cls.get())->L11 = L11;
    ((aux_lower_solve_cont0_closure*)SN_aux_lower_solve_cont0.cls.get())->L10 = L10;
    ((aux_lower_solve_cont0_closure*)SN_aux_lower_solve_cont0.cls.get())->nb = largs->nb;
    ((aux_lower_solve_cont0_closure*)SN_aux_lower_solve_cont0.cls.get())->Mb = largs->Mb;
    ((aux_lower_solve_cont0_closure*)SN_aux_lower_solve_cont0.cls.get())->Ma = largs->Ma;
    // Original sync was here
    return;
}
THREAD(lower_solve) {
    Matrix M00;
    Matrix M01;
    Matrix M10;
    Matrix M11;
    int hnb;
    lower_solve_closure *largs = (lower_solve_closure*)(args.get());
    if ((largs->nb == 1)) {
        block_lower_solve(*(largs->M),*(largs->L));
        SEND_ARGUMENT(largs->k, 0);
    } else {
        hnb = (largs->nb / 2);
        M00 = &(largs->M[((0 * nBlocks) + 0)]);
        M01 = &(largs->M[((0 * nBlocks) + hnb)]);
        M10 = &(largs->M[((hnb * nBlocks) + 0)]);
        M11 = &(largs->M[((hnb * nBlocks) + hnb)]);
        lower_solve_cont0_closure SN_lower_solve_cont0c(largs->k);
        spawn_next<lower_solve_cont0_closure> SN_lower_solve_cont0(SN_lower_solve_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_lower_solve_cont0, &sp0k);
        aux_lower_solve_closure sp0c(sp0k);
        sp0c.Ma = M00;
        sp0c.Mb = M10;
        sp0c.L = largs->L;
        sp0c.nb = hnb;
        spawn<aux_lower_solve_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_lower_solve_cont0, &sp1k);
        aux_lower_solve_closure sp1c(sp1k);
        sp1c.Ma = M01;
        sp1c.Mb = M11;
        sp1c.L = largs->L;
        sp1c.nb = hnb;
        spawn<aux_lower_solve_closure> sp1(sp1c);

        // Original sync was here
    }
}
THREAD(aux_upper_solve) {
    Matrix U00;
    Matrix U01;
    Matrix U10;
    Matrix U11;
    aux_upper_solve_closure *largs = (aux_upper_solve_closure*)(args.get());
    U00 = &(largs->U[((0 * nBlocks) + 0)]);
    U01 = &(largs->U[((0 * nBlocks) + largs->nb)]);
    U10 = &(largs->U[((largs->nb * nBlocks) + 0)]);
    U11 = &(largs->U[((largs->nb * nBlocks) + largs->nb)]);
    upper_solve(largs->Ma,U00,largs->nb);
    aux_upper_solve_cont0_closure SN_aux_upper_solve_cont0c(largs->k);
    spawn_next<aux_upper_solve_cont0_closure> SN_aux_upper_solve_cont0(SN_aux_upper_solve_cont0c);
    cont sp0k;
    SN_BIND_VOID(SN_aux_upper_solve_cont0, &sp0k);
    schur_closure sp0c(sp0k);
    sp0c.M = largs->Mb;
    sp0c.V = largs->Ma;
    sp0c.W = U01;
    sp0c.nb = largs->nb;
    spawn<schur_closure> sp0(sp0c);

    ((aux_upper_solve_cont0_closure*)SN_aux_upper_solve_cont0.cls.get())->U11 = U11;
    ((aux_upper_solve_cont0_closure*)SN_aux_upper_solve_cont0.cls.get())->nb = largs->nb;
    ((aux_upper_solve_cont0_closure*)SN_aux_upper_solve_cont0.cls.get())->Mb = largs->Mb;
    // Original sync was here
    return;
}
void upper_solve(Matrix M, Matrix U, int nb) {
    Matrix M00;
    Matrix M01;
    Matrix M10;
    Matrix M11;
    int hnb;
    if ((nb == 1)) {
        block_upper_solve(*(M),*(U));
        return ;
    } else {
        hnb = (nb / 2);
        M00 = &(M[((0 * nBlocks) + 0)]);
        M01 = &(M[((0 * nBlocks) + hnb)]);
        M10 = &(M[((hnb * nBlocks) + 0)]);
        M11 = &(M[((hnb * nBlocks) + hnb)]);
        upper_solve_cont0_closure SN_upper_solve_cont0c(CONT_DUMMY);
        spawn_next<upper_solve_cont0_closure> SN_upper_solve_cont0(SN_upper_solve_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_upper_solve_cont0, &sp0k);
        aux_upper_solve_closure sp0c(sp0k);
        sp0c.Ma = M00;
        sp0c.Mb = M01;
        sp0c.U = U;
        sp0c.nb = hnb;
        spawn<aux_upper_solve_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_upper_solve_cont0, &sp1k);
        aux_upper_solve_closure sp1c(sp1k);
        sp1c.Ma = M10;
        sp1c.Mb = M11;
        sp1c.U = U;
        sp1c.nb = hnb;
        spawn<aux_upper_solve_closure> sp1(sp1c);

        // Original sync was here
    }
}
void lu(Matrix M, int nb) {
    Matrix M00;
    Matrix M01;
    Matrix M10;
    Matrix M11;
    int hnb;
    if ((nb == 1)) {
        block_lu(*(M));
        return ;
    } else {
        hnb = (nb / 2);
        M00 = &(M[((0 * nBlocks) + 0)]);
        M01 = &(M[((0 * nBlocks) + hnb)]);
        M10 = &(M[((hnb * nBlocks) + 0)]);
        M11 = &(M[((hnb * nBlocks) + hnb)]);
        lu(M00,hnb);
        lu_cont0_closure SN_lu_cont0c(CONT_DUMMY);
        spawn_next<lu_cont0_closure> SN_lu_cont0(SN_lu_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_lu_cont0, &sp0k);
        lower_solve_closure sp0c(sp0k);
        sp0c.M = M01;
        sp0c.L = M00;
        sp0c.nb = hnb;
        spawn<lower_solve_closure> sp0(sp0c);

        upper_solve(M10,M00,hnb);
        ((lu_cont0_closure*)SN_lu_cont0.cls.get())->M11 = M11;
        ((lu_cont0_closure*)SN_lu_cont0.cls.get())->M10 = M10;
        ((lu_cont0_closure*)SN_lu_cont0.cls.get())->hnb = hnb;
        ((lu_cont0_closure*)SN_lu_cont0.cls.get())->M01 = M01;
        // Original sync was here
    }
}
THREAD(schur_cont0) {
    schur_cont0_closure *largs = (schur_cont0_closure*)(args.get());
    schur_cont1_closure SN_schur_cont1c(largs->k);
    spawn_next<schur_cont1_closure> SN_schur_cont1(SN_schur_cont1c);
    ((schur_cont1_closure*)SN_schur_cont1.cls.get())->W10 = largs->W10;
    ((schur_cont1_closure*)SN_schur_cont1.cls.get())->V11 = largs->V11;
    ((schur_cont1_closure*)SN_schur_cont1.cls.get())->M00 = largs->M00;
    ((schur_cont1_closure*)SN_schur_cont1.cls.get())->V01 = largs->V01;
    ((schur_cont1_closure*)SN_schur_cont1.cls.get())->M10 = largs->M10;
    ((schur_cont1_closure*)SN_schur_cont1.cls.get())->M01 = largs->M01;
    ((schur_cont1_closure*)SN_schur_cont1.cls.get())->hnb = largs->hnb;
    ((schur_cont1_closure*)SN_schur_cont1.cls.get())->W11 = largs->W11;
    ((schur_cont1_closure*)SN_schur_cont1.cls.get())->M11 = largs->M11;
    // Original sync was here
    return;
}
THREAD(schur_cont1) {
    schur_cont1_closure *largs = (schur_cont1_closure*)(args.get());
    schur_cont2_closure SN_schur_cont2c(largs->k);
    spawn_next<schur_cont2_closure> SN_schur_cont2(SN_schur_cont2c);
    cont sp0k;
    SN_BIND_VOID(SN_schur_cont2, &sp0k);
    schur_closure sp0c(sp0k);
    sp0c.M = largs->M00;
    sp0c.V = largs->V01;
    sp0c.W = largs->W10;
    sp0c.nb = largs->hnb;
    spawn<schur_closure> sp0(sp0c);

    cont sp1k;
    SN_BIND_VOID(SN_schur_cont2, &sp1k);
    schur_closure sp1c(sp1k);
    sp1c.M = largs->M01;
    sp1c.V = largs->V01;
    sp1c.W = largs->W11;
    sp1c.nb = largs->hnb;
    spawn<schur_closure> sp1(sp1c);

    cont sp2k;
    SN_BIND_VOID(SN_schur_cont2, &sp2k);
    schur_closure sp2c(sp2k);
    sp2c.M = largs->M10;
    sp2c.V = largs->V11;
    sp2c.W = largs->W10;
    sp2c.nb = largs->hnb;
    spawn<schur_closure> sp2(sp2c);

    cont sp3k;
    SN_BIND_VOID(SN_schur_cont2, &sp3k);
    schur_closure sp3c(sp3k);
    sp3c.M = largs->M11;
    sp3c.V = largs->V11;
    sp3c.W = largs->W11;
    sp3c.nb = largs->hnb;
    spawn<schur_closure> sp3(sp3c);

    // Original sync was here
    return;
}
THREAD(schur_cont2) {
    schur_cont2_closure *largs = (schur_cont2_closure*)(args.get());
    schur_cont3_closure SN_schur_cont3c(largs->k);
    spawn_next<schur_cont3_closure> SN_schur_cont3(SN_schur_cont3c);
    // Original sync was here
    return;
}
THREAD(schur_cont3) {
    schur_cont3_closure *largs = (schur_cont3_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(aux_lower_solve_cont0) {
    aux_lower_solve_cont0_closure *largs = (aux_lower_solve_cont0_closure*)(args.get());
    aux_lower_solve_cont1_closure SN_aux_lower_solve_cont1c(largs->k);
    spawn_next<aux_lower_solve_cont1_closure> SN_aux_lower_solve_cont1(SN_aux_lower_solve_cont1c);
    cont sp0k;
    SN_BIND_VOID(SN_aux_lower_solve_cont1, &sp0k);
    schur_closure sp0c(sp0k);
    sp0c.M = largs->Mb;
    sp0c.V = largs->L10;
    sp0c.W = largs->Ma;
    sp0c.nb = largs->nb;
    spawn<schur_closure> sp0(sp0c);

    ((aux_lower_solve_cont1_closure*)SN_aux_lower_solve_cont1.cls.get())->L11 = largs->L11;
    ((aux_lower_solve_cont1_closure*)SN_aux_lower_solve_cont1.cls.get())->nb = largs->nb;
    ((aux_lower_solve_cont1_closure*)SN_aux_lower_solve_cont1.cls.get())->Mb = largs->Mb;
    // Original sync was here
    return;
}
THREAD(aux_lower_solve_cont1) {
    aux_lower_solve_cont1_closure *largs = (aux_lower_solve_cont1_closure*)(args.get());
    aux_lower_solve_cont2_closure SN_aux_lower_solve_cont2c(largs->k);
    spawn_next<aux_lower_solve_cont2_closure> SN_aux_lower_solve_cont2(SN_aux_lower_solve_cont2c);
    cont sp0k;
    SN_BIND_VOID(SN_aux_lower_solve_cont2, &sp0k);
    lower_solve_closure sp0c(sp0k);
    sp0c.M = largs->Mb;
    sp0c.L = largs->L11;
    sp0c.nb = largs->nb;
    spawn<lower_solve_closure> sp0(sp0c);

    // Original sync was here
    return;
}
THREAD(aux_lower_solve_cont2) {
    aux_lower_solve_cont2_closure *largs = (aux_lower_solve_cont2_closure*)(args.get());
    return;
}
THREAD(lower_solve_cont0) {
    lower_solve_cont0_closure *largs = (lower_solve_cont0_closure*)(args.get());
    lower_solve_cont1_closure SN_lower_solve_cont1c(largs->k);
    spawn_next<lower_solve_cont1_closure> SN_lower_solve_cont1(SN_lower_solve_cont1c);
    // Original sync was here
    return;
}
THREAD(lower_solve_cont1) {
    lower_solve_cont1_closure *largs = (lower_solve_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(aux_upper_solve_cont0) {
    aux_upper_solve_cont0_closure *largs = (aux_upper_solve_cont0_closure*)(args.get());
    upper_solve(largs->Mb,largs->U11,largs->nb);
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(upper_solve_cont0) {
    upper_solve_cont0_closure *largs = (upper_solve_cont0_closure*)(args.get());
    upper_solve_cont1_closure SN_upper_solve_cont1c(largs->k);
    spawn_next<upper_solve_cont1_closure> SN_upper_solve_cont1(SN_upper_solve_cont1c);
    // Original sync was here
    return;
}
THREAD(upper_solve_cont1) {
    upper_solve_cont1_closure *largs = (upper_solve_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(lu_cont0) {
    lu_cont0_closure *largs = (lu_cont0_closure*)(args.get());
    lu_cont1_closure SN_lu_cont1c(largs->k);
    spawn_next<lu_cont1_closure> SN_lu_cont1(SN_lu_cont1c);
    cont sp0k;
    SN_BIND_VOID(SN_lu_cont1, &sp0k);
    schur_closure sp0c(sp0k);
    sp0c.M = largs->M11;
    sp0c.V = largs->M10;
    sp0c.W = largs->M01;
    sp0c.nb = largs->hnb;
    spawn<schur_closure> sp0(sp0c);

    ((lu_cont1_closure*)SN_lu_cont1.cls.get())->hnb = largs->hnb;
    ((lu_cont1_closure*)SN_lu_cont1.cls.get())->M11 = largs->M11;
    // Original sync was here
    return;
}
THREAD(lu_cont1) {
    lu_cont1_closure *largs = (lu_cont1_closure*)(args.get());
    lu(largs->M11,largs->hnb);
    SEND_ARGUMENT(largs->k, 0);
}
