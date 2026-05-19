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
void matrixmul(int n, REAL *A, int an, REAL *B, int bn, REAL *C, int cn);
void FastNaiveMatrixMultiply(REAL *C, REAL *A, REAL *B, unsigned int MatrixSize, unsigned int RowWidthC, unsigned int RowWidthA, unsigned int RowWidthB);
void FastAdditiveNaiveMatrixMultiply(REAL *C, REAL *A, REAL *B, unsigned int MatrixSize, unsigned int RowWidthC, unsigned int RowWidthA, unsigned int RowWidthB);
void MultiplyByDivideAndConquer(REAL *C, REAL *A, REAL *B, unsigned int MatrixSize, unsigned int RowWidthC, unsigned int RowWidthA, unsigned int RowWidthB, int AdditiveMode);
THREAD(OptimizedStrassenMultiply);
int compare_vec(int n, REAL *V1, REAL *V2);
REAL * alloc_vec(int n);
void free_vec(REAL *V);
void init_matrix(int n, REAL *A, int an);
int compare_matrix(int n, REAL *A, int an, REAL *B, int bn);
REAL * alloc_matrix(int n);
void free_matrix(REAL *A);
int usage();
int main(int argc, char **argv);
THREAD(OptimizedStrassenMultiply_cont0);
THREAD(OptimizedStrassenMultiply_cont1);
THREAD(main_cont0);

