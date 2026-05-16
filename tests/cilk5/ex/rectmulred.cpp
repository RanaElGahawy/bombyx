#include "cilk_explicit.hh"
/*
 * Program to multiply two rectangualar matrizes A(n,m) * B(m,n), where
 * (n < m) and (n mod 16 = 0) and (m mod n = 0). (Otherwise fill with 0s
 * to fit the shape.)
 *
 * written by Harald Prokop (prokop@mit.edu) Fall 97.
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
#include <sys/time.h>

#include "getoptions.h"

#if CILKSAN
#include "cilksan.h"
#endif

#define BLOCK_EDGE 16
#define BLOCK_SIZE (BLOCK_EDGE * BLOCK_EDGE)

typedef double DTYPE;

typedef DTYPE block[BLOCK_SIZE];
typedef block *pblock;

// apparently register storage specifier is deprecated
#define register

unsigned long long todval(struct timeval *tp);
long long mult_add_block(block *A, block *B, block *R);
long long multiply_block(block *A0, block *B0, block *R0);
void check_block(block *R1, DTYPE v, int *errorf);
THREAD(check_matrix);
long long add_block(block *T, block *R3);
THREAD(add_matrix);
void init_block(block *R5, DTYPE v1);
THREAD(init_matrix);
THREAD(multiply_matrix);
int run(long x3, long y3, long z0, int check);
THREAD(run_afterif0);
THREAD(multiply_matrix_afterif1);
THREAD(multiply_matrix_afterif2);
THREAD(init_matrix_afterif3);
THREAD(add_matrix_afterif4);
THREAD(check_matrix_afterif5);
THREAD(check_matrix_cont0);
THREAD(check_matrix_cont1);
THREAD(check_matrix_cont2);
THREAD(check_matrix_cont3);
THREAD(add_matrix_cont0);
THREAD(add_matrix_cont1);
THREAD(init_matrix_cont0);
THREAD(init_matrix_cont1);
THREAD(multiply_matrix_cont0);
THREAD(multiply_matrix_cont1);
THREAD(multiply_matrix_cont2);
THREAD(multiply_matrix_cont3);
THREAD(run_cont0);
THREAD(run_cont1);
THREAD(run_cont2);
THREAD(run_cont3);
THREAD(multiply_matrix_afterif1_cont0);
THREAD(init_matrix_afterif3_cont0);
THREAD(add_matrix_afterif4_cont0);

CLOSURE_DEF(check_matrix,
    block *R2;
    long x;
    long y;
    long o;
    DTYPE v0;
    int *errorf0;
);
CLOSURE_DEF(add_matrix,
    block *T0;
    long ot;
    block *R4;
    long orr;
    long x0;
    long y0;
);
CLOSURE_DEF(init_matrix,
    block *R6;
    long x1;
    long y1;
    long o0;
    DTYPE v2;
);
CLOSURE_DEF(multiply_matrix,
    block *A1;
    long oa;
    block *B1;
    long ob;
    long x2;
    long y2;
    long z;
    block *R7;
    long orr0;
    int add;
);
CLOSURE_DEF(run_afterif0,
    long x3;
    long y3;
    long z0;
    int check;
    block *A2;
    block *B2;
    block *R8;
    struct timeval t1;
    struct timeval t2;
    unsigned long long runtime_ms;
);
CLOSURE_DEF(multiply_matrix_afterif1,
    block *A1;
    long oa;
    block *B1;
    long ob;
    long x2;
    long y2;
    long z;
    block *R7;
    long orr0;
    int add;
);
CLOSURE_DEF(multiply_matrix_afterif2,
    block *A1;
    long oa;
    block *B1;
    long ob;
    long x2;
    long y2;
    long z;
    block *R7;
    long orr0;
    int add;
);
CLOSURE_DEF(init_matrix_afterif3,
    block *R6;
    long x1;
    long y1;
    long o0;
    DTYPE v2;
);
CLOSURE_DEF(add_matrix_afterif4,
    block *T0;
    long ot;
    block *R4;
    long orr;
    long x0;
    long y0;
);
CLOSURE_DEF(check_matrix_afterif5,
    block *R2;
    long x;
    long y;
    long o;
    DTYPE v0;
    int *errorf0;
    int a;
    int b;
);
CLOSURE_DEF(check_matrix_cont0,
    block *R2;
    long x;
    long y;
    long o;
    DTYPE v0;
    int *errorf0;
    int a;
    int b;
);
CLOSURE_DEF(check_matrix_cont1,
    block *R2;
    long x;
    long y;
    long o;
    DTYPE v0;
    int *errorf0;
    int a;
    int b;
);
CLOSURE_DEF(check_matrix_cont2,
    block *R2;
    long x;
    long y;
    long o;
    DTYPE v0;
    int *errorf0;
    int a;
    int b;
);
CLOSURE_DEF(check_matrix_cont3,
    block *R2;
    long x;
    long y;
    long o;
    DTYPE v0;
    int *errorf0;
    int a;
    int b;
);
CLOSURE_DEF(add_matrix_cont0,
    block *T0;
    long ot;
    block *R4;
    long orr;
    long x0;
    long y0;
);
CLOSURE_DEF(add_matrix_cont1,
    block *T0;
    long ot;
    block *R4;
    long orr;
    long x0;
    long y0;
);
CLOSURE_DEF(init_matrix_cont0,
    block *R6;
    long x1;
    long y1;
    long o0;
    DTYPE v2;
);
CLOSURE_DEF(init_matrix_cont1,
    block *R6;
    long x1;
    long y1;
    long o0;
    DTYPE v2;
);
CLOSURE_DEF(multiply_matrix_cont0,
    block *A1;
    long oa;
    block *B1;
    long ob;
    long x2;
    long y2;
    long z;
    block *R7;
    long orr0;
    int add;
);
CLOSURE_DEF(multiply_matrix_cont1,
    block *A1;
    long oa;
    block *B1;
    long ob;
    long x2;
    long y2;
    long z;
    block *R7;
    long orr0;
    int add;
);
CLOSURE_DEF(multiply_matrix_cont2,
    block *A1;
    long oa;
    block *B1;
    long ob;
    long x2;
    long y2;
    long z;
    block *R7;
    long orr0;
    int add;
);
CLOSURE_DEF(multiply_matrix_cont3,
    block *A1;
    long oa;
    block *B1;
    long ob;
    long x2;
    long y2;
    long z;
    block *R7;
    long orr0;
    int add;
);
CLOSURE_DEF(run_cont0,
    long x3;
    long y3;
    long z0;
    int check;
    block *A2;
    block *B2;
    block *R8;
    struct timeval t1;
    struct timeval t2;
);
CLOSURE_DEF(run_cont1,
    long x3;
    long y3;
    long z0;
    int check;
    block *A2;
    block *B2;
    block *R8;
    struct timeval t1;
    struct timeval t2;
);
CLOSURE_DEF(run_cont2,
    long x3;
    long y3;
    long z0;
    int check;
    block *A2;
    block *B2;
    block *R8;
    struct timeval t1;
    struct timeval t2;
);
CLOSURE_DEF(run_cont3,
    long x3;
    long y3;
    long z0;
    int check;
    block *A2;
    block *B2;
    block *R8;
    struct timeval t1;
    struct timeval t2;
    unsigned long long runtime_ms;
);
CLOSURE_DEF(multiply_matrix_afterif1_cont0,
);
CLOSURE_DEF(init_matrix_afterif3_cont0,
);
CLOSURE_DEF(add_matrix_afterif4_cont0,
);


/*
double min;
double max;
int count;
double sum;
*/

/* compute R = R+AB, where R,A,B are BLOCK_EDGE x BLOCK_EDGE matricies
 */


/* compute R = AB, where R,A,B are BLOCK_EDGE x BLOCK_EDGE matricies
 */


/* Checks if each A[i,j] of a martix A of size nb x nb blocks has value v
 */


int compare_block(block *R, block *B) {

  int i;
  int error = 0;

  // fprintf(stderr, "R: %lx.\n", R);
  for (i = 0; i < BLOCK_SIZE; i++)
    if (((DTYPE *)R)[i] != ((DTYPE *)B)[i]) {
      if (i == 0)
        fprintf(stderr, "(R[%d]) %lf != %lf (B[%d]).\n", i, ((DTYPE *)R)[i],
                ((DTYPE *)B)[i], i);
      // return 1;
      error++;
    }

  return error;
}





