#include "cilk_explicit.hh"
/*
 * Copyright (c) 1996 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to use, copy, modify, and distribute the Software without
 * restriction, provided the Software, including any modified copies made
 * under this license, is not distributed for a fee, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE MASSACHUSETTS INSTITUTE OF TECHNOLOGY BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the Massachusetts
 * Institute of Technology shall not be used in advertising or otherwise
 * to promote the sale, use or other dealings in this Software without
 * prior written authorization from the Massachusetts Institute of
 * Technology.
 *
 */

#include <cilk/cilk.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "getoptions.h"

#if CILKSAN
#include "cilksan.h"
#endif

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

#define SizeAtWhichDivideAndConquerIsMoreEfficient 64
#define SizeAtWhichNaiveAlgorithmIsMoreEfficient 16
#define CacheBlockSizeInBytes 32

/* The real numbers we are using --- either double or float */
typedef double REAL;
typedef unsigned long PTR;

/* maximum tolerable relative error (for the checking routine) */
#define EPSILON (1.0E-6)

/*
 * Matrices are stored in row-major order; A is a pointer to
 * the first element of the matrix, and an is the number of elements
 * between two rows. This macro produces the element A[i,j]
 * given A, an, i and j
 */
#define ELEM(A, an, i, j) (A[(i) * (an) + (j)])

unsigned long rand_nxt = 0;

unsigned long long todval(struct timeval *tp);
int cilk_rand();
void mat_vec_mul(int m, int n, int rw, REAL *A, REAL *V, REAL *P, int add);
void matrixmul(int n0, REAL *A0, int an, REAL *B, int bn, REAL *C, int cn);
void FastNaiveMatrixMultiply(REAL *C0, REAL *A1, REAL *B0, unsigned int MatrixSize, unsigned int RowWidthC, unsigned int RowWidthA, unsigned int RowWidthB);
void FastAdditiveNaiveMatrixMultiply(REAL *C1, REAL *A2, REAL *B1, unsigned int MatrixSize0, unsigned int RowWidthC0, unsigned int RowWidthA0, unsigned int RowWidthB0);
void MultiplyByDivideAndConquer(REAL *C3, REAL *A3, REAL *B2, unsigned int MatrixSize1, unsigned int RowWidthC1, unsigned int RowWidthA1, unsigned int RowWidthB1, int AdditiveMode);
THREAD(OptimizedStrassenMultiply);
int compare_vec(int n2, REAL *V1, REAL *V2);
REAL * alloc_vec(int n3);
void free_vec(REAL *V0);
void init_matrix(int n4, REAL *A5, int an0);
int compare_matrix(int n5, REAL *A6, int an1, REAL *B4, int bn0);
REAL * alloc_matrix(int n6);
void free_matrix(REAL *A7);
int usage();
int main(int argc, char **argv);
THREAD(OptimizedStrassenMultiply_cont0);
THREAD(OptimizedStrassenMultiply_cont1);
THREAD(main_cont0);

CLOSURE_DEF(OptimizedStrassenMultiply,
    REAL *C4;
    REAL *A4;
    REAL *B3;
    unsigned int MatrixSize2;
    unsigned int RowWidthC2;
    unsigned int RowWidthA2;
    unsigned int RowWidthB2;
);
CLOSURE_DEF(OptimizedStrassenMultiply_cont0,
    REAL *C4;
    unsigned int QuadrantSize0;
    REAL *C12;
    REAL *C21;
    REAL *C22;
    REAL *M2;
    REAL *M5;
    REAL *T1sMULT;
    PTR RowIncrementC1;
    void *StartHeap;
);
CLOSURE_DEF(OptimizedStrassenMultiply_cont1,
    REAL *C4;
    unsigned int QuadrantSize0;
    REAL *C12;
    REAL *C21;
    REAL *C22;
    REAL *M2;
    REAL *M5;
    REAL *T1sMULT;
    PTR RowIncrementC1;
    void *StartHeap;
);
CLOSURE_DEF(main_cont0,
    REAL *A8;
    REAL *B5;
    REAL *C5;
    int verify;
    int rand_check;
    int n7;
    struct timeval t1;
    struct timeval t2;
);




/*
 * ANGE:
 * recursively multiply an m x n matrix A with size n vector V, and store
 * result in vector size m P.  The value rw is the row width of A, and
 * add the result into P if variable add != 0
 */


/*
 * Naive sequential algorithm, for comparison purposes
 */


/*****************************************************************************
**
** FastNaiveMatrixMultiply
**
** For small to medium sized matrices A, B, and C of size
** MatrixSize * MatrixSize this function performs the operation
** C = A x B efficiently.
**
** Note MatrixSize must be divisible by 8.
**
** INPUT:
**    C = (*C WRITE) Address of top left element of matrix C.
**    A = (*A IS READ ONLY) Address of top left element of matrix A.
**    B = (*B IS READ ONLY) Address of top left element of matrix B.
**    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
**    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
**    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
**    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
**
** OUTPUT:
**    C = (*C WRITE) Matrix C contains A x B. (Initial value of *C undefined.)
**
*****************************************************************************/


