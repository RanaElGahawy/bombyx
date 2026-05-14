#include "cilk_explicit.hh"
/*
 * Sparse Cholesky code with little blocks at the leaves of the Quad tree
 * Keith Randall -- Aske Plaat
 *
 * This code should run with any square sparse real symmetric matrix
 * from MatrixMarket (http://math.nist.gov/MatrixMarket)
 *
 * run with `cholesky -f george-liu.mtx' for a given matrix, or
 * `cholesky -n 1000 -z 10000' for a 1000x1000 random matrix with 10000
 * nonzeros (caution: random matrices produce lots of fill).
 */

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

#include <cilk/cilk.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "getoptions.h"

#if CILKSAN
#include "cilksan.h"
#endif

#if HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef SERIAL
#include <cilk/cilk_stub.h>
#endif

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

unsigned long rand_nxt = 0;

/*************************************************************\
 * Basic types
\*************************************************************/

typedef double Real;

#define BLOCK_DEPTH 2                 /* logarithm base 2 of BLOCK_SIZE */
#define BLOCK_SIZE (1 << BLOCK_DEPTH) /* 4 seems to be the optimum */

typedef Real Block[BLOCK_SIZE][BLOCK_SIZE];

#define BLOCK(B, I, J) (B[I][J])

#define _00 0
#define _01 1
#define _10 2
#define _11 3

#define TR_00 _00
#define TR_01 _10
#define TR_10 _01
#define TR_11 _11

typedef struct InternalNode {
  struct InternalNode *child[4];
} InternalNode;

typedef struct {
  Block block;
} LeafNode;

typedef InternalNode *Matrix;

unsigned long long todval(struct timeval *tp);
int cilk_rand();
void cilk_srand(unsigned int seed);
void block_schur_full(Block B, Block A, Block C);
void block_schur_half(Block B, Block A, Block C);
void block_backsub(Block B, Block U);
void block_cholesky(Block B);
void block_zero(Block B);
InternalNode *new_block_leaf();
InternalNode *new_internal(InternalNode *a00, InternalNode *a01,
                           InternalNode *a10, InternalNode *a11);
THREAD(copy_matrix);
void free_matrix(int depth0, Matrix a0);
Real get_matrix(int depth0, Matrix a0, int r0, int c);
Matrix set_matrix(int depth0, Matrix a0, int r0, int c, Real value);
int num_blocks(int depth0, Matrix a0);
int num_nonzeros(int depth0, Matrix a0);
Real mag(int depth0, Matrix a0);
THREAD(mul_and_subT);
THREAD(backsub);
THREAD(cholesky);
int logarithm(int size);
int usage();
int main(int argc, char *argv[]);
THREAD(main_afterif0);
THREAD(cholesky_afterif1);
THREAD(copy_matrix_cont0);
THREAD(copy_matrix_cont1);
THREAD(mul_and_subT_cont0);
THREAD(mul_and_subT_cont1);
THREAD(backsub_cont0);
THREAD(backsub_cont1);
THREAD(backsub_cont2);
THREAD(cholesky_cont0);
THREAD(cholesky_cont1);
THREAD(cholesky_cont2);
THREAD(cholesky_cont3);
THREAD(cholesky_cont4);
THREAD(cholesky_cont5);
THREAD(main_cont0);
THREAD(main_cont1);
THREAD(main_cont2);

CLOSURE_DEF(copy_matrix, int depth; Matrix a;);
CLOSURE_DEF(mul_and_subT, int depth0; int lower; Matrix a0; Matrix b;
            Matrix r0;);
CLOSURE_DEF(backsub, int depth1; Matrix a1; Matrix l;);
CLOSURE_DEF(cholesky, int depth2; Matrix a2;);
CLOSURE_DEF(main_afterif0, int argc; char **argv; Matrix A3; Matrix R1;
            int size; int depth3; int nonzeros; int i; int benchmark; int help;
            int check; int input_nonzeros; int input_blocks;
            int output_nonzeros; int output_blocks; Real error; char buf[1000];
            char filename[100]; int sizex; int sizey; FILE * f; double fr;
            double fc; int r1; int c; Real val; int res; double rnd; int t;
            int r2; int c0; bool ok; struct timeval t1; struct timeval t2;
            unsigned long long runtime_ms;);
CLOSURE_DEF(cholesky_afterif1, int depth2; Matrix a2; LeafNode * A2;
            Matrix a000; Matrix a100; Matrix a110;);
CLOSURE_DEF(copy_matrix_cont0, Matrix r00; Matrix r01; Matrix r10; Matrix r11;);
CLOSURE_DEF(copy_matrix_cont1, Matrix r00; Matrix r01; Matrix r10; Matrix r11;);
CLOSURE_DEF(mul_and_subT_cont0, int depth0; int lower; Matrix a0; Matrix b;
            Matrix r0; Matrix r000; Matrix r010; Matrix r100; Matrix r110;);
CLOSURE_DEF(mul_and_subT_cont1, Matrix r0; Matrix r000; Matrix r010;
            Matrix r100; Matrix r110;);
CLOSURE_DEF(backsub_cont0, int depth1; Matrix a1; Matrix a00; Matrix a01;
            Matrix a10; Matrix a11; Matrix l10; Matrix l11;);
CLOSURE_DEF(backsub_cont1, int depth1; Matrix a1; Matrix a00; Matrix a01;
            Matrix a10; Matrix a11; Matrix l11;);
CLOSURE_DEF(backsub_cont2, Matrix a1; Matrix a00; Matrix a01; Matrix a10;
            Matrix a11;);
CLOSURE_DEF(cholesky_cont0, int depth2; Matrix a2; LeafNode * A2; Matrix a000;
            Matrix a100; Matrix a110;);
CLOSURE_DEF(cholesky_cont1, int depth2; Matrix a2; LeafNode * A2; Matrix a000;
            Matrix a100; Matrix a110;);
CLOSURE_DEF(cholesky_cont2, int depth2; Matrix a2; LeafNode * A2; Matrix a000;
            Matrix a100; Matrix a110;);
CLOSURE_DEF(cholesky_cont3, int depth2; Matrix a2; LeafNode * A2; Matrix a000;
            Matrix a100; Matrix a110;);
CLOSURE_DEF(cholesky_cont4, int depth2; Matrix a2; LeafNode * A2; Matrix a000;
            Matrix a100; Matrix a110;);
CLOSURE_DEF(cholesky_cont5, int depth2; Matrix a2; LeafNode * A2; Matrix a000;
            Matrix a100; Matrix a110;);
CLOSURE_DEF(main_cont0, int argc; char **argv; Matrix A3; Matrix R1; int size;
            int depth3; int nonzeros; int i; int benchmark; int help; int check;
            Real error; char buf[1000]; char filename[100]; int sizex;
            int sizey; FILE * f; double fr; double fc; int r1; int c; Real val;
            int res; double rnd; int t; int r2; int c0; bool ok;
            struct timeval t1; struct timeval t2;);
CLOSURE_DEF(main_cont1, int argc; char **argv; Matrix A3; Matrix R1; int size;
            int depth3; int nonzeros; int i; int benchmark; int help; int check;
            int input_nonzeros; int input_blocks; Real error; char buf[1000];
            char filename[100]; int sizex; int sizey; FILE * f; double fr;
            double fc; int r1; int c; Real val; int res; double rnd; int t;
            int r2; int c0; bool ok; struct timeval t1; struct timeval t2;);
CLOSURE_DEF(main_cont2, int argc; char **argv; Matrix A3; Matrix R1; int size;
            int depth3; int nonzeros; int i; int benchmark; int help; int check;
            int input_nonzeros; int input_blocks; int output_nonzeros;
            int output_blocks; char buf[1000]; char filename[100]; int sizex;
            int sizey; FILE * f; double fr; double fc; int r1; int c; Real val;
            int res; double rnd; int t; int r2; int c0; bool ok;
            struct timeval t1; struct timeval t2;
            unsigned long long runtime_ms;);

/*************************************************************\
 * Linear algebra on blocks
 \*************************************************************/