/*
 * Add matrix T into matrix R, where T and R are bl blocks in size
 */










int usage(void) {
  fprintf(stderr, "Program to multiply two rectangualar matrizes "
                  "A(n,m) * B(m,n), where \n");
  fprintf(stderr, "(n < m) and (n mod 16 = 0) and (m mod n = 0). "
                  "(Otherwise fill with 0s \n to fit the shape.)\n");
  fprintf(stderr, "Usage: rectmul [<cilk-options>] [<options>]\n\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -c   : Check result.\n");
  fprintf(stderr, "-benchmark short / medium / long.\n");
  fprintf(stderr, "Default benchmark size: medium (512 * 512).\n\n");

  return 1;
}

const char *specifiers[] = {"-x", "-y", "-z", "-c", "-benchmark", "-h", 0};
int opt_types[] = {INTARG, INTARG, INTARG, BOOLARG, BENCHMARK, BOOLARG, 0};

int main(int argc, char *argv[]) {

  int x, y, z, benchmark, help, t, check;

  /* standard benchmark options */
  x = 128;
  y = 128;
  z = 128;
  check = 0;

  get_options(argc, argv, specifiers, opt_types, &x, &y, &z, &check, &benchmark,
              &help);

  if (help)
    return usage();

  if (benchmark) {
    switch (benchmark) {
    case 1: /* short benchmark options -- a little work*/
      x = 512;
      y = 512;
      z = 512;
      break;
    case 2: /* standard benchmark options*/
      x = 2048;
      y = 2048;
      z = 2048;
      break;
    case 3: /* long benchmark options -- a lot of work*/
      x = 4096;
      y = 4096;
      z = 4096;
      break;
    }
  }

  x = x / BLOCK_EDGE;
  y = y / BLOCK_EDGE;
  z = z / BLOCK_EDGE;

  if (x < 1)
    x = 1;
  if (y < 1)
    y = 1;
  if (z < 1)
    z = 1;

  t = run(x, y, z, check);

  return t;
}

