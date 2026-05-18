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
InternalNode * new_block_leaf();
InternalNode * new_internal(InternalNode *a00, InternalNode *a01, InternalNode *a10, InternalNode *a11);
THREAD(copy_matrix);
void free_matrix(int depth, Matrix a);
Real get_matrix(int depth, Matrix a, int r, int c);
Matrix set_matrix(int depth, Matrix a, int r, int c, Real value);
int num_blocks(int depth, Matrix a);
int num_nonzeros(int depth, Matrix a);
Real mag(int depth, Matrix a);
THREAD(mul_and_subT);
THREAD(backsub);
THREAD(cholesky);
int logarithm(int size);
int usage();
int main(int argc, char **argv);
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

CLOSURE_DEF(copy_matrix,
    int depth;
    Matrix a;
);
CLOSURE_DEF(mul_and_subT,
    int depth;
    int lower;
    Matrix a;
    Matrix b;
    Matrix r;
);
CLOSURE_DEF(backsub,
    int depth;
    Matrix a;
    Matrix l;
);
CLOSURE_DEF(cholesky,
    int depth;
    Matrix a;
);
CLOSURE_DEF(main_afterif0,
    int argc;
    char **argv;
    Matrix A;
    Matrix R;
    int size;
    int depth;
    int nonzeros;
    int i;
    int benchmark;
    int help;
    int check;
    int input_nonzeros;
    int input_blocks;
    int output_nonzeros;
    int output_blocks;
    Real error;
    char buf[1000];
    char filename[100];
    int sizex;
    int sizey;
    FILE *f;
    double fr;
    double fc;
    int r;
    int c;
    Real val;
    int res;
    double rnd;
    int t;
    int r0;
    int c0;
    struct timeval t1;
    struct timeval t2;
    unsigned long long runtime_ms;
);
CLOSURE_DEF(cholesky_afterif1,
    int depth;
    Matrix a;
    LeafNode *A;
    Matrix a00;
    Matrix a10;
    Matrix a11;
);
CLOSURE_DEF(copy_matrix_cont0,
    Matrix r00;
    Matrix r01;
    Matrix r10;
    Matrix r11;
);
CLOSURE_DEF(copy_matrix_cont1,
    Matrix r00;
    Matrix r01;
    Matrix r10;
    Matrix r11;
);
CLOSURE_DEF(mul_and_subT_cont0,
    int depth;
    int lower;
    Matrix a;
    Matrix b;
    Matrix r;
    Matrix r00;
    Matrix r01;
    Matrix r10;
    Matrix r11;
);
CLOSURE_DEF(mul_and_subT_cont1,
    Matrix r;
    Matrix r00;
    Matrix r01;
    Matrix r10;
    Matrix r11;
);
CLOSURE_DEF(backsub_cont0,
    int depth;
    Matrix a;
    Matrix a00;
    Matrix a01;
    Matrix a10;
    Matrix a11;
    Matrix l10;
    Matrix l11;
);
CLOSURE_DEF(backsub_cont1,
    int depth;
    Matrix a;
    Matrix a00;
    Matrix a01;
    Matrix a10;
    Matrix a11;
    Matrix l11;
);
CLOSURE_DEF(backsub_cont2,
    Matrix a;
    Matrix a00;
    Matrix a01;
    Matrix a10;
    Matrix a11;
);
CLOSURE_DEF(cholesky_cont0,
    int depth;
    Matrix a;
    LeafNode *A;
    Matrix a00;
    Matrix a10;
    Matrix a11;
);
CLOSURE_DEF(cholesky_cont1,
    int depth;
    Matrix a;
    LeafNode *A;
    Matrix a00;
    Matrix a10;
    Matrix a11;
);
CLOSURE_DEF(cholesky_cont2,
    int depth;
    Matrix a;
    LeafNode *A;
    Matrix a00;
    Matrix a10;
    Matrix a11;
);
CLOSURE_DEF(cholesky_cont3,
    int depth;
    Matrix a;
    LeafNode *A;
    Matrix a00;
    Matrix a10;
    Matrix a11;
);
CLOSURE_DEF(cholesky_cont4,
    int depth;
    Matrix a;
    LeafNode *A;
    Matrix a00;
    Matrix a10;
    Matrix a11;
);
CLOSURE_DEF(cholesky_cont5,
    int depth;
    Matrix a;
    LeafNode *A;
    Matrix a00;
    Matrix a10;
    Matrix a11;
);
CLOSURE_DEF(main_cont0,
    int argc;
    char **argv;
    Matrix A;
    Matrix R;
    int size;
    int depth;
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
    int r;
    int c;
    Real val;
    int res;
    double rnd;
    int t;
    int r0;
    int c0;
    struct timeval t1;
    struct timeval t2;
);
CLOSURE_DEF(main_cont1,
    int argc;
    char **argv;
    Matrix A;
    Matrix R;
    int size;
    int depth;
    int nonzeros;
    int i;
    int benchmark;
    int help;
    int check;
    int input_nonzeros;
    int input_blocks;
    Real error;
    char buf[1000];
    char filename[100];
    int sizex;
    int sizey;
    FILE *f;
    double fr;
    double fc;
    int r;
    int c;
    Real val;
    int res;
    double rnd;
    int t;
    int r0;
    int c0;
    struct timeval t1;
    struct timeval t2;
);
CLOSURE_DEF(main_cont2,
    int argc;
    char **argv;
    Matrix A;
    Matrix R;
    int size;
    int depth;
    int nonzeros;
    int i;
    int benchmark;
    int help;
    int check;
    int input_nonzeros;
    int input_blocks;
    int output_nonzeros;
    int output_blocks;
    char buf[1000];
    char filename[100];
    int sizex;
    int sizey;
    FILE *f;
    double fr;
    double fc;
    int r;
    int c;
    Real val;
    int res;
    double rnd;
    int t;
    int r0;
    int c0;
    struct timeval t1;
    struct timeval t2;
    unsigned long long runtime_ms;
);