/*****************************************************************************
 **
 ** FastAdditiveNaiveMatrixMultiply
 **
 ** For small to medium sized matrices A, B, and C of size
 ** MatrixSize * MatrixSize this function performs the operation
 ** C += A x B efficiently.
 **
 ** Note MatrixSize must be divisible by 8.
 **
 ** INPUT:
 **    C = (*C READ/WRITE) Address of top left element of matrix C.
 **    A = (*A IS READ ONLY) Address of top left element of matrix A.
 **    B = (*B IS READ ONLY) Address of top left element of matrix B.
 **    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
 **    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
 **    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
 **    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
 **
 ** OUTPUT:
 **    C = (*C READ/WRITE) Matrix C contains C + A x B.
 **
 *****************************************************************************/


/*****************************************************************************
 **
 ** MultiplyByDivideAndConquer
 **
 ** For medium to medium-large (would you like fries with that) sized
 ** matrices A, B, and C of size MatrixSize * MatrixSize this function
 ** efficiently performs the operation
 **    C  = A x B (if AdditiveMode == 0)
 **    C += A x B (if AdditiveMode != 0)
 **
 ** Note MatrixSize must be divisible by 16.
 **
 ** INPUT:
 **    C = (*C READ/WRITE) Address of top left element of matrix C.
 **    A = (*A IS READ ONLY) Address of top left element of matrix A.
 **    B = (*B IS READ ONLY) Address of top left element of matrix B.
 **    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
 **    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
 **    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
 **    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
 **    AdditiveMode = 0 if we want C = A x B, otherwise we'll do C += A x B
 **
 ** OUTPUT:
 **    C (+)= A x B. (+ if AdditiveMode != 0)
 **
 *****************************************************************************/


/*****************************************************************************
 **
 ** OptimizedStrassenMultiply
 **
 ** For large matrices A, B, and C of size MatrixSize * MatrixSize this
 ** function performs the operation C = A x B efficiently.
 **
 ** INPUT:
 **    C = (*C WRITE) Address of top left element of matrix C.
 **    A = (*A IS READ ONLY) Address of top left element of matrix A.
 **    B = (*B IS READ ONLY) Address of top left element of matrix B.
 **    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
 **    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
 **    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
 **    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
 **
 ** OUTPUT:
 **    C = (*C WRITE) Matrix C contains A x B. (Initial value of *C undefined.)
 **
 *****************************************************************************/

#define strassen(n, A, an, B, bn, C, cn)                                       \
  OptimizedStrassenMultiply(C, A, B, n, cn, bn, an)


/*
 * Set an size n vector V to random values.
 */
void init_vec(int n, REAL *V) {
  int i;

  for (i = 0; i < n; i++) {
    V[i] = ((double)cilk_rand()) / (double)RAND_MAX;
  }
}

/*
 * Compare two matrices.  Return -1 if they differ more EPSILON.
 */


/*
 * Allocate a vector of size n
 */


/*
 * free a vector
 */


/*
 * Set an n by n matrix A to random values.  The distance between
 * rows is an
 */


/*
 * Compare two matrices.  Print an error message if they differ by
 * more than EPSILON.
 */


/*
 * Allocate a matrix of side n (therefore n^2 elements)
 */


/*
 * free a matrix (Never used because Matteo expects
 *                the OS to clean up his garbage. Tsk. Tsk.)
 */


/*
 * simple test program
 */


const char *specifiers[] = {"-n", "-c", "-rc", "-benchmark", "-h", 0};
int opt_types[] = {INTARG, BOOLARG, BOOLARG, BENCHMARK, BOOLARG, 0};