unsigned long long todval(struct timeval *tp) {
    return (((tp->tv_sec * 1000) * 1000) + tp->tv_usec);
}
long long mult_add_block(block *A, block *B, block *R) {
    int i;
    int j;
    long long flops;
    DTYPE *bp;
    DTYPE *ap;
    DTYPE *rp;
    DTYPE s0_0;
    DTYPE s0_1;
    DTYPE s1_0;
    DTYPE s1_1;
    flops = 0LL;
    for (j = 0;(j < 16);j = (j + 2)) {
        bp = &(((DTYPE *) B)[j]);
        for (i = 0;(i < 16);i = (i + 2)) {
            ap = &(((DTYPE *) A)[(i * 16)]);
            rp = &(((DTYPE *) R)[(j + (i * 16))]);
            s0_0 = rp[0];
            s0_1 = rp[1];
            s1_0 = rp[16];
            s1_1 = rp[17];
            s0_0 = (s0_0 + (ap[0] * bp[0]));
            s0_1 = (s0_1 + (ap[0] * bp[1]));
            s1_0 = (s1_0 + (ap[16] * bp[0]));
            s1_1 = (s1_1 + (ap[16] * bp[1]));
            s0_0 = (s0_0 + (ap[1] * bp[16]));
            s0_1 = (s0_1 + (ap[1] * bp[17]));
            s1_0 = (s1_0 + (ap[17] * bp[16]));
            s1_1 = (s1_1 + (ap[17] * bp[17]));
            s0_0 = (s0_0 + (ap[2] * bp[32]));
            s0_1 = (s0_1 + (ap[2] * bp[33]));
            s1_0 = (s1_0 + (ap[18] * bp[32]));
            s1_1 = (s1_1 + (ap[18] * bp[33]));
            s0_0 = (s0_0 + (ap[3] * bp[48]));
            s0_1 = (s0_1 + (ap[3] * bp[49]));
            s1_0 = (s1_0 + (ap[19] * bp[48]));
            s1_1 = (s1_1 + (ap[19] * bp[49]));
            s0_0 = (s0_0 + (ap[4] * bp[64]));
            s0_1 = (s0_1 + (ap[4] * bp[65]));
            s1_0 = (s1_0 + (ap[20] * bp[64]));
            s1_1 = (s1_1 + (ap[20] * bp[65]));
            s0_0 = (s0_0 + (ap[5] * bp[80]));
            s0_1 = (s0_1 + (ap[5] * bp[81]));
            s1_0 = (s1_0 + (ap[21] * bp[80]));
            s1_1 = (s1_1 + (ap[21] * bp[81]));
            s0_0 = (s0_0 + (ap[6] * bp[96]));
            s0_1 = (s0_1 + (ap[6] * bp[97]));
            s1_0 = (s1_0 + (ap[22] * bp[96]));
            s1_1 = (s1_1 + (ap[22] * bp[97]));
            s0_0 = (s0_0 + (ap[7] * bp[112]));
            s0_1 = (s0_1 + (ap[7] * bp[113]));
            s1_0 = (s1_0 + (ap[23] * bp[112]));
            s1_1 = (s1_1 + (ap[23] * bp[113]));
            s0_0 = (s0_0 + (ap[8] * bp[128]));
            s0_1 = (s0_1 + (ap[8] * bp[129]));
            s1_0 = (s1_0 + (ap[24] * bp[128]));
            s1_1 = (s1_1 + (ap[24] * bp[129]));
            s0_0 = (s0_0 + (ap[9] * bp[144]));
            s0_1 = (s0_1 + (ap[9] * bp[145]));
            s1_0 = (s1_0 + (ap[25] * bp[144]));
            s1_1 = (s1_1 + (ap[25] * bp[145]));
            s0_0 = (s0_0 + (ap[10] * bp[160]));
            s0_1 = (s0_1 + (ap[10] * bp[161]));
            s1_0 = (s1_0 + (ap[26] * bp[160]));
            s1_1 = (s1_1 + (ap[26] * bp[161]));
            s0_0 = (s0_0 + (ap[11] * bp[176]));
            s0_1 = (s0_1 + (ap[11] * bp[177]));
            s1_0 = (s1_0 + (ap[27] * bp[176]));
            s1_1 = (s1_1 + (ap[27] * bp[177]));
            s0_0 = (s0_0 + (ap[12] * bp[192]));
            s0_1 = (s0_1 + (ap[12] * bp[193]));
            s1_0 = (s1_0 + (ap[28] * bp[192]));
            s1_1 = (s1_1 + (ap[28] * bp[193]));
            s0_0 = (s0_0 + (ap[13] * bp[208]));
            s0_1 = (s0_1 + (ap[13] * bp[209]));
            s1_0 = (s1_0 + (ap[29] * bp[208]));
            s1_1 = (s1_1 + (ap[29] * bp[209]));
            s0_0 = (s0_0 + (ap[14] * bp[224]));
            s0_1 = (s0_1 + (ap[14] * bp[225]));
            s1_0 = (s1_0 + (ap[30] * bp[224]));
            s1_1 = (s1_1 + (ap[30] * bp[225]));
            s0_0 = (s0_0 + (ap[15] * bp[240]));
            s0_1 = (s0_1 + (ap[15] * bp[241]));
            s1_0 = (s1_0 + (ap[31] * bp[240]));
            s1_1 = (s1_1 + (ap[31] * bp[241]));
            rp[0] = s0_0;
            rp[1] = s0_1;
            rp[16] = s1_0;
            rp[17] = s1_1;
            flops = (flops + 128);
        }
    }
    return flops;
}
long long multiply_block(block *A0, block *B0, block *R0) {
    int i0;
    int j0;
    long long flops0;
    DTYPE *bp0;
    DTYPE *ap0;
    DTYPE *rp0;
    DTYPE s0_00;
    DTYPE s0_10;
    DTYPE s1_00;
    DTYPE s1_10;
    flops0 = 0LL;
    for (j0 = 0;(j0 < 16);j0 = (j0 + 2)) {
        bp0 = &(((DTYPE *) B0)[j0]);
        for (i0 = 0;(i0 < 16);i0 = (i0 + 2)) {
            ap0 = &(((DTYPE *) A0)[(i0 * 16)]);
            rp0 = &(((DTYPE *) R0)[(j0 + (i0 * 16))]);
            s0_00 = (ap0[0] * bp0[0]);
            s0_10 = (ap0[0] * bp0[1]);
            s1_00 = (ap0[16] * bp0[0]);
            s1_10 = (ap0[16] * bp0[1]);
            s0_00 = (s0_00 + (ap0[1] * bp0[16]));
            s0_10 = (s0_10 + (ap0[1] * bp0[17]));
            s1_00 = (s1_00 + (ap0[17] * bp0[16]));
            s1_10 = (s1_10 + (ap0[17] * bp0[17]));
            s0_00 = (s0_00 + (ap0[2] * bp0[32]));
            s0_10 = (s0_10 + (ap0[2] * bp0[33]));
            s1_00 = (s1_00 + (ap0[18] * bp0[32]));
            s1_10 = (s1_10 + (ap0[18] * bp0[33]));
            s0_00 = (s0_00 + (ap0[3] * bp0[48]));
            s0_10 = (s0_10 + (ap0[3] * bp0[49]));
            s1_00 = (s1_00 + (ap0[19] * bp0[48]));
            s1_10 = (s1_10 + (ap0[19] * bp0[49]));
            s0_00 = (s0_00 + (ap0[4] * bp0[64]));
            s0_10 = (s0_10 + (ap0[4] * bp0[65]));
            s1_00 = (s1_00 + (ap0[20] * bp0[64]));
            s1_10 = (s1_10 + (ap0[20] * bp0[65]));
            s0_00 = (s0_00 + (ap0[5] * bp0[80]));
            s0_10 = (s0_10 + (ap0[5] * bp0[81]));
            s1_00 = (s1_00 + (ap0[21] * bp0[80]));
            s1_10 = (s1_10 + (ap0[21] * bp0[81]));
            s0_00 = (s0_00 + (ap0[6] * bp0[96]));
            s0_10 = (s0_10 + (ap0[6] * bp0[97]));
            s1_00 = (s1_00 + (ap0[22] * bp0[96]));
            s1_10 = (s1_10 + (ap0[22] * bp0[97]));
            s0_00 = (s0_00 + (ap0[7] * bp0[112]));
            s0_10 = (s0_10 + (ap0[7] * bp0[113]));
            s1_00 = (s1_00 + (ap0[23] * bp0[112]));
            s1_10 = (s1_10 + (ap0[23] * bp0[113]));
            s0_00 = (s0_00 + (ap0[8] * bp0[128]));
            s0_10 = (s0_10 + (ap0[8] * bp0[129]));
            s1_00 = (s1_00 + (ap0[24] * bp0[128]));
            s1_10 = (s1_10 + (ap0[24] * bp0[129]));
            s0_00 = (s0_00 + (ap0[9] * bp0[144]));
            s0_10 = (s0_10 + (ap0[9] * bp0[145]));
            s1_00 = (s1_00 + (ap0[25] * bp0[144]));
            s1_10 = (s1_10 + (ap0[25] * bp0[145]));
            s0_00 = (s0_00 + (ap0[10] * bp0[160]));
            s0_10 = (s0_10 + (ap0[10] * bp0[161]));
            s1_00 = (s1_00 + (ap0[26] * bp0[160]));
            s1_10 = (s1_10 + (ap0[26] * bp0[161]));
            s0_00 = (s0_00 + (ap0[11] * bp0[176]));
            s0_10 = (s0_10 + (ap0[11] * bp0[177]));
            s1_00 = (s1_00 + (ap0[27] * bp0[176]));
            s1_10 = (s1_10 + (ap0[27] * bp0[177]));
            s0_00 = (s0_00 + (ap0[12] * bp0[192]));
            s0_10 = (s0_10 + (ap0[12] * bp0[193]));
            s1_00 = (s1_00 + (ap0[28] * bp0[192]));
            s1_10 = (s1_10 + (ap0[28] * bp0[193]));
            s0_00 = (s0_00 + (ap0[13] * bp0[208]));
            s0_10 = (s0_10 + (ap0[13] * bp0[209]));
            s1_00 = (s1_00 + (ap0[29] * bp0[208]));
            s1_10 = (s1_10 + (ap0[29] * bp0[209]));
            s0_00 = (s0_00 + (ap0[14] * bp0[224]));
            s0_10 = (s0_10 + (ap0[14] * bp0[225]));
            s1_00 = (s1_00 + (ap0[30] * bp0[224]));
            s1_10 = (s1_10 + (ap0[30] * bp0[225]));
            s0_00 = (s0_00 + (ap0[15] * bp0[240]));
            s0_10 = (s0_10 + (ap0[15] * bp0[241]));
            s1_00 = (s1_00 + (ap0[31] * bp0[240]));
            s1_10 = (s1_10 + (ap0[31] * bp0[241]));
            rp0[0] = s0_00;
            rp0[1] = s0_10;
            rp0[16] = s1_00;
            rp0[17] = s1_10;
            flops0 = (flops0 + 124);
        }
    }
    return flops0;
}
void check_block(block *R1, DTYPE v, int *errorf) {
    int i1;
    int error;
    error = 0;
    for (i1 = 0;(i1 < (16 * 16));(i1++)) {
        if ((((DTYPE *) R1)[i1] != v)) {
            if ((i1 == 0)) {
                fprintf(__stderrp,"R[%d]: %lf != %lf.\n",i1,((DTYPE *) R1)[i1],v);
            }
            (error++);
        }
    }
    *errorf += error;
}
THREAD(check_matrix) {
    int a;
    int b;
    check_matrix_closure *largs = (check_matrix_closure*)(args.get());
    a = 0;
    b = 0;
    if (((largs->x * largs->y) == 1)) {
        check_block(largs->R2,largs->v0,largs->errorf0);
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if ((largs->x > largs->y)) {
            check_matrix_cont2_closure SN_check_matrix_cont2c(largs->k);
            spawn_next<check_matrix_cont2_closure> SN_check_matrix_cont2(SN_check_matrix_cont2c);
            cont sp0k;
            SN_BIND_VOID(SN_check_matrix_cont2, &sp0k);
            check_matrix_closure sp0c(sp0k);
            sp0c.R2 = largs->R2;
            sp0c.x = (largs->x / 2);
            sp0c.y = largs->y;
            sp0c.o = largs->o;
            sp0c.v0 = largs->v0;
            sp0c.errorf0 = largs->errorf0;
            spawn<check_matrix_closure> sp0(sp0c);

            cont sp1k;
            SN_BIND_VOID(SN_check_matrix_cont2, &sp1k);
            check_matrix_closure sp1c(sp1k);
            sp1c.R2 = (largs->R2 + ((largs->x / 2) * largs->o));
            sp1c.x = ((largs->x + 1) / 2);
            sp1c.y = largs->y;
            sp1c.o = largs->o;
            sp1c.v0 = largs->v0;
            sp1c.errorf0 = largs->errorf0;
            spawn<check_matrix_closure> sp1(sp1c);

            ((check_matrix_cont2_closure*)SN_check_matrix_cont2.cls.get())->b = b;
            ((check_matrix_cont2_closure*)SN_check_matrix_cont2.cls.get())->a = a;
            ((check_matrix_cont2_closure*)SN_check_matrix_cont2.cls.get())->v0 = largs->v0;
            ((check_matrix_cont2_closure*)SN_check_matrix_cont2.cls.get())->o = largs->o;
            ((check_matrix_cont2_closure*)SN_check_matrix_cont2.cls.get())->y = largs->y;
            ((check_matrix_cont2_closure*)SN_check_matrix_cont2.cls.get())->x = largs->x;
            ((check_matrix_cont2_closure*)SN_check_matrix_cont2.cls.get())->errorf0 = largs->errorf0;
            ((check_matrix_cont2_closure*)SN_check_matrix_cont2.cls.get())->R2 = largs->R2;
            // Original sync was here
        } else {
            check_matrix_cont0_closure SN_check_matrix_cont0c(largs->k);
            spawn_next<check_matrix_cont0_closure> SN_check_matrix_cont0(SN_check_matrix_cont0c);
            cont sp2k;
            SN_BIND_VOID(SN_check_matrix_cont0, &sp2k);
            check_matrix_closure sp2c(sp2k);
            sp2c.R2 = largs->R2;
            sp2c.x = largs->x;
            sp2c.y = (largs->y / 2);
            sp2c.o = largs->o;
            sp2c.v0 = largs->v0;
            sp2c.errorf0 = largs->errorf0;
            spawn<check_matrix_closure> sp2(sp2c);

            cont sp3k;
            SN_BIND_VOID(SN_check_matrix_cont0, &sp3k);
            check_matrix_closure sp3c(sp3k);
            sp3c.R2 = (largs->R2 + (largs->y / 2));
            sp3c.x = largs->x;
            sp3c.y = ((largs->y + 1) / 2);
            sp3c.o = largs->o;
            sp3c.v0 = largs->v0;
            sp3c.errorf0 = largs->errorf0;
            spawn<check_matrix_closure> sp3(sp3c);

            ((check_matrix_cont0_closure*)SN_check_matrix_cont0.cls.get())->b = b;
            ((check_matrix_cont0_closure*)SN_check_matrix_cont0.cls.get())->a = a;
            ((check_matrix_cont0_closure*)SN_check_matrix_cont0.cls.get())->v0 = largs->v0;
            ((check_matrix_cont0_closure*)SN_check_matrix_cont0.cls.get())->o = largs->o;
            ((check_matrix_cont0_closure*)SN_check_matrix_cont0.cls.get())->y = largs->y;
            ((check_matrix_cont0_closure*)SN_check_matrix_cont0.cls.get())->x = largs->x;
            ((check_matrix_cont0_closure*)SN_check_matrix_cont0.cls.get())->errorf0 = largs->errorf0;
            ((check_matrix_cont0_closure*)SN_check_matrix_cont0.cls.get())->R2 = largs->R2;
            // Original sync was here
        }
    }
    return;
}
long long add_block(block *T, block *R3) {
    long i2;
    for (i2 = 0;(i2 < (16 * 16));i2 = (i2 + 4)) {
        ((DTYPE *)R3)[i2] += ((DTYPE *)T)[i2];
        ((DTYPE *)R3)[i2 + 1] += ((DTYPE *)T)[i2 + 1];
        ((DTYPE *)R3)[i2 + 2] += ((DTYPE *)T)[i2 + 2];
        ((DTYPE *)R3)[i2 + 3] += ((DTYPE *)T)[i2 + 3];
    }
    return (16 * 16);
}
THREAD(add_matrix) {
    add_matrix_closure *largs = (add_matrix_closure*)(args.get());
    if (((largs->x0 + largs->y0) == 2)) {
        add_block(largs->T0,largs->R4);
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if ((largs->x0 > largs->y0)) {
            add_matrix_cont1_closure SN_add_matrix_cont1c(largs->k);
            spawn_next<add_matrix_cont1_closure> SN_add_matrix_cont1(SN_add_matrix_cont1c);
            cont sp0k;
            SN_BIND_VOID(SN_add_matrix_cont1, &sp0k);
            add_matrix_closure sp0c(sp0k);
            sp0c.T0 = largs->T0;
            sp0c.ot = largs->ot;
            sp0c.R4 = largs->R4;
            sp0c.orr = largs->orr;
            sp0c.x0 = (largs->x0 / 2);
            sp0c.y0 = largs->y0;
            spawn<add_matrix_closure> sp0(sp0c);

            cont sp1k;
            SN_BIND_VOID(SN_add_matrix_cont1, &sp1k);
            add_matrix_closure sp1c(sp1k);
            sp1c.T0 = (largs->T0 + ((largs->x0 / 2) * largs->ot));
            sp1c.ot = largs->ot;
            sp1c.R4 = (largs->R4 + ((largs->x0 / 2) * largs->orr));
            sp1c.orr = largs->orr;
            sp1c.x0 = ((largs->x0 + 1) / 2);
            sp1c.y0 = largs->y0;
            spawn<add_matrix_closure> sp1(sp1c);

            ((add_matrix_cont1_closure*)SN_add_matrix_cont1.cls.get())->y0 = largs->y0;
            ((add_matrix_cont1_closure*)SN_add_matrix_cont1.cls.get())->R4 = largs->R4;
            ((add_matrix_cont1_closure*)SN_add_matrix_cont1.cls.get())->orr = largs->orr;
            ((add_matrix_cont1_closure*)SN_add_matrix_cont1.cls.get())->x0 = largs->x0;
            ((add_matrix_cont1_closure*)SN_add_matrix_cont1.cls.get())->ot = largs->ot;
            ((add_matrix_cont1_closure*)SN_add_matrix_cont1.cls.get())->T0 = largs->T0;
            // Original sync was here
        } else {
            add_matrix_cont0_closure SN_add_matrix_cont0c(largs->k);
            spawn_next<add_matrix_cont0_closure> SN_add_matrix_cont0(SN_add_matrix_cont0c);
            cont sp2k;
            SN_BIND_VOID(SN_add_matrix_cont0, &sp2k);
            add_matrix_closure sp2c(sp2k);
            sp2c.T0 = largs->T0;
            sp2c.ot = largs->ot;
            sp2c.R4 = largs->R4;
            sp2c.orr = largs->orr;
            sp2c.x0 = largs->x0;
            sp2c.y0 = (largs->y0 / 2);
            spawn<add_matrix_closure> sp2(sp2c);

            cont sp3k;
            SN_BIND_VOID(SN_add_matrix_cont0, &sp3k);
            add_matrix_closure sp3c(sp3k);
            sp3c.T0 = (largs->T0 + (largs->y0 / 2));
            sp3c.ot = largs->ot;
            sp3c.R4 = (largs->R4 + (largs->y0 / 2));
            sp3c.orr = largs->orr;
            sp3c.x0 = largs->x0;
            sp3c.y0 = ((largs->y0 + 1) / 2);
            spawn<add_matrix_closure> sp3(sp3c);

            ((add_matrix_cont0_closure*)SN_add_matrix_cont0.cls.get())->y0 = largs->y0;
            ((add_matrix_cont0_closure*)SN_add_matrix_cont0.cls.get())->R4 = largs->R4;
            ((add_matrix_cont0_closure*)SN_add_matrix_cont0.cls.get())->orr = largs->orr;
            ((add_matrix_cont0_closure*)SN_add_matrix_cont0.cls.get())->x0 = largs->x0;
            ((add_matrix_cont0_closure*)SN_add_matrix_cont0.cls.get())->ot = largs->ot;
            ((add_matrix_cont0_closure*)SN_add_matrix_cont0.cls.get())->T0 = largs->T0;
            // Original sync was here
        }
    }
    return;
}
void init_block(block *R5, DTYPE v1) {
    int i3;
    for (i3 = 0;(i3 < (16 * 16));(i3++)) {
        ((DTYPE *) R5)[i3] = v1;
    }
}
THREAD(init_matrix) {
    init_matrix_closure *largs = (init_matrix_closure*)(args.get());
    if (((largs->x1 + largs->y1) == 2)) {
        init_block(largs->R6,largs->v2);
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if ((largs->x1 > largs->y1)) {
            init_matrix_cont1_closure SN_init_matrix_cont1c(largs->k);
            spawn_next<init_matrix_cont1_closure> SN_init_matrix_cont1(SN_init_matrix_cont1c);
            cont sp0k;
            SN_BIND_VOID(SN_init_matrix_cont1, &sp0k);
            init_matrix_closure sp0c(sp0k);
            sp0c.R6 = largs->R6;
            sp0c.x1 = (largs->x1 / 2);
            sp0c.y1 = largs->y1;
            sp0c.o0 = largs->o0;
            sp0c.v2 = largs->v2;
            spawn<init_matrix_closure> sp0(sp0c);

            cont sp1k;
            SN_BIND_VOID(SN_init_matrix_cont1, &sp1k);
            init_matrix_closure sp1c(sp1k);
            sp1c.R6 = (largs->R6 + ((largs->x1 / 2) * largs->o0));
            sp1c.x1 = ((largs->x1 + 1) / 2);
            sp1c.y1 = largs->y1;
            sp1c.o0 = largs->o0;
            sp1c.v2 = largs->v2;
            spawn<init_matrix_closure> sp1(sp1c);

            ((init_matrix_cont1_closure*)SN_init_matrix_cont1.cls.get())->v2 = largs->v2;
            ((init_matrix_cont1_closure*)SN_init_matrix_cont1.cls.get())->y1 = largs->y1;
            ((init_matrix_cont1_closure*)SN_init_matrix_cont1.cls.get())->o0 = largs->o0;
            ((init_matrix_cont1_closure*)SN_init_matrix_cont1.cls.get())->x1 = largs->x1;
            ((init_matrix_cont1_closure*)SN_init_matrix_cont1.cls.get())->R6 = largs->R6;
            // Original sync was here
        } else {
            init_matrix_cont0_closure SN_init_matrix_cont0c(largs->k);
            spawn_next<init_matrix_cont0_closure> SN_init_matrix_cont0(SN_init_matrix_cont0c);
            cont sp2k;
            SN_BIND_VOID(SN_init_matrix_cont0, &sp2k);
            init_matrix_closure sp2c(sp2k);
            sp2c.R6 = largs->R6;
            sp2c.x1 = largs->x1;
            sp2c.y1 = (largs->y1 / 2);
            sp2c.o0 = largs->o0;
            sp2c.v2 = largs->v2;
            spawn<init_matrix_closure> sp2(sp2c);

            cont sp3k;
            SN_BIND_VOID(SN_init_matrix_cont0, &sp3k);
            init_matrix_closure sp3c(sp3k);
            sp3c.R6 = (largs->R6 + (largs->y1 / 2));
            sp3c.x1 = largs->x1;
            sp3c.y1 = ((largs->y1 + 1) / 2);
            sp3c.o0 = largs->o0;
            sp3c.v2 = largs->v2;
            spawn<init_matrix_closure> sp3(sp3c);

            ((init_matrix_cont0_closure*)SN_init_matrix_cont0.cls.get())->v2 = largs->v2;
            ((init_matrix_cont0_closure*)SN_init_matrix_cont0.cls.get())->y1 = largs->y1;
            ((init_matrix_cont0_closure*)SN_init_matrix_cont0.cls.get())->o0 = largs->o0;
            ((init_matrix_cont0_closure*)SN_init_matrix_cont0.cls.get())->x1 = largs->x1;
            ((init_matrix_cont0_closure*)SN_init_matrix_cont0.cls.get())->R6 = largs->R6;
            // Original sync was here
        }
    }
    return;
}
THREAD(multiply_matrix) {
    multiply_matrix_closure *largs = (multiply_matrix_closure*)(args.get());
    if ((((largs->x2 + largs->y2) + largs->z) == 3)) {
        if (largs->add) {
            mult_add_block(largs->A1,largs->B1,largs->R7);
        } else {
            multiply_block(largs->A1,largs->B1,largs->R7);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if (((largs->x2 >= largs->y2) && (largs->x2 >= largs->z))) {
            multiply_matrix_cont3_closure SN_multiply_matrix_cont3c(largs->k);
            spawn_next<multiply_matrix_cont3_closure> SN_multiply_matrix_cont3(SN_multiply_matrix_cont3c);
            cont sp0k;
            SN_BIND_VOID(SN_multiply_matrix_cont3, &sp0k);
            multiply_matrix_closure sp0c(sp0k);
            sp0c.A1 = largs->A1;
            sp0c.oa = largs->oa;
            sp0c.B1 = largs->B1;
            sp0c.ob = largs->ob;
            sp0c.x2 = (largs->x2 / 2);
            sp0c.y2 = largs->y2;
            sp0c.z = largs->z;
            sp0c.R7 = largs->R7;
            sp0c.orr0 = largs->orr0;
            sp0c.add = largs->add;
            spawn<multiply_matrix_closure> sp0(sp0c);

            cont sp1k;
            SN_BIND_VOID(SN_multiply_matrix_cont3, &sp1k);
            multiply_matrix_closure sp1c(sp1k);
            sp1c.A1 = (largs->A1 + ((largs->x2 / 2) * largs->oa));
            sp1c.oa = largs->oa;
            sp1c.B1 = largs->B1;
            sp1c.ob = largs->ob;
            sp1c.x2 = ((largs->x2 + 1) / 2);
            sp1c.y2 = largs->y2;
            sp1c.z = largs->z;
            sp1c.R7 = (largs->R7 + ((largs->x2 / 2) * largs->orr0));
            sp1c.orr0 = largs->orr0;
            sp1c.add = largs->add;
            spawn<multiply_matrix_closure> sp1(sp1c);

            ((multiply_matrix_cont3_closure*)SN_multiply_matrix_cont3.cls.get())->orr0 = largs->orr0;
            ((multiply_matrix_cont3_closure*)SN_multiply_matrix_cont3.cls.get())->R7 = largs->R7;
            ((multiply_matrix_cont3_closure*)SN_multiply_matrix_cont3.cls.get())->y2 = largs->y2;
            ((multiply_matrix_cont3_closure*)SN_multiply_matrix_cont3.cls.get())->x2 = largs->x2;
            ((multiply_matrix_cont3_closure*)SN_multiply_matrix_cont3.cls.get())->add = largs->add;
            ((multiply_matrix_cont3_closure*)SN_multiply_matrix_cont3.cls.get())->B1 = largs->B1;
            ((multiply_matrix_cont3_closure*)SN_multiply_matrix_cont3.cls.get())->ob = largs->ob;
            ((multiply_matrix_cont3_closure*)SN_multiply_matrix_cont3.cls.get())->z = largs->z;
            ((multiply_matrix_cont3_closure*)SN_multiply_matrix_cont3.cls.get())->oa = largs->oa;
            ((multiply_matrix_cont3_closure*)SN_multiply_matrix_cont3.cls.get())->A1 = largs->A1;
            // Original sync was here
        } else {
            if (((largs->y2 > largs->x2) && (largs->y2 > largs->z))) {
                multiply_matrix_cont1_closure SN_multiply_matrix_cont1c(largs->k);
                spawn_next<multiply_matrix_cont1_closure> SN_multiply_matrix_cont1(SN_multiply_matrix_cont1c);
                cont sp2k;
                SN_BIND_VOID(SN_multiply_matrix_cont1, &sp2k);
                multiply_matrix_closure sp2c(sp2k);
                sp2c.A1 = (largs->A1 + (largs->y2 / 2));
                sp2c.oa = largs->oa;
                sp2c.B1 = (largs->B1 + ((largs->y2 / 2) * largs->ob));
                sp2c.ob = largs->ob;
                sp2c.x2 = largs->x2;
                sp2c.y2 = ((largs->y2 + 1) / 2);
                sp2c.z = largs->z;
                sp2c.R7 = largs->R7;
                sp2c.orr0 = largs->orr0;
                sp2c.add = largs->add;
                spawn<multiply_matrix_closure> sp2(sp2c);

                ((multiply_matrix_cont1_closure*)SN_multiply_matrix_cont1.cls.get())->orr0 = largs->orr0;
                ((multiply_matrix_cont1_closure*)SN_multiply_matrix_cont1.cls.get())->R7 = largs->R7;
                ((multiply_matrix_cont1_closure*)SN_multiply_matrix_cont1.cls.get())->y2 = largs->y2;
                ((multiply_matrix_cont1_closure*)SN_multiply_matrix_cont1.cls.get())->x2 = largs->x2;
                ((multiply_matrix_cont1_closure*)SN_multiply_matrix_cont1.cls.get())->add = largs->add;
                ((multiply_matrix_cont1_closure*)SN_multiply_matrix_cont1.cls.get())->B1 = largs->B1;
                ((multiply_matrix_cont1_closure*)SN_multiply_matrix_cont1.cls.get())->ob = largs->ob;
                ((multiply_matrix_cont1_closure*)SN_multiply_matrix_cont1.cls.get())->z = largs->z;
                ((multiply_matrix_cont1_closure*)SN_multiply_matrix_cont1.cls.get())->oa = largs->oa;
                ((multiply_matrix_cont1_closure*)SN_multiply_matrix_cont1.cls.get())->A1 = largs->A1;
                // Original sync was here
            } else {
                multiply_matrix_cont0_closure SN_multiply_matrix_cont0c(largs->k);
                spawn_next<multiply_matrix_cont0_closure> SN_multiply_matrix_cont0(SN_multiply_matrix_cont0c);
                cont sp3k;
                SN_BIND_VOID(SN_multiply_matrix_cont0, &sp3k);
                multiply_matrix_closure sp3c(sp3k);
                sp3c.A1 = largs->A1;
                sp3c.oa = largs->oa;
                sp3c.B1 = largs->B1;
                sp3c.ob = largs->ob;
                sp3c.x2 = largs->x2;
                sp3c.y2 = largs->y2;
                sp3c.z = (largs->z / 2);
                sp3c.R7 = largs->R7;
                sp3c.orr0 = largs->orr0;
                sp3c.add = largs->add;
                spawn<multiply_matrix_closure> sp3(sp3c);

                cont sp4k;
                SN_BIND_VOID(SN_multiply_matrix_cont0, &sp4k);
                multiply_matrix_closure sp4c(sp4k);
                sp4c.A1 = largs->A1;
                sp4c.oa = largs->oa;
                sp4c.B1 = (largs->B1 + (largs->z / 2));
                sp4c.ob = largs->ob;
                sp4c.x2 = largs->x2;
                sp4c.y2 = largs->y2;
                sp4c.z = ((largs->z + 1) / 2);
                sp4c.R7 = (largs->R7 + (largs->z / 2));
                sp4c.orr0 = largs->orr0;
                sp4c.add = largs->add;
                spawn<multiply_matrix_closure> sp4(sp4c);

                ((multiply_matrix_cont0_closure*)SN_multiply_matrix_cont0.cls.get())->orr0 = largs->orr0;
                ((multiply_matrix_cont0_closure*)SN_multiply_matrix_cont0.cls.get())->R7 = largs->R7;
                ((multiply_matrix_cont0_closure*)SN_multiply_matrix_cont0.cls.get())->y2 = largs->y2;
                ((multiply_matrix_cont0_closure*)SN_multiply_matrix_cont0.cls.get())->x2 = largs->x2;
                ((multiply_matrix_cont0_closure*)SN_multiply_matrix_cont0.cls.get())->add = largs->add;
                ((multiply_matrix_cont0_closure*)SN_multiply_matrix_cont0.cls.get())->B1 = largs->B1;
                ((multiply_matrix_cont0_closure*)SN_multiply_matrix_cont0.cls.get())->ob = largs->ob;
                ((multiply_matrix_cont0_closure*)SN_multiply_matrix_cont0.cls.get())->z = largs->z;
                ((multiply_matrix_cont0_closure*)SN_multiply_matrix_cont0.cls.get())->oa = largs->oa;
                ((multiply_matrix_cont0_closure*)SN_multiply_matrix_cont0.cls.get())->A1 = largs->A1;
                // Original sync was here
            }
        }
    }
    return;
}
int run(long x3, long y3, long z0, int check) {
    block *A2;
    block *B2;
    block *R8;
    A2 = ((block *) malloc(((x3 * y3) * sizeof(block))));
    B2 = ((block *) malloc(((y3 * z0) * sizeof(block))));
    R8 = ((block *) malloc(((x3 * z0) * sizeof(block))));
    run_cont0_closure SN_run_cont0c(CONT_DUMMY);
    spawn_next<run_cont0_closure> SN_run_cont0(SN_run_cont0c);
    cont sp0k;
    SN_BIND_VOID(SN_run_cont0, &sp0k);
    init_matrix_closure sp0c(sp0k);
    sp0c.R6 = A2;
    sp0c.x1 = x3;
    sp0c.y1 = y3;
    sp0c.o0 = y3;
    sp0c.v2 = 1.;
    spawn<init_matrix_closure> sp0(sp0c);

    cont sp1k;
    SN_BIND_VOID(SN_run_cont0, &sp1k);
    init_matrix_closure sp1c(sp1k);
    sp1c.R6 = B2;
    sp1c.x1 = y3;
    sp1c.y1 = z0;
    sp1c.o0 = z0;
    sp1c.v2 = 1.;
    spawn<init_matrix_closure> sp1(sp1c);

    cont sp2k;
    SN_BIND_VOID(SN_run_cont0, &sp2k);
    init_matrix_closure sp2c(sp2k);
    sp2c.R6 = R8;
    sp2c.x1 = x3;
    sp2c.y1 = z0;
    sp2c.o0 = z0;
    sp2c.v2 = 0.;
    spawn<init_matrix_closure> sp2(sp2c);

    ((run_cont0_closure*)SN_run_cont0.cls.get())->R8 = R8;
    ((run_cont0_closure*)SN_run_cont0.cls.get())->B2 = B2;
    ((run_cont0_closure*)SN_run_cont0.cls.get())->A2 = A2;
    ((run_cont0_closure*)SN_run_cont0.cls.get())->check = check;
    ((run_cont0_closure*)SN_run_cont0.cls.get())->z0 = z0;
    ((run_cont0_closure*)SN_run_cont0.cls.get())->y3 = y3;
    ((run_cont0_closure*)SN_run_cont0.cls.get())->x3 = x3;
    // Original sync was here
    return 0;
}
THREAD(run_afterif0) {
    run_afterif0_closure *largs = (run_afterif0_closure*)(args.get());
    if (largs->check) {
        printf("WRONG RESULT!\n");
        printf("check: %d\n",largs->check);
    } else {
        fprintf(__stderrp,"\nCilk Example: rectmul\n");
        fprintf(__stderrp,"Options: x = %ld\n",(16 * largs->x3));
        fprintf(__stderrp,"         y = %ld\n",(16 * largs->y3));
        fprintf(__stderrp,"         z = %ld\n\n",(16 * largs->z0));
    }
    free(largs->A2);
    free(largs->B2);
    free(largs->R8);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(multiply_matrix_afterif1) {
    multiply_matrix_afterif1_closure *largs = (multiply_matrix_afterif1_closure*)(args.get());
    multiply_matrix_afterif1_cont0_closure SN_multiply_matrix_afterif1_cont0c(largs->k);
    spawn_next<multiply_matrix_afterif1_cont0_closure> SN_multiply_matrix_afterif1_cont0(SN_multiply_matrix_afterif1_cont0c);
    // Original sync was here
    return;
}
THREAD(multiply_matrix_afterif2) {
    multiply_matrix_afterif2_closure *largs = (multiply_matrix_afterif2_closure*)(args.get());
    auto sp0c = std::make_shared<multiply_matrix_afterif1_closure>(largs->k);
    sp0c->A1 = largs->A1;
    sp0c->oa = largs->oa;
    sp0c->B1 = largs->B1;
    sp0c->ob = largs->ob;
    sp0c->x2 = largs->x2;
    sp0c->y2 = largs->y2;
    sp0c->z = largs->z;
    sp0c->R7 = largs->R7;
    sp0c->orr0 = largs->orr0;
    sp0c->add = largs->add;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(init_matrix_afterif3) {
    init_matrix_afterif3_closure *largs = (init_matrix_afterif3_closure*)(args.get());
    init_matrix_afterif3_cont0_closure SN_init_matrix_afterif3_cont0c(largs->k);
    spawn_next<init_matrix_afterif3_cont0_closure> SN_init_matrix_afterif3_cont0(SN_init_matrix_afterif3_cont0c);
    // Original sync was here
    return;
}
THREAD(add_matrix_afterif4) {
    add_matrix_afterif4_closure *largs = (add_matrix_afterif4_closure*)(args.get());
    add_matrix_afterif4_cont0_closure SN_add_matrix_afterif4_cont0c(largs->k);
    spawn_next<add_matrix_afterif4_cont0_closure> SN_add_matrix_afterif4_cont0(SN_add_matrix_afterif4_cont0c);
    // Original sync was here
    return;
}
THREAD(check_matrix_afterif5) {
    check_matrix_afterif5_closure *largs = (check_matrix_afterif5_closure*)(args.get());
    return;
}
THREAD(check_matrix_cont0) {
    check_matrix_cont0_closure *largs = (check_matrix_cont0_closure*)(args.get());
    check_matrix_cont1_closure SN_check_matrix_cont1c(largs->k);
    spawn_next<check_matrix_cont1_closure> SN_check_matrix_cont1(SN_check_matrix_cont1c);
    ((check_matrix_cont1_closure*)SN_check_matrix_cont1.cls.get())->b = largs->b;
    ((check_matrix_cont1_closure*)SN_check_matrix_cont1.cls.get())->a = largs->a;
    ((check_matrix_cont1_closure*)SN_check_matrix_cont1.cls.get())->errorf0 = largs->errorf0;
    ((check_matrix_cont1_closure*)SN_check_matrix_cont1.cls.get())->v0 = largs->v0;
    ((check_matrix_cont1_closure*)SN_check_matrix_cont1.cls.get())->x = largs->x;
    ((check_matrix_cont1_closure*)SN_check_matrix_cont1.cls.get())->o = largs->o;
    ((check_matrix_cont1_closure*)SN_check_matrix_cont1.cls.get())->y = largs->y;
    ((check_matrix_cont1_closure*)SN_check_matrix_cont1.cls.get())->R2 = largs->R2;
    // Original sync was here
    return;
}
THREAD(check_matrix_cont1) {
    check_matrix_cont1_closure *largs = (check_matrix_cont1_closure*)(args.get());
    auto sp0c = std::make_shared<check_matrix_afterif5_closure>(largs->k);
    sp0c->R2 = largs->R2;
    sp0c->x = largs->x;
    sp0c->y = largs->y;
    sp0c->o = largs->o;
    sp0c->v0 = largs->v0;
    sp0c->errorf0 = largs->errorf0;
    sp0c->a = largs->a;
    sp0c->b = largs->b;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(check_matrix_cont2) {
    check_matrix_cont2_closure *largs = (check_matrix_cont2_closure*)(args.get());
    check_matrix_cont3_closure SN_check_matrix_cont3c(largs->k);
    spawn_next<check_matrix_cont3_closure> SN_check_matrix_cont3(SN_check_matrix_cont3c);
    ((check_matrix_cont3_closure*)SN_check_matrix_cont3.cls.get())->b = largs->b;
    ((check_matrix_cont3_closure*)SN_check_matrix_cont3.cls.get())->y = largs->y;
    ((check_matrix_cont3_closure*)SN_check_matrix_cont3.cls.get())->v0 = largs->v0;
    ((check_matrix_cont3_closure*)SN_check_matrix_cont3.cls.get())->o = largs->o;
    ((check_matrix_cont3_closure*)SN_check_matrix_cont3.cls.get())->a = largs->a;
    ((check_matrix_cont3_closure*)SN_check_matrix_cont3.cls.get())->errorf0 = largs->errorf0;
    ((check_matrix_cont3_closure*)SN_check_matrix_cont3.cls.get())->x = largs->x;
    ((check_matrix_cont3_closure*)SN_check_matrix_cont3.cls.get())->R2 = largs->R2;
    // Original sync was here
    return;
}
THREAD(check_matrix_cont3) {
    check_matrix_cont3_closure *largs = (check_matrix_cont3_closure*)(args.get());
    auto sp0c = std::make_shared<check_matrix_afterif5_closure>(largs->k);
    sp0c->R2 = largs->R2;
    sp0c->x = largs->x;
    sp0c->y = largs->y;
    sp0c->o = largs->o;
    sp0c->v0 = largs->v0;
    sp0c->errorf0 = largs->errorf0;
    sp0c->a = largs->a;
    sp0c->b = largs->b;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(add_matrix_cont0) {
    add_matrix_cont0_closure *largs = (add_matrix_cont0_closure*)(args.get());
    auto sp0c = std::make_shared<add_matrix_afterif4_closure>(largs->k);
    sp0c->T0 = largs->T0;
    sp0c->ot = largs->ot;
    sp0c->R4 = largs->R4;
    sp0c->orr = largs->orr;
    sp0c->x0 = largs->x0;
    sp0c->y0 = largs->y0;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(add_matrix_cont1) {
    add_matrix_cont1_closure *largs = (add_matrix_cont1_closure*)(args.get());
    auto sp0c = std::make_shared<add_matrix_afterif4_closure>(largs->k);
    sp0c->T0 = largs->T0;
    sp0c->ot = largs->ot;
    sp0c->R4 = largs->R4;
    sp0c->orr = largs->orr;
    sp0c->x0 = largs->x0;
    sp0c->y0 = largs->y0;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(init_matrix_cont0) {
    init_matrix_cont0_closure *largs = (init_matrix_cont0_closure*)(args.get());
    auto sp0c = std::make_shared<init_matrix_afterif3_closure>(largs->k);
    sp0c->R6 = largs->R6;
    sp0c->x1 = largs->x1;
    sp0c->y1 = largs->y1;
    sp0c->o0 = largs->o0;
    sp0c->v2 = largs->v2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(init_matrix_cont1) {
    init_matrix_cont1_closure *largs = (init_matrix_cont1_closure*)(args.get());
    auto sp0c = std::make_shared<init_matrix_afterif3_closure>(largs->k);
    sp0c->R6 = largs->R6;
    sp0c->x1 = largs->x1;
    sp0c->y1 = largs->y1;
    sp0c->o0 = largs->o0;
    sp0c->v2 = largs->v2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(multiply_matrix_cont0) {
    multiply_matrix_cont0_closure *largs = (multiply_matrix_cont0_closure*)(args.get());
    auto sp0c = std::make_shared<multiply_matrix_afterif2_closure>(largs->k);
    sp0c->A1 = largs->A1;
    sp0c->oa = largs->oa;
    sp0c->B1 = largs->B1;
    sp0c->ob = largs->ob;
    sp0c->x2 = largs->x2;
    sp0c->y2 = largs->y2;
    sp0c->z = largs->z;
    sp0c->R7 = largs->R7;
    sp0c->orr0 = largs->orr0;
    sp0c->add = largs->add;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(multiply_matrix_cont1) {
    multiply_matrix_cont1_closure *largs = (multiply_matrix_cont1_closure*)(args.get());
    multiply_matrix_cont2_closure SN_multiply_matrix_cont2c(largs->k);
    spawn_next<multiply_matrix_cont2_closure> SN_multiply_matrix_cont2(SN_multiply_matrix_cont2c);
    cont sp0k;
    SN_BIND_VOID(SN_multiply_matrix_cont2, &sp0k);
    multiply_matrix_closure sp0c(sp0k);
    sp0c.A1 = largs->A1;
    sp0c.oa = largs->oa;
    sp0c.B1 = largs->B1;
    sp0c.ob = largs->ob;
    sp0c.x2 = largs->x2;
    sp0c.y2 = (largs->y2 / 2);
    sp0c.z = largs->z;
    sp0c.R7 = largs->R7;
    sp0c.orr0 = largs->orr0;
    sp0c.add = 1;
    spawn<multiply_matrix_closure> sp0(sp0c);

    ((multiply_matrix_cont2_closure*)SN_multiply_matrix_cont2.cls.get())->add = largs->add;
    ((multiply_matrix_cont2_closure*)SN_multiply_matrix_cont2.cls.get())->R7 = largs->R7;
    ((multiply_matrix_cont2_closure*)SN_multiply_matrix_cont2.cls.get())->z = largs->z;
    ((multiply_matrix_cont2_closure*)SN_multiply_matrix_cont2.cls.get())->x2 = largs->x2;
    ((multiply_matrix_cont2_closure*)SN_multiply_matrix_cont2.cls.get())->orr0 = largs->orr0;
    ((multiply_matrix_cont2_closure*)SN_multiply_matrix_cont2.cls.get())->B1 = largs->B1;
    ((multiply_matrix_cont2_closure*)SN_multiply_matrix_cont2.cls.get())->y2 = largs->y2;
    ((multiply_matrix_cont2_closure*)SN_multiply_matrix_cont2.cls.get())->oa = largs->oa;
    ((multiply_matrix_cont2_closure*)SN_multiply_matrix_cont2.cls.get())->ob = largs->ob;
    ((multiply_matrix_cont2_closure*)SN_multiply_matrix_cont2.cls.get())->A1 = largs->A1;
    // Original sync was here
    return;
}
THREAD(multiply_matrix_cont2) {
    multiply_matrix_cont2_closure *largs = (multiply_matrix_cont2_closure*)(args.get());
    auto sp0c = std::make_shared<multiply_matrix_afterif2_closure>(largs->k);
    sp0c->A1 = largs->A1;
    sp0c->oa = largs->oa;
    sp0c->B1 = largs->B1;
    sp0c->ob = largs->ob;
    sp0c->x2 = largs->x2;
    sp0c->y2 = largs->y2;
    sp0c->z = largs->z;
    sp0c->R7 = largs->R7;
    sp0c->orr0 = largs->orr0;
    sp0c->add = largs->add;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(multiply_matrix_cont3) {
    multiply_matrix_cont3_closure *largs = (multiply_matrix_cont3_closure*)(args.get());
    auto sp0c = std::make_shared<multiply_matrix_afterif1_closure>(largs->k);
    sp0c->A1 = largs->A1;
    sp0c->oa = largs->oa;
    sp0c->B1 = largs->B1;
    sp0c->ob = largs->ob;
    sp0c->x2 = largs->x2;
    sp0c->y2 = largs->y2;
    sp0c->z = largs->z;
    sp0c->R7 = largs->R7;
    sp0c->orr0 = largs->orr0;
    sp0c->add = largs->add;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(run_cont0) {
    run_cont0_closure *largs = (run_cont0_closure*)(args.get());
    run_cont1_closure SN_run_cont1c(largs->k);
    spawn_next<run_cont1_closure> SN_run_cont1(SN_run_cont1c);
    ((run_cont1_closure*)SN_run_cont1.cls.get())->t2 = largs->t2;
    ((run_cont1_closure*)SN_run_cont1.cls.get())->t1 = largs->t1;
    ((run_cont1_closure*)SN_run_cont1.cls.get())->B2 = largs->B2;
    ((run_cont1_closure*)SN_run_cont1.cls.get())->y3 = largs->y3;
    ((run_cont1_closure*)SN_run_cont1.cls.get())->A2 = largs->A2;
    ((run_cont1_closure*)SN_run_cont1.cls.get())->check = largs->check;
    ((run_cont1_closure*)SN_run_cont1.cls.get())->z0 = largs->z0;
    ((run_cont1_closure*)SN_run_cont1.cls.get())->R8 = largs->R8;
    ((run_cont1_closure*)SN_run_cont1.cls.get())->x3 = largs->x3;
    // Original sync was here
    return;
}
THREAD(run_cont1) {
    run_cont1_closure *largs = (run_cont1_closure*)(args.get());
    gettimeofday(&(largs->t1),0);
    run_cont2_closure SN_run_cont2c(largs->k);
    spawn_next<run_cont2_closure> SN_run_cont2(SN_run_cont2c);
    cont sp0k;
    SN_BIND_VOID(SN_run_cont2, &sp0k);
    multiply_matrix_closure sp0c(sp0k);
    sp0c.A1 = largs->A2;
    sp0c.oa = largs->y3;
    sp0c.B1 = largs->B2;
    sp0c.ob = largs->z0;
    sp0c.x2 = largs->x3;
    sp0c.y2 = largs->y3;
    sp0c.z = largs->z0;
    sp0c.R7 = largs->R8;
    sp0c.orr0 = largs->z0;
    sp0c.add = 0;
    spawn<multiply_matrix_closure> sp0(sp0c);

    ((run_cont2_closure*)SN_run_cont2.cls.get())->t2 = largs->t2;
    ((run_cont2_closure*)SN_run_cont2.cls.get())->t1 = largs->t1;
    ((run_cont2_closure*)SN_run_cont2.cls.get())->R8 = largs->R8;
    ((run_cont2_closure*)SN_run_cont2.cls.get())->B2 = largs->B2;
    ((run_cont2_closure*)SN_run_cont2.cls.get())->x3 = largs->x3;
    ((run_cont2_closure*)SN_run_cont2.cls.get())->check = largs->check;
    ((run_cont2_closure*)SN_run_cont2.cls.get())->z0 = largs->z0;
    ((run_cont2_closure*)SN_run_cont2.cls.get())->y3 = largs->y3;
    ((run_cont2_closure*)SN_run_cont2.cls.get())->A2 = largs->A2;
    // Original sync was here
    return;
}
THREAD(run_cont2) {
    unsigned long long runtime_ms;
    run_cont2_closure *largs = (run_cont2_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    printf("%f\n",(runtime_ms / 1000.));
    if (largs->check) {
        printf("Now check result ... \n");
        largs->check = 0;
        run_cont3_closure SN_run_cont3c(largs->k);
        spawn_next<run_cont3_closure> SN_run_cont3(SN_run_cont3c);
        cont sp0k;
        SN_BIND_VOID(SN_run_cont3, &sp0k);
        check_matrix_closure sp0c(sp0k);
        sp0c.R2 = largs->R8;
        sp0c.x = largs->x3;
        sp0c.y = largs->z0;
        sp0c.o = largs->z0;
        sp0c.v0 = (largs->y3 * 16);
        sp0c.errorf0 = &(largs->check);
        spawn<check_matrix_closure> sp0(sp0c);

        ((run_cont3_closure*)SN_run_cont3.cls.get())->t1 = largs->t1;
        ((run_cont3_closure*)SN_run_cont3.cls.get())->runtime_ms = runtime_ms;
        ((run_cont3_closure*)SN_run_cont3.cls.get())->R8 = largs->R8;
        ((run_cont3_closure*)SN_run_cont3.cls.get())->t2 = largs->t2;
        ((run_cont3_closure*)SN_run_cont3.cls.get())->B2 = largs->B2;
        ((run_cont3_closure*)SN_run_cont3.cls.get())->x3 = largs->x3;
        ((run_cont3_closure*)SN_run_cont3.cls.get())->check = largs->check;
        ((run_cont3_closure*)SN_run_cont3.cls.get())->A2 = largs->A2;
        ((run_cont3_closure*)SN_run_cont3.cls.get())->z0 = largs->z0;
        ((run_cont3_closure*)SN_run_cont3.cls.get())->y3 = largs->y3;
        // Original sync was here
    } else {
        auto sp1c = std::make_shared<run_afterif0_closure>(largs->k);
        sp1c->x3 = largs->x3;
        sp1c->y3 = largs->y3;
        sp1c->z0 = largs->z0;
        sp1c->check = largs->check;
        sp1c->A2 = largs->A2;
        sp1c->B2 = largs->B2;
        sp1c->R8 = largs->R8;
        sp1c->t1 = largs->t1;
        sp1c->t2 = largs->t2;
        sp1c->runtime_ms = runtime_ms;
        cilk_spawn taskSpawn(sp1c->getTask(), sp1c);
        return;
    }
    return;
}
THREAD(run_cont3) {
    run_cont3_closure *largs = (run_cont3_closure*)(args.get());
    auto sp0c = std::make_shared<run_afterif0_closure>(largs->k);
    sp0c->x3 = largs->x3;
    sp0c->y3 = largs->y3;
    sp0c->z0 = largs->z0;
    sp0c->check = largs->check;
    sp0c->A2 = largs->A2;
    sp0c->B2 = largs->B2;
    sp0c->R8 = largs->R8;
    sp0c->t1 = largs->t1;
    sp0c->t2 = largs->t2;
    sp0c->runtime_ms = largs->runtime_ms;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(multiply_matrix_afterif1_cont0) {
    multiply_matrix_afterif1_cont0_closure *largs = (multiply_matrix_afterif1_cont0_closure*)(args.get());
    return;
}
THREAD(init_matrix_afterif3_cont0) {
    init_matrix_afterif3_cont0_closure *largs = (init_matrix_afterif3_cont0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(add_matrix_afterif4_cont0) {
    add_matrix_afterif4_cont0_closure *largs = (add_matrix_afterif4_cont0_closure*)(args.get());
    return;
}