/*
 * elem_daxmy - Compute y' = y - ax where a is a Real and x and y are
 * vectors of Reals.  Unused.
 *
static void
elem_daxmy (Real a, Real * x, Real * y, int n) {
  for (n--; n >= 0; n--)
    y[n] -= a * x[n];
}
*/

/*
 * block_schur - Compute Schur complement B' = B - AC.
 */

/*
 * block_schur - Compute Schur complement B' = B - AC.
 */

/*
 * block_upper_solve - Perform substitution to solve for B' in
 * B'U = B.
 */

/*
 * block_lower_solve - Perform forward substitution to solve for B' in
 * LB' = B.  Unused.
 *
static void
xblock_backsub (Block B, Block L) {

  int i, k;

  // Perform forward substitution.
  for (i = 0; i < BLOCK_SIZE; i = i + 1)
    for (k = 0; k <= i; k = k + 1) {
      BLOCK (B, i, k) /= BLOCK (L, k, k);
      elem_daxmy (BLOCK (L, i, k), &BLOCK (B, k, 0),
          &BLOCK (B, i, 0), BLOCK_SIZE - k);
    }
}
*/

/*
 * block_cholesky - Factor block B.
 */

/*
 * block_zero - zero block B.
 */

/*************************************************************\
 * Allocation and initialization
 \*************************************************************/

/*
 * Create new leaf nodes (BLOCK_SIZE x BLOCK_SIZE submatrices)
 */

/*
 * Create internal node in quadtree representation
 */

/*
 * Duplicate matrix.  Resulting matrix may be laid out in memory
 * better than source matrix.
 */

/*
 * Deallocate matrix.
 */

/*************************************************************\
 * Simple matrix operations
 \*************************************************************/

/*
 * Get matrix element at row r, column c.
 */

/*
 * Set matrix element at row r, column c to value.
 */

void print_matrix_aux(int depth, Matrix a, int r, int c) {

  if (a == NULL)
    return;

  if (depth == BLOCK_DEPTH) {
    LeafNode *A = (LeafNode *)a;
    int i, j;
    for (i = 0; i < BLOCK_SIZE; i = i + 1)
      for (j = 0; j < BLOCK_SIZE; j = j + 1)
        printf("%6d %6d: %12f\n", r + i, c + j, BLOCK(A->block, i, j));

  } else {
    int mid;
    depth = depth - 1;
    mid = 1 << depth;
    print_matrix_aux(depth, a->child[_00], r, c);
    print_matrix_aux(depth, a->child[_01], r, c + mid);
    print_matrix_aux(depth, a->child[_10], r + mid, c);
    print_matrix_aux(depth, a->child[_11], r + mid, c + mid);
  }
}

/*
 * Print matrix
 */
void print_matrix(int depth, Matrix a) { print_matrix_aux(depth, a, 0, 0); }

/*
 * Count number of blocks (leaves) in matrix representation
 */

/*
 * Count number of nonzeros in matrix
 */

/*
 * Compute sum of squares of elements of matrix
 */

/*************************************************************\
 * Cholesky algorithm
 \*************************************************************/

/*
 * Perform R -= A * Transpose(B)
 * if lower==1, update only lower-triangular part of R
 */

/*
 * Perform substitution to solve for B in BL = A
 * Returns B in place of A.
 */

/*
 * Compute Cholesky factorization of A.
 */

const char *specifiers[] = {"-n", "-z", "-c", "-f", "-benchmark", "-h", 0};
int opt_types[] = {INTARG, INTARG, BOOLARG, STRINGARG, BENCHMARK, BOOLARG, 0};