CLOSURE_DEF(OptimizedStrassenMultiply,
    REAL *C;
    REAL *A;
    REAL *B;
    unsigned int MatrixSize;
    unsigned int RowWidthC;
    unsigned int RowWidthA;
    unsigned int RowWidthB;
);
CLOSURE_DEF(OptimizedStrassenMultiply_cont0,
    REAL *C;
    unsigned int QuadrantSize;
    REAL *C12;
    REAL *C21;
    REAL *C22;
    REAL *M2;
    REAL *M5;
    REAL *T1sMULT;
    PTR RowIncrementC;
    void *StartHeap;
);
CLOSURE_DEF(OptimizedStrassenMultiply_cont1,
    REAL *C;
    unsigned int QuadrantSize;
    REAL *C12;
    REAL *C21;
    REAL *C22;
    REAL *M2;
    REAL *M5;
    REAL *T1sMULT;
    PTR RowIncrementC;
    void *StartHeap;
);
CLOSURE_DEF(main_cont0,
    REAL *A;
    REAL *B;
    REAL *C;
    int verify;
    int rand_check;
    int n;
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
void matrixmul(int n, REAL *A, int an, REAL *B, int bn, REAL *C, int cn) {
    int i;
    int j;
    int k0;
    REAL s;
    for (i = 0;(i < n);(++i)) {
        for (j = 0;(j < n);(++j)) {
            s = 0.;
            for (k0 = 0;(k0 < n);(++k0)) {
                s = (s + (A[((i * an) + k0)] * B[((k0 * bn) + j)]));
            }
            C[((i * cn) + j)] = s;
        }
    }
}
void FastNaiveMatrixMultiply(REAL *C, REAL *A, REAL *B, unsigned int MatrixSize, unsigned int RowWidthC, unsigned int RowWidthA, unsigned int RowWidthB) {
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
    ARowStart = A;
    for (Vertical = 0;(Vertical < MatrixSize);(Vertical++)) {
        for (Horizontal = 0;(Horizontal < MatrixSize);Horizontal = (Horizontal + 8)) {
            BColumnStart = (B + Horizontal);
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
            *(C) = Sum0;
            *((C + 1)) = Sum1;
            *((C + 2)) = Sum2;
            *((C + 3)) = Sum3;
            *((C + 4)) = Sum4;
            *((C + 5)) = Sum5;
            *((C + 6)) = Sum6;
            *((C + 7)) = Sum7;
            C = (C + 8);
        }
        ARowStart = ((REAL *) (((PTR) ARowStart) + RowWidthAInBytes));
        C = ((REAL *) (((PTR) C) + RowIncrementC));
    }
}
void FastAdditiveNaiveMatrixMultiply(REAL *C, REAL *A, REAL *B, unsigned int MatrixSize, unsigned int RowWidthC, unsigned int RowWidthA, unsigned int RowWidthB) {
    PTR RowWidthBInBytes;
    PTR RowWidthAInBytes;
    PTR MatrixWidthInBytes;
    PTR RowIncrementC;
    unsigned int Horizontal;
    unsigned int Vertical;
    REAL *ARowStart;
    REAL *BColumnStart;
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
    ARowStart = A;
    for (Vertical = 0;(Vertical < MatrixSize);(Vertical++)) {
        for (Horizontal = 0;(Horizontal < MatrixSize);Horizontal = (Horizontal + 8)) {
            BColumnStart = (B + Horizontal);
            Sum0 = *(C);
            Sum1 = *((C + 1));
            Sum2 = *((C + 2));
            Sum3 = *((C + 3));
            Sum4 = *((C + 4));
            Sum5 = *((C + 5));
            Sum6 = *((C + 6));
            Sum7 = *((C + 7));
            for (Products = 0;(Products < MatrixSize);(Products++)) {
                ARowValue = *((ARowStart++));
                Sum0 = (Sum0 + (ARowValue * *(BColumnStart)));
                Sum1 = (Sum1 + (ARowValue * *((BColumnStart + 1))));
                Sum2 = (Sum2 + (ARowValue * *((BColumnStart + 2))));
                Sum3 = (Sum3 + (ARowValue * *((BColumnStart + 3))));
                Sum4 = (Sum4 + (ARowValue * *((BColumnStart + 4))));
                Sum5 = (Sum5 + (ARowValue * *((BColumnStart + 5))));
                Sum6 = (Sum6 + (ARowValue * *((BColumnStart + 6))));
                Sum7 = (Sum7 + (ARowValue * *((BColumnStart + 7))));
                BColumnStart = ((REAL *) (((PTR) BColumnStart) + RowWidthBInBytes));
            }
            ARowStart = ((REAL *) (((PTR) ARowStart) - MatrixWidthInBytes));
            *(C) = Sum0;
            *((C + 1)) = Sum1;
            *((C + 2)) = Sum2;
            *((C + 3)) = Sum3;
            *((C + 4)) = Sum4;
            *((C + 5)) = Sum5;
            *((C + 6)) = Sum6;
            *((C + 7)) = Sum7;
            C = (C + 8);
        }
        ARowStart = ((REAL *) (((PTR) ARowStart) + RowWidthAInBytes));
        C = ((REAL *) (((PTR) C) + RowIncrementC));
    }
}
void MultiplyByDivideAndConquer(REAL *C, REAL *A, REAL *B, unsigned int MatrixSize, unsigned int RowWidthC, unsigned int RowWidthA, unsigned int RowWidthB, int AdditiveMode) {
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
    QuadrantSize = (MatrixSize >> 1);
    A01 = (A + QuadrantSize);
    A10 = (A + (RowWidthA * QuadrantSize));
    A11 = (A10 + QuadrantSize);
    B01 = (B + QuadrantSize);
    B10 = (B + (RowWidthB * QuadrantSize));
    B11 = (B10 + QuadrantSize);
    C01 = (C + QuadrantSize);
    C10 = (C + (RowWidthC * QuadrantSize));
    C11 = (C10 + QuadrantSize);
    if ((QuadrantSize > 16)) {
        MultiplyByDivideAndConquer(C,A,B,QuadrantSize,RowWidthC,RowWidthA,RowWidthB,AdditiveMode);
        MultiplyByDivideAndConquer(C01,A,B01,QuadrantSize,RowWidthC,RowWidthA,RowWidthB,AdditiveMode);
        MultiplyByDivideAndConquer(C11,A10,B01,QuadrantSize,RowWidthC,RowWidthA,RowWidthB,AdditiveMode);
        MultiplyByDivideAndConquer(C10,A10,B,QuadrantSize,RowWidthC,RowWidthA,RowWidthB,AdditiveMode);
        MultiplyByDivideAndConquer(C,A01,B10,QuadrantSize,RowWidthC,RowWidthA,RowWidthB,1);
        MultiplyByDivideAndConquer(C01,A01,B11,QuadrantSize,RowWidthC,RowWidthA,RowWidthB,1);
        MultiplyByDivideAndConquer(C11,A11,B11,QuadrantSize,RowWidthC,RowWidthA,RowWidthB,1);
        MultiplyByDivideAndConquer(C10,A11,B10,QuadrantSize,RowWidthC,RowWidthA,RowWidthB,1);
    } else {
        if (AdditiveMode) {
            FastAdditiveNaiveMatrixMultiply(C,A,B,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
            FastAdditiveNaiveMatrixMultiply(C01,A,B01,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
            FastAdditiveNaiveMatrixMultiply(C11,A10,B01,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
            FastAdditiveNaiveMatrixMultiply(C10,A10,B,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
        } else {
            FastNaiveMatrixMultiply(C,A,B,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
            FastNaiveMatrixMultiply(C01,A,B01,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
            FastNaiveMatrixMultiply(C11,A10,B01,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
            FastNaiveMatrixMultiply(C10,A10,B,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
        }
        FastAdditiveNaiveMatrixMultiply(C,A01,B10,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
        FastAdditiveNaiveMatrixMultiply(C01,A01,B11,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
        FastAdditiveNaiveMatrixMultiply(C11,A11,B11,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
        FastAdditiveNaiveMatrixMultiply(C10,A11,B10,QuadrantSize,RowWidthC,RowWidthA,RowWidthB);
    }
    return ;
}
THREAD(OptimizedStrassenMultiply) {
    unsigned int QuadrantSize;
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
    PTR RowIncrementC;
    char *Heap;
    void *StartHeap;
    char *_tmp;
    OptimizedStrassenMultiply_closure *largs = (OptimizedStrassenMultiply_closure*)(args.get());
    QuadrantSize = (largs->MatrixSize >> 1);
    QuadrantSizeInBytes = (((sizeof(REAL) * QuadrantSize) * QuadrantSize) + 32);
    TempMatrixOffset = 0;
    MatrixOffsetA = 0;
    MatrixOffsetB = 0;
    RowIncrementA = ((largs->RowWidthA - QuadrantSize) << 3);
    RowIncrementB = ((largs->RowWidthB - QuadrantSize) << 3);
    RowIncrementC = ((largs->RowWidthC - QuadrantSize) << 3);
    if ((largs->MatrixSize <= 64)) {
        MultiplyByDivideAndConquer(largs->C,largs->A,largs->B,largs->MatrixSize,largs->RowWidthC,largs->RowWidthA,largs->RowWidthB,0);
        SEND_ARGUMENT(largs->k, 0);
    } else {
        A12 = (largs->A + QuadrantSize);
        B12 = (largs->B + QuadrantSize);
        C12 = (largs->C + QuadrantSize);
        A21 = (largs->A + (largs->RowWidthA * QuadrantSize));
        B21 = (largs->B + (largs->RowWidthB * QuadrantSize));
        C21 = (largs->C + (largs->RowWidthC * QuadrantSize));
        A22 = (A21 + QuadrantSize);
        B22 = (B21 + QuadrantSize);
        C22 = (C21 + QuadrantSize);
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
        for (Row = 0;(Row < QuadrantSize);(Row++)) {
            for (Column = 0;(Column < QuadrantSize);(Column++)) {
                *(((REAL *) (((PTR) S1) + TempMatrixOffset))) = (*(((REAL *) (((PTR) A21) + MatrixOffsetA))) + *(((REAL *) (((PTR) A22) + MatrixOffsetA))));
                *(((REAL *) (((PTR) S2) + TempMatrixOffset))) = (*(((REAL *) (((PTR) S1) + TempMatrixOffset))) - *(((REAL *) (((PTR) largs->A) + MatrixOffsetA))));
                *(((REAL *) (((PTR) S4) + TempMatrixOffset))) = (*(((REAL *) (((PTR) A12) + MatrixOffsetA))) - *(((REAL *) (((PTR) S2) + TempMatrixOffset))));
                *(((REAL *) (((PTR) S5) + TempMatrixOffset))) = (*(((REAL *) (((PTR) B12) + MatrixOffsetB))) - *(((REAL *) (((PTR) largs->B) + MatrixOffsetB))));
                *(((REAL *) (((PTR) S6) + TempMatrixOffset))) = (*(((REAL *) (((PTR) B22) + MatrixOffsetB))) - *(((REAL *) (((PTR) S5) + TempMatrixOffset))));
                *(((REAL *) (((PTR) S8) + TempMatrixOffset))) = (*(((REAL *) (((PTR) S6) + TempMatrixOffset))) - *(((REAL *) (((PTR) B21) + MatrixOffsetB))));
                *(((REAL *) (((PTR) S3) + TempMatrixOffset))) = (*(((REAL *) (((PTR) largs->A) + MatrixOffsetA))) - *(((REAL *) (((PTR) A21) + MatrixOffsetA))));
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
        sp0c.C = M2;
        sp0c.A = largs->A;
        sp0c.B = largs->B;
        sp0c.MatrixSize = QuadrantSize;
        sp0c.RowWidthC = QuadrantSize;
        sp0c.RowWidthA = largs->RowWidthA;
        sp0c.RowWidthB = largs->RowWidthB;
        spawn<OptimizedStrassenMultiply_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp1k);
        OptimizedStrassenMultiply_closure sp1c(sp1k);
        sp1c.C = M5;
        sp1c.A = S1;
        sp1c.B = S5;
        sp1c.MatrixSize = QuadrantSize;
        sp1c.RowWidthC = QuadrantSize;
        sp1c.RowWidthA = QuadrantSize;
        sp1c.RowWidthB = QuadrantSize;
        spawn<OptimizedStrassenMultiply_closure> sp1(sp1c);

        cont sp2k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp2k);
        OptimizedStrassenMultiply_closure sp2c(sp2k);
        sp2c.C = T1sMULT;
        sp2c.A = S2;
        sp2c.B = S6;
        sp2c.MatrixSize = QuadrantSize;
        sp2c.RowWidthC = QuadrantSize;
        sp2c.RowWidthA = QuadrantSize;
        sp2c.RowWidthB = QuadrantSize;
        spawn<OptimizedStrassenMultiply_closure> sp2(sp2c);

        cont sp3k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp3k);
        OptimizedStrassenMultiply_closure sp3c(sp3k);
        sp3c.C = C22;
        sp3c.A = S3;
        sp3c.B = S7;
        sp3c.MatrixSize = QuadrantSize;
        sp3c.RowWidthC = largs->RowWidthC;
        sp3c.RowWidthA = QuadrantSize;
        sp3c.RowWidthB = QuadrantSize;
        spawn<OptimizedStrassenMultiply_closure> sp3(sp3c);

        cont sp4k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp4k);
        OptimizedStrassenMultiply_closure sp4c(sp4k);
        sp4c.C = largs->C;
        sp4c.A = A12;
        sp4c.B = B21;
        sp4c.MatrixSize = QuadrantSize;
        sp4c.RowWidthC = largs->RowWidthC;
        sp4c.RowWidthA = largs->RowWidthA;
        sp4c.RowWidthB = largs->RowWidthB;
        spawn<OptimizedStrassenMultiply_closure> sp4(sp4c);

        cont sp5k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp5k);
        OptimizedStrassenMultiply_closure sp5c(sp5k);
        sp5c.C = C12;
        sp5c.A = S4;
        sp5c.B = B22;
        sp5c.MatrixSize = QuadrantSize;
        sp5c.RowWidthC = largs->RowWidthC;
        sp5c.RowWidthA = QuadrantSize;
        sp5c.RowWidthB = largs->RowWidthB;
        spawn<OptimizedStrassenMultiply_closure> sp5(sp5c);

        cont sp6k;
        SN_BIND_VOID(SN_OptimizedStrassenMultiply_cont0, &sp6k);
        OptimizedStrassenMultiply_closure sp6c(sp6k);
        sp6c.C = C21;
        sp6c.A = A22;
        sp6c.B = S8;
        sp6c.MatrixSize = QuadrantSize;
        sp6c.RowWidthC = largs->RowWidthC;
        sp6c.RowWidthA = largs->RowWidthA;
        sp6c.RowWidthB = QuadrantSize;
        spawn<OptimizedStrassenMultiply_closure> sp6(sp6c);

        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->StartHeap = StartHeap;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->RowIncrementC = RowIncrementC;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->T1sMULT = T1sMULT;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->M2 = M2;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->C22 = C22;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->C12 = C12;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->M5 = M5;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->C21 = C21;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->QuadrantSize = QuadrantSize;
        ((OptimizedStrassenMultiply_cont0_closure*)SN_OptimizedStrassenMultiply_cont0.cls.get())->C = largs->C;
        // Original sync was here
    }
}
int compare_vec(int n, REAL *V1, REAL *V2) {
    int i;
    REAL c;
    REAL sum;
    sum = 0.;
    for (i = 0;(i < n);(++i)) {
        c = (V1[i] - V2[i]);
        if ((c < 0.)) {
            c = (-c);
        }
        sum = (sum + c);
        if ((c > 9.9999999999999995E-7)) {
            return (-1);
        } else {
        }
    }
    printf("Sum of errors: %g\n",sum);
    return 0;
}
REAL * alloc_vec(int n) {
    return ((REAL *) malloc((n * sizeof(REAL))));
}
void free_vec(REAL *V) {
    free(V);
}
void init_matrix(int n, REAL *A, int an) {
    int i;
    int j;
    for (i = 0;(i < n);(++i)) {
        for (j = 0;(j < n);(++j)) {
            A[((i * an) + j)] = (((double) cilk_rand()) / ((double) 2147483647));
        }
    }
}
int compare_matrix(int n, REAL *A, int an, REAL *B, int bn) {
    int i;
    int j;
    REAL c;
    for (i = 0;(i < n);(++i)) {
        for (j = 0;(j < n);(++j)) {
            c = (A[((i * an) + j)] - B[((i * bn) + j)]);
            if ((c < 0.)) {
                c = (-c);
            }
            c = (c / A[((i * an) + j)]);
            if ((c > 9.9999999999999995E-7)) {
                return (-1);
            } else {
            }
        }
    }
    return 0;
}
REAL * alloc_matrix(int n) {
    return ((REAL *) malloc(((n * n) * sizeof(REAL))));
}
void free_matrix(REAL *A) {
    free(A);
}
int usage() {
    fprintf(stderr,"\nUsage: strassen [<cilk-options>] [-n #] [-c] [-rc]\n\nMultiplies two randomly generated n x n matrices. To check for\ncorrectness use -c using iterative matrix multiply or use -rc \nusing randomized algorithm due to Freivalds.\n\n");
    return 1;
}
int main(int argc, char **argv) {
    REAL *A;
    REAL *B;
    REAL *C;
    int verify;
    int rand_check;
    int benchmark;
    int help;
    int n;
    struct timeval t1;
    n = 512;
    verify = 0;
    rand_check = 0;
    get_options(argc,argv,specifiers,opt_types,&(n),&(verify),&(rand_check),&(benchmark),&(help));
    if (help) {
        return usage();
    } else {
        if (benchmark) {
            switch (benchmark) {
  case 1:
    n = 512;
    break;
  case 2:
    n = 2048;
    break;
  case 3:
    n = 4096;
    break;
}
;
        }
        if ((((n & (n - 1)) != 0) || ((n % 16) != 0))) {
            printf("%d: matrix size must be a power of 2 and a multiple of %d\n",n,16);
            return 1;
        } else {
            A = alloc_matrix(n);
            B = alloc_matrix(n);
            C = alloc_matrix(n);
            init_matrix(n,A,n);
            init_matrix(n,B,n);
            gettimeofday(&(t1),0);
            main_cont0_closure SN_main_cont0c(CONT_DUMMY);
            spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
            cont sp0k;
            SN_BIND_VOID(SN_main_cont0, &sp0k);
            OptimizedStrassenMultiply_closure sp0c(sp0k);
            sp0c.C = C;
            sp0c.A = A;
            sp0c.B = B;
            sp0c.MatrixSize = n;
            sp0c.RowWidthC = n;
            sp0c.RowWidthA = n;
            sp0c.RowWidthB = n;
            spawn<OptimizedStrassenMultiply_closure> sp0(sp0c);

            ((main_cont0_closure*)SN_main_cont0.cls.get())->t1 = t1;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->n = n;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->rand_check = rand_check;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->verify = verify;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->C = C;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->B = B;
            ((main_cont0_closure*)SN_main_cont0.cls.get())->A = A;
            // Original sync was here
        }
    }
}
THREAD(OptimizedStrassenMultiply_cont0) {
    OptimizedStrassenMultiply_cont0_closure *largs = (OptimizedStrassenMultiply_cont0_closure*)(args.get());
    OptimizedStrassenMultiply_cont1_closure SN_OptimizedStrassenMultiply_cont1c(largs->k);
    spawn_next<OptimizedStrassenMultiply_cont1_closure> SN_OptimizedStrassenMultiply_cont1(SN_OptimizedStrassenMultiply_cont1c);
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->StartHeap = largs->StartHeap;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->RowIncrementC = largs->RowIncrementC;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->T1sMULT = largs->T1sMULT;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->M5 = largs->M5;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->M2 = largs->M2;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->C22 = largs->C22;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->C21 = largs->C21;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->C12 = largs->C12;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->QuadrantSize = largs->QuadrantSize;
    ((OptimizedStrassenMultiply_cont1_closure*)SN_OptimizedStrassenMultiply_cont1.cls.get())->C = largs->C;
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
    for (Row = 0;(Row < largs->QuadrantSize);(Row++)) {
        for (Column = 0;(Column < largs->QuadrantSize);Column = (Column + 4)) {
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
            (*(largs->C)) += LocalM2_0;
            (*(largs->C + 1)) += LocalM2_1;
            (*(largs->C + 2)) += LocalM2_2;
            (*(largs->C + 3)) += LocalM2_3;
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
            largs->C = (largs->C + 4);
            largs->C12 = (largs->C12 + 4);
            largs->C21 = (largs->C21 + 4);
            largs->C22 = (largs->C22 + 4);
        }
        largs->C = ((REAL *) (((PTR) largs->C) + largs->RowIncrementC));
        largs->C12 = ((REAL *) (((PTR) largs->C12) + largs->RowIncrementC));
        largs->C21 = ((REAL *) (((PTR) largs->C21) + largs->RowIncrementC));
        largs->C22 = ((REAL *) (((PTR) largs->C22) + largs->RowIncrementC));
    }
    free(largs->StartHeap);
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(main_cont0) {
    unsigned long long runtime_ms;
    REAL *R;
    REAL *V1;
    REAL *V2;
    REAL *C2;
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    printf("%f\n",(runtime_ms / 1000.));
    if (largs->rand_check) {
        R = alloc_vec(largs->n);
        V1 = alloc_vec(largs->n);
        V2 = alloc_vec(largs->n);
        mat_vec_mul(largs->n,largs->n,largs->n,largs->B,R,V1,0);
        mat_vec_mul(largs->n,largs->n,largs->n,largs->A,V1,V2,0);
        mat_vec_mul(largs->n,largs->n,largs->n,largs->C,R,V1,0);
        largs->rand_check = compare_vec(largs->n,V1,V2);
        free_vec(R);
        free_vec(V1);
        free_vec(V2);
    } else {
        if (largs->verify) {
            fprintf(stderr,"Checking results ... \n");
            C2 = alloc_matrix(largs->n);
            matrixmul(largs->n,largs->A,largs->n,largs->B,largs->n,C2,largs->n);
            largs->verify = compare_matrix(largs->n,largs->C,largs->n,C2,largs->n);
            free_matrix(C2);
        }
    }
    if ((largs->rand_check || largs->verify)) {
        fprintf(stderr,"WRONG RESULT!\n");
    } else {
        fprintf(stderr,"\nCilk Example: strassen\n");
        fprintf(stderr,"Options: n = %d\n\n",largs->n);
    }
    free_matrix(largs->A);
    free_matrix(largs->B);
    free_matrix(largs->C);
    SEND_ARGUMENT(largs->k, 0);
}
