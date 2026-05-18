#include "cilk_explicit.hh"
/*
 * this is a cilk FFT code.  Most of the code is machine-generated.
 * We use it to find bugs in cilk2c.
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "getoptions.h"

#if CILKSAN
#include "cilksan.h"
#endif

/* Definitions and operations for complex numbers */

/* our real numbers */
typedef float REAL;

/* Complex numbers and operations */
typedef struct {
  REAL re, im;
} COMPLEX;

#define c_re(c) ((c).re)
#define c_im(c) ((c).im)

/* apparently register storage specifier is deprecated */
#define register

THREAD(compute_w_coefficients);
int factor(int n0);
THREAD(unshuffle);
void fft_twiddle_gen1(COMPLEX *in0, COMPLEX *out0, COMPLEX *W0, int r2, int m0, int nW, int nWdnti, int nWdntm);
THREAD(fft_twiddle_gen);
void fft_base_2(COMPLEX *in2, COMPLEX *out2);
THREAD(fft_twiddle_2);
THREAD(fft_unshuffle_2);
void fft_base_4(COMPLEX *in5, COMPLEX *out5);
THREAD(fft_twiddle_4);
THREAD(fft_unshuffle_4);
void fft_base_8(COMPLEX *in8, COMPLEX *out8);
THREAD(fft_twiddle_8);
THREAD(fft_unshuffle_8);
void fft_base_16(COMPLEX *in11, COMPLEX *out11);
THREAD(fft_twiddle_16);
THREAD(fft_unshuffle_16);
void fft_base_32(COMPLEX *in14, COMPLEX *out14);
THREAD(fft_twiddle_32);
THREAD(fft_unshuffle_32);
THREAD(fft_aux);
void cilk_fft(int n2, COMPLEX *in18, COMPLEX *out18);
THREAD(test_fft_elem);
void test_fft(int n4, COMPLEX *in20, COMPLEX *out20);
THREAD(fft_aux_afterif0);
THREAD(fft_aux_afterif0_afterif1);
THREAD(fft_aux_afterif0_afterif2);
THREAD(fft_aux_afterif0_afterif3);
THREAD(fft_aux_afterif0_afterif4);
THREAD(fft_aux_afterif0_afterif5);
THREAD(fft_aux_afterif6);
THREAD(fft_aux_afterif7);
THREAD(fft_aux_afterif8);
THREAD(fft_aux_afterif9);
THREAD(fft_aux_afterif10);
THREAD(compute_w_coefficients_cont0);
THREAD(compute_w_coefficients_cont1);
THREAD(unshuffle_cont0);
THREAD(unshuffle_cont1);
THREAD(fft_twiddle_gen_cont0);
THREAD(fft_twiddle_gen_cont1);
THREAD(fft_twiddle_2_cont0);
THREAD(fft_twiddle_2_cont1);
THREAD(fft_unshuffle_2_cont0);
THREAD(fft_unshuffle_2_cont1);
THREAD(fft_twiddle_4_cont0);
THREAD(fft_twiddle_4_cont1);
THREAD(fft_unshuffle_4_cont0);
THREAD(fft_unshuffle_4_cont1);
THREAD(fft_twiddle_8_cont0);
THREAD(fft_twiddle_8_cont1);
THREAD(fft_unshuffle_8_cont0);
THREAD(fft_unshuffle_8_cont1);
THREAD(fft_twiddle_16_cont0);
THREAD(fft_twiddle_16_cont1);
THREAD(fft_unshuffle_16_cont0);
THREAD(fft_unshuffle_16_cont1);
THREAD(fft_twiddle_32_cont0);
THREAD(fft_twiddle_32_cont1);
THREAD(fft_unshuffle_32_cont0);
THREAD(fft_unshuffle_32_cont1);
THREAD(fft_aux_cont0);
THREAD(fft_aux_cont1);
THREAD(fft_aux_cont2);
THREAD(fft_aux_cont3);
THREAD(fft_aux_cont4);
THREAD(fft_aux_cont5);
THREAD(cilk_fft_cont0);
THREAD(cilk_fft_cont1);
THREAD(test_fft_cont0);
THREAD(fft_aux_afterif0_cont0);
THREAD(fft_aux_afterif0_cont1);
THREAD(fft_aux_afterif0_cont2);
THREAD(fft_aux_afterif0_cont3);
THREAD(fft_aux_afterif0_cont4);
THREAD(fft_aux_afterif0_cont5);
THREAD(fft_aux_afterif6_cont0);