unsigned long long todval(struct timeval *tp) {
  return (((tp->tv_sec * 1000) * 1000) + tp->tv_usec);
}
int cilk_rand() {
  int result;
  result = ((rand_nxt >> 16) % (((unsigned int)2147483647) + 1));
  return result;
}
void cilk_srand(unsigned int seed) { rand_nxt = seed; }
void block_schur_full(Block B, Block A, Block C) {
  int i;
  int j;
  int k;
  for (i = 0; (i < (1 << 2)); i = (i + 1)) {
    for (j = 0; (j < (1 << 2)); j = (j + 1)) {
      for (k = 0; (k < (1 << 2)); k = (k + 1)) {
        B[i][j] = (B[i][j] - (A[i][k] * C[j][k]));
      }
    }
  }
}
void block_schur_half(Block B, Block A, Block C) {
  int i;
  int j;
  int k;
  for (i = 0; (i < (1 << 2)); i = (i + 1)) {
    for (j = 0; (j <= i); j = (j + 1)) {
      for (k = 0; (k < (1 << 2)); k = (k + 1)) {
        B[i][j] = (B[i][j] - (A[i][k] * C[j][k]));
      }
    }
  }
}
void block_backsub(Block B, Block U) {
  int i;
  int j;
  int k;
  for (i = 0; (i < (1 << 2)); i = (i + 1)) {
    for (j = 0; (j < (1 << 2)); j = (j + 1)) {
      for (k = 0; (k < i); k = (k + 1)) {
        B[j][i] = (B[j][i] - (U[i][k] * B[j][k]));
      }
      B[j][i] = (B[j][i] / U[i][i]);
    }
  }
}
void block_cholesky(Block B) {
  int i;
  int j;
  int k;
  Real x;
  for (k = 0; (k < (1 << 2)); k = (k + 1)) {
    if ((B[k][k] < 0.)) {
      printf("sqrt error: %f\n", B[k][k]);
      printf("matrix is probably not numerically stable\n");
      exit(9);
    }
    x = sqrt(B[k][k]);
    for (i = k; (i < (1 << 2)); i = (i + 1)) {
      B[i][k] = (B[i][k] / x);
    }
    for (j = (k + 1); (j < (1 << 2)); j = (j + 1)) {
      for (i = j; (i < (1 << 2)); i = (i + 1)) {
        B[i][j] = (B[i][j] - (B[i][k] * B[j][k]));
        if (((j > i) && (B[i][j] != 0.))) {
          printf("Upper not empty\n");
        }
      }
    }
  }
}
void block_zero(Block B) {
  int i;
  int k;
  for (i = 0; (i < (1 << 2)); i = (i + 1)) {
    for (k = 0; (k < (1 << 2)); k = (k + 1)) {
      B[i][k] = 0.;
    }
  }
}
InternalNode *new_block_leaf() {
  LeafNode *leaf;
  leaf = ((LeafNode *)malloc(sizeof(LeafNode)));
  if ((leaf == __null)) {
    printf("out of memory!\n");
    exit(1);
  }
  return ((InternalNode *)leaf);
}
InternalNode *new_internal(InternalNode *a00, InternalNode *a01,
                           InternalNode *a10, InternalNode *a11) {
  InternalNode *node;
  node = ((InternalNode *)malloc(sizeof(InternalNode)));
  if ((node == __null)) {
    printf("out of memory!\n");
    exit(1);
  }
  node->child[0] = a00;
  node->child[1] = a01;
  node->child[2] = a10;
  node->child[3] = a11;
  return node;
}
THREAD(copy_matrix) {
  Matrix r;
  LeafNode *A;
  LeafNode *R;
  Matrix r00;
  Matrix r01;
  Matrix r10;
  Matrix r11;
  copy_matrix_closure *largs = (copy_matrix_closure *)(args.get());
  if ((!largs->a)) {
    SEND_ARGUMENT(largs->k, largs->a);
  } else {
    if ((largs->depth == 2)) {
      A = ((LeafNode *)largs->a);
      r = new_block_leaf();
      R = ((LeafNode *)r);
      memcpy(R->block, A->block, sizeof(Block));
      SEND_ARGUMENT(largs->k, r);
    } else {
      r00 = __null;
      r01 = __null;
      r10 = __null;
      r11 = __null;
      largs->depth = (largs->depth - 1);
      copy_matrix_cont0_closure SN_copy_matrix_cont0c(largs->k);
      spawn_next<copy_matrix_cont0_closure> SN_copy_matrix_cont0(
          SN_copy_matrix_cont0c);
      cont sp0k;
      SN_BIND(SN_copy_matrix_cont0, &sp0k, r00);
      copy_matrix_closure sp0c(sp0k);
      sp0c.depth = largs->depth;
      sp0c.a = largs->a->child[0];
      spawn<copy_matrix_closure> sp0(sp0c);

      cont sp1k;
      SN_BIND(SN_copy_matrix_cont0, &sp1k, r01);
      copy_matrix_closure sp1c(sp1k);
      sp1c.depth = largs->depth;
      sp1c.a = largs->a->child[1];
      spawn<copy_matrix_closure> sp1(sp1c);

      cont sp2k;
      SN_BIND(SN_copy_matrix_cont0, &sp2k, r10);
      copy_matrix_closure sp2c(sp2k);
      sp2c.depth = largs->depth;
      sp2c.a = largs->a->child[2];
      spawn<copy_matrix_closure> sp2(sp2c);

      cont sp3k;
      SN_BIND(SN_copy_matrix_cont0, &sp3k, r11);
      copy_matrix_closure sp3c(sp3k);
      sp3c.depth = largs->depth;
      sp3c.a = largs->a->child[3];
      spawn<copy_matrix_closure> sp3(sp3c);

      // Original sync was here
    }
  }
  return;
}
void free_matrix(int depth0, Matrix a0) {
  if ((a0 == __null)) {
    return;
  } else {
    if ((depth0 == 2)) {
      free(a0);
    } else {
      depth0 = (depth0 - 1);
      free_matrix(depth0, a0->child[0]);
      free_matrix(depth0, a0->child[1]);
      free_matrix(depth0, a0->child[2]);
      free_matrix(depth0, a0->child[3]);
      free(a0);
    }
  }
}
Real get_matrix(int depth0, Matrix a0, int r0, int c) {
  LeafNode *A0;
  int mid;
  __builtin_expect((!(depth0 >= 2)), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 344, "depth >= BLOCK_DEPTH")
      : ((void)0);
  __builtin_expect((!(r0 < (1 << depth0))), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 345, "r < (1 << depth)")
      : ((void)0);
  __builtin_expect((!(c < (1 << depth0))), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 346, "c < (1 << depth)")
      : ((void)0);
  if ((a0 == __null)) {
    return 0.;
  } else {
    if ((depth0 == 2)) {
      A0 = ((LeafNode *)a0);
      return A0->block[r0][c];
    } else {
      depth0 = (depth0 - 1);
      mid = (1 << depth0);
      if ((r0 < mid)) {
        if ((c < mid)) {
          return get_matrix(depth0, a0->child[0], r0, c);
        } else {
          return get_matrix(depth0, a0->child[1], r0, (c - mid));
        }
      } else {
        if ((c < mid)) {
          return get_matrix(depth0, a0->child[2], (r0 - mid), c);
        } else {
          return get_matrix(depth0, a0->child[3], (r0 - mid), (c - mid));
        }
      }
    }
  }
}
Matrix set_matrix(int depth0, Matrix a0, int r0, int c, Real value) {
  LeafNode *A0;
  int mid;
  __builtin_expect((!(depth0 >= 2)), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 380, "depth >= BLOCK_DEPTH")
      : ((void)0);
  __builtin_expect((!(r0 < (1 << depth0))), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 381, "r < (1 << depth)")
      : ((void)0);
  __builtin_expect((!(c < (1 << depth0))), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 382, "c < (1 << depth)")
      : ((void)0);
  if ((depth0 == 2)) {
    if ((a0 == __null)) {
      a0 = new_block_leaf();
      A0 = ((LeafNode *)a0);
      block_zero(A0->block);
    } else {
      A0 = ((LeafNode *)a0);
    }
    A0->block[r0][c] = value;
  } else {
    if ((a0 == __null)) {
      a0 = new_internal(__null, __null, __null, __null);
    }
    depth0 = (depth0 - 1);
    mid = (1 << depth0);
    if ((r0 < mid)) {
      if ((c < mid)) {
        a0->child[0] = set_matrix(depth0, a0->child[0], r0, c, value);
      } else {
        a0->child[1] = set_matrix(depth0, a0->child[1], r0, (c - mid), value);
      }
    } else {
      if ((c < mid)) {
        a0->child[2] = set_matrix(depth0, a0->child[2], (r0 - mid), c, value);
      } else {
        a0->child[3] =
            set_matrix(depth0, a0->child[3], (r0 - mid), (c - mid), value);
      }
    }
  }
  return a0;
}
int num_blocks(int depth0, Matrix a0) {
  int res;
  if ((a0 == __null)) {
    return 0;
  } else {
    if ((depth0 == 2)) {
      return 1;
    } else {
      depth0 = (depth0 - 1);
      res = 0;
      res = (res + num_blocks(depth0, a0->child[0]));
      res = (res + num_blocks(depth0, a0->child[1]));
      res = (res + num_blocks(depth0, a0->child[2]));
      res = (res + num_blocks(depth0, a0->child[3]));
      return res;
    }
  }
}
int num_nonzeros(int depth0, Matrix a0) {
  int res;
  LeafNode *A0;
  int i;
  int j;
  if ((a0 == __null)) {
    return 0;
  } else {
    if ((depth0 == 2)) {
      A0 = ((LeafNode *)a0);
      res = 0;
      for (i = 0; (i < (1 << 2)); i = (i + 1)) {
        for (j = 0; (j < (1 << 2)); j = (j + 1)) {
          if ((A0->block[i][j] != 0.)) {
            res = (res + 1);
          }
        }
      }
      return res;
    } else {
      depth0 = (depth0 - 1);
      res = 0;
      res = (res + num_nonzeros(depth0, a0->child[0]));
      res = (res + num_nonzeros(depth0, a0->child[1]));
      res = (res + num_nonzeros(depth0, a0->child[2]));
      res = (res + num_nonzeros(depth0, a0->child[3]));
      return res;
    }
  }
}
Real mag(int depth0, Matrix a0) {
  Real res;
  LeafNode *A0;
  int i;
  int j;
  res = 0.;
  if ((!a0)) {
    return res;
  } else {
    if ((depth0 == 2)) {
      A0 = ((LeafNode *)a0);
      for (i = 0; (i < (1 << 2)); i = (i + 1)) {
        for (j = 0; (j < (1 << 2)); j = (j + 1)) {
          res = (res + (A0->block[i][j] * A0->block[i][j]));
        }
      }
    } else {
      depth0 = (depth0 - 1);
      res = (res + mag(depth0, a0->child[0]));
      res = (res + mag(depth0, a0->child[1]));
      res = (res + mag(depth0, a0->child[2]));
      res = (res + mag(depth0, a0->child[3]));
    }
    return res;
  }
}
THREAD(mul_and_subT) {
  LeafNode *A0;
  LeafNode *B;
  LeafNode *R0;
  Matrix r000;
  Matrix r010;
  Matrix r100;
  Matrix r110;
  mul_and_subT_closure *largs = (mul_and_subT_closure *)(args.get());
  __builtin_expect((!((largs->a0 != __null) && (largs->b != __null))), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 537, "a != NULL && b != NULL")
      : ((void)0);
  if ((largs->depth0 == 2)) {
    A0 = ((LeafNode *)largs->a0);
    B = ((LeafNode *)largs->b);
    if ((largs->r0 == __null)) {
      largs->r0 = new_block_leaf();
      R0 = ((LeafNode *)largs->r0);
      block_zero(R0->block);
    } else {
      R0 = ((LeafNode *)largs->r0);
    }
    if (largs->lower) {
      block_schur_half(R0->block, A0->block, B->block);
    } else {
      block_schur_full(R0->block, A0->block, B->block);
    }
    SEND_ARGUMENT(largs->k, largs->r0);
  } else {
    largs->depth0 = (largs->depth0 - 1);
    mul_and_subT_cont0_closure SN_mul_and_subT_cont0c(largs->k);
    spawn_next<mul_and_subT_cont0_closure> SN_mul_and_subT_cont0(
        SN_mul_and_subT_cont0c);
    if ((largs->r0 != __null)) {
      r000 = largs->r0->child[0];
      r010 = largs->r0->child[1];
      r100 = largs->r0->child[2];
      r110 = largs->r0->child[3];
    } else {
      r000 = __null;
      r010 = __null;
      r100 = __null;
      r110 = __null;
    }
    if ((largs->a0->child[0] && largs->b->child[0])) {
      cont sp0k;
      SN_BIND(SN_mul_and_subT_cont0, &sp0k, r000);
      mul_and_subT_closure sp0c(sp0k);
      sp0c.depth0 = largs->depth0;
      sp0c.lower = largs->lower;
      sp0c.a0 = largs->a0->child[0];
      sp0c.b = largs->b->child[0];
      sp0c.r0 = r000;
      spawn<mul_and_subT_closure> sp0(sp0c);
    }
    if ((((!largs->lower) && largs->a0->child[0]) && largs->b->child[2])) {
      cont sp1k;
      SN_BIND(SN_mul_and_subT_cont0, &sp1k, r010);
      mul_and_subT_closure sp1c(sp1k);
      sp1c.depth0 = largs->depth0;
      sp1c.lower = 0;
      sp1c.a0 = largs->a0->child[0];
      sp1c.b = largs->b->child[2];
      sp1c.r0 = r010;
      spawn<mul_and_subT_closure> sp1(sp1c);
    }
    if ((largs->a0->child[2] && largs->b->child[0])) {
      cont sp2k;
      SN_BIND(SN_mul_and_subT_cont0, &sp2k, r100);
      mul_and_subT_closure sp2c(sp2k);
      sp2c.depth0 = largs->depth0;
      sp2c.lower = 0;
      sp2c.a0 = largs->a0->child[2];
      sp2c.b = largs->b->child[0];
      sp2c.r0 = r100;
      spawn<mul_and_subT_closure> sp2(sp2c);
    }
    if ((largs->a0->child[2] && largs->b->child[2])) {
      cont sp3k;
      SN_BIND(SN_mul_and_subT_cont0, &sp3k, r110);
      mul_and_subT_closure sp3c(sp3k);
      sp3c.depth0 = largs->depth0;
      sp3c.lower = largs->lower;
      sp3c.a0 = largs->a0->child[2];
      sp3c.b = largs->b->child[2];
      sp3c.r0 = r110;
      spawn<mul_and_subT_closure> sp3(sp3c);
    }
    ((mul_and_subT_cont0_closure *)SN_mul_and_subT_cont0.cls.get())->depth0 =
        largs->depth0;
    ((mul_and_subT_cont0_closure *)SN_mul_and_subT_cont0.cls.get())->r0 =
        largs->r0;
    ((mul_and_subT_cont0_closure *)SN_mul_and_subT_cont0.cls.get())->b =
        largs->b;
    ((mul_and_subT_cont0_closure *)SN_mul_and_subT_cont0.cls.get())->a0 =
        largs->a0;
    ((mul_and_subT_cont0_closure *)SN_mul_and_subT_cont0.cls.get())->lower =
        largs->lower;
    // Original sync was here
  }
  return;
}
THREAD(backsub) {
  LeafNode *A1;
  LeafNode *L;
  Matrix a00;
  Matrix a01;
  Matrix a10;
  Matrix a11;
  Matrix l00;
  Matrix l10;
  Matrix l11;
  backsub_closure *largs = (backsub_closure *)(args.get());
  __builtin_expect((!(largs->a1 != __null)), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 643, "a != NULL")
      : ((void)0);
  __builtin_expect((!(largs->l != __null)), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 644, "l != NULL")
      : ((void)0);
  if ((largs->depth1 == 2)) {
    A1 = ((LeafNode *)largs->a1);
    L = ((LeafNode *)largs->l);
    block_backsub(A1->block, L->block);
    SEND_ARGUMENT(largs->k, largs->a1);
  } else {
    largs->depth1 = (largs->depth1 - 1);
    a00 = largs->a1->child[0];
    a01 = largs->a1->child[1];
    a10 = largs->a1->child[2];
    a11 = largs->a1->child[3];
    l00 = largs->l->child[0];
    l10 = largs->l->child[2];
    l11 = largs->l->child[3];
    __builtin_expect((!(l00 && l11)), 0)
        ? __assert_rtn(__func__, "cholesky.cpp", 668, "l00 && l11")
        : ((void)0);
    backsub_cont0_closure SN_backsub_cont0c(largs->k);
    spawn_next<backsub_cont0_closure> SN_backsub_cont0(SN_backsub_cont0c);
    if (a00) {
      cont sp0k;
      SN_BIND(SN_backsub_cont0, &sp0k, a00);
      backsub_closure sp0c(sp0k);
      sp0c.depth1 = largs->depth1;
      sp0c.a1 = a00;
      sp0c.l = l00;
      spawn<backsub_closure> sp0(sp0c);
    }
    if (a10) {
      cont sp1k;
      SN_BIND(SN_backsub_cont0, &sp1k, a10);
      backsub_closure sp1c(sp1k);
      sp1c.depth1 = largs->depth1;
      sp1c.a1 = a10;
      sp1c.l = l00;
      spawn<backsub_closure> sp1(sp1c);
    }
    ((backsub_cont0_closure *)SN_backsub_cont0.cls.get())->l10 = l10;
    ((backsub_cont0_closure *)SN_backsub_cont0.cls.get())->a11 = a11;
    ((backsub_cont0_closure *)SN_backsub_cont0.cls.get())->a01 = a01;
    ((backsub_cont0_closure *)SN_backsub_cont0.cls.get())->l11 = l11;
    ((backsub_cont0_closure *)SN_backsub_cont0.cls.get())->a1 = largs->a1;
    ((backsub_cont0_closure *)SN_backsub_cont0.cls.get())->depth1 =
        largs->depth1;
    // Original sync was here
  }
  return;
}
THREAD(cholesky) {
  LeafNode *A2;
  Matrix a000;
  Matrix a100;
  Matrix a110;
  cholesky_closure *largs = (cholesky_closure *)(args.get());
  __builtin_expect((!(largs->a2 != __null)), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 707, "a != NULL")
      : ((void)0);
  if ((largs->depth2 == 2)) {
    A2 = ((LeafNode *)largs->a2);
    block_cholesky(A2->block);
    SEND_ARGUMENT(largs->k, largs->a2);
  } else {
    largs->depth2 = (largs->depth2 - 1);
    a000 = largs->a2->child[0];
    a100 = largs->a2->child[2];
    a110 = largs->a2->child[3];
    __builtin_expect((!a000), 0)
        ? __assert_rtn(__func__, "cholesky.cpp", 724, "a00")
        : ((void)0);
    if ((!a100)) {
      cholesky_cont4_closure SN_cholesky_cont4c(largs->k);
      spawn_next<cholesky_cont4_closure> SN_cholesky_cont4(SN_cholesky_cont4c);
      cont sp0k;
      SN_BIND(SN_cholesky_cont4, &sp0k, a000);
      cholesky_closure sp0c(sp0k);
      sp0c.depth2 = largs->depth2;
      sp0c.a2 = a000;
      spawn<cholesky_closure> sp0(sp0c);

      cont sp1k;
      SN_BIND(SN_cholesky_cont4, &sp1k, a110);
      cholesky_closure sp1c(sp1k);
      sp1c.depth2 = largs->depth2;
      sp1c.a2 = a110;
      spawn<cholesky_closure> sp1(sp1c);

      ((cholesky_cont4_closure *)SN_cholesky_cont4.cls.get())->a100 = a100;
      ((cholesky_cont4_closure *)SN_cholesky_cont4.cls.get())->A2 = A2;
      ((cholesky_cont4_closure *)SN_cholesky_cont4.cls.get())->a2 = largs->a2;
      ((cholesky_cont4_closure *)SN_cholesky_cont4.cls.get())->depth2 =
          largs->depth2;
      // Original sync was here
    } else {
      cholesky_cont0_closure SN_cholesky_cont0c(largs->k);
      spawn_next<cholesky_cont0_closure> SN_cholesky_cont0(SN_cholesky_cont0c);
      cont sp2k;
      SN_BIND(SN_cholesky_cont0, &sp2k, a000);
      cholesky_closure sp2c(sp2k);
      sp2c.depth2 = largs->depth2;
      sp2c.a2 = a000;
      spawn<cholesky_closure> sp2(sp2c);

      ((cholesky_cont0_closure *)SN_cholesky_cont0.cls.get())->a100 = a100;
      ((cholesky_cont0_closure *)SN_cholesky_cont0.cls.get())->A2 = A2;
      ((cholesky_cont0_closure *)SN_cholesky_cont0.cls.get())->a2 = largs->a2;
      ((cholesky_cont0_closure *)SN_cholesky_cont0.cls.get())->depth2 =
          largs->depth2;
      // Original sync was here
    }
  }
  return;
}
int logarithm(int size) {
  int k;
  k = 0;
  while (((1 << k) < size)) {
    k = (k + 1);
  }
  return k;
}
int usage() {
  fprintf(__stderrp,
          "\nUsage: cholesky [<cilk-options>] [-n size] [-z nonzeros]\n        "
          "        [-f filename] [-benchmark] [-h]\n\nDefault: cholesky -n 500 "
          "-z 1000\n\nThis program performs a divide and conquer Cholesky "
          "factorization of a\nsparse symmetric positive definite matrix "
          "(A=LL^T).  Using the fact\nthat the matrix is symmetric, Cholesky "
          "does half the number of\noperations of LU.  The method used is the "
          "same as with LU, with work\nTheta(n^3) and critical path Theta(n "
          "lg(n)) for the dense case.  A\n");
  fprintf(__stderrp,
          "quad-tree is used to store the nonzero entries of the "
          "sparse\nmatrix. Actual work and critical path are influenced by the "
          "sparsity\n pattern of the matrix.\n\nThe input matrix is either "
          "read from the provided file or generated\nrandomly with size and "
          "nonzero-elements as specified.\n\n");
  return 1;
}
int main(int argc, char *argv[]) {
  Matrix A3;
  Matrix R1;
  int size;
  int depth3;
  int nonzeros;
  int i;
  int benchmark;
  int help;
  int check;
  Real error;
  char buf[1000];
  char filename[100];
  int sizex;
  int sizey;
  FILE *f;
  double fr;
  double fc;
  int r1;
  int c;
  Real val;
  int res;
  double rnd;
  int t;
  int r2;
  int c0;
  bool ok;
  check = 1;
  error = 0.;
  A3 = __null;
  filename[0] = 0;
  size = 500;
  nonzeros = 1000;
  get_options(argc, argv, specifiers, opt_types, &(size), &(nonzeros), &(check),
              filename, &(benchmark), &(help));
  if (help) {
    return usage();
  } else {
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    if (benchmark) {
    }
    if (filename[0]) {
      f = fopen(filename, "r");
      if ((f == __null)) {
        printf("\nFile not found!\n\n");
        return 1;
      } else {
        fgets(buf, 1000, f);
        while ((buf[0] == '%')) {
          fgets(buf, 1000, f);
        }
        sscanf(buf, "%d %d", &(sizex), &(sizey));
        __builtin_expect((!(sizex == sizey)), 0)
            ? __assert_rtn(__func__, "cholesky.cpp", 846, "sizex == sizey")
            : ((void)0);
        size = sizex;
        depth3 = logarithm(size);
        srand(61066);
        cilk_srand(61066);
        nonzeros = 0;
        while ((!feof(f))) {
          fgets(buf, 1000, f);
          res = sscanf(buf, "%lf %lf %lf", &(fr), &(fc), &(val));
          r1 = fr;
          c = fc;
          if ((res <= 0)) {
            break;
          }
          if ((res == 2)) {
            rnd = (((double)rand()) / ((double)2147483647));
            val = (r1 == c) ? (1.0E+5 * rnd) : rnd;
          }
          r1 = (r1 - 1);
          c = (c - 1);
          if ((r1 < c)) {
            t = r1;
            r1 = c;
            c = t;
          }
          __builtin_expect((!(r1 >= c)), 0)
              ? __assert_rtn(__func__, "cholesky.cpp", 887, "r >= c")
              : ((void)0);
          __builtin_expect((!(r1 < size)), 0)
              ? __assert_rtn(__func__, "cholesky.cpp", 888, "r < size")
              : ((void)0);
          __builtin_expect((!(c < size)), 0)
              ? __assert_rtn(__func__, "cholesky.cpp", 889, "c < size")
              : ((void)0);
          A3 = set_matrix(depth3, A3, r1, c, val);
          nonzeros = (nonzeros + 1);
        }
      }
    } else {
      depth3 = logarithm(size);
      for (i = 0; (i < size); i = (i + 1)) {
        A3 = set_matrix(depth3, A3, i, i, 1.);
      }
      for (i = 0; (i < (nonzeros - size)); i = (i + 1)) {
        ok = false;
        while ((!ok)) {
          r2 = (cilk_rand() % size);
          c0 = (cilk_rand() % size);
          ok = ((r2 > c0) && (get_matrix(depth3, A3, r2, c0) == 0.));
        }
        A3 = set_matrix(depth3, A3, r2, c0, 0.10000000000000001);
      }
    }
    for (i = size; (i < (1 << depth3)); i = (i + 1)) {
      A3 = set_matrix(depth3, A3, i, i, 1.);
    }
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, R1);
    copy_matrix_closure sp0c(sp0k);
    sp0c.depth = depth3;
    sp0c.a = A3;
    spawn<copy_matrix_closure> sp0(sp0c);

    ((main_cont0_closure *)SN_main_cont0.cls.get())->r2 = r2;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->t = t;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->rnd = rnd;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->res = res;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->error = error;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->r1 = r1;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->c0 = c0;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->f = f;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->sizey = sizey;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->check = check;
    memcpy(((main_cont0_closure *)SN_main_cont0.cls.get())->filename, filename,
           sizeof(filename));
    memcpy(((main_cont0_closure *)SN_main_cont0.cls.get())->buf, buf,
           sizeof(buf));
    ((main_cont0_closure *)SN_main_cont0.cls.get())->c = c;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->help = help;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->val = val;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->benchmark = benchmark;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->argv = argv;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->A3 = A3;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->size = size;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->fr = fr;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->i = i;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->fc = fc;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->nonzeros = nonzeros;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->ok = ok;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->depth3 = depth3;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->sizex = sizex;
    ((main_cont0_closure *)SN_main_cont0.cls.get())->argc = argc;
    // Original sync was here
  }
  return 0;
}
THREAD(main_afterif0) {
  main_afterif0_closure *largs = (main_afterif0_closure *)(args.get());
  fprintf(__stderrp, "\nCilk Example: cholesky\n");
  if (largs->check) {
    printf("Error: %f\n\n", largs->error);
  }
  fprintf(__stderrp, "Options: original size     = %d\n", largs->size);
  fprintf(__stderrp, "         original nonzeros = %d\n", largs->nonzeros);
  fprintf(__stderrp, "         input nonzeros    = %d\n",
          largs->input_nonzeros);
  fprintf(__stderrp, "         input blocks      = %d\n", largs->input_blocks);
  fprintf(__stderrp, "         output nonzeros   = %d\n",
          largs->output_nonzeros);
  fprintf(__stderrp, "         output blocks     = %d\n\n",
          largs->output_blocks);
  free_matrix(largs->depth3, largs->A3);
  free_matrix(largs->depth3, largs->R1);
  SEND_ARGUMENT(largs->k, 0);
  return;
}
THREAD(cholesky_afterif1) {
  cholesky_afterif1_closure *largs = (cholesky_afterif1_closure *)(args.get());
  largs->a2->child[0] = largs->a000;
  largs->a2->child[2] = largs->a100;
  largs->a2->child[3] = largs->a110;
  SEND_ARGUMENT(largs->k, largs->a2);
  return;
}
THREAD(copy_matrix_cont0) {
  copy_matrix_cont0_closure *largs = (copy_matrix_cont0_closure *)(args.get());
  copy_matrix_cont1_closure SN_copy_matrix_cont1c(largs->k);
  spawn_next<copy_matrix_cont1_closure> SN_copy_matrix_cont1(
      SN_copy_matrix_cont1c);
  ((copy_matrix_cont1_closure *)SN_copy_matrix_cont1.cls.get())->r10 =
      largs->r10;
  ((copy_matrix_cont1_closure *)SN_copy_matrix_cont1.cls.get())->r11 =
      largs->r11;
  ((copy_matrix_cont1_closure *)SN_copy_matrix_cont1.cls.get())->r01 =
      largs->r01;
  ((copy_matrix_cont1_closure *)SN_copy_matrix_cont1.cls.get())->r00 =
      largs->r00;
  // Original sync was here
  return;
}
THREAD(copy_matrix_cont1) {
  Matrix r;
  copy_matrix_cont1_closure *largs = (copy_matrix_cont1_closure *)(args.get());
  r = new_internal(largs->r00, largs->r01, largs->r10, largs->r11);
  SEND_ARGUMENT(largs->k, r);
  return;
}
THREAD(mul_and_subT_cont0) {
  mul_and_subT_cont0_closure *largs =
      (mul_and_subT_cont0_closure *)(args.get());
  mul_and_subT_cont1_closure SN_mul_and_subT_cont1c(largs->k);
  spawn_next<mul_and_subT_cont1_closure> SN_mul_and_subT_cont1(
      SN_mul_and_subT_cont1c);
  if ((largs->a0->child[1] && largs->b->child[1])) {
    cont sp0k;
    SN_BIND(SN_mul_and_subT_cont1, &sp0k, r000);
    mul_and_subT_closure sp0c(sp0k);
    sp0c.depth0 = largs->depth0;
    sp0c.lower = largs->lower;
    sp0c.a0 = largs->a0->child[1];
    sp0c.b = largs->b->child[1];
    sp0c.r0 = largs->r000;
    spawn<mul_and_subT_closure> sp0(sp0c);
  }
  if ((((!largs->lower) && largs->a0->child[1]) && largs->b->child[3])) {
    cont sp1k;
    SN_BIND(SN_mul_and_subT_cont1, &sp1k, r010);
    mul_and_subT_closure sp1c(sp1k);
    sp1c.depth0 = largs->depth0;
    sp1c.lower = 0;
    sp1c.a0 = largs->a0->child[1];
    sp1c.b = largs->b->child[3];
    sp1c.r0 = largs->r010;
    spawn<mul_and_subT_closure> sp1(sp1c);
  }
  if ((largs->a0->child[3] && largs->b->child[1])) {
    cont sp2k;
    SN_BIND(SN_mul_and_subT_cont1, &sp2k, r100);
    mul_and_subT_closure sp2c(sp2k);
    sp2c.depth0 = largs->depth0;
    sp2c.lower = 0;
    sp2c.a0 = largs->a0->child[3];
    sp2c.b = largs->b->child[1];
    sp2c.r0 = largs->r100;
    spawn<mul_and_subT_closure> sp2(sp2c);
  }
  if ((largs->a0->child[3] && largs->b->child[3])) {
    cont sp3k;
    SN_BIND(SN_mul_and_subT_cont1, &sp3k, r110);
    mul_and_subT_closure sp3c(sp3k);
    sp3c.depth0 = largs->depth0;
    sp3c.lower = largs->lower;
    sp3c.a0 = largs->a0->child[3];
    sp3c.b = largs->b->child[3];
    sp3c.r0 = largs->r110;
    spawn<mul_and_subT_closure> sp3(sp3c);
  }
  ((mul_and_subT_cont1_closure *)SN_mul_and_subT_cont1.cls.get())->r0 =
      largs->r0;
  // Original sync was here
  return;
}
THREAD(mul_and_subT_cont1) {
  mul_and_subT_cont1_closure *largs =
      (mul_and_subT_cont1_closure *)(args.get());
  if ((largs->r0 == __null)) {
    if ((((largs->r000 || largs->r010) || largs->r100) || largs->r110)) {
      largs->r0 =
          new_internal(largs->r000, largs->r010, largs->r100, largs->r110);
    }
  } else {
    __builtin_expect((!((largs->r0->child[0] == __null) ||
                        (largs->r0->child[0] == largs->r000))),
                     0)
        ? __assert_rtn(__func__, "cholesky.cpp", 624,
                       "r->child[_00] == NULL || r->child[_00] == r00")
        : ((void)0);
    __builtin_expect((!((largs->r0->child[1] == __null) ||
                        (largs->r0->child[1] == largs->r010))),
                     0)
        ? __assert_rtn(__func__, "cholesky.cpp", 625,
                       "r->child[_01] == NULL || r->child[_01] == r01")
        : ((void)0);
    __builtin_expect((!((largs->r0->child[2] == __null) ||
                        (largs->r0->child[2] == largs->r100))),
                     0)
        ? __assert_rtn(__func__, "cholesky.cpp", 626,
                       "r->child[_10] == NULL || r->child[_10] == r10")
        : ((void)0);
    __builtin_expect((!((largs->r0->child[3] == __null) ||
                        (largs->r0->child[3] == largs->r110))),
                     0)
        ? __assert_rtn(__func__, "cholesky.cpp", 627,
                       "r->child[_11] == NULL || r->child[_11] == r11")
        : ((void)0);
    largs->r0->child[0] = largs->r000;
    largs->r0->child[1] = largs->r010;
    largs->r0->child[2] = largs->r100;
    largs->r0->child[3] = largs->r110;
  }
  SEND_ARGUMENT(largs->k, largs->r0);
  return;
}
THREAD(backsub_cont0) {
  backsub_cont0_closure *largs = (backsub_cont0_closure *)(args.get());
  backsub_cont1_closure SN_backsub_cont1c(largs->k);
  spawn_next<backsub_cont1_closure> SN_backsub_cont1(SN_backsub_cont1c);
  if ((largs->a00 && largs->l10)) {
    cont sp0k;
    SN_BIND(SN_backsub_cont1, &sp0k, a01);
    mul_and_subT_closure sp0c(sp0k);
    sp0c.depth0 = largs->depth1;
    sp0c.lower = 0;
    sp0c.a0 = largs->a00;
    sp0c.b = largs->l10;
    sp0c.r0 = largs->a01;
    spawn<mul_and_subT_closure> sp0(sp0c);
  }
  if ((largs->a10 && largs->l10)) {
    cont sp1k;
    SN_BIND(SN_backsub_cont1, &sp1k, a11);
    mul_and_subT_closure sp1c(sp1k);
    sp1c.depth0 = largs->depth1;
    sp1c.lower = 0;
    sp1c.a0 = largs->a10;
    sp1c.b = largs->l10;
    sp1c.r0 = largs->a11;
    spawn<mul_and_subT_closure> sp1(sp1c);
  }
  ((backsub_cont1_closure *)SN_backsub_cont1.cls.get())->l11 = largs->l11;
  ((backsub_cont1_closure *)SN_backsub_cont1.cls.get())->a10 = largs->a10;
  ((backsub_cont1_closure *)SN_backsub_cont1.cls.get())->a00 = largs->a00;
  ((backsub_cont1_closure *)SN_backsub_cont1.cls.get())->a1 = largs->a1;
  ((backsub_cont1_closure *)SN_backsub_cont1.cls.get())->depth1 = largs->depth1;
  // Original sync was here
  return;
}
THREAD(backsub_cont1) {
  backsub_cont1_closure *largs = (backsub_cont1_closure *)(args.get());
  backsub_cont2_closure SN_backsub_cont2c(largs->k);
  spawn_next<backsub_cont2_closure> SN_backsub_cont2(SN_backsub_cont2c);
  if (largs->a01) {
    cont sp0k;
    SN_BIND(SN_backsub_cont2, &sp0k, a01);
    backsub_closure sp0c(sp0k);
    sp0c.depth1 = largs->depth1;
    sp0c.a1 = largs->a01;
    sp0c.l = largs->l11;
    spawn<backsub_closure> sp0(sp0c);
  }
  if (largs->a11) {
    cont sp1k;
    SN_BIND(SN_backsub_cont2, &sp1k, a11);
    backsub_closure sp1c(sp1k);
    sp1c.depth1 = largs->depth1;
    sp1c.a1 = largs->a11;
    sp1c.l = largs->l11;
    spawn<backsub_closure> sp1(sp1c);
  }
  ((backsub_cont2_closure *)SN_backsub_cont2.cls.get())->a10 = largs->a10;
  ((backsub_cont2_closure *)SN_backsub_cont2.cls.get())->a00 = largs->a00;
  ((backsub_cont2_closure *)SN_backsub_cont2.cls.get())->a1 = largs->a1;
  // Original sync was here
  return;
}
THREAD(backsub_cont2) {
  backsub_cont2_closure *largs = (backsub_cont2_closure *)(args.get());
  largs->a1->child[0] = largs->a00;
  largs->a1->child[1] = largs->a01;
  largs->a1->child[2] = largs->a10;
  largs->a1->child[3] = largs->a11;
  SEND_ARGUMENT(largs->k, largs->a1);
  return;
}
THREAD(cholesky_cont0) {
  cholesky_cont0_closure *largs = (cholesky_cont0_closure *)(args.get());
  __builtin_expect((!largs->a000), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 733, "a00")
      : ((void)0);
  cholesky_cont1_closure SN_cholesky_cont1c(largs->k);
  spawn_next<cholesky_cont1_closure> SN_cholesky_cont1(SN_cholesky_cont1c);
  cont sp0k;
  SN_BIND(SN_cholesky_cont1, &sp0k, a100);
  backsub_closure sp0c(sp0k);
  sp0c.depth1 = largs->depth2;
  sp0c.a1 = largs->a100;
  sp0c.l = largs->a000;
  spawn<backsub_closure> sp0(sp0c);

  ((cholesky_cont1_closure *)SN_cholesky_cont1.cls.get())->A2 = largs->A2;
  ((cholesky_cont1_closure *)SN_cholesky_cont1.cls.get())->a000 = largs->a000;
  ((cholesky_cont1_closure *)SN_cholesky_cont1.cls.get())->a110 = largs->a110;
  ((cholesky_cont1_closure *)SN_cholesky_cont1.cls.get())->a2 = largs->a2;
  ((cholesky_cont1_closure *)SN_cholesky_cont1.cls.get())->depth2 =
      largs->depth2;
  // Original sync was here
  return;
}
THREAD(cholesky_cont1) {
  cholesky_cont1_closure *largs = (cholesky_cont1_closure *)(args.get());
  __builtin_expect((!largs->a100), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 736, "a10")
      : ((void)0);
  cholesky_cont2_closure SN_cholesky_cont2c(largs->k);
  spawn_next<cholesky_cont2_closure> SN_cholesky_cont2(SN_cholesky_cont2c);
  cont sp0k;
  SN_BIND(SN_cholesky_cont2, &sp0k, a110);
  mul_and_subT_closure sp0c(sp0k);
  sp0c.depth0 = largs->depth2;
  sp0c.lower = 1;
  sp0c.a0 = largs->a100;
  sp0c.b = largs->a100;
  sp0c.r0 = largs->a110;
  spawn<mul_and_subT_closure> sp0(sp0c);

  ((cholesky_cont2_closure *)SN_cholesky_cont2.cls.get())->a000 = largs->a000;
  ((cholesky_cont2_closure *)SN_cholesky_cont2.cls.get())->A2 = largs->A2;
  ((cholesky_cont2_closure *)SN_cholesky_cont2.cls.get())->a2 = largs->a2;
  ((cholesky_cont2_closure *)SN_cholesky_cont2.cls.get())->a100 = largs->a100;
  ((cholesky_cont2_closure *)SN_cholesky_cont2.cls.get())->depth2 =
      largs->depth2;
  // Original sync was here
  return;
}
THREAD(cholesky_cont2) {
  cholesky_cont2_closure *largs = (cholesky_cont2_closure *)(args.get());
  __builtin_expect((!largs->a110), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 739, "a11")
      : ((void)0);
  cholesky_cont3_closure SN_cholesky_cont3c(largs->k);
  spawn_next<cholesky_cont3_closure> SN_cholesky_cont3(SN_cholesky_cont3c);
  cont sp0k;
  SN_BIND(SN_cholesky_cont3, &sp0k, a110);
  cholesky_closure sp0c(sp0k);
  sp0c.depth2 = largs->depth2;
  sp0c.a2 = largs->a110;
  spawn<cholesky_closure> sp0(sp0c);

  ((cholesky_cont3_closure *)SN_cholesky_cont3.cls.get())->depth2 =
      largs->depth2;
  ((cholesky_cont3_closure *)SN_cholesky_cont3.cls.get())->a100 = largs->a100;
  ((cholesky_cont3_closure *)SN_cholesky_cont3.cls.get())->a000 = largs->a000;
  ((cholesky_cont3_closure *)SN_cholesky_cont3.cls.get())->A2 = largs->A2;
  ((cholesky_cont3_closure *)SN_cholesky_cont3.cls.get())->a2 = largs->a2;
  // Original sync was here
  return;
}
THREAD(cholesky_cont3) {
  cholesky_cont3_closure *largs = (cholesky_cont3_closure *)(args.get());
  __builtin_expect((!largs->a110), 0)
      ? __assert_rtn(__func__, "cholesky.cpp", 742, "a11")
      : ((void)0);
  auto sp0c = std::make_shared<cholesky_afterif1_closure>(largs->k);
  sp0c->depth2 = largs->depth2;
  sp0c->a2 = largs->a2;
  sp0c->A2 = largs->A2;
  sp0c->a000 = largs->a000;
  sp0c->a100 = largs->a100;
  sp0c->a110 = largs->a110;
  cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
  return;
  return;
}
THREAD(cholesky_cont4) {
  cholesky_cont4_closure *largs = (cholesky_cont4_closure *)(args.get());
  cholesky_cont5_closure SN_cholesky_cont5c(largs->k);
  spawn_next<cholesky_cont5_closure> SN_cholesky_cont5(SN_cholesky_cont5c);
  ((cholesky_cont5_closure *)SN_cholesky_cont5.cls.get())->a110 = largs->a110;
  ((cholesky_cont5_closure *)SN_cholesky_cont5.cls.get())->A2 = largs->A2;
  ((cholesky_cont5_closure *)SN_cholesky_cont5.cls.get())->a100 = largs->a100;
  ((cholesky_cont5_closure *)SN_cholesky_cont5.cls.get())->a2 = largs->a2;
  ((cholesky_cont5_closure *)SN_cholesky_cont5.cls.get())->a000 = largs->a000;
  ((cholesky_cont5_closure *)SN_cholesky_cont5.cls.get())->depth2 =
      largs->depth2;
  // Original sync was here
  return;
}
THREAD(cholesky_cont5) {
  cholesky_cont5_closure *largs = (cholesky_cont5_closure *)(args.get());
  auto sp0c = std::make_shared<cholesky_afterif1_closure>(largs->k);
  sp0c->depth2 = largs->depth2;
  sp0c->a2 = largs->a2;
  sp0c->A2 = largs->A2;
  sp0c->a000 = largs->a000;
  sp0c->a100 = largs->a100;
  sp0c->a110 = largs->a110;
  cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
  return;
  return;
}
THREAD(main_cont0) {
  int input_nonzeros;
  int input_blocks;
  main_cont0_closure *largs = (main_cont0_closure *)(args.get());
  input_blocks = num_blocks(largs->depth3, largs->R1);
  input_nonzeros = num_nonzeros(largs->depth3, largs->R1);
  gettimeofday(&(largs->t1), 0);
  main_cont1_closure SN_main_cont1c(largs->k);
  spawn_next<main_cont1_closure> SN_main_cont1(SN_main_cont1c);
  cont sp0k;
  SN_BIND(SN_main_cont1, &sp0k, R1);
  cholesky_closure sp0c(sp0k);
  sp0c.depth2 = largs->depth3;
  sp0c.a2 = largs->R1;
  spawn<cholesky_closure> sp0(sp0c);

  ((main_cont1_closure *)SN_main_cont1.cls.get())->t2 = largs->t2;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->t1 = largs->t1;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->ok = largs->ok;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->r2 = largs->r2;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->t = largs->t;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->r1 = largs->r1;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->A3 = largs->A3;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->error = largs->error;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->fr = largs->fr;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->f = largs->f;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->res = largs->res;
  memcpy(((main_cont1_closure *)SN_main_cont1.cls.get())->filename,
         largs->filename, sizeof(largs->filename));
  memcpy(((main_cont1_closure *)SN_main_cont1.cls.get())->buf, largs->buf,
         sizeof(largs->buf));
  ((main_cont1_closure *)SN_main_cont1.cls.get())->input_nonzeros =
      input_nonzeros;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->sizex = largs->sizex;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->depth3 = largs->depth3;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->help = largs->help;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->check = largs->check;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->c0 = largs->c0;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->sizey = largs->sizey;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->nonzeros = largs->nonzeros;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->val = largs->val;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->input_blocks = input_blocks;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->argv = largs->argv;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->benchmark = largs->benchmark;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->size = largs->size;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->rnd = largs->rnd;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->fc = largs->fc;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->i = largs->i;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->argc = largs->argc;
  ((main_cont1_closure *)SN_main_cont1.cls.get())->c = largs->c;
  // Original sync was here
  return;
}
THREAD(main_cont1) {
  int output_nonzeros;
  int output_blocks;
  unsigned long long runtime_ms;
  main_cont1_closure *largs = (main_cont1_closure *)(args.get());
  gettimeofday(&(largs->t2), 0);
  runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
  printf("%f\n", (runtime_ms / 1000.));
  output_blocks = num_blocks(largs->depth3, largs->R1);
  output_nonzeros = num_nonzeros(largs->depth3, largs->R1);
  if (largs->check) {
    printf("Now check result ... \n");
    main_cont2_closure SN_main_cont2c(largs->k);
    spawn_next<main_cont2_closure> SN_main_cont2(SN_main_cont2c);
    cont sp0k;
    SN_BIND(SN_main_cont2, &sp0k, A3);
    mul_and_subT_closure sp0c(sp0k);
    sp0c.depth0 = largs->depth3;
    sp0c.lower = 1;
    sp0c.a0 = largs->R1;
    sp0c.b = largs->R1;
    sp0c.r0 = largs->A3;
    spawn<mul_and_subT_closure> sp0(sp0c);

    ((main_cont2_closure *)SN_main_cont2.cls.get())->runtime_ms = runtime_ms;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->t2 = largs->t2;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->t = largs->t;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->rnd = largs->rnd;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->val = largs->val;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->c = largs->c;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->r1 = largs->r1;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->sizey = largs->sizey;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->c0 = largs->c0;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->output_blocks =
        output_blocks;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->input_blocks =
        largs->input_blocks;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->input_nonzeros =
        largs->input_nonzeros;
    memcpy(((main_cont2_closure *)SN_main_cont2.cls.get())->buf, largs->buf,
           sizeof(largs->buf));
    ((main_cont2_closure *)SN_main_cont2.cls.get())->argv = largs->argv;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->check = largs->check;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->t1 = largs->t1;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->f = largs->f;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->benchmark =
        largs->benchmark;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->i = largs->i;
    memcpy(((main_cont2_closure *)SN_main_cont2.cls.get())->filename,
           largs->filename, sizeof(largs->filename));
    ((main_cont2_closure *)SN_main_cont2.cls.get())->nonzeros = largs->nonzeros;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->sizex = largs->sizex;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->ok = largs->ok;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->res = largs->res;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->output_nonzeros =
        output_nonzeros;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->depth3 = largs->depth3;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->size = largs->size;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->R1 = largs->R1;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->fc = largs->fc;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->r2 = largs->r2;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->help = largs->help;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->fr = largs->fr;
    ((main_cont2_closure *)SN_main_cont2.cls.get())->argc = largs->argc;
    // Original sync was here
  } else {
    auto sp1c = std::make_shared<main_afterif0_closure>(largs->k);
    sp1c->argc = largs->argc;
    sp1c->argv = largs->argv;
    sp1c->A3 = largs->A3;
    sp1c->R1 = largs->R1;
    sp1c->size = largs->size;
    sp1c->depth3 = largs->depth3;
    sp1c->nonzeros = largs->nonzeros;
    sp1c->i = largs->i;
    sp1c->benchmark = largs->benchmark;
    sp1c->help = largs->help;
    sp1c->check = largs->check;
    sp1c->input_nonzeros = largs->input_nonzeros;
    sp1c->input_blocks = largs->input_blocks;
    sp1c->output_nonzeros = output_nonzeros;
    sp1c->output_blocks = output_blocks;
    sp1c->error = largs->error;
    memcpy(sp1c->buf, largs->buf, sizeof(sp1c->buf));
    memcpy(sp1c->filename, largs->filename, sizeof(sp1c->filename));
    sp1c->sizex = largs->sizex;
    sp1c->sizey = largs->sizey;
    sp1c->f = largs->f;
    sp1c->fr = largs->fr;
    sp1c->fc = largs->fc;
    sp1c->r1 = largs->r1;
    sp1c->c = largs->c;
    sp1c->val = largs->val;
    sp1c->res = largs->res;
    sp1c->rnd = largs->rnd;
    sp1c->t = largs->t;
    sp1c->r2 = largs->r2;
    sp1c->c0 = largs->c0;
    sp1c->ok = largs->ok;
    sp1c->t1 = largs->t1;
    sp1c->t2 = largs->t2;
    sp1c->runtime_ms = runtime_ms;
    cilk_spawn taskSpawn(sp1c->getTask(), sp1c);
    return;
  }
  return;
}
THREAD(main_cont2) {
  Real error;
  main_cont2_closure *largs = (main_cont2_closure *)(args.get());
  error = mag(largs->depth3, largs->A3);
  auto sp0c = std::make_shared<main_afterif0_closure>(largs->k);
  sp0c->argc = largs->argc;
  sp0c->argv = largs->argv;
  sp0c->A3 = largs->A3;
  sp0c->R1 = largs->R1;
  sp0c->size = largs->size;
  sp0c->depth3 = largs->depth3;
  sp0c->nonzeros = largs->nonzeros;
  sp0c->i = largs->i;
  sp0c->benchmark = largs->benchmark;
  sp0c->help = largs->help;
  sp0c->check = largs->check;
  sp0c->input_nonzeros = largs->input_nonzeros;
  sp0c->input_blocks = largs->input_blocks;
  sp0c->output_nonzeros = largs->output_nonzeros;
  sp0c->output_blocks = largs->output_blocks;
  sp0c->error = error;
  memcpy(sp0c->buf, largs->buf, sizeof(sp0c->buf));
  memcpy(sp0c->filename, largs->filename, sizeof(sp0c->filename));
  sp0c->sizex = largs->sizex;
  sp0c->sizey = largs->sizey;
  sp0c->f = largs->f;
  sp0c->fr = largs->fr;
  sp0c->fc = largs->fc;
  sp0c->r1 = largs->r1;
  sp0c->c = largs->c;
  sp0c->val = largs->val;
  sp0c->res = largs->res;
  sp0c->rnd = largs->rnd;
  sp0c->t = largs->t;
  sp0c->r2 = largs->r2;
  sp0c->c0 = largs->c0;
  sp0c->ok = largs->ok;
  sp0c->t1 = largs->t1;
  sp0c->t2 = largs->t2;
  sp0c->runtime_ms = largs->runtime_ms;
  cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
  return;
  return;
}
