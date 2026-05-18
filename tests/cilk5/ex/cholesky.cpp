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
void block_schur_full(Real (*B)[4], Real (*A)[4], Real (*C)[4]);
void block_schur_half(Real (*B0)[4], Real (*A0)[4], Real (*C0)[4]);
void block_backsub(Real (*B1)[4], Real (*U)[4]);
void block_cholesky(Real (*B2)[4]);
void block_zero(Real (*B3)[4]);
InternalNode * new_block_leaf();
InternalNode * new_internal(InternalNode *a00, InternalNode *a01, InternalNode *a10, InternalNode *a11);
THREAD(copy_matrix);
void free_matrix(int depth0, Matrix a0);
Real get_matrix(int depth1, Matrix a1, int r0, int c);
Matrix set_matrix(int depth2, Matrix a2, int r1, int c0, Real value);
int num_blocks(int depth3, Matrix a3);
int num_nonzeros(int depth4, Matrix a4);
Real mag(int depth5, Matrix a5);
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
    int depth6;
    int lower;
    Matrix a6;
    Matrix b;
    Matrix r2;
);
CLOSURE_DEF(backsub,
    int depth7;
    Matrix a7;
    Matrix l;
);
CLOSURE_DEF(cholesky,
    int depth8;
    Matrix a8;
);
CLOSURE_DEF(main_afterif0,
    int argc;
    char **argv;
    Matrix A9;
    Matrix R1;
    int size0;
    int depth9;
    int nonzeros;
    int i6;
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
    int r3;
    int c1;
    Real val;
    int res2;
    double rnd;
    int t;
    int r4;
    int c2;
    struct timeval t1;
    struct timeval t2;
    unsigned long long runtime_ms;
);
CLOSURE_DEF(cholesky_afterif1,
    int depth8;
    Matrix a8;
    LeafNode *A8;
    Matrix a001;
    Matrix a101;
    Matrix a111;
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
    int depth6;
    int lower;
    Matrix a6;
    Matrix b;
    Matrix r2;
    Matrix r000;
    Matrix r010;
    Matrix r100;
    Matrix r110;
);
CLOSURE_DEF(mul_and_subT_cont1,
    Matrix r2;
    Matrix r000;
    Matrix r010;
    Matrix r100;
    Matrix r110;
);
CLOSURE_DEF(backsub_cont0,
    int depth7;
    Matrix a7;
    Matrix a000;
    Matrix a010;
    Matrix a100;
    Matrix a110;
    Matrix l10;
    Matrix l11;
);
CLOSURE_DEF(backsub_cont1,
    int depth7;
    Matrix a7;
    Matrix a000;
    Matrix a010;
    Matrix a100;
    Matrix a110;
    Matrix l11;
);
CLOSURE_DEF(backsub_cont2,
    Matrix a7;
    Matrix a000;
    Matrix a010;
    Matrix a100;
    Matrix a110;
);
CLOSURE_DEF(cholesky_cont0,
    int depth8;
    Matrix a8;
    LeafNode *A8;
    Matrix a001;
    Matrix a101;
    Matrix a111;
);
CLOSURE_DEF(cholesky_cont1,
    int depth8;
    Matrix a8;
    LeafNode *A8;
    Matrix a001;
    Matrix a101;
    Matrix a111;
);
CLOSURE_DEF(cholesky_cont2,
    int depth8;
    Matrix a8;
    LeafNode *A8;
    Matrix a001;
    Matrix a101;
    Matrix a111;
);
CLOSURE_DEF(cholesky_cont3,
    int depth8;
    Matrix a8;
    LeafNode *A8;
    Matrix a001;
    Matrix a101;
    Matrix a111;
);
CLOSURE_DEF(cholesky_cont4,
    int depth8;
    Matrix a8;
    LeafNode *A8;
    Matrix a001;
    Matrix a101;
    Matrix a111;
);
CLOSURE_DEF(cholesky_cont5,
    int depth8;
    Matrix a8;
    LeafNode *A8;
    Matrix a001;
    Matrix a101;
    Matrix a111;
);
CLOSURE_DEF(main_cont0,
    int argc;
    char **argv;
    Matrix A9;
    Matrix R1;
    int size0;
    int depth9;
    int nonzeros;
    int i6;
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
    int r3;
    int c1;
    Real val;
    int res2;
    double rnd;
    int t;
    int r4;
    int c2;
    struct timeval t1;
    struct timeval t2;
);
CLOSURE_DEF(main_cont1,
    int argc;
    char **argv;
    Matrix A9;
    Matrix R1;
    int size0;
    int depth9;
    int nonzeros;
    int i6;
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
    int r3;
    int c1;
    Real val;
    int res2;
    double rnd;
    int t;
    int r4;
    int c2;
    struct timeval t1;
    struct timeval t2;
);
CLOSURE_DEF(main_cont2,
    int argc;
    char **argv;
    Matrix A9;
    Matrix R1;
    int size0;
    int depth9;
    int nonzeros;
    int i6;
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
    int r3;
    int c1;
    Real val;
    int res2;
    double rnd;
    int t;
    int r4;
    int c2;
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
void block_schur_full(Real (*B)[4], Real (*A)[4], Real (*C)[4]) {
    int i;
    int j;
    int k;
    for (i = 0;(i < (1 << 2));(i++)) {
        for (j = 0;(j < (1 << 2));(j++)) {
            for (k = 0;(k < (1 << 2));(k++)) {
                (B[i][j]) -= (A[i][k]) * (C[j][k]);
            }
        }
    }
}
void block_schur_half(Real (*B0)[4], Real (*A0)[4], Real (*C0)[4]) {
    int i0;
    int j0;
    int k0;
    for (i0 = 0;(i0 < (1 << 2));(i0++)) {
        for (j0 = 0;(j0 <= i0);(j0++)) {
            for (k0 = 0;(k0 < (1 << 2));(k0++)) {
                (B0[i0][j0]) -= (A0[i0][k0]) * (C0[j0][k0]);
            }
        }
    }
}
void block_backsub(Real (*B1)[4], Real (*U)[4]) {
    int i1;
    int j1;
    int k1;
    for (i1 = 0;(i1 < (1 << 2));(i1++)) {
        for (j1 = 0;(j1 < (1 << 2));(j1++)) {
            for (k1 = 0;(k1 < i1);(k1++)) {
                (B1[j1][i1]) -= (U[i1][k1]) * (B1[j1][k1]);
            }
            (B1[j1][i1]) /= (U[i1][i1]);
        }
    }
}
void block_cholesky(Real (*B2)[4]) {
    int i2;
    int j2;
    int k2;
    Real x;
    for (k2 = 0;(k2 < (1 << 2));(k2++)) {
        if ((B2[k2][k2] < 0.)) {
            printf("sqrt error: %f\n",B2[k2][k2]);
            printf("matrix is probably not numerically stable\n");
            exit(9);
        }
        x = sqrt(B2[k2][k2]);
        for (i2 = k2;(i2 < (1 << 2));(i2++)) {
            (B2[i2][k2]) /= x;
        }
        for (j2 = (k2 + 1);(j2 < (1 << 2));(j2++)) {
            for (i2 = j2;(i2 < (1 << 2));(i2++)) {
                (B2[i2][j2]) -= (B2[i2][k2]) * (B2[j2][k2]);
                if (((j2 > i2) && (B2[i2][j2] != 0.))) {
                    printf("Upper not empty\n");
                }
            }
        }
    }
}
void block_zero(Real (*B3)[4]) {
    int i3;
    int k3;
    for (i3 = 0;(i3 < (1 << 2));(i3++)) {
        for (k3 = 0;(k3 < (1 << 2));(k3++)) {
            B3[i3][k3] = 0.;
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
    LeafNode *A1;
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
            A1 = ((LeafNode *) largs->a);
            r = new_block_leaf();
            R = ((LeafNode *) r);
            memcpy(R->block,A1->block,sizeof(Block));
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
    return;
}
void free_matrix(int depth0, Matrix a0) {
    if ((a0 == __null)) {
        return ;
    } else {
        if ((depth0 == 2)) {
            free(a0);
        } else {
            (depth0--);
            free_matrix(depth0,a0->child[0]);
            free_matrix(depth0,a0->child[1]);
            free_matrix(depth0,a0->child[2]);
            free_matrix(depth0,a0->child[3]);
            free(a0);
        }
    }
}
Real get_matrix(int depth1, Matrix a1, int r0, int c) {
    LeafNode *A2;
    int mid;
    __builtin_expect(!(depth1 >= 2), 0) ? __assert_rtn(__func__, "cholesky.cpp", 342, "depth >= BLOCK_DEPTH") : (void)0;
    __builtin_expect(!(r0 < (1 << depth1)), 0) ? __assert_rtn(__func__, "cholesky.cpp", 343, "r < (1 << depth)") : (void)0;
    __builtin_expect(!(c < (1 << depth1)), 0) ? __assert_rtn(__func__, "cholesky.cpp", 344, "c < (1 << depth)") : (void)0;
    if ((a1 == __null)) {
        return 0.;
    } else {
        if ((depth1 == 2)) {
            A2 = ((LeafNode *) a1);
            return A2->block[r0][c];
        } else {
            (depth1--);
            mid = (1 << depth1);
            if ((r0 < mid)) {
                if ((c < mid)) {
                    return get_matrix(depth1,a1->child[0],r0,c);
                } else {
                    return get_matrix(depth1,a1->child[1],r0,(c - mid));
                }
            } else {
                if ((c < mid)) {
                    return get_matrix(depth1,a1->child[2],(r0 - mid),c);
                } else {
                    return get_matrix(depth1,a1->child[3],(r0 - mid),(c - mid));
                }
            }
        }
    }
}
Matrix set_matrix(int depth2, Matrix a2, int r1, int c0, Real value) {
    LeafNode *A3;
    int mid0;
    __builtin_expect(!(depth2 >= 2), 0) ? __assert_rtn(__func__, "cholesky.cpp", 378, "depth >= BLOCK_DEPTH") : (void)0;
    __builtin_expect(!(r1 < (1 << depth2)), 0) ? __assert_rtn(__func__, "cholesky.cpp", 379, "r < (1 << depth)") : (void)0;
    __builtin_expect(!(c0 < (1 << depth2)), 0) ? __assert_rtn(__func__, "cholesky.cpp", 380, "c < (1 << depth)") : (void)0;
    if ((depth2 == 2)) {
        if ((a2 == __null)) {
            a2 = new_block_leaf();
            A3 = ((LeafNode *) a2);
            block_zero(A3->block);
        } else {
            A3 = ((LeafNode *) a2);
        }
        A3->block[r1][c0] = value;
    } else {
        if ((a2 == __null)) {
            a2 = new_internal(__null,__null,__null,__null);
        }
        (depth2--);
        mid0 = (1 << depth2);
        if ((r1 < mid0)) {
            if ((c0 < mid0)) {
                a2->child[0] = set_matrix(depth2,a2->child[0],r1,c0,value);
            } else {
                a2->child[1] = set_matrix(depth2,a2->child[1],r1,(c0 - mid0),value);
            }
        } else {
            if ((c0 < mid0)) {
                a2->child[2] = set_matrix(depth2,a2->child[2],(r1 - mid0),c0,value);
            } else {
                a2->child[3] = set_matrix(depth2,a2->child[3],(r1 - mid0),(c0 - mid0),value);
            }
        }
    }
    return a2;
}
int num_blocks(int depth3, Matrix a3) {
    int res;
    if ((a3 == __null)) {
        return 0;
    } else {
        if ((depth3 == 2)) {
            return 1;
        } else {
            (depth3--);
            res = 0;
            res = (res + num_blocks(depth3,a3->child[0]));
            res = (res + num_blocks(depth3,a3->child[1]));
            res = (res + num_blocks(depth3,a3->child[2]));
            res = (res + num_blocks(depth3,a3->child[3]));
            return res;
        }
    }
}
int num_nonzeros(int depth4, Matrix a4) {
    int res0;
    LeafNode *A4;
    int i4;
    int j3;
    if ((a4 == __null)) {
        return 0;
    } else {
        if ((depth4 == 2)) {
            A4 = ((LeafNode *) a4);
            res0 = 0;
            for (i4 = 0;(i4 < (1 << 2));(i4++)) {
                for (j3 = 0;(j3 < (1 << 2));(j3++)) {
                    if ((A4->block[i4][j3] != 0.)) {
                        (res0++);
                    }
                }
            }
            return res0;
        } else {
            (depth4--);
            res0 = 0;
            res0 = (res0 + num_nonzeros(depth4,a4->child[0]));
            res0 = (res0 + num_nonzeros(depth4,a4->child[1]));
            res0 = (res0 + num_nonzeros(depth4,a4->child[2]));
            res0 = (res0 + num_nonzeros(depth4,a4->child[3]));
            return res0;
        }
    }
}
Real mag(int depth5, Matrix a5) {
    Real res1;
    LeafNode *A5;
    int i5;
    int j4;
    res1 = 0.;
    if ((!a5)) {
        return res1;
    } else {
        if ((depth5 == 2)) {
            A5 = ((LeafNode *) a5);
            for (i5 = 0;(i5 < (1 << 2));(i5++)) {
                for (j4 = 0;(j4 < (1 << 2));(j4++)) {
                    res1 = (res1 + (A5->block[i5][j4] * A5->block[i5][j4]));
                }
            }
        } else {
            (depth5--);
            res1 = (res1 + mag(depth5,a5->child[0]));
            res1 = (res1 + mag(depth5,a5->child[1]));
            res1 = (res1 + mag(depth5,a5->child[2]));
            res1 = (res1 + mag(depth5,a5->child[3]));
        }
        return res1;
    }
}
THREAD(mul_and_subT) {
    LeafNode *A6;
    LeafNode *B4;
    LeafNode *R0;
    Matrix r000;
    Matrix r010;
    Matrix r100;
    Matrix r110;
    mul_and_subT_closure *largs = (mul_and_subT_closure*)(args.get());
    __builtin_expect(!(largs->a6 != __null && largs->b != __null), 0) ? __assert_rtn(__func__, "cholesky.cpp", 535, "a != NULL && b != NULL") : (void)0;
    if ((largs->depth6 == 2)) {
        A6 = ((LeafNode *) largs->a6);
        B4 = ((LeafNode *) largs->b);
        if ((largs->r2 == __null)) {
            largs->r2 = new_block_leaf();
            R0 = ((LeafNode *) largs->r2);
            block_zero(R0->block);
        } else {
            R0 = ((LeafNode *) largs->r2);
        }
        if (largs->lower) {
            block_schur_half(R0->block,A6->block,B4->block);
        } else {
            block_schur_full(R0->block,A6->block,B4->block);
        }
        SEND_ARGUMENT(largs->k, largs->r2);
    } else {
        (largs->depth6--);
        mul_and_subT_cont0_closure SN_mul_and_subT_cont0c(largs->k);
        spawn_next<mul_and_subT_cont0_closure> SN_mul_and_subT_cont0(SN_mul_and_subT_cont0c);
        if ((largs->r2 != __null)) {
            r000 = largs->r2->child[0];
            r010 = largs->r2->child[1];
            r100 = largs->r2->child[2];
            r110 = largs->r2->child[3];
        } else {
            r000 = __null;
            r010 = __null;
            r100 = __null;
            r110 = __null;
        }
        if ((largs->a6->child[0] && largs->b->child[0])) {
            cont sp0k;
            SN_BIND(SN_mul_and_subT_cont0, &sp0k, r000);
            mul_and_subT_closure sp0c(sp0k);
            sp0c.depth6 = largs->depth6;
            sp0c.lower = largs->lower;
            sp0c.a6 = largs->a6->child[0];
            sp0c.b = largs->b->child[0];
            sp0c.r2 = r000;
            spawn<mul_and_subT_closure> sp0(sp0c);

        }
        if ((((!largs->lower) && largs->a6->child[0]) && largs->b->child[2])) {
            cont sp1k;
            SN_BIND(SN_mul_and_subT_cont0, &sp1k, r010);
            mul_and_subT_closure sp1c(sp1k);
            sp1c.depth6 = largs->depth6;
            sp1c.lower = 0;
            sp1c.a6 = largs->a6->child[0];
            sp1c.b = largs->b->child[2];
            sp1c.r2 = r010;
            spawn<mul_and_subT_closure> sp1(sp1c);

        }
        if ((largs->a6->child[2] && largs->b->child[0])) {
            cont sp2k;
            SN_BIND(SN_mul_and_subT_cont0, &sp2k, r100);
            mul_and_subT_closure sp2c(sp2k);
            sp2c.depth6 = largs->depth6;
            sp2c.lower = 0;
            sp2c.a6 = largs->a6->child[2];
            sp2c.b = largs->b->child[0];
            sp2c.r2 = r100;
            spawn<mul_and_subT_closure> sp2(sp2c);

        }
        if ((largs->a6->child[2] && largs->b->child[2])) {
            cont sp3k;
            SN_BIND(SN_mul_and_subT_cont0, &sp3k, r110);
            mul_and_subT_closure sp3c(sp3k);
            sp3c.depth6 = largs->depth6;
            sp3c.lower = largs->lower;
            sp3c.a6 = largs->a6->child[2];
            sp3c.b = largs->b->child[2];
            sp3c.r2 = r110;
            spawn<mul_and_subT_closure> sp3(sp3c);

        }
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->r100 = r100;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->r010 = r010;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->r2 = largs->r2;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->r000 = r000;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->a6 = largs->a6;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->b = largs->b;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->lower = largs->lower;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->r110 = r110;
        ((mul_and_subT_cont0_closure*)SN_mul_and_subT_cont0.cls.get())->depth6 = largs->depth6;
        // Original sync was here
    }
    return;
}
THREAD(backsub) {
    LeafNode *A7;
    LeafNode *L;
    Matrix a000;
    Matrix a010;
    Matrix a100;
    Matrix a110;
    Matrix l00;
    Matrix l10;
    Matrix l11;
    backsub_closure *largs = (backsub_closure*)(args.get());
    __builtin_expect(!(largs->a7 != __null), 0) ? __assert_rtn(__func__, "cholesky.cpp", 641, "a != NULL") : (void)0;
    __builtin_expect(!(largs->l != __null), 0) ? __assert_rtn(__func__, "cholesky.cpp", 642, "l != NULL") : (void)0;
    if ((largs->depth7 == 2)) {
        A7 = ((LeafNode *) largs->a7);
        L = ((LeafNode *) largs->l);
        block_backsub(A7->block,L->block);
        SEND_ARGUMENT(largs->k, largs->a7);
    } else {
        (largs->depth7--);
        a000 = largs->a7->child[0];
        a010 = largs->a7->child[1];
        a100 = largs->a7->child[2];
        a110 = largs->a7->child[3];
        l00 = largs->l->child[0];
        l10 = largs->l->child[2];
        l11 = largs->l->child[3];
        __builtin_expect(!(l00 && l11), 0) ? __assert_rtn(__func__, "cholesky.cpp", 666, "l00 && l11") : (void)0;
        backsub_cont0_closure SN_backsub_cont0c(largs->k);
        spawn_next<backsub_cont0_closure> SN_backsub_cont0(SN_backsub_cont0c);
        if (a000) {
            cont sp0k;
            SN_BIND(SN_backsub_cont0, &sp0k, a000);
            backsub_closure sp0c(sp0k);
            sp0c.depth7 = largs->depth7;
            sp0c.a7 = a000;
            sp0c.l = l00;
            spawn<backsub_closure> sp0(sp0c);

        }
        if (a100) {
            cont sp1k;
            SN_BIND(SN_backsub_cont0, &sp1k, a100);
            backsub_closure sp1c(sp1k);
            sp1c.depth7 = largs->depth7;
            sp1c.a7 = a100;
            sp1c.l = l00;
            spawn<backsub_closure> sp1(sp1c);

        }
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->l11 = l11;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->l10 = l10;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->a110 = a110;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->a000 = a000;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->a100 = a100;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->a010 = a010;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->a7 = largs->a7;
        ((backsub_cont0_closure*)SN_backsub_cont0.cls.get())->depth7 = largs->depth7;
        // Original sync was here
    }
    return;
}
THREAD(cholesky) {
    LeafNode *A8;
    Matrix a001;
    Matrix a101;
    Matrix a111;
    cholesky_closure *largs = (cholesky_closure*)(args.get());
    __builtin_expect(!(largs->a8 != __null), 0) ? __assert_rtn(__func__, "cholesky.cpp", 705, "a != NULL") : (void)0;
    if ((largs->depth8 == 2)) {
        A8 = ((LeafNode *) largs->a8);
        block_cholesky(A8->block);
        SEND_ARGUMENT(largs->k, largs->a8);
    } else {
        (largs->depth8--);
        a001 = largs->a8->child[0];
        a101 = largs->a8->child[2];
        a111 = largs->a8->child[3];
        __builtin_expect(!(a001), 0) ? __assert_rtn(__func__, "cholesky.cpp", 722, "a00") : (void)0;
        if ((!a101)) {
            cholesky_cont4_closure SN_cholesky_cont4c(largs->k);
            spawn_next<cholesky_cont4_closure> SN_cholesky_cont4(SN_cholesky_cont4c);
            cont sp0k;
            SN_BIND(SN_cholesky_cont4, &sp0k, a001);
            cholesky_closure sp0c(sp0k);
            sp0c.depth8 = largs->depth8;
            sp0c.a8 = a001;
            spawn<cholesky_closure> sp0(sp0c);

            cont sp1k;
            SN_BIND(SN_cholesky_cont4, &sp1k, a111);
            cholesky_closure sp1c(sp1k);
            sp1c.depth8 = largs->depth8;
            sp1c.a8 = a111;
            spawn<cholesky_closure> sp1(sp1c);

            ((cholesky_cont4_closure*)SN_cholesky_cont4.cls.get())->a101 = a101;
            ((cholesky_cont4_closure*)SN_cholesky_cont4.cls.get())->A8 = A8;
            ((cholesky_cont4_closure*)SN_cholesky_cont4.cls.get())->a8 = largs->a8;
            ((cholesky_cont4_closure*)SN_cholesky_cont4.cls.get())->depth8 = largs->depth8;
            // Original sync was here
        } else {
            cholesky_cont0_closure SN_cholesky_cont0c(largs->k);
            spawn_next<cholesky_cont0_closure> SN_cholesky_cont0(SN_cholesky_cont0c);
            cont sp2k;
            SN_BIND(SN_cholesky_cont0, &sp2k, a001);
            cholesky_closure sp2c(sp2k);
            sp2c.depth8 = largs->depth8;
            sp2c.a8 = a001;
            spawn<cholesky_closure> sp2(sp2c);

            ((cholesky_cont0_closure*)SN_cholesky_cont0.cls.get())->a111 = a111;
            ((cholesky_cont0_closure*)SN_cholesky_cont0.cls.get())->a101 = a101;
            ((cholesky_cont0_closure*)SN_cholesky_cont0.cls.get())->A8 = A8;
            ((cholesky_cont0_closure*)SN_cholesky_cont0.cls.get())->a8 = largs->a8;
            ((cholesky_cont0_closure*)SN_cholesky_cont0.cls.get())->depth8 = largs->depth8;
            // Original sync was here
        }
    }
    return;
}
int logarithm(int size) {
    int k4;
    k4 = 0;
    while (((1 << k4) < size)) {
        (k4++);
    }
    return k4;
}
int usage() {
    fprintf(__stderrp,"\nUsage: cholesky [<cilk-options>] [-n size] [-z nonzeros]\n                [-f filename] [-benchmark] [-h]\n\nDefault: cholesky -n 500 -z 1000\n\nThis program performs a divide and conquer Cholesky factorization of a\nsparse symmetric positive definite matrix (A=LL^T).  Using the fact\nthat the matrix is symmetric, Cholesky does half the number of\noperations of LU.  The method used is the same as with LU, with work\nTheta(n^3) and critical path Theta(n lg(n)) for the dense case.  A\n");
    fprintf(__stderrp,"quad-tree is used to store the nonzero entries of the sparse\nmatrix. Actual work and critical path are influenced by the sparsity\n pattern of the matrix.\n\nThe input matrix is either read from the provided file or generated\nrandomly with size and nonzero-elements as specified.\n\n");
    return 1;
}
int main(int argc, char **argv) {
    Matrix A9;
    Matrix R1;
    int size0;
    int depth9;
    int nonzeros;
    int i6;
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
    int r3;
    int c1;
    Real val;
    int res2;
    double rnd;
    int t;
    int r4;
    int c2;
    check = 1;
    error = 0.;
    A9 = __null;
    filename[0] = 0;
    size0 = 500;
    nonzeros = 1000;
    get_options(argc,argv,specifiers,opt_types,&(size0),&(nonzeros),&(check),filename,&(benchmark),&(help));
    if (help) {
        return usage();
    } else {
        main_cont0_closure SN_main_cont0c(CONT_DUMMY);
        spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
        if (benchmark) {
            switch (benchmark) {
  case 1:
    filename[0] = 0;
    size0 = 128;
    nonzeros = 100;
    break;
  case 2:
    filename[0] = 0;
    size0 = 1000;
    nonzeros = 10000;
    break;
  case 3:
    filename[0] = 0;
    size0 = 2000;
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
                size0 = sizex;
                depth9 = logarithm(size0);
                srand(61066);
                cilk_srand(61066);
                nonzeros = 0;
                while ((!feof(f))) {
                    fgets(buf,1000,f);
                    res2 = sscanf(buf,"%lf %lf %lf",&(fr),&(fc),&(val));
                    r3 = fr;
                    c1 = fc;
                    if ((res2 <= 0)) {
                        break;
                    }
                    if ((res2 == 2)) {
                        rnd = (((double) rand()) / ((double) 2147483647));
                        val = r3 == c1 ? 1.0E+5 * rnd : rnd;
                    }
                    (r3--);
                    (c1--);
                    if ((r3 < c1)) {
                        t = r3;
                        r3 = c1;
                        c1 = t;
                    }
                    __builtin_expect(!(r3 >= c1), 0) ? __assert_rtn(__func__, "cholesky.cpp", 884, "r >= c") : (void)0;
                    __builtin_expect(!(r3 < size0), 0) ? __assert_rtn(__func__, "cholesky.cpp", 885, "r < size") : (void)0;
                    __builtin_expect(!(c1 < size0), 0) ? __assert_rtn(__func__, "cholesky.cpp", 886, "c < size") : (void)0;
                    A9 = set_matrix(depth9,A9,r3,c1,val);
                    (nonzeros++);
                }
            }
        } else {
            depth9 = logarithm(size0);
            for (i6 = 0;(i6 < size0);(i6++)) {
                A9 = set_matrix(depth9,A9,i6,i6,1.);
            }
            for (i6 = 0;(i6 < (nonzeros - size0));(i6++)) {
                again:
r4 = cilk_rand() % size0;
;
                c2 = (cilk_rand() % size0);
                if ((r4 <= c2)) {
                    goto again;
;
                }
                if ((get_matrix(depth9,A9,r4,c2) != 0.)) {
                    goto again;
;
                }
                A9 = set_matrix(depth9,A9,r4,c2,0.10000000000000001);
            }
        }
        for (i6 = size0;(i6 < (1 << depth9));(i6++)) {
            A9 = set_matrix(depth9,A9,i6,i6,1.);
        }
        cont sp0k;
        SN_BIND(SN_main_cont0, &sp0k, R1);
        copy_matrix_closure sp0c(sp0k);
        sp0c.depth = depth9;
        sp0c.a = A9;
        spawn<copy_matrix_closure> sp0(sp0c);

        ((main_cont0_closure*)SN_main_cont0.cls.get())->r4 = r4;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->r3 = r3;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->val = val;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->fc = fc;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->sizex = sizex;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->res2 = res2;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->f = f;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->i6 = i6;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->error = error;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->c1 = c1;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->nonzeros = nonzeros;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->t = t;
        std::memcpy(((main_cont0_closure*)SN_main_cont0.cls.get())->buf, buf, sizeof(buf));
        ((main_cont0_closure*)SN_main_cont0.cls.get())->depth9 = depth9;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->sizey = sizey;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->check = check;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->c2 = c2;
        std::memcpy(((main_cont0_closure*)SN_main_cont0.cls.get())->filename, filename, sizeof(filename));
        ((main_cont0_closure*)SN_main_cont0.cls.get())->A9 = A9;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->rnd = rnd;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->help = help;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->benchmark = benchmark;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->fr = fr;
        ((main_cont0_closure*)SN_main_cont0.cls.get())->size0 = size0;
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
    fprintf(__stderrp,"Options: original size     = %d\n",largs->size0);
    fprintf(__stderrp,"         original nonzeros = %d\n",largs->nonzeros);
    fprintf(__stderrp,"         input nonzeros    = %d\n",largs->input_nonzeros);
    fprintf(__stderrp,"         input blocks      = %d\n",largs->input_blocks);
    fprintf(__stderrp,"         output nonzeros   = %d\n",largs->output_nonzeros);
    fprintf(__stderrp,"         output blocks     = %d\n\n",largs->output_blocks);
    free_matrix(largs->depth9,largs->A9);
    free_matrix(largs->depth9,largs->R1);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(cholesky_afterif1) {
    cholesky_afterif1_closure *largs = (cholesky_afterif1_closure*)(args.get());
    largs->a8->child[0] = largs->a001;
    largs->a8->child[2] = largs->a101;
    largs->a8->child[3] = largs->a111;
    SEND_ARGUMENT(largs->k, largs->a8);
    return;
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
    return;
}
THREAD(mul_and_subT_cont0) {
    mul_and_subT_cont0_closure *largs = (mul_and_subT_cont0_closure*)(args.get());
    mul_and_subT_cont1_closure SN_mul_and_subT_cont1c(largs->k);
    spawn_next<mul_and_subT_cont1_closure> SN_mul_and_subT_cont1(SN_mul_and_subT_cont1c);
    if ((largs->a6->child[1] && largs->b->child[1])) {
        cont sp0k;
        SN_BIND(SN_mul_and_subT_cont1, &sp0k, r000);
        mul_and_subT_closure sp0c(sp0k);
        sp0c.depth6 = largs->depth6;
        sp0c.lower = largs->lower;
        sp0c.a6 = largs->a6->child[1];
        sp0c.b = largs->b->child[1];
        sp0c.r2 = largs->r000;
        spawn<mul_and_subT_closure> sp0(sp0c);

    }
    if ((((!largs->lower) && largs->a6->child[1]) && largs->b->child[3])) {
        cont sp1k;
        SN_BIND(SN_mul_and_subT_cont1, &sp1k, r010);
        mul_and_subT_closure sp1c(sp1k);
        sp1c.depth6 = largs->depth6;
        sp1c.lower = 0;
        sp1c.a6 = largs->a6->child[1];
        sp1c.b = largs->b->child[3];
        sp1c.r2 = largs->r010;
        spawn<mul_and_subT_closure> sp1(sp1c);

    }
    if ((largs->a6->child[3] && largs->b->child[1])) {
        cont sp2k;
        SN_BIND(SN_mul_and_subT_cont1, &sp2k, r100);
        mul_and_subT_closure sp2c(sp2k);
        sp2c.depth6 = largs->depth6;
        sp2c.lower = 0;
        sp2c.a6 = largs->a6->child[3];
        sp2c.b = largs->b->child[1];
        sp2c.r2 = largs->r100;
        spawn<mul_and_subT_closure> sp2(sp2c);

    }
    if ((largs->a6->child[3] && largs->b->child[3])) {
        cont sp3k;
        SN_BIND(SN_mul_and_subT_cont1, &sp3k, r110);
        mul_and_subT_closure sp3c(sp3k);
        sp3c.depth6 = largs->depth6;
        sp3c.lower = largs->lower;
        sp3c.a6 = largs->a6->child[3];
        sp3c.b = largs->b->child[3];
        sp3c.r2 = largs->r110;
        spawn<mul_and_subT_closure> sp3(sp3c);

    }
    ((mul_and_subT_cont1_closure*)SN_mul_and_subT_cont1.cls.get())->r110 = largs->r110;
    ((mul_and_subT_cont1_closure*)SN_mul_and_subT_cont1.cls.get())->r100 = largs->r100;
    ((mul_and_subT_cont1_closure*)SN_mul_and_subT_cont1.cls.get())->r010 = largs->r010;
    ((mul_and_subT_cont1_closure*)SN_mul_and_subT_cont1.cls.get())->r000 = largs->r000;
    ((mul_and_subT_cont1_closure*)SN_mul_and_subT_cont1.cls.get())->r2 = largs->r2;
    // Original sync was here
    return;
}
THREAD(mul_and_subT_cont1) {
    mul_and_subT_cont1_closure *largs = (mul_and_subT_cont1_closure*)(args.get());
    if ((largs->r2 == __null)) {
        if ((((largs->r000 || largs->r010) || largs->r100) || largs->r110)) {
            largs->r2 = new_internal(largs->r000,largs->r010,largs->r100,largs->r110);
        }
    } else {
        __builtin_expect(!(largs->r2->child[0] == __null || largs->r2->child[0] == largs->r000), 0) ? __assert_rtn(__func__, "cholesky.cpp", 622, "r->child[_00] == NULL || r->child[_00] == r00") : (void)0;
        __builtin_expect(!(largs->r2->child[1] == __null || largs->r2->child[1] == largs->r010), 0) ? __assert_rtn(__func__, "cholesky.cpp", 623, "r->child[_01] == NULL || r->child[_01] == r01") : (void)0;
        __builtin_expect(!(largs->r2->child[2] == __null || largs->r2->child[2] == largs->r100), 0) ? __assert_rtn(__func__, "cholesky.cpp", 624, "r->child[_10] == NULL || r->child[_10] == r10") : (void)0;
        __builtin_expect(!(largs->r2->child[3] == __null || largs->r2->child[3] == largs->r110), 0) ? __assert_rtn(__func__, "cholesky.cpp", 625, "r->child[_11] == NULL || r->child[_11] == r11") : (void)0;
        largs->r2->child[0] = largs->r000;
        largs->r2->child[1] = largs->r010;
        largs->r2->child[2] = largs->r100;
        largs->r2->child[3] = largs->r110;
    }
    SEND_ARGUMENT(largs->k, largs->r2);
    return;
}
THREAD(backsub_cont0) {
    backsub_cont0_closure *largs = (backsub_cont0_closure*)(args.get());
    backsub_cont1_closure SN_backsub_cont1c(largs->k);
    spawn_next<backsub_cont1_closure> SN_backsub_cont1(SN_backsub_cont1c);
    if ((largs->a000 && largs->l10)) {
        cont sp0k;
        SN_BIND(SN_backsub_cont1, &sp0k, a010);
        mul_and_subT_closure sp0c(sp0k);
        sp0c.depth6 = largs->depth7;
        sp0c.lower = 0;
        sp0c.a6 = largs->a000;
        sp0c.b = largs->l10;
        sp0c.r2 = largs->a010;
        spawn<mul_and_subT_closure> sp0(sp0c);

    }
    if ((largs->a100 && largs->l10)) {
        cont sp1k;
        SN_BIND(SN_backsub_cont1, &sp1k, a110);
        mul_and_subT_closure sp1c(sp1k);
        sp1c.depth6 = largs->depth7;
        sp1c.lower = 0;
        sp1c.a6 = largs->a100;
        sp1c.b = largs->l10;
        sp1c.r2 = largs->a110;
        spawn<mul_and_subT_closure> sp1(sp1c);

    }
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->l11 = largs->l11;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->a110 = largs->a110;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->a100 = largs->a100;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->a000 = largs->a000;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->a010 = largs->a010;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->a7 = largs->a7;
    ((backsub_cont1_closure*)SN_backsub_cont1.cls.get())->depth7 = largs->depth7;
    // Original sync was here
    return;
}
THREAD(backsub_cont1) {
    backsub_cont1_closure *largs = (backsub_cont1_closure*)(args.get());
    backsub_cont2_closure SN_backsub_cont2c(largs->k);
    spawn_next<backsub_cont2_closure> SN_backsub_cont2(SN_backsub_cont2c);
    if (largs->a010) {
        cont sp0k;
        SN_BIND(SN_backsub_cont2, &sp0k, a010);
        backsub_closure sp0c(sp0k);
        sp0c.depth7 = largs->depth7;
        sp0c.a7 = largs->a010;
        sp0c.l = largs->l11;
        spawn<backsub_closure> sp0(sp0c);

    }
    if (largs->a110) {
        cont sp1k;
        SN_BIND(SN_backsub_cont2, &sp1k, a110);
        backsub_closure sp1c(sp1k);
        sp1c.depth7 = largs->depth7;
        sp1c.a7 = largs->a110;
        sp1c.l = largs->l11;
        spawn<backsub_closure> sp1(sp1c);

    }
    ((backsub_cont2_closure*)SN_backsub_cont2.cls.get())->a110 = largs->a110;
    ((backsub_cont2_closure*)SN_backsub_cont2.cls.get())->a100 = largs->a100;
    ((backsub_cont2_closure*)SN_backsub_cont2.cls.get())->a010 = largs->a010;
    ((backsub_cont2_closure*)SN_backsub_cont2.cls.get())->a000 = largs->a000;
    ((backsub_cont2_closure*)SN_backsub_cont2.cls.get())->a7 = largs->a7;
    // Original sync was here
    return;
}
THREAD(backsub_cont2) {
    backsub_cont2_closure *largs = (backsub_cont2_closure*)(args.get());
    largs->a7->child[0] = largs->a000;
    largs->a7->child[1] = largs->a010;
    largs->a7->child[2] = largs->a100;
    largs->a7->child[3] = largs->a110;
    SEND_ARGUMENT(largs->k, largs->a7);
    return;
}
THREAD(cholesky_cont0) {
    cholesky_cont0_closure *largs = (cholesky_cont0_closure*)(args.get());
    __builtin_expect(!(largs->a001), 0) ? __assert_rtn(__func__, "cholesky.cpp", 731, "a00") : (void)0;
    cholesky_cont1_closure SN_cholesky_cont1c(largs->k);
    spawn_next<cholesky_cont1_closure> SN_cholesky_cont1(SN_cholesky_cont1c);
    cont sp0k;
    SN_BIND(SN_cholesky_cont1, &sp0k, a101);
    backsub_closure sp0c(sp0k);
    sp0c.depth7 = largs->depth8;
    sp0c.a7 = largs->a101;
    sp0c.l = largs->a001;
    spawn<backsub_closure> sp0(sp0c);

    ((cholesky_cont1_closure*)SN_cholesky_cont1.cls.get())->a001 = largs->a001;
    ((cholesky_cont1_closure*)SN_cholesky_cont1.cls.get())->A8 = largs->A8;
    ((cholesky_cont1_closure*)SN_cholesky_cont1.cls.get())->a111 = largs->a111;
    ((cholesky_cont1_closure*)SN_cholesky_cont1.cls.get())->a8 = largs->a8;
    ((cholesky_cont1_closure*)SN_cholesky_cont1.cls.get())->depth8 = largs->depth8;
    // Original sync was here
    return;
}
THREAD(cholesky_cont1) {
    cholesky_cont1_closure *largs = (cholesky_cont1_closure*)(args.get());
    __builtin_expect(!(largs->a101), 0) ? __assert_rtn(__func__, "cholesky.cpp", 734, "a10") : (void)0;
    cholesky_cont2_closure SN_cholesky_cont2c(largs->k);
    spawn_next<cholesky_cont2_closure> SN_cholesky_cont2(SN_cholesky_cont2c);
    cont sp0k;
    SN_BIND(SN_cholesky_cont2, &sp0k, a111);
    mul_and_subT_closure sp0c(sp0k);
    sp0c.depth6 = largs->depth8;
    sp0c.lower = 1;
    sp0c.a6 = largs->a101;
    sp0c.b = largs->a101;
    sp0c.r2 = largs->a111;
    spawn<mul_and_subT_closure> sp0(sp0c);

    ((cholesky_cont2_closure*)SN_cholesky_cont2.cls.get())->a101 = largs->a101;
    ((cholesky_cont2_closure*)SN_cholesky_cont2.cls.get())->a8 = largs->a8;
    ((cholesky_cont2_closure*)SN_cholesky_cont2.cls.get())->a001 = largs->a001;
    ((cholesky_cont2_closure*)SN_cholesky_cont2.cls.get())->A8 = largs->A8;
    ((cholesky_cont2_closure*)SN_cholesky_cont2.cls.get())->depth8 = largs->depth8;
    // Original sync was here
    return;
}
THREAD(cholesky_cont2) {
    cholesky_cont2_closure *largs = (cholesky_cont2_closure*)(args.get());
    __builtin_expect(!(largs->a111), 0) ? __assert_rtn(__func__, "cholesky.cpp", 737, "a11") : (void)0;
    cholesky_cont3_closure SN_cholesky_cont3c(largs->k);
    spawn_next<cholesky_cont3_closure> SN_cholesky_cont3(SN_cholesky_cont3c);
    cont sp0k;
    SN_BIND(SN_cholesky_cont3, &sp0k, a111);
    cholesky_closure sp0c(sp0k);
    sp0c.depth8 = largs->depth8;
    sp0c.a8 = largs->a111;
    spawn<cholesky_closure> sp0(sp0c);

    ((cholesky_cont3_closure*)SN_cholesky_cont3.cls.get())->a8 = largs->a8;
    ((cholesky_cont3_closure*)SN_cholesky_cont3.cls.get())->a101 = largs->a101;
    ((cholesky_cont3_closure*)SN_cholesky_cont3.cls.get())->a001 = largs->a001;
    ((cholesky_cont3_closure*)SN_cholesky_cont3.cls.get())->A8 = largs->A8;
    ((cholesky_cont3_closure*)SN_cholesky_cont3.cls.get())->depth8 = largs->depth8;
    // Original sync was here
    return;
}
THREAD(cholesky_cont3) {
    cholesky_cont3_closure *largs = (cholesky_cont3_closure*)(args.get());
    __builtin_expect(!(largs->a111), 0) ? __assert_rtn(__func__, "cholesky.cpp", 740, "a11") : (void)0;
    auto sp0c = std::make_shared<cholesky_afterif1_closure>(largs->k);
    sp0c->depth8 = largs->depth8;
    sp0c->a8 = largs->a8;
    sp0c->A8 = largs->A8;
    sp0c->a001 = largs->a001;
    sp0c->a101 = largs->a101;
    sp0c->a111 = largs->a111;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(cholesky_cont4) {
    cholesky_cont4_closure *largs = (cholesky_cont4_closure*)(args.get());
    cholesky_cont5_closure SN_cholesky_cont5c(largs->k);
    spawn_next<cholesky_cont5_closure> SN_cholesky_cont5(SN_cholesky_cont5c);
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->a111 = largs->a111;
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->a101 = largs->a101;
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->a8 = largs->a8;
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->a001 = largs->a001;
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->A8 = largs->A8;
    ((cholesky_cont5_closure*)SN_cholesky_cont5.cls.get())->depth8 = largs->depth8;
    // Original sync was here
    return;
}
THREAD(cholesky_cont5) {
    cholesky_cont5_closure *largs = (cholesky_cont5_closure*)(args.get());
    auto sp0c = std::make_shared<cholesky_afterif1_closure>(largs->k);
    sp0c->depth8 = largs->depth8;
    sp0c->a8 = largs->a8;
    sp0c->A8 = largs->A8;
    sp0c->a001 = largs->a001;
    sp0c->a101 = largs->a101;
    sp0c->a111 = largs->a111;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(main_cont0) {
    int input_nonzeros;
    int input_blocks;
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    input_blocks = num_blocks(largs->depth9,largs->R1);
    input_nonzeros = num_nonzeros(largs->depth9,largs->R1);
    gettimeofday(&(largs->t1),0);
    main_cont1_closure SN_main_cont1c(largs->k);
    spawn_next<main_cont1_closure> SN_main_cont1(SN_main_cont1c);
    cont sp0k;
    SN_BIND(SN_main_cont1, &sp0k, R1);
    cholesky_closure sp0c(sp0k);
    sp0c.depth8 = largs->depth9;
    sp0c.a8 = largs->R1;
    spawn<cholesky_closure> sp0(sp0c);

    ((main_cont1_closure*)SN_main_cont1.cls.get())->t1 = largs->t1;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->c2 = largs->c2;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->r4 = largs->r4;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->t = largs->t;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->t2 = largs->t2;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->rnd = largs->rnd;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->val = largs->val;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->i6 = largs->i6;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->c1 = largs->c1;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->f = largs->f;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->fr = largs->fr;
    std::memcpy(((main_cont1_closure*)SN_main_cont1.cls.get())->buf, largs->buf, sizeof(largs->buf));
    ((main_cont1_closure*)SN_main_cont1.cls.get())->sizex = largs->sizex;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->input_nonzeros = input_nonzeros;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->input_blocks = input_blocks;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->nonzeros = largs->nonzeros;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->r3 = largs->r3;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->check = largs->check;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->sizey = largs->sizey;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->error = largs->error;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->help = largs->help;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->res2 = largs->res2;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->fc = largs->fc;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->benchmark = largs->benchmark;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->depth9 = largs->depth9;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->size0 = largs->size0;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->argc = largs->argc;
    std::memcpy(((main_cont1_closure*)SN_main_cont1.cls.get())->filename, largs->filename, sizeof(largs->filename));
    ((main_cont1_closure*)SN_main_cont1.cls.get())->A9 = largs->A9;
    ((main_cont1_closure*)SN_main_cont1.cls.get())->argv = largs->argv;
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
    output_blocks = num_blocks(largs->depth9,largs->R1);
    output_nonzeros = num_nonzeros(largs->depth9,largs->R1);
    if (largs->check) {
        printf("Now check result ... \n");
        main_cont2_closure SN_main_cont2c(largs->k);
        spawn_next<main_cont2_closure> SN_main_cont2(SN_main_cont2c);
        cont sp0k;
        SN_BIND(SN_main_cont2, &sp0k, A9);
        mul_and_subT_closure sp0c(sp0k);
        sp0c.depth6 = largs->depth9;
        sp0c.lower = 1;
        sp0c.a6 = largs->R1;
        sp0c.b = largs->R1;
        sp0c.r2 = largs->A9;
        spawn<mul_and_subT_closure> sp0(sp0c);

        ((main_cont2_closure*)SN_main_cont2.cls.get())->t1 = largs->t1;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->r4 = largs->r4;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->rnd = largs->rnd;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->res2 = largs->res2;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->val = largs->val;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->c1 = largs->c1;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->r3 = largs->r3;
        std::memcpy(((main_cont2_closure*)SN_main_cont2.cls.get())->buf, largs->buf, sizeof(largs->buf));
        ((main_cont2_closure*)SN_main_cont2.cls.get())->fc = largs->fc;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->runtime_ms = runtime_ms;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->f = largs->f;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->size0 = largs->size0;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->sizey = largs->sizey;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->output_blocks = output_blocks;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->input_blocks = largs->input_blocks;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->t2 = largs->t2;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->sizex = largs->sizex;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->c2 = largs->c2;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->nonzeros = largs->nonzeros;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->input_nonzeros = largs->input_nonzeros;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->benchmark = largs->benchmark;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->argv = largs->argv;
        std::memcpy(((main_cont2_closure*)SN_main_cont2.cls.get())->filename, largs->filename, sizeof(largs->filename));
        ((main_cont2_closure*)SN_main_cont2.cls.get())->argc = largs->argc;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->check = largs->check;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->i6 = largs->i6;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->help = largs->help;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->output_nonzeros = output_nonzeros;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->depth9 = largs->depth9;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->R1 = largs->R1;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->fr = largs->fr;
        ((main_cont2_closure*)SN_main_cont2.cls.get())->t = largs->t;
        // Original sync was here
    } else {
        auto sp1c = std::make_shared<main_afterif0_closure>(largs->k);
        sp1c->argc = largs->argc;
        sp1c->argv = largs->argv;
        sp1c->A9 = largs->A9;
        sp1c->R1 = largs->R1;
        sp1c->size0 = largs->size0;
        sp1c->depth9 = largs->depth9;
        sp1c->nonzeros = largs->nonzeros;
        sp1c->i6 = largs->i6;
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
        sp1c->r3 = largs->r3;
        sp1c->c1 = largs->c1;
        sp1c->val = largs->val;
        sp1c->res2 = largs->res2;
        sp1c->rnd = largs->rnd;
        sp1c->t = largs->t;
        sp1c->r4 = largs->r4;
        sp1c->c2 = largs->c2;
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
    main_cont2_closure *largs = (main_cont2_closure*)(args.get());
    error = mag(largs->depth9,largs->A9);
    auto sp0c = std::make_shared<main_afterif0_closure>(largs->k);
    sp0c->argc = largs->argc;
    sp0c->argv = largs->argv;
    sp0c->A9 = largs->A9;
    sp0c->R1 = largs->R1;
    sp0c->size0 = largs->size0;
    sp0c->depth9 = largs->depth9;
    sp0c->nonzeros = largs->nonzeros;
    sp0c->i6 = largs->i6;
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
    sp0c->r3 = largs->r3;
    sp0c->c1 = largs->c1;
    sp0c->val = largs->val;
    sp0c->res2 = largs->res2;
    sp0c->rnd = largs->rnd;
    sp0c->t = largs->t;
    sp0c->r4 = largs->r4;
    sp0c->c2 = largs->c2;
    sp0c->t1 = largs->t1;
    sp0c->t2 = largs->t2;
    sp0c->runtime_ms = largs->runtime_ms;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