unsigned long long todval(struct timeval *tp) {
    return (((tp->tv_sec * 1000) * 1000) + tp->tv_usec);
}
int cilk_rand() {
    int result;
    rand_nxt = rand_nxt * 1103515245 + 12345;
    result = ((rand_nxt >> 16) % (((unsigned int) 2147483647) + 1));
    return result;
}
void mat_vec_mul(int m, int n, int rw, REAL *A, REAL *V, REAL *P, int add) {
    int i;
    int j;
    REAL c;
    REAL c0;
    int m1;
    int n1;
    if (((m + n) <= 64)) {
        if (add) {
            for (i = 0;(i < m);(i++)) {
                c = 0;
                for (j = 0;(j < n);(j++)) {
                    c = (c + (A[((i * rw) + j)] * V[j]));
                }
                P[i] += c;
            }
        } else {
            for (i = 0;(i < m);(i++)) {
                c0 = 0;
                for (j = 0;(j < n);(j++)) {
                    c0 = (c0 + (A[((i * rw) + j)] * V[j]));
                }
                P[i] = c0;
            }
        }
    } else {
        if ((m >= n)) {
            m1 = (m >> 1);
            mat_vec_mul(m1,n,rw,A,V,P,add);
            mat_vec_mul((m - m1),n,rw,&(A[((m1 * rw) + 0)]),V,(P + m1),add);
        } else {
            n1 = (n >> 1);
            mat_vec_mul(m,n1,rw,A,V,P,add);
            mat_vec_mul(m,(n - n1),rw,&(A[((0 * rw) + n1)]),(V + n1),P,1);
        }
    }
}
void matrixmul(int n0, REAL *A0, int an, REAL *B, int bn, REAL *C, int cn) {
    int i0;
    int j0;
    int k;
    REAL s;
    for (i0 = 0;(i0 < n0);(++i0)) {
        for (j0 = 0;(j0 < n0);(++j0)) {
            s = 0.;
            for (k = 0;(k < n0);(++k)) {
                s = (s + (A0[((i0 * an) + k)] * B[((k * bn) + j0)]));
            }
            C[((i0 * cn) + j0)] = s;
        }
    }
}
void FastNaiveMatrixMultiply(REAL *C0, REAL *A1, REAL *B0, unsigned int MatrixSize, unsigned int RowWidthC, unsigned int RowWidthA, unsigned int RowWidthB) {
    PTR RowWidthBInBytes;
    PTR RowWidthAInBytes;
    PTR MatrixWidthInBytes;
    PTR RowIncrementC;
    unsigned int Horizontal;
    unsigned int Vertical;
    REAL *ARowStart;
    REAL *BColumnStart;
    REAL FirstARowValue;
    REAL Sum0;
    REAL Sum1;
    REAL Sum2;
    REAL Sum3;
    REAL Sum4;
    REAL Sum5;
    REAL Sum6;
    REAL Sum7;
    unsigned int Products;
    REAL ARowValue;
    RowWidthBInBytes = (RowWidthB << 3);
    RowWidthAInBytes = (RowWidthA << 3);
    MatrixWidthInBytes = (MatrixSize << 3);
    RowIncrementC = ((RowWidthC - MatrixSize) << 3);
    ARowStart = A1;
    for (Vertical = 0;(Vertical < MatrixSize);(Vertical++)) {
        for (Horizontal = 0;(Horizontal < MatrixSize);Horizontal = (Horizontal + 8)) {
            BColumnStart = (B0 + Horizontal);
            FirstARowValue = *((ARowStart++));
            Sum0 = (FirstARowValue * *(BColumnStart));
            Sum1 = (FirstARowValue * *((BColumnStart + 1)));
            Sum2 = (FirstARowValue * *((BColumnStart + 2)));
            Sum3 = (FirstARowValue * *((BColumnStart + 3)));
            Sum4 = (FirstARowValue * *((BColumnStart + 4)));
            Sum5 = (FirstARowValue * *((BColumnStart + 5)));
            Sum6 = (FirstARowValue * *((BColumnStart + 6)));
            Sum7 = (FirstARowValue * *((BColumnStart + 7)));
            for (Products = 1;(Products < MatrixSize);(Products++)) {
                ARowValue = *((ARowStart++));
                BColumnStart = ((REAL *) (((PTR) BColumnStart) + RowWidthBInBytes));
                Sum0 = (Sum0 + (ARowValue * *(BColumnStart)));
                Sum1 = (Sum1 + (ARowValue * *((BColumnStart + 1))));
                Sum2 = (Sum2 + (ARowValue * *((BColumnStart + 2))));
                Sum3 = (Sum3 + (ARowValue * *((BColumnStart + 3))));
                Sum4 = (Sum4 + (ARowValue * *((BColumnStart + 4))));
                Sum5 = (Sum5 + (ARowValue * *((BColumnStart + 5))));
                Sum6 = (Sum6 + (ARowValue * *((BColumnStart + 6))));
                Sum7 = (Sum7 + (ARowValue * *((BColumnStart + 7))));
            }
            ARowStart = ((REAL *) (((PTR) ARowStart) - MatrixWidthInBytes));
            *(C0) = Sum0;
            *((C0 + 1)) = Sum1;
            *((C0 + 2)) = Sum2;
            *((C0 + 3)) = Sum3;
            *((C0 + 4)) = Sum4;
            *((C0 + 5)) = Sum5;
            *((C0 + 6)) = Sum6;
            *((C0 + 7)) = Sum7;
            C0 = (C0 + 8);
        }
        ARowStart = ((REAL *) (((PTR) ARowStart) + RowWidthAInBytes));
        C0 = ((REAL *) (((PTR) C0) + RowIncrementC));
    }
}
void FastAdditiveNaiveMatrixMultiply(REAL *C1, REAL *A2, REAL *B1, unsigned int MatrixSize0, unsigned int RowWidthC0, unsigned int RowWidthA0, unsigned int RowWidthB0) {
    PTR RowWidthBInBytes0;
    PTR RowWidthAInBytes0;
    PTR MatrixWidthInBytes0;
    PTR RowIncrementC0;
    unsigned int Horizontal0;
    unsigned int Vertical0;
    REAL *ARowStart0;
    REAL *BColumnStart0;
    REAL Sum00;
    REAL Sum10;
    REAL Sum20;
    REAL Sum30;
    REAL Sum40;
    REAL Sum50;
    REAL Sum60;
    REAL Sum70;
    unsigned int Products0;
    REAL ARowValue0;
    RowWidthBInBytes0 = (RowWidthB0 << 3);
    RowWidthAInBytes0 = (RowWidthA0 << 3);
    MatrixWidthInBytes0 = (MatrixSize0 << 3);
    RowIncrementC0 = ((RowWidthC0 - MatrixSize0) << 3);
    ARowStart0 = A2;
    for (Vertical0 = 0;(Vertical0 < MatrixSize0);(Vertical0++)) {
        for (Horizontal0 = 0;(Horizontal0 < MatrixSize0);Horizontal0 = (Horizontal0 + 8)) {
            BColumnStart0 = (B1 + Horizontal0);
            Sum00 = *(C1);
            Sum10 = *((C1 + 1));
            Sum20 = *((C1 + 2));
            Sum30 = *((C1 + 3));
            Sum40 = *((C1 + 4));
            Sum50 = *((C1 + 5));
            Sum60 = *((C1 + 6));
            Sum70 = *((C1 + 7));
            for (Products0 = 0;(Products0 < MatrixSize0);(Products0++)) {
                ARowValue0 = *((ARowStart0++));
                Sum00 = (Sum00 + (ARowValue0 * *(BColumnStart0)));
                Sum10 = (Sum10 + (ARowValue0 * *((BColumnStart0 + 1))));
                Sum20 = (Sum20 + (ARowValue0 * *((BColumnStart0 + 2))));
                Sum30 = (Sum30 + (ARowValue0 * *((BColumnStart0 + 3))));
                Sum40 = (Sum40 + (ARowValue0 * *((BColumnStart0 + 4))));
                Sum50 = (Sum50 + (ARowValue0 * *((BColumnStart0 + 5))));
                Sum60 = (Sum60 + (ARowValue0 * *((BColumnStart0 + 6))));
                Sum70 = (Sum70 + (ARowValue0 * *((BColumnStart0 + 7))));
                BColumnStart0 = ((REAL *) (((PTR) BColumnStart0) + RowWidthBInBytes0));
            }
            ARowStart0 = ((REAL *) (((PTR) ARowStart0) - MatrixWidthInBytes0));
            *(C1) = Sum00;
            *((C1 + 1)) = Sum10;
            *((C1 + 2)) = Sum20;
            *((C1 + 3)) = Sum30;
            *((C1 + 4)) = Sum40;
            *((C1 + 5)) = Sum50;
            *((C1 + 6)) = Sum60;
            *((C1 + 7)) = Sum70;
            C1 = (C1 + 8);
        }
        ARowStart0 = ((REAL *) (((PTR) ARowStart0) + RowWidthAInBytes0));
        C1 = ((REAL *) (((PTR) C1) + RowIncrementC0));
    }
}
void MultiplyByDivideAndConquer(REAL *C3, REAL *A3, REAL *B2, unsigned int MatrixSize1, unsigned int RowWidthC1, unsigned int RowWidthA1, unsigned int RowWidthB1, int AdditiveMode) {
    REAL *A01;
    REAL *A10;
    REAL *A11;
    REAL *B01;
    REAL *B10;
    REAL *B11;
    REAL *C01;
    REAL *C10;
    REAL *C11;
    unsigned int QuadrantSize;
    QuadrantSize = (MatrixSize1 >> 1);
    A01 = (A3 + QuadrantSize);
    A10 = (A3 + (RowWidthA1 * QuadrantSize));
    A11 = (A10 + QuadrantSize);
    B01 = (B2 + QuadrantSize);
    B10 = (B2 + (RowWidthB1 * QuadrantSize));
    B11 = (B10 + QuadrantSize);
    C01 = (C3 + QuadrantSize);
    C10 = (C3 + (RowWidthC1 * QuadrantSize));
    C11 = (C10 + QuadrantSize);
    if ((QuadrantSize > 16)) {
        MultiplyByDivideAndConquer(C3,A3,B2,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1,AdditiveMode);
        MultiplyByDivideAndConquer(C01,A3,B01,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1,AdditiveMode);
        MultiplyByDivideAndConquer(C11,A10,B01,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1,AdditiveMode);
        MultiplyByDivideAndConquer(C10,A10,B2,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1,AdditiveMode);
        MultiplyByDivideAndConquer(C3,A01,B10,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1,1);
        MultiplyByDivideAndConquer(C01,A01,B11,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1,1);
        MultiplyByDivideAndConquer(C11,A11,B11,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1,1);
        MultiplyByDivideAndConquer(C10,A11,B10,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1,1);
    } else {
        if (AdditiveMode) {
            FastAdditiveNaiveMatrixMultiply(C3,A3,B2,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
            FastAdditiveNaiveMatrixMultiply(C01,A3,B01,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
            FastAdditiveNaiveMatrixMultiply(C11,A10,B01,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
            FastAdditiveNaiveMatrixMultiply(C10,A10,B2,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
        } else {
            FastNaiveMatrixMultiply(C3,A3,B2,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
            FastNaiveMatrixMultiply(C01,A3,B01,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
            FastNaiveMatrixMultiply(C11,A10,B01,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
            FastNaiveMatrixMultiply(C10,A10,B2,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
        }
        FastAdditiveNaiveMatrixMultiply(C3,A01,B10,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
        FastAdditiveNaiveMatrixMultiply(C01,A01,B11,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
        FastAdditiveNaiveMatrixMultiply(C11,A11,B11,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
        FastAdditiveNaiveMatrixMultiply(C10,A11,B10,QuadrantSize,RowWidthC1,RowWidthA1,RowWidthB1);
    }
    return ;
}
THREAD(OptimizedStrassenMultiply) {
    unsigned int QuadrantSize0;
    unsigned int QuadrantSizeInBytes;
    unsigned int Column;
    unsigned int Row;
    REAL *A12;
    REAL *B12;
    REAL *C12;
    REAL *A21;
    REAL *B21;
    REAL *C21;
    REAL *A22;
    REAL *B22;
    REAL *C22;
    REAL *S1;
    REAL *S2;
    REAL *S3;
    REAL *S4;
    REAL *S5;
    REAL *S6;
    REAL *S7;
    REAL *S8;
    REAL *M2;
    REAL *M5;
    REAL *T1sMULT;
    PTR TempMatrixOffset;
    PTR MatrixOffsetA;
    PTR MatrixOffsetB;
    PTR RowIncrementA;
    PTR RowIncrementB;
    PTR RowIncrementC1;
    char *Heap;
    void *StartHeap;
    char *_tmp;
    OptimizedStrassenMultiply_closure *largs = (OptimizedStrassenMultiply_closure*)(args.get());
    QuadrantSize0 = (largs->MatrixSize2 >> 1);
    QuadrantSizeInBytes = (((sizeof(REAL) * QuadrantSize0) * QuadrantSize0) + 32);
    TempMatrixOffset = 0;
    MatrixOffsetA = 0;
    MatrixOffsetB = 0;
    RowIncrementA = ((largs->RowWidthA2 - QuadrantSize0) << 3);
    RowIncrementB = ((largs->RowWidthB2 - QuadrantSize0) << 3);
    RowIncrementC1 = ((largs->RowWidthC2 - QuadrantSize0) << 3);
    if ((largs->MatrixSize2 <= 64)) {
        MultiplyByDivideAndConquer(largs->C4,largs->A4,largs->B3,largs->MatrixSize2,largs->RowWidthC2,largs->RowWidthA2,largs->RowWidthB2,0);
        SEND_ARGUMENT(largs->k, 0);
    } else {
        A12 = (largs->A4 + QuadrantSize0);
        B12 = (largs->B3 + QuadrantSize0);
        C12 = (largs->C4 + QuadrantSize0);
        A21 = (largs->A4 + (largs->RowWidthA2 * QuadrantSize0));
        B21 = (largs->B3 + (largs->RowWidthB2 * QuadrantSize0));
        C21 = (largs->C4 + (largs->RowWidthC2 * QuadrantSize0));
        A22 = (A21 + QuadrantSize0);
        B22 = (B21 + QuadrantSize0);
        C22 = (C21 + QuadrantSize0);
        _tmp = ((char *) malloc((QuadrantSizeInBytes * 11)));
        Heap = _tmp;
        StartHeap = Heap;
        OptimizedStrassenMultiply_cont0_closure SN_OptimizedStrassenMultiply_cont0c(largs->k);
        spawn_next<OptimizedStrassenMultiply_cont0_closure> SN_OptimizedStrassenMultiply_cont0(SN_OptimizedStrassenMultiply_cont0c);
        if ((((PTR) Heap) & 31)) {
            Heap = ((char *) ((((PTR) Heap) + 32) - (((PTR) Heap) & 31)));
        }
        S1 = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        S2 = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        S3 = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        S4 = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        S5 = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        S6 = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        S7 = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        S8 = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        M2 = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        M5 = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        T1sMULT = ((REAL *) Heap);
        Heap = (Heap + QuadrantSizeInBytes);
        for (Row = 0;(Row < QuadrantSize0);(Row++)) {
            for (Column = 0;(Column < QuadrantSize0);(Column++)) {
                *(((REAL *) (((PTR) S1) + TempMatrixOffset))) = (*(((REAL *) (((PTR) A21) + MatrixOffsetA))) + *(((REAL *) (((PTR) A22) + MatrixOffsetA))));
                *(((REAL *) (((PTR) S2) + TempMatrixOffset))) = (*(((REAL *) (((PTR) S1) + TempMatrixOffset))) - *(((REAL *) (((PTR) largs->A4) + MatrixOffsetA))));
                *(((REAL *) (((PTR) S4) + TempMatrixOffset))) = (*(((REAL *) (((PTR) A12) + MatrixOffsetA))) - *(((REAL *) (((PTR) S2) + TempMatrixOffset))));
                *(((REAL *) (((PTR) S5) + TempMatrixOffset))) = (*(((REAL *) (((PTR) B12) + MatrixOffsetB))) - *(((REAL *) (((PTR) largs->B3) + MatrixOffsetB))));
                *(((REAL *) (((PTR) S6) + TempMatrixOffset))) = (*(((REAL *) (((PTR) B22) + MatrixOffsetB))) - *(((REAL *) (((PTR) S5) + TempMatrixOffset))));
                *(((REAL *) (((PTR) S8) + TempMatrixOffset))) = (*(((REAL *) (((PTR) S6) + TempMatrixOffset))) - *(((REAL *) (((PTR) B21) + MatrixOffsetB))));
                *(((REAL *) (((PTR) S3) + TempMatrixOffset))) = (*(((REAL *) (((PTR) largs->A4) + MatrixOffsetA))) - *(((REAL *) (((PTR) A21) + MatrixOffsetA))));
                *(((REAL *) (((PTR) S7) + TempMatrixOffset))) = (*(((REAL *) (((PTR) B22) + MatrixOffsetB))) - *(((REAL *) (((PTR) B12) + MatrixOffsetB))));
                TempMatrixOffset = (TempMatrixOffset + sizeof(REAL));
                MatrixOffsetA = (MatrixOffsetA + sizeof(REAL));
                MatrixOffsetB = (MatrixOffsetB + sizeof(REAL));
            }
            MatrixOffsetA = (MatrixOffsetA + RowIncrementA);
            MatrixOffsetB = (MatrixOffsetB + RowIncrementB);
        }
        cont sp0k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp0k);
        OptimizedStrassenMultiply_closure sp0c(sp0k);
        sp0c.C4 = M2;
        sp0c.A4 = largs->A4;
        sp0c.B3 = largs->B3;
        sp0c.MatrixSize2 = QuadrantSize0;
        sp0c.RowWidthC2 = QuadrantSize0;
        sp0c.RowWidthA2 = largs->RowWidthA2;
        sp0c.RowWidthB2 = largs->RowWidthB2;
        spawn<OptimizedStrassenMultiply_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp1k);
        OptimizedStrassenMultiply_closure sp1c(sp1k);
        sp1c.C4 = M5;
        sp1c.A4 = S1;
        sp1c.B3 = S5;
        sp1c.MatrixSize2 = QuadrantSize0;
        sp1c.RowWidthC2 = QuadrantSize0;
        sp1c.RowWidthA2 = QuadrantSize0;
        sp1c.RowWidthB2 = QuadrantSize0;
        spawn<OptimizedStrassenMultiply_closure> sp1(sp1c);

        cont sp2k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp2k);
        OptimizedStrassenMultiply_closure sp2c(sp2k);
        sp2c.C4 = T1sMULT;
        sp2c.A4 = S2;
        sp2c.B3 = S6;
        sp2c.MatrixSize2 = QuadrantSize0;
        sp2c.RowWidthC2 = QuadrantSize0;
        sp2c.RowWidthA2 = QuadrantSize0;
        sp2c.RowWidthB2 = QuadrantSize0;
        spawn<OptimizedStrassenMultiply_closure> sp2(sp2c);

        cont sp3k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp3k);
        OptimizedStrassenMultiply_closure sp3c(sp3k);
        sp3c.C4 = C22;
        sp3c.A4 = S3;
        sp3c.B3 = S7;
        sp3c.MatrixSize2 = QuadrantSize0;
        sp3c.RowWidthC2 = largs->RowWidthC2;
        sp3c.RowWidthA2 = QuadrantSize0;
        sp3c.RowWidthB2 = QuadrantSize0;
        spawn<OptimizedStrassenMultiply_closure> sp3(sp3c);

        cont sp4k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp4k);
        OptimizedStrassenMultiply_closure sp4c(sp4k);
        sp4c.C4 = largs->C4;
        sp4c.A4 = A12;
        sp4c.B3 = B21;
        sp4c.MatrixSize2 = QuadrantSize0;
        sp4c.RowWidthC2 = largs->RowWidthC2;
        sp4c.RowWidthA2 = largs->RowWidthA2;
        sp4c.RowWidthB2 = largs->RowWidthB2;
        spawn<OptimizedStrassenMultiply_closure> sp4(sp4c);

        cont sp5k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp5k);
        OptimizedStrassenMultiply_closure sp5c(sp5k);
        sp5c.C4 = C12;
        sp5c.A4 = S4;
        sp5c.B3 = B22;
        sp5c.MatrixSize2 = QuadrantSize0;
        sp5c.RowWidthC2 = largs->RowWidthC2;
        sp5c.RowWidthA2 = QuadrantSize0;
        sp5c.RowWidthB2 = largs->RowWidthB2;
        spawn<OptimizedStrassenMultiply_closure> sp5(sp5c);

        cont sp6k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp6k);
        OptimizedStrassenMultiply_closure sp6c(sp6k);
        sp6c.C4 = C21;
        sp6c.A4 = A22;
        sp6c.B3 = S8;
        sp6c.MatrixSize2 = QuadrantSize0;
        sp6c.RowWidthC2 = largs->RowWidthC2;
        sp6c.RowWidthA2 = largs->RowWidthA2;
        sp6c.RowWidthB2 = QuadrantSize0;
        spawn<OptimizedStrassenMultiply_closure> sp6(sp6c);

        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->T1sMULT = T1sMULT;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->M5 = M5;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->M2 = M2;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->C22 = C22;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->C21 = C21;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->RowIncrementC1 = RowIncrementC1;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->C12 = C12;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->QuadrantSize0 = QuadrantSize0;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->StartHeap = StartHeap;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->C4 = largs->C4;
        // Original sync was here
    }
    return;
}
int compare_vec(int n2, REAL *V1, REAL *V2) {
    int i1;
    REAL c1;
    REAL sum;
    sum = 0.;
    for (i1 = 0;(i1 < n2);(++i1)) {
        c1 = (V1[i1] - V2[i1]);
        if ((c1 < 0.)) {
            c1 = (-c1);
        }
        sum = (sum + c1);
        if ((c1 > 9.9999999999999995E-7)) {
            return (-1);
        } else {
        }
    }
    printf("Sum of errors: %g\n",sum);
    return 0;
}
REAL * alloc_vec(int n3) {
    return ((REAL *) malloc((n3 * sizeof(REAL))));
}
void free_vec(REAL *V0) {
    free(V0);
}
void init_matrix(int n4, REAL *A5, int an0) {
    int i2;
    int j1;
    for (i2 = 0;(i2 < n4);(++i2)) {
        for (j1 = 0;(j1 < n4);(++j1)) {
            A5[((i2 * an0) + j1)] = (((double) cilk_rand()) / ((double) 2147483647));
        }
    }
}
int compare_matrix(int n5, REAL *A6, int an1, REAL *B4, int bn0) {
    int i3;
    int j2;
    REAL c2;
    for (i3 = 0;(i3 < n5);(++i3)) {
        for (j2 = 0;(j2 < n5);(++j2)) {
            c2 = (A6[((i3 * an1) + j2)] - B4[((i3 * bn0) + j2)]);
            if ((c2 < 0.)) {
                c2 = (-c2);
            }
            c2 = (c2 / A6[((i3 * an1) + j2)]);
            if ((c2 > 9.9999999999999995E-7)) {
                return (-1);
            } else {
            }
        }
    }
    return 0;
}
REAL * alloc_matrix(int n6) {
    return ((REAL *) malloc(((n6 * n6) * sizeof(REAL))));
}
void free_matrix(REAL *A7) {
    free(A7);
}
int usage() {
    fprintf(__stderrp,"\nUsage: strassen [<cilk-options>] [-n #] [-c] [-rc]\n\nMultiplies two randomly generated n x n matrices. To check for\ncorrectness use -c using iterative matrix multiply or use -rc \nusing randomized algorithm due to Freivalds.\n\n");
    return 1;
}
int main(int argc, char **argv) {
    REAL *A8;
    REAL *B5;
    REAL *C5;
    int verify;
    int rand_check;
    int benchmark;
    int help;
    int n7;
    struct timeval t1;
    n7 = 512;
    verify = 0;
    rand_check = 0;
    get_options(argc,argv,specifiers,opt_types,&(n7),&(verify),&(rand_check),&(benchmark),&(help));
    if (help) {
        return usage();
    } else {
        if (benchmark) {
            switch (benchmark) {
  case 1:
    n7 = 512;
    break;
  case 2:
    n7 = 2048;
    break;
  case 3:
    n7 = 4096;
    break;
}
;
        }
        if ((((n7 & (n7 - 1)) != 0) || ((n7 % 16) != 0))) {
            printf("%d: matrix size must be a power of 2 and a multiple of %d\n",n7,16);
            return 1;
        } else {
            A8 = alloc_matrix(n7);
            B5 = alloc_matrix(n7);
            C5 = alloc_matrix(n7);
            init_matrix(n7,A8,n7);
            init_matrix(n7,B5,n7);
            gettimeofday(&(t1),0);
            main_cont0_closure SN_main_cont0c(CONT_DUMMY);
            spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
            cont sp0k;
            SN_BIND_VOID(SN_main_cont0, &sp0k);
            OptimizedStrassenMultiply_closure sp0c(sp0k);
            sp0c.C4 = C5;
            sp0c.A4 = A8;
            sp0c.B3 = B5;
            sp0c.MatrixSize2 = n7;
            sp0c.RowWidthC2 = n7;
            sp0c.RowWidthA2 = n7;
            sp0c.RowWidthB2 = n7;
            spawn<OptimizedStrassenMultiply_closure> sp0(sp0c);

            ((main_cont0_closure*)SN_main_cont0.cls.get())->n7 = n7;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->verify = verify;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->C5 = C5;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->t1 = t1;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->B5 = B5;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->rand_check = rand_check;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->A8 = A8;
            // Original sync was here
        }
    }
}
THREAD(OptimizedStrassenMultiply_cont0) {
    OptimizedStrassenMultiply_cont0_closure *largs = (OptimizedStrassenMultiply_cont0_closure*)(args.get());
    OptimizedStrassenMultiply_cont1_closure SN_OptimizedStrassenMultiply_cont1c(largs->k);
    spawn_next<OptimizedStrassenMultiply_cont1_closure> SN_OptimizedStrassenMultiply_cont1(SN_OptimizedStrassenMultiply_cont1c);
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->StartHeap = largs->StartHeap;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->T1sMULT = largs->T1sMULT;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->M2 = largs->M2;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->C22 = largs->C22;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->RowIncrementC1 = largs->RowIncrementC1;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->C21 = largs->C21;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->C12 = largs->C12;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->M5 = largs->M5;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->QuadrantSize0 = largs->QuadrantSize0;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->C4 = largs->C4;
    // Original sync was here
    return;
}
THREAD(OptimizedStrassenMultiply_cont1) {
    unsigned int Column;
    unsigned int Row;
    REAL LocalM5_0;
    REAL LocalM5_1;
    REAL LocalM5_2;
    REAL LocalM5_3;
    REAL LocalM2_0;
    REAL LocalM2_1;
    REAL LocalM2_2;
    REAL LocalM2_3;
    REAL T1_0;
    REAL T1_1;
    REAL T1_2;
    REAL T1_3;
    REAL T2_0;
    REAL T2_1;
    REAL T2_2;
    REAL T2_3;
    OptimizedStrassenMultiply_cont1_closure *largs = (OptimizedStrassenMultiply_cont1_closure*)(args.get());
    for (Row = 0;(Row < largs->QuadrantSize0);(Row++)) {
        for (Column = 0;(Column < largs->QuadrantSize0);Column = (Column + 4)) {
            LocalM5_0 = *(largs->M5);
            LocalM5_1 = *((largs->M5 + 1));
            LocalM5_2 = *((largs->M5 + 2));
            LocalM5_3 = *((largs->M5 + 3));
            LocalM2_0 = *(largs->M2);
            LocalM2_1 = *((largs->M2 + 1));
            LocalM2_2 = *((largs->M2 + 2));
            LocalM2_3 = *((largs->M2 + 3));
            T1_0 = (*(largs->T1sMULT) + LocalM2_0);
            T1_1 = (*((largs->T1sMULT + 1)) + LocalM2_1);
            T1_2 = (*((largs->T1sMULT + 2)) + LocalM2_2);
            T1_3 = (*((largs->T1sMULT + 3)) + LocalM2_3);
            T2_0 = (*(largs->C22) + T1_0);
            T2_1 = (*((largs->C22 + 1)) + T1_1);
            T2_2 = (*((largs->C22 + 2)) + T1_2);
            T2_3 = (*((largs->C22 + 3)) + T1_3);
            (*(largs->C4)) += LocalM2_0;
            (*(largs->C4 + 1)) += LocalM2_1;
            (*(largs->C4 + 2)) += LocalM2_2;
            (*(largs->C4 + 3)) += LocalM2_3;
            (*(largs->C12)) += LocalM5_0 + T1_0;
            (*(largs->C12 + 1)) += LocalM5_1 + T1_1;
            (*(largs->C12 + 2)) += LocalM5_2 + T1_2;
            (*(largs->C12 + 3)) += LocalM5_3 + T1_3;
            *(largs->C22) = (LocalM5_0 + T2_0);
            *((largs->C22 + 1)) = (LocalM5_1 + T2_1);
            *((largs->C22 + 2)) = (LocalM5_2 + T2_2);
            *((largs->C22 + 3)) = (LocalM5_3 + T2_3);
            *(largs->C21) = ((-*(largs->C21)) + T2_0);
            *((largs->C21 + 1)) = ((-*((largs->C21 + 1))) + T2_1);
            *((largs->C21 + 2)) = ((-*((largs->C21 + 2))) + T2_2);
            *((largs->C21 + 3)) = ((-*((largs->C21 + 3))) + T2_3);
            largs->M5 = (largs->M5 + 4);
            largs->M2 = (largs->M2 + 4);
            largs->T1sMULT = (largs->T1sMULT + 4);
            largs->C4 = (largs->C4 + 4);
            largs->C12 = (largs->C12 + 4);
            largs->C21 = (largs->C21 + 4);
            largs->C22 = (largs->C22 + 4);
        }
        largs->C4 = ((REAL *) (((PTR) largs->C4) + largs->RowIncrementC1));
        largs->C12 = ((REAL *) (((PTR) largs->C12) + largs->RowIncrementC1));
        largs->C21 = ((REAL *) (((PTR) largs->C21) + largs->RowIncrementC1));
        largs->C22 = ((REAL *) (((PTR) largs->C22) + largs->RowIncrementC1));
    }
    free(largs->StartHeap);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(main_cont0) {
    unsigned long long runtime_ms;
    REAL *R;
    REAL *V10;
    REAL *V20;
    REAL *C2;
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    printf("%f\n",(runtime_ms / 1000.));
    if (largs->rand_check) {
        R = alloc_vec(largs->n7);
        V10 = alloc_vec(largs->n7);
        V20 = alloc_vec(largs->n7);
        mat_vec_mul(largs->n7,largs->n7,largs->n7,largs->B5,R,V10,0);
        mat_vec_mul(largs->n7,largs->n7,largs->n7,largs->A8,V10,V20,0);
        mat_vec_mul(largs->n7,largs->n7,largs->n7,largs->C5,R,V10,0);
        largs->rand_check = compare_vec(largs->n7,V10,V20);
        free_vec(R);
        free_vec(V10);
        free_vec(V20);
    } else {
        if (largs->verify) {
            fprintf(__stderrp,"Checking results ... \n");
            C2 = alloc_matrix(largs->n7);
            matrixmul(largs->n7,largs->A8,largs->n7,largs->B5,largs->n7,C2,largs->n7);
            largs->verify = compare_matrix(largs->n7,largs->C5,largs->n7,C2,largs->n7);
            free_matrix(C2);
        }
    }
    if ((largs->rand_check || largs->verify)) {
        fprintf(__stderrp,"WRONG RESULT!\n");
    } else {
        fprintf(__stderrp,"\nCilk Example: strassen\n");
        fprintf(__stderrp,"Options: n = %d\n\n",largs->n7);
    }
    free_matrix(largs->A8);
    free_matrix(largs->B5);
    free_matrix(largs->C5);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