unsigned long rand_nxt = 0;




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
  for (i = 0; i < BLOCK_SIZE; i++)
    for (k = 0; k <= i; k++) {
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
    for (i = 0; i < BLOCK_SIZE; i++)
      for (j = 0; j < BLOCK_SIZE; j++)
        printf("%6d %6d: %12f\n", r + i, c + j, BLOCK(A->block, i, j));

  } else {
    int mid;
    depth--;
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
    rand_nxt = rand_nxt * 1103515245 + 12345;
    result = ((rand_nxt >> 16) % (((unsigned int) 2147483647) + 1));
    return result;
}
void cilk_srand(unsigned int seed) {
    rand_nxt = seed;
}
void block_schur_full(Block B, Block A, Block C) {
    int i;
    int j;
    int k0;
    for (i = 0;(i < (1 << 2));(i++)) {
        for (j = 0;(j < (1 << 2));(j++)) {
            for (k0 = 0;(k0 < (1 << 2));(k0++)) {
                (B[i][j]) -= (A[i][k0]) * (C[j][k0]);
            }
        }
    }
}
void block_schur_half(Block B, Block A, Block C) {
    int i;
    int j;
    int k0;
    for (i = 0;(i < (1 << 2));(i++)) {
        for (j = 0;(j <= i);(j++)) {
            for (k0 = 0;(k0 < (1 << 2));(k0++)) {
                (B[i][j]) -= (A[i][k0]) * (C[j][k0]);
            }
        }
    }
}
void block_backsub(Block B, Block U) {
    int i;
    int j;
    int k0;
    for (i = 0;(i < (1 << 2));(i++)) {
        for (j = 0;(j < (1 << 2));(j++)) {
            for (k0 = 0;(k0 < i);(k0++)) {
                (B[j][i]) -= (U[i][k0]) * (B[j][k0]);
            }
            (B[j][i]) /= (U[i][i]);
        }
    }
}
void block_cholesky(Block B) {
    int i;
    int j;
    int k0;
    Real x;
    for (k0 = 0;(k0 < (1 << 2));(k0++)) {
        if ((B[k0][k0] < 0.)) {
            printf("sqrt error: %f\n",B[k0][k0]);
            printf("matrix is probably not numerically stable\n");
            exit(9);
        }
        x = sqrt(B[k0][k0]);
        for (i = k0;(i < (1 << 2));(i++)) {
            (B[i][k0]) /= x;
        }
        for (j = (k0 + 1);(j < (1 << 2));(j++)) {
            for (i = j;(i < (1 << 2));(i++)) {
                (B[i][j]) -= (B[i][k0]) * (B[j][k0]);
                if (((j > i) && (B[i][j] != 0.))) {
                    printf("Upper not empty\n");
                }
            }
        }
    }
}
void block_zero(Block B) {
    int i;
    int k0;
    for (i = 0;(i < (1 << 2));(i++)) {
        for (k0 = 0;(k0 < (1 << 2));(k0++)) {
            B[i][k0] = 0.;
        }
    }
}
InternalNode * new_block_leaf() {
    LeafNode *leaf;
    leaf = ((LeafNode *) malloc(sizeof(LeafNode)));
    if ((leaf == __null)) {
        printf("out of memory!\n");
        exit(1);
    }
    return ((InternalNode *) leaf);
}
InternalNode * new_internal(InternalNode *a00, InternalNode *a01, InternalNode *a10, InternalNode *a11) {
    InternalNode *node;
    node = ((InternalNode *) malloc(sizeof(InternalNode)));
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
    copy_matrix_closure *largs = (copy_matrix_closure*)(args.get());
    if ((!largs->a)) {
        SEND_ARGUMENT(largs->k, largs->a);
    } else {
        if ((largs->depth == 2)) {
            A = ((LeafNode *) largs->a);
            r = new_block_leaf();
            R = ((LeafNode *) r);
            memcpy(R->block,A->block,sizeof(Block));
            SEND_ARGUMENT(largs->k, r);
        } else {
            r00 = __null;
            r01 = __null;
            r10 = __null;
            r11 = __null;
            (largs->depth--);
            copy_matrix_cont0_closure SN_copy_matrix_cont0c(largs->k);
            spawn_next<copy_matrix_cont0_closure> SN_copy_matrix_cont0(SN_copy_matrix_cont0c);
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
}
void free_matrix(int depth, Matrix a) {
    if ((a == __null)) {
        return ;
    } else {
        if ((depth == 2)) {
            free(a);
        } else {
            (depth--);
            free_matrix(depth,a->child[0]);
            free_matrix(depth,a->child[1]);
            free_matrix(depth,a->child[2]);
            free_matrix(depth,a->child[3]);
            free(a);
        }
    }
}
Real get_matrix(int depth, Matrix a, int r, int c) {
    LeafNode *A;
    int mid;
    __builtin_expect(!(depth >= 2), 0) ? __assert_rtn(__func__, "cholesky.cpp", 342, "depth >= BLOCK_DEPTH") : (void)0;
    __builtin_expect(!(r < (1 << depth)), 0) ? __assert_rtn(__func__, "cholesky.cpp", 343, "r < (1 << depth)") : (void)0;
    __builtin_expect(!(c < (1 << depth)), 0) ? __assert_rtn(__func__, "cholesky.cpp", 344, "c < (1 << depth)") : (void)0;
    if ((a == __null)) {
        return 0.;
    } else {
        if ((depth == 2)) {
            A = ((LeafNode *) a);
            return A->block[r][c];
        } else {
            (depth--);
            mid = (1 << depth);
            if ((r < mid)) {
                if ((c < mid)) {
                    return get_matrix(depth,a->child[0],r,c);
                } else {
                    return get_matrix(depth,a->child[1],r,(c - mid));
                }
            } else {
                if ((c < mid)) {
                    return get_matrix(depth,a->child[2],(r - mid),c);
                } else {
                    return get_matrix(depth,a->child[3],(r - mid),(c - mid));
                }
            }
        }
    }
}
Matrix set_matrix(int depth, Matrix a, int r, int c, Real value) {
    LeafNode *A;
    int mid;
    __builtin_expect(!(depth >= 2), 0) ? __assert_rtn(__func__, "cholesky.cpp", 378, "depth >= BLOCK_DEPTH") : (void)0;
    __builtin_expect(!(r < (1 << depth)), 0) ? __assert_rtn(__func__, "cholesky.cpp", 379, "r < (1 << depth)") : (void)0;
    __builtin_expect(!(c < (1 << depth)), 0) ? __assert_rtn(__func__, "cholesky.cpp", 380, "c < (1 << depth)") : (void)0;
    if ((depth == 2)) {
        if ((a == __null)) {
            a = new_block_leaf();
            A = ((LeafNode *) a);
            block_zero(A->block);
        } else {
            A = ((LeafNode *) a);
        }
        A->block[r][c] = value;
    } else {
        if ((a == __null)) {
            a = new_internal(__null,__null,__null,__null);
        }
        (depth--);
        mid = (1 << depth);
        if ((r < mid)) {
            if ((c < mid)) {
                a->child[0] = set_matrix(depth,a->child[0],r,c,value);
            } else {
                a->child[1] = set_matrix(depth,a->child[1],r,(c - mid),value);
            }
        } else {
            if ((c < mid)) {
                a->child[2] = set_matrix(depth,a->child[2],(r - mid),c,value);
            } else {
                a->child[3] = set_matrix(depth,a->child[3],(r - mid),(c - mid),value);
            }
        }
    }
    return a;
}
int num_blocks(int depth, Matrix a) {
    int res;
    if ((a == __null)) {
        return 0;
    } else {
        if ((depth == 2)) {
            return 1;
        } else {
            (depth--);
            res = 0;
            res = (res + num_blocks(depth,a->child[0]));
            res = (res + num_blocks(depth,a->child[1]));
            res = (res + num_blocks(depth,a->child[2]));
            res = (res + num_blocks(depth,a->child[3]));
            return res;
        }
    }
}
int num_nonzeros(int depth, Matrix a) {
    int res;
    LeafNode *A;
    int i;
    int j;
    if ((a == __null)) {
        return 0;
    } else {
        if ((depth == 2)) {
            A = ((LeafNode *) a);
            res = 0;
            for (i = 0;(i < (1 << 2));(i++)) {
                for (j = 0;(j < (1 << 2));(j++)) {
                    if ((A->block[i][j] != 0.)) {
                        (res++);
                    }
                }
            }
            return res;
        } else {
            (depth--);
            res = 0;
            res = (res + num_nonzeros(depth,a->child[0]));
            res = (res + num_nonzeros(depth,a->child[1]));
            res = (res + num_nonzeros(depth,a->child[2]));
            res = (res + num_nonzeros(depth,a->child[3]));
            return res;
        }
    }
}
Real mag(int depth, Matrix a) {
    Real res;
    LeafNode *A;
    int i;
    int j;
    res = 0.;
    if ((!a)) {
        return res;
    } else {
        if ((depth == 2)) {
            A = ((LeafNode *) a);
            for (i = 0;(i < (1 << 2));(i++)) {
                for (j = 0;(j < (1 << 2));(j++)) {
                    res = (res + (A->block[i][j] * A->block[i][j]));
                }
            }
        } else {
            (depth--);
            res = (res + mag(depth,a->child[0]));
            res = (res + mag(depth,a->child[1]));
            res = (res + mag(depth,a->child[2]));
            res = (res + mag(depth,a->child[3]));
        }
        return res;
    }
}
THREAD(mul_and_subT) {
    LeafNode *A;
    LeafNode *B;
    LeafNode *R;
    Matrix r00;
    Matrix r01;
    Matrix r10;
    Matrix r11;
    mul_and_subT_closure *largs = (mul_and_subT_closure*)(args.get());
    __builtin_expect(!(largs->a != __null && largs->b != __null), 0) ? __assert_rtn(__func__, "cholesky.cpp", 535, "a != NULL && b != NULL") : (void)0;
    if ((largs->depth == 2)) {
        A = ((LeafNode *) largs->a);
        B = ((LeafNode *) largs->b);
        if ((largs->r == __null)) {
            largs->r = new_block_leaf();
            R = ((LeafNode *) largs->r);
            block_zero(R->block);
        } else {
            R = ((LeafNode *) largs->r);
        }
        if (largs->lower) {
            block_schur_half(R->block,A->block,B->block);
        } else {
            block_schur_full(R->block,A->block,B->block);
        }
        SEND_ARGUMENT(largs->k, largs->r);
    } else {
        (largs->depth--);
        mul_and_subT_cont0_closure SN_mul_and_subT_cont0c(largs->k);
        spawn_next<mul_and_subT_cont0_closure> SN_mul_and_subT_cont0(SN_mul_and_subT_cont0c);
        if ((largs->r != __null)) {
            r00 = largs->r->child[0];
            r01 = largs->r->child[1];
            r10 = largs->r->child[2];
            r11 = largs->r->child[3];
        } else {
            r00 = __null;
            r01 = __null;
            r10 = __null;
            r11 = __null;
        }
        if ((largs->a->child[0] && largs->b->child[0])) {
            cont sp0k;
            SN_BIND(SN_mul_and_subT_cont0, &sp0k, r00);
            mul_and_subT_closure sp0c(sp0k);
            sp0c.depth = largs->depth;
            sp0c.lower = largs->lower;
            sp0c.a = largs->a->child[0];
            sp0c.b = largs->b->child[0];
            sp0c.r = r00;
            spawn<mul_and_subT_closure> sp0(sp0c);

        }
        if ((((!largs->lower) && largs->a->child[0]) && largs->b->child[2])) {
            cont sp1k;
            SN_BIND(SN_mul_and_subT_cont0, &sp1k, r01);
            mul_and_subT_closure sp1c(sp1k);
            sp1c.depth = largs->depth;
            sp1c.lower = 0;
            sp1c.a = largs->a->child[0];
            sp1c.b = largs->b->child[2];
            sp1c.r = r01;
            spawn<mul_and_subT_closure> sp1(sp1c);

        }
        if ((largs->a->child[2] && largs->b->child[0])) {
            cont sp2k;
            SN_BIND(SN_mul_and_subT_cont0, &sp2k, r10);
            mul_and_subT_closure sp2c(sp2k);
            sp2c.depth = largs->depth;
            sp2c.lower = 0;
            sp2c.a = largs->a->child[2];
            sp2c.b = largs->b->child[0];
            sp2c.r = r10;
            spawn<mul_and_subT_closure> sp2(sp2c);

        }
        if ((largs->a->child[2] && largs->b->child[2])) {
            cont sp3k;
            SN_BIND(SN_mul_and_subT_cont0, &sp3k, r11);
            mul_and_subT_closure sp3c(sp3k);
            sp3c.depth = largs->depth;
            sp3c.lower = largs->lower;
            sp3c.a = largs->a->child[2];
            sp3c.b = largs->b->child[2];
            sp3c.r = r11;
            spawn<mul_and_subT_closure> sp3(sp3c);

        }
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->r10 = r10;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->r01 = r01;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->r00 = r00;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->r11 = r11;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->a = largs->a;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->r = largs->r;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->b = largs->b;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->lower = largs->lower;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->depth = largs->depth;
        // Original sync was here
    }
}
THREAD(backsub) {
    LeafNode *A;
    LeafNode *L;
    Matrix a00;
    Matrix a01;
    Matrix a10;
    Matrix a11;
    Matrix l00;
    Matrix l10;
    Matrix l11;
    backsub_closure *largs = (backsub_closure*)(args.get());
    __builtin_expect(!(largs->a != __null), 0) ? __assert_rtn(__func__, "cholesky.cpp", 641, "a != NULL") : (void)0;
    __builtin_expect(!(largs->l != __null), 0) ? __assert_rtn(__func__, "cholesky.cpp", 642, "l != NULL") : (void)0;
    if ((largs->depth == 2)) {
        A = ((LeafNode *) largs->a);
        L = ((LeafNode *) largs->l);
        block_backsub(A->block,L->block);
        SEND_ARGUMENT(largs->k, largs->a);
    } else {
        (largs->depth--);
        a00 = largs->a->child[0];
        a01 = largs->a->child[1];
        a10 = largs->a->child[2];
        a11 = largs->a->child[3];
        l00 = largs->l->child[0];
        l10 = largs->l->child[2];
        l11 = largs->l->child[3];
        __builtin_expect(!(l00 && l11), 0) ? __assert_rtn(__func__, "cholesky.cpp", 666, "l00 && l11") : (void)0;
        backsub_cont0_closure SN_backsub_cont0c(largs->k);
        spawn_next<backsub_cont0_closure> SN_backsub_cont0(SN_backsub_cont0c);
        if (a00) {
            cont sp0k;
            SN_BIND(SN_backsub_cont0, &sp0k, a00);
            backsub_closure sp0c(sp0k);
            sp0c.depth = largs->depth;
            sp0c.a = a00;
            sp0c.l = l00;
            spawn<backsub_closure> sp0(sp0c);

        }
        if (a10) {
            cont sp1k;
            SN_BIND(SN_backsub_cont0, &sp1k, a10);
            backsub_closure sp1c(sp1k);
            sp1c.depth = largs->depth;
            sp1c.a = a10;
            sp1c.l = l00;
            spawn<backsub_closure> sp1(sp1c);

        }
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->a11 = a11;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->a = largs->a;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->a01 = a01;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->a10 = a10;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->l11 = l11;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->a00 = a00;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->l10 = l10;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->depth = largs->depth;
        // Original sync was here
    }
}
THREAD(cholesky) {
    LeafNode *A;
    Matrix a00;
    Matrix a10;
    Matrix a11;
    cholesky_closure *largs = (cholesky_closure*)(args.get());
    __builtin_expect(!(largs->a != __null), 0) ? __assert_rtn(__func__, "cholesky.cpp", 705, "a != NULL") : (void)0;
    if ((largs->depth == 2)) {
        A = ((LeafNode *) largs->a);
        block_cholesky(A->block);
        SEND_ARGUMENT(largs->k, largs->a);
    } else {
        (largs->depth--);
        a00 = largs->a->child[0];
        a10 = largs->a->child[2];
        a11 = largs->a->child[3];
        __builtin_expect(!(a00), 0) ? __assert_rtn(__func__, "cholesky.cpp", 722, "a00") : (void)0;
        if ((!a10)) {
            cholesky_cont4_closure SN_cholesky_cont4c(largs->k);
            spawn_next<cholesky_cont4_closure> SN_cholesky_cont4(SN_cholesky_cont4c);
            cont sp0k;
            SN_BIND(SN_cholesky_cont4, &sp0k, a00);
            cholesky_closure sp0c(sp0k);
            sp0c.depth = largs->depth;
            sp0c.a = a00;
            spawn<cholesky_closure> sp0(sp0c);

            cont sp1k;
            SN_BIND(SN_cholesky_cont4, &sp1k, a11);
            cholesky_closure sp1c(sp1k);
            sp1c.depth = largs->depth;
            sp1c.a = a11;
            spawn<cholesky_closure> sp1(sp1c);

            ((cholesky_cont4_closure*)SN_cholesky_cont4.cls.get())->A = A;
            ((cholesky_cont4_closure*)SN_cholesky_cont4.cls.get())->a10 = a10;
            ((cholesky_cont4_closure*)SN_cholesky_cont4.cls.get())->a = largs->a;
            ((cholesky_cont4_closure*)SN_cholesky_cont4.cls.get())->depth = largs->depth;
            // Original sync was here
        } else {
            cholesky_cont0_closure SN_cholesky_cont0c(largs->k);
            spawn_next<cholesky_cont0_closure> SN_cholesky_cont0(SN_cholesky_cont0c);
            cont sp2k;
            SN_BIND(SN_cholesky_cont0, &sp2k, a00);
            cholesky_closure sp2c(sp2k);
            sp2c.depth = largs->depth;
            sp2c.a = a00;
            spawn<cholesky_closure> sp2(sp2c);

            ((cholesky_cont0_closure*)SN_cholesky_cont0.cls.get())->a11 = a11;
            ((cholesky_cont0_closure*)SN_cholesky_cont0.cls.get())->A = A;
            ((cholesky_cont0_closure*)SN_cholesky_cont0.cls.get())->a10 = a10;
            ((cholesky_cont0_closure*)SN_cholesky_cont0.cls.get())->a = largs->a;
            ((cholesky_cont0_closure*)SN_cholesky_cont0.cls.get())->depth = largs->depth;
            // Original sync was here
        }
    }
}
int logarithm(int size) {
    int k0;
    k0 = 0;
    while (((1 << k0) < size)) {
        (k0++);
    }
    return k0;
}
int usage() {
    fprintf(__stderrp,"\nUsage: cholesky [<cilk-options>] [-n size] [-z nonzeros]\n                [-f filename] [-benchmark] [-h]\n\nDefault: cholesky -n 500 -z 1000\n\nThis program performs a divide and conquer Cholesky factorization of a\nsparse symmetric positive definite matrix (A=LL^T).  Using the fact\nthat the matrix is symmetric, Cholesky does half the number of\noperations of LU.  The method used is the same as with LU, with work\nTheta(n^3) and critical path Theta(n lg(n)) for the dense case.  A\n");
    fprintf(__stderrp,"quad-tree is used to store the nonzero entries of the sparse\nmatrix. Actual work and critical path are influenced by the sparsity\n pattern of the matrix.\n\nThe input matrix is either read from the provided file or generated\nrandomly with size and nonzero-elements as specified.\n\n");
    return 1;
}
int main(int argc, char **argv) {
    Matrix A;
    Matrix R;
    int size;
    int depth;
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
    int r;
    int c;
    Real val;
    int res;
    double rnd;
    int t;
    int r0;
    int c0;
    check = 1;
    error = 0.;
    A = __null;
    filename[0] = 0;
    size = 500;
    nonzeros = 1000;
    get_options(argc,argv,specifiers,opt_types,&(size),&(nonzeros),&(check),filename,&(benchmark),&(help));
    if (help) {
        return usage();
    } else {
        main_cont0_closure SN_main_cont0c(CONT_DUMMY);
        spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
        if (benchmark) {
            switch (benchmark) {
  case 1:
    filename[0] = 0;
    size = 128;
    nonzeros = 100;
    break;
  case 2:
    filename[0] = 0;
    size = 1000;
    nonzeros = 10000;
    break;
  case 3:
    filename[0] = 0;
    size = 2000;
    nonzeros = 10000;
    break;
}
;
        }
        if (filename[0]) {
            f = fopen(filename,"r");
            if ((f == __null)) {
                printf("\nFile not found!\n\n");
                return 1;
            } else {
                do 
    fgets(buf, 1000, f);
while (buf[0] == '%');
;
                sscanf(buf,"%d %d",&(sizex),&(sizey));
                __builtin_expect(!(sizex == sizey), 0) ? __assert_rtn(__func__, "cholesky.cpp", 843, "sizex == sizey") : (void)0;
                size = sizex;
                depth = logarithm(size);
                srand(61066);
                cilk_srand(61066);
                nonzeros = 0;
                while ((!feof(f))) {
                    fgets(buf,1000,f);
                    res = sscanf(buf,"%lf %lf %lf",&(fr),&(fc),&(val));
                    r = fr;
                    c = fc;
                    if ((res <= 0)) {
                        break;
                    }
                    if ((res == 2)) {
                        rnd = (((double) rand()) / ((double) 2147483647));
                        val = r == c ? 1.0E+5 * rnd : rnd;
                    }
                    (r--);
                    (c--);
                    if ((r < c)) {
                        t = r;
                        r = c;
                        c = t;
                    }
                    __builtin_expect(!(r >= c), 0) ? __assert_rtn(__func__, "cholesky.cpp", 884, "r >= c") : (void)0;
                    __builtin_expect(!(r < size), 0) ? __assert_rtn(__func__, "cholesky.cpp", 885, "r < size") : (void)0;
                    __builtin_expect(!(c < size), 0) ? __assert_rtn(__func__, "cholesky.cpp", 886, "c < size") : (void)0;
                    A = set_matrix(depth,A,r,c,val);
                    (nonzeros++);
                }
            }
        } else {
            depth = logarithm(size);
            for (i = 0;(i < size);(i++)) {
                A = set_matrix(depth,A,i,i,1.);
            }
            for (i = 0;(i < (nonzeros - size));(i++)) {
                again:
r0 = cilk_rand() % size;
;
                c0 = (cilk_rand() % size);
                if ((r0 <= c0)) {
                    goto again;
;
                }
                if ((get_matrix(depth,A,r0,c0) != 0.)) {
                    goto again;
;
                }
                A = set_matrix(depth,A,r0,c0,0.10000000000000001);
            }
        }
        for (i = size;(i < (1 << depth));(i++)) {
            A = set_matrix(depth,A,i,i,1.);
        }
        cont sp0k;
        SN_BIND(SN_main_cont0, &sp0k, R);
        copy_matrix_closure sp0c(sp0k);
        sp0c.depth = depth;
        sp0c.a = A;
        spawn<copy_matrix_closure> sp0(sp0c);

        ((main_cont0_closure*)SN_main_cont0.cls.get())->c0 = c0;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->r0 = r0;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->t = t;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->rnd = rnd;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->val = val;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->sizey = sizey;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->res = res;
        std::memcpy(((main_cont0_closure*)SN_main_cont0.cls.get())->filename, filename, sizeof(filename));
        ((main_cont0_closure*)SN_main_cont0.cls.get())->f = f;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->fc = fc;
        std::memcpy(((main_cont0_closure*)SN_main_cont0.cls.get())->buf, buf, sizeof(buf));
        ((main_cont0_closure*)SN_main_cont0.cls.get())->fr = fr;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->sizex = sizex;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->benchmark = benchmark;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->size = size;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->r = r;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->help = help;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->i = i;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->check = check;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->nonzeros = nonzeros;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->depth = depth;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->A = A;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->error = error;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->c = c;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->argv = argv;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->argc = argc;
        // Original sync was here
    }
}
THREAD(main_afterif0) {
    main_afterif0_closure *largs = (main_afterif0_closure*)(args.get());
    fprintf(__stderrp,"\nCilk Example: cholesky\n");
    if (largs->check) {
        printf("Error: %f\n\n",largs->error);
    }
    fprintf(__stderrp,"Options: original size     = %d\n",largs->size);
    fprintf(__stderrp,"         original nonzeros = %d\n",largs->nonzeros);
    fprintf(__stderrp,"         input nonzeros    = %d\n",largs->input_nonzeros);
    fprintf(__stderrp,"         input blocks      = %d\n",largs->input_blocks);
    fprintf(__stderrp,"         output nonzeros   = %d\n",largs->output_nonzeros);
    fprintf(__stderrp,"         output blocks     = %d\n\n",largs->output_blocks);
    free_matrix(largs->depth,largs->A);
    free_matrix(largs->depth,largs->R);
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(cholesky_afterif1) {
    cholesky_afterif1_closure *largs = (cholesky_afterif1_closure*)(args.get());
    largs->a->child[0] = largs->a00;
    largs->a->child[2] = largs->a10;
    largs->a->child[3] = largs->a11;
    SEND_ARGUMENT(largs->k, largs->a);
}
THREAD(copy_matrix_cont0) {
    copy_matrix_cont0_closure *largs = (copy_matrix_cont0_closure*)(args.get());
    copy_matrix_cont1_closure SN_copy_matrix_cont1c(largs->k);
    spawn_next<copy_matrix_cont1_closure> SN_copy_matrix_cont1(SN_copy_matrix_cont1c);
    ((copy_matrix_cont1_closure*)SN_copy_matrix_cont1.cls.get())->r11 = largs->r11;
    ((copy_matrix_cont1_closure*)SN_copy_matrix_cont1.cls.get())->r10 = largs->r10;
    ((copy_matrix_cont1_closure*)SN_copy_matrix_cont1.cls.get())->r01 = largs->r01;
    ((copy_matrix_cont1_closure*)SN_copy_matrix_cont1.cls.get())->r00 = largs->r00;
    // Original sync was here
    return;
}
THREAD(copy_matrix_cont1) {
    Matrix r;
    copy_matrix_cont1_closure *largs = (copy_matrix_cont1_closure*)(args.get());
    r = new_internal(largs->r00,largs->r01,largs->r10,largs->r11);
    SEND_ARGUMENT(largs->k, r);
}
THREAD(mul_and_subT_cont0) {
    mul_and_subT_cont0_closure *largs = (mul_and_subT_cont0_closure*)(args.get());
    mul_and_subT_cont1_closure SN_mul_and_subT_cont1c(largs->k);
    spawn_next<mul_and_subT_cont1_closure> SN_mul_and_subT_cont1(SN_mul_and_subT_cont1c);
    if ((largs->a->child[1] && largs->b->child[1])) {
        cont sp0k;
        SN_BIND(SN_mul_and_subT_cont1, &sp0k, r00);
        mul_and_subT_closure sp0c(sp0k);
        sp0c.depth = largs->depth;
        sp0c.lower = largs->lower;
        sp0c.a = largs->a->child[1];
        sp0c.b = largs->b->child[1];
        sp0c.r = largs->r00;
        spawn<mul_and_subT_closure> sp0(sp0c);

    }
    if ((((!largs->lower) && largs->a->child[1]) && largs->b->child[3])) {
        cont sp1k;
        SN_BIND(SN_mul_and_subT_cont1, &sp1k, r01);
        mul_and_subT_closure sp1c(sp1k);
        sp1c.depth = largs->depth;
        sp1c.lower = 0;
        sp1c.a = largs->a->child[1];
        sp1c.b = largs->b->child[3];
        sp1c.r = largs->r01;
        spawn<mul_and_subT_closure> sp1(sp1c);

    }
    if ((largs->a->child[3] && largs->b->child[1])) {
        cont sp2k;
        SN_BIND(SN_mul_and_subT_cont1, &sp2k, r10);
        mul_and_subT_closure sp2c(sp2k);
        sp2c.depth = largs->depth;
        sp2c.lower = 0;
        sp2c.a = largs->a->child[3];
        sp2c.b = largs->b->child[1];
        sp2c.r = largs->r10;
        spawn<mul_and_subT_closure> sp2(sp2c);

    }
    if ((largs->a->child[3] && largs->b->child[3])) {
        cont sp3k;
        SN_BIND(SN_mul_and_subT_cont1, &sp3k, r11);
        mul_and_subT_closure sp3c(sp3k);
        sp3c.depth = largs->depth;
        sp3c.lower = largs->lower;
        sp3c.a = largs->a->child[3];
        sp3c.b = largs->b->child[3];
        sp3c.r = largs->r11;
        spawn<mul_and_subT_closure> sp3(sp3c);

    }
    ((mul_and_subT_cont1_closure*)SN_mul_and_subT_cont1.cls.get())->r11 = largs->r11;
    ((mul_and_subT_cont1_closure*)SN_mul_and_subT_cont1.cls.get())->r01 = largs->r01;
    ((mul_and_subT_cont1_closure*)SN_mul_and_subT_cont1.cls.get())->r00 = largs->r00;
    ((mul_and_subT_cont1_closure*)SN_mul_and_subT_cont1.cls.get())->r10 = largs->r10;
    ((mul_and_subT_cont1_closure*)SN_mul_and_subT_cont1.cls.get())->r = largs->r;
    // Original sync was here
    return;
}
THREAD(mul_and_subT_cont1) {
    mul_and_subT_cont1_closure *largs = (mul_and_subT_cont1_closure*)(args.get());
    if ((largs->r == __null)) {
        if ((((largs->r00 || largs->r01) || largs->r10) || largs->r11)) {
            largs->r = new_internal(largs->r00,largs->r01,largs->r10,largs->r11);
        }
    } else {
        __builtin_expect(!(largs->r->child[0] == __null || largs->r->child[0] == largs->r00), 0) ? __assert_rtn(__func__, "cholesky.cpp", 622, "r->child[_00] == NULL || r->child[_00] == r00") : (void)0;
        __builtin_expect(!(largs->r->child[1] == __null || largs->r->child[1] == largs->r01), 0) ? __assert_rtn(__func__, "cholesky.cpp", 623, "r->child[_01] == NULL || r->child[_01] == r01") : (void)0;
        __builtin_expect(!(largs->r->child[2] == __null || largs->r->child[2] == largs->r10), 0) ? __assert_rtn(__func__, "cholesky.cpp", 624, "r->child[_10] == NULL || r->child[_10] == r10") : (void)0;
        __builtin_expect(!(largs->r->child[3] == __null || largs->r->child[3] == largs->r11), 0) ? __assert_rtn(__func__, "cholesky.cpp", 625, "r->child[_11] == NULL || r->child[_11] == r11") : (void)0;
        largs->r->child[0] = largs->r00;
        largs->r->child[1] = largs->r01;
        largs->r->child[2] = largs->r10;
        largs->r->child[3] = largs->r11;
    }
    SEND_ARGUMENT(largs->k, largs->r);
}
THREAD(backsub_cont0) {
    backsub_cont0_closure *largs = (backsub_cont0_closure*)(args.get());
    backsub_cont1_closure SN_backsub_cont1c(largs->k);
    spawn_next<backsub_cont1_closure> SN_backsub_cont1(SN_backsub_cont1c);
    if ((largs->a00 && largs->l10)) {
        cont sp0k;
        SN_BIND(SN_backsub_cont1, &sp0k, a01);
        mul_and_subT_closure sp0c(sp0k);
        sp0c.depth = largs->depth;
        sp0c.lower = 0;
        sp0c.a = largs->a00;
        sp0c.b = largs->l10;
        sp0c.r = largs->a01;
        spawn<mul_and_subT_closure> sp0(sp0c);

    }
    if ((largs->a10 && largs->l10)) {
        cont sp1k;
        SN_BIND(SN_backsub_cont1, &sp1k, a11);
        mul_and_subT_closure sp1c(sp1k);
        sp1c.depth = largs->depth;
        sp1c.lower = 0;
        sp1c.a = largs->a10;
        sp1c.b = largs->l10;
        sp1c.r = largs->a11;
        spawn<mul_and_subT_closure> sp1(sp1c);

    }
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->l11 = largs->l11;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->a11 = largs->a11;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->a10 = largs->a10;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->a00 = largs->a00;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->a01 = largs->a01;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->a = largs->a;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->depth = largs->depth;
    // Original sync was here
    return;
}
THREAD(backsub_cont1) {
    backsub_cont1_closure *largs = (backsub_cont1_closure*)(args.get());
    backsub_cont2_closure SN_backsub_cont2c(largs->k);
    spawn_next<backsub_cont2_closure> SN_backsub_cont2(SN_backsub_cont2c);
    if (largs->a01) {
        cont sp0k;
        SN_BIND(SN_backsub_cont2, &sp0k, a01);
        backsub_closure sp0c(sp0k);
        sp0c.depth = largs->depth;
        sp0c.a = largs->a01;
        sp0c.l = largs->l11;
        spawn<backsub_closure> sp0(sp0c);

    }
    if (largs->a11) {
        cont sp1k;
        SN_BIND(SN_backsub_cont2, &sp1k, a11);
        backsub_closure sp1c(sp1k);
        sp1c.depth = largs->depth;
        sp1c.a = largs->a11;
        sp1c.l = largs->l11;
        spawn<backsub_closure> sp1(sp1c);

    }
    ((backsub_cont2_closure*)SN_backsub_cont2.cls.get())->a11 = largs->a11;
    ((backsub_cont2_closure*)SN_backsub_cont2.cls.get())->a10 = largs->a10;
    ((backsub_cont2_closure*)SN_backsub_cont2.cls.get())->a01 = largs->a01;
    ((backsub_cont2_closure*)SN_backsub_cont2.cls.get())->a00 = largs->a00;
    ((backsub_cont2_closure*)SN_backsub_cont2.cls.get())->a = largs->a;
    // Original sync was here
    return;
}
THREAD(backsub_cont2) {
    backsub_cont2_closure *largs = (backsub_cont2_closure*)(args.get());
    largs->a->child[0] = largs->a00;
    largs->a->child[1] = largs->a01;
    largs->a->child[2] = largs->a10;
    largs->a->child[3] = largs->a11;
    SEND_ARGUMENT(largs->k, largs->a);
}
THREAD(cholesky_cont0) {
    cholesky_cont0_closure *largs = (cholesky_cont0_closure*)(args.get());
    __builtin_expect(!(largs->a00), 0) ? __assert_rtn(__func__, "cholesky.cpp", 731, "a00") : (void)0;
    cholesky_cont1_closure SN_cholesky_cont1c(largs->k);
    spawn_next<cholesky_cont1_closure> SN_cholesky_cont1(SN_cholesky_cont1c);
    cont sp0k;
    SN_BIND(SN_cholesky_cont1, &sp0k, a10);
    backsub_closure sp0c(sp0k);
    sp0c.depth = largs->depth;
    sp0c.a = largs->a10;
    sp0c.l = largs->a00;
    spawn<backsub_closure> sp0(sp0c);

    ((cholesky_cont1_closure*)SN_cholesky_cont1.cls.get())->a11 = largs->a11;
    ((cholesky_cont1_closure*)SN_cholesky_cont1.cls.get())->A = largs->A;
    ((cholesky_cont1_closure*)SN_cholesky_cont1.cls.get())->a00 = largs->a00;
    ((cholesky_cont1_closure*)SN_cholesky_cont1.cls.get())->a = largs->a;
    ((cholesky_cont1_closure*)SN_cholesky_cont1.cls.get())->depth = largs->depth;
    // Original sync was here
    return;
}
THREAD(cholesky_cont1) {
    cholesky_cont1_closure *largs = (cholesky_cont1_closure*)(args.get());
    __builtin_expect(!(largs->a10), 0) ? __assert_rtn(__func__, "cholesky.cpp", 734, "a10") : (void)0;
    cholesky_cont2_closure SN_cholesky_cont2c(largs->k);
    spawn_next<cholesky_cont2_closure> SN_cholesky_cont2(SN_cholesky_cont2c);
    cont sp0k;
    SN_BIND(SN_cholesky_cont2, &sp0k, a11);
    mul_and_subT_closure sp0c(sp0k);
    sp0c.depth = largs->depth;
    sp0c.lower = 1;
    sp0c.a = largs->a10;
    sp0c.b = largs->a10;
    sp0c.r = largs->a11;
    spawn<mul_and_subT_closure> sp0(sp0c);

    ((cholesky_cont2_closure*)SN_cholesky_cont2.cls.get())->a10 = largs->a10;
    ((cholesky_cont2_closure*)SN_cholesky_cont2.cls.get())->a00 = largs->a00;
    ((cholesky_cont2_closure*)SN_cholesky_cont2.cls.get())->A = largs->A;
    ((cholesky_cont2_closure*)SN_cholesky_cont2.cls.get())->a = largs->a;
    ((cholesky_cont2_closure*)SN_cholesky_cont2.cls.get())->depth = largs->depth;
    // Original sync was here
    return;
}
THREAD(cholesky_cont2) {
    cholesky_cont2_closure *largs = (cholesky_cont2_closure*)(args.get());
    __builtin_expect(!(largs->a11), 0) ? __assert_rtn(__func__, "cholesky.cpp", 737, "a11") : (void)0;
    cholesky_cont3_closure SN_cholesky_cont3c(largs->k);
    spawn_next<cholesky_cont3_closure> SN_cholesky_cont3(SN_cholesky_cont3c);
    cont sp0k;
    SN_BIND(SN_cholesky_cont3, &sp0k, a11);
    cholesky_closure sp0c(sp0k);
    sp0c.depth = largs->depth;
    sp0c.a = largs->a11;
    spawn<cholesky_closure> sp0(sp0c);

    ((cholesky_cont3_closure*)SN_cholesky_cont3.cls.get())->depth = largs->depth;
    ((cholesky_cont3_closure*)SN_cholesky_cont3.cls.get())->a = largs->a;
    ((cholesky_cont3_closure*)SN_cholesky_cont3.cls.get())->a10 = largs->a10;
    ((cholesky_cont3_closure*)SN_cholesky_cont3.cls.get())->a00 = largs->a00;
    ((cholesky_cont3_closure*)SN_cholesky_cont3.cls.get())->A = largs->A;
    // Original sync was here
    return;
}
THREAD(cholesky_cont3) {
    cholesky_cont3_closure *largs = (cholesky_cont3_closure*)(args.get());
    __builtin_expect(!(largs->a11), 0) ? __assert_rtn(__func__, "cholesky.cpp", 740, "a11") : (void)0;
    auto sp0c = std::make_shared<cholesky_afterif1_closure>(largs->k);
    sp0c->depth = largs->depth;
    sp0c->a = largs->a;
    sp0c->A = largs->A;
    sp0c->a00 = largs->a00;
    sp0c->a10 = largs->a10;
    sp0c->a11 = largs->a11;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(cholesky_cont4) {
    cholesky_cont4_closure *largs = (cholesky_cont4_closure*)(args.get());
    cholesky_cont5_closure SN_cholesky_cont5c(largs->k);
    spawn_next<cholesky_cont5_closure> SN_cholesky_cont5(SN_cholesky_cont5c);
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->a11 = largs->a11;
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->a00 = largs->a00;
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->a = largs->a;
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->a10 = largs->a10;
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->A = largs->A;
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->depth = largs->depth;
    // Original sync was here
    return;
}
THREAD(cholesky_cont5) {
    cholesky_cont5_closure *largs = (cholesky_cont5_closure*)(args.get());
    auto sp0c = std::make_shared<cholesky_afterif1_closure>(largs->k);
    sp0c->depth = largs->depth;
    sp0c->a = largs->a;
    sp0c->A = largs->A;
    sp0c->a00 = largs->a00;
    sp0c->a10 = largs->a10;
    sp0c->a11 = largs->a11;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(main_cont0) {
    int input_nonzeros;
    int input_blocks;
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    input_blocks = num_blocks(largs->depth,largs->R);
    input_nonzeros = num_nonzeros(largs->depth,largs->R);
    gettimeofday(&(largs->t1),0);
    main_cont1_closure SN_main_cont1c(largs->k);
    spawn_next<main_cont1_closure> SN_main_cont1(SN_main_cont1c);
    cont sp0k;
    SN_BIND(SN_main_cont1, &sp0k, R);
    cholesky_closure sp0c(sp0k);
    sp0c.depth = largs->depth;
    sp0c.a = largs->R;
    spawn<cholesky_closure> sp0(sp0c);

    ((main_cont1_closure*)SN_main_cont1.cls.get())->t1 = largs->t1;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->r0 = largs->r0;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->t = largs->t;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->c = largs->c;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->r = largs->r;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->fc = largs->fc;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->res = largs->res;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->argv = largs->argv;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->depth = largs->depth;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->fr = largs->fr;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->t2 = largs->t2;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->sizey = largs->sizey;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->f = largs->f;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->c0 = largs->c0;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->benchmark = largs->benchmark;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->error = largs->error;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->input_nonzeros = input_nonzeros;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->val = largs->val;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->help = largs->help;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->check = largs->check;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->rnd = largs->rnd;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->argc = largs->argc;
    std::memcpy(((main_cont1_closure*)SN_main_cont1.cls.get())->filename, largs->filename, sizeof(largs->filename));
    ((main_cont1_closure*)SN_main_cont1.cls.get())->nonzeros = largs->nonzeros;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->size = largs->size;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->i = largs->i;
    std::memcpy(((main_cont1_closure*)SN_main_cont1.cls.get())->buf, largs->buf, sizeof(largs->buf));
    ((main_cont1_closure*)SN_main_cont1.cls.get())->input_blocks = input_blocks;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->sizex = largs->sizex;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->A = largs->A;
    // Original sync was here
    return;
}
THREAD(main_cont1) {
    int output_nonzeros;
    int output_blocks;
    unsigned long long runtime_ms;
    main_cont1_closure *largs = (main_cont1_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    printf("%f\n",(runtime_ms / 1000.));
    output_blocks = num_blocks(largs->depth,largs->R);
    output_nonzeros = num_nonzeros(largs->depth,largs->R);
    if (largs->check) {
        printf("Now check result ... \n");
        main_cont2_closure SN_main_cont2c(largs->k);
        spawn_next<main_cont2_closure> SN_main_cont2(SN_main_cont2c);
        cont sp0k;
        SN_BIND(SN_main_cont2, &sp0k, A);
        mul_and_subT_closure sp0c(sp0k);
        sp0c.depth = largs->depth;
        sp0c.lower = 1;
        sp0c.a = largs->R;
        sp0c.b = largs->R;
        sp0c.r = largs->A;
        spawn<mul_and_subT_closure> sp0(sp0c);

        ((main_cont2_closure*)SN_main_cont2.cls.get())->runtime_ms = runtime_ms;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->t2 = largs->t2;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->t1 = largs->t1;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->c0 = largs->c0;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->val = largs->val;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->r0 = largs->r0;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->c = largs->c;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->nonzeros = largs->nonzeros;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->fc = largs->fc;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->fr = largs->fr;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->f = largs->f;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->sizey = largs->sizey;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->sizex = largs->sizex;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->help = largs->help;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->output_blocks = output_blocks;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->output_nonzeros = output_nonzeros;
        std::memcpy(((main_cont2_closure*)SN_main_cont2.cls.get())->filename, largs->filename, sizeof(largs->filename));
        ((main_cont2_closure*)SN_main_cont2.cls.get())->i = largs->i;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->input_blocks = largs->input_blocks;
        std::memcpy(((main_cont2_closure*)SN_main_cont2.cls.get())->buf, largs->buf, sizeof(largs->buf));
        ((main_cont2_closure*)SN_main_cont2.cls.get())->input_nonzeros = largs->input_nonzeros;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->R = largs->R;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->benchmark = largs->benchmark;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->size = largs->size;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->r = largs->r;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->t = largs->t;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->depth = largs->depth;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->argv = largs->argv;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->rnd = largs->rnd;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->check = largs->check;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->res = largs->res;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->argc = largs->argc;
        // Original sync was here
    } else {
        auto sp1c = std::make_shared<main_afterif0_closure>(largs->k);
        sp1c->argc = largs->argc;
        sp1c->argv = largs->argv;
        sp1c->A = largs->A;
        sp1c->R = largs->R;
        sp1c->size = largs->size;
        sp1c->depth = largs->depth;
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
        std::memcpy(sp1c->buf, largs->buf, sizeof(sp1c->buf));
        std::memcpy(sp1c->filename, largs->filename, sizeof(sp1c->filename));
        sp1c->sizex = largs->sizex;
        sp1c->sizey = largs->sizey;
        sp1c->f = largs->f;
        sp1c->fr = largs->fr;
        sp1c->fc = largs->fc;
        sp1c->r = largs->r;
        sp1c->c = largs->c;
        sp1c->val = largs->val;
        sp1c->res = largs->res;
        sp1c->rnd = largs->rnd;
        sp1c->t = largs->t;
        sp1c->r0 = largs->r0;
        sp1c->c0 = largs->c0;
        sp1c->t1 = largs->t1;
        sp1c->t2 = largs->t2;
        sp1c->runtime_ms = runtime_ms;
        cilk_spawn taskSpawn(sp1c->getTask(), sp1c);
        return;
    }
}
THREAD(main_cont2) {
    Real error;
    main_cont2_closure *largs = (main_cont2_closure*)(args.get());
    error = mag(largs->depth,largs->A);
    auto sp0c = std::make_shared<main_afterif0_closure>(largs->k);
    sp0c->argc = largs->argc;
    sp0c->argv = largs->argv;
    sp0c->A = largs->A;
    sp0c->R = largs->R;
    sp0c->size = largs->size;
    sp0c->depth = largs->depth;
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
    std::memcpy(sp0c->buf, largs->buf, sizeof(sp0c->buf));
    std::memcpy(sp0c->filename, largs->filename, sizeof(sp0c->filename));
    sp0c->sizex = largs->sizex;
    sp0c->sizey = largs->sizey;
    sp0c->f = largs->f;
    sp0c->fr = largs->fr;
    sp0c->fc = largs->fc;
    sp0c->r = largs->r;
    sp0c->c = largs->c;
    sp0c->val = largs->val;
    sp0c->res = largs->res;
    sp0c->rnd = largs->rnd;
    sp0c->t = largs->t;
    sp0c->r0 = largs->r0;
    sp0c->c0 = largs->c0;
    sp0c->t1 = largs->t1;
    sp0c->t2 = largs->t2;
    sp0c->runtime_ms = largs->runtime_ms;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