CLOSURE_DEF(compute_w_coefficients,
    int n;
    int a;
    int b;
    COMPLEX *W;
);
CLOSURE_DEF(unshuffle,
    int a0;
    int b0;
    COMPLEX *in;
    COMPLEX *out;
    int r1;
    int m;
);
CLOSURE_DEF(fft_twiddle_gen,
    int i3;
    int i1;
    COMPLEX *in1;
    COMPLEX *out1;
    COMPLEX *W1;
    int nW0;
    int nWdn;
    int r3;
    int m1;
);
CLOSURE_DEF(fft_twiddle_2,
    int a1;
    int b1;
    COMPLEX *in3;
    COMPLEX *out3;
    COMPLEX *W2;
    int nW1;
    int nWdn0;
    int m2;
);
CLOSURE_DEF(fft_unshuffle_2,
    int a2;
    int b2;
    COMPLEX *in4;
    COMPLEX *out4;
    int m3;
);
CLOSURE_DEF(fft_twiddle_4,
    int a3;
    int b3;
    COMPLEX *in6;
    COMPLEX *out6;
    COMPLEX *W3;
    int nW2;
    int nWdn1;
    int m4;
);
CLOSURE_DEF(fft_unshuffle_4,
    int a4;
    int b4;
    COMPLEX *in7;
    COMPLEX *out7;
    int m5;
);
CLOSURE_DEF(fft_twiddle_8,
    int a5;
    int b5;
    COMPLEX *in9;
    COMPLEX *out9;
    COMPLEX *W4;
    int nW3;
    int nWdn2;
    int m6;
);
CLOSURE_DEF(fft_unshuffle_8,
    int a6;
    int b6;
    COMPLEX *in10;
    COMPLEX *out10;
    int m7;
);
CLOSURE_DEF(fft_twiddle_16,
    int a7;
    int b7;
    COMPLEX *in12;
    COMPLEX *out12;
    COMPLEX *W5;
    int nW4;
    int nWdn3;
    int m8;
);
CLOSURE_DEF(fft_unshuffle_16,
    int a8;
    int b8;
    COMPLEX *in13;
    COMPLEX *out13;
    int m9;
);
CLOSURE_DEF(fft_twiddle_32,
    int a9;
    int b9;
    COMPLEX *in15;
    COMPLEX *out15;
    COMPLEX *W6;
    int nW5;
    int nWdn4;
    int m10;
);
CLOSURE_DEF(fft_unshuffle_32,
    int a10;
    int b10;
    COMPLEX *in16;
    COMPLEX *out16;
    int m11;
);
CLOSURE_DEF(fft_aux,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
);
CLOSURE_DEF(test_fft_elem,
    int n3;
    int j1;
    COMPLEX *in19;
    COMPLEX *out19;
);
CLOSURE_DEF(fft_aux_afterif0,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif0_afterif1,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif0_afterif2,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif0_afterif3,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif0_afterif4,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif0_afterif5,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif6,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif7,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif8,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif9,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif10,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(compute_w_coefficients_cont0,
);
CLOSURE_DEF(compute_w_coefficients_cont1,
);
CLOSURE_DEF(unshuffle_cont0,
);
CLOSURE_DEF(unshuffle_cont1,
);
CLOSURE_DEF(fft_twiddle_gen_cont0,
);
CLOSURE_DEF(fft_twiddle_gen_cont1,
);
CLOSURE_DEF(fft_twiddle_2_cont0,
);
CLOSURE_DEF(fft_twiddle_2_cont1,
);
CLOSURE_DEF(fft_unshuffle_2_cont0,
);
CLOSURE_DEF(fft_unshuffle_2_cont1,
);
CLOSURE_DEF(fft_twiddle_4_cont0,
);
CLOSURE_DEF(fft_twiddle_4_cont1,
);
CLOSURE_DEF(fft_unshuffle_4_cont0,
);
CLOSURE_DEF(fft_unshuffle_4_cont1,
);
CLOSURE_DEF(fft_twiddle_8_cont0,
);
CLOSURE_DEF(fft_twiddle_8_cont1,
);
CLOSURE_DEF(fft_unshuffle_8_cont0,
);
CLOSURE_DEF(fft_unshuffle_8_cont1,
);
CLOSURE_DEF(fft_twiddle_16_cont0,
);
CLOSURE_DEF(fft_twiddle_16_cont1,
);
CLOSURE_DEF(fft_unshuffle_16_cont0,
);
CLOSURE_DEF(fft_unshuffle_16_cont1,
);
CLOSURE_DEF(fft_twiddle_32_cont0,
);
CLOSURE_DEF(fft_twiddle_32_cont1,
);
CLOSURE_DEF(fft_unshuffle_32_cont0,
);
CLOSURE_DEF(fft_unshuffle_32_cont1,
);
CLOSURE_DEF(fft_aux_cont0,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_cont1,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_cont2,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_cont3,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_cont4,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_cont5,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(cilk_fft_cont0,
    int n2;
    COMPLEX *in18;
    COMPLEX *out18;
    int factors0[40];
    COMPLEX *W8;
);
CLOSURE_DEF(cilk_fft_cont1,
    COMPLEX *W8;
);
CLOSURE_DEF(test_fft_cont0,
);
CLOSURE_DEF(fft_aux_afterif0_cont0,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif0_cont1,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif0_cont2,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif0_cont3,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif0_cont4,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif0_cont5,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
CLOSURE_DEF(fft_aux_afterif6_cont0,
    int n1;
    COMPLEX *in17;
    COMPLEX *out17;
    int *factors;
    COMPLEX *W7;
    int nW6;
    int r5;
    int m12;
    int k1;
    int k2;
);
unsigned long long todval(struct timeval *tp) {
  return tp->tv_sec * 1000 * 1000 + tp->tv_usec;
}

/*
 * compute the W coefficients (that is, powers of the root of 1)
 * and store them into an array.
 */


/*
 * Determine (in a stupid way) if n is divisible by eight, then by four, else
 * find the smallest prime factor of n.
 */








/* machine-generated code begins here */






























/* end of machine-generated code */

/*
 * Recursive complex FFT on the n complex components of the array in:
 * basic Cooley-Tukey algorithm, with some improvements for
 * n power of two. The result is placed in the array out. n is arbitrary.
 * The algorithm runs in time O(n*(r1 + ... + rk)) where r1, ..., rk
 * are prime numbers, and r1 * r2 * ... * rk = n.
 *
 * n: size of the input
 * in: pointer to input
 * out: pointer to output
 * factors: list of factors of n, precomputed
 * W: twiddle factors
 * nW: size of W, that is, size of the original transform
 *
 */


/*
 * user interface for fft_aux
 */


/****************************************************************
 *                     END OF FFT ALGORITHM
 ****************************************************************/

/*                            tests                             */

/*
 * trivial DFT algorithm O(n^2)
 */




#define max 800
void test_correctness(void) {

  COMPLEX *in1, *in2, *out1, *out2;
  int n;
  int i;
  double error, a;

  in1 = (COMPLEX *)malloc(max * sizeof(COMPLEX));
  in2 = (COMPLEX *)malloc(max * sizeof(COMPLEX));
  out1 = (COMPLEX *)malloc(max * sizeof(COMPLEX));
  out2 = (COMPLEX *)malloc(max * sizeof(COMPLEX));

  for (n = 1; n < max; ++n) {
    /* generate random inputs */
    for (i = 0; i < n; ++i) {
      c_re(in1[i]) = c_re(in2[i]) = i;   /* drand48(); */
      c_im(in1[i]) = c_im(in2[i]) = 0.0; /* drand48(); */
    }

    /* fft-ize */
    cilk_fft(n, in1, out1);

    test_fft(n, in2, out2);

    /* compute the relative error */
    error = 0.0;
    for (i = 0; i < n; ++i) {
      double d;
      a = sqrt(
          (c_re(out1[i]) - c_re(out2[i])) * (c_re(out1[i]) - c_re(out2[i])) +
          (c_im(out1[i]) - c_im(out2[i])) * (c_im(out1[i]) - c_im(out2[i])));
      d = sqrt(c_re(out2[i]) * c_re(out2[i]) + c_im(out2[i]) * c_im(out2[i]));
      if (d < -1.0e-10 || d > 1.0e-10)
        a /= d;
      if (a > error)
        error = a;
    }
    if (error > 1e-3) {
      printf("n=%d error=%e\n", n, error);
      printf("ct:\n");
      for (i = 0; i < n; ++i)
        printf("%f + %fi\n", c_re(out2[i]), c_im(out2[i]));
      printf("seq:\n");
      for (i = 0; i < n; ++i)
        printf("%f + %fi\n", c_re(out1[i]), c_im(out1[i]));
    }
    if (n % 10 == 0)
      printf("n=%d ok\n", n);
  }

  return;
}

void test_speed(long size) {

  COMPLEX *in, *out;
  int i = 0;

  in = (COMPLEX *)malloc(size * sizeof(COMPLEX));
  out = (COMPLEX *)malloc(size * sizeof(COMPLEX));

  /* generate random input */
  for (i = 0; i < size; ++i) {
    c_re(in[i]) = 1.0;
    c_im(in[i]) = 1.0;
  }

  struct timeval t1, t2;
  gettimeofday(&t1, 0);
  cilk_fft(size, in, out);
  gettimeofday(&t2, 0);
  unsigned long long runtime_ms = (todval(&t2) - todval(&t1)) / 1000;
  printf("%f\n", runtime_ms / 1000.0);

  fprintf(stderr, "\ncilk example: fft\n");
  fprintf(stderr, "options:  number of elements   n = %ld\n\n", size);

  free(in);
  free(out);
}

int usage(void) {

  fprintf(stderr,
          "\nusage: fft [<cilk-options>] [-n #] [-c] [-benchmark] [-h]\n\n");
  fprintf(stderr,
          "this program is a highly optimized version of the classical\n");
  fprintf(stderr, "cooley-tukey fast fourier transform algorithm.  "
                  "some documentation can\n");
  fprintf(
      stderr,
      "be found in the source code. the program is optimized for an exact\n");
  fprintf(stderr, "power of 2.  to test for correctness use parameter -c.\n\n");
  return 1;
}

const char *specifiers[] = {"-n", "-c", "-benchmark", "-h", 0};
int opt_types[] = {LONGARG, BOOLARG, BENCHMARK, BOOLARG, 0};

int main(int argc, char *argv[]) {

  int correctness, help, benchmark;
  long size;

  /* standard benchmark options */
  correctness = 0;
  size = 1024 * 1024;

  fprintf(stderr, "Testing cos: %f\n", cos(2.35));

  get_options(argc, argv, specifiers, opt_types, &size, &correctness,
              &benchmark, &help);

  if (help)
    return usage();

  if (benchmark) {
    switch (benchmark) {
    case 1: /* short benchmark options -- a little work */
      // size = 512 * 512;
      size = 16 * 1024 * 1024;
      break;
    case 2: /* standard benchmark options */
      // size = 1024 * 1024;
      size = 32 * 1024 * 1024;
      break;
    case 3: /* long benchmark options -- a lot of work */
      // size = 4 * 1024 * 1024;
      size = 64 * 1024 * 1024;
      break;
    }
  }
  if (correctness)
    test_correctness();
  else {
    test_speed(size);
  }

  return 0;
}
THREAD(compute_w_coefficients) {
    double twoPiOverN;
    int k;
    REAL s;
    REAL c;
    int ab;
    compute_w_coefficients_closure *largs = (compute_w_coefficients_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        twoPiOverN = ((2. * 3.1415926535897931) / largs->n);
        for (k = largs->a;(k <= largs->b);(++k)) {
            c = cos((twoPiOverN * k));
            largs->W[(largs->n - k)].re = c;
            largs->W[k].re = largs->W[(largs->n - k)].re;
            s = sin((twoPiOverN * k));
            largs->W[k].im = (-s);
            largs->W[(largs->n - k)].im = s;
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        compute_w_coefficients_cont0_closure SN_compute_w_coefficients_cont0c(largs->k);
        spawn_next<compute_w_coefficients_cont0_closure> SN_compute_w_coefficients_cont0(SN_compute_w_coefficients_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_compute_w_coefficients_cont0, &sp0k);
        compute_w_coefficients_closure sp0c(sp0k);
        sp0c.n = largs->n;
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.W = largs->W;
        spawn<compute_w_coefficients_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_compute_w_coefficients_cont0, &sp1k);
        compute_w_coefficients_closure sp1c(sp1k);
        sp1c.n = largs->n;
        sp1c.a = (ab + 1);
        sp1c.b = largs->b;
        sp1c.W = largs->W;
        spawn<compute_w_coefficients_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
int factor(int n0) {
    int r;
    if ((n0 < 2)) {
        return 1;
    } else {
        if (((((((n0 == 64) || (n0 == 128)) || (n0 == 256)) || (n0 == 1024)) || (n0 == 2048)) || (n0 == 4096))) {
            return 8;
        } else {
            if (((n0 & 15) == 0)) {
                return 16;
            } else {
                if (((n0 & 7) == 0)) {
                    return 8;
                } else {
                    if (((n0 & 3) == 0)) {
                        return 4;
                    } else {
                        if (((n0 & 1) == 0)) {
                            return 2;
                        } else {
                            for (r = 3;(r < n0);r = (r + 2)) {
                                if (((n0 % r) == 0)) {
                                    return r;
                                } else {
                                }
                            }
                            return n0;
                        }
                    }
                }
            }
        }
    }
}
THREAD(unshuffle) {
    int i;
    int j;
    int r4;
    const COMPLEX *ip;
    COMPLEX *jp;
    int ab0;
    unshuffle_closure *largs = (unshuffle_closure*)(args.get());
    r4 = (largs->r1 & (~3));
    if (((largs->b0 - largs->a0) < 16)) {
        ip = (largs->in + (largs->a0 * largs->r1));
        for (i = largs->a0;(i < largs->b0);(++i)) {
            jp = (largs->out + i);
            for (j = 0;(j < r4);j = (j + 4)) {
                jp[0] = ip[0];
                jp[largs->m] = ip[1];
                jp[2 * largs->m] = ip[2];
                jp[3 * largs->m] = ip[3];
                jp = (jp + (4 * largs->m));
                ip = (ip + 4);
            }
            for (;(j < largs->r1);(++j)) {
                *jp = *ip;
                (ip++);
                jp = (jp + largs->m);
            }
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab0 = ((largs->a0 + largs->b0) / 2);
        unshuffle_cont0_closure SN_unshuffle_cont0c(largs->k);
        spawn_next<unshuffle_cont0_closure> SN_unshuffle_cont0(SN_unshuffle_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_unshuffle_cont0, &sp0k);
        unshuffle_closure sp0c(sp0k);
        sp0c.a0 = largs->a0;
        sp0c.b0 = ab0;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.r1 = largs->r1;
        sp0c.m = largs->m;
        spawn<unshuffle_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_unshuffle_cont0, &sp1k);
        unshuffle_closure sp1c(sp1k);
        sp1c.a0 = ab0;
        sp1c.b0 = largs->b0;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.r1 = largs->r1;
        sp1c.m = largs->m;
        spawn<unshuffle_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
void fft_twiddle_gen1(COMPLEX *in0, COMPLEX *out0, COMPLEX *W0, int r2, int m0, int nW, int nWdnti, int nWdntm) {
    int j0;
    int k0;
    COMPLEX *jp0;
    COMPLEX *kp;
    REAL r0;
    REAL i0;
    REAL rt;
    REAL it;
    REAL rw;
    REAL iw;
    int l1;
    int l0;
    k0 = 0;
    for (kp = out0;(k0 < r2);kp = (kp + m0)) {
        l1 = (nWdnti + (nWdntm * k0));
        i0 = 0.;
        r0 = i0;
        j0 = 0;
        jp0 = in0;
        for (l0 = 0;(j0 < r2);jp0 = (jp0 + m0)) {
            rw = W0[l0].re;
            iw = W0[l0].im;
            rt = jp0->re;
            it = jp0->im;
            r0 = (r0 + ((rt * rw) - (it * iw)));
            i0 = (i0 + ((rt * iw) + (it * rw)));
            l0 = (l0 + l1);
            if ((l0 > nW)) {
                l0 = (l0 - nW);
            }
            (++j0);
        }
        kp->re = r0;
        kp->im = i0;
        (++k0);
    }
    return ;
}
THREAD(fft_twiddle_gen) {
    int i2;
    fft_twiddle_gen_closure *largs = (fft_twiddle_gen_closure*)(args.get());
    if ((largs->i3 == (largs->i1 - 1))) {
        fft_twiddle_gen1((largs->in1 + largs->i3),(largs->out1 + largs->i3),largs->W1,largs->r3,largs->m1,largs->nW0,(largs->nWdn * largs->i3),(largs->nWdn * largs->m1));
        SEND_ARGUMENT(largs->k, 0);
    } else {
        i2 = ((largs->i3 + largs->i1) / 2);
        fft_twiddle_gen_cont0_closure SN_fft_twiddle_gen_cont0c(largs->k);
        spawn_next<fft_twiddle_gen_cont0_closure> SN_fft_twiddle_gen_cont0(SN_fft_twiddle_gen_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_gen_cont0, &sp0k);
        fft_twiddle_gen_closure sp0c(sp0k);
        sp0c.i3 = largs->i3;
        sp0c.i1 = i2;
        sp0c.in1 = largs->in1;
        sp0c.out1 = largs->out1;
        sp0c.W1 = largs->W1;
        sp0c.nW0 = largs->nW0;
        sp0c.nWdn = largs->nWdn;
        sp0c.r3 = largs->r3;
        sp0c.m1 = largs->m1;
        spawn<fft_twiddle_gen_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_gen_cont0, &sp1k);
        fft_twiddle_gen_closure sp1c(sp1k);
        sp1c.i3 = i2;
        sp1c.i1 = largs->i1;
        sp1c.in1 = largs->in1;
        sp1c.out1 = largs->out1;
        sp1c.W1 = largs->W1;
        sp1c.nW0 = largs->nW0;
        sp1c.nWdn = largs->nWdn;
        sp1c.r3 = largs->r3;
        sp1c.m1 = largs->m1;
        spawn<fft_twiddle_gen_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
void fft_base_2(COMPLEX *in2, COMPLEX *out2) {
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
    r1_0 = in2[0].re;
    i1_0 = in2[0].im;
    r1_1 = in2[1].re;
    i1_1 = in2[1].im;
    out2[0].re = (r1_0 + r1_1);
    out2[0].im = (i1_0 + i1_1);
    out2[1].re = (r1_0 - r1_1);
    out2[1].im = (i1_0 - i1_1);
}
THREAD(fft_twiddle_2) {
    int l10;
    int i4;
    COMPLEX *jp1;
    COMPLEX *kp0;
    REAL tmpr;
    REAL tmpi;
    REAL wr;
    REAL wi;
    REAL r1_00;
    REAL i1_00;
    REAL r1_110;
    REAL i1_110;
    int ab1;
    fft_twiddle_2_closure *largs = (fft_twiddle_2_closure*)(args.get());
    if (((largs->b1 - largs->a1) < 128)) {
        i4 = largs->a1;
        l10 = (largs->nWdn0 * i4);
        for (kp0 = (largs->out3 + i4);(i4 < largs->b1);(kp0++)) {
            jp1 = (largs->in3 + i4);
            r1_00 = jp1[(0 * largs->m2)].re;
            i1_00 = jp1[(0 * largs->m2)].im;
            wr = largs->W2[(1 * l10)].re;
            wi = largs->W2[(1 * l10)].im;
            tmpr = jp1[(1 * largs->m2)].re;
            tmpi = jp1[(1 * largs->m2)].im;
            r1_110 = ((wr * tmpr) - (wi * tmpi));
            i1_110 = ((wi * tmpr) + (wr * tmpi));
            kp0[(0 * largs->m2)].re = (r1_00 + r1_110);
            kp0[(0 * largs->m2)].im = (i1_00 + i1_110);
            kp0[(1 * largs->m2)].re = (r1_00 - r1_110);
            kp0[(1 * largs->m2)].im = (i1_00 - i1_110);
            (i4++);
            l10 = (l10 + largs->nWdn0);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab1 = ((largs->a1 + largs->b1) / 2);
        fft_twiddle_2_cont0_closure SN_fft_twiddle_2_cont0c(largs->k);
        spawn_next<fft_twiddle_2_cont0_closure> SN_fft_twiddle_2_cont0(SN_fft_twiddle_2_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_2_cont0, &sp0k);
        fft_twiddle_2_closure sp0c(sp0k);
        sp0c.a1 = largs->a1;
        sp0c.b1 = ab1;
        sp0c.in3 = largs->in3;
        sp0c.out3 = largs->out3;
        sp0c.W2 = largs->W2;
        sp0c.nW1 = largs->nW1;
        sp0c.nWdn0 = largs->nWdn0;
        sp0c.m2 = largs->m2;
        spawn<fft_twiddle_2_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_2_cont0, &sp1k);
        fft_twiddle_2_closure sp1c(sp1k);
        sp1c.a1 = ab1;
        sp1c.b1 = largs->b1;
        sp1c.in3 = largs->in3;
        sp1c.out3 = largs->out3;
        sp1c.W2 = largs->W2;
        sp1c.nW1 = largs->nW1;
        sp1c.nWdn0 = largs->nWdn0;
        sp1c.m2 = largs->m2;
        spawn<fft_twiddle_2_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
THREAD(fft_unshuffle_2) {
    int i5;
    const COMPLEX *ip0;
    COMPLEX *jp2;
    int ab2;
    fft_unshuffle_2_closure *largs = (fft_unshuffle_2_closure*)(args.get());
    if (((largs->b2 - largs->a2) < 128)) {
        ip0 = (largs->in4 + (largs->a2 * 2));
        for (i5 = largs->a2;(i5 < largs->b2);(++i5)) {
            jp2 = (largs->out4 + i5);
            jp2[0] = ip0[0];
            jp2[largs->m3] = ip0[1];
            ip0 = (ip0 + 2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab2 = ((largs->a2 + largs->b2) / 2);
        fft_unshuffle_2_cont0_closure SN_fft_unshuffle_2_cont0c(largs->k);
        spawn_next<fft_unshuffle_2_cont0_closure> SN_fft_unshuffle_2_cont0(SN_fft_unshuffle_2_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_unshuffle_2_cont0, &sp0k);
        fft_unshuffle_2_closure sp0c(sp0k);
        sp0c.a2 = largs->a2;
        sp0c.b2 = ab2;
        sp0c.in4 = largs->in4;
        sp0c.out4 = largs->out4;
        sp0c.m3 = largs->m3;
        spawn<fft_unshuffle_2_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_unshuffle_2_cont0, &sp1k);
        fft_unshuffle_2_closure sp1c(sp1k);
        sp1c.a2 = ab2;
        sp1c.b2 = largs->b2;
        sp1c.in4 = largs->in4;
        sp1c.out4 = largs->out4;
        sp1c.m3 = largs->m3;
        spawn<fft_unshuffle_2_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
void fft_base_4(COMPLEX *in5, COMPLEX *out5) {
    REAL r1_01;
    REAL i1_01;
    REAL r1_111;
    REAL i1_111;
    REAL r1_2;
    REAL i1_2;
    REAL r1_3;
    REAL i1_3;
    REAL r2_0;
    REAL i2_0;
    REAL r2_2;
    REAL i2_2;
    REAL r2_1;
    REAL i2_1;
    REAL r2_3;
    REAL i2_3;
    r2_0 = in5[0].re;
    i2_0 = in5[0].im;
    r2_2 = in5[2].re;
    i2_2 = in5[2].im;
    r1_01 = (r2_0 + r2_2);
    i1_01 = (i2_0 + i2_2);
    r1_2 = (r2_0 - r2_2);
    i1_2 = (i2_0 - i2_2);
    r2_1 = in5[1].re;
    i2_1 = in5[1].im;
    r2_3 = in5[3].re;
    i2_3 = in5[3].im;
    r1_111 = (r2_1 + r2_3);
    i1_111 = (i2_1 + i2_3);
    r1_3 = (r2_1 - r2_3);
    i1_3 = (i2_1 - i2_3);
    out5[0].re = (r1_01 + r1_111);
    out5[0].im = (i1_01 + i1_111);
    out5[2].re = (r1_01 - r1_111);
    out5[2].im = (i1_01 - i1_111);
    out5[1].re = (r1_2 + i1_3);
    out5[1].im = (i1_2 - r1_3);
    out5[3].re = (r1_2 - i1_3);
    out5[3].im = (i1_2 + r1_3);
}
THREAD(fft_twiddle_4) {
    int l11;
    int i6;
    COMPLEX *jp3;
    COMPLEX *kp1;
    REAL tmpr0;
    REAL tmpi0;
    REAL wr0;
    REAL wi0;
    REAL r1_02;
    REAL i1_02;
    REAL r1_112;
    REAL i1_112;
    REAL r1_210;
    REAL i1_210;
    REAL r1_32;
    REAL i1_32;
    REAL r2_00;
    REAL i2_00;
    REAL r2_210;
    REAL i2_210;
    REAL r2_110;
    REAL i2_110;
    REAL r2_32;
    REAL i2_32;
    int ab3;
    fft_twiddle_4_closure *largs = (fft_twiddle_4_closure*)(args.get());
    if (((largs->b3 - largs->a3) < 128)) {
        i6 = largs->a3;
        l11 = (largs->nWdn1 * i6);
        for (kp1 = (largs->out6 + i6);(i6 < largs->b3);(kp1++)) {
            jp3 = (largs->in6 + i6);
            r2_00 = jp3[(0 * largs->m4)].re;
            i2_00 = jp3[(0 * largs->m4)].im;
            wr0 = largs->W3[(2 * l11)].re;
            wi0 = largs->W3[(2 * l11)].im;
            tmpr0 = jp3[(2 * largs->m4)].re;
            tmpi0 = jp3[(2 * largs->m4)].im;
            r2_210 = ((wr0 * tmpr0) - (wi0 * tmpi0));
            i2_210 = ((wi0 * tmpr0) + (wr0 * tmpi0));
            r1_02 = (r2_00 + r2_210);
            i1_02 = (i2_00 + i2_210);
            r1_210 = (r2_00 - r2_210);
            i1_210 = (i2_00 - i2_210);
            wr0 = largs->W3[(1 * l11)].re;
            wi0 = largs->W3[(1 * l11)].im;
            tmpr0 = jp3[(1 * largs->m4)].re;
            tmpi0 = jp3[(1 * largs->m4)].im;
            r2_110 = ((wr0 * tmpr0) - (wi0 * tmpi0));
            i2_110 = ((wi0 * tmpr0) + (wr0 * tmpi0));
            wr0 = largs->W3[(3 * l11)].re;
            wi0 = largs->W3[(3 * l11)].im;
            tmpr0 = jp3[(3 * largs->m4)].re;
            tmpi0 = jp3[(3 * largs->m4)].im;
            r2_32 = ((wr0 * tmpr0) - (wi0 * tmpi0));
            i2_32 = ((wi0 * tmpr0) + (wr0 * tmpi0));
            r1_112 = (r2_110 + r2_32);
            i1_112 = (i2_110 + i2_32);
            r1_32 = (r2_110 - r2_32);
            i1_32 = (i2_110 - i2_32);
            kp1[(0 * largs->m4)].re = (r1_02 + r1_112);
            kp1[(0 * largs->m4)].im = (i1_02 + i1_112);
            kp1[(2 * largs->m4)].re = (r1_02 - r1_112);
            kp1[(2 * largs->m4)].im = (i1_02 - i1_112);
            kp1[(1 * largs->m4)].re = (r1_210 + i1_32);
            kp1[(1 * largs->m4)].im = (i1_210 - r1_32);
            kp1[(3 * largs->m4)].re = (r1_210 - i1_32);
            kp1[(3 * largs->m4)].im = (i1_210 + r1_32);
            (i6++);
            l11 = (l11 + largs->nWdn1);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab3 = ((largs->a3 + largs->b3) / 2);
        fft_twiddle_4_cont0_closure SN_fft_twiddle_4_cont0c(largs->k);
        spawn_next<fft_twiddle_4_cont0_closure> SN_fft_twiddle_4_cont0(SN_fft_twiddle_4_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_4_cont0, &sp0k);
        fft_twiddle_4_closure sp0c(sp0k);
        sp0c.a3 = largs->a3;
        sp0c.b3 = ab3;
        sp0c.in6 = largs->in6;
        sp0c.out6 = largs->out6;
        sp0c.W3 = largs->W3;
        sp0c.nW2 = largs->nW2;
        sp0c.nWdn1 = largs->nWdn1;
        sp0c.m4 = largs->m4;
        spawn<fft_twiddle_4_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_4_cont0, &sp1k);
        fft_twiddle_4_closure sp1c(sp1k);
        sp1c.a3 = ab3;
        sp1c.b3 = largs->b3;
        sp1c.in6 = largs->in6;
        sp1c.out6 = largs->out6;
        sp1c.W3 = largs->W3;
        sp1c.nW2 = largs->nW2;
        sp1c.nWdn1 = largs->nWdn1;
        sp1c.m4 = largs->m4;
        spawn<fft_twiddle_4_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
THREAD(fft_unshuffle_4) {
    int i7;
    const COMPLEX *ip1;
    COMPLEX *jp4;
    int ab4;
    fft_unshuffle_4_closure *largs = (fft_unshuffle_4_closure*)(args.get());
    if (((largs->b4 - largs->a4) < 128)) {
        ip1 = (largs->in7 + (largs->a4 * 4));
        for (i7 = largs->a4;(i7 < largs->b4);(++i7)) {
            jp4 = (largs->out7 + i7);
            jp4[0] = ip1[0];
            jp4[largs->m5] = ip1[1];
            ip1 = (ip1 + 2);
            jp4 = (jp4 + (2 * largs->m5));
            jp4[0] = ip1[0];
            jp4[largs->m5] = ip1[1];
            ip1 = (ip1 + 2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab4 = ((largs->a4 + largs->b4) / 2);
        fft_unshuffle_4_cont0_closure SN_fft_unshuffle_4_cont0c(largs->k);
        spawn_next<fft_unshuffle_4_cont0_closure> SN_fft_unshuffle_4_cont0(SN_fft_unshuffle_4_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_unshuffle_4_cont0, &sp0k);
        fft_unshuffle_4_closure sp0c(sp0k);
        sp0c.a4 = largs->a4;
        sp0c.b4 = ab4;
        sp0c.in7 = largs->in7;
        sp0c.out7 = largs->out7;
        sp0c.m5 = largs->m5;
        spawn<fft_unshuffle_4_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_unshuffle_4_cont0, &sp1k);
        fft_unshuffle_4_closure sp1c(sp1k);
        sp1c.a4 = ab4;
        sp1c.b4 = largs->b4;
        sp1c.in7 = largs->in7;
        sp1c.out7 = largs->out7;
        sp1c.m5 = largs->m5;
        spawn<fft_unshuffle_4_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
void fft_base_8(COMPLEX *in8, COMPLEX *out8) {
    REAL tmpr1;
    REAL tmpi1;
    REAL r1_03;
    REAL i1_03;
    REAL r1_113;
    REAL i1_113;
    REAL r1_211;
    REAL i1_211;
    REAL r1_33;
    REAL i1_33;
    REAL r1_4;
    REAL i1_4;
    REAL r1_5;
    REAL i1_5;
    REAL r1_6;
    REAL i1_6;
    REAL r1_7;
    REAL i1_7;
    REAL r2_01;
    REAL i2_01;
    REAL r2_211;
    REAL i2_211;
    REAL r2_4;
    REAL i2_4;
    REAL r2_6;
    REAL i2_6;
    REAL r3_0;
    REAL i3_0;
    REAL r3_4;
    REAL i3_4;
    REAL r3_2;
    REAL i3_2;
    REAL r3_6;
    REAL i3_6;
    REAL r2_111;
    REAL i2_111;
    REAL r2_33;
    REAL i2_33;
    REAL r2_5;
    REAL i2_5;
    REAL r2_7;
    REAL i2_7;
    REAL r3_1;
    REAL i3_1;
    REAL r3_5;
    REAL i3_5;
    REAL r3_3;
    REAL i3_3;
    REAL r3_7;
    REAL i3_7;
    r3_0 = in8[0].re;
    i3_0 = in8[0].im;
    r3_4 = in8[4].re;
    i3_4 = in8[4].im;
    r2_01 = (r3_0 + r3_4);
    i2_01 = (i3_0 + i3_4);
    r2_4 = (r3_0 - r3_4);
    i2_4 = (i3_0 - i3_4);
    r3_2 = in8[2].re;
    i3_2 = in8[2].im;
    r3_6 = in8[6].re;
    i3_6 = in8[6].im;
    r2_211 = (r3_2 + r3_6);
    i2_211 = (i3_2 + i3_6);
    r2_6 = (r3_2 - r3_6);
    i2_6 = (i3_2 - i3_6);
    r1_03 = (r2_01 + r2_211);
    i1_03 = (i2_01 + i2_211);
    r1_4 = (r2_01 - r2_211);
    i1_4 = (i2_01 - i2_211);
    r1_211 = (r2_4 + i2_6);
    i1_211 = (i2_4 - r2_6);
    r1_6 = (r2_4 - i2_6);
    i1_6 = (i2_4 + r2_6);
    r3_1 = in8[1].re;
    i3_1 = in8[1].im;
    r3_5 = in8[5].re;
    i3_5 = in8[5].im;
    r2_111 = (r3_1 + r3_5);
    i2_111 = (i3_1 + i3_5);
    r2_5 = (r3_1 - r3_5);
    i2_5 = (i3_1 - i3_5);
    r3_3 = in8[3].re;
    i3_3 = in8[3].im;
    r3_7 = in8[7].re;
    i3_7 = in8[7].im;
    r2_33 = (r3_3 + r3_7);
    i2_33 = (i3_3 + i3_7);
    r2_7 = (r3_3 - r3_7);
    i2_7 = (i3_3 - i3_7);
    r1_113 = (r2_111 + r2_33);
    i1_113 = (i2_111 + i2_33);
    r1_5 = (r2_111 - r2_33);
    i1_5 = (i2_111 - i2_33);
    r1_33 = (r2_5 + i2_7);
    i1_33 = (i2_5 - r2_7);
    r1_7 = (r2_5 - i2_7);
    i1_7 = (i2_5 + r2_7);
    out8[0].re = (r1_03 + r1_113);
    out8[0].im = (i1_03 + i1_113);
    out8[4].re = (r1_03 - r1_113);
    out8[4].im = (i1_03 - i1_113);
    tmpr1 = (0.70710678118699999 * (r1_33 + i1_33));
    tmpi1 = (0.70710678118699999 * (i1_33 - r1_33));
    out8[1].re = (r1_211 + tmpr1);
    out8[1].im = (i1_211 + tmpi1);
    out8[5].re = (r1_211 - tmpr1);
    out8[5].im = (i1_211 - tmpi1);
    out8[2].re = (r1_4 + i1_5);
    out8[2].im = (i1_4 - r1_5);
    out8[6].re = (r1_4 - i1_5);
    out8[6].im = (i1_4 + r1_5);
    tmpr1 = (0.70710678118699999 * (i1_7 - r1_7));
    tmpi1 = (0.70710678118699999 * (r1_7 + i1_7));
    out8[3].re = (r1_6 + tmpr1);
    out8[3].im = (i1_6 - tmpi1);
    out8[7].re = (r1_6 - tmpr1);
    out8[7].im = (i1_6 + tmpi1);
}
THREAD(fft_twiddle_8) {
    int l12;
    int i8;
    COMPLEX *jp5;
    COMPLEX *kp2;
    REAL tmpr2;
    REAL tmpi2;
    REAL wr1;
    REAL wi1;
    REAL r1_04;
    REAL i1_04;
    REAL r1_114;
    REAL i1_114;
    REAL r1_212;
    REAL i1_212;
    REAL r1_34;
    REAL i1_34;
    REAL r1_40;
    REAL i1_40;
    REAL r1_50;
    REAL i1_50;
    REAL r1_60;
    REAL i1_60;
    REAL r1_70;
    REAL i1_70;
    REAL r2_02;
    REAL i2_02;
    REAL r2_212;
    REAL i2_212;
    REAL r2_40;
    REAL i2_40;
    REAL r2_60;
    REAL i2_60;
    REAL r3_00;
    REAL i3_00;
    REAL r3_40;
    REAL i3_40;
    REAL r3_210;
    REAL i3_210;
    REAL r3_60;
    REAL i3_60;
    REAL r2_112;
    REAL i2_112;
    REAL r2_34;
    REAL i2_34;
    REAL r2_50;
    REAL i2_50;
    REAL r2_70;
    REAL i2_70;
    REAL r3_110;
    REAL i3_110;
    REAL r3_50;
    REAL i3_50;
    REAL r3_32;
    REAL i3_32;
    REAL r3_70;
    REAL i3_70;
    int ab5;
    fft_twiddle_8_closure *largs = (fft_twiddle_8_closure*)(args.get());
    if (((largs->b5 - largs->a5) < 128)) {
        i8 = largs->a5;
        l12 = (largs->nWdn2 * i8);
        for (kp2 = (largs->out9 + i8);(i8 < largs->b5);(kp2++)) {
            jp5 = (largs->in9 + i8);
            r3_00 = jp5[(0 * largs->m6)].re;
            i3_00 = jp5[(0 * largs->m6)].im;
            wr1 = largs->W4[(4 * l12)].re;
            wi1 = largs->W4[(4 * l12)].im;
            tmpr2 = jp5[(4 * largs->m6)].re;
            tmpi2 = jp5[(4 * largs->m6)].im;
            r3_40 = ((wr1 * tmpr2) - (wi1 * tmpi2));
            i3_40 = ((wi1 * tmpr2) + (wr1 * tmpi2));
            r2_02 = (r3_00 + r3_40);
            i2_02 = (i3_00 + i3_40);
            r2_40 = (r3_00 - r3_40);
            i2_40 = (i3_00 - i3_40);
            wr1 = largs->W4[(2 * l12)].re;
            wi1 = largs->W4[(2 * l12)].im;
            tmpr2 = jp5[(2 * largs->m6)].re;
            tmpi2 = jp5[(2 * largs->m6)].im;
            r3_210 = ((wr1 * tmpr2) - (wi1 * tmpi2));
            i3_210 = ((wi1 * tmpr2) + (wr1 * tmpi2));
            wr1 = largs->W4[(6 * l12)].re;
            wi1 = largs->W4[(6 * l12)].im;
            tmpr2 = jp5[(6 * largs->m6)].re;
            tmpi2 = jp5[(6 * largs->m6)].im;
            r3_60 = ((wr1 * tmpr2) - (wi1 * tmpi2));
            i3_60 = ((wi1 * tmpr2) + (wr1 * tmpi2));
            r2_212 = (r3_210 + r3_60);
            i2_212 = (i3_210 + i3_60);
            r2_60 = (r3_210 - r3_60);
            i2_60 = (i3_210 - i3_60);
            r1_04 = (r2_02 + r2_212);
            i1_04 = (i2_02 + i2_212);
            r1_40 = (r2_02 - r2_212);
            i1_40 = (i2_02 - i2_212);
            r1_212 = (r2_40 + i2_60);
            i1_212 = (i2_40 - r2_60);
            r1_60 = (r2_40 - i2_60);
            i1_60 = (i2_40 + r2_60);
            wr1 = largs->W4[(1 * l12)].re;
            wi1 = largs->W4[(1 * l12)].im;
            tmpr2 = jp5[(1 * largs->m6)].re;
            tmpi2 = jp5[(1 * largs->m6)].im;
            r3_110 = ((wr1 * tmpr2) - (wi1 * tmpi2));
            i3_110 = ((wi1 * tmpr2) + (wr1 * tmpi2));
            wr1 = largs->W4[(5 * l12)].re;
            wi1 = largs->W4[(5 * l12)].im;
            tmpr2 = jp5[(5 * largs->m6)].re;
            tmpi2 = jp5[(5 * largs->m6)].im;
            r3_50 = ((wr1 * tmpr2) - (wi1 * tmpi2));
            i3_50 = ((wi1 * tmpr2) + (wr1 * tmpi2));
            r2_112 = (r3_110 + r3_50);
            i2_112 = (i3_110 + i3_50);
            r2_50 = (r3_110 - r3_50);
            i2_50 = (i3_110 - i3_50);
            wr1 = largs->W4[(3 * l12)].re;
            wi1 = largs->W4[(3 * l12)].im;
            tmpr2 = jp5[(3 * largs->m6)].re;
            tmpi2 = jp5[(3 * largs->m6)].im;
            r3_32 = ((wr1 * tmpr2) - (wi1 * tmpi2));
            i3_32 = ((wi1 * tmpr2) + (wr1 * tmpi2));
            wr1 = largs->W4[(7 * l12)].re;
            wi1 = largs->W4[(7 * l12)].im;
            tmpr2 = jp5[(7 * largs->m6)].re;
            tmpi2 = jp5[(7 * largs->m6)].im;
            r3_70 = ((wr1 * tmpr2) - (wi1 * tmpi2));
            i3_70 = ((wi1 * tmpr2) + (wr1 * tmpi2));
            r2_34 = (r3_32 + r3_70);
            i2_34 = (i3_32 + i3_70);
            r2_70 = (r3_32 - r3_70);
            i2_70 = (i3_32 - i3_70);
            r1_114 = (r2_112 + r2_34);
            i1_114 = (i2_112 + i2_34);
            r1_50 = (r2_112 - r2_34);
            i1_50 = (i2_112 - i2_34);
            r1_34 = (r2_50 + i2_70);
            i1_34 = (i2_50 - r2_70);
            r1_70 = (r2_50 - i2_70);
            i1_70 = (i2_50 + r2_70);
            kp2[(0 * largs->m6)].re = (r1_04 + r1_114);
            kp2[(0 * largs->m6)].im = (i1_04 + i1_114);
            kp2[(4 * largs->m6)].re = (r1_04 - r1_114);
            kp2[(4 * largs->m6)].im = (i1_04 - i1_114);
            tmpr2 = (0.70710678118699999 * (r1_34 + i1_34));
            tmpi2 = (0.70710678118699999 * (i1_34 - r1_34));
            kp2[(1 * largs->m6)].re = (r1_212 + tmpr2);
            kp2[(1 * largs->m6)].im = (i1_212 + tmpi2);
            kp2[(5 * largs->m6)].re = (r1_212 - tmpr2);
            kp2[(5 * largs->m6)].im = (i1_212 - tmpi2);
            kp2[(2 * largs->m6)].re = (r1_40 + i1_50);
            kp2[(2 * largs->m6)].im = (i1_40 - r1_50);
            kp2[(6 * largs->m6)].re = (r1_40 - i1_50);
            kp2[(6 * largs->m6)].im = (i1_40 + r1_50);
            tmpr2 = (0.70710678118699999 * (i1_70 - r1_70));
            tmpi2 = (0.70710678118699999 * (r1_70 + i1_70));
            kp2[(3 * largs->m6)].re = (r1_60 + tmpr2);
            kp2[(3 * largs->m6)].im = (i1_60 - tmpi2);
            kp2[(7 * largs->m6)].re = (r1_60 - tmpr2);
            kp2[(7 * largs->m6)].im = (i1_60 + tmpi2);
            (i8++);
            l12 = (l12 + largs->nWdn2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab5 = ((largs->a5 + largs->b5) / 2);
        fft_twiddle_8_cont0_closure SN_fft_twiddle_8_cont0c(largs->k);
        spawn_next<fft_twiddle_8_cont0_closure> SN_fft_twiddle_8_cont0(SN_fft_twiddle_8_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_8_cont0, &sp0k);
        fft_twiddle_8_closure sp0c(sp0k);
        sp0c.a5 = largs->a5;
        sp0c.b5 = ab5;
        sp0c.in9 = largs->in9;
        sp0c.out9 = largs->out9;
        sp0c.W4 = largs->W4;
        sp0c.nW3 = largs->nW3;
        sp0c.nWdn2 = largs->nWdn2;
        sp0c.m6 = largs->m6;
        spawn<fft_twiddle_8_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_8_cont0, &sp1k);
        fft_twiddle_8_closure sp1c(sp1k);
        sp1c.a5 = ab5;
        sp1c.b5 = largs->b5;
        sp1c.in9 = largs->in9;
        sp1c.out9 = largs->out9;
        sp1c.W4 = largs->W4;
        sp1c.nW3 = largs->nW3;
        sp1c.nWdn2 = largs->nWdn2;
        sp1c.m6 = largs->m6;
        spawn<fft_twiddle_8_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
THREAD(fft_unshuffle_8) {
    int i9;
    const COMPLEX *ip2;
    COMPLEX *jp6;
    int ab6;
    fft_unshuffle_8_closure *largs = (fft_unshuffle_8_closure*)(args.get());
    if (((largs->b6 - largs->a6) < 128)) {
        ip2 = (largs->in10 + (largs->a6 * 8));
        for (i9 = largs->a6;(i9 < largs->b6);(++i9)) {
            jp6 = (largs->out10 + i9);
            jp6[0] = ip2[0];
            jp6[largs->m7] = ip2[1];
            ip2 = (ip2 + 2);
            jp6 = (jp6 + (2 * largs->m7));
            jp6[0] = ip2[0];
            jp6[largs->m7] = ip2[1];
            ip2 = (ip2 + 2);
            jp6 = (jp6 + (2 * largs->m7));
            jp6[0] = ip2[0];
            jp6[largs->m7] = ip2[1];
            ip2 = (ip2 + 2);
            jp6 = (jp6 + (2 * largs->m7));
            jp6[0] = ip2[0];
            jp6[largs->m7] = ip2[1];
            ip2 = (ip2 + 2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab6 = ((largs->a6 + largs->b6) / 2);
        fft_unshuffle_8_cont0_closure SN_fft_unshuffle_8_cont0c(largs->k);
        spawn_next<fft_unshuffle_8_cont0_closure> SN_fft_unshuffle_8_cont0(SN_fft_unshuffle_8_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_unshuffle_8_cont0, &sp0k);
        fft_unshuffle_8_closure sp0c(sp0k);
        sp0c.a6 = largs->a6;
        sp0c.b6 = ab6;
        sp0c.in10 = largs->in10;
        sp0c.out10 = largs->out10;
        sp0c.m7 = largs->m7;
        spawn<fft_unshuffle_8_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_unshuffle_8_cont0, &sp1k);
        fft_unshuffle_8_closure sp1c(sp1k);
        sp1c.a6 = ab6;
        sp1c.b6 = largs->b6;
        sp1c.in10 = largs->in10;
        sp1c.out10 = largs->out10;
        sp1c.m7 = largs->m7;
        spawn<fft_unshuffle_8_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
void fft_base_16(COMPLEX *in11, COMPLEX *out11) {
    REAL tmpr3;
    REAL tmpi3;
    REAL r1_05;
    REAL i1_05;
    REAL r1_115;
    REAL i1_115;
    REAL r1_213;
    REAL i1_213;
    REAL r1_35;
    REAL i1_35;
    REAL r1_41;
    REAL i1_41;
    REAL r1_51;
    REAL i1_51;
    REAL r1_61;
    REAL i1_61;
    REAL r1_71;
    REAL i1_71;
    REAL r1_8;
    REAL i1_8;
    REAL r1_9;
    REAL i1_9;
    REAL r1_10;
    REAL i1_10;
    REAL r1_11;
    REAL i1_11;
    REAL r1_12;
    REAL i1_12;
    REAL r1_13;
    REAL i1_13;
    REAL r1_14;
    REAL i1_14;
    REAL r1_15;
    REAL i1_15;
    REAL r2_03;
    REAL i2_03;
    REAL r2_213;
    REAL i2_213;
    REAL r2_41;
    REAL i2_41;
    REAL r2_61;
    REAL i2_61;
    REAL r2_8;
    REAL i2_8;
    REAL r2_10;
    REAL i2_10;
    REAL r2_12;
    REAL i2_12;
    REAL r2_14;
    REAL i2_14;
    REAL r3_01;
    REAL i3_01;
    REAL r3_41;
    REAL i3_41;
    REAL r3_8;
    REAL i3_8;
    REAL r3_12;
    REAL i3_12;
    REAL r4_0;
    REAL i4_0;
    REAL r4_8;
    REAL i4_8;
    REAL r4_4;
    REAL i4_4;
    REAL r4_12;
    REAL i4_12;
    REAL r3_211;
    REAL i3_211;
    REAL r3_61;
    REAL i3_61;
    REAL r3_10;
    REAL i3_10;
    REAL r3_14;
    REAL i3_14;
    REAL r4_2;
    REAL i4_2;
    REAL r4_10;
    REAL i4_10;
    REAL r4_6;
    REAL i4_6;
    REAL r4_14;
    REAL i4_14;
    REAL r2_113;
    REAL i2_113;
    REAL r2_35;
    REAL i2_35;
    REAL r2_51;
    REAL i2_51;
    REAL r2_71;
    REAL i2_71;
    REAL r2_9;
    REAL i2_9;
    REAL r2_11;
    REAL i2_11;
    REAL r2_13;
    REAL i2_13;
    REAL r2_15;
    REAL i2_15;
    REAL r3_111;
    REAL i3_111;
    REAL r3_51;
    REAL i3_51;
    REAL r3_9;
    REAL i3_9;
    REAL r3_13;
    REAL i3_13;
    REAL r4_1;
    REAL i4_1;
    REAL r4_9;
    REAL i4_9;
    REAL r4_5;
    REAL i4_5;
    REAL r4_13;
    REAL i4_13;
    REAL r3_33;
    REAL i3_33;
    REAL r3_71;
    REAL i3_71;
    REAL r3_11;
    REAL i3_11;
    REAL r3_15;
    REAL i3_15;
    REAL r4_3;
    REAL i4_3;
    REAL r4_11;
    REAL i4_11;
    REAL r4_7;
    REAL i4_7;
    REAL r4_15;
    REAL i4_15;
    r4_0 = in11[0].re;
    i4_0 = in11[0].im;
    r4_8 = in11[8].re;
    i4_8 = in11[8].im;
    r3_01 = (r4_0 + r4_8);
    i3_01 = (i4_0 + i4_8);
    r3_8 = (r4_0 - r4_8);
    i3_8 = (i4_0 - i4_8);
    r4_4 = in11[4].re;
    i4_4 = in11[4].im;
    r4_12 = in11[12].re;
    i4_12 = in11[12].im;
    r3_41 = (r4_4 + r4_12);
    i3_41 = (i4_4 + i4_12);
    r3_12 = (r4_4 - r4_12);
    i3_12 = (i4_4 - i4_12);
    r2_03 = (r3_01 + r3_41);
    i2_03 = (i3_01 + i3_41);
    r2_8 = (r3_01 - r3_41);
    i2_8 = (i3_01 - i3_41);
    r2_41 = (r3_8 + i3_12);
    i2_41 = (i3_8 - r3_12);
    r2_12 = (r3_8 - i3_12);
    i2_12 = (i3_8 + r3_12);
    r4_2 = in11[2].re;
    i4_2 = in11[2].im;
    r4_10 = in11[10].re;
    i4_10 = in11[10].im;
    r3_211 = (r4_2 + r4_10);
    i3_211 = (i4_2 + i4_10);
    r3_10 = (r4_2 - r4_10);
    i3_10 = (i4_2 - i4_10);
    r4_6 = in11[6].re;
    i4_6 = in11[6].im;
    r4_14 = in11[14].re;
    i4_14 = in11[14].im;
    r3_61 = (r4_6 + r4_14);
    i3_61 = (i4_6 + i4_14);
    r3_14 = (r4_6 - r4_14);
    i3_14 = (i4_6 - i4_14);
    r2_213 = (r3_211 + r3_61);
    i2_213 = (i3_211 + i3_61);
    r2_10 = (r3_211 - r3_61);
    i2_10 = (i3_211 - i3_61);
    r2_61 = (r3_10 + i3_14);
    i2_61 = (i3_10 - r3_14);
    r2_14 = (r3_10 - i3_14);
    i2_14 = (i3_10 + r3_14);
    r1_05 = (r2_03 + r2_213);
    i1_05 = (i2_03 + i2_213);
    r1_8 = (r2_03 - r2_213);
    i1_8 = (i2_03 - i2_213);
    tmpr3 = (0.70710678118699999 * (r2_61 + i2_61));
    tmpi3 = (0.70710678118699999 * (i2_61 - r2_61));
    r1_213 = (r2_41 + tmpr3);
    i1_213 = (i2_41 + tmpi3);
    r1_10 = (r2_41 - tmpr3);
    i1_10 = (i2_41 - tmpi3);
    r1_41 = (r2_8 + i2_10);
    i1_41 = (i2_8 - r2_10);
    r1_12 = (r2_8 - i2_10);
    i1_12 = (i2_8 + r2_10);
    tmpr3 = (0.70710678118699999 * (i2_14 - r2_14));
    tmpi3 = (0.70710678118699999 * (r2_14 + i2_14));
    r1_61 = (r2_12 + tmpr3);
    i1_61 = (i2_12 - tmpi3);
    r1_14 = (r2_12 - tmpr3);
    i1_14 = (i2_12 + tmpi3);
    r4_1 = in11[1].re;
    i4_1 = in11[1].im;
    r4_9 = in11[9].re;
    i4_9 = in11[9].im;
    r3_111 = (r4_1 + r4_9);
    i3_111 = (i4_1 + i4_9);
    r3_9 = (r4_1 - r4_9);
    i3_9 = (i4_1 - i4_9);
    r4_5 = in11[5].re;
    i4_5 = in11[5].im;
    r4_13 = in11[13].re;
    i4_13 = in11[13].im;
    r3_51 = (r4_5 + r4_13);
    i3_51 = (i4_5 + i4_13);
    r3_13 = (r4_5 - r4_13);
    i3_13 = (i4_5 - i4_13);
    r2_113 = (r3_111 + r3_51);
    i2_113 = (i3_111 + i3_51);
    r2_9 = (r3_111 - r3_51);
    i2_9 = (i3_111 - i3_51);
    r2_51 = (r3_9 + i3_13);
    i2_51 = (i3_9 - r3_13);
    r2_13 = (r3_9 - i3_13);
    i2_13 = (i3_9 + r3_13);
    r4_3 = in11[3].re;
    i4_3 = in11[3].im;
    r4_11 = in11[11].re;
    i4_11 = in11[11].im;
    r3_33 = (r4_3 + r4_11);
    i3_33 = (i4_3 + i4_11);
    r3_11 = (r4_3 - r4_11);
    i3_11 = (i4_3 - i4_11);
    r4_7 = in11[7].re;
    i4_7 = in11[7].im;
    r4_15 = in11[15].re;
    i4_15 = in11[15].im;
    r3_71 = (r4_7 + r4_15);
    i3_71 = (i4_7 + i4_15);
    r3_15 = (r4_7 - r4_15);
    i3_15 = (i4_7 - i4_15);
    r2_35 = (r3_33 + r3_71);
    i2_35 = (i3_33 + i3_71);
    r2_11 = (r3_33 - r3_71);
    i2_11 = (i3_33 - i3_71);
    r2_71 = (r3_11 + i3_15);
    i2_71 = (i3_11 - r3_15);
    r2_15 = (r3_11 - i3_15);
    i2_15 = (i3_11 + r3_15);
    r1_115 = (r2_113 + r2_35);
    i1_115 = (i2_113 + i2_35);
    r1_9 = (r2_113 - r2_35);
    i1_9 = (i2_113 - i2_35);
    tmpr3 = (0.70710678118699999 * (r2_71 + i2_71));
    tmpi3 = (0.70710678118699999 * (i2_71 - r2_71));
    r1_35 = (r2_51 + tmpr3);
    i1_35 = (i2_51 + tmpi3);
    r1_11 = (r2_51 - tmpr3);
    i1_11 = (i2_51 - tmpi3);
    r1_51 = (r2_9 + i2_11);
    i1_51 = (i2_9 - r2_11);
    r1_13 = (r2_9 - i2_11);
    i1_13 = (i2_9 + r2_11);
    tmpr3 = (0.70710678118699999 * (i2_15 - r2_15));
    tmpi3 = (0.70710678118699999 * (r2_15 + i2_15));
    r1_71 = (r2_13 + tmpr3);
    i1_71 = (i2_13 - tmpi3);
    r1_15 = (r2_13 - tmpr3);
    i1_15 = (i2_13 + tmpi3);
    out11[0].re = (r1_05 + r1_115);
    out11[0].im = (i1_05 + i1_115);
    out11[8].re = (r1_05 - r1_115);
    out11[8].im = (i1_05 - i1_115);
    tmpr3 = ((0.92387953251099997 * r1_35) + (0.38268343236500002 * i1_35));
    tmpi3 = ((0.92387953251099997 * i1_35) - (0.38268343236500002 * r1_35));
    out11[1].re = (r1_213 + tmpr3);
    out11[1].im = (i1_213 + tmpi3);
    out11[9].re = (r1_213 - tmpr3);
    out11[9].im = (i1_213 - tmpi3);
    tmpr3 = (0.70710678118699999 * (r1_51 + i1_51));
    tmpi3 = (0.70710678118699999 * (i1_51 - r1_51));
    out11[2].re = (r1_41 + tmpr3);
    out11[2].im = (i1_41 + tmpi3);
    out11[10].re = (r1_41 - tmpr3);
    out11[10].im = (i1_41 - tmpi3);
    tmpr3 = ((0.38268343236500002 * r1_71) + (0.92387953251099997 * i1_71));
    tmpi3 = ((0.38268343236500002 * i1_71) - (0.92387953251099997 * r1_71));
    out11[3].re = (r1_61 + tmpr3);
    out11[3].im = (i1_61 + tmpi3);
    out11[11].re = (r1_61 - tmpr3);
    out11[11].im = (i1_61 - tmpi3);
    out11[4].re = (r1_8 + i1_9);
    out11[4].im = (i1_8 - r1_9);
    out11[12].re = (r1_8 - i1_9);
    out11[12].im = (i1_8 + r1_9);
    tmpr3 = ((0.92387953251099997 * i1_11) - (0.38268343236500002 * r1_11));
    tmpi3 = ((0.92387953251099997 * r1_11) + (0.38268343236500002 * i1_11));
    out11[5].re = (r1_10 + tmpr3);
    out11[5].im = (i1_10 - tmpi3);
    out11[13].re = (r1_10 - tmpr3);
    out11[13].im = (i1_10 + tmpi3);
    tmpr3 = (0.70710678118699999 * (i1_13 - r1_13));
    tmpi3 = (0.70710678118699999 * (r1_13 + i1_13));
    out11[6].re = (r1_12 + tmpr3);
    out11[6].im = (i1_12 - tmpi3);
    out11[14].re = (r1_12 - tmpr3);
    out11[14].im = (i1_12 + tmpi3);
    tmpr3 = ((0.38268343236500002 * i1_15) - (0.92387953251099997 * r1_15));
    tmpi3 = ((0.38268343236500002 * r1_15) + (0.92387953251099997 * i1_15));
    out11[7].re = (r1_14 + tmpr3);
    out11[7].im = (i1_14 - tmpi3);
    out11[15].re = (r1_14 - tmpr3);
    out11[15].im = (i1_14 + tmpi3);
}
THREAD(fft_twiddle_16) {
    int l13;
    int i10;
    COMPLEX *jp7;
    COMPLEX *kp3;
    REAL tmpr4;
    REAL tmpi4;
    REAL wr2;
    REAL wi2;
    REAL r1_06;
    REAL i1_06;
    REAL r1_116;
    REAL i1_116;
    REAL r1_214;
    REAL i1_214;
    REAL r1_36;
    REAL i1_36;
    REAL r1_42;
    REAL i1_42;
    REAL r1_52;
    REAL i1_52;
    REAL r1_62;
    REAL i1_62;
    REAL r1_72;
    REAL i1_72;
    REAL r1_80;
    REAL i1_80;
    REAL r1_90;
    REAL i1_90;
    REAL r1_100;
    REAL i1_100;
    REAL r1_117;
    REAL i1_117;
    REAL r1_120;
    REAL i1_120;
    REAL r1_130;
    REAL i1_130;
    REAL r1_140;
    REAL i1_140;
    REAL r1_150;
    REAL i1_150;
    REAL r2_04;
    REAL i2_04;
    REAL r2_214;
    REAL i2_214;
    REAL r2_42;
    REAL i2_42;
    REAL r2_62;
    REAL i2_62;
    REAL r2_80;
    REAL i2_80;
    REAL r2_100;
    REAL i2_100;
    REAL r2_120;
    REAL i2_120;
    REAL r2_140;
    REAL i2_140;
    REAL r3_02;
    REAL i3_02;
    REAL r3_42;
    REAL i3_42;
    REAL r3_80;
    REAL i3_80;
    REAL r3_120;
    REAL i3_120;
    REAL r4_00;
    REAL i4_00;
    REAL r4_80;
    REAL i4_80;
    REAL r4_40;
    REAL i4_40;
    REAL r4_120;
    REAL i4_120;
    REAL r3_212;
    REAL i3_212;
    REAL r3_62;
    REAL i3_62;
    REAL r3_100;
    REAL i3_100;
    REAL r3_140;
    REAL i3_140;
    REAL r4_210;
    REAL i4_210;
    REAL r4_100;
    REAL i4_100;
    REAL r4_60;
    REAL i4_60;
    REAL r4_140;
    REAL i4_140;
    REAL r2_114;
    REAL i2_114;
    REAL r2_36;
    REAL i2_36;
    REAL r2_52;
    REAL i2_52;
    REAL r2_72;
    REAL i2_72;
    REAL r2_90;
    REAL i2_90;
    REAL r2_115;
    REAL i2_115;
    REAL r2_130;
    REAL i2_130;
    REAL r2_150;
    REAL i2_150;
    REAL r3_112;
    REAL i3_112;
    REAL r3_52;
    REAL i3_52;
    REAL r3_90;
    REAL i3_90;
    REAL r3_130;
    REAL i3_130;
    REAL r4_110;
    REAL i4_110;
    REAL r4_90;
    REAL i4_90;
    REAL r4_50;
    REAL i4_50;
    REAL r4_130;
    REAL i4_130;
    REAL r3_34;
    REAL i3_34;
    REAL r3_72;
    REAL i3_72;
    REAL r3_113;
    REAL i3_113;
    REAL r3_150;
    REAL i3_150;
    REAL r4_32;
    REAL i4_32;
    REAL r4_111;
    REAL i4_111;
    REAL r4_70;
    REAL i4_70;
    REAL r4_150;
    REAL i4_150;
    int ab7;
    fft_twiddle_16_closure *largs = (fft_twiddle_16_closure*)(args.get());
    if (((largs->b7 - largs->a7) < 128)) {
        i10 = largs->a7;
        l13 = (largs->nWdn3 * i10);
        for (kp3 = (largs->out12 + i10);(i10 < largs->b7);(kp3++)) {
            jp7 = (largs->in12 + i10);
            r4_00 = jp7[(0 * largs->m8)].re;
            i4_00 = jp7[(0 * largs->m8)].im;
            wr2 = largs->W5[(8 * l13)].re;
            wi2 = largs->W5[(8 * l13)].im;
            tmpr4 = jp7[(8 * largs->m8)].re;
            tmpi4 = jp7[(8 * largs->m8)].im;
            r4_80 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_80 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            r3_02 = (r4_00 + r4_80);
            i3_02 = (i4_00 + i4_80);
            r3_80 = (r4_00 - r4_80);
            i3_80 = (i4_00 - i4_80);
            wr2 = largs->W5[(4 * l13)].re;
            wi2 = largs->W5[(4 * l13)].im;
            tmpr4 = jp7[(4 * largs->m8)].re;
            tmpi4 = jp7[(4 * largs->m8)].im;
            r4_40 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_40 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            wr2 = largs->W5[(12 * l13)].re;
            wi2 = largs->W5[(12 * l13)].im;
            tmpr4 = jp7[(12 * largs->m8)].re;
            tmpi4 = jp7[(12 * largs->m8)].im;
            r4_120 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_120 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            r3_42 = (r4_40 + r4_120);
            i3_42 = (i4_40 + i4_120);
            r3_120 = (r4_40 - r4_120);
            i3_120 = (i4_40 - i4_120);
            r2_04 = (r3_02 + r3_42);
            i2_04 = (i3_02 + i3_42);
            r2_80 = (r3_02 - r3_42);
            i2_80 = (i3_02 - i3_42);
            r2_42 = (r3_80 + i3_120);
            i2_42 = (i3_80 - r3_120);
            r2_120 = (r3_80 - i3_120);
            i2_120 = (i3_80 + r3_120);
            wr2 = largs->W5[(2 * l13)].re;
            wi2 = largs->W5[(2 * l13)].im;
            tmpr4 = jp7[(2 * largs->m8)].re;
            tmpi4 = jp7[(2 * largs->m8)].im;
            r4_210 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_210 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            wr2 = largs->W5[(10 * l13)].re;
            wi2 = largs->W5[(10 * l13)].im;
            tmpr4 = jp7[(10 * largs->m8)].re;
            tmpi4 = jp7[(10 * largs->m8)].im;
            r4_100 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_100 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            r3_212 = (r4_210 + r4_100);
            i3_212 = (i4_210 + i4_100);
            r3_100 = (r4_210 - r4_100);
            i3_100 = (i4_210 - i4_100);
            wr2 = largs->W5[(6 * l13)].re;
            wi2 = largs->W5[(6 * l13)].im;
            tmpr4 = jp7[(6 * largs->m8)].re;
            tmpi4 = jp7[(6 * largs->m8)].im;
            r4_60 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_60 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            wr2 = largs->W5[(14 * l13)].re;
            wi2 = largs->W5[(14 * l13)].im;
            tmpr4 = jp7[(14 * largs->m8)].re;
            tmpi4 = jp7[(14 * largs->m8)].im;
            r4_140 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_140 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            r3_62 = (r4_60 + r4_140);
            i3_62 = (i4_60 + i4_140);
            r3_140 = (r4_60 - r4_140);
            i3_140 = (i4_60 - i4_140);
            r2_214 = (r3_212 + r3_62);
            i2_214 = (i3_212 + i3_62);
            r2_100 = (r3_212 - r3_62);
            i2_100 = (i3_212 - i3_62);
            r2_62 = (r3_100 + i3_140);
            i2_62 = (i3_100 - r3_140);
            r2_140 = (r3_100 - i3_140);
            i2_140 = (i3_100 + r3_140);
            r1_06 = (r2_04 + r2_214);
            i1_06 = (i2_04 + i2_214);
            r1_80 = (r2_04 - r2_214);
            i1_80 = (i2_04 - i2_214);
            tmpr4 = (0.70710678118699999 * (r2_62 + i2_62));
            tmpi4 = (0.70710678118699999 * (i2_62 - r2_62));
            r1_214 = (r2_42 + tmpr4);
            i1_214 = (i2_42 + tmpi4);
            r1_100 = (r2_42 - tmpr4);
            i1_100 = (i2_42 - tmpi4);
            r1_42 = (r2_80 + i2_100);
            i1_42 = (i2_80 - r2_100);
            r1_120 = (r2_80 - i2_100);
            i1_120 = (i2_80 + r2_100);
            tmpr4 = (0.70710678118699999 * (i2_140 - r2_140));
            tmpi4 = (0.70710678118699999 * (r2_140 + i2_140));
            r1_62 = (r2_120 + tmpr4);
            i1_62 = (i2_120 - tmpi4);
            r1_140 = (r2_120 - tmpr4);
            i1_140 = (i2_120 + tmpi4);
            wr2 = largs->W5[(1 * l13)].re;
            wi2 = largs->W5[(1 * l13)].im;
            tmpr4 = jp7[(1 * largs->m8)].re;
            tmpi4 = jp7[(1 * largs->m8)].im;
            r4_110 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_110 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            wr2 = largs->W5[(9 * l13)].re;
            wi2 = largs->W5[(9 * l13)].im;
            tmpr4 = jp7[(9 * largs->m8)].re;
            tmpi4 = jp7[(9 * largs->m8)].im;
            r4_90 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_90 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            r3_112 = (r4_110 + r4_90);
            i3_112 = (i4_110 + i4_90);
            r3_90 = (r4_110 - r4_90);
            i3_90 = (i4_110 - i4_90);
            wr2 = largs->W5[(5 * l13)].re;
            wi2 = largs->W5[(5 * l13)].im;
            tmpr4 = jp7[(5 * largs->m8)].re;
            tmpi4 = jp7[(5 * largs->m8)].im;
            r4_50 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_50 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            wr2 = largs->W5[(13 * l13)].re;
            wi2 = largs->W5[(13 * l13)].im;
            tmpr4 = jp7[(13 * largs->m8)].re;
            tmpi4 = jp7[(13 * largs->m8)].im;
            r4_130 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_130 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            r3_52 = (r4_50 + r4_130);
            i3_52 = (i4_50 + i4_130);
            r3_130 = (r4_50 - r4_130);
            i3_130 = (i4_50 - i4_130);
            r2_114 = (r3_112 + r3_52);
            i2_114 = (i3_112 + i3_52);
            r2_90 = (r3_112 - r3_52);
            i2_90 = (i3_112 - i3_52);
            r2_52 = (r3_90 + i3_130);
            i2_52 = (i3_90 - r3_130);
            r2_130 = (r3_90 - i3_130);
            i2_130 = (i3_90 + r3_130);
            wr2 = largs->W5[(3 * l13)].re;
            wi2 = largs->W5[(3 * l13)].im;
            tmpr4 = jp7[(3 * largs->m8)].re;
            tmpi4 = jp7[(3 * largs->m8)].im;
            r4_32 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_32 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            wr2 = largs->W5[(11 * l13)].re;
            wi2 = largs->W5[(11 * l13)].im;
            tmpr4 = jp7[(11 * largs->m8)].re;
            tmpi4 = jp7[(11 * largs->m8)].im;
            r4_111 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_111 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            r3_34 = (r4_32 + r4_111);
            i3_34 = (i4_32 + i4_111);
            r3_113 = (r4_32 - r4_111);
            i3_113 = (i4_32 - i4_111);
            wr2 = largs->W5[(7 * l13)].re;
            wi2 = largs->W5[(7 * l13)].im;
            tmpr4 = jp7[(7 * largs->m8)].re;
            tmpi4 = jp7[(7 * largs->m8)].im;
            r4_70 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_70 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            wr2 = largs->W5[(15 * l13)].re;
            wi2 = largs->W5[(15 * l13)].im;
            tmpr4 = jp7[(15 * largs->m8)].re;
            tmpi4 = jp7[(15 * largs->m8)].im;
            r4_150 = ((wr2 * tmpr4) - (wi2 * tmpi4));
            i4_150 = ((wi2 * tmpr4) + (wr2 * tmpi4));
            r3_72 = (r4_70 + r4_150);
            i3_72 = (i4_70 + i4_150);
            r3_150 = (r4_70 - r4_150);
            i3_150 = (i4_70 - i4_150);
            r2_36 = (r3_34 + r3_72);
            i2_36 = (i3_34 + i3_72);
            r2_115 = (r3_34 - r3_72);
            i2_115 = (i3_34 - i3_72);
            r2_72 = (r3_113 + i3_150);
            i2_72 = (i3_113 - r3_150);
            r2_150 = (r3_113 - i3_150);
            i2_150 = (i3_113 + r3_150);
            r1_116 = (r2_114 + r2_36);
            i1_116 = (i2_114 + i2_36);
            r1_90 = (r2_114 - r2_36);
            i1_90 = (i2_114 - i2_36);
            tmpr4 = (0.70710678118699999 * (r2_72 + i2_72));
            tmpi4 = (0.70710678118699999 * (i2_72 - r2_72));
            r1_36 = (r2_52 + tmpr4);
            i1_36 = (i2_52 + tmpi4);
            r1_117 = (r2_52 - tmpr4);
            i1_117 = (i2_52 - tmpi4);
            r1_52 = (r2_90 + i2_115);
            i1_52 = (i2_90 - r2_115);
            r1_130 = (r2_90 - i2_115);
            i1_130 = (i2_90 + r2_115);
            tmpr4 = (0.70710678118699999 * (i2_150 - r2_150));
            tmpi4 = (0.70710678118699999 * (r2_150 + i2_150));
            r1_72 = (r2_130 + tmpr4);
            i1_72 = (i2_130 - tmpi4);
            r1_150 = (r2_130 - tmpr4);
            i1_150 = (i2_130 + tmpi4);
            kp3[(0 * largs->m8)].re = (r1_06 + r1_116);
            kp3[(0 * largs->m8)].im = (i1_06 + i1_116);
            kp3[(8 * largs->m8)].re = (r1_06 - r1_116);
            kp3[(8 * largs->m8)].im = (i1_06 - i1_116);
            tmpr4 = ((0.92387953251099997 * r1_36) + (0.38268343236500002 * i1_36));
            tmpi4 = ((0.92387953251099997 * i1_36) - (0.38268343236500002 * r1_36));
            kp3[(1 * largs->m8)].re = (r1_214 + tmpr4);
            kp3[(1 * largs->m8)].im = (i1_214 + tmpi4);
            kp3[(9 * largs->m8)].re = (r1_214 - tmpr4);
            kp3[(9 * largs->m8)].im = (i1_214 - tmpi4);
            tmpr4 = (0.70710678118699999 * (r1_52 + i1_52));
            tmpi4 = (0.70710678118699999 * (i1_52 - r1_52));
            kp3[(2 * largs->m8)].re = (r1_42 + tmpr4);
            kp3[(2 * largs->m8)].im = (i1_42 + tmpi4);
            kp3[(10 * largs->m8)].re = (r1_42 - tmpr4);
            kp3[(10 * largs->m8)].im = (i1_42 - tmpi4);
            tmpr4 = ((0.38268343236500002 * r1_72) + (0.92387953251099997 * i1_72));
            tmpi4 = ((0.38268343236500002 * i1_72) - (0.92387953251099997 * r1_72));
            kp3[(3 * largs->m8)].re = (r1_62 + tmpr4);
            kp3[(3 * largs->m8)].im = (i1_62 + tmpi4);
            kp3[(11 * largs->m8)].re = (r1_62 - tmpr4);
            kp3[(11 * largs->m8)].im = (i1_62 - tmpi4);
            kp3[(4 * largs->m8)].re = (r1_80 + i1_90);
            kp3[(4 * largs->m8)].im = (i1_80 - r1_90);
            kp3[(12 * largs->m8)].re = (r1_80 - i1_90);
            kp3[(12 * largs->m8)].im = (i1_80 + r1_90);
            tmpr4 = ((0.92387953251099997 * i1_117) - (0.38268343236500002 * r1_117));
            tmpi4 = ((0.92387953251099997 * r1_117) + (0.38268343236500002 * i1_117));
            kp3[(5 * largs->m8)].re = (r1_100 + tmpr4);
            kp3[(5 * largs->m8)].im = (i1_100 - tmpi4);
            kp3[(13 * largs->m8)].re = (r1_100 - tmpr4);
            kp3[(13 * largs->m8)].im = (i1_100 + tmpi4);
            tmpr4 = (0.70710678118699999 * (i1_130 - r1_130));
            tmpi4 = (0.70710678118699999 * (r1_130 + i1_130));
            kp3[(6 * largs->m8)].re = (r1_120 + tmpr4);
            kp3[(6 * largs->m8)].im = (i1_120 - tmpi4);
            kp3[(14 * largs->m8)].re = (r1_120 - tmpr4);
            kp3[(14 * largs->m8)].im = (i1_120 + tmpi4);
            tmpr4 = ((0.38268343236500002 * i1_150) - (0.92387953251099997 * r1_150));
            tmpi4 = ((0.38268343236500002 * r1_150) + (0.92387953251099997 * i1_150));
            kp3[(7 * largs->m8)].re = (r1_140 + tmpr4);
            kp3[(7 * largs->m8)].im = (i1_140 - tmpi4);
            kp3[(15 * largs->m8)].re = (r1_140 - tmpr4);
            kp3[(15 * largs->m8)].im = (i1_140 + tmpi4);
            (i10++);
            l13 = (l13 + largs->nWdn3);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab7 = ((largs->a7 + largs->b7) / 2);
        fft_twiddle_16_cont0_closure SN_fft_twiddle_16_cont0c(largs->k);
        spawn_next<fft_twiddle_16_cont0_closure> SN_fft_twiddle_16_cont0(SN_fft_twiddle_16_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_16_cont0, &sp0k);
        fft_twiddle_16_closure sp0c(sp0k);
        sp0c.a7 = largs->a7;
        sp0c.b7 = ab7;
        sp0c.in12 = largs->in12;
        sp0c.out12 = largs->out12;
        sp0c.W5 = largs->W5;
        sp0c.nW4 = largs->nW4;
        sp0c.nWdn3 = largs->nWdn3;
        sp0c.m8 = largs->m8;
        spawn<fft_twiddle_16_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_16_cont0, &sp1k);
        fft_twiddle_16_closure sp1c(sp1k);
        sp1c.a7 = ab7;
        sp1c.b7 = largs->b7;
        sp1c.in12 = largs->in12;
        sp1c.out12 = largs->out12;
        sp1c.W5 = largs->W5;
        sp1c.nW4 = largs->nW4;
        sp1c.nWdn3 = largs->nWdn3;
        sp1c.m8 = largs->m8;
        spawn<fft_twiddle_16_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
THREAD(fft_unshuffle_16) {
    int i11;
    const COMPLEX *ip3;
    COMPLEX *jp8;
    int ab8;
    fft_unshuffle_16_closure *largs = (fft_unshuffle_16_closure*)(args.get());
    if (((largs->b8 - largs->a8) < 128)) {
        ip3 = (largs->in13 + (largs->a8 * 16));
        for (i11 = largs->a8;(i11 < largs->b8);(++i11)) {
            jp8 = (largs->out13 + i11);
            jp8[0] = ip3[0];
            jp8[largs->m9] = ip3[1];
            ip3 = (ip3 + 2);
            jp8 = (jp8 + (2 * largs->m9));
            jp8[0] = ip3[0];
            jp8[largs->m9] = ip3[1];
            ip3 = (ip3 + 2);
            jp8 = (jp8 + (2 * largs->m9));
            jp8[0] = ip3[0];
            jp8[largs->m9] = ip3[1];
            ip3 = (ip3 + 2);
            jp8 = (jp8 + (2 * largs->m9));
            jp8[0] = ip3[0];
            jp8[largs->m9] = ip3[1];
            ip3 = (ip3 + 2);
            jp8 = (jp8 + (2 * largs->m9));
            jp8[0] = ip3[0];
            jp8[largs->m9] = ip3[1];
            ip3 = (ip3 + 2);
            jp8 = (jp8 + (2 * largs->m9));
            jp8[0] = ip3[0];
            jp8[largs->m9] = ip3[1];
            ip3 = (ip3 + 2);
            jp8 = (jp8 + (2 * largs->m9));
            jp8[0] = ip3[0];
            jp8[largs->m9] = ip3[1];
            ip3 = (ip3 + 2);
            jp8 = (jp8 + (2 * largs->m9));
            jp8[0] = ip3[0];
            jp8[largs->m9] = ip3[1];
            ip3 = (ip3 + 2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab8 = ((largs->a8 + largs->b8) / 2);
        fft_unshuffle_16_cont0_closure SN_fft_unshuffle_16_cont0c(largs->k);
        spawn_next<fft_unshuffle_16_cont0_closure> SN_fft_unshuffle_16_cont0(SN_fft_unshuffle_16_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_unshuffle_16_cont0, &sp0k);
        fft_unshuffle_16_closure sp0c(sp0k);
        sp0c.a8 = largs->a8;
        sp0c.b8 = ab8;
        sp0c.in13 = largs->in13;
        sp0c.out13 = largs->out13;
        sp0c.m9 = largs->m9;
        spawn<fft_unshuffle_16_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_unshuffle_16_cont0, &sp1k);
        fft_unshuffle_16_closure sp1c(sp1k);
        sp1c.a8 = ab8;
        sp1c.b8 = largs->b8;
        sp1c.in13 = largs->in13;
        sp1c.out13 = largs->out13;
        sp1c.m9 = largs->m9;
        spawn<fft_unshuffle_16_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
void fft_base_32(COMPLEX *in14, COMPLEX *out14) {
    REAL tmpr5;
    REAL tmpi5;
    REAL r1_07;
    REAL i1_07;
    REAL r1_118;
    REAL i1_118;
    REAL r1_215;
    REAL i1_215;
    REAL r1_37;
    REAL i1_37;
    REAL r1_43;
    REAL i1_43;
    REAL r1_53;
    REAL i1_53;
    REAL r1_63;
    REAL i1_63;
    REAL r1_73;
    REAL i1_73;
    REAL r1_81;
    REAL i1_81;
    REAL r1_91;
    REAL i1_91;
    REAL r1_101;
    REAL i1_101;
    REAL r1_119;
    REAL i1_119;
    REAL r1_121;
    REAL i1_121;
    REAL r1_131;
    REAL i1_131;
    REAL r1_141;
    REAL i1_141;
    REAL r1_151;
    REAL i1_151;
    REAL r1_16;
    REAL i1_16;
    REAL r1_17;
    REAL i1_17;
    REAL r1_18;
    REAL i1_18;
    REAL r1_19;
    REAL i1_19;
    REAL r1_20;
    REAL i1_20;
    REAL r1_21;
    REAL i1_21;
    REAL r1_22;
    REAL i1_22;
    REAL r1_23;
    REAL i1_23;
    REAL r1_24;
    REAL i1_24;
    REAL r1_25;
    REAL i1_25;
    REAL r1_26;
    REAL i1_26;
    REAL r1_27;
    REAL i1_27;
    REAL r1_28;
    REAL i1_28;
    REAL r1_29;
    REAL i1_29;
    REAL r1_30;
    REAL i1_30;
    REAL r1_31;
    REAL i1_31;
    REAL r2_05;
    REAL i2_05;
    REAL r2_215;
    REAL i2_215;
    REAL r2_43;
    REAL i2_43;
    REAL r2_63;
    REAL i2_63;
    REAL r2_81;
    REAL i2_81;
    REAL r2_101;
    REAL i2_101;
    REAL r2_121;
    REAL i2_121;
    REAL r2_141;
    REAL i2_141;
    REAL r2_16;
    REAL i2_16;
    REAL r2_18;
    REAL i2_18;
    REAL r2_20;
    REAL i2_20;
    REAL r2_22;
    REAL i2_22;
    REAL r2_24;
    REAL i2_24;
    REAL r2_26;
    REAL i2_26;
    REAL r2_28;
    REAL i2_28;
    REAL r2_30;
    REAL i2_30;
    REAL r3_03;
    REAL i3_03;
    REAL r3_43;
    REAL i3_43;
    REAL r3_81;
    REAL i3_81;
    REAL r3_121;
    REAL i3_121;
    REAL r3_16;
    REAL i3_16;
    REAL r3_20;
    REAL i3_20;
    REAL r3_24;
    REAL i3_24;
    REAL r3_28;
    REAL i3_28;
    REAL r4_01;
    REAL i4_01;
    REAL r4_81;
    REAL i4_81;
    REAL r4_16;
    REAL i4_16;
    REAL r4_24;
    REAL i4_24;
    REAL r5_0;
    REAL i5_0;
    REAL r5_16;
    REAL i5_16;
    REAL r5_8;
    REAL i5_8;
    REAL r5_24;
    REAL i5_24;
    REAL r4_41;
    REAL i4_41;
    REAL r4_121;
    REAL i4_121;
    REAL r4_20;
    REAL i4_20;
    REAL r4_28;
    REAL i4_28;
    REAL r5_4;
    REAL i5_4;
    REAL r5_20;
    REAL i5_20;
    REAL r5_12;
    REAL i5_12;
    REAL r5_28;
    REAL i5_28;
    REAL r3_213;
    REAL i3_213;
    REAL r3_63;
    REAL i3_63;
    REAL r3_101;
    REAL i3_101;
    REAL r3_141;
    REAL i3_141;
    REAL r3_18;
    REAL i3_18;
    REAL r3_22;
    REAL i3_22;
    REAL r3_26;
    REAL i3_26;
    REAL r3_30;
    REAL i3_30;
    REAL r4_211;
    REAL i4_211;
    REAL r4_101;
    REAL i4_101;
    REAL r4_18;
    REAL i4_18;
    REAL r4_26;
    REAL i4_26;
    REAL r5_2;
    REAL i5_2;
    REAL r5_18;
    REAL i5_18;
    REAL r5_10;
    REAL i5_10;
    REAL r5_26;
    REAL i5_26;
    REAL r4_61;
    REAL i4_61;
    REAL r4_141;
    REAL i4_141;
    REAL r4_22;
    REAL i4_22;
    REAL r4_30;
    REAL i4_30;
    REAL r5_6;
    REAL i5_6;
    REAL r5_22;
    REAL i5_22;
    REAL r5_14;
    REAL i5_14;
    REAL r5_30;
    REAL i5_30;
    REAL r2_116;
    REAL i2_116;
    REAL r2_37;
    REAL i2_37;
    REAL r2_53;
    REAL i2_53;
    REAL r2_73;
    REAL i2_73;
    REAL r2_91;
    REAL i2_91;
    REAL r2_117;
    REAL i2_117;
    REAL r2_131;
    REAL i2_131;
    REAL r2_151;
    REAL i2_151;
    REAL r2_17;
    REAL i2_17;
    REAL r2_19;
    REAL i2_19;
    REAL r2_21;
    REAL i2_21;
    REAL r2_23;
    REAL i2_23;
    REAL r2_25;
    REAL i2_25;
    REAL r2_27;
    REAL i2_27;
    REAL r2_29;
    REAL i2_29;
    REAL r2_31;
    REAL i2_31;
    REAL r3_114;
    REAL i3_114;
    REAL r3_53;
    REAL i3_53;
    REAL r3_91;
    REAL i3_91;
    REAL r3_131;
    REAL i3_131;
    REAL r3_17;
    REAL i3_17;
    REAL r3_21;
    REAL i3_21;
    REAL r3_25;
    REAL i3_25;
    REAL r3_29;
    REAL i3_29;
    REAL r4_112;
    REAL i4_112;
    REAL r4_91;
    REAL i4_91;
    REAL r4_17;
    REAL i4_17;
    REAL r4_25;
    REAL i4_25;
    REAL r5_1;
    REAL i5_1;
    REAL r5_17;
    REAL i5_17;
    REAL r5_9;
    REAL i5_9;
    REAL r5_25;
    REAL i5_25;
    REAL r4_51;
    REAL i4_51;
    REAL r4_131;
    REAL i4_131;
    REAL r4_21;
    REAL i4_21;
    REAL r4_29;
    REAL i4_29;
    REAL r5_5;
    REAL i5_5;
    REAL r5_21;
    REAL i5_21;
    REAL r5_13;
    REAL i5_13;
    REAL r5_29;
    REAL i5_29;
    REAL r3_35;
    REAL i3_35;
    REAL r3_73;
    REAL i3_73;
    REAL r3_115;
    REAL i3_115;
    REAL r3_151;
    REAL i3_151;
    REAL r3_19;
    REAL i3_19;
    REAL r3_23;
    REAL i3_23;
    REAL r3_27;
    REAL i3_27;
    REAL r3_31;
    REAL i3_31;
    REAL r4_33;
    REAL i4_33;
    REAL r4_113;
    REAL i4_113;
    REAL r4_19;
    REAL i4_19;
    REAL r4_27;
    REAL i4_27;
    REAL r5_3;
    REAL i5_3;
    REAL r5_19;
    REAL i5_19;
    REAL r5_11;
    REAL i5_11;
    REAL r5_27;
    REAL i5_27;
    REAL r4_71;
    REAL i4_71;
    REAL r4_151;
    REAL i4_151;
    REAL r4_23;
    REAL i4_23;
    REAL r4_31;
    REAL i4_31;
    REAL r5_7;
    REAL i5_7;
    REAL r5_23;
    REAL i5_23;
    REAL r5_15;
    REAL i5_15;
    REAL r5_31;
    REAL i5_31;
    r5_0 = in14[0].re;
    i5_0 = in14[0].im;
    r5_16 = in14[16].re;
    i5_16 = in14[16].im;
    r4_01 = (r5_0 + r5_16);
    i4_01 = (i5_0 + i5_16);
    r4_16 = (r5_0 - r5_16);
    i4_16 = (i5_0 - i5_16);
    r5_8 = in14[8].re;
    i5_8 = in14[8].im;
    r5_24 = in14[24].re;
    i5_24 = in14[24].im;
    r4_81 = (r5_8 + r5_24);
    i4_81 = (i5_8 + i5_24);
    r4_24 = (r5_8 - r5_24);
    i4_24 = (i5_8 - i5_24);
    r3_03 = (r4_01 + r4_81);
    i3_03 = (i4_01 + i4_81);
    r3_16 = (r4_01 - r4_81);
    i3_16 = (i4_01 - i4_81);
    r3_81 = (r4_16 + i4_24);
    i3_81 = (i4_16 - r4_24);
    r3_24 = (r4_16 - i4_24);
    i3_24 = (i4_16 + r4_24);
    r5_4 = in14[4].re;
    i5_4 = in14[4].im;
    r5_20 = in14[20].re;
    i5_20 = in14[20].im;
    r4_41 = (r5_4 + r5_20);
    i4_41 = (i5_4 + i5_20);
    r4_20 = (r5_4 - r5_20);
    i4_20 = (i5_4 - i5_20);
    r5_12 = in14[12].re;
    i5_12 = in14[12].im;
    r5_28 = in14[28].re;
    i5_28 = in14[28].im;
    r4_121 = (r5_12 + r5_28);
    i4_121 = (i5_12 + i5_28);
    r4_28 = (r5_12 - r5_28);
    i4_28 = (i5_12 - i5_28);
    r3_43 = (r4_41 + r4_121);
    i3_43 = (i4_41 + i4_121);
    r3_20 = (r4_41 - r4_121);
    i3_20 = (i4_41 - i4_121);
    r3_121 = (r4_20 + i4_28);
    i3_121 = (i4_20 - r4_28);
    r3_28 = (r4_20 - i4_28);
    i3_28 = (i4_20 + r4_28);
    r2_05 = (r3_03 + r3_43);
    i2_05 = (i3_03 + i3_43);
    r2_16 = (r3_03 - r3_43);
    i2_16 = (i3_03 - i3_43);
    tmpr5 = (0.70710678118699999 * (r3_121 + i3_121));
    tmpi5 = (0.70710678118699999 * (i3_121 - r3_121));
    r2_43 = (r3_81 + tmpr5);
    i2_43 = (i3_81 + tmpi5);
    r2_20 = (r3_81 - tmpr5);
    i2_20 = (i3_81 - tmpi5);
    r2_81 = (r3_16 + i3_20);
    i2_81 = (i3_16 - r3_20);
    r2_24 = (r3_16 - i3_20);
    i2_24 = (i3_16 + r3_20);
    tmpr5 = (0.70710678118699999 * (i3_28 - r3_28));
    tmpi5 = (0.70710678118699999 * (r3_28 + i3_28));
    r2_121 = (r3_24 + tmpr5);
    i2_121 = (i3_24 - tmpi5);
    r2_28 = (r3_24 - tmpr5);
    i2_28 = (i3_24 + tmpi5);
    r5_2 = in14[2].re;
    i5_2 = in14[2].im;
    r5_18 = in14[18].re;
    i5_18 = in14[18].im;
    r4_211 = (r5_2 + r5_18);
    i4_211 = (i5_2 + i5_18);
    r4_18 = (r5_2 - r5_18);
    i4_18 = (i5_2 - i5_18);
    r5_10 = in14[10].re;
    i5_10 = in14[10].im;
    r5_26 = in14[26].re;
    i5_26 = in14[26].im;
    r4_101 = (r5_10 + r5_26);
    i4_101 = (i5_10 + i5_26);
    r4_26 = (r5_10 - r5_26);
    i4_26 = (i5_10 - i5_26);
    r3_213 = (r4_211 + r4_101);
    i3_213 = (i4_211 + i4_101);
    r3_18 = (r4_211 - r4_101);
    i3_18 = (i4_211 - i4_101);
    r3_101 = (r4_18 + i4_26);
    i3_101 = (i4_18 - r4_26);
    r3_26 = (r4_18 - i4_26);
    i3_26 = (i4_18 + r4_26);
    r5_6 = in14[6].re;
    i5_6 = in14[6].im;
    r5_22 = in14[22].re;
    i5_22 = in14[22].im;
    r4_61 = (r5_6 + r5_22);
    i4_61 = (i5_6 + i5_22);
    r4_22 = (r5_6 - r5_22);
    i4_22 = (i5_6 - i5_22);
    r5_14 = in14[14].re;
    i5_14 = in14[14].im;
    r5_30 = in14[30].re;
    i5_30 = in14[30].im;
    r4_141 = (r5_14 + r5_30);
    i4_141 = (i5_14 + i5_30);
    r4_30 = (r5_14 - r5_30);
    i4_30 = (i5_14 - i5_30);
    r3_63 = (r4_61 + r4_141);
    i3_63 = (i4_61 + i4_141);
    r3_22 = (r4_61 - r4_141);
    i3_22 = (i4_61 - i4_141);
    r3_141 = (r4_22 + i4_30);
    i3_141 = (i4_22 - r4_30);
    r3_30 = (r4_22 - i4_30);
    i3_30 = (i4_22 + r4_30);
    r2_215 = (r3_213 + r3_63);
    i2_215 = (i3_213 + i3_63);
    r2_18 = (r3_213 - r3_63);
    i2_18 = (i3_213 - i3_63);
    tmpr5 = (0.70710678118699999 * (r3_141 + i3_141));
    tmpi5 = (0.70710678118699999 * (i3_141 - r3_141));
    r2_63 = (r3_101 + tmpr5);
    i2_63 = (i3_101 + tmpi5);
    r2_22 = (r3_101 - tmpr5);
    i2_22 = (i3_101 - tmpi5);
    r2_101 = (r3_18 + i3_22);
    i2_101 = (i3_18 - r3_22);
    r2_26 = (r3_18 - i3_22);
    i2_26 = (i3_18 + r3_22);
    tmpr5 = (0.70710678118699999 * (i3_30 - r3_30));
    tmpi5 = (0.70710678118699999 * (r3_30 + i3_30));
    r2_141 = (r3_26 + tmpr5);
    i2_141 = (i3_26 - tmpi5);
    r2_30 = (r3_26 - tmpr5);
    i2_30 = (i3_26 + tmpi5);
    r1_07 = (r2_05 + r2_215);
    i1_07 = (i2_05 + i2_215);
    r1_16 = (r2_05 - r2_215);
    i1_16 = (i2_05 - i2_215);
    tmpr5 = ((0.92387953251099997 * r2_63) + (0.38268343236500002 * i2_63));
    tmpi5 = ((0.92387953251099997 * i2_63) - (0.38268343236500002 * r2_63));
    r1_215 = (r2_43 + tmpr5);
    i1_215 = (i2_43 + tmpi5);
    r1_18 = (r2_43 - tmpr5);
    i1_18 = (i2_43 - tmpi5);
    tmpr5 = (0.70710678118699999 * (r2_101 + i2_101));
    tmpi5 = (0.70710678118699999 * (i2_101 - r2_101));
    r1_43 = (r2_81 + tmpr5);
    i1_43 = (i2_81 + tmpi5);
    r1_20 = (r2_81 - tmpr5);
    i1_20 = (i2_81 - tmpi5);
    tmpr5 = ((0.38268343236500002 * r2_141) + (0.92387953251099997 * i2_141));
    tmpi5 = ((0.38268343236500002 * i2_141) - (0.92387953251099997 * r2_141));
    r1_63 = (r2_121 + tmpr5);
    i1_63 = (i2_121 + tmpi5);
    r1_22 = (r2_121 - tmpr5);
    i1_22 = (i2_121 - tmpi5);
    r1_81 = (r2_16 + i2_18);
    i1_81 = (i2_16 - r2_18);
    r1_24 = (r2_16 - i2_18);
    i1_24 = (i2_16 + r2_18);
    tmpr5 = ((0.92387953251099997 * i2_22) - (0.38268343236500002 * r2_22));
    tmpi5 = ((0.92387953251099997 * r2_22) + (0.38268343236500002 * i2_22));
    r1_101 = (r2_20 + tmpr5);
    i1_101 = (i2_20 - tmpi5);
    r1_26 = (r2_20 - tmpr5);
    i1_26 = (i2_20 + tmpi5);
    tmpr5 = (0.70710678118699999 * (i2_26 - r2_26));
    tmpi5 = (0.70710678118699999 * (r2_26 + i2_26));
    r1_121 = (r2_24 + tmpr5);
    i1_121 = (i2_24 - tmpi5);
    r1_28 = (r2_24 - tmpr5);
    i1_28 = (i2_24 + tmpi5);
    tmpr5 = ((0.38268343236500002 * i2_30) - (0.92387953251099997 * r2_30));
    tmpi5 = ((0.38268343236500002 * r2_30) + (0.92387953251099997 * i2_30));
    r1_141 = (r2_28 + tmpr5);
    i1_141 = (i2_28 - tmpi5);
    r1_30 = (r2_28 - tmpr5);
    i1_30 = (i2_28 + tmpi5);
    r5_1 = in14[1].re;
    i5_1 = in14[1].im;
    r5_17 = in14[17].re;
    i5_17 = in14[17].im;
    r4_112 = (r5_1 + r5_17);
    i4_112 = (i5_1 + i5_17);
    r4_17 = (r5_1 - r5_17);
    i4_17 = (i5_1 - i5_17);
    r5_9 = in14[9].re;
    i5_9 = in14[9].im;
    r5_25 = in14[25].re;
    i5_25 = in14[25].im;
    r4_91 = (r5_9 + r5_25);
    i4_91 = (i5_9 + i5_25);
    r4_25 = (r5_9 - r5_25);
    i4_25 = (i5_9 - i5_25);
    r3_114 = (r4_112 + r4_91);
    i3_114 = (i4_112 + i4_91);
    r3_17 = (r4_112 - r4_91);
    i3_17 = (i4_112 - i4_91);
    r3_91 = (r4_17 + i4_25);
    i3_91 = (i4_17 - r4_25);
    r3_25 = (r4_17 - i4_25);
    i3_25 = (i4_17 + r4_25);
    r5_5 = in14[5].re;
    i5_5 = in14[5].im;
    r5_21 = in14[21].re;
    i5_21 = in14[21].im;
    r4_51 = (r5_5 + r5_21);
    i4_51 = (i5_5 + i5_21);
    r4_21 = (r5_5 - r5_21);
    i4_21 = (i5_5 - i5_21);
    r5_13 = in14[13].re;
    i5_13 = in14[13].im;
    r5_29 = in14[29].re;
    i5_29 = in14[29].im;
    r4_131 = (r5_13 + r5_29);
    i4_131 = (i5_13 + i5_29);
    r4_29 = (r5_13 - r5_29);
    i4_29 = (i5_13 - i5_29);
    r3_53 = (r4_51 + r4_131);
    i3_53 = (i4_51 + i4_131);
    r3_21 = (r4_51 - r4_131);
    i3_21 = (i4_51 - i4_131);
    r3_131 = (r4_21 + i4_29);
    i3_131 = (i4_21 - r4_29);
    r3_29 = (r4_21 - i4_29);
    i3_29 = (i4_21 + r4_29);
    r2_116 = (r3_114 + r3_53);
    i2_116 = (i3_114 + i3_53);
    r2_17 = (r3_114 - r3_53);
    i2_17 = (i3_114 - i3_53);
    tmpr5 = (0.70710678118699999 * (r3_131 + i3_131));
    tmpi5 = (0.70710678118699999 * (i3_131 - r3_131));
    r2_53 = (r3_91 + tmpr5);
    i2_53 = (i3_91 + tmpi5);
    r2_21 = (r3_91 - tmpr5);
    i2_21 = (i3_91 - tmpi5);
    r2_91 = (r3_17 + i3_21);
    i2_91 = (i3_17 - r3_21);
    r2_25 = (r3_17 - i3_21);
    i2_25 = (i3_17 + r3_21);
    tmpr5 = (0.70710678118699999 * (i3_29 - r3_29));
    tmpi5 = (0.70710678118699999 * (r3_29 + i3_29));
    r2_131 = (r3_25 + tmpr5);
    i2_131 = (i3_25 - tmpi5);
    r2_29 = (r3_25 - tmpr5);
    i2_29 = (i3_25 + tmpi5);
    r5_3 = in14[3].re;
    i5_3 = in14[3].im;
    r5_19 = in14[19].re;
    i5_19 = in14[19].im;
    r4_33 = (r5_3 + r5_19);
    i4_33 = (i5_3 + i5_19);
    r4_19 = (r5_3 - r5_19);
    i4_19 = (i5_3 - i5_19);
    r5_11 = in14[11].re;
    i5_11 = in14[11].im;
    r5_27 = in14[27].re;
    i5_27 = in14[27].im;
    r4_113 = (r5_11 + r5_27);
    i4_113 = (i5_11 + i5_27);
    r4_27 = (r5_11 - r5_27);
    i4_27 = (i5_11 - i5_27);
    r3_35 = (r4_33 + r4_113);
    i3_35 = (i4_33 + i4_113);
    r3_19 = (r4_33 - r4_113);
    i3_19 = (i4_33 - i4_113);
    r3_115 = (r4_19 + i4_27);
    i3_115 = (i4_19 - r4_27);
    r3_27 = (r4_19 - i4_27);
    i3_27 = (i4_19 + r4_27);
    r5_7 = in14[7].re;
    i5_7 = in14[7].im;
    r5_23 = in14[23].re;
    i5_23 = in14[23].im;
    r4_71 = (r5_7 + r5_23);
    i4_71 = (i5_7 + i5_23);
    r4_23 = (r5_7 - r5_23);
    i4_23 = (i5_7 - i5_23);
    r5_15 = in14[15].re;
    i5_15 = in14[15].im;
    r5_31 = in14[31].re;
    i5_31 = in14[31].im;
    r4_151 = (r5_15 + r5_31);
    i4_151 = (i5_15 + i5_31);
    r4_31 = (r5_15 - r5_31);
    i4_31 = (i5_15 - i5_31);
    r3_73 = (r4_71 + r4_151);
    i3_73 = (i4_71 + i4_151);
    r3_23 = (r4_71 - r4_151);
    i3_23 = (i4_71 - i4_151);
    r3_151 = (r4_23 + i4_31);
    i3_151 = (i4_23 - r4_31);
    r3_31 = (r4_23 - i4_31);
    i3_31 = (i4_23 + r4_31);
    r2_37 = (r3_35 + r3_73);
    i2_37 = (i3_35 + i3_73);
    r2_19 = (r3_35 - r3_73);
    i2_19 = (i3_35 - i3_73);
    tmpr5 = (0.70710678118699999 * (r3_151 + i3_151));
    tmpi5 = (0.70710678118699999 * (i3_151 - r3_151));
    r2_73 = (r3_115 + tmpr5);
    i2_73 = (i3_115 + tmpi5);
    r2_23 = (r3_115 - tmpr5);
    i2_23 = (i3_115 - tmpi5);
    r2_117 = (r3_19 + i3_23);
    i2_117 = (i3_19 - r3_23);
    r2_27 = (r3_19 - i3_23);
    i2_27 = (i3_19 + r3_23);
    tmpr5 = (0.70710678118699999 * (i3_31 - r3_31));
    tmpi5 = (0.70710678118699999 * (r3_31 + i3_31));
    r2_151 = (r3_27 + tmpr5);
    i2_151 = (i3_27 - tmpi5);
    r2_31 = (r3_27 - tmpr5);
    i2_31 = (i3_27 + tmpi5);
    r1_118 = (r2_116 + r2_37);
    i1_118 = (i2_116 + i2_37);
    r1_17 = (r2_116 - r2_37);
    i1_17 = (i2_116 - i2_37);
    tmpr5 = ((0.92387953251099997 * r2_73) + (0.38268343236500002 * i2_73));
    tmpi5 = ((0.92387953251099997 * i2_73) - (0.38268343236500002 * r2_73));
    r1_37 = (r2_53 + tmpr5);
    i1_37 = (i2_53 + tmpi5);
    r1_19 = (r2_53 - tmpr5);
    i1_19 = (i2_53 - tmpi5);
    tmpr5 = (0.70710678118699999 * (r2_117 + i2_117));
    tmpi5 = (0.70710678118699999 * (i2_117 - r2_117));
    r1_53 = (r2_91 + tmpr5);
    i1_53 = (i2_91 + tmpi5);
    r1_21 = (r2_91 - tmpr5);
    i1_21 = (i2_91 - tmpi5);
    tmpr5 = ((0.38268343236500002 * r2_151) + (0.92387953251099997 * i2_151));
    tmpi5 = ((0.38268343236500002 * i2_151) - (0.92387953251099997 * r2_151));
    r1_73 = (r2_131 + tmpr5);
    i1_73 = (i2_131 + tmpi5);
    r1_23 = (r2_131 - tmpr5);
    i1_23 = (i2_131 - tmpi5);
    r1_91 = (r2_17 + i2_19);
    i1_91 = (i2_17 - r2_19);
    r1_25 = (r2_17 - i2_19);
    i1_25 = (i2_17 + r2_19);
    tmpr5 = ((0.92387953251099997 * i2_23) - (0.38268343236500002 * r2_23));
    tmpi5 = ((0.92387953251099997 * r2_23) + (0.38268343236500002 * i2_23));
    r1_119 = (r2_21 + tmpr5);
    i1_119 = (i2_21 - tmpi5);
    r1_27 = (r2_21 - tmpr5);
    i1_27 = (i2_21 + tmpi5);
    tmpr5 = (0.70710678118699999 * (i2_27 - r2_27));
    tmpi5 = (0.70710678118699999 * (r2_27 + i2_27));
    r1_131 = (r2_25 + tmpr5);
    i1_131 = (i2_25 - tmpi5);
    r1_29 = (r2_25 - tmpr5);
    i1_29 = (i2_25 + tmpi5);
    tmpr5 = ((0.38268343236500002 * i2_31) - (0.92387953251099997 * r2_31));
    tmpi5 = ((0.38268343236500002 * r2_31) + (0.92387953251099997 * i2_31));
    r1_151 = (r2_29 + tmpr5);
    i1_151 = (i2_29 - tmpi5);
    r1_31 = (r2_29 - tmpr5);
    i1_31 = (i2_29 + tmpi5);
    out14[0].re = (r1_07 + r1_118);
    out14[0].im = (i1_07 + i1_118);
    out14[16].re = (r1_07 - r1_118);
    out14[16].im = (i1_07 - i1_118);
    tmpr5 = ((0.98078528040299994 * r1_37) + (0.19509032201599999 * i1_37));
    tmpi5 = ((0.98078528040299994 * i1_37) - (0.19509032201599999 * r1_37));
    out14[1].re = (r1_215 + tmpr5);
    out14[1].im = (i1_215 + tmpi5);
    out14[17].re = (r1_215 - tmpr5);
    out14[17].im = (i1_215 - tmpi5);
    tmpr5 = ((0.92387953251099997 * r1_53) + (0.38268343236500002 * i1_53));
    tmpi5 = ((0.92387953251099997 * i1_53) - (0.38268343236500002 * r1_53));
    out14[2].re = (r1_43 + tmpr5);
    out14[2].im = (i1_43 + tmpi5);
    out14[18].re = (r1_43 - tmpr5);
    out14[18].im = (i1_43 - tmpi5);
    tmpr5 = ((0.83146961230299998 * r1_73) + (0.55557023301999997 * i1_73));
    tmpi5 = ((0.83146961230299998 * i1_73) - (0.55557023301999997 * r1_73));
    out14[3].re = (r1_63 + tmpr5);
    out14[3].im = (i1_63 + tmpi5);
    out14[19].re = (r1_63 - tmpr5);
    out14[19].im = (i1_63 - tmpi5);
    tmpr5 = (0.70710678118699999 * (r1_91 + i1_91));
    tmpi5 = (0.70710678118699999 * (i1_91 - r1_91));
    out14[4].re = (r1_81 + tmpr5);
    out14[4].im = (i1_81 + tmpi5);
    out14[20].re = (r1_81 - tmpr5);
    out14[20].im = (i1_81 - tmpi5);
    tmpr5 = ((0.55557023301999997 * r1_119) + (0.83146961230299998 * i1_119));
    tmpi5 = ((0.55557023301999997 * i1_119) - (0.83146961230299998 * r1_119));
    out14[5].re = (r1_101 + tmpr5);
    out14[5].im = (i1_101 + tmpi5);
    out14[21].re = (r1_101 - tmpr5);
    out14[21].im = (i1_101 - tmpi5);
    tmpr5 = ((0.38268343236500002 * r1_131) + (0.92387953251099997 * i1_131));
    tmpi5 = ((0.38268343236500002 * i1_131) - (0.92387953251099997 * r1_131));
    out14[6].re = (r1_121 + tmpr5);
    out14[6].im = (i1_121 + tmpi5);
    out14[22].re = (r1_121 - tmpr5);
    out14[22].im = (i1_121 - tmpi5);
    tmpr5 = ((0.19509032201599999 * r1_151) + (0.98078528040299994 * i1_151));
    tmpi5 = ((0.19509032201599999 * i1_151) - (0.98078528040299994 * r1_151));
    out14[7].re = (r1_141 + tmpr5);
    out14[7].im = (i1_141 + tmpi5);
    out14[23].re = (r1_141 - tmpr5);
    out14[23].im = (i1_141 - tmpi5);
    out14[8].re = (r1_16 + i1_17);
    out14[8].im = (i1_16 - r1_17);
    out14[24].re = (r1_16 - i1_17);
    out14[24].im = (i1_16 + r1_17);
    tmpr5 = ((0.98078528040299994 * i1_19) - (0.19509032201599999 * r1_19));
    tmpi5 = ((0.98078528040299994 * r1_19) + (0.19509032201599999 * i1_19));
    out14[9].re = (r1_18 + tmpr5);
    out14[9].im = (i1_18 - tmpi5);
    out14[25].re = (r1_18 - tmpr5);
    out14[25].im = (i1_18 + tmpi5);
    tmpr5 = ((0.92387953251099997 * i1_21) - (0.38268343236500002 * r1_21));
    tmpi5 = ((0.92387953251099997 * r1_21) + (0.38268343236500002 * i1_21));
    out14[10].re = (r1_20 + tmpr5);
    out14[10].im = (i1_20 - tmpi5);
    out14[26].re = (r1_20 - tmpr5);
    out14[26].im = (i1_20 + tmpi5);
    tmpr5 = ((0.83146961230299998 * i1_23) - (0.55557023301999997 * r1_23));
    tmpi5 = ((0.83146961230299998 * r1_23) + (0.55557023301999997 * i1_23));
    out14[11].re = (r1_22 + tmpr5);
    out14[11].im = (i1_22 - tmpi5);
    out14[27].re = (r1_22 - tmpr5);
    out14[27].im = (i1_22 + tmpi5);
    tmpr5 = (0.70710678118699999 * (i1_25 - r1_25));
    tmpi5 = (0.70710678118699999 * (r1_25 + i1_25));
    out14[12].re = (r1_24 + tmpr5);
    out14[12].im = (i1_24 - tmpi5);
    out14[28].re = (r1_24 - tmpr5);
    out14[28].im = (i1_24 + tmpi5);
    tmpr5 = ((0.55557023301999997 * i1_27) - (0.83146961230299998 * r1_27));
    tmpi5 = ((0.55557023301999997 * r1_27) + (0.83146961230299998 * i1_27));
    out14[13].re = (r1_26 + tmpr5);
    out14[13].im = (i1_26 - tmpi5);
    out14[29].re = (r1_26 - tmpr5);
    out14[29].im = (i1_26 + tmpi5);
    tmpr5 = ((0.38268343236500002 * i1_29) - (0.92387953251099997 * r1_29));
    tmpi5 = ((0.38268343236500002 * r1_29) + (0.92387953251099997 * i1_29));
    out14[14].re = (r1_28 + tmpr5);
    out14[14].im = (i1_28 - tmpi5);
    out14[30].re = (r1_28 - tmpr5);
    out14[30].im = (i1_28 + tmpi5);
    tmpr5 = ((0.19509032201599999 * i1_31) - (0.98078528040299994 * r1_31));
    tmpi5 = ((0.19509032201599999 * r1_31) + (0.98078528040299994 * i1_31));
    out14[15].re = (r1_30 + tmpr5);
    out14[15].im = (i1_30 - tmpi5);
    out14[31].re = (r1_30 - tmpr5);
    out14[31].im = (i1_30 + tmpi5);
}
THREAD(fft_twiddle_32) {
    int l14;
    int i12;
    COMPLEX *jp9;
    COMPLEX *kp4;
    REAL tmpr6;
    REAL tmpi6;
    REAL wr3;
    REAL wi3;
    REAL r1_08;
    REAL i1_08;
    REAL r1_122;
    REAL i1_122;
    REAL r1_216;
    REAL i1_216;
    REAL r1_38;
    REAL i1_38;
    REAL r1_44;
    REAL i1_44;
    REAL r1_54;
    REAL i1_54;
    REAL r1_64;
    REAL i1_64;
    REAL r1_74;
    REAL i1_74;
    REAL r1_82;
    REAL i1_82;
    REAL r1_92;
    REAL i1_92;
    REAL r1_102;
    REAL i1_102;
    REAL r1_1110;
    REAL i1_1110;
    REAL r1_123;
    REAL i1_123;
    REAL r1_132;
    REAL i1_132;
    REAL r1_142;
    REAL i1_142;
    REAL r1_152;
    REAL i1_152;
    REAL r1_160;
    REAL i1_160;
    REAL r1_170;
    REAL i1_170;
    REAL r1_180;
    REAL i1_180;
    REAL r1_190;
    REAL i1_190;
    REAL r1_200;
    REAL i1_200;
    REAL r1_217;
    REAL i1_217;
    REAL r1_220;
    REAL i1_220;
    REAL r1_230;
    REAL i1_230;
    REAL r1_240;
    REAL i1_240;
    REAL r1_250;
    REAL i1_250;
    REAL r1_260;
    REAL i1_260;
    REAL r1_270;
    REAL i1_270;
    REAL r1_280;
    REAL i1_280;
    REAL r1_290;
    REAL i1_290;
    REAL r1_300;
    REAL i1_300;
    REAL r1_310;
    REAL i1_310;
    REAL r2_06;
    REAL i2_06;
    REAL r2_216;
    REAL i2_216;
    REAL r2_44;
    REAL i2_44;
    REAL r2_64;
    REAL i2_64;
    REAL r2_82;
    REAL i2_82;
    REAL r2_102;
    REAL i2_102;
    REAL r2_122;
    REAL i2_122;
    REAL r2_142;
    REAL i2_142;
    REAL r2_160;
    REAL i2_160;
    REAL r2_180;
    REAL i2_180;
    REAL r2_200;
    REAL i2_200;
    REAL r2_220;
    REAL i2_220;
    REAL r2_240;
    REAL i2_240;
    REAL r2_260;
    REAL i2_260;
    REAL r2_280;
    REAL i2_280;
    REAL r2_300;
    REAL i2_300;
    REAL r3_04;
    REAL i3_04;
    REAL r3_44;
    REAL i3_44;
    REAL r3_82;
    REAL i3_82;
    REAL r3_122;
    REAL i3_122;
    REAL r3_160;
    REAL i3_160;
    REAL r3_200;
    REAL i3_200;
    REAL r3_240;
    REAL i3_240;
    REAL r3_280;
    REAL i3_280;
    REAL r4_02;
    REAL i4_02;
    REAL r4_82;
    REAL i4_82;
    REAL r4_160;
    REAL i4_160;
    REAL r4_240;
    REAL i4_240;
    REAL r5_00;
    REAL i5_00;
    REAL r5_160;
    REAL i5_160;
    REAL r5_80;
    REAL i5_80;
    REAL r5_240;
    REAL i5_240;
    REAL r4_42;
    REAL i4_42;
    REAL r4_122;
    REAL i4_122;
    REAL r4_200;
    REAL i4_200;
    REAL r4_280;
    REAL i4_280;
    REAL r5_40;
    REAL i5_40;
    REAL r5_200;
    REAL i5_200;
    REAL r5_120;
    REAL i5_120;
    REAL r5_280;
    REAL i5_280;
    REAL r3_214;
    REAL i3_214;
    REAL r3_64;
    REAL i3_64;
    REAL r3_102;
    REAL i3_102;
    REAL r3_142;
    REAL i3_142;
    REAL r3_180;
    REAL i3_180;
    REAL r3_220;
    REAL i3_220;
    REAL r3_260;
    REAL i3_260;
    REAL r3_300;
    REAL i3_300;
    REAL r4_212;
    REAL i4_212;
    REAL r4_102;
    REAL i4_102;
    REAL r4_180;
    REAL i4_180;
    REAL r4_260;
    REAL i4_260;
    REAL r5_210;
    REAL i5_210;
    REAL r5_180;
    REAL i5_180;
    REAL r5_100;
    REAL i5_100;
    REAL r5_260;
    REAL i5_260;
    REAL r4_62;
    REAL i4_62;
    REAL r4_142;
    REAL i4_142;
    REAL r4_220;
    REAL i4_220;
    REAL r4_300;
    REAL i4_300;
    REAL r5_60;
    REAL i5_60;
    REAL r5_220;
    REAL i5_220;
    REAL r5_140;
    REAL i5_140;
    REAL r5_300;
    REAL i5_300;
    REAL r2_118;
    REAL i2_118;
    REAL r2_38;
    REAL i2_38;
    REAL r2_54;
    REAL i2_54;
    REAL r2_74;
    REAL i2_74;
    REAL r2_92;
    REAL i2_92;
    REAL r2_119;
    REAL i2_119;
    REAL r2_132;
    REAL i2_132;
    REAL r2_152;
    REAL i2_152;
    REAL r2_170;
    REAL i2_170;
    REAL r2_190;
    REAL i2_190;
    REAL r2_217;
    REAL i2_217;
    REAL r2_230;
    REAL i2_230;
    REAL r2_250;
    REAL i2_250;
    REAL r2_270;
    REAL i2_270;
    REAL r2_290;
    REAL i2_290;
    REAL r2_310;
    REAL i2_310;
    REAL r3_116;
    REAL i3_116;
    REAL r3_54;
    REAL i3_54;
    REAL r3_92;
    REAL i3_92;
    REAL r3_132;
    REAL i3_132;
    REAL r3_170;
    REAL i3_170;
    REAL r3_215;
    REAL i3_215;
    REAL r3_250;
    REAL i3_250;
    REAL r3_290;
    REAL i3_290;
    REAL r4_114;
    REAL i4_114;
    REAL r4_92;
    REAL i4_92;
    REAL r4_170;
    REAL i4_170;
    REAL r4_250;
    REAL i4_250;
    REAL r5_110;
    REAL i5_110;
    REAL r5_170;
    REAL i5_170;
    REAL r5_90;
    REAL i5_90;
    REAL r5_250;
    REAL i5_250;
    REAL r4_52;
    REAL i4_52;
    REAL r4_132;
    REAL i4_132;
    REAL r4_213;
    REAL i4_213;
    REAL r4_290;
    REAL i4_290;
    REAL r5_50;
    REAL i5_50;
    REAL r5_211;
    REAL i5_211;
    REAL r5_130;
    REAL i5_130;
    REAL r5_290;
    REAL i5_290;
    REAL r3_36;
    REAL i3_36;
    REAL r3_74;
    REAL i3_74;
    REAL r3_117;
    REAL i3_117;
    REAL r3_152;
    REAL i3_152;
    REAL r3_190;
    REAL i3_190;
    REAL r3_230;
    REAL i3_230;
    REAL r3_270;
    REAL i3_270;
    REAL r3_310;
    REAL i3_310;
    REAL r4_34;
    REAL i4_34;
    REAL r4_115;
    REAL i4_115;
    REAL r4_190;
    REAL i4_190;
    REAL r4_270;
    REAL i4_270;
    REAL r5_32;
    REAL i5_32;
    REAL r5_190;
    REAL i5_190;
    REAL r5_111;
    REAL i5_111;
    REAL r5_270;
    REAL i5_270;
    REAL r4_72;
    REAL i4_72;
    REAL r4_152;
    REAL i4_152;
    REAL r4_230;
    REAL i4_230;
    REAL r4_310;
    REAL i4_310;
    REAL r5_70;
    REAL i5_70;
    REAL r5_230;
    REAL i5_230;
    REAL r5_150;
    REAL i5_150;
    REAL r5_310;
    REAL i5_310;
    int ab9;
    fft_twiddle_32_closure *largs = (fft_twiddle_32_closure*)(args.get());
    if (((largs->b9 - largs->a9) < 128)) {
        i12 = largs->a9;
        l14 = (largs->nWdn4 * i12);
        for (kp4 = (largs->out15 + i12);(i12 < largs->b9);(kp4++)) {
            jp9 = (largs->in15 + i12);
            r5_00 = jp9[(0 * largs->m10)].re;
            i5_00 = jp9[(0 * largs->m10)].im;
            wr3 = largs->W6[(16 * l14)].re;
            wi3 = largs->W6[(16 * l14)].im;
            tmpr6 = jp9[(16 * largs->m10)].re;
            tmpi6 = jp9[(16 * largs->m10)].im;
            r5_160 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_160 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_02 = (r5_00 + r5_160);
            i4_02 = (i5_00 + i5_160);
            r4_160 = (r5_00 - r5_160);
            i4_160 = (i5_00 - i5_160);
            wr3 = largs->W6[(8 * l14)].re;
            wi3 = largs->W6[(8 * l14)].im;
            tmpr6 = jp9[(8 * largs->m10)].re;
            tmpi6 = jp9[(8 * largs->m10)].im;
            r5_80 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_80 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(24 * l14)].re;
            wi3 = largs->W6[(24 * l14)].im;
            tmpr6 = jp9[(24 * largs->m10)].re;
            tmpi6 = jp9[(24 * largs->m10)].im;
            r5_240 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_240 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_82 = (r5_80 + r5_240);
            i4_82 = (i5_80 + i5_240);
            r4_240 = (r5_80 - r5_240);
            i4_240 = (i5_80 - i5_240);
            r3_04 = (r4_02 + r4_82);
            i3_04 = (i4_02 + i4_82);
            r3_160 = (r4_02 - r4_82);
            i3_160 = (i4_02 - i4_82);
            r3_82 = (r4_160 + i4_240);
            i3_82 = (i4_160 - r4_240);
            r3_240 = (r4_160 - i4_240);
            i3_240 = (i4_160 + r4_240);
            wr3 = largs->W6[(4 * l14)].re;
            wi3 = largs->W6[(4 * l14)].im;
            tmpr6 = jp9[(4 * largs->m10)].re;
            tmpi6 = jp9[(4 * largs->m10)].im;
            r5_40 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_40 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(20 * l14)].re;
            wi3 = largs->W6[(20 * l14)].im;
            tmpr6 = jp9[(20 * largs->m10)].re;
            tmpi6 = jp9[(20 * largs->m10)].im;
            r5_200 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_200 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_42 = (r5_40 + r5_200);
            i4_42 = (i5_40 + i5_200);
            r4_200 = (r5_40 - r5_200);
            i4_200 = (i5_40 - i5_200);
            wr3 = largs->W6[(12 * l14)].re;
            wi3 = largs->W6[(12 * l14)].im;
            tmpr6 = jp9[(12 * largs->m10)].re;
            tmpi6 = jp9[(12 * largs->m10)].im;
            r5_120 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_120 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(28 * l14)].re;
            wi3 = largs->W6[(28 * l14)].im;
            tmpr6 = jp9[(28 * largs->m10)].re;
            tmpi6 = jp9[(28 * largs->m10)].im;
            r5_280 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_280 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_122 = (r5_120 + r5_280);
            i4_122 = (i5_120 + i5_280);
            r4_280 = (r5_120 - r5_280);
            i4_280 = (i5_120 - i5_280);
            r3_44 = (r4_42 + r4_122);
            i3_44 = (i4_42 + i4_122);
            r3_200 = (r4_42 - r4_122);
            i3_200 = (i4_42 - i4_122);
            r3_122 = (r4_200 + i4_280);
            i3_122 = (i4_200 - r4_280);
            r3_280 = (r4_200 - i4_280);
            i3_280 = (i4_200 + r4_280);
            r2_06 = (r3_04 + r3_44);
            i2_06 = (i3_04 + i3_44);
            r2_160 = (r3_04 - r3_44);
            i2_160 = (i3_04 - i3_44);
            tmpr6 = (0.70710678118699999 * (r3_122 + i3_122));
            tmpi6 = (0.70710678118699999 * (i3_122 - r3_122));
            r2_44 = (r3_82 + tmpr6);
            i2_44 = (i3_82 + tmpi6);
            r2_200 = (r3_82 - tmpr6);
            i2_200 = (i3_82 - tmpi6);
            r2_82 = (r3_160 + i3_200);
            i2_82 = (i3_160 - r3_200);
            r2_240 = (r3_160 - i3_200);
            i2_240 = (i3_160 + r3_200);
            tmpr6 = (0.70710678118699999 * (i3_280 - r3_280));
            tmpi6 = (0.70710678118699999 * (r3_280 + i3_280));
            r2_122 = (r3_240 + tmpr6);
            i2_122 = (i3_240 - tmpi6);
            r2_280 = (r3_240 - tmpr6);
            i2_280 = (i3_240 + tmpi6);
            wr3 = largs->W6[(2 * l14)].re;
            wi3 = largs->W6[(2 * l14)].im;
            tmpr6 = jp9[(2 * largs->m10)].re;
            tmpi6 = jp9[(2 * largs->m10)].im;
            r5_210 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_210 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(18 * l14)].re;
            wi3 = largs->W6[(18 * l14)].im;
            tmpr6 = jp9[(18 * largs->m10)].re;
            tmpi6 = jp9[(18 * largs->m10)].im;
            r5_180 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_180 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_212 = (r5_210 + r5_180);
            i4_212 = (i5_210 + i5_180);
            r4_180 = (r5_210 - r5_180);
            i4_180 = (i5_210 - i5_180);
            wr3 = largs->W6[(10 * l14)].re;
            wi3 = largs->W6[(10 * l14)].im;
            tmpr6 = jp9[(10 * largs->m10)].re;
            tmpi6 = jp9[(10 * largs->m10)].im;
            r5_100 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_100 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(26 * l14)].re;
            wi3 = largs->W6[(26 * l14)].im;
            tmpr6 = jp9[(26 * largs->m10)].re;
            tmpi6 = jp9[(26 * largs->m10)].im;
            r5_260 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_260 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_102 = (r5_100 + r5_260);
            i4_102 = (i5_100 + i5_260);
            r4_260 = (r5_100 - r5_260);
            i4_260 = (i5_100 - i5_260);
            r3_214 = (r4_212 + r4_102);
            i3_214 = (i4_212 + i4_102);
            r3_180 = (r4_212 - r4_102);
            i3_180 = (i4_212 - i4_102);
            r3_102 = (r4_180 + i4_260);
            i3_102 = (i4_180 - r4_260);
            r3_260 = (r4_180 - i4_260);
            i3_260 = (i4_180 + r4_260);
            wr3 = largs->W6[(6 * l14)].re;
            wi3 = largs->W6[(6 * l14)].im;
            tmpr6 = jp9[(6 * largs->m10)].re;
            tmpi6 = jp9[(6 * largs->m10)].im;
            r5_60 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_60 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(22 * l14)].re;
            wi3 = largs->W6[(22 * l14)].im;
            tmpr6 = jp9[(22 * largs->m10)].re;
            tmpi6 = jp9[(22 * largs->m10)].im;
            r5_220 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_220 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_62 = (r5_60 + r5_220);
            i4_62 = (i5_60 + i5_220);
            r4_220 = (r5_60 - r5_220);
            i4_220 = (i5_60 - i5_220);
            wr3 = largs->W6[(14 * l14)].re;
            wi3 = largs->W6[(14 * l14)].im;
            tmpr6 = jp9[(14 * largs->m10)].re;
            tmpi6 = jp9[(14 * largs->m10)].im;
            r5_140 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_140 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(30 * l14)].re;
            wi3 = largs->W6[(30 * l14)].im;
            tmpr6 = jp9[(30 * largs->m10)].re;
            tmpi6 = jp9[(30 * largs->m10)].im;
            r5_300 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_300 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_142 = (r5_140 + r5_300);
            i4_142 = (i5_140 + i5_300);
            r4_300 = (r5_140 - r5_300);
            i4_300 = (i5_140 - i5_300);
            r3_64 = (r4_62 + r4_142);
            i3_64 = (i4_62 + i4_142);
            r3_220 = (r4_62 - r4_142);
            i3_220 = (i4_62 - i4_142);
            r3_142 = (r4_220 + i4_300);
            i3_142 = (i4_220 - r4_300);
            r3_300 = (r4_220 - i4_300);
            i3_300 = (i4_220 + r4_300);
            r2_216 = (r3_214 + r3_64);
            i2_216 = (i3_214 + i3_64);
            r2_180 = (r3_214 - r3_64);
            i2_180 = (i3_214 - i3_64);
            tmpr6 = (0.70710678118699999 * (r3_142 + i3_142));
            tmpi6 = (0.70710678118699999 * (i3_142 - r3_142));
            r2_64 = (r3_102 + tmpr6);
            i2_64 = (i3_102 + tmpi6);
            r2_220 = (r3_102 - tmpr6);
            i2_220 = (i3_102 - tmpi6);
            r2_102 = (r3_180 + i3_220);
            i2_102 = (i3_180 - r3_220);
            r2_260 = (r3_180 - i3_220);
            i2_260 = (i3_180 + r3_220);
            tmpr6 = (0.70710678118699999 * (i3_300 - r3_300));
            tmpi6 = (0.70710678118699999 * (r3_300 + i3_300));
            r2_142 = (r3_260 + tmpr6);
            i2_142 = (i3_260 - tmpi6);
            r2_300 = (r3_260 - tmpr6);
            i2_300 = (i3_260 + tmpi6);
            r1_08 = (r2_06 + r2_216);
            i1_08 = (i2_06 + i2_216);
            r1_160 = (r2_06 - r2_216);
            i1_160 = (i2_06 - i2_216);
            tmpr6 = ((0.92387953251099997 * r2_64) + (0.38268343236500002 * i2_64));
            tmpi6 = ((0.92387953251099997 * i2_64) - (0.38268343236500002 * r2_64));
            r1_216 = (r2_44 + tmpr6);
            i1_216 = (i2_44 + tmpi6);
            r1_180 = (r2_44 - tmpr6);
            i1_180 = (i2_44 - tmpi6);
            tmpr6 = (0.70710678118699999 * (r2_102 + i2_102));
            tmpi6 = (0.70710678118699999 * (i2_102 - r2_102));
            r1_44 = (r2_82 + tmpr6);
            i1_44 = (i2_82 + tmpi6);
            r1_200 = (r2_82 - tmpr6);
            i1_200 = (i2_82 - tmpi6);
            tmpr6 = ((0.38268343236500002 * r2_142) + (0.92387953251099997 * i2_142));
            tmpi6 = ((0.38268343236500002 * i2_142) - (0.92387953251099997 * r2_142));
            r1_64 = (r2_122 + tmpr6);
            i1_64 = (i2_122 + tmpi6);
            r1_220 = (r2_122 - tmpr6);
            i1_220 = (i2_122 - tmpi6);
            r1_82 = (r2_160 + i2_180);
            i1_82 = (i2_160 - r2_180);
            r1_240 = (r2_160 - i2_180);
            i1_240 = (i2_160 + r2_180);
            tmpr6 = ((0.92387953251099997 * i2_220) - (0.38268343236500002 * r2_220));
            tmpi6 = ((0.92387953251099997 * r2_220) + (0.38268343236500002 * i2_220));
            r1_102 = (r2_200 + tmpr6);
            i1_102 = (i2_200 - tmpi6);
            r1_260 = (r2_200 - tmpr6);
            i1_260 = (i2_200 + tmpi6);
            tmpr6 = (0.70710678118699999 * (i2_260 - r2_260));
            tmpi6 = (0.70710678118699999 * (r2_260 + i2_260));
            r1_123 = (r2_240 + tmpr6);
            i1_123 = (i2_240 - tmpi6);
            r1_280 = (r2_240 - tmpr6);
            i1_280 = (i2_240 + tmpi6);
            tmpr6 = ((0.38268343236500002 * i2_300) - (0.92387953251099997 * r2_300));
            tmpi6 = ((0.38268343236500002 * r2_300) + (0.92387953251099997 * i2_300));
            r1_142 = (r2_280 + tmpr6);
            i1_142 = (i2_280 - tmpi6);
            r1_300 = (r2_280 - tmpr6);
            i1_300 = (i2_280 + tmpi6);
            wr3 = largs->W6[(1 * l14)].re;
            wi3 = largs->W6[(1 * l14)].im;
            tmpr6 = jp9[(1 * largs->m10)].re;
            tmpi6 = jp9[(1 * largs->m10)].im;
            r5_110 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_110 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(17 * l14)].re;
            wi3 = largs->W6[(17 * l14)].im;
            tmpr6 = jp9[(17 * largs->m10)].re;
            tmpi6 = jp9[(17 * largs->m10)].im;
            r5_170 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_170 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_114 = (r5_110 + r5_170);
            i4_114 = (i5_110 + i5_170);
            r4_170 = (r5_110 - r5_170);
            i4_170 = (i5_110 - i5_170);
            wr3 = largs->W6[(9 * l14)].re;
            wi3 = largs->W6[(9 * l14)].im;
            tmpr6 = jp9[(9 * largs->m10)].re;
            tmpi6 = jp9[(9 * largs->m10)].im;
            r5_90 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_90 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(25 * l14)].re;
            wi3 = largs->W6[(25 * l14)].im;
            tmpr6 = jp9[(25 * largs->m10)].re;
            tmpi6 = jp9[(25 * largs->m10)].im;
            r5_250 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_250 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_92 = (r5_90 + r5_250);
            i4_92 = (i5_90 + i5_250);
            r4_250 = (r5_90 - r5_250);
            i4_250 = (i5_90 - i5_250);
            r3_116 = (r4_114 + r4_92);
            i3_116 = (i4_114 + i4_92);
            r3_170 = (r4_114 - r4_92);
            i3_170 = (i4_114 - i4_92);
            r3_92 = (r4_170 + i4_250);
            i3_92 = (i4_170 - r4_250);
            r3_250 = (r4_170 - i4_250);
            i3_250 = (i4_170 + r4_250);
            wr3 = largs->W6[(5 * l14)].re;
            wi3 = largs->W6[(5 * l14)].im;
            tmpr6 = jp9[(5 * largs->m10)].re;
            tmpi6 = jp9[(5 * largs->m10)].im;
            r5_50 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_50 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(21 * l14)].re;
            wi3 = largs->W6[(21 * l14)].im;
            tmpr6 = jp9[(21 * largs->m10)].re;
            tmpi6 = jp9[(21 * largs->m10)].im;
            r5_211 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_211 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_52 = (r5_50 + r5_211);
            i4_52 = (i5_50 + i5_211);
            r4_213 = (r5_50 - r5_211);
            i4_213 = (i5_50 - i5_211);
            wr3 = largs->W6[(13 * l14)].re;
            wi3 = largs->W6[(13 * l14)].im;
            tmpr6 = jp9[(13 * largs->m10)].re;
            tmpi6 = jp9[(13 * largs->m10)].im;
            r5_130 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_130 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(29 * l14)].re;
            wi3 = largs->W6[(29 * l14)].im;
            tmpr6 = jp9[(29 * largs->m10)].re;
            tmpi6 = jp9[(29 * largs->m10)].im;
            r5_290 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_290 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_132 = (r5_130 + r5_290);
            i4_132 = (i5_130 + i5_290);
            r4_290 = (r5_130 - r5_290);
            i4_290 = (i5_130 - i5_290);
            r3_54 = (r4_52 + r4_132);
            i3_54 = (i4_52 + i4_132);
            r3_215 = (r4_52 - r4_132);
            i3_215 = (i4_52 - i4_132);
            r3_132 = (r4_213 + i4_290);
            i3_132 = (i4_213 - r4_290);
            r3_290 = (r4_213 - i4_290);
            i3_290 = (i4_213 + r4_290);
            r2_118 = (r3_116 + r3_54);
            i2_118 = (i3_116 + i3_54);
            r2_170 = (r3_116 - r3_54);
            i2_170 = (i3_116 - i3_54);
            tmpr6 = (0.70710678118699999 * (r3_132 + i3_132));
            tmpi6 = (0.70710678118699999 * (i3_132 - r3_132));
            r2_54 = (r3_92 + tmpr6);
            i2_54 = (i3_92 + tmpi6);
            r2_217 = (r3_92 - tmpr6);
            i2_217 = (i3_92 - tmpi6);
            r2_92 = (r3_170 + i3_215);
            i2_92 = (i3_170 - r3_215);
            r2_250 = (r3_170 - i3_215);
            i2_250 = (i3_170 + r3_215);
            tmpr6 = (0.70710678118699999 * (i3_290 - r3_290));
            tmpi6 = (0.70710678118699999 * (r3_290 + i3_290));
            r2_132 = (r3_250 + tmpr6);
            i2_132 = (i3_250 - tmpi6);
            r2_290 = (r3_250 - tmpr6);
            i2_290 = (i3_250 + tmpi6);
            wr3 = largs->W6[(3 * l14)].re;
            wi3 = largs->W6[(3 * l14)].im;
            tmpr6 = jp9[(3 * largs->m10)].re;
            tmpi6 = jp9[(3 * largs->m10)].im;
            r5_32 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_32 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(19 * l14)].re;
            wi3 = largs->W6[(19 * l14)].im;
            tmpr6 = jp9[(19 * largs->m10)].re;
            tmpi6 = jp9[(19 * largs->m10)].im;
            r5_190 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_190 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_34 = (r5_32 + r5_190);
            i4_34 = (i5_32 + i5_190);
            r4_190 = (r5_32 - r5_190);
            i4_190 = (i5_32 - i5_190);
            wr3 = largs->W6[(11 * l14)].re;
            wi3 = largs->W6[(11 * l14)].im;
            tmpr6 = jp9[(11 * largs->m10)].re;
            tmpi6 = jp9[(11 * largs->m10)].im;
            r5_111 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_111 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(27 * l14)].re;
            wi3 = largs->W6[(27 * l14)].im;
            tmpr6 = jp9[(27 * largs->m10)].re;
            tmpi6 = jp9[(27 * largs->m10)].im;
            r5_270 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_270 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_115 = (r5_111 + r5_270);
            i4_115 = (i5_111 + i5_270);
            r4_270 = (r5_111 - r5_270);
            i4_270 = (i5_111 - i5_270);
            r3_36 = (r4_34 + r4_115);
            i3_36 = (i4_34 + i4_115);
            r3_190 = (r4_34 - r4_115);
            i3_190 = (i4_34 - i4_115);
            r3_117 = (r4_190 + i4_270);
            i3_117 = (i4_190 - r4_270);
            r3_270 = (r4_190 - i4_270);
            i3_270 = (i4_190 + r4_270);
            wr3 = largs->W6[(7 * l14)].re;
            wi3 = largs->W6[(7 * l14)].im;
            tmpr6 = jp9[(7 * largs->m10)].re;
            tmpi6 = jp9[(7 * largs->m10)].im;
            r5_70 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_70 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(23 * l14)].re;
            wi3 = largs->W6[(23 * l14)].im;
            tmpr6 = jp9[(23 * largs->m10)].re;
            tmpi6 = jp9[(23 * largs->m10)].im;
            r5_230 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_230 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_72 = (r5_70 + r5_230);
            i4_72 = (i5_70 + i5_230);
            r4_230 = (r5_70 - r5_230);
            i4_230 = (i5_70 - i5_230);
            wr3 = largs->W6[(15 * l14)].re;
            wi3 = largs->W6[(15 * l14)].im;
            tmpr6 = jp9[(15 * largs->m10)].re;
            tmpi6 = jp9[(15 * largs->m10)].im;
            r5_150 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_150 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            wr3 = largs->W6[(31 * l14)].re;
            wi3 = largs->W6[(31 * l14)].im;
            tmpr6 = jp9[(31 * largs->m10)].re;
            tmpi6 = jp9[(31 * largs->m10)].im;
            r5_310 = ((wr3 * tmpr6) - (wi3 * tmpi6));
            i5_310 = ((wi3 * tmpr6) + (wr3 * tmpi6));
            r4_152 = (r5_150 + r5_310);
            i4_152 = (i5_150 + i5_310);
            r4_310 = (r5_150 - r5_310);
            i4_310 = (i5_150 - i5_310);
            r3_74 = (r4_72 + r4_152);
            i3_74 = (i4_72 + i4_152);
            r3_230 = (r4_72 - r4_152);
            i3_230 = (i4_72 - i4_152);
            r3_152 = (r4_230 + i4_310);
            i3_152 = (i4_230 - r4_310);
            r3_310 = (r4_230 - i4_310);
            i3_310 = (i4_230 + r4_310);
            r2_38 = (r3_36 + r3_74);
            i2_38 = (i3_36 + i3_74);
            r2_190 = (r3_36 - r3_74);
            i2_190 = (i3_36 - i3_74);
            tmpr6 = (0.70710678118699999 * (r3_152 + i3_152));
            tmpi6 = (0.70710678118699999 * (i3_152 - r3_152));
            r2_74 = (r3_117 + tmpr6);
            i2_74 = (i3_117 + tmpi6);
            r2_230 = (r3_117 - tmpr6);
            i2_230 = (i3_117 - tmpi6);
            r2_119 = (r3_190 + i3_230);
            i2_119 = (i3_190 - r3_230);
            r2_270 = (r3_190 - i3_230);
            i2_270 = (i3_190 + r3_230);
            tmpr6 = (0.70710678118699999 * (i3_310 - r3_310));
            tmpi6 = (0.70710678118699999 * (r3_310 + i3_310));
            r2_152 = (r3_270 + tmpr6);
            i2_152 = (i3_270 - tmpi6);
            r2_310 = (r3_270 - tmpr6);
            i2_310 = (i3_270 + tmpi6);
            r1_122 = (r2_118 + r2_38);
            i1_122 = (i2_118 + i2_38);
            r1_170 = (r2_118 - r2_38);
            i1_170 = (i2_118 - i2_38);
            tmpr6 = ((0.92387953251099997 * r2_74) + (0.38268343236500002 * i2_74));
            tmpi6 = ((0.92387953251099997 * i2_74) - (0.38268343236500002 * r2_74));
            r1_38 = (r2_54 + tmpr6);
            i1_38 = (i2_54 + tmpi6);
            r1_190 = (r2_54 - tmpr6);
            i1_190 = (i2_54 - tmpi6);
            tmpr6 = (0.70710678118699999 * (r2_119 + i2_119));
            tmpi6 = (0.70710678118699999 * (i2_119 - r2_119));
            r1_54 = (r2_92 + tmpr6);
            i1_54 = (i2_92 + tmpi6);
            r1_217 = (r2_92 - tmpr6);
            i1_217 = (i2_92 - tmpi6);
            tmpr6 = ((0.38268343236500002 * r2_152) + (0.92387953251099997 * i2_152));
            tmpi6 = ((0.38268343236500002 * i2_152) - (0.92387953251099997 * r2_152));
            r1_74 = (r2_132 + tmpr6);
            i1_74 = (i2_132 + tmpi6);
            r1_230 = (r2_132 - tmpr6);
            i1_230 = (i2_132 - tmpi6);
            r1_92 = (r2_170 + i2_190);
            i1_92 = (i2_170 - r2_190);
            r1_250 = (r2_170 - i2_190);
            i1_250 = (i2_170 + r2_190);
            tmpr6 = ((0.92387953251099997 * i2_230) - (0.38268343236500002 * r2_230));
            tmpi6 = ((0.92387953251099997 * r2_230) + (0.38268343236500002 * i2_230));
            r1_1110 = (r2_217 + tmpr6);
            i1_1110 = (i2_217 - tmpi6);
            r1_270 = (r2_217 - tmpr6);
            i1_270 = (i2_217 + tmpi6);
            tmpr6 = (0.70710678118699999 * (i2_270 - r2_270));
            tmpi6 = (0.70710678118699999 * (r2_270 + i2_270));
            r1_132 = (r2_250 + tmpr6);
            i1_132 = (i2_250 - tmpi6);
            r1_290 = (r2_250 - tmpr6);
            i1_290 = (i2_250 + tmpi6);
            tmpr6 = ((0.38268343236500002 * i2_310) - (0.92387953251099997 * r2_310));
            tmpi6 = ((0.38268343236500002 * r2_310) + (0.92387953251099997 * i2_310));
            r1_152 = (r2_290 + tmpr6);
            i1_152 = (i2_290 - tmpi6);
            r1_310 = (r2_290 - tmpr6);
            i1_310 = (i2_290 + tmpi6);
            kp4[(0 * largs->m10)].re = (r1_08 + r1_122);
            kp4[(0 * largs->m10)].im = (i1_08 + i1_122);
            kp4[(16 * largs->m10)].re = (r1_08 - r1_122);
            kp4[(16 * largs->m10)].im = (i1_08 - i1_122);
            tmpr6 = ((0.98078528040299994 * r1_38) + (0.19509032201599999 * i1_38));
            tmpi6 = ((0.98078528040299994 * i1_38) - (0.19509032201599999 * r1_38));
            kp4[(1 * largs->m10)].re = (r1_216 + tmpr6);
            kp4[(1 * largs->m10)].im = (i1_216 + tmpi6);
            kp4[(17 * largs->m10)].re = (r1_216 - tmpr6);
            kp4[(17 * largs->m10)].im = (i1_216 - tmpi6);
            tmpr6 = ((0.92387953251099997 * r1_54) + (0.38268343236500002 * i1_54));
            tmpi6 = ((0.92387953251099997 * i1_54) - (0.38268343236500002 * r1_54));
            kp4[(2 * largs->m10)].re = (r1_44 + tmpr6);
            kp4[(2 * largs->m10)].im = (i1_44 + tmpi6);
            kp4[(18 * largs->m10)].re = (r1_44 - tmpr6);
            kp4[(18 * largs->m10)].im = (i1_44 - tmpi6);
            tmpr6 = ((0.83146961230299998 * r1_74) + (0.55557023301999997 * i1_74));
            tmpi6 = ((0.83146961230299998 * i1_74) - (0.55557023301999997 * r1_74));
            kp4[(3 * largs->m10)].re = (r1_64 + tmpr6);
            kp4[(3 * largs->m10)].im = (i1_64 + tmpi6);
            kp4[(19 * largs->m10)].re = (r1_64 - tmpr6);
            kp4[(19 * largs->m10)].im = (i1_64 - tmpi6);
            tmpr6 = (0.70710678118699999 * (r1_92 + i1_92));
            tmpi6 = (0.70710678118699999 * (i1_92 - r1_92));
            kp4[(4 * largs->m10)].re = (r1_82 + tmpr6);
            kp4[(4 * largs->m10)].im = (i1_82 + tmpi6);
            kp4[(20 * largs->m10)].re = (r1_82 - tmpr6);
            kp4[(20 * largs->m10)].im = (i1_82 - tmpi6);
            tmpr6 = ((0.55557023301999997 * r1_1110) + (0.83146961230299998 * i1_1110));
            tmpi6 = ((0.55557023301999997 * i1_1110) - (0.83146961230299998 * r1_1110));
            kp4[(5 * largs->m10)].re = (r1_102 + tmpr6);
            kp4[(5 * largs->m10)].im = (i1_102 + tmpi6);
            kp4[(21 * largs->m10)].re = (r1_102 - tmpr6);
            kp4[(21 * largs->m10)].im = (i1_102 - tmpi6);
            tmpr6 = ((0.38268343236500002 * r1_132) + (0.92387953251099997 * i1_132));
            tmpi6 = ((0.38268343236500002 * i1_132) - (0.92387953251099997 * r1_132));
            kp4[(6 * largs->m10)].re = (r1_123 + tmpr6);
            kp4[(6 * largs->m10)].im = (i1_123 + tmpi6);
            kp4[(22 * largs->m10)].re = (r1_123 - tmpr6);
            kp4[(22 * largs->m10)].im = (i1_123 - tmpi6);
            tmpr6 = ((0.19509032201599999 * r1_152) + (0.98078528040299994 * i1_152));
            tmpi6 = ((0.19509032201599999 * i1_152) - (0.98078528040299994 * r1_152));
            kp4[(7 * largs->m10)].re = (r1_142 + tmpr6);
            kp4[(7 * largs->m10)].im = (i1_142 + tmpi6);
            kp4[(23 * largs->m10)].re = (r1_142 - tmpr6);
            kp4[(23 * largs->m10)].im = (i1_142 - tmpi6);
            kp4[(8 * largs->m10)].re = (r1_160 + i1_170);
            kp4[(8 * largs->m10)].im = (i1_160 - r1_170);
            kp4[(24 * largs->m10)].re = (r1_160 - i1_170);
            kp4[(24 * largs->m10)].im = (i1_160 + r1_170);
            tmpr6 = ((0.98078528040299994 * i1_190) - (0.19509032201599999 * r1_190));
            tmpi6 = ((0.98078528040299994 * r1_190) + (0.19509032201599999 * i1_190));
            kp4[(9 * largs->m10)].re = (r1_180 + tmpr6);
            kp4[(9 * largs->m10)].im = (i1_180 - tmpi6);
            kp4[(25 * largs->m10)].re = (r1_180 - tmpr6);
            kp4[(25 * largs->m10)].im = (i1_180 + tmpi6);
            tmpr6 = ((0.92387953251099997 * i1_217) - (0.38268343236500002 * r1_217));
            tmpi6 = ((0.92387953251099997 * r1_217) + (0.38268343236500002 * i1_217));
            kp4[(10 * largs->m10)].re = (r1_200 + tmpr6);
            kp4[(10 * largs->m10)].im = (i1_200 - tmpi6);
            kp4[(26 * largs->m10)].re = (r1_200 - tmpr6);
            kp4[(26 * largs->m10)].im = (i1_200 + tmpi6);
            tmpr6 = ((0.83146961230299998 * i1_230) - (0.55557023301999997 * r1_230));
            tmpi6 = ((0.83146961230299998 * r1_230) + (0.55557023301999997 * i1_230));
            kp4[(11 * largs->m10)].re = (r1_220 + tmpr6);
            kp4[(11 * largs->m10)].im = (i1_220 - tmpi6);
            kp4[(27 * largs->m10)].re = (r1_220 - tmpr6);
            kp4[(27 * largs->m10)].im = (i1_220 + tmpi6);
            tmpr6 = (0.70710678118699999 * (i1_250 - r1_250));
            tmpi6 = (0.70710678118699999 * (r1_250 + i1_250));
            kp4[(12 * largs->m10)].re = (r1_240 + tmpr6);
            kp4[(12 * largs->m10)].im = (i1_240 - tmpi6);
            kp4[(28 * largs->m10)].re = (r1_240 - tmpr6);
            kp4[(28 * largs->m10)].im = (i1_240 + tmpi6);
            tmpr6 = ((0.55557023301999997 * i1_270) - (0.83146961230299998 * r1_270));
            tmpi6 = ((0.55557023301999997 * r1_270) + (0.83146961230299998 * i1_270));
            kp4[(13 * largs->m10)].re = (r1_260 + tmpr6);
            kp4[(13 * largs->m10)].im = (i1_260 - tmpi6);
            kp4[(29 * largs->m10)].re = (r1_260 - tmpr6);
            kp4[(29 * largs->m10)].im = (i1_260 + tmpi6);
            tmpr6 = ((0.38268343236500002 * i1_290) - (0.92387953251099997 * r1_290));
            tmpi6 = ((0.38268343236500002 * r1_290) + (0.92387953251099997 * i1_290));
            kp4[(14 * largs->m10)].re = (r1_280 + tmpr6);
            kp4[(14 * largs->m10)].im = (i1_280 - tmpi6);
            kp4[(30 * largs->m10)].re = (r1_280 - tmpr6);
            kp4[(30 * largs->m10)].im = (i1_280 + tmpi6);
            tmpr6 = ((0.19509032201599999 * i1_310) - (0.98078528040299994 * r1_310));
            tmpi6 = ((0.19509032201599999 * r1_310) + (0.98078528040299994 * i1_310));
            kp4[(15 * largs->m10)].re = (r1_300 + tmpr6);
            kp4[(15 * largs->m10)].im = (i1_300 - tmpi6);
            kp4[(31 * largs->m10)].re = (r1_300 - tmpr6);
            kp4[(31 * largs->m10)].im = (i1_300 + tmpi6);
            (i12++);
            l14 = (l14 + largs->nWdn4);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab9 = ((largs->a9 + largs->b9) / 2);
        fft_twiddle_32_cont0_closure SN_fft_twiddle_32_cont0c(largs->k);
        spawn_next<fft_twiddle_32_cont0_closure> SN_fft_twiddle_32_cont0(SN_fft_twiddle_32_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_32_cont0, &sp0k);
        fft_twiddle_32_closure sp0c(sp0k);
        sp0c.a9 = largs->a9;
        sp0c.b9 = ab9;
        sp0c.in15 = largs->in15;
        sp0c.out15 = largs->out15;
        sp0c.W6 = largs->W6;
        sp0c.nW5 = largs->nW5;
        sp0c.nWdn4 = largs->nWdn4;
        sp0c.m10 = largs->m10;
        spawn<fft_twiddle_32_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_32_cont0, &sp1k);
        fft_twiddle_32_closure sp1c(sp1k);
        sp1c.a9 = ab9;
        sp1c.b9 = largs->b9;
        sp1c.in15 = largs->in15;
        sp1c.out15 = largs->out15;
        sp1c.W6 = largs->W6;
        sp1c.nW5 = largs->nW5;
        sp1c.nWdn4 = largs->nWdn4;
        sp1c.m10 = largs->m10;
        spawn<fft_twiddle_32_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
THREAD(fft_unshuffle_32) {
    int i13;
    const COMPLEX *ip4;
    COMPLEX *jp10;
    int ab10;
    fft_unshuffle_32_closure *largs = (fft_unshuffle_32_closure*)(args.get());
    if (((largs->b10 - largs->a10) < 128)) {
        ip4 = (largs->in16 + (largs->a10 * 32));
        for (i13 = largs->a10;(i13 < largs->b10);(++i13)) {
            jp10 = (largs->out16 + i13);
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
            jp10 = (jp10 + (2 * largs->m11));
            jp10[0] = ip4[0];
            jp10[largs->m11] = ip4[1];
            ip4 = (ip4 + 2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab10 = ((largs->a10 + largs->b10) / 2);
        fft_unshuffle_32_cont0_closure SN_fft_unshuffle_32_cont0c(largs->k);
        spawn_next<fft_unshuffle_32_cont0_closure> SN_fft_unshuffle_32_cont0(SN_fft_unshuffle_32_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_unshuffle_32_cont0, &sp0k);
        fft_unshuffle_32_closure sp0c(sp0k);
        sp0c.a10 = largs->a10;
        sp0c.b10 = ab10;
        sp0c.in16 = largs->in16;
        sp0c.out16 = largs->out16;
        sp0c.m11 = largs->m11;
        spawn<fft_unshuffle_32_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_unshuffle_32_cont0, &sp1k);
        fft_unshuffle_32_closure sp1c(sp1k);
        sp1c.a10 = ab10;
        sp1c.b10 = largs->b10;
        sp1c.in16 = largs->in16;
        sp1c.out16 = largs->out16;
        sp1c.m11 = largs->m11;
        spawn<fft_unshuffle_32_closure> sp1(sp1c);

        // Original sync was here
    }
    return;
}
THREAD(fft_aux) {
    int r5;
    int m12;
    int k1;
    int k2;
    fft_aux_closure *largs = (fft_aux_closure*)(args.get());
    if ((largs->n1 == 32)) {
        fft_base_32(largs->in17,largs->out17);
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if ((largs->n1 == 16)) {
            fft_base_16(largs->in17,largs->out17);
            SEND_ARGUMENT(largs->k, 0);
        } else {
            if ((largs->n1 == 8)) {
                fft_base_8(largs->in17,largs->out17);
                SEND_ARGUMENT(largs->k, 0);
            } else {
                if ((largs->n1 == 4)) {
                    fft_base_4(largs->in17,largs->out17);
                    SEND_ARGUMENT(largs->k, 0);
                } else {
                    if ((largs->n1 == 2)) {
                        fft_base_2(largs->in17,largs->out17);
                        SEND_ARGUMENT(largs->k, 0);
                    } else {
                        r5 = *(largs->factors);
                        m12 = (largs->n1 / r5);
                        if ((r5 < largs->n1)) {
                            if ((r5 == 32)) {
                                fft_aux_cont5_closure SN_fft_aux_cont5c(largs->k);
                                spawn_next<fft_aux_cont5_closure> SN_fft_aux_cont5(SN_fft_aux_cont5c);
                                cont sp0k;
                                SN_BIND_VOID(SN_fft_aux_cont5, &sp0k);
                                fft_unshuffle_32_closure sp0c(sp0k);
                                sp0c.a10 = 0;
                                sp0c.b10 = m12;
                                sp0c.in16 = largs->in17;
                                sp0c.out16 = largs->out17;
                                sp0c.m11 = m12;
                                spawn<fft_unshuffle_32_closure> sp0(sp0c);

                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->k1 = k1;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->r5 = r5;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->m12 = m12;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->nW6 = largs->nW6;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->out17 = largs->out17;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->in17 = largs->in17;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->W7 = largs->W7;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->k2 = k2;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->factors = largs->factors;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->n1 = largs->n1;
                                // Original sync was here
                            } else {
                                if ((r5 == 16)) {
                                    fft_aux_cont4_closure SN_fft_aux_cont4c(largs->k);
                                    spawn_next<fft_aux_cont4_closure> SN_fft_aux_cont4(SN_fft_aux_cont4c);
                                    cont sp1k;
                                    SN_BIND_VOID(SN_fft_aux_cont4, &sp1k);
                                    fft_unshuffle_16_closure sp1c(sp1k);
                                    sp1c.a8 = 0;
                                    sp1c.b8 = m12;
                                    sp1c.in13 = largs->in17;
                                    sp1c.out13 = largs->out17;
                                    sp1c.m9 = m12;
                                    spawn<fft_unshuffle_16_closure> sp1(sp1c);

                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->k1 = k1;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->r5 = r5;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->m12 = m12;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->nW6 = largs->nW6;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->out17 = largs->out17;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->in17 = largs->in17;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->W7 = largs->W7;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->k2 = k2;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->factors = largs->factors;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->n1 = largs->n1;
                                    // Original sync was here
                                } else {
                                    if ((r5 == 8)) {
                                        fft_aux_cont3_closure SN_fft_aux_cont3c(largs->k);
                                        spawn_next<fft_aux_cont3_closure> SN_fft_aux_cont3(SN_fft_aux_cont3c);
                                        cont sp2k;
                                        SN_BIND_VOID(SN_fft_aux_cont3, &sp2k);
                                        fft_unshuffle_8_closure sp2c(sp2k);
                                        sp2c.a6 = 0;
                                        sp2c.b6 = m12;
                                        sp2c.in10 = largs->in17;
                                        sp2c.out10 = largs->out17;
                                        sp2c.m7 = m12;
                                        spawn<fft_unshuffle_8_closure> sp2(sp2c);

                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->k1 = k1;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->r5 = r5;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->m12 = m12;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->nW6 = largs->nW6;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->out17 = largs->out17;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->in17 = largs->in17;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->W7 = largs->W7;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->k2 = k2;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->factors = largs->factors;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->n1 = largs->n1;
                                        // Original sync was here
                                    } else {
                                        if ((r5 == 4)) {
                                            fft_aux_cont2_closure SN_fft_aux_cont2c(largs->k);
                                            spawn_next<fft_aux_cont2_closure> SN_fft_aux_cont2(SN_fft_aux_cont2c);
                                            cont sp3k;
                                            SN_BIND_VOID(SN_fft_aux_cont2, &sp3k);
                                            fft_unshuffle_4_closure sp3c(sp3k);
                                            sp3c.a4 = 0;
                                            sp3c.b4 = m12;
                                            sp3c.in7 = largs->in17;
                                            sp3c.out7 = largs->out17;
                                            sp3c.m5 = m12;
                                            spawn<fft_unshuffle_4_closure> sp3(sp3c);

                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->k1 = k1;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->r5 = r5;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->m12 = m12;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->nW6 = largs->nW6;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->out17 = largs->out17;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->in17 = largs->in17;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->W7 = largs->W7;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->k2 = k2;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->factors = largs->factors;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->n1 = largs->n1;
                                            // Original sync was here
                                        } else {
                                            if ((r5 == 2)) {
                                                fft_aux_cont1_closure SN_fft_aux_cont1c(largs->k);
                                                spawn_next<fft_aux_cont1_closure> SN_fft_aux_cont1(SN_fft_aux_cont1c);
                                                cont sp4k;
                                                SN_BIND_VOID(SN_fft_aux_cont1, &sp4k);
                                                fft_unshuffle_2_closure sp4c(sp4k);
                                                sp4c.a2 = 0;
                                                sp4c.b2 = m12;
                                                sp4c.in4 = largs->in17;
                                                sp4c.out4 = largs->out17;
                                                sp4c.m3 = m12;
                                                spawn<fft_unshuffle_2_closure> sp4(sp4c);

                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->k1 = k1;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->r5 = r5;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->m12 = m12;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->nW6 = largs->nW6;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->out17 = largs->out17;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->in17 = largs->in17;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->W7 = largs->W7;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->k2 = k2;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->factors = largs->factors;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->n1 = largs->n1;
                                                // Original sync was here
                                            } else {
                                                fft_aux_cont0_closure SN_fft_aux_cont0c(largs->k);
                                                spawn_next<fft_aux_cont0_closure> SN_fft_aux_cont0(SN_fft_aux_cont0c);
                                                cont sp5k;
                                                SN_BIND_VOID(SN_fft_aux_cont0, &sp5k);
                                                unshuffle_closure sp5c(sp5k);
                                                sp5c.a0 = 0;
                                                sp5c.b0 = m12;
                                                sp5c.in = largs->in17;
                                                sp5c.out = largs->out17;
                                                sp5c.r1 = r5;
                                                sp5c.m = m12;
                                                spawn<unshuffle_closure> sp5(sp5c);

                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->k1 = k1;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->r5 = r5;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->m12 = m12;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->nW6 = largs->nW6;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->out17 = largs->out17;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->in17 = largs->in17;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->W7 = largs->W7;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->k2 = k2;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->factors = largs->factors;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->n1 = largs->n1;
                                                // Original sync was here
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            auto sp6c = std::make_shared<fft_aux_afterif0_closure>(largs->k);
                            sp6c->n1 = largs->n1;
                            sp6c->in17 = largs->in17;
                            sp6c->out17 = largs->out17;
                            sp6c->factors = largs->factors;
                            sp6c->W7 = largs->W7;
                            sp6c->nW6 = largs->nW6;
                            sp6c->r5 = r5;
                            sp6c->m12 = m12;
                            sp6c->k1 = k1;
                            sp6c->k2 = k2;
                            cilk_spawn taskSpawn(sp6c->getTask(), sp6c);
                            return;
                        }
                    }
                }
            }
        }
    }
    return;
}
void cilk_fft(int n2, COMPLEX *in18, COMPLEX *out18) {
    int factors0[40];
    int *p;
    int l;
    int r6;
    COMPLEX *W8;
    p = factors0;
    l = n2;
    W8 = ((COMPLEX *) malloc(((n2 + 1) * sizeof(COMPLEX))));
    cilk_fft_cont0_closure SN_cilk_fft_cont0c(CONT_DUMMY);
    spawn_next<cilk_fft_cont0_closure> SN_cilk_fft_cont0(SN_cilk_fft_cont0c);
    cont sp0k;
    SN_BIND_VOID(SN_cilk_fft_cont0, &sp0k);
    compute_w_coefficients_closure sp0c(sp0k);
    sp0c.n = n2;
    sp0c.a = 0;
    sp0c.b = (n2 / 2);
    sp0c.W = W8;
    spawn<compute_w_coefficients_closure> sp0(sp0c);

    do {
    r6 = factor(l);
    *p++ = r6;
    l /= r6;
} while (l > 1);
;
    ((cilk_fft_cont0_closure*)SN_cilk_fft_cont0.cls.get())->W8 = W8;
    std::memcpy(((cilk_fft_cont0_closure*)SN_cilk_fft_cont0.cls.get())->factors0, factors0, sizeof(factors0));
    ((cilk_fft_cont0_closure*)SN_cilk_fft_cont0.cls.get())->out18 = out18;
    ((cilk_fft_cont0_closure*)SN_cilk_fft_cont0.cls.get())->in18 = in18;
    ((cilk_fft_cont0_closure*)SN_cilk_fft_cont0.cls.get())->n2 = n2;
    // Original sync was here
}
THREAD(test_fft_elem) {
    int i14;
    COMPLEX sum;
    COMPLEX w;
    REAL pi;
    test_fft_elem_closure *largs = (test_fft_elem_closure*)(args.get());
    pi = 3.1415926535897931;
    sum.im = 0.;
    sum.re = sum.im;
    for (i14 = 0;(i14 < largs->n3);(++i14)) {
        w.re = cos((((2. * pi) * ((i14 * largs->j1) % largs->n3)) / largs->n3));
        w.im = (-sin((((2. * pi) * ((i14 * largs->j1) % largs->n3)) / largs->n3)));
        ((sum).re) += ((largs->in19[i14]).re) * ((w).re) - ((largs->in19[i14]).im) * ((w).im);
        ((sum).im) += ((largs->in19[i14]).im) * ((w).re) + ((largs->in19[i14]).re) * ((w).im);
    }
    largs->out19[largs->j1] = sum;
    SEND_ARGUMENT(largs->k, 0);
    return;
}
void test_fft(int n4, COMPLEX *in20, COMPLEX *out20) {
    int j2;
    int j3;
    j2 = 0;
    test_fft_cont0_closure SN_test_fft_cont0c(CONT_DUMMY);
    spawn_next<test_fft_cont0_closure> SN_test_fft_cont0(SN_test_fft_cont0c);
    for (j3 = 0;(j3 < n4);(++j3)) {
        cont sp0k;
        SN_BIND_VOID(SN_test_fft_cont0, &sp0k);
        test_fft_elem_closure sp0c(sp0k);
        sp0c.n3 = n4;
        sp0c.j1 = j3;
        sp0c.in19 = in20;
        sp0c.out19 = out20;
        spawn<test_fft_elem_closure> sp0(sp0c);

    }
    // Original sync was here
}
THREAD(fft_aux_afterif0) {
    fft_aux_afterif0_closure *largs = (fft_aux_afterif0_closure*)(args.get());
    if ((largs->r5 == 2)) {
        fft_aux_afterif0_cont5_closure SN_fft_aux_afterif0_cont5c(largs->k);
        spawn_next<fft_aux_afterif0_cont5_closure> SN_fft_aux_afterif0_cont5(SN_fft_aux_afterif0_cont5c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_aux_afterif0_cont5, &sp0k);
        fft_twiddle_2_closure sp0c(sp0k);
        sp0c.a1 = 0;
        sp0c.b1 = largs->m12;
        sp0c.in3 = largs->in17;
        sp0c.out3 = largs->out17;
        sp0c.W2 = largs->W7;
        sp0c.nW1 = largs->nW6;
        sp0c.nWdn0 = (largs->nW6 / largs->n1);
        sp0c.m2 = largs->m12;
        spawn<fft_twiddle_2_closure> sp0(sp0c);

        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->k1 = largs->k1;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->m12 = largs->m12;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->nW6 = largs->nW6;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->r5 = largs->r5;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->n1 = largs->n1;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->W7 = largs->W7;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->k2 = largs->k2;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->factors = largs->factors;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->in17 = largs->in17;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->out17 = largs->out17;
        // Original sync was here
    } else {
        if ((largs->r5 == 4)) {
            fft_aux_afterif0_cont4_closure SN_fft_aux_afterif0_cont4c(largs->k);
            spawn_next<fft_aux_afterif0_cont4_closure> SN_fft_aux_afterif0_cont4(SN_fft_aux_afterif0_cont4c);
            cont sp1k;
            SN_BIND_VOID(SN_fft_aux_afterif0_cont4, &sp1k);
            fft_twiddle_4_closure sp1c(sp1k);
            sp1c.a3 = 0;
            sp1c.b3 = largs->m12;
            sp1c.in6 = largs->in17;
            sp1c.out6 = largs->out17;
            sp1c.W3 = largs->W7;
            sp1c.nW2 = largs->nW6;
            sp1c.nWdn1 = (largs->nW6 / largs->n1);
            sp1c.m4 = largs->m12;
            spawn<fft_twiddle_4_closure> sp1(sp1c);

            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->k1 = largs->k1;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->m12 = largs->m12;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->nW6 = largs->nW6;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->r5 = largs->r5;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->n1 = largs->n1;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->W7 = largs->W7;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->k2 = largs->k2;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->factors = largs->factors;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->in17 = largs->in17;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->out17 = largs->out17;
            // Original sync was here
        } else {
            if ((largs->r5 == 8)) {
                fft_aux_afterif0_cont3_closure SN_fft_aux_afterif0_cont3c(largs->k);
                spawn_next<fft_aux_afterif0_cont3_closure> SN_fft_aux_afterif0_cont3(SN_fft_aux_afterif0_cont3c);
                cont sp2k;
                SN_BIND_VOID(SN_fft_aux_afterif0_cont3, &sp2k);
                fft_twiddle_8_closure sp2c(sp2k);
                sp2c.a5 = 0;
                sp2c.b5 = largs->m12;
                sp2c.in9 = largs->in17;
                sp2c.out9 = largs->out17;
                sp2c.W4 = largs->W7;
                sp2c.nW3 = largs->nW6;
                sp2c.nWdn2 = (largs->nW6 / largs->n1);
                sp2c.m6 = largs->m12;
                spawn<fft_twiddle_8_closure> sp2(sp2c);

                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->k1 = largs->k1;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->m12 = largs->m12;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->nW6 = largs->nW6;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->r5 = largs->r5;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->n1 = largs->n1;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->W7 = largs->W7;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->k2 = largs->k2;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->factors = largs->factors;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->in17 = largs->in17;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->out17 = largs->out17;
                // Original sync was here
            } else {
                if ((largs->r5 == 16)) {
                    fft_aux_afterif0_cont2_closure SN_fft_aux_afterif0_cont2c(largs->k);
                    spawn_next<fft_aux_afterif0_cont2_closure> SN_fft_aux_afterif0_cont2(SN_fft_aux_afterif0_cont2c);
                    cont sp3k;
                    SN_BIND_VOID(SN_fft_aux_afterif0_cont2, &sp3k);
                    fft_twiddle_16_closure sp3c(sp3k);
                    sp3c.a7 = 0;
                    sp3c.b7 = largs->m12;
                    sp3c.in12 = largs->in17;
                    sp3c.out12 = largs->out17;
                    sp3c.W5 = largs->W7;
                    sp3c.nW4 = largs->nW6;
                    sp3c.nWdn3 = (largs->nW6 / largs->n1);
                    sp3c.m8 = largs->m12;
                    spawn<fft_twiddle_16_closure> sp3(sp3c);

                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->k1 = largs->k1;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->m12 = largs->m12;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->nW6 = largs->nW6;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->r5 = largs->r5;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->n1 = largs->n1;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->W7 = largs->W7;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->k2 = largs->k2;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->factors = largs->factors;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->in17 = largs->in17;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->out17 = largs->out17;
                    // Original sync was here
                } else {
                    if ((largs->r5 == 32)) {
                        fft_aux_afterif0_cont1_closure SN_fft_aux_afterif0_cont1c(largs->k);
                        spawn_next<fft_aux_afterif0_cont1_closure> SN_fft_aux_afterif0_cont1(SN_fft_aux_afterif0_cont1c);
                        cont sp4k;
                        SN_BIND_VOID(SN_fft_aux_afterif0_cont1, &sp4k);
                        fft_twiddle_32_closure sp4c(sp4k);
                        sp4c.a9 = 0;
                        sp4c.b9 = largs->m12;
                        sp4c.in15 = largs->in17;
                        sp4c.out15 = largs->out17;
                        sp4c.W6 = largs->W7;
                        sp4c.nW5 = largs->nW6;
                        sp4c.nWdn4 = (largs->nW6 / largs->n1);
                        sp4c.m10 = largs->m12;
                        spawn<fft_twiddle_32_closure> sp4(sp4c);

                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->k1 = largs->k1;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->m12 = largs->m12;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->nW6 = largs->nW6;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->r5 = largs->r5;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->n1 = largs->n1;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->W7 = largs->W7;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->k2 = largs->k2;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->factors = largs->factors;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->in17 = largs->in17;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->out17 = largs->out17;
                        // Original sync was here
                    } else {
                        fft_aux_afterif0_cont0_closure SN_fft_aux_afterif0_cont0c(largs->k);
                        spawn_next<fft_aux_afterif0_cont0_closure> SN_fft_aux_afterif0_cont0(SN_fft_aux_afterif0_cont0c);
                        cont sp5k;
                        SN_BIND_VOID(SN_fft_aux_afterif0_cont0, &sp5k);
                        fft_twiddle_gen_closure sp5c(sp5k);
                        sp5c.i3 = 0;
                        sp5c.i1 = largs->m12;
                        sp5c.in1 = largs->in17;
                        sp5c.out1 = largs->out17;
                        sp5c.W1 = largs->W7;
                        sp5c.nW0 = largs->nW6;
                        sp5c.nWdn = (largs->nW6 / largs->n1);
                        sp5c.r3 = largs->r5;
                        sp5c.m1 = largs->m12;
                        spawn<fft_twiddle_gen_closure> sp5(sp5c);

                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->k1 = largs->k1;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->m12 = largs->m12;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->nW6 = largs->nW6;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->r5 = largs->r5;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->n1 = largs->n1;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->W7 = largs->W7;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->k2 = largs->k2;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->factors = largs->factors;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->in17 = largs->in17;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->out17 = largs->out17;
                        // Original sync was here
                    }
                }
            }
        }
    }
    return;
}
THREAD(fft_aux_afterif0_afterif1) {
    fft_aux_afterif0_afterif1_closure *largs = (fft_aux_afterif0_afterif1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_aux_afterif0_afterif2) {
    fft_aux_afterif0_afterif2_closure *largs = (fft_aux_afterif0_afterif2_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif1_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif0_afterif3) {
    fft_aux_afterif0_afterif3_closure *largs = (fft_aux_afterif0_afterif3_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif2_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif0_afterif4) {
    fft_aux_afterif0_afterif4_closure *largs = (fft_aux_afterif0_afterif4_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif3_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif0_afterif5) {
    fft_aux_afterif0_afterif5_closure *largs = (fft_aux_afterif0_afterif5_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif4_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif6) {
    fft_aux_afterif6_closure *largs = (fft_aux_afterif6_closure*)(args.get());
    fft_aux_afterif6_cont0_closure SN_fft_aux_afterif6_cont0c(largs->k);
    spawn_next<fft_aux_afterif6_cont0_closure> SN_fft_aux_afterif6_cont0(SN_fft_aux_afterif6_cont0c);
    for (largs->k2 = 0;(largs->k2 < largs->n1);largs->k2 = (largs->k2 + largs->m12)) {
        cont sp0k;
        SN_BIND_VOID(SN_fft_aux_afterif6_cont0, &sp0k);
        fft_aux_closure sp0c(sp0k);
        sp0c.n1 = largs->m12;
        sp0c.in17 = (largs->out17 + largs->k2);
        sp0c.out17 = (largs->in17 + largs->k2);
        sp0c.factors = (largs->factors + 1);
        sp0c.W7 = largs->W7;
        sp0c.nW6 = largs->nW6;
        spawn<fft_aux_closure> sp0(sp0c);

    }
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->m12 = largs->m12;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->k1 = largs->k1;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->W7 = largs->W7;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->k2 = largs->k2;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->nW6 = largs->nW6;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->n1 = largs->n1;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->factors = largs->factors;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->r5 = largs->r5;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->out17 = largs->out17;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->in17 = largs->in17;
    // Original sync was here
    return;
}
THREAD(fft_aux_afterif7) {
    fft_aux_afterif7_closure *largs = (fft_aux_afterif7_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif6_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif8) {
    fft_aux_afterif8_closure *largs = (fft_aux_afterif8_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif7_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif9) {
    fft_aux_afterif9_closure *largs = (fft_aux_afterif9_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif8_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif10) {
    fft_aux_afterif10_closure *largs = (fft_aux_afterif10_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif9_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(compute_w_coefficients_cont0) {
    compute_w_coefficients_cont0_closure *largs = (compute_w_coefficients_cont0_closure*)(args.get());
    compute_w_coefficients_cont1_closure SN_compute_w_coefficients_cont1c(largs->k);
    spawn_next<compute_w_coefficients_cont1_closure> SN_compute_w_coefficients_cont1(SN_compute_w_coefficients_cont1c);
    // Original sync was here
    return;
}
THREAD(compute_w_coefficients_cont1) {
    compute_w_coefficients_cont1_closure *largs = (compute_w_coefficients_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(unshuffle_cont0) {
    unshuffle_cont0_closure *largs = (unshuffle_cont0_closure*)(args.get());
    unshuffle_cont1_closure SN_unshuffle_cont1c(largs->k);
    spawn_next<unshuffle_cont1_closure> SN_unshuffle_cont1(SN_unshuffle_cont1c);
    // Original sync was here
    return;
}
THREAD(unshuffle_cont1) {
    unshuffle_cont1_closure *largs = (unshuffle_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_twiddle_gen_cont0) {
    fft_twiddle_gen_cont0_closure *largs = (fft_twiddle_gen_cont0_closure*)(args.get());
    fft_twiddle_gen_cont1_closure SN_fft_twiddle_gen_cont1c(largs->k);
    spawn_next<fft_twiddle_gen_cont1_closure> SN_fft_twiddle_gen_cont1(SN_fft_twiddle_gen_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_twiddle_gen_cont1) {
    fft_twiddle_gen_cont1_closure *largs = (fft_twiddle_gen_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_twiddle_2_cont0) {
    fft_twiddle_2_cont0_closure *largs = (fft_twiddle_2_cont0_closure*)(args.get());
    fft_twiddle_2_cont1_closure SN_fft_twiddle_2_cont1c(largs->k);
    spawn_next<fft_twiddle_2_cont1_closure> SN_fft_twiddle_2_cont1(SN_fft_twiddle_2_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_twiddle_2_cont1) {
    fft_twiddle_2_cont1_closure *largs = (fft_twiddle_2_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_unshuffle_2_cont0) {
    fft_unshuffle_2_cont0_closure *largs = (fft_unshuffle_2_cont0_closure*)(args.get());
    fft_unshuffle_2_cont1_closure SN_fft_unshuffle_2_cont1c(largs->k);
    spawn_next<fft_unshuffle_2_cont1_closure> SN_fft_unshuffle_2_cont1(SN_fft_unshuffle_2_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_unshuffle_2_cont1) {
    fft_unshuffle_2_cont1_closure *largs = (fft_unshuffle_2_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_twiddle_4_cont0) {
    fft_twiddle_4_cont0_closure *largs = (fft_twiddle_4_cont0_closure*)(args.get());
    fft_twiddle_4_cont1_closure SN_fft_twiddle_4_cont1c(largs->k);
    spawn_next<fft_twiddle_4_cont1_closure> SN_fft_twiddle_4_cont1(SN_fft_twiddle_4_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_twiddle_4_cont1) {
    fft_twiddle_4_cont1_closure *largs = (fft_twiddle_4_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_unshuffle_4_cont0) {
    fft_unshuffle_4_cont0_closure *largs = (fft_unshuffle_4_cont0_closure*)(args.get());
    fft_unshuffle_4_cont1_closure SN_fft_unshuffle_4_cont1c(largs->k);
    spawn_next<fft_unshuffle_4_cont1_closure> SN_fft_unshuffle_4_cont1(SN_fft_unshuffle_4_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_unshuffle_4_cont1) {
    fft_unshuffle_4_cont1_closure *largs = (fft_unshuffle_4_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_twiddle_8_cont0) {
    fft_twiddle_8_cont0_closure *largs = (fft_twiddle_8_cont0_closure*)(args.get());
    fft_twiddle_8_cont1_closure SN_fft_twiddle_8_cont1c(largs->k);
    spawn_next<fft_twiddle_8_cont1_closure> SN_fft_twiddle_8_cont1(SN_fft_twiddle_8_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_twiddle_8_cont1) {
    fft_twiddle_8_cont1_closure *largs = (fft_twiddle_8_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_unshuffle_8_cont0) {
    fft_unshuffle_8_cont0_closure *largs = (fft_unshuffle_8_cont0_closure*)(args.get());
    fft_unshuffle_8_cont1_closure SN_fft_unshuffle_8_cont1c(largs->k);
    spawn_next<fft_unshuffle_8_cont1_closure> SN_fft_unshuffle_8_cont1(SN_fft_unshuffle_8_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_unshuffle_8_cont1) {
    fft_unshuffle_8_cont1_closure *largs = (fft_unshuffle_8_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_twiddle_16_cont0) {
    fft_twiddle_16_cont0_closure *largs = (fft_twiddle_16_cont0_closure*)(args.get());
    fft_twiddle_16_cont1_closure SN_fft_twiddle_16_cont1c(largs->k);
    spawn_next<fft_twiddle_16_cont1_closure> SN_fft_twiddle_16_cont1(SN_fft_twiddle_16_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_twiddle_16_cont1) {
    fft_twiddle_16_cont1_closure *largs = (fft_twiddle_16_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_unshuffle_16_cont0) {
    fft_unshuffle_16_cont0_closure *largs = (fft_unshuffle_16_cont0_closure*)(args.get());
    fft_unshuffle_16_cont1_closure SN_fft_unshuffle_16_cont1c(largs->k);
    spawn_next<fft_unshuffle_16_cont1_closure> SN_fft_unshuffle_16_cont1(SN_fft_unshuffle_16_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_unshuffle_16_cont1) {
    fft_unshuffle_16_cont1_closure *largs = (fft_unshuffle_16_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_twiddle_32_cont0) {
    fft_twiddle_32_cont0_closure *largs = (fft_twiddle_32_cont0_closure*)(args.get());
    fft_twiddle_32_cont1_closure SN_fft_twiddle_32_cont1c(largs->k);
    spawn_next<fft_twiddle_32_cont1_closure> SN_fft_twiddle_32_cont1(SN_fft_twiddle_32_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_twiddle_32_cont1) {
    fft_twiddle_32_cont1_closure *largs = (fft_twiddle_32_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_unshuffle_32_cont0) {
    fft_unshuffle_32_cont0_closure *largs = (fft_unshuffle_32_cont0_closure*)(args.get());
    fft_unshuffle_32_cont1_closure SN_fft_unshuffle_32_cont1c(largs->k);
    spawn_next<fft_unshuffle_32_cont1_closure> SN_fft_unshuffle_32_cont1(SN_fft_unshuffle_32_cont1c);
    // Original sync was here
    return;
}
THREAD(fft_unshuffle_32_cont1) {
    fft_unshuffle_32_cont1_closure *largs = (fft_unshuffle_32_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_aux_cont0) {
    fft_aux_cont0_closure *largs = (fft_aux_cont0_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif10_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_cont1) {
    fft_aux_cont1_closure *largs = (fft_aux_cont1_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif10_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_cont2) {
    fft_aux_cont2_closure *largs = (fft_aux_cont2_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif9_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_cont3) {
    fft_aux_cont3_closure *largs = (fft_aux_cont3_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif8_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_cont4) {
    fft_aux_cont4_closure *largs = (fft_aux_cont4_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif7_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_cont5) {
    fft_aux_cont5_closure *largs = (fft_aux_cont5_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif6_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(cilk_fft_cont0) {
    cilk_fft_cont0_closure *largs = (cilk_fft_cont0_closure*)(args.get());
    cilk_fft_cont1_closure SN_cilk_fft_cont1c(largs->k);
    spawn_next<cilk_fft_cont1_closure> SN_cilk_fft_cont1(SN_cilk_fft_cont1c);
    cont sp0k;
    SN_BIND_VOID(SN_cilk_fft_cont1, &sp0k);
    fft_aux_closure sp0c(sp0k);
    sp0c.n1 = largs->n2;
    sp0c.in17 = largs->in18;
    sp0c.out17 = largs->out18;
    sp0c.factors = largs->factors0;
    sp0c.W7 = largs->W8;
    sp0c.nW6 = largs->n2;
    spawn<fft_aux_closure> sp0(sp0c);

    ((cilk_fft_cont1_closure*)SN_cilk_fft_cont1.cls.get())->W8 = largs->W8;
    // Original sync was here
    return;
}
THREAD(cilk_fft_cont1) {
    cilk_fft_cont1_closure *largs = (cilk_fft_cont1_closure*)(args.get());
    free(largs->W8);
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(test_fft_cont0) {
    test_fft_cont0_closure *largs = (test_fft_cont0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
    return;
}
THREAD(fft_aux_afterif0_cont0) {
    fft_aux_afterif0_cont0_closure *largs = (fft_aux_afterif0_cont0_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif5_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif0_cont1) {
    fft_aux_afterif0_cont1_closure *largs = (fft_aux_afterif0_cont1_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif5_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif0_cont2) {
    fft_aux_afterif0_cont2_closure *largs = (fft_aux_afterif0_cont2_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif4_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif0_cont3) {
    fft_aux_afterif0_cont3_closure *largs = (fft_aux_afterif0_cont3_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif3_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif0_cont4) {
    fft_aux_afterif0_cont4_closure *largs = (fft_aux_afterif0_cont4_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif2_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif0_cont5) {
    fft_aux_afterif0_cont5_closure *largs = (fft_aux_afterif0_cont5_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif1_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(fft_aux_afterif6_cont0) {
    fft_aux_afterif6_cont0_closure *largs = (fft_aux_afterif6_cont0_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_closure>(largs->k);
    sp0c->n1 = largs->n1;
    sp0c->in17 = largs->in17;
    sp0c->out17 = largs->out17;
    sp0c->factors = largs->factors;
    sp0c->W7 = largs->W7;
    sp0c->nW6 = largs->nW6;
    sp0c->r5 = largs->r5;
    sp0c->m12 = largs->m12;
    sp0c->k1 = largs->k1;
    sp0c->k2 = largs->k2;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
