#include "cilk_explicit.hh"
#include <cstring>
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
int factor(int n);
THREAD(unshuffle);
void fft_twiddle_gen1(COMPLEX *in, COMPLEX *out, COMPLEX *W, int r, int m, int nW, int nWdnti, int nWdntm);
THREAD(fft_twiddle_gen);
void fft_base_2(COMPLEX *in, COMPLEX *out);
THREAD(fft_twiddle_2);
THREAD(fft_unshuffle_2);
void fft_base_4(COMPLEX *in, COMPLEX *out);
THREAD(fft_twiddle_4);
THREAD(fft_unshuffle_4);
void fft_base_8(COMPLEX *in, COMPLEX *out);
THREAD(fft_twiddle_8);
THREAD(fft_unshuffle_8);
void fft_base_16(COMPLEX *in, COMPLEX *out);
THREAD(fft_twiddle_16);
THREAD(fft_unshuffle_16);
void fft_base_32(COMPLEX *in, COMPLEX *out);
THREAD(fft_twiddle_32);
THREAD(fft_unshuffle_32);
THREAD(fft_aux);
void cilk_fft(int n, COMPLEX *in, COMPLEX *out);
THREAD(test_fft_elem);
void test_fft(int n, COMPLEX *in, COMPLEX *out);
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
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    int r;
    int m;
);
CLOSURE_DEF(fft_twiddle_gen,
    int i;
    int i1;
    COMPLEX *in;
    COMPLEX *out;
    COMPLEX *W;
    int nW;
    int nWdn;
    int r;
    int m;
);
CLOSURE_DEF(fft_twiddle_2,
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    COMPLEX *W;
    int nW;
    int nWdn;
    int m;
);
CLOSURE_DEF(fft_unshuffle_2,
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    int m;
);
CLOSURE_DEF(fft_twiddle_4,
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    COMPLEX *W;
    int nW;
    int nWdn;
    int m;
);
CLOSURE_DEF(fft_unshuffle_4,
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    int m;
);
CLOSURE_DEF(fft_twiddle_8,
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    COMPLEX *W;
    int nW;
    int nWdn;
    int m;
);
CLOSURE_DEF(fft_unshuffle_8,
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    int m;
);
CLOSURE_DEF(fft_twiddle_16,
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    COMPLEX *W;
    int nW;
    int nWdn;
    int m;
);
CLOSURE_DEF(fft_unshuffle_16,
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    int m;
);
CLOSURE_DEF(fft_twiddle_32,
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    COMPLEX *W;
    int nW;
    int nWdn;
    int m;
);
CLOSURE_DEF(fft_unshuffle_32,
    int a;
    int b;
    COMPLEX *in;
    COMPLEX *out;
    int m;
);
CLOSURE_DEF(fft_aux,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
);
CLOSURE_DEF(test_fft_elem,
    int n;
    int j;
    COMPLEX *in;
    COMPLEX *out;
);
CLOSURE_DEF(fft_aux_afterif0,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif0_afterif1,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif0_afterif2,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif0_afterif3,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif0_afterif4,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif0_afterif5,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif6,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif7,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif8,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif9,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif10,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
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
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_cont1,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_cont2,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_cont3,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_cont4,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_cont5,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(cilk_fft_cont0,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int factors[40];
    COMPLEX *W;
);
CLOSURE_DEF(cilk_fft_cont1,
    COMPLEX *W;
);
CLOSURE_DEF(test_fft_cont0,
);
CLOSURE_DEF(fft_aux_afterif0_cont0,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif0_cont1,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif0_cont2,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif0_cont3,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif0_cont4,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif0_cont5,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
);
CLOSURE_DEF(fft_aux_afterif6_cont0,
    int n;
    COMPLEX *in;
    COMPLEX *out;
    int *factors;
    COMPLEX *W;
    int nW;
    int r;
    int m;
    int k0;
    int k1;
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
    int k0;
    REAL s;
    REAL c;
    int ab;
    compute_w_coefficients_closure *largs = (compute_w_coefficients_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        twoPiOverN = ((2. * 3.1415926535897931) / largs->n);
        for (k0 = largs->a;(k0 <= largs->b);(++k0)) {
            c = cos((twoPiOverN * k0));
            largs->W[(largs->n - k0)].re = c;
            largs->W[k0].re = largs->W[(largs->n - k0)].re;
            s = sin((twoPiOverN * k0));
            largs->W[k0].im = (-s);
            largs->W[(largs->n - k0)].im = s;
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
}
int factor(int n) {
    int r;
    if ((n < 2)) {
        return 1;
    } else {
        if (((((((n == 64) || (n == 128)) || (n == 256)) || (n == 1024)) || (n == 2048)) || (n == 4096))) {
            return 8;
        } else {
            if (((n & 15) == 0)) {
                return 16;
            } else {
                if (((n & 7) == 0)) {
                    return 8;
                } else {
                    if (((n & 3) == 0)) {
                        return 4;
                    } else {
                        if (((n & 1) == 0)) {
                            return 2;
                        } else {
                            for (r = 3;(r < n);r = (r + 2)) {
                                if (((n % r) == 0)) {
                                    return r;
                                } else {
                                }
                            }
                            return n;
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
    int ab;
    unshuffle_closure *largs = (unshuffle_closure*)(args.get());
    r4 = (largs->r & (~3));
    if (((largs->b - largs->a) < 16)) {
        ip = (largs->in + (largs->a * largs->r));
        for (i = largs->a;(i < largs->b);(++i)) {
            jp = (largs->out + i);
            for (j = 0;(j < r4);j = (j + 4)) {
                jp[0] = ip[0];
                jp[largs->m] = ip[1];
                jp[2 * largs->m] = ip[2];
                jp[3 * largs->m] = ip[3];
                jp = (jp + (4 * largs->m));
                ip = (ip + 4);
            }
            for (;(j < largs->r);(++j)) {
                *jp = *ip;
                (ip++);
                jp = (jp + largs->m);
            }
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        unshuffle_cont0_closure SN_unshuffle_cont0c(largs->k);
        spawn_next<unshuffle_cont0_closure> SN_unshuffle_cont0(SN_unshuffle_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_unshuffle_cont0, &sp0k);
        unshuffle_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.r = largs->r;
        sp0c.m = largs->m;
        spawn<unshuffle_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_unshuffle_cont0, &sp1k);
        unshuffle_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.r = largs->r;
        sp1c.m = largs->m;
        spawn<unshuffle_closure> sp1(sp1c);

        // Original sync was here
    }
}
void fft_twiddle_gen1(COMPLEX *in, COMPLEX *out, COMPLEX *W, int r, int m, int nW, int nWdnti, int nWdntm) {
    int j;
    int k0;
    COMPLEX *jp;
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
    for (kp = out;(k0 < r);kp = (kp + m)) {
        l1 = (nWdnti + (nWdntm * k0));
        i0 = 0.;
        r0 = i0;
        j = 0;
        jp = in;
        for (l0 = 0;(j < r);jp = (jp + m)) {
            rw = W[l0].re;
            iw = W[l0].im;
            rt = jp->re;
            it = jp->im;
            r0 = (r0 + ((rt * rw) - (it * iw)));
            i0 = (i0 + ((rt * iw) + (it * rw)));
            l0 = (l0 + l1);
            if ((l0 > nW)) {
                l0 = (l0 - nW);
            }
            (++j);
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
    if ((largs->i == (largs->i1 - 1))) {
        fft_twiddle_gen1((largs->in + largs->i),(largs->out + largs->i),largs->W,largs->r,largs->m,largs->nW,(largs->nWdn * largs->i),(largs->nWdn * largs->m));
        SEND_ARGUMENT(largs->k, 0);
    } else {
        i2 = ((largs->i + largs->i1) / 2);
        fft_twiddle_gen_cont0_closure SN_fft_twiddle_gen_cont0c(largs->k);
        spawn_next<fft_twiddle_gen_cont0_closure> SN_fft_twiddle_gen_cont0(SN_fft_twiddle_gen_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_gen_cont0, &sp0k);
        fft_twiddle_gen_closure sp0c(sp0k);
        sp0c.i = largs->i;
        sp0c.i1 = i2;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.W = largs->W;
        sp0c.nW = largs->nW;
        sp0c.nWdn = largs->nWdn;
        sp0c.r = largs->r;
        sp0c.m = largs->m;
        spawn<fft_twiddle_gen_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_gen_cont0, &sp1k);
        fft_twiddle_gen_closure sp1c(sp1k);
        sp1c.i = i2;
        sp1c.i1 = largs->i1;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.W = largs->W;
        sp1c.nW = largs->nW;
        sp1c.nWdn = largs->nWdn;
        sp1c.r = largs->r;
        sp1c.m = largs->m;
        spawn<fft_twiddle_gen_closure> sp1(sp1c);

        // Original sync was here
    }
}
void fft_base_2(COMPLEX *in, COMPLEX *out) {
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
    r1_0 = in[0].re;
    i1_0 = in[0].im;
    r1_1 = in[1].re;
    i1_1 = in[1].im;
    out[0].re = (r1_0 + r1_1);
    out[0].im = (i1_0 + i1_1);
    out[1].re = (r1_0 - r1_1);
    out[1].im = (i1_0 - i1_1);
}
THREAD(fft_twiddle_2) {
    int l1;
    int i;
    COMPLEX *jp;
    COMPLEX *kp;
    REAL tmpr;
    REAL tmpi;
    REAL wr;
    REAL wi;
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
    int ab;
    fft_twiddle_2_closure *largs = (fft_twiddle_2_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        i = largs->a;
        l1 = (largs->nWdn * i);
        for (kp = (largs->out + i);(i < largs->b);(kp++)) {
            jp = (largs->in + i);
            r1_0 = jp[(0 * largs->m)].re;
            i1_0 = jp[(0 * largs->m)].im;
            wr = largs->W[(1 * l1)].re;
            wi = largs->W[(1 * l1)].im;
            tmpr = jp[(1 * largs->m)].re;
            tmpi = jp[(1 * largs->m)].im;
            r1_1 = ((wr * tmpr) - (wi * tmpi));
            i1_1 = ((wi * tmpr) + (wr * tmpi));
            kp[(0 * largs->m)].re = (r1_0 + r1_1);
            kp[(0 * largs->m)].im = (i1_0 + i1_1);
            kp[(1 * largs->m)].re = (r1_0 - r1_1);
            kp[(1 * largs->m)].im = (i1_0 - i1_1);
            (i++);
            l1 = (l1 + largs->nWdn);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        fft_twiddle_2_cont0_closure SN_fft_twiddle_2_cont0c(largs->k);
        spawn_next<fft_twiddle_2_cont0_closure> SN_fft_twiddle_2_cont0(SN_fft_twiddle_2_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_2_cont0, &sp0k);
        fft_twiddle_2_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.W = largs->W;
        sp0c.nW = largs->nW;
        sp0c.nWdn = largs->nWdn;
        sp0c.m = largs->m;
        spawn<fft_twiddle_2_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_2_cont0, &sp1k);
        fft_twiddle_2_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.W = largs->W;
        sp1c.nW = largs->nW;
        sp1c.nWdn = largs->nWdn;
        sp1c.m = largs->m;
        spawn<fft_twiddle_2_closure> sp1(sp1c);

        // Original sync was here
    }
}
THREAD(fft_unshuffle_2) {
    int i;
    const COMPLEX *ip;
    COMPLEX *jp;
    int ab;
    fft_unshuffle_2_closure *largs = (fft_unshuffle_2_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        ip = (largs->in + (largs->a * 2));
        for (i = largs->a;(i < largs->b);(++i)) {
            jp = (largs->out + i);
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        fft_unshuffle_2_cont0_closure SN_fft_unshuffle_2_cont0c(largs->k);
        spawn_next<fft_unshuffle_2_cont0_closure> SN_fft_unshuffle_2_cont0(SN_fft_unshuffle_2_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_unshuffle_2_cont0, &sp0k);
        fft_unshuffle_2_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.m = largs->m;
        spawn<fft_unshuffle_2_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_unshuffle_2_cont0, &sp1k);
        fft_unshuffle_2_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.m = largs->m;
        spawn<fft_unshuffle_2_closure> sp1(sp1c);

        // Original sync was here
    }
}
void fft_base_4(COMPLEX *in, COMPLEX *out) {
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
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
    r2_0 = in[0].re;
    i2_0 = in[0].im;
    r2_2 = in[2].re;
    i2_2 = in[2].im;
    r1_0 = (r2_0 + r2_2);
    i1_0 = (i2_0 + i2_2);
    r1_2 = (r2_0 - r2_2);
    i1_2 = (i2_0 - i2_2);
    r2_1 = in[1].re;
    i2_1 = in[1].im;
    r2_3 = in[3].re;
    i2_3 = in[3].im;
    r1_1 = (r2_1 + r2_3);
    i1_1 = (i2_1 + i2_3);
    r1_3 = (r2_1 - r2_3);
    i1_3 = (i2_1 - i2_3);
    out[0].re = (r1_0 + r1_1);
    out[0].im = (i1_0 + i1_1);
    out[2].re = (r1_0 - r1_1);
    out[2].im = (i1_0 - i1_1);
    out[1].re = (r1_2 + i1_3);
    out[1].im = (i1_2 - r1_3);
    out[3].re = (r1_2 - i1_3);
    out[3].im = (i1_2 + r1_3);
}
THREAD(fft_twiddle_4) {
    int l1;
    int i;
    COMPLEX *jp;
    COMPLEX *kp;
    REAL tmpr;
    REAL tmpi;
    REAL wr;
    REAL wi;
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
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
    int ab;
    fft_twiddle_4_closure *largs = (fft_twiddle_4_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        i = largs->a;
        l1 = (largs->nWdn * i);
        for (kp = (largs->out + i);(i < largs->b);(kp++)) {
            jp = (largs->in + i);
            r2_0 = jp[(0 * largs->m)].re;
            i2_0 = jp[(0 * largs->m)].im;
            wr = largs->W[(2 * l1)].re;
            wi = largs->W[(2 * l1)].im;
            tmpr = jp[(2 * largs->m)].re;
            tmpi = jp[(2 * largs->m)].im;
            r2_2 = ((wr * tmpr) - (wi * tmpi));
            i2_2 = ((wi * tmpr) + (wr * tmpi));
            r1_0 = (r2_0 + r2_2);
            i1_0 = (i2_0 + i2_2);
            r1_2 = (r2_0 - r2_2);
            i1_2 = (i2_0 - i2_2);
            wr = largs->W[(1 * l1)].re;
            wi = largs->W[(1 * l1)].im;
            tmpr = jp[(1 * largs->m)].re;
            tmpi = jp[(1 * largs->m)].im;
            r2_1 = ((wr * tmpr) - (wi * tmpi));
            i2_1 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(3 * l1)].re;
            wi = largs->W[(3 * l1)].im;
            tmpr = jp[(3 * largs->m)].re;
            tmpi = jp[(3 * largs->m)].im;
            r2_3 = ((wr * tmpr) - (wi * tmpi));
            i2_3 = ((wi * tmpr) + (wr * tmpi));
            r1_1 = (r2_1 + r2_3);
            i1_1 = (i2_1 + i2_3);
            r1_3 = (r2_1 - r2_3);
            i1_3 = (i2_1 - i2_3);
            kp[(0 * largs->m)].re = (r1_0 + r1_1);
            kp[(0 * largs->m)].im = (i1_0 + i1_1);
            kp[(2 * largs->m)].re = (r1_0 - r1_1);
            kp[(2 * largs->m)].im = (i1_0 - i1_1);
            kp[(1 * largs->m)].re = (r1_2 + i1_3);
            kp[(1 * largs->m)].im = (i1_2 - r1_3);
            kp[(3 * largs->m)].re = (r1_2 - i1_3);
            kp[(3 * largs->m)].im = (i1_2 + r1_3);
            (i++);
            l1 = (l1 + largs->nWdn);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        fft_twiddle_4_cont0_closure SN_fft_twiddle_4_cont0c(largs->k);
        spawn_next<fft_twiddle_4_cont0_closure> SN_fft_twiddle_4_cont0(SN_fft_twiddle_4_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_4_cont0, &sp0k);
        fft_twiddle_4_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.W = largs->W;
        sp0c.nW = largs->nW;
        sp0c.nWdn = largs->nWdn;
        sp0c.m = largs->m;
        spawn<fft_twiddle_4_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_4_cont0, &sp1k);
        fft_twiddle_4_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.W = largs->W;
        sp1c.nW = largs->nW;
        sp1c.nWdn = largs->nWdn;
        sp1c.m = largs->m;
        spawn<fft_twiddle_4_closure> sp1(sp1c);

        // Original sync was here
    }
}
THREAD(fft_unshuffle_4) {
    int i;
    const COMPLEX *ip;
    COMPLEX *jp;
    int ab;
    fft_unshuffle_4_closure *largs = (fft_unshuffle_4_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        ip = (largs->in + (largs->a * 4));
        for (i = largs->a;(i < largs->b);(++i)) {
            jp = (largs->out + i);
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        fft_unshuffle_4_cont0_closure SN_fft_unshuffle_4_cont0c(largs->k);
        spawn_next<fft_unshuffle_4_cont0_closure> SN_fft_unshuffle_4_cont0(SN_fft_unshuffle_4_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_unshuffle_4_cont0, &sp0k);
        fft_unshuffle_4_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.m = largs->m;
        spawn<fft_unshuffle_4_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_unshuffle_4_cont0, &sp1k);
        fft_unshuffle_4_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.m = largs->m;
        spawn<fft_unshuffle_4_closure> sp1(sp1c);

        // Original sync was here
    }
}
void fft_base_8(COMPLEX *in, COMPLEX *out) {
    REAL tmpr;
    REAL tmpi;
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
    REAL r1_2;
    REAL i1_2;
    REAL r1_3;
    REAL i1_3;
    REAL r1_4;
    REAL i1_4;
    REAL r1_5;
    REAL i1_5;
    REAL r1_6;
    REAL i1_6;
    REAL r1_7;
    REAL i1_7;
    REAL r2_0;
    REAL i2_0;
    REAL r2_2;
    REAL i2_2;
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
    REAL r2_1;
    REAL i2_1;
    REAL r2_3;
    REAL i2_3;
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
    r3_0 = in[0].re;
    i3_0 = in[0].im;
    r3_4 = in[4].re;
    i3_4 = in[4].im;
    r2_0 = (r3_0 + r3_4);
    i2_0 = (i3_0 + i3_4);
    r2_4 = (r3_0 - r3_4);
    i2_4 = (i3_0 - i3_4);
    r3_2 = in[2].re;
    i3_2 = in[2].im;
    r3_6 = in[6].re;
    i3_6 = in[6].im;
    r2_2 = (r3_2 + r3_6);
    i2_2 = (i3_2 + i3_6);
    r2_6 = (r3_2 - r3_6);
    i2_6 = (i3_2 - i3_6);
    r1_0 = (r2_0 + r2_2);
    i1_0 = (i2_0 + i2_2);
    r1_4 = (r2_0 - r2_2);
    i1_4 = (i2_0 - i2_2);
    r1_2 = (r2_4 + i2_6);
    i1_2 = (i2_4 - r2_6);
    r1_6 = (r2_4 - i2_6);
    i1_6 = (i2_4 + r2_6);
    r3_1 = in[1].re;
    i3_1 = in[1].im;
    r3_5 = in[5].re;
    i3_5 = in[5].im;
    r2_1 = (r3_1 + r3_5);
    i2_1 = (i3_1 + i3_5);
    r2_5 = (r3_1 - r3_5);
    i2_5 = (i3_1 - i3_5);
    r3_3 = in[3].re;
    i3_3 = in[3].im;
    r3_7 = in[7].re;
    i3_7 = in[7].im;
    r2_3 = (r3_3 + r3_7);
    i2_3 = (i3_3 + i3_7);
    r2_7 = (r3_3 - r3_7);
    i2_7 = (i3_3 - i3_7);
    r1_1 = (r2_1 + r2_3);
    i1_1 = (i2_1 + i2_3);
    r1_5 = (r2_1 - r2_3);
    i1_5 = (i2_1 - i2_3);
    r1_3 = (r2_5 + i2_7);
    i1_3 = (i2_5 - r2_7);
    r1_7 = (r2_5 - i2_7);
    i1_7 = (i2_5 + r2_7);
    out[0].re = (r1_0 + r1_1);
    out[0].im = (i1_0 + i1_1);
    out[4].re = (r1_0 - r1_1);
    out[4].im = (i1_0 - i1_1);
    tmpr = (0.70710678118699999 * (r1_3 + i1_3));
    tmpi = (0.70710678118699999 * (i1_3 - r1_3));
    out[1].re = (r1_2 + tmpr);
    out[1].im = (i1_2 + tmpi);
    out[5].re = (r1_2 - tmpr);
    out[5].im = (i1_2 - tmpi);
    out[2].re = (r1_4 + i1_5);
    out[2].im = (i1_4 - r1_5);
    out[6].re = (r1_4 - i1_5);
    out[6].im = (i1_4 + r1_5);
    tmpr = (0.70710678118699999 * (i1_7 - r1_7));
    tmpi = (0.70710678118699999 * (r1_7 + i1_7));
    out[3].re = (r1_6 + tmpr);
    out[3].im = (i1_6 - tmpi);
    out[7].re = (r1_6 - tmpr);
    out[7].im = (i1_6 + tmpi);
}
THREAD(fft_twiddle_8) {
    int l1;
    int i;
    COMPLEX *jp;
    COMPLEX *kp;
    REAL tmpr;
    REAL tmpi;
    REAL wr;
    REAL wi;
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
    REAL r1_2;
    REAL i1_2;
    REAL r1_3;
    REAL i1_3;
    REAL r1_4;
    REAL i1_4;
    REAL r1_5;
    REAL i1_5;
    REAL r1_6;
    REAL i1_6;
    REAL r1_7;
    REAL i1_7;
    REAL r2_0;
    REAL i2_0;
    REAL r2_2;
    REAL i2_2;
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
    REAL r2_1;
    REAL i2_1;
    REAL r2_3;
    REAL i2_3;
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
    int ab;
    fft_twiddle_8_closure *largs = (fft_twiddle_8_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        i = largs->a;
        l1 = (largs->nWdn * i);
        for (kp = (largs->out + i);(i < largs->b);(kp++)) {
            jp = (largs->in + i);
            r3_0 = jp[(0 * largs->m)].re;
            i3_0 = jp[(0 * largs->m)].im;
            wr = largs->W[(4 * l1)].re;
            wi = largs->W[(4 * l1)].im;
            tmpr = jp[(4 * largs->m)].re;
            tmpi = jp[(4 * largs->m)].im;
            r3_4 = ((wr * tmpr) - (wi * tmpi));
            i3_4 = ((wi * tmpr) + (wr * tmpi));
            r2_0 = (r3_0 + r3_4);
            i2_0 = (i3_0 + i3_4);
            r2_4 = (r3_0 - r3_4);
            i2_4 = (i3_0 - i3_4);
            wr = largs->W[(2 * l1)].re;
            wi = largs->W[(2 * l1)].im;
            tmpr = jp[(2 * largs->m)].re;
            tmpi = jp[(2 * largs->m)].im;
            r3_2 = ((wr * tmpr) - (wi * tmpi));
            i3_2 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(6 * l1)].re;
            wi = largs->W[(6 * l1)].im;
            tmpr = jp[(6 * largs->m)].re;
            tmpi = jp[(6 * largs->m)].im;
            r3_6 = ((wr * tmpr) - (wi * tmpi));
            i3_6 = ((wi * tmpr) + (wr * tmpi));
            r2_2 = (r3_2 + r3_6);
            i2_2 = (i3_2 + i3_6);
            r2_6 = (r3_2 - r3_6);
            i2_6 = (i3_2 - i3_6);
            r1_0 = (r2_0 + r2_2);
            i1_0 = (i2_0 + i2_2);
            r1_4 = (r2_0 - r2_2);
            i1_4 = (i2_0 - i2_2);
            r1_2 = (r2_4 + i2_6);
            i1_2 = (i2_4 - r2_6);
            r1_6 = (r2_4 - i2_6);
            i1_6 = (i2_4 + r2_6);
            wr = largs->W[(1 * l1)].re;
            wi = largs->W[(1 * l1)].im;
            tmpr = jp[(1 * largs->m)].re;
            tmpi = jp[(1 * largs->m)].im;
            r3_1 = ((wr * tmpr) - (wi * tmpi));
            i3_1 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(5 * l1)].re;
            wi = largs->W[(5 * l1)].im;
            tmpr = jp[(5 * largs->m)].re;
            tmpi = jp[(5 * largs->m)].im;
            r3_5 = ((wr * tmpr) - (wi * tmpi));
            i3_5 = ((wi * tmpr) + (wr * tmpi));
            r2_1 = (r3_1 + r3_5);
            i2_1 = (i3_1 + i3_5);
            r2_5 = (r3_1 - r3_5);
            i2_5 = (i3_1 - i3_5);
            wr = largs->W[(3 * l1)].re;
            wi = largs->W[(3 * l1)].im;
            tmpr = jp[(3 * largs->m)].re;
            tmpi = jp[(3 * largs->m)].im;
            r3_3 = ((wr * tmpr) - (wi * tmpi));
            i3_3 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(7 * l1)].re;
            wi = largs->W[(7 * l1)].im;
            tmpr = jp[(7 * largs->m)].re;
            tmpi = jp[(7 * largs->m)].im;
            r3_7 = ((wr * tmpr) - (wi * tmpi));
            i3_7 = ((wi * tmpr) + (wr * tmpi));
            r2_3 = (r3_3 + r3_7);
            i2_3 = (i3_3 + i3_7);
            r2_7 = (r3_3 - r3_7);
            i2_7 = (i3_3 - i3_7);
            r1_1 = (r2_1 + r2_3);
            i1_1 = (i2_1 + i2_3);
            r1_5 = (r2_1 - r2_3);
            i1_5 = (i2_1 - i2_3);
            r1_3 = (r2_5 + i2_7);
            i1_3 = (i2_5 - r2_7);
            r1_7 = (r2_5 - i2_7);
            i1_7 = (i2_5 + r2_7);
            kp[(0 * largs->m)].re = (r1_0 + r1_1);
            kp[(0 * largs->m)].im = (i1_0 + i1_1);
            kp[(4 * largs->m)].re = (r1_0 - r1_1);
            kp[(4 * largs->m)].im = (i1_0 - i1_1);
            tmpr = (0.70710678118699999 * (r1_3 + i1_3));
            tmpi = (0.70710678118699999 * (i1_3 - r1_3));
            kp[(1 * largs->m)].re = (r1_2 + tmpr);
            kp[(1 * largs->m)].im = (i1_2 + tmpi);
            kp[(5 * largs->m)].re = (r1_2 - tmpr);
            kp[(5 * largs->m)].im = (i1_2 - tmpi);
            kp[(2 * largs->m)].re = (r1_4 + i1_5);
            kp[(2 * largs->m)].im = (i1_4 - r1_5);
            kp[(6 * largs->m)].re = (r1_4 - i1_5);
            kp[(6 * largs->m)].im = (i1_4 + r1_5);
            tmpr = (0.70710678118699999 * (i1_7 - r1_7));
            tmpi = (0.70710678118699999 * (r1_7 + i1_7));
            kp[(3 * largs->m)].re = (r1_6 + tmpr);
            kp[(3 * largs->m)].im = (i1_6 - tmpi);
            kp[(7 * largs->m)].re = (r1_6 - tmpr);
            kp[(7 * largs->m)].im = (i1_6 + tmpi);
            (i++);
            l1 = (l1 + largs->nWdn);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        fft_twiddle_8_cont0_closure SN_fft_twiddle_8_cont0c(largs->k);
        spawn_next<fft_twiddle_8_cont0_closure> SN_fft_twiddle_8_cont0(SN_fft_twiddle_8_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_8_cont0, &sp0k);
        fft_twiddle_8_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.W = largs->W;
        sp0c.nW = largs->nW;
        sp0c.nWdn = largs->nWdn;
        sp0c.m = largs->m;
        spawn<fft_twiddle_8_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_8_cont0, &sp1k);
        fft_twiddle_8_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.W = largs->W;
        sp1c.nW = largs->nW;
        sp1c.nWdn = largs->nWdn;
        sp1c.m = largs->m;
        spawn<fft_twiddle_8_closure> sp1(sp1c);

        // Original sync was here
    }
}
THREAD(fft_unshuffle_8) {
    int i;
    const COMPLEX *ip;
    COMPLEX *jp;
    int ab;
    fft_unshuffle_8_closure *largs = (fft_unshuffle_8_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        ip = (largs->in + (largs->a * 8));
        for (i = largs->a;(i < largs->b);(++i)) {
            jp = (largs->out + i);
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        fft_unshuffle_8_cont0_closure SN_fft_unshuffle_8_cont0c(largs->k);
        spawn_next<fft_unshuffle_8_cont0_closure> SN_fft_unshuffle_8_cont0(SN_fft_unshuffle_8_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_unshuffle_8_cont0, &sp0k);
        fft_unshuffle_8_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.m = largs->m;
        spawn<fft_unshuffle_8_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_unshuffle_8_cont0, &sp1k);
        fft_unshuffle_8_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.m = largs->m;
        spawn<fft_unshuffle_8_closure> sp1(sp1c);

        // Original sync was here
    }
}
void fft_base_16(COMPLEX *in, COMPLEX *out) {
    REAL tmpr;
    REAL tmpi;
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
    REAL r1_2;
    REAL i1_2;
    REAL r1_3;
    REAL i1_3;
    REAL r1_4;
    REAL i1_4;
    REAL r1_5;
    REAL i1_5;
    REAL r1_6;
    REAL i1_6;
    REAL r1_7;
    REAL i1_7;
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
    REAL r2_0;
    REAL i2_0;
    REAL r2_2;
    REAL i2_2;
    REAL r2_4;
    REAL i2_4;
    REAL r2_6;
    REAL i2_6;
    REAL r2_8;
    REAL i2_8;
    REAL r2_10;
    REAL i2_10;
    REAL r2_12;
    REAL i2_12;
    REAL r2_14;
    REAL i2_14;
    REAL r3_0;
    REAL i3_0;
    REAL r3_4;
    REAL i3_4;
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
    REAL r3_2;
    REAL i3_2;
    REAL r3_6;
    REAL i3_6;
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
    REAL r2_1;
    REAL i2_1;
    REAL r2_3;
    REAL i2_3;
    REAL r2_5;
    REAL i2_5;
    REAL r2_7;
    REAL i2_7;
    REAL r2_9;
    REAL i2_9;
    REAL r2_11;
    REAL i2_11;
    REAL r2_13;
    REAL i2_13;
    REAL r2_15;
    REAL i2_15;
    REAL r3_1;
    REAL i3_1;
    REAL r3_5;
    REAL i3_5;
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
    REAL r3_3;
    REAL i3_3;
    REAL r3_7;
    REAL i3_7;
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
    r4_0 = in[0].re;
    i4_0 = in[0].im;
    r4_8 = in[8].re;
    i4_8 = in[8].im;
    r3_0 = (r4_0 + r4_8);
    i3_0 = (i4_0 + i4_8);
    r3_8 = (r4_0 - r4_8);
    i3_8 = (i4_0 - i4_8);
    r4_4 = in[4].re;
    i4_4 = in[4].im;
    r4_12 = in[12].re;
    i4_12 = in[12].im;
    r3_4 = (r4_4 + r4_12);
    i3_4 = (i4_4 + i4_12);
    r3_12 = (r4_4 - r4_12);
    i3_12 = (i4_4 - i4_12);
    r2_0 = (r3_0 + r3_4);
    i2_0 = (i3_0 + i3_4);
    r2_8 = (r3_0 - r3_4);
    i2_8 = (i3_0 - i3_4);
    r2_4 = (r3_8 + i3_12);
    i2_4 = (i3_8 - r3_12);
    r2_12 = (r3_8 - i3_12);
    i2_12 = (i3_8 + r3_12);
    r4_2 = in[2].re;
    i4_2 = in[2].im;
    r4_10 = in[10].re;
    i4_10 = in[10].im;
    r3_2 = (r4_2 + r4_10);
    i3_2 = (i4_2 + i4_10);
    r3_10 = (r4_2 - r4_10);
    i3_10 = (i4_2 - i4_10);
    r4_6 = in[6].re;
    i4_6 = in[6].im;
    r4_14 = in[14].re;
    i4_14 = in[14].im;
    r3_6 = (r4_6 + r4_14);
    i3_6 = (i4_6 + i4_14);
    r3_14 = (r4_6 - r4_14);
    i3_14 = (i4_6 - i4_14);
    r2_2 = (r3_2 + r3_6);
    i2_2 = (i3_2 + i3_6);
    r2_10 = (r3_2 - r3_6);
    i2_10 = (i3_2 - i3_6);
    r2_6 = (r3_10 + i3_14);
    i2_6 = (i3_10 - r3_14);
    r2_14 = (r3_10 - i3_14);
    i2_14 = (i3_10 + r3_14);
    r1_0 = (r2_0 + r2_2);
    i1_0 = (i2_0 + i2_2);
    r1_8 = (r2_0 - r2_2);
    i1_8 = (i2_0 - i2_2);
    tmpr = (0.70710678118699999 * (r2_6 + i2_6));
    tmpi = (0.70710678118699999 * (i2_6 - r2_6));
    r1_2 = (r2_4 + tmpr);
    i1_2 = (i2_4 + tmpi);
    r1_10 = (r2_4 - tmpr);
    i1_10 = (i2_4 - tmpi);
    r1_4 = (r2_8 + i2_10);
    i1_4 = (i2_8 - r2_10);
    r1_12 = (r2_8 - i2_10);
    i1_12 = (i2_8 + r2_10);
    tmpr = (0.70710678118699999 * (i2_14 - r2_14));
    tmpi = (0.70710678118699999 * (r2_14 + i2_14));
    r1_6 = (r2_12 + tmpr);
    i1_6 = (i2_12 - tmpi);
    r1_14 = (r2_12 - tmpr);
    i1_14 = (i2_12 + tmpi);
    r4_1 = in[1].re;
    i4_1 = in[1].im;
    r4_9 = in[9].re;
    i4_9 = in[9].im;
    r3_1 = (r4_1 + r4_9);
    i3_1 = (i4_1 + i4_9);
    r3_9 = (r4_1 - r4_9);
    i3_9 = (i4_1 - i4_9);
    r4_5 = in[5].re;
    i4_5 = in[5].im;
    r4_13 = in[13].re;
    i4_13 = in[13].im;
    r3_5 = (r4_5 + r4_13);
    i3_5 = (i4_5 + i4_13);
    r3_13 = (r4_5 - r4_13);
    i3_13 = (i4_5 - i4_13);
    r2_1 = (r3_1 + r3_5);
    i2_1 = (i3_1 + i3_5);
    r2_9 = (r3_1 - r3_5);
    i2_9 = (i3_1 - i3_5);
    r2_5 = (r3_9 + i3_13);
    i2_5 = (i3_9 - r3_13);
    r2_13 = (r3_9 - i3_13);
    i2_13 = (i3_9 + r3_13);
    r4_3 = in[3].re;
    i4_3 = in[3].im;
    r4_11 = in[11].re;
    i4_11 = in[11].im;
    r3_3 = (r4_3 + r4_11);
    i3_3 = (i4_3 + i4_11);
    r3_11 = (r4_3 - r4_11);
    i3_11 = (i4_3 - i4_11);
    r4_7 = in[7].re;
    i4_7 = in[7].im;
    r4_15 = in[15].re;
    i4_15 = in[15].im;
    r3_7 = (r4_7 + r4_15);
    i3_7 = (i4_7 + i4_15);
    r3_15 = (r4_7 - r4_15);
    i3_15 = (i4_7 - i4_15);
    r2_3 = (r3_3 + r3_7);
    i2_3 = (i3_3 + i3_7);
    r2_11 = (r3_3 - r3_7);
    i2_11 = (i3_3 - i3_7);
    r2_7 = (r3_11 + i3_15);
    i2_7 = (i3_11 - r3_15);
    r2_15 = (r3_11 - i3_15);
    i2_15 = (i3_11 + r3_15);
    r1_1 = (r2_1 + r2_3);
    i1_1 = (i2_1 + i2_3);
    r1_9 = (r2_1 - r2_3);
    i1_9 = (i2_1 - i2_3);
    tmpr = (0.70710678118699999 * (r2_7 + i2_7));
    tmpi = (0.70710678118699999 * (i2_7 - r2_7));
    r1_3 = (r2_5 + tmpr);
    i1_3 = (i2_5 + tmpi);
    r1_11 = (r2_5 - tmpr);
    i1_11 = (i2_5 - tmpi);
    r1_5 = (r2_9 + i2_11);
    i1_5 = (i2_9 - r2_11);
    r1_13 = (r2_9 - i2_11);
    i1_13 = (i2_9 + r2_11);
    tmpr = (0.70710678118699999 * (i2_15 - r2_15));
    tmpi = (0.70710678118699999 * (r2_15 + i2_15));
    r1_7 = (r2_13 + tmpr);
    i1_7 = (i2_13 - tmpi);
    r1_15 = (r2_13 - tmpr);
    i1_15 = (i2_13 + tmpi);
    out[0].re = (r1_0 + r1_1);
    out[0].im = (i1_0 + i1_1);
    out[8].re = (r1_0 - r1_1);
    out[8].im = (i1_0 - i1_1);
    tmpr = ((0.92387953251099997 * r1_3) + (0.38268343236500002 * i1_3));
    tmpi = ((0.92387953251099997 * i1_3) - (0.38268343236500002 * r1_3));
    out[1].re = (r1_2 + tmpr);
    out[1].im = (i1_2 + tmpi);
    out[9].re = (r1_2 - tmpr);
    out[9].im = (i1_2 - tmpi);
    tmpr = (0.70710678118699999 * (r1_5 + i1_5));
    tmpi = (0.70710678118699999 * (i1_5 - r1_5));
    out[2].re = (r1_4 + tmpr);
    out[2].im = (i1_4 + tmpi);
    out[10].re = (r1_4 - tmpr);
    out[10].im = (i1_4 - tmpi);
    tmpr = ((0.38268343236500002 * r1_7) + (0.92387953251099997 * i1_7));
    tmpi = ((0.38268343236500002 * i1_7) - (0.92387953251099997 * r1_7));
    out[3].re = (r1_6 + tmpr);
    out[3].im = (i1_6 + tmpi);
    out[11].re = (r1_6 - tmpr);
    out[11].im = (i1_6 - tmpi);
    out[4].re = (r1_8 + i1_9);
    out[4].im = (i1_8 - r1_9);
    out[12].re = (r1_8 - i1_9);
    out[12].im = (i1_8 + r1_9);
    tmpr = ((0.92387953251099997 * i1_11) - (0.38268343236500002 * r1_11));
    tmpi = ((0.92387953251099997 * r1_11) + (0.38268343236500002 * i1_11));
    out[5].re = (r1_10 + tmpr);
    out[5].im = (i1_10 - tmpi);
    out[13].re = (r1_10 - tmpr);
    out[13].im = (i1_10 + tmpi);
    tmpr = (0.70710678118699999 * (i1_13 - r1_13));
    tmpi = (0.70710678118699999 * (r1_13 + i1_13));
    out[6].re = (r1_12 + tmpr);
    out[6].im = (i1_12 - tmpi);
    out[14].re = (r1_12 - tmpr);
    out[14].im = (i1_12 + tmpi);
    tmpr = ((0.38268343236500002 * i1_15) - (0.92387953251099997 * r1_15));
    tmpi = ((0.38268343236500002 * r1_15) + (0.92387953251099997 * i1_15));
    out[7].re = (r1_14 + tmpr);
    out[7].im = (i1_14 - tmpi);
    out[15].re = (r1_14 - tmpr);
    out[15].im = (i1_14 + tmpi);
}
THREAD(fft_twiddle_16) {
    int l1;
    int i;
    COMPLEX *jp;
    COMPLEX *kp;
    REAL tmpr;
    REAL tmpi;
    REAL wr;
    REAL wi;
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
    REAL r1_2;
    REAL i1_2;
    REAL r1_3;
    REAL i1_3;
    REAL r1_4;
    REAL i1_4;
    REAL r1_5;
    REAL i1_5;
    REAL r1_6;
    REAL i1_6;
    REAL r1_7;
    REAL i1_7;
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
    REAL r2_0;
    REAL i2_0;
    REAL r2_2;
    REAL i2_2;
    REAL r2_4;
    REAL i2_4;
    REAL r2_6;
    REAL i2_6;
    REAL r2_8;
    REAL i2_8;
    REAL r2_10;
    REAL i2_10;
    REAL r2_12;
    REAL i2_12;
    REAL r2_14;
    REAL i2_14;
    REAL r3_0;
    REAL i3_0;
    REAL r3_4;
    REAL i3_4;
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
    REAL r3_2;
    REAL i3_2;
    REAL r3_6;
    REAL i3_6;
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
    REAL r2_1;
    REAL i2_1;
    REAL r2_3;
    REAL i2_3;
    REAL r2_5;
    REAL i2_5;
    REAL r2_7;
    REAL i2_7;
    REAL r2_9;
    REAL i2_9;
    REAL r2_11;
    REAL i2_11;
    REAL r2_13;
    REAL i2_13;
    REAL r2_15;
    REAL i2_15;
    REAL r3_1;
    REAL i3_1;
    REAL r3_5;
    REAL i3_5;
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
    REAL r3_3;
    REAL i3_3;
    REAL r3_7;
    REAL i3_7;
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
    int ab;
    fft_twiddle_16_closure *largs = (fft_twiddle_16_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        i = largs->a;
        l1 = (largs->nWdn * i);
        for (kp = (largs->out + i);(i < largs->b);(kp++)) {
            jp = (largs->in + i);
            r4_0 = jp[(0 * largs->m)].re;
            i4_0 = jp[(0 * largs->m)].im;
            wr = largs->W[(8 * l1)].re;
            wi = largs->W[(8 * l1)].im;
            tmpr = jp[(8 * largs->m)].re;
            tmpi = jp[(8 * largs->m)].im;
            r4_8 = ((wr * tmpr) - (wi * tmpi));
            i4_8 = ((wi * tmpr) + (wr * tmpi));
            r3_0 = (r4_0 + r4_8);
            i3_0 = (i4_0 + i4_8);
            r3_8 = (r4_0 - r4_8);
            i3_8 = (i4_0 - i4_8);
            wr = largs->W[(4 * l1)].re;
            wi = largs->W[(4 * l1)].im;
            tmpr = jp[(4 * largs->m)].re;
            tmpi = jp[(4 * largs->m)].im;
            r4_4 = ((wr * tmpr) - (wi * tmpi));
            i4_4 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(12 * l1)].re;
            wi = largs->W[(12 * l1)].im;
            tmpr = jp[(12 * largs->m)].re;
            tmpi = jp[(12 * largs->m)].im;
            r4_12 = ((wr * tmpr) - (wi * tmpi));
            i4_12 = ((wi * tmpr) + (wr * tmpi));
            r3_4 = (r4_4 + r4_12);
            i3_4 = (i4_4 + i4_12);
            r3_12 = (r4_4 - r4_12);
            i3_12 = (i4_4 - i4_12);
            r2_0 = (r3_0 + r3_4);
            i2_0 = (i3_0 + i3_4);
            r2_8 = (r3_0 - r3_4);
            i2_8 = (i3_0 - i3_4);
            r2_4 = (r3_8 + i3_12);
            i2_4 = (i3_8 - r3_12);
            r2_12 = (r3_8 - i3_12);
            i2_12 = (i3_8 + r3_12);
            wr = largs->W[(2 * l1)].re;
            wi = largs->W[(2 * l1)].im;
            tmpr = jp[(2 * largs->m)].re;
            tmpi = jp[(2 * largs->m)].im;
            r4_2 = ((wr * tmpr) - (wi * tmpi));
            i4_2 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(10 * l1)].re;
            wi = largs->W[(10 * l1)].im;
            tmpr = jp[(10 * largs->m)].re;
            tmpi = jp[(10 * largs->m)].im;
            r4_10 = ((wr * tmpr) - (wi * tmpi));
            i4_10 = ((wi * tmpr) + (wr * tmpi));
            r3_2 = (r4_2 + r4_10);
            i3_2 = (i4_2 + i4_10);
            r3_10 = (r4_2 - r4_10);
            i3_10 = (i4_2 - i4_10);
            wr = largs->W[(6 * l1)].re;
            wi = largs->W[(6 * l1)].im;
            tmpr = jp[(6 * largs->m)].re;
            tmpi = jp[(6 * largs->m)].im;
            r4_6 = ((wr * tmpr) - (wi * tmpi));
            i4_6 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(14 * l1)].re;
            wi = largs->W[(14 * l1)].im;
            tmpr = jp[(14 * largs->m)].re;
            tmpi = jp[(14 * largs->m)].im;
            r4_14 = ((wr * tmpr) - (wi * tmpi));
            i4_14 = ((wi * tmpr) + (wr * tmpi));
            r3_6 = (r4_6 + r4_14);
            i3_6 = (i4_6 + i4_14);
            r3_14 = (r4_6 - r4_14);
            i3_14 = (i4_6 - i4_14);
            r2_2 = (r3_2 + r3_6);
            i2_2 = (i3_2 + i3_6);
            r2_10 = (r3_2 - r3_6);
            i2_10 = (i3_2 - i3_6);
            r2_6 = (r3_10 + i3_14);
            i2_6 = (i3_10 - r3_14);
            r2_14 = (r3_10 - i3_14);
            i2_14 = (i3_10 + r3_14);
            r1_0 = (r2_0 + r2_2);
            i1_0 = (i2_0 + i2_2);
            r1_8 = (r2_0 - r2_2);
            i1_8 = (i2_0 - i2_2);
            tmpr = (0.70710678118699999 * (r2_6 + i2_6));
            tmpi = (0.70710678118699999 * (i2_6 - r2_6));
            r1_2 = (r2_4 + tmpr);
            i1_2 = (i2_4 + tmpi);
            r1_10 = (r2_4 - tmpr);
            i1_10 = (i2_4 - tmpi);
            r1_4 = (r2_8 + i2_10);
            i1_4 = (i2_8 - r2_10);
            r1_12 = (r2_8 - i2_10);
            i1_12 = (i2_8 + r2_10);
            tmpr = (0.70710678118699999 * (i2_14 - r2_14));
            tmpi = (0.70710678118699999 * (r2_14 + i2_14));
            r1_6 = (r2_12 + tmpr);
            i1_6 = (i2_12 - tmpi);
            r1_14 = (r2_12 - tmpr);
            i1_14 = (i2_12 + tmpi);
            wr = largs->W[(1 * l1)].re;
            wi = largs->W[(1 * l1)].im;
            tmpr = jp[(1 * largs->m)].re;
            tmpi = jp[(1 * largs->m)].im;
            r4_1 = ((wr * tmpr) - (wi * tmpi));
            i4_1 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(9 * l1)].re;
            wi = largs->W[(9 * l1)].im;
            tmpr = jp[(9 * largs->m)].re;
            tmpi = jp[(9 * largs->m)].im;
            r4_9 = ((wr * tmpr) - (wi * tmpi));
            i4_9 = ((wi * tmpr) + (wr * tmpi));
            r3_1 = (r4_1 + r4_9);
            i3_1 = (i4_1 + i4_9);
            r3_9 = (r4_1 - r4_9);
            i3_9 = (i4_1 - i4_9);
            wr = largs->W[(5 * l1)].re;
            wi = largs->W[(5 * l1)].im;
            tmpr = jp[(5 * largs->m)].re;
            tmpi = jp[(5 * largs->m)].im;
            r4_5 = ((wr * tmpr) - (wi * tmpi));
            i4_5 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(13 * l1)].re;
            wi = largs->W[(13 * l1)].im;
            tmpr = jp[(13 * largs->m)].re;
            tmpi = jp[(13 * largs->m)].im;
            r4_13 = ((wr * tmpr) - (wi * tmpi));
            i4_13 = ((wi * tmpr) + (wr * tmpi));
            r3_5 = (r4_5 + r4_13);
            i3_5 = (i4_5 + i4_13);
            r3_13 = (r4_5 - r4_13);
            i3_13 = (i4_5 - i4_13);
            r2_1 = (r3_1 + r3_5);
            i2_1 = (i3_1 + i3_5);
            r2_9 = (r3_1 - r3_5);
            i2_9 = (i3_1 - i3_5);
            r2_5 = (r3_9 + i3_13);
            i2_5 = (i3_9 - r3_13);
            r2_13 = (r3_9 - i3_13);
            i2_13 = (i3_9 + r3_13);
            wr = largs->W[(3 * l1)].re;
            wi = largs->W[(3 * l1)].im;
            tmpr = jp[(3 * largs->m)].re;
            tmpi = jp[(3 * largs->m)].im;
            r4_3 = ((wr * tmpr) - (wi * tmpi));
            i4_3 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(11 * l1)].re;
            wi = largs->W[(11 * l1)].im;
            tmpr = jp[(11 * largs->m)].re;
            tmpi = jp[(11 * largs->m)].im;
            r4_11 = ((wr * tmpr) - (wi * tmpi));
            i4_11 = ((wi * tmpr) + (wr * tmpi));
            r3_3 = (r4_3 + r4_11);
            i3_3 = (i4_3 + i4_11);
            r3_11 = (r4_3 - r4_11);
            i3_11 = (i4_3 - i4_11);
            wr = largs->W[(7 * l1)].re;
            wi = largs->W[(7 * l1)].im;
            tmpr = jp[(7 * largs->m)].re;
            tmpi = jp[(7 * largs->m)].im;
            r4_7 = ((wr * tmpr) - (wi * tmpi));
            i4_7 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(15 * l1)].re;
            wi = largs->W[(15 * l1)].im;
            tmpr = jp[(15 * largs->m)].re;
            tmpi = jp[(15 * largs->m)].im;
            r4_15 = ((wr * tmpr) - (wi * tmpi));
            i4_15 = ((wi * tmpr) + (wr * tmpi));
            r3_7 = (r4_7 + r4_15);
            i3_7 = (i4_7 + i4_15);
            r3_15 = (r4_7 - r4_15);
            i3_15 = (i4_7 - i4_15);
            r2_3 = (r3_3 + r3_7);
            i2_3 = (i3_3 + i3_7);
            r2_11 = (r3_3 - r3_7);
            i2_11 = (i3_3 - i3_7);
            r2_7 = (r3_11 + i3_15);
            i2_7 = (i3_11 - r3_15);
            r2_15 = (r3_11 - i3_15);
            i2_15 = (i3_11 + r3_15);
            r1_1 = (r2_1 + r2_3);
            i1_1 = (i2_1 + i2_3);
            r1_9 = (r2_1 - r2_3);
            i1_9 = (i2_1 - i2_3);
            tmpr = (0.70710678118699999 * (r2_7 + i2_7));
            tmpi = (0.70710678118699999 * (i2_7 - r2_7));
            r1_3 = (r2_5 + tmpr);
            i1_3 = (i2_5 + tmpi);
            r1_11 = (r2_5 - tmpr);
            i1_11 = (i2_5 - tmpi);
            r1_5 = (r2_9 + i2_11);
            i1_5 = (i2_9 - r2_11);
            r1_13 = (r2_9 - i2_11);
            i1_13 = (i2_9 + r2_11);
            tmpr = (0.70710678118699999 * (i2_15 - r2_15));
            tmpi = (0.70710678118699999 * (r2_15 + i2_15));
            r1_7 = (r2_13 + tmpr);
            i1_7 = (i2_13 - tmpi);
            r1_15 = (r2_13 - tmpr);
            i1_15 = (i2_13 + tmpi);
            kp[(0 * largs->m)].re = (r1_0 + r1_1);
            kp[(0 * largs->m)].im = (i1_0 + i1_1);
            kp[(8 * largs->m)].re = (r1_0 - r1_1);
            kp[(8 * largs->m)].im = (i1_0 - i1_1);
            tmpr = ((0.92387953251099997 * r1_3) + (0.38268343236500002 * i1_3));
            tmpi = ((0.92387953251099997 * i1_3) - (0.38268343236500002 * r1_3));
            kp[(1 * largs->m)].re = (r1_2 + tmpr);
            kp[(1 * largs->m)].im = (i1_2 + tmpi);
            kp[(9 * largs->m)].re = (r1_2 - tmpr);
            kp[(9 * largs->m)].im = (i1_2 - tmpi);
            tmpr = (0.70710678118699999 * (r1_5 + i1_5));
            tmpi = (0.70710678118699999 * (i1_5 - r1_5));
            kp[(2 * largs->m)].re = (r1_4 + tmpr);
            kp[(2 * largs->m)].im = (i1_4 + tmpi);
            kp[(10 * largs->m)].re = (r1_4 - tmpr);
            kp[(10 * largs->m)].im = (i1_4 - tmpi);
            tmpr = ((0.38268343236500002 * r1_7) + (0.92387953251099997 * i1_7));
            tmpi = ((0.38268343236500002 * i1_7) - (0.92387953251099997 * r1_7));
            kp[(3 * largs->m)].re = (r1_6 + tmpr);
            kp[(3 * largs->m)].im = (i1_6 + tmpi);
            kp[(11 * largs->m)].re = (r1_6 - tmpr);
            kp[(11 * largs->m)].im = (i1_6 - tmpi);
            kp[(4 * largs->m)].re = (r1_8 + i1_9);
            kp[(4 * largs->m)].im = (i1_8 - r1_9);
            kp[(12 * largs->m)].re = (r1_8 - i1_9);
            kp[(12 * largs->m)].im = (i1_8 + r1_9);
            tmpr = ((0.92387953251099997 * i1_11) - (0.38268343236500002 * r1_11));
            tmpi = ((0.92387953251099997 * r1_11) + (0.38268343236500002 * i1_11));
            kp[(5 * largs->m)].re = (r1_10 + tmpr);
            kp[(5 * largs->m)].im = (i1_10 - tmpi);
            kp[(13 * largs->m)].re = (r1_10 - tmpr);
            kp[(13 * largs->m)].im = (i1_10 + tmpi);
            tmpr = (0.70710678118699999 * (i1_13 - r1_13));
            tmpi = (0.70710678118699999 * (r1_13 + i1_13));
            kp[(6 * largs->m)].re = (r1_12 + tmpr);
            kp[(6 * largs->m)].im = (i1_12 - tmpi);
            kp[(14 * largs->m)].re = (r1_12 - tmpr);
            kp[(14 * largs->m)].im = (i1_12 + tmpi);
            tmpr = ((0.38268343236500002 * i1_15) - (0.92387953251099997 * r1_15));
            tmpi = ((0.38268343236500002 * r1_15) + (0.92387953251099997 * i1_15));
            kp[(7 * largs->m)].re = (r1_14 + tmpr);
            kp[(7 * largs->m)].im = (i1_14 - tmpi);
            kp[(15 * largs->m)].re = (r1_14 - tmpr);
            kp[(15 * largs->m)].im = (i1_14 + tmpi);
            (i++);
            l1 = (l1 + largs->nWdn);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        fft_twiddle_16_cont0_closure SN_fft_twiddle_16_cont0c(largs->k);
        spawn_next<fft_twiddle_16_cont0_closure> SN_fft_twiddle_16_cont0(SN_fft_twiddle_16_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_16_cont0, &sp0k);
        fft_twiddle_16_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.W = largs->W;
        sp0c.nW = largs->nW;
        sp0c.nWdn = largs->nWdn;
        sp0c.m = largs->m;
        spawn<fft_twiddle_16_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_16_cont0, &sp1k);
        fft_twiddle_16_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.W = largs->W;
        sp1c.nW = largs->nW;
        sp1c.nWdn = largs->nWdn;
        sp1c.m = largs->m;
        spawn<fft_twiddle_16_closure> sp1(sp1c);

        // Original sync was here
    }
}
THREAD(fft_unshuffle_16) {
    int i;
    const COMPLEX *ip;
    COMPLEX *jp;
    int ab;
    fft_unshuffle_16_closure *largs = (fft_unshuffle_16_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        ip = (largs->in + (largs->a * 16));
        for (i = largs->a;(i < largs->b);(++i)) {
            jp = (largs->out + i);
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        fft_unshuffle_16_cont0_closure SN_fft_unshuffle_16_cont0c(largs->k);
        spawn_next<fft_unshuffle_16_cont0_closure> SN_fft_unshuffle_16_cont0(SN_fft_unshuffle_16_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_unshuffle_16_cont0, &sp0k);
        fft_unshuffle_16_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.m = largs->m;
        spawn<fft_unshuffle_16_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_unshuffle_16_cont0, &sp1k);
        fft_unshuffle_16_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.m = largs->m;
        spawn<fft_unshuffle_16_closure> sp1(sp1c);

        // Original sync was here
    }
}
void fft_base_32(COMPLEX *in, COMPLEX *out) {
    REAL tmpr;
    REAL tmpi;
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
    REAL r1_2;
    REAL i1_2;
    REAL r1_3;
    REAL i1_3;
    REAL r1_4;
    REAL i1_4;
    REAL r1_5;
    REAL i1_5;
    REAL r1_6;
    REAL i1_6;
    REAL r1_7;
    REAL i1_7;
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
    REAL r2_0;
    REAL i2_0;
    REAL r2_2;
    REAL i2_2;
    REAL r2_4;
    REAL i2_4;
    REAL r2_6;
    REAL i2_6;
    REAL r2_8;
    REAL i2_8;
    REAL r2_10;
    REAL i2_10;
    REAL r2_12;
    REAL i2_12;
    REAL r2_14;
    REAL i2_14;
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
    REAL r3_0;
    REAL i3_0;
    REAL r3_4;
    REAL i3_4;
    REAL r3_8;
    REAL i3_8;
    REAL r3_12;
    REAL i3_12;
    REAL r3_16;
    REAL i3_16;
    REAL r3_20;
    REAL i3_20;
    REAL r3_24;
    REAL i3_24;
    REAL r3_28;
    REAL i3_28;
    REAL r4_0;
    REAL i4_0;
    REAL r4_8;
    REAL i4_8;
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
    REAL r4_4;
    REAL i4_4;
    REAL r4_12;
    REAL i4_12;
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
    REAL r3_2;
    REAL i3_2;
    REAL r3_6;
    REAL i3_6;
    REAL r3_10;
    REAL i3_10;
    REAL r3_14;
    REAL i3_14;
    REAL r3_18;
    REAL i3_18;
    REAL r3_22;
    REAL i3_22;
    REAL r3_26;
    REAL i3_26;
    REAL r3_30;
    REAL i3_30;
    REAL r4_2;
    REAL i4_2;
    REAL r4_10;
    REAL i4_10;
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
    REAL r4_6;
    REAL i4_6;
    REAL r4_14;
    REAL i4_14;
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
    REAL r2_1;
    REAL i2_1;
    REAL r2_3;
    REAL i2_3;
    REAL r2_5;
    REAL i2_5;
    REAL r2_7;
    REAL i2_7;
    REAL r2_9;
    REAL i2_9;
    REAL r2_11;
    REAL i2_11;
    REAL r2_13;
    REAL i2_13;
    REAL r2_15;
    REAL i2_15;
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
    REAL r3_1;
    REAL i3_1;
    REAL r3_5;
    REAL i3_5;
    REAL r3_9;
    REAL i3_9;
    REAL r3_13;
    REAL i3_13;
    REAL r3_17;
    REAL i3_17;
    REAL r3_21;
    REAL i3_21;
    REAL r3_25;
    REAL i3_25;
    REAL r3_29;
    REAL i3_29;
    REAL r4_1;
    REAL i4_1;
    REAL r4_9;
    REAL i4_9;
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
    REAL r4_5;
    REAL i4_5;
    REAL r4_13;
    REAL i4_13;
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
    REAL r3_3;
    REAL i3_3;
    REAL r3_7;
    REAL i3_7;
    REAL r3_11;
    REAL i3_11;
    REAL r3_15;
    REAL i3_15;
    REAL r3_19;
    REAL i3_19;
    REAL r3_23;
    REAL i3_23;
    REAL r3_27;
    REAL i3_27;
    REAL r3_31;
    REAL i3_31;
    REAL r4_3;
    REAL i4_3;
    REAL r4_11;
    REAL i4_11;
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
    REAL r4_7;
    REAL i4_7;
    REAL r4_15;
    REAL i4_15;
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
    r5_0 = in[0].re;
    i5_0 = in[0].im;
    r5_16 = in[16].re;
    i5_16 = in[16].im;
    r4_0 = (r5_0 + r5_16);
    i4_0 = (i5_0 + i5_16);
    r4_16 = (r5_0 - r5_16);
    i4_16 = (i5_0 - i5_16);
    r5_8 = in[8].re;
    i5_8 = in[8].im;
    r5_24 = in[24].re;
    i5_24 = in[24].im;
    r4_8 = (r5_8 + r5_24);
    i4_8 = (i5_8 + i5_24);
    r4_24 = (r5_8 - r5_24);
    i4_24 = (i5_8 - i5_24);
    r3_0 = (r4_0 + r4_8);
    i3_0 = (i4_0 + i4_8);
    r3_16 = (r4_0 - r4_8);
    i3_16 = (i4_0 - i4_8);
    r3_8 = (r4_16 + i4_24);
    i3_8 = (i4_16 - r4_24);
    r3_24 = (r4_16 - i4_24);
    i3_24 = (i4_16 + r4_24);
    r5_4 = in[4].re;
    i5_4 = in[4].im;
    r5_20 = in[20].re;
    i5_20 = in[20].im;
    r4_4 = (r5_4 + r5_20);
    i4_4 = (i5_4 + i5_20);
    r4_20 = (r5_4 - r5_20);
    i4_20 = (i5_4 - i5_20);
    r5_12 = in[12].re;
    i5_12 = in[12].im;
    r5_28 = in[28].re;
    i5_28 = in[28].im;
    r4_12 = (r5_12 + r5_28);
    i4_12 = (i5_12 + i5_28);
    r4_28 = (r5_12 - r5_28);
    i4_28 = (i5_12 - i5_28);
    r3_4 = (r4_4 + r4_12);
    i3_4 = (i4_4 + i4_12);
    r3_20 = (r4_4 - r4_12);
    i3_20 = (i4_4 - i4_12);
    r3_12 = (r4_20 + i4_28);
    i3_12 = (i4_20 - r4_28);
    r3_28 = (r4_20 - i4_28);
    i3_28 = (i4_20 + r4_28);
    r2_0 = (r3_0 + r3_4);
    i2_0 = (i3_0 + i3_4);
    r2_16 = (r3_0 - r3_4);
    i2_16 = (i3_0 - i3_4);
    tmpr = (0.70710678118699999 * (r3_12 + i3_12));
    tmpi = (0.70710678118699999 * (i3_12 - r3_12));
    r2_4 = (r3_8 + tmpr);
    i2_4 = (i3_8 + tmpi);
    r2_20 = (r3_8 - tmpr);
    i2_20 = (i3_8 - tmpi);
    r2_8 = (r3_16 + i3_20);
    i2_8 = (i3_16 - r3_20);
    r2_24 = (r3_16 - i3_20);
    i2_24 = (i3_16 + r3_20);
    tmpr = (0.70710678118699999 * (i3_28 - r3_28));
    tmpi = (0.70710678118699999 * (r3_28 + i3_28));
    r2_12 = (r3_24 + tmpr);
    i2_12 = (i3_24 - tmpi);
    r2_28 = (r3_24 - tmpr);
    i2_28 = (i3_24 + tmpi);
    r5_2 = in[2].re;
    i5_2 = in[2].im;
    r5_18 = in[18].re;
    i5_18 = in[18].im;
    r4_2 = (r5_2 + r5_18);
    i4_2 = (i5_2 + i5_18);
    r4_18 = (r5_2 - r5_18);
    i4_18 = (i5_2 - i5_18);
    r5_10 = in[10].re;
    i5_10 = in[10].im;
    r5_26 = in[26].re;
    i5_26 = in[26].im;
    r4_10 = (r5_10 + r5_26);
    i4_10 = (i5_10 + i5_26);
    r4_26 = (r5_10 - r5_26);
    i4_26 = (i5_10 - i5_26);
    r3_2 = (r4_2 + r4_10);
    i3_2 = (i4_2 + i4_10);
    r3_18 = (r4_2 - r4_10);
    i3_18 = (i4_2 - i4_10);
    r3_10 = (r4_18 + i4_26);
    i3_10 = (i4_18 - r4_26);
    r3_26 = (r4_18 - i4_26);
    i3_26 = (i4_18 + r4_26);
    r5_6 = in[6].re;
    i5_6 = in[6].im;
    r5_22 = in[22].re;
    i5_22 = in[22].im;
    r4_6 = (r5_6 + r5_22);
    i4_6 = (i5_6 + i5_22);
    r4_22 = (r5_6 - r5_22);
    i4_22 = (i5_6 - i5_22);
    r5_14 = in[14].re;
    i5_14 = in[14].im;
    r5_30 = in[30].re;
    i5_30 = in[30].im;
    r4_14 = (r5_14 + r5_30);
    i4_14 = (i5_14 + i5_30);
    r4_30 = (r5_14 - r5_30);
    i4_30 = (i5_14 - i5_30);
    r3_6 = (r4_6 + r4_14);
    i3_6 = (i4_6 + i4_14);
    r3_22 = (r4_6 - r4_14);
    i3_22 = (i4_6 - i4_14);
    r3_14 = (r4_22 + i4_30);
    i3_14 = (i4_22 - r4_30);
    r3_30 = (r4_22 - i4_30);
    i3_30 = (i4_22 + r4_30);
    r2_2 = (r3_2 + r3_6);
    i2_2 = (i3_2 + i3_6);
    r2_18 = (r3_2 - r3_6);
    i2_18 = (i3_2 - i3_6);
    tmpr = (0.70710678118699999 * (r3_14 + i3_14));
    tmpi = (0.70710678118699999 * (i3_14 - r3_14));
    r2_6 = (r3_10 + tmpr);
    i2_6 = (i3_10 + tmpi);
    r2_22 = (r3_10 - tmpr);
    i2_22 = (i3_10 - tmpi);
    r2_10 = (r3_18 + i3_22);
    i2_10 = (i3_18 - r3_22);
    r2_26 = (r3_18 - i3_22);
    i2_26 = (i3_18 + r3_22);
    tmpr = (0.70710678118699999 * (i3_30 - r3_30));
    tmpi = (0.70710678118699999 * (r3_30 + i3_30));
    r2_14 = (r3_26 + tmpr);
    i2_14 = (i3_26 - tmpi);
    r2_30 = (r3_26 - tmpr);
    i2_30 = (i3_26 + tmpi);
    r1_0 = (r2_0 + r2_2);
    i1_0 = (i2_0 + i2_2);
    r1_16 = (r2_0 - r2_2);
    i1_16 = (i2_0 - i2_2);
    tmpr = ((0.92387953251099997 * r2_6) + (0.38268343236500002 * i2_6));
    tmpi = ((0.92387953251099997 * i2_6) - (0.38268343236500002 * r2_6));
    r1_2 = (r2_4 + tmpr);
    i1_2 = (i2_4 + tmpi);
    r1_18 = (r2_4 - tmpr);
    i1_18 = (i2_4 - tmpi);
    tmpr = (0.70710678118699999 * (r2_10 + i2_10));
    tmpi = (0.70710678118699999 * (i2_10 - r2_10));
    r1_4 = (r2_8 + tmpr);
    i1_4 = (i2_8 + tmpi);
    r1_20 = (r2_8 - tmpr);
    i1_20 = (i2_8 - tmpi);
    tmpr = ((0.38268343236500002 * r2_14) + (0.92387953251099997 * i2_14));
    tmpi = ((0.38268343236500002 * i2_14) - (0.92387953251099997 * r2_14));
    r1_6 = (r2_12 + tmpr);
    i1_6 = (i2_12 + tmpi);
    r1_22 = (r2_12 - tmpr);
    i1_22 = (i2_12 - tmpi);
    r1_8 = (r2_16 + i2_18);
    i1_8 = (i2_16 - r2_18);
    r1_24 = (r2_16 - i2_18);
    i1_24 = (i2_16 + r2_18);
    tmpr = ((0.92387953251099997 * i2_22) - (0.38268343236500002 * r2_22));
    tmpi = ((0.92387953251099997 * r2_22) + (0.38268343236500002 * i2_22));
    r1_10 = (r2_20 + tmpr);
    i1_10 = (i2_20 - tmpi);
    r1_26 = (r2_20 - tmpr);
    i1_26 = (i2_20 + tmpi);
    tmpr = (0.70710678118699999 * (i2_26 - r2_26));
    tmpi = (0.70710678118699999 * (r2_26 + i2_26));
    r1_12 = (r2_24 + tmpr);
    i1_12 = (i2_24 - tmpi);
    r1_28 = (r2_24 - tmpr);
    i1_28 = (i2_24 + tmpi);
    tmpr = ((0.38268343236500002 * i2_30) - (0.92387953251099997 * r2_30));
    tmpi = ((0.38268343236500002 * r2_30) + (0.92387953251099997 * i2_30));
    r1_14 = (r2_28 + tmpr);
    i1_14 = (i2_28 - tmpi);
    r1_30 = (r2_28 - tmpr);
    i1_30 = (i2_28 + tmpi);
    r5_1 = in[1].re;
    i5_1 = in[1].im;
    r5_17 = in[17].re;
    i5_17 = in[17].im;
    r4_1 = (r5_1 + r5_17);
    i4_1 = (i5_1 + i5_17);
    r4_17 = (r5_1 - r5_17);
    i4_17 = (i5_1 - i5_17);
    r5_9 = in[9].re;
    i5_9 = in[9].im;
    r5_25 = in[25].re;
    i5_25 = in[25].im;
    r4_9 = (r5_9 + r5_25);
    i4_9 = (i5_9 + i5_25);
    r4_25 = (r5_9 - r5_25);
    i4_25 = (i5_9 - i5_25);
    r3_1 = (r4_1 + r4_9);
    i3_1 = (i4_1 + i4_9);
    r3_17 = (r4_1 - r4_9);
    i3_17 = (i4_1 - i4_9);
    r3_9 = (r4_17 + i4_25);
    i3_9 = (i4_17 - r4_25);
    r3_25 = (r4_17 - i4_25);
    i3_25 = (i4_17 + r4_25);
    r5_5 = in[5].re;
    i5_5 = in[5].im;
    r5_21 = in[21].re;
    i5_21 = in[21].im;
    r4_5 = (r5_5 + r5_21);
    i4_5 = (i5_5 + i5_21);
    r4_21 = (r5_5 - r5_21);
    i4_21 = (i5_5 - i5_21);
    r5_13 = in[13].re;
    i5_13 = in[13].im;
    r5_29 = in[29].re;
    i5_29 = in[29].im;
    r4_13 = (r5_13 + r5_29);
    i4_13 = (i5_13 + i5_29);
    r4_29 = (r5_13 - r5_29);
    i4_29 = (i5_13 - i5_29);
    r3_5 = (r4_5 + r4_13);
    i3_5 = (i4_5 + i4_13);
    r3_21 = (r4_5 - r4_13);
    i3_21 = (i4_5 - i4_13);
    r3_13 = (r4_21 + i4_29);
    i3_13 = (i4_21 - r4_29);
    r3_29 = (r4_21 - i4_29);
    i3_29 = (i4_21 + r4_29);
    r2_1 = (r3_1 + r3_5);
    i2_1 = (i3_1 + i3_5);
    r2_17 = (r3_1 - r3_5);
    i2_17 = (i3_1 - i3_5);
    tmpr = (0.70710678118699999 * (r3_13 + i3_13));
    tmpi = (0.70710678118699999 * (i3_13 - r3_13));
    r2_5 = (r3_9 + tmpr);
    i2_5 = (i3_9 + tmpi);
    r2_21 = (r3_9 - tmpr);
    i2_21 = (i3_9 - tmpi);
    r2_9 = (r3_17 + i3_21);
    i2_9 = (i3_17 - r3_21);
    r2_25 = (r3_17 - i3_21);
    i2_25 = (i3_17 + r3_21);
    tmpr = (0.70710678118699999 * (i3_29 - r3_29));
    tmpi = (0.70710678118699999 * (r3_29 + i3_29));
    r2_13 = (r3_25 + tmpr);
    i2_13 = (i3_25 - tmpi);
    r2_29 = (r3_25 - tmpr);
    i2_29 = (i3_25 + tmpi);
    r5_3 = in[3].re;
    i5_3 = in[3].im;
    r5_19 = in[19].re;
    i5_19 = in[19].im;
    r4_3 = (r5_3 + r5_19);
    i4_3 = (i5_3 + i5_19);
    r4_19 = (r5_3 - r5_19);
    i4_19 = (i5_3 - i5_19);
    r5_11 = in[11].re;
    i5_11 = in[11].im;
    r5_27 = in[27].re;
    i5_27 = in[27].im;
    r4_11 = (r5_11 + r5_27);
    i4_11 = (i5_11 + i5_27);
    r4_27 = (r5_11 - r5_27);
    i4_27 = (i5_11 - i5_27);
    r3_3 = (r4_3 + r4_11);
    i3_3 = (i4_3 + i4_11);
    r3_19 = (r4_3 - r4_11);
    i3_19 = (i4_3 - i4_11);
    r3_11 = (r4_19 + i4_27);
    i3_11 = (i4_19 - r4_27);
    r3_27 = (r4_19 - i4_27);
    i3_27 = (i4_19 + r4_27);
    r5_7 = in[7].re;
    i5_7 = in[7].im;
    r5_23 = in[23].re;
    i5_23 = in[23].im;
    r4_7 = (r5_7 + r5_23);
    i4_7 = (i5_7 + i5_23);
    r4_23 = (r5_7 - r5_23);
    i4_23 = (i5_7 - i5_23);
    r5_15 = in[15].re;
    i5_15 = in[15].im;
    r5_31 = in[31].re;
    i5_31 = in[31].im;
    r4_15 = (r5_15 + r5_31);
    i4_15 = (i5_15 + i5_31);
    r4_31 = (r5_15 - r5_31);
    i4_31 = (i5_15 - i5_31);
    r3_7 = (r4_7 + r4_15);
    i3_7 = (i4_7 + i4_15);
    r3_23 = (r4_7 - r4_15);
    i3_23 = (i4_7 - i4_15);
    r3_15 = (r4_23 + i4_31);
    i3_15 = (i4_23 - r4_31);
    r3_31 = (r4_23 - i4_31);
    i3_31 = (i4_23 + r4_31);
    r2_3 = (r3_3 + r3_7);
    i2_3 = (i3_3 + i3_7);
    r2_19 = (r3_3 - r3_7);
    i2_19 = (i3_3 - i3_7);
    tmpr = (0.70710678118699999 * (r3_15 + i3_15));
    tmpi = (0.70710678118699999 * (i3_15 - r3_15));
    r2_7 = (r3_11 + tmpr);
    i2_7 = (i3_11 + tmpi);
    r2_23 = (r3_11 - tmpr);
    i2_23 = (i3_11 - tmpi);
    r2_11 = (r3_19 + i3_23);
    i2_11 = (i3_19 - r3_23);
    r2_27 = (r3_19 - i3_23);
    i2_27 = (i3_19 + r3_23);
    tmpr = (0.70710678118699999 * (i3_31 - r3_31));
    tmpi = (0.70710678118699999 * (r3_31 + i3_31));
    r2_15 = (r3_27 + tmpr);
    i2_15 = (i3_27 - tmpi);
    r2_31 = (r3_27 - tmpr);
    i2_31 = (i3_27 + tmpi);
    r1_1 = (r2_1 + r2_3);
    i1_1 = (i2_1 + i2_3);
    r1_17 = (r2_1 - r2_3);
    i1_17 = (i2_1 - i2_3);
    tmpr = ((0.92387953251099997 * r2_7) + (0.38268343236500002 * i2_7));
    tmpi = ((0.92387953251099997 * i2_7) - (0.38268343236500002 * r2_7));
    r1_3 = (r2_5 + tmpr);
    i1_3 = (i2_5 + tmpi);
    r1_19 = (r2_5 - tmpr);
    i1_19 = (i2_5 - tmpi);
    tmpr = (0.70710678118699999 * (r2_11 + i2_11));
    tmpi = (0.70710678118699999 * (i2_11 - r2_11));
    r1_5 = (r2_9 + tmpr);
    i1_5 = (i2_9 + tmpi);
    r1_21 = (r2_9 - tmpr);
    i1_21 = (i2_9 - tmpi);
    tmpr = ((0.38268343236500002 * r2_15) + (0.92387953251099997 * i2_15));
    tmpi = ((0.38268343236500002 * i2_15) - (0.92387953251099997 * r2_15));
    r1_7 = (r2_13 + tmpr);
    i1_7 = (i2_13 + tmpi);
    r1_23 = (r2_13 - tmpr);
    i1_23 = (i2_13 - tmpi);
    r1_9 = (r2_17 + i2_19);
    i1_9 = (i2_17 - r2_19);
    r1_25 = (r2_17 - i2_19);
    i1_25 = (i2_17 + r2_19);
    tmpr = ((0.92387953251099997 * i2_23) - (0.38268343236500002 * r2_23));
    tmpi = ((0.92387953251099997 * r2_23) + (0.38268343236500002 * i2_23));
    r1_11 = (r2_21 + tmpr);
    i1_11 = (i2_21 - tmpi);
    r1_27 = (r2_21 - tmpr);
    i1_27 = (i2_21 + tmpi);
    tmpr = (0.70710678118699999 * (i2_27 - r2_27));
    tmpi = (0.70710678118699999 * (r2_27 + i2_27));
    r1_13 = (r2_25 + tmpr);
    i1_13 = (i2_25 - tmpi);
    r1_29 = (r2_25 - tmpr);
    i1_29 = (i2_25 + tmpi);
    tmpr = ((0.38268343236500002 * i2_31) - (0.92387953251099997 * r2_31));
    tmpi = ((0.38268343236500002 * r2_31) + (0.92387953251099997 * i2_31));
    r1_15 = (r2_29 + tmpr);
    i1_15 = (i2_29 - tmpi);
    r1_31 = (r2_29 - tmpr);
    i1_31 = (i2_29 + tmpi);
    out[0].re = (r1_0 + r1_1);
    out[0].im = (i1_0 + i1_1);
    out[16].re = (r1_0 - r1_1);
    out[16].im = (i1_0 - i1_1);
    tmpr = ((0.98078528040299994 * r1_3) + (0.19509032201599999 * i1_3));
    tmpi = ((0.98078528040299994 * i1_3) - (0.19509032201599999 * r1_3));
    out[1].re = (r1_2 + tmpr);
    out[1].im = (i1_2 + tmpi);
    out[17].re = (r1_2 - tmpr);
    out[17].im = (i1_2 - tmpi);
    tmpr = ((0.92387953251099997 * r1_5) + (0.38268343236500002 * i1_5));
    tmpi = ((0.92387953251099997 * i1_5) - (0.38268343236500002 * r1_5));
    out[2].re = (r1_4 + tmpr);
    out[2].im = (i1_4 + tmpi);
    out[18].re = (r1_4 - tmpr);
    out[18].im = (i1_4 - tmpi);
    tmpr = ((0.83146961230299998 * r1_7) + (0.55557023301999997 * i1_7));
    tmpi = ((0.83146961230299998 * i1_7) - (0.55557023301999997 * r1_7));
    out[3].re = (r1_6 + tmpr);
    out[3].im = (i1_6 + tmpi);
    out[19].re = (r1_6 - tmpr);
    out[19].im = (i1_6 - tmpi);
    tmpr = (0.70710678118699999 * (r1_9 + i1_9));
    tmpi = (0.70710678118699999 * (i1_9 - r1_9));
    out[4].re = (r1_8 + tmpr);
    out[4].im = (i1_8 + tmpi);
    out[20].re = (r1_8 - tmpr);
    out[20].im = (i1_8 - tmpi);
    tmpr = ((0.55557023301999997 * r1_11) + (0.83146961230299998 * i1_11));
    tmpi = ((0.55557023301999997 * i1_11) - (0.83146961230299998 * r1_11));
    out[5].re = (r1_10 + tmpr);
    out[5].im = (i1_10 + tmpi);
    out[21].re = (r1_10 - tmpr);
    out[21].im = (i1_10 - tmpi);
    tmpr = ((0.38268343236500002 * r1_13) + (0.92387953251099997 * i1_13));
    tmpi = ((0.38268343236500002 * i1_13) - (0.92387953251099997 * r1_13));
    out[6].re = (r1_12 + tmpr);
    out[6].im = (i1_12 + tmpi);
    out[22].re = (r1_12 - tmpr);
    out[22].im = (i1_12 - tmpi);
    tmpr = ((0.19509032201599999 * r1_15) + (0.98078528040299994 * i1_15));
    tmpi = ((0.19509032201599999 * i1_15) - (0.98078528040299994 * r1_15));
    out[7].re = (r1_14 + tmpr);
    out[7].im = (i1_14 + tmpi);
    out[23].re = (r1_14 - tmpr);
    out[23].im = (i1_14 - tmpi);
    out[8].re = (r1_16 + i1_17);
    out[8].im = (i1_16 - r1_17);
    out[24].re = (r1_16 - i1_17);
    out[24].im = (i1_16 + r1_17);
    tmpr = ((0.98078528040299994 * i1_19) - (0.19509032201599999 * r1_19));
    tmpi = ((0.98078528040299994 * r1_19) + (0.19509032201599999 * i1_19));
    out[9].re = (r1_18 + tmpr);
    out[9].im = (i1_18 - tmpi);
    out[25].re = (r1_18 - tmpr);
    out[25].im = (i1_18 + tmpi);
    tmpr = ((0.92387953251099997 * i1_21) - (0.38268343236500002 * r1_21));
    tmpi = ((0.92387953251099997 * r1_21) + (0.38268343236500002 * i1_21));
    out[10].re = (r1_20 + tmpr);
    out[10].im = (i1_20 - tmpi);
    out[26].re = (r1_20 - tmpr);
    out[26].im = (i1_20 + tmpi);
    tmpr = ((0.83146961230299998 * i1_23) - (0.55557023301999997 * r1_23));
    tmpi = ((0.83146961230299998 * r1_23) + (0.55557023301999997 * i1_23));
    out[11].re = (r1_22 + tmpr);
    out[11].im = (i1_22 - tmpi);
    out[27].re = (r1_22 - tmpr);
    out[27].im = (i1_22 + tmpi);
    tmpr = (0.70710678118699999 * (i1_25 - r1_25));
    tmpi = (0.70710678118699999 * (r1_25 + i1_25));
    out[12].re = (r1_24 + tmpr);
    out[12].im = (i1_24 - tmpi);
    out[28].re = (r1_24 - tmpr);
    out[28].im = (i1_24 + tmpi);
    tmpr = ((0.55557023301999997 * i1_27) - (0.83146961230299998 * r1_27));
    tmpi = ((0.55557023301999997 * r1_27) + (0.83146961230299998 * i1_27));
    out[13].re = (r1_26 + tmpr);
    out[13].im = (i1_26 - tmpi);
    out[29].re = (r1_26 - tmpr);
    out[29].im = (i1_26 + tmpi);
    tmpr = ((0.38268343236500002 * i1_29) - (0.92387953251099997 * r1_29));
    tmpi = ((0.38268343236500002 * r1_29) + (0.92387953251099997 * i1_29));
    out[14].re = (r1_28 + tmpr);
    out[14].im = (i1_28 - tmpi);
    out[30].re = (r1_28 - tmpr);
    out[30].im = (i1_28 + tmpi);
    tmpr = ((0.19509032201599999 * i1_31) - (0.98078528040299994 * r1_31));
    tmpi = ((0.19509032201599999 * r1_31) + (0.98078528040299994 * i1_31));
    out[15].re = (r1_30 + tmpr);
    out[15].im = (i1_30 - tmpi);
    out[31].re = (r1_30 - tmpr);
    out[31].im = (i1_30 + tmpi);
}
THREAD(fft_twiddle_32) {
    int l1;
    int i;
    COMPLEX *jp;
    COMPLEX *kp;
    REAL tmpr;
    REAL tmpi;
    REAL wr;
    REAL wi;
    REAL r1_0;
    REAL i1_0;
    REAL r1_1;
    REAL i1_1;
    REAL r1_2;
    REAL i1_2;
    REAL r1_3;
    REAL i1_3;
    REAL r1_4;
    REAL i1_4;
    REAL r1_5;
    REAL i1_5;
    REAL r1_6;
    REAL i1_6;
    REAL r1_7;
    REAL i1_7;
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
    REAL r2_0;
    REAL i2_0;
    REAL r2_2;
    REAL i2_2;
    REAL r2_4;
    REAL i2_4;
    REAL r2_6;
    REAL i2_6;
    REAL r2_8;
    REAL i2_8;
    REAL r2_10;
    REAL i2_10;
    REAL r2_12;
    REAL i2_12;
    REAL r2_14;
    REAL i2_14;
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
    REAL r3_0;
    REAL i3_0;
    REAL r3_4;
    REAL i3_4;
    REAL r3_8;
    REAL i3_8;
    REAL r3_12;
    REAL i3_12;
    REAL r3_16;
    REAL i3_16;
    REAL r3_20;
    REAL i3_20;
    REAL r3_24;
    REAL i3_24;
    REAL r3_28;
    REAL i3_28;
    REAL r4_0;
    REAL i4_0;
    REAL r4_8;
    REAL i4_8;
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
    REAL r4_4;
    REAL i4_4;
    REAL r4_12;
    REAL i4_12;
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
    REAL r3_2;
    REAL i3_2;
    REAL r3_6;
    REAL i3_6;
    REAL r3_10;
    REAL i3_10;
    REAL r3_14;
    REAL i3_14;
    REAL r3_18;
    REAL i3_18;
    REAL r3_22;
    REAL i3_22;
    REAL r3_26;
    REAL i3_26;
    REAL r3_30;
    REAL i3_30;
    REAL r4_2;
    REAL i4_2;
    REAL r4_10;
    REAL i4_10;
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
    REAL r4_6;
    REAL i4_6;
    REAL r4_14;
    REAL i4_14;
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
    REAL r2_1;
    REAL i2_1;
    REAL r2_3;
    REAL i2_3;
    REAL r2_5;
    REAL i2_5;
    REAL r2_7;
    REAL i2_7;
    REAL r2_9;
    REAL i2_9;
    REAL r2_11;
    REAL i2_11;
    REAL r2_13;
    REAL i2_13;
    REAL r2_15;
    REAL i2_15;
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
    REAL r3_1;
    REAL i3_1;
    REAL r3_5;
    REAL i3_5;
    REAL r3_9;
    REAL i3_9;
    REAL r3_13;
    REAL i3_13;
    REAL r3_17;
    REAL i3_17;
    REAL r3_21;
    REAL i3_21;
    REAL r3_25;
    REAL i3_25;
    REAL r3_29;
    REAL i3_29;
    REAL r4_1;
    REAL i4_1;
    REAL r4_9;
    REAL i4_9;
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
    REAL r4_5;
    REAL i4_5;
    REAL r4_13;
    REAL i4_13;
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
    REAL r3_3;
    REAL i3_3;
    REAL r3_7;
    REAL i3_7;
    REAL r3_11;
    REAL i3_11;
    REAL r3_15;
    REAL i3_15;
    REAL r3_19;
    REAL i3_19;
    REAL r3_23;
    REAL i3_23;
    REAL r3_27;
    REAL i3_27;
    REAL r3_31;
    REAL i3_31;
    REAL r4_3;
    REAL i4_3;
    REAL r4_11;
    REAL i4_11;
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
    REAL r4_7;
    REAL i4_7;
    REAL r4_15;
    REAL i4_15;
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
    int ab;
    fft_twiddle_32_closure *largs = (fft_twiddle_32_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        i = largs->a;
        l1 = (largs->nWdn * i);
        for (kp = (largs->out + i);(i < largs->b);(kp++)) {
            jp = (largs->in + i);
            r5_0 = jp[(0 * largs->m)].re;
            i5_0 = jp[(0 * largs->m)].im;
            wr = largs->W[(16 * l1)].re;
            wi = largs->W[(16 * l1)].im;
            tmpr = jp[(16 * largs->m)].re;
            tmpi = jp[(16 * largs->m)].im;
            r5_16 = ((wr * tmpr) - (wi * tmpi));
            i5_16 = ((wi * tmpr) + (wr * tmpi));
            r4_0 = (r5_0 + r5_16);
            i4_0 = (i5_0 + i5_16);
            r4_16 = (r5_0 - r5_16);
            i4_16 = (i5_0 - i5_16);
            wr = largs->W[(8 * l1)].re;
            wi = largs->W[(8 * l1)].im;
            tmpr = jp[(8 * largs->m)].re;
            tmpi = jp[(8 * largs->m)].im;
            r5_8 = ((wr * tmpr) - (wi * tmpi));
            i5_8 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(24 * l1)].re;
            wi = largs->W[(24 * l1)].im;
            tmpr = jp[(24 * largs->m)].re;
            tmpi = jp[(24 * largs->m)].im;
            r5_24 = ((wr * tmpr) - (wi * tmpi));
            i5_24 = ((wi * tmpr) + (wr * tmpi));
            r4_8 = (r5_8 + r5_24);
            i4_8 = (i5_8 + i5_24);
            r4_24 = (r5_8 - r5_24);
            i4_24 = (i5_8 - i5_24);
            r3_0 = (r4_0 + r4_8);
            i3_0 = (i4_0 + i4_8);
            r3_16 = (r4_0 - r4_8);
            i3_16 = (i4_0 - i4_8);
            r3_8 = (r4_16 + i4_24);
            i3_8 = (i4_16 - r4_24);
            r3_24 = (r4_16 - i4_24);
            i3_24 = (i4_16 + r4_24);
            wr = largs->W[(4 * l1)].re;
            wi = largs->W[(4 * l1)].im;
            tmpr = jp[(4 * largs->m)].re;
            tmpi = jp[(4 * largs->m)].im;
            r5_4 = ((wr * tmpr) - (wi * tmpi));
            i5_4 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(20 * l1)].re;
            wi = largs->W[(20 * l1)].im;
            tmpr = jp[(20 * largs->m)].re;
            tmpi = jp[(20 * largs->m)].im;
            r5_20 = ((wr * tmpr) - (wi * tmpi));
            i5_20 = ((wi * tmpr) + (wr * tmpi));
            r4_4 = (r5_4 + r5_20);
            i4_4 = (i5_4 + i5_20);
            r4_20 = (r5_4 - r5_20);
            i4_20 = (i5_4 - i5_20);
            wr = largs->W[(12 * l1)].re;
            wi = largs->W[(12 * l1)].im;
            tmpr = jp[(12 * largs->m)].re;
            tmpi = jp[(12 * largs->m)].im;
            r5_12 = ((wr * tmpr) - (wi * tmpi));
            i5_12 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(28 * l1)].re;
            wi = largs->W[(28 * l1)].im;
            tmpr = jp[(28 * largs->m)].re;
            tmpi = jp[(28 * largs->m)].im;
            r5_28 = ((wr * tmpr) - (wi * tmpi));
            i5_28 = ((wi * tmpr) + (wr * tmpi));
            r4_12 = (r5_12 + r5_28);
            i4_12 = (i5_12 + i5_28);
            r4_28 = (r5_12 - r5_28);
            i4_28 = (i5_12 - i5_28);
            r3_4 = (r4_4 + r4_12);
            i3_4 = (i4_4 + i4_12);
            r3_20 = (r4_4 - r4_12);
            i3_20 = (i4_4 - i4_12);
            r3_12 = (r4_20 + i4_28);
            i3_12 = (i4_20 - r4_28);
            r3_28 = (r4_20 - i4_28);
            i3_28 = (i4_20 + r4_28);
            r2_0 = (r3_0 + r3_4);
            i2_0 = (i3_0 + i3_4);
            r2_16 = (r3_0 - r3_4);
            i2_16 = (i3_0 - i3_4);
            tmpr = (0.70710678118699999 * (r3_12 + i3_12));
            tmpi = (0.70710678118699999 * (i3_12 - r3_12));
            r2_4 = (r3_8 + tmpr);
            i2_4 = (i3_8 + tmpi);
            r2_20 = (r3_8 - tmpr);
            i2_20 = (i3_8 - tmpi);
            r2_8 = (r3_16 + i3_20);
            i2_8 = (i3_16 - r3_20);
            r2_24 = (r3_16 - i3_20);
            i2_24 = (i3_16 + r3_20);
            tmpr = (0.70710678118699999 * (i3_28 - r3_28));
            tmpi = (0.70710678118699999 * (r3_28 + i3_28));
            r2_12 = (r3_24 + tmpr);
            i2_12 = (i3_24 - tmpi);
            r2_28 = (r3_24 - tmpr);
            i2_28 = (i3_24 + tmpi);
            wr = largs->W[(2 * l1)].re;
            wi = largs->W[(2 * l1)].im;
            tmpr = jp[(2 * largs->m)].re;
            tmpi = jp[(2 * largs->m)].im;
            r5_2 = ((wr * tmpr) - (wi * tmpi));
            i5_2 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(18 * l1)].re;
            wi = largs->W[(18 * l1)].im;
            tmpr = jp[(18 * largs->m)].re;
            tmpi = jp[(18 * largs->m)].im;
            r5_18 = ((wr * tmpr) - (wi * tmpi));
            i5_18 = ((wi * tmpr) + (wr * tmpi));
            r4_2 = (r5_2 + r5_18);
            i4_2 = (i5_2 + i5_18);
            r4_18 = (r5_2 - r5_18);
            i4_18 = (i5_2 - i5_18);
            wr = largs->W[(10 * l1)].re;
            wi = largs->W[(10 * l1)].im;
            tmpr = jp[(10 * largs->m)].re;
            tmpi = jp[(10 * largs->m)].im;
            r5_10 = ((wr * tmpr) - (wi * tmpi));
            i5_10 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(26 * l1)].re;
            wi = largs->W[(26 * l1)].im;
            tmpr = jp[(26 * largs->m)].re;
            tmpi = jp[(26 * largs->m)].im;
            r5_26 = ((wr * tmpr) - (wi * tmpi));
            i5_26 = ((wi * tmpr) + (wr * tmpi));
            r4_10 = (r5_10 + r5_26);
            i4_10 = (i5_10 + i5_26);
            r4_26 = (r5_10 - r5_26);
            i4_26 = (i5_10 - i5_26);
            r3_2 = (r4_2 + r4_10);
            i3_2 = (i4_2 + i4_10);
            r3_18 = (r4_2 - r4_10);
            i3_18 = (i4_2 - i4_10);
            r3_10 = (r4_18 + i4_26);
            i3_10 = (i4_18 - r4_26);
            r3_26 = (r4_18 - i4_26);
            i3_26 = (i4_18 + r4_26);
            wr = largs->W[(6 * l1)].re;
            wi = largs->W[(6 * l1)].im;
            tmpr = jp[(6 * largs->m)].re;
            tmpi = jp[(6 * largs->m)].im;
            r5_6 = ((wr * tmpr) - (wi * tmpi));
            i5_6 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(22 * l1)].re;
            wi = largs->W[(22 * l1)].im;
            tmpr = jp[(22 * largs->m)].re;
            tmpi = jp[(22 * largs->m)].im;
            r5_22 = ((wr * tmpr) - (wi * tmpi));
            i5_22 = ((wi * tmpr) + (wr * tmpi));
            r4_6 = (r5_6 + r5_22);
            i4_6 = (i5_6 + i5_22);
            r4_22 = (r5_6 - r5_22);
            i4_22 = (i5_6 - i5_22);
            wr = largs->W[(14 * l1)].re;
            wi = largs->W[(14 * l1)].im;
            tmpr = jp[(14 * largs->m)].re;
            tmpi = jp[(14 * largs->m)].im;
            r5_14 = ((wr * tmpr) - (wi * tmpi));
            i5_14 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(30 * l1)].re;
            wi = largs->W[(30 * l1)].im;
            tmpr = jp[(30 * largs->m)].re;
            tmpi = jp[(30 * largs->m)].im;
            r5_30 = ((wr * tmpr) - (wi * tmpi));
            i5_30 = ((wi * tmpr) + (wr * tmpi));
            r4_14 = (r5_14 + r5_30);
            i4_14 = (i5_14 + i5_30);
            r4_30 = (r5_14 - r5_30);
            i4_30 = (i5_14 - i5_30);
            r3_6 = (r4_6 + r4_14);
            i3_6 = (i4_6 + i4_14);
            r3_22 = (r4_6 - r4_14);
            i3_22 = (i4_6 - i4_14);
            r3_14 = (r4_22 + i4_30);
            i3_14 = (i4_22 - r4_30);
            r3_30 = (r4_22 - i4_30);
            i3_30 = (i4_22 + r4_30);
            r2_2 = (r3_2 + r3_6);
            i2_2 = (i3_2 + i3_6);
            r2_18 = (r3_2 - r3_6);
            i2_18 = (i3_2 - i3_6);
            tmpr = (0.70710678118699999 * (r3_14 + i3_14));
            tmpi = (0.70710678118699999 * (i3_14 - r3_14));
            r2_6 = (r3_10 + tmpr);
            i2_6 = (i3_10 + tmpi);
            r2_22 = (r3_10 - tmpr);
            i2_22 = (i3_10 - tmpi);
            r2_10 = (r3_18 + i3_22);
            i2_10 = (i3_18 - r3_22);
            r2_26 = (r3_18 - i3_22);
            i2_26 = (i3_18 + r3_22);
            tmpr = (0.70710678118699999 * (i3_30 - r3_30));
            tmpi = (0.70710678118699999 * (r3_30 + i3_30));
            r2_14 = (r3_26 + tmpr);
            i2_14 = (i3_26 - tmpi);
            r2_30 = (r3_26 - tmpr);
            i2_30 = (i3_26 + tmpi);
            r1_0 = (r2_0 + r2_2);
            i1_0 = (i2_0 + i2_2);
            r1_16 = (r2_0 - r2_2);
            i1_16 = (i2_0 - i2_2);
            tmpr = ((0.92387953251099997 * r2_6) + (0.38268343236500002 * i2_6));
            tmpi = ((0.92387953251099997 * i2_6) - (0.38268343236500002 * r2_6));
            r1_2 = (r2_4 + tmpr);
            i1_2 = (i2_4 + tmpi);
            r1_18 = (r2_4 - tmpr);
            i1_18 = (i2_4 - tmpi);
            tmpr = (0.70710678118699999 * (r2_10 + i2_10));
            tmpi = (0.70710678118699999 * (i2_10 - r2_10));
            r1_4 = (r2_8 + tmpr);
            i1_4 = (i2_8 + tmpi);
            r1_20 = (r2_8 - tmpr);
            i1_20 = (i2_8 - tmpi);
            tmpr = ((0.38268343236500002 * r2_14) + (0.92387953251099997 * i2_14));
            tmpi = ((0.38268343236500002 * i2_14) - (0.92387953251099997 * r2_14));
            r1_6 = (r2_12 + tmpr);
            i1_6 = (i2_12 + tmpi);
            r1_22 = (r2_12 - tmpr);
            i1_22 = (i2_12 - tmpi);
            r1_8 = (r2_16 + i2_18);
            i1_8 = (i2_16 - r2_18);
            r1_24 = (r2_16 - i2_18);
            i1_24 = (i2_16 + r2_18);
            tmpr = ((0.92387953251099997 * i2_22) - (0.38268343236500002 * r2_22));
            tmpi = ((0.92387953251099997 * r2_22) + (0.38268343236500002 * i2_22));
            r1_10 = (r2_20 + tmpr);
            i1_10 = (i2_20 - tmpi);
            r1_26 = (r2_20 - tmpr);
            i1_26 = (i2_20 + tmpi);
            tmpr = (0.70710678118699999 * (i2_26 - r2_26));
            tmpi = (0.70710678118699999 * (r2_26 + i2_26));
            r1_12 = (r2_24 + tmpr);
            i1_12 = (i2_24 - tmpi);
            r1_28 = (r2_24 - tmpr);
            i1_28 = (i2_24 + tmpi);
            tmpr = ((0.38268343236500002 * i2_30) - (0.92387953251099997 * r2_30));
            tmpi = ((0.38268343236500002 * r2_30) + (0.92387953251099997 * i2_30));
            r1_14 = (r2_28 + tmpr);
            i1_14 = (i2_28 - tmpi);
            r1_30 = (r2_28 - tmpr);
            i1_30 = (i2_28 + tmpi);
            wr = largs->W[(1 * l1)].re;
            wi = largs->W[(1 * l1)].im;
            tmpr = jp[(1 * largs->m)].re;
            tmpi = jp[(1 * largs->m)].im;
            r5_1 = ((wr * tmpr) - (wi * tmpi));
            i5_1 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(17 * l1)].re;
            wi = largs->W[(17 * l1)].im;
            tmpr = jp[(17 * largs->m)].re;
            tmpi = jp[(17 * largs->m)].im;
            r5_17 = ((wr * tmpr) - (wi * tmpi));
            i5_17 = ((wi * tmpr) + (wr * tmpi));
            r4_1 = (r5_1 + r5_17);
            i4_1 = (i5_1 + i5_17);
            r4_17 = (r5_1 - r5_17);
            i4_17 = (i5_1 - i5_17);
            wr = largs->W[(9 * l1)].re;
            wi = largs->W[(9 * l1)].im;
            tmpr = jp[(9 * largs->m)].re;
            tmpi = jp[(9 * largs->m)].im;
            r5_9 = ((wr * tmpr) - (wi * tmpi));
            i5_9 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(25 * l1)].re;
            wi = largs->W[(25 * l1)].im;
            tmpr = jp[(25 * largs->m)].re;
            tmpi = jp[(25 * largs->m)].im;
            r5_25 = ((wr * tmpr) - (wi * tmpi));
            i5_25 = ((wi * tmpr) + (wr * tmpi));
            r4_9 = (r5_9 + r5_25);
            i4_9 = (i5_9 + i5_25);
            r4_25 = (r5_9 - r5_25);
            i4_25 = (i5_9 - i5_25);
            r3_1 = (r4_1 + r4_9);
            i3_1 = (i4_1 + i4_9);
            r3_17 = (r4_1 - r4_9);
            i3_17 = (i4_1 - i4_9);
            r3_9 = (r4_17 + i4_25);
            i3_9 = (i4_17 - r4_25);
            r3_25 = (r4_17 - i4_25);
            i3_25 = (i4_17 + r4_25);
            wr = largs->W[(5 * l1)].re;
            wi = largs->W[(5 * l1)].im;
            tmpr = jp[(5 * largs->m)].re;
            tmpi = jp[(5 * largs->m)].im;
            r5_5 = ((wr * tmpr) - (wi * tmpi));
            i5_5 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(21 * l1)].re;
            wi = largs->W[(21 * l1)].im;
            tmpr = jp[(21 * largs->m)].re;
            tmpi = jp[(21 * largs->m)].im;
            r5_21 = ((wr * tmpr) - (wi * tmpi));
            i5_21 = ((wi * tmpr) + (wr * tmpi));
            r4_5 = (r5_5 + r5_21);
            i4_5 = (i5_5 + i5_21);
            r4_21 = (r5_5 - r5_21);
            i4_21 = (i5_5 - i5_21);
            wr = largs->W[(13 * l1)].re;
            wi = largs->W[(13 * l1)].im;
            tmpr = jp[(13 * largs->m)].re;
            tmpi = jp[(13 * largs->m)].im;
            r5_13 = ((wr * tmpr) - (wi * tmpi));
            i5_13 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(29 * l1)].re;
            wi = largs->W[(29 * l1)].im;
            tmpr = jp[(29 * largs->m)].re;
            tmpi = jp[(29 * largs->m)].im;
            r5_29 = ((wr * tmpr) - (wi * tmpi));
            i5_29 = ((wi * tmpr) + (wr * tmpi));
            r4_13 = (r5_13 + r5_29);
            i4_13 = (i5_13 + i5_29);
            r4_29 = (r5_13 - r5_29);
            i4_29 = (i5_13 - i5_29);
            r3_5 = (r4_5 + r4_13);
            i3_5 = (i4_5 + i4_13);
            r3_21 = (r4_5 - r4_13);
            i3_21 = (i4_5 - i4_13);
            r3_13 = (r4_21 + i4_29);
            i3_13 = (i4_21 - r4_29);
            r3_29 = (r4_21 - i4_29);
            i3_29 = (i4_21 + r4_29);
            r2_1 = (r3_1 + r3_5);
            i2_1 = (i3_1 + i3_5);
            r2_17 = (r3_1 - r3_5);
            i2_17 = (i3_1 - i3_5);
            tmpr = (0.70710678118699999 * (r3_13 + i3_13));
            tmpi = (0.70710678118699999 * (i3_13 - r3_13));
            r2_5 = (r3_9 + tmpr);
            i2_5 = (i3_9 + tmpi);
            r2_21 = (r3_9 - tmpr);
            i2_21 = (i3_9 - tmpi);
            r2_9 = (r3_17 + i3_21);
            i2_9 = (i3_17 - r3_21);
            r2_25 = (r3_17 - i3_21);
            i2_25 = (i3_17 + r3_21);
            tmpr = (0.70710678118699999 * (i3_29 - r3_29));
            tmpi = (0.70710678118699999 * (r3_29 + i3_29));
            r2_13 = (r3_25 + tmpr);
            i2_13 = (i3_25 - tmpi);
            r2_29 = (r3_25 - tmpr);
            i2_29 = (i3_25 + tmpi);
            wr = largs->W[(3 * l1)].re;
            wi = largs->W[(3 * l1)].im;
            tmpr = jp[(3 * largs->m)].re;
            tmpi = jp[(3 * largs->m)].im;
            r5_3 = ((wr * tmpr) - (wi * tmpi));
            i5_3 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(19 * l1)].re;
            wi = largs->W[(19 * l1)].im;
            tmpr = jp[(19 * largs->m)].re;
            tmpi = jp[(19 * largs->m)].im;
            r5_19 = ((wr * tmpr) - (wi * tmpi));
            i5_19 = ((wi * tmpr) + (wr * tmpi));
            r4_3 = (r5_3 + r5_19);
            i4_3 = (i5_3 + i5_19);
            r4_19 = (r5_3 - r5_19);
            i4_19 = (i5_3 - i5_19);
            wr = largs->W[(11 * l1)].re;
            wi = largs->W[(11 * l1)].im;
            tmpr = jp[(11 * largs->m)].re;
            tmpi = jp[(11 * largs->m)].im;
            r5_11 = ((wr * tmpr) - (wi * tmpi));
            i5_11 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(27 * l1)].re;
            wi = largs->W[(27 * l1)].im;
            tmpr = jp[(27 * largs->m)].re;
            tmpi = jp[(27 * largs->m)].im;
            r5_27 = ((wr * tmpr) - (wi * tmpi));
            i5_27 = ((wi * tmpr) + (wr * tmpi));
            r4_11 = (r5_11 + r5_27);
            i4_11 = (i5_11 + i5_27);
            r4_27 = (r5_11 - r5_27);
            i4_27 = (i5_11 - i5_27);
            r3_3 = (r4_3 + r4_11);
            i3_3 = (i4_3 + i4_11);
            r3_19 = (r4_3 - r4_11);
            i3_19 = (i4_3 - i4_11);
            r3_11 = (r4_19 + i4_27);
            i3_11 = (i4_19 - r4_27);
            r3_27 = (r4_19 - i4_27);
            i3_27 = (i4_19 + r4_27);
            wr = largs->W[(7 * l1)].re;
            wi = largs->W[(7 * l1)].im;
            tmpr = jp[(7 * largs->m)].re;
            tmpi = jp[(7 * largs->m)].im;
            r5_7 = ((wr * tmpr) - (wi * tmpi));
            i5_7 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(23 * l1)].re;
            wi = largs->W[(23 * l1)].im;
            tmpr = jp[(23 * largs->m)].re;
            tmpi = jp[(23 * largs->m)].im;
            r5_23 = ((wr * tmpr) - (wi * tmpi));
            i5_23 = ((wi * tmpr) + (wr * tmpi));
            r4_7 = (r5_7 + r5_23);
            i4_7 = (i5_7 + i5_23);
            r4_23 = (r5_7 - r5_23);
            i4_23 = (i5_7 - i5_23);
            wr = largs->W[(15 * l1)].re;
            wi = largs->W[(15 * l1)].im;
            tmpr = jp[(15 * largs->m)].re;
            tmpi = jp[(15 * largs->m)].im;
            r5_15 = ((wr * tmpr) - (wi * tmpi));
            i5_15 = ((wi * tmpr) + (wr * tmpi));
            wr = largs->W[(31 * l1)].re;
            wi = largs->W[(31 * l1)].im;
            tmpr = jp[(31 * largs->m)].re;
            tmpi = jp[(31 * largs->m)].im;
            r5_31 = ((wr * tmpr) - (wi * tmpi));
            i5_31 = ((wi * tmpr) + (wr * tmpi));
            r4_15 = (r5_15 + r5_31);
            i4_15 = (i5_15 + i5_31);
            r4_31 = (r5_15 - r5_31);
            i4_31 = (i5_15 - i5_31);
            r3_7 = (r4_7 + r4_15);
            i3_7 = (i4_7 + i4_15);
            r3_23 = (r4_7 - r4_15);
            i3_23 = (i4_7 - i4_15);
            r3_15 = (r4_23 + i4_31);
            i3_15 = (i4_23 - r4_31);
            r3_31 = (r4_23 - i4_31);
            i3_31 = (i4_23 + r4_31);
            r2_3 = (r3_3 + r3_7);
            i2_3 = (i3_3 + i3_7);
            r2_19 = (r3_3 - r3_7);
            i2_19 = (i3_3 - i3_7);
            tmpr = (0.70710678118699999 * (r3_15 + i3_15));
            tmpi = (0.70710678118699999 * (i3_15 - r3_15));
            r2_7 = (r3_11 + tmpr);
            i2_7 = (i3_11 + tmpi);
            r2_23 = (r3_11 - tmpr);
            i2_23 = (i3_11 - tmpi);
            r2_11 = (r3_19 + i3_23);
            i2_11 = (i3_19 - r3_23);
            r2_27 = (r3_19 - i3_23);
            i2_27 = (i3_19 + r3_23);
            tmpr = (0.70710678118699999 * (i3_31 - r3_31));
            tmpi = (0.70710678118699999 * (r3_31 + i3_31));
            r2_15 = (r3_27 + tmpr);
            i2_15 = (i3_27 - tmpi);
            r2_31 = (r3_27 - tmpr);
            i2_31 = (i3_27 + tmpi);
            r1_1 = (r2_1 + r2_3);
            i1_1 = (i2_1 + i2_3);
            r1_17 = (r2_1 - r2_3);
            i1_17 = (i2_1 - i2_3);
            tmpr = ((0.92387953251099997 * r2_7) + (0.38268343236500002 * i2_7));
            tmpi = ((0.92387953251099997 * i2_7) - (0.38268343236500002 * r2_7));
            r1_3 = (r2_5 + tmpr);
            i1_3 = (i2_5 + tmpi);
            r1_19 = (r2_5 - tmpr);
            i1_19 = (i2_5 - tmpi);
            tmpr = (0.70710678118699999 * (r2_11 + i2_11));
            tmpi = (0.70710678118699999 * (i2_11 - r2_11));
            r1_5 = (r2_9 + tmpr);
            i1_5 = (i2_9 + tmpi);
            r1_21 = (r2_9 - tmpr);
            i1_21 = (i2_9 - tmpi);
            tmpr = ((0.38268343236500002 * r2_15) + (0.92387953251099997 * i2_15));
            tmpi = ((0.38268343236500002 * i2_15) - (0.92387953251099997 * r2_15));
            r1_7 = (r2_13 + tmpr);
            i1_7 = (i2_13 + tmpi);
            r1_23 = (r2_13 - tmpr);
            i1_23 = (i2_13 - tmpi);
            r1_9 = (r2_17 + i2_19);
            i1_9 = (i2_17 - r2_19);
            r1_25 = (r2_17 - i2_19);
            i1_25 = (i2_17 + r2_19);
            tmpr = ((0.92387953251099997 * i2_23) - (0.38268343236500002 * r2_23));
            tmpi = ((0.92387953251099997 * r2_23) + (0.38268343236500002 * i2_23));
            r1_11 = (r2_21 + tmpr);
            i1_11 = (i2_21 - tmpi);
            r1_27 = (r2_21 - tmpr);
            i1_27 = (i2_21 + tmpi);
            tmpr = (0.70710678118699999 * (i2_27 - r2_27));
            tmpi = (0.70710678118699999 * (r2_27 + i2_27));
            r1_13 = (r2_25 + tmpr);
            i1_13 = (i2_25 - tmpi);
            r1_29 = (r2_25 - tmpr);
            i1_29 = (i2_25 + tmpi);
            tmpr = ((0.38268343236500002 * i2_31) - (0.92387953251099997 * r2_31));
            tmpi = ((0.38268343236500002 * r2_31) + (0.92387953251099997 * i2_31));
            r1_15 = (r2_29 + tmpr);
            i1_15 = (i2_29 - tmpi);
            r1_31 = (r2_29 - tmpr);
            i1_31 = (i2_29 + tmpi);
            kp[(0 * largs->m)].re = (r1_0 + r1_1);
            kp[(0 * largs->m)].im = (i1_0 + i1_1);
            kp[(16 * largs->m)].re = (r1_0 - r1_1);
            kp[(16 * largs->m)].im = (i1_0 - i1_1);
            tmpr = ((0.98078528040299994 * r1_3) + (0.19509032201599999 * i1_3));
            tmpi = ((0.98078528040299994 * i1_3) - (0.19509032201599999 * r1_3));
            kp[(1 * largs->m)].re = (r1_2 + tmpr);
            kp[(1 * largs->m)].im = (i1_2 + tmpi);
            kp[(17 * largs->m)].re = (r1_2 - tmpr);
            kp[(17 * largs->m)].im = (i1_2 - tmpi);
            tmpr = ((0.92387953251099997 * r1_5) + (0.38268343236500002 * i1_5));
            tmpi = ((0.92387953251099997 * i1_5) - (0.38268343236500002 * r1_5));
            kp[(2 * largs->m)].re = (r1_4 + tmpr);
            kp[(2 * largs->m)].im = (i1_4 + tmpi);
            kp[(18 * largs->m)].re = (r1_4 - tmpr);
            kp[(18 * largs->m)].im = (i1_4 - tmpi);
            tmpr = ((0.83146961230299998 * r1_7) + (0.55557023301999997 * i1_7));
            tmpi = ((0.83146961230299998 * i1_7) - (0.55557023301999997 * r1_7));
            kp[(3 * largs->m)].re = (r1_6 + tmpr);
            kp[(3 * largs->m)].im = (i1_6 + tmpi);
            kp[(19 * largs->m)].re = (r1_6 - tmpr);
            kp[(19 * largs->m)].im = (i1_6 - tmpi);
            tmpr = (0.70710678118699999 * (r1_9 + i1_9));
            tmpi = (0.70710678118699999 * (i1_9 - r1_9));
            kp[(4 * largs->m)].re = (r1_8 + tmpr);
            kp[(4 * largs->m)].im = (i1_8 + tmpi);
            kp[(20 * largs->m)].re = (r1_8 - tmpr);
            kp[(20 * largs->m)].im = (i1_8 - tmpi);
            tmpr = ((0.55557023301999997 * r1_11) + (0.83146961230299998 * i1_11));
            tmpi = ((0.55557023301999997 * i1_11) - (0.83146961230299998 * r1_11));
            kp[(5 * largs->m)].re = (r1_10 + tmpr);
            kp[(5 * largs->m)].im = (i1_10 + tmpi);
            kp[(21 * largs->m)].re = (r1_10 - tmpr);
            kp[(21 * largs->m)].im = (i1_10 - tmpi);
            tmpr = ((0.38268343236500002 * r1_13) + (0.92387953251099997 * i1_13));
            tmpi = ((0.38268343236500002 * i1_13) - (0.92387953251099997 * r1_13));
            kp[(6 * largs->m)].re = (r1_12 + tmpr);
            kp[(6 * largs->m)].im = (i1_12 + tmpi);
            kp[(22 * largs->m)].re = (r1_12 - tmpr);
            kp[(22 * largs->m)].im = (i1_12 - tmpi);
            tmpr = ((0.19509032201599999 * r1_15) + (0.98078528040299994 * i1_15));
            tmpi = ((0.19509032201599999 * i1_15) - (0.98078528040299994 * r1_15));
            kp[(7 * largs->m)].re = (r1_14 + tmpr);
            kp[(7 * largs->m)].im = (i1_14 + tmpi);
            kp[(23 * largs->m)].re = (r1_14 - tmpr);
            kp[(23 * largs->m)].im = (i1_14 - tmpi);
            kp[(8 * largs->m)].re = (r1_16 + i1_17);
            kp[(8 * largs->m)].im = (i1_16 - r1_17);
            kp[(24 * largs->m)].re = (r1_16 - i1_17);
            kp[(24 * largs->m)].im = (i1_16 + r1_17);
            tmpr = ((0.98078528040299994 * i1_19) - (0.19509032201599999 * r1_19));
            tmpi = ((0.98078528040299994 * r1_19) + (0.19509032201599999 * i1_19));
            kp[(9 * largs->m)].re = (r1_18 + tmpr);
            kp[(9 * largs->m)].im = (i1_18 - tmpi);
            kp[(25 * largs->m)].re = (r1_18 - tmpr);
            kp[(25 * largs->m)].im = (i1_18 + tmpi);
            tmpr = ((0.92387953251099997 * i1_21) - (0.38268343236500002 * r1_21));
            tmpi = ((0.92387953251099997 * r1_21) + (0.38268343236500002 * i1_21));
            kp[(10 * largs->m)].re = (r1_20 + tmpr);
            kp[(10 * largs->m)].im = (i1_20 - tmpi);
            kp[(26 * largs->m)].re = (r1_20 - tmpr);
            kp[(26 * largs->m)].im = (i1_20 + tmpi);
            tmpr = ((0.83146961230299998 * i1_23) - (0.55557023301999997 * r1_23));
            tmpi = ((0.83146961230299998 * r1_23) + (0.55557023301999997 * i1_23));
            kp[(11 * largs->m)].re = (r1_22 + tmpr);
            kp[(11 * largs->m)].im = (i1_22 - tmpi);
            kp[(27 * largs->m)].re = (r1_22 - tmpr);
            kp[(27 * largs->m)].im = (i1_22 + tmpi);
            tmpr = (0.70710678118699999 * (i1_25 - r1_25));
            tmpi = (0.70710678118699999 * (r1_25 + i1_25));
            kp[(12 * largs->m)].re = (r1_24 + tmpr);
            kp[(12 * largs->m)].im = (i1_24 - tmpi);
            kp[(28 * largs->m)].re = (r1_24 - tmpr);
            kp[(28 * largs->m)].im = (i1_24 + tmpi);
            tmpr = ((0.55557023301999997 * i1_27) - (0.83146961230299998 * r1_27));
            tmpi = ((0.55557023301999997 * r1_27) + (0.83146961230299998 * i1_27));
            kp[(13 * largs->m)].re = (r1_26 + tmpr);
            kp[(13 * largs->m)].im = (i1_26 - tmpi);
            kp[(29 * largs->m)].re = (r1_26 - tmpr);
            kp[(29 * largs->m)].im = (i1_26 + tmpi);
            tmpr = ((0.38268343236500002 * i1_29) - (0.92387953251099997 * r1_29));
            tmpi = ((0.38268343236500002 * r1_29) + (0.92387953251099997 * i1_29));
            kp[(14 * largs->m)].re = (r1_28 + tmpr);
            kp[(14 * largs->m)].im = (i1_28 - tmpi);
            kp[(30 * largs->m)].re = (r1_28 - tmpr);
            kp[(30 * largs->m)].im = (i1_28 + tmpi);
            tmpr = ((0.19509032201599999 * i1_31) - (0.98078528040299994 * r1_31));
            tmpi = ((0.19509032201599999 * r1_31) + (0.98078528040299994 * i1_31));
            kp[(15 * largs->m)].re = (r1_30 + tmpr);
            kp[(15 * largs->m)].im = (i1_30 - tmpi);
            kp[(31 * largs->m)].re = (r1_30 - tmpr);
            kp[(31 * largs->m)].im = (i1_30 + tmpi);
            (i++);
            l1 = (l1 + largs->nWdn);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        fft_twiddle_32_cont0_closure SN_fft_twiddle_32_cont0c(largs->k);
        spawn_next<fft_twiddle_32_cont0_closure> SN_fft_twiddle_32_cont0(SN_fft_twiddle_32_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_twiddle_32_cont0, &sp0k);
        fft_twiddle_32_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.W = largs->W;
        sp0c.nW = largs->nW;
        sp0c.nWdn = largs->nWdn;
        sp0c.m = largs->m;
        spawn<fft_twiddle_32_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_twiddle_32_cont0, &sp1k);
        fft_twiddle_32_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.W = largs->W;
        sp1c.nW = largs->nW;
        sp1c.nWdn = largs->nWdn;
        sp1c.m = largs->m;
        spawn<fft_twiddle_32_closure> sp1(sp1c);

        // Original sync was here
    }
}
THREAD(fft_unshuffle_32) {
    int i;
    const COMPLEX *ip;
    COMPLEX *jp;
    int ab;
    fft_unshuffle_32_closure *largs = (fft_unshuffle_32_closure*)(args.get());
    if (((largs->b - largs->a) < 128)) {
        ip = (largs->in + (largs->a * 32));
        for (i = largs->a;(i < largs->b);(++i)) {
            jp = (largs->out + i);
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
            jp = (jp + (2 * largs->m));
            jp[0] = ip[0];
            jp[largs->m] = ip[1];
            ip = (ip + 2);
        }
        SEND_ARGUMENT(largs->k, 0);
    } else {
        ab = ((largs->a + largs->b) / 2);
        fft_unshuffle_32_cont0_closure SN_fft_unshuffle_32_cont0c(largs->k);
        spawn_next<fft_unshuffle_32_cont0_closure> SN_fft_unshuffle_32_cont0(SN_fft_unshuffle_32_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_unshuffle_32_cont0, &sp0k);
        fft_unshuffle_32_closure sp0c(sp0k);
        sp0c.a = largs->a;
        sp0c.b = ab;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.m = largs->m;
        spawn<fft_unshuffle_32_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_fft_unshuffle_32_cont0, &sp1k);
        fft_unshuffle_32_closure sp1c(sp1k);
        sp1c.a = ab;
        sp1c.b = largs->b;
        sp1c.in = largs->in;
        sp1c.out = largs->out;
        sp1c.m = largs->m;
        spawn<fft_unshuffle_32_closure> sp1(sp1c);

        // Original sync was here
    }
}
THREAD(fft_aux) {
    int r;
    int m;
    int k0;
    int k1;
    fft_aux_closure *largs = (fft_aux_closure*)(args.get());
    if ((largs->n == 32)) {
        fft_base_32(largs->in,largs->out);
        SEND_ARGUMENT(largs->k, 0);
    } else {
        if ((largs->n == 16)) {
            fft_base_16(largs->in,largs->out);
            SEND_ARGUMENT(largs->k, 0);
        } else {
            if ((largs->n == 8)) {
                fft_base_8(largs->in,largs->out);
                SEND_ARGUMENT(largs->k, 0);
            } else {
                if ((largs->n == 4)) {
                    fft_base_4(largs->in,largs->out);
                    SEND_ARGUMENT(largs->k, 0);
                } else {
                    if ((largs->n == 2)) {
                        fft_base_2(largs->in,largs->out);
                        SEND_ARGUMENT(largs->k, 0);
                    } else {
                        r = *(largs->factors);
                        m = (largs->n / r);
                        if ((r < largs->n)) {
                            if ((r == 32)) {
                                fft_aux_cont5_closure SN_fft_aux_cont5c(largs->k);
                                spawn_next<fft_aux_cont5_closure> SN_fft_aux_cont5(SN_fft_aux_cont5c);
                                cont sp0k;
                                SN_BIND_VOID(SN_fft_aux_cont5, &sp0k);
                                fft_unshuffle_32_closure sp0c(sp0k);
                                sp0c.a = 0;
                                sp0c.b = m;
                                sp0c.in = largs->in;
                                sp0c.out = largs->out;
                                sp0c.m = m;
                                spawn<fft_unshuffle_32_closure> sp0(sp0c);

                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->k0 = k0;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->k1 = k1;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->m = m;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->r = r;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->nW = largs->nW;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->W = largs->W;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->factors = largs->factors;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->out = largs->out;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->in = largs->in;
                                ((fft_aux_cont5_closure*)SN_fft_aux_cont5.cls.get())->n = largs->n;
                                // Original sync was here
                            } else {
                                if ((r == 16)) {
                                    fft_aux_cont4_closure SN_fft_aux_cont4c(largs->k);
                                    spawn_next<fft_aux_cont4_closure> SN_fft_aux_cont4(SN_fft_aux_cont4c);
                                    cont sp1k;
                                    SN_BIND_VOID(SN_fft_aux_cont4, &sp1k);
                                    fft_unshuffle_16_closure sp1c(sp1k);
                                    sp1c.a = 0;
                                    sp1c.b = m;
                                    sp1c.in = largs->in;
                                    sp1c.out = largs->out;
                                    sp1c.m = m;
                                    spawn<fft_unshuffle_16_closure> sp1(sp1c);

                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->k0 = k0;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->k1 = k1;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->m = m;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->r = r;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->nW = largs->nW;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->W = largs->W;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->factors = largs->factors;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->out = largs->out;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->in = largs->in;
                                    ((fft_aux_cont4_closure*)SN_fft_aux_cont4.cls.get())->n = largs->n;
                                    // Original sync was here
                                } else {
                                    if ((r == 8)) {
                                        fft_aux_cont3_closure SN_fft_aux_cont3c(largs->k);
                                        spawn_next<fft_aux_cont3_closure> SN_fft_aux_cont3(SN_fft_aux_cont3c);
                                        cont sp2k;
                                        SN_BIND_VOID(SN_fft_aux_cont3, &sp2k);
                                        fft_unshuffle_8_closure sp2c(sp2k);
                                        sp2c.a = 0;
                                        sp2c.b = m;
                                        sp2c.in = largs->in;
                                        sp2c.out = largs->out;
                                        sp2c.m = m;
                                        spawn<fft_unshuffle_8_closure> sp2(sp2c);

                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->k0 = k0;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->k1 = k1;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->m = m;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->r = r;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->nW = largs->nW;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->W = largs->W;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->factors = largs->factors;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->out = largs->out;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->in = largs->in;
                                        ((fft_aux_cont3_closure*)SN_fft_aux_cont3.cls.get())->n = largs->n;
                                        // Original sync was here
                                    } else {
                                        if ((r == 4)) {
                                            fft_aux_cont2_closure SN_fft_aux_cont2c(largs->k);
                                            spawn_next<fft_aux_cont2_closure> SN_fft_aux_cont2(SN_fft_aux_cont2c);
                                            cont sp3k;
                                            SN_BIND_VOID(SN_fft_aux_cont2, &sp3k);
                                            fft_unshuffle_4_closure sp3c(sp3k);
                                            sp3c.a = 0;
                                            sp3c.b = m;
                                            sp3c.in = largs->in;
                                            sp3c.out = largs->out;
                                            sp3c.m = m;
                                            spawn<fft_unshuffle_4_closure> sp3(sp3c);

                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->k0 = k0;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->k1 = k1;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->m = m;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->r = r;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->nW = largs->nW;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->W = largs->W;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->factors = largs->factors;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->out = largs->out;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->in = largs->in;
                                            ((fft_aux_cont2_closure*)SN_fft_aux_cont2.cls.get())->n = largs->n;
                                            // Original sync was here
                                        } else {
                                            if ((r == 2)) {
                                                fft_aux_cont1_closure SN_fft_aux_cont1c(largs->k);
                                                spawn_next<fft_aux_cont1_closure> SN_fft_aux_cont1(SN_fft_aux_cont1c);
                                                cont sp4k;
                                                SN_BIND_VOID(SN_fft_aux_cont1, &sp4k);
                                                fft_unshuffle_2_closure sp4c(sp4k);
                                                sp4c.a = 0;
                                                sp4c.b = m;
                                                sp4c.in = largs->in;
                                                sp4c.out = largs->out;
                                                sp4c.m = m;
                                                spawn<fft_unshuffle_2_closure> sp4(sp4c);

                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->k0 = k0;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->k1 = k1;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->m = m;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->r = r;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->nW = largs->nW;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->W = largs->W;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->factors = largs->factors;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->out = largs->out;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->in = largs->in;
                                                ((fft_aux_cont1_closure*)SN_fft_aux_cont1.cls.get())->n = largs->n;
                                                // Original sync was here
                                            } else {
                                                fft_aux_cont0_closure SN_fft_aux_cont0c(largs->k);
                                                spawn_next<fft_aux_cont0_closure> SN_fft_aux_cont0(SN_fft_aux_cont0c);
                                                cont sp5k;
                                                SN_BIND_VOID(SN_fft_aux_cont0, &sp5k);
                                                unshuffle_closure sp5c(sp5k);
                                                sp5c.a = 0;
                                                sp5c.b = m;
                                                sp5c.in = largs->in;
                                                sp5c.out = largs->out;
                                                sp5c.r = r;
                                                sp5c.m = m;
                                                spawn<unshuffle_closure> sp5(sp5c);

                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->k0 = k0;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->k1 = k1;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->m = m;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->r = r;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->nW = largs->nW;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->W = largs->W;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->factors = largs->factors;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->out = largs->out;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->in = largs->in;
                                                ((fft_aux_cont0_closure*)SN_fft_aux_cont0.cls.get())->n = largs->n;
                                                // Original sync was here
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            auto sp6c = std::make_shared<fft_aux_afterif0_closure>(largs->k);
                            sp6c->n = largs->n;
                            sp6c->in = largs->in;
                            sp6c->out = largs->out;
                            sp6c->factors = largs->factors;
                            sp6c->W = largs->W;
                            sp6c->nW = largs->nW;
                            sp6c->r = r;
                            sp6c->m = m;
                            sp6c->k0 = k0;
                            sp6c->k1 = k1;
                            cilk_spawn taskSpawn(sp6c->getTask(), sp6c);
                            return;
                        }
                    }
                }
            }
        }
    }
}
void cilk_fft(int n, COMPLEX *in, COMPLEX *out) {
    int factors[40];
    int *p;
    int l;
    int r;
    COMPLEX *W;
    p = factors;
    l = n;
    W = ((COMPLEX *) malloc(((n + 1) * sizeof(COMPLEX))));
    cilk_fft_cont0_closure SN_cilk_fft_cont0c(CONT_DUMMY);
    spawn_next<cilk_fft_cont0_closure> SN_cilk_fft_cont0(SN_cilk_fft_cont0c);
    cont sp0k;
    SN_BIND_VOID(SN_cilk_fft_cont0, &sp0k);
    compute_w_coefficients_closure sp0c(sp0k);
    sp0c.n = n;
    sp0c.a = 0;
    sp0c.b = (n / 2);
    sp0c.W = W;
    spawn<compute_w_coefficients_closure> sp0(sp0c);

    do {
    r = factor(l);
    *p++ = r;
    l /= r;
} while (l > 1);
;
    ((cilk_fft_cont0_closure*)SN_cilk_fft_cont0.cls.get())->W = W;
    std::memcpy(((cilk_fft_cont0_closure*)SN_cilk_fft_cont0.cls.get())->factors, factors, sizeof(factors));
    ((cilk_fft_cont0_closure*)SN_cilk_fft_cont0.cls.get())->out = out;
    ((cilk_fft_cont0_closure*)SN_cilk_fft_cont0.cls.get())->in = in;
    ((cilk_fft_cont0_closure*)SN_cilk_fft_cont0.cls.get())->n = n;
    // Original sync was here
}
THREAD(test_fft_elem) {
    int i;
    COMPLEX sum;
    COMPLEX w;
    REAL pi;
    test_fft_elem_closure *largs = (test_fft_elem_closure*)(args.get());
    pi = 3.1415926535897931;
    sum.im = 0.;
    sum.re = sum.im;
    for (i = 0;(i < largs->n);(++i)) {
        w.re = cos((((2. * pi) * ((i * largs->j) % largs->n)) / largs->n));
        w.im = (-sin((((2. * pi) * ((i * largs->j) % largs->n)) / largs->n)));
        ((sum).re) += ((largs->in[i]).re) * ((w).re) - ((largs->in[i]).im) * ((w).im);
        ((sum).im) += ((largs->in[i]).im) * ((w).re) + ((largs->in[i]).re) * ((w).im);
    }
    largs->out[largs->j] = sum;
    SEND_ARGUMENT(largs->k, 0);
}
void test_fft(int n, COMPLEX *in, COMPLEX *out) {
    int j;
    int j0;
    j = 0;
    test_fft_cont0_closure SN_test_fft_cont0c(CONT_DUMMY);
    spawn_next<test_fft_cont0_closure> SN_test_fft_cont0(SN_test_fft_cont0c);
    for (j0 = 0;(j0 < n);(++j0)) {
        cont sp0k;
        SN_BIND_VOID(SN_test_fft_cont0, &sp0k);
        test_fft_elem_closure sp0c(sp0k);
        sp0c.n = n;
        sp0c.j = j0;
        sp0c.in = in;
        sp0c.out = out;
        spawn<test_fft_elem_closure> sp0(sp0c);

    }
    // Original sync was here
}
THREAD(fft_aux_afterif0) {
    fft_aux_afterif0_closure *largs = (fft_aux_afterif0_closure*)(args.get());
    if ((largs->r == 2)) {
        fft_aux_afterif0_cont5_closure SN_fft_aux_afterif0_cont5c(largs->k);
        spawn_next<fft_aux_afterif0_cont5_closure> SN_fft_aux_afterif0_cont5(SN_fft_aux_afterif0_cont5c);
        cont sp0k;
        SN_BIND_VOID(SN_fft_aux_afterif0_cont5, &sp0k);
        fft_twiddle_2_closure sp0c(sp0k);
        sp0c.a = 0;
        sp0c.b = largs->m;
        sp0c.in = largs->in;
        sp0c.out = largs->out;
        sp0c.W = largs->W;
        sp0c.nW = largs->nW;
        sp0c.nWdn = (largs->nW / largs->n);
        sp0c.m = largs->m;
        spawn<fft_twiddle_2_closure> sp0(sp0c);

        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->k1 = largs->k1;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->k0 = largs->k0;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->m = largs->m;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->r = largs->r;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->nW = largs->nW;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->W = largs->W;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->factors = largs->factors;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->out = largs->out;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->in = largs->in;
        ((fft_aux_afterif0_cont5_closure*)SN_fft_aux_afterif0_cont5.cls.get())->n = largs->n;
        // Original sync was here
    } else {
        if ((largs->r == 4)) {
            fft_aux_afterif0_cont4_closure SN_fft_aux_afterif0_cont4c(largs->k);
            spawn_next<fft_aux_afterif0_cont4_closure> SN_fft_aux_afterif0_cont4(SN_fft_aux_afterif0_cont4c);
            cont sp1k;
            SN_BIND_VOID(SN_fft_aux_afterif0_cont4, &sp1k);
            fft_twiddle_4_closure sp1c(sp1k);
            sp1c.a = 0;
            sp1c.b = largs->m;
            sp1c.in = largs->in;
            sp1c.out = largs->out;
            sp1c.W = largs->W;
            sp1c.nW = largs->nW;
            sp1c.nWdn = (largs->nW / largs->n);
            sp1c.m = largs->m;
            spawn<fft_twiddle_4_closure> sp1(sp1c);

            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->k1 = largs->k1;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->k0 = largs->k0;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->m = largs->m;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->r = largs->r;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->nW = largs->nW;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->W = largs->W;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->factors = largs->factors;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->out = largs->out;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->in = largs->in;
            ((fft_aux_afterif0_cont4_closure*)SN_fft_aux_afterif0_cont4.cls.get())->n = largs->n;
            // Original sync was here
        } else {
            if ((largs->r == 8)) {
                fft_aux_afterif0_cont3_closure SN_fft_aux_afterif0_cont3c(largs->k);
                spawn_next<fft_aux_afterif0_cont3_closure> SN_fft_aux_afterif0_cont3(SN_fft_aux_afterif0_cont3c);
                cont sp2k;
                SN_BIND_VOID(SN_fft_aux_afterif0_cont3, &sp2k);
                fft_twiddle_8_closure sp2c(sp2k);
                sp2c.a = 0;
                sp2c.b = largs->m;
                sp2c.in = largs->in;
                sp2c.out = largs->out;
                sp2c.W = largs->W;
                sp2c.nW = largs->nW;
                sp2c.nWdn = (largs->nW / largs->n);
                sp2c.m = largs->m;
                spawn<fft_twiddle_8_closure> sp2(sp2c);

                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->k1 = largs->k1;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->k0 = largs->k0;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->m = largs->m;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->r = largs->r;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->nW = largs->nW;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->W = largs->W;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->factors = largs->factors;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->out = largs->out;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->in = largs->in;
                ((fft_aux_afterif0_cont3_closure*)SN_fft_aux_afterif0_cont3.cls.get())->n = largs->n;
                // Original sync was here
            } else {
                if ((largs->r == 16)) {
                    fft_aux_afterif0_cont2_closure SN_fft_aux_afterif0_cont2c(largs->k);
                    spawn_next<fft_aux_afterif0_cont2_closure> SN_fft_aux_afterif0_cont2(SN_fft_aux_afterif0_cont2c);
                    cont sp3k;
                    SN_BIND_VOID(SN_fft_aux_afterif0_cont2, &sp3k);
                    fft_twiddle_16_closure sp3c(sp3k);
                    sp3c.a = 0;
                    sp3c.b = largs->m;
                    sp3c.in = largs->in;
                    sp3c.out = largs->out;
                    sp3c.W = largs->W;
                    sp3c.nW = largs->nW;
                    sp3c.nWdn = (largs->nW / largs->n);
                    sp3c.m = largs->m;
                    spawn<fft_twiddle_16_closure> sp3(sp3c);

                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->k1 = largs->k1;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->k0 = largs->k0;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->m = largs->m;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->r = largs->r;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->nW = largs->nW;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->W = largs->W;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->factors = largs->factors;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->out = largs->out;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->in = largs->in;
                    ((fft_aux_afterif0_cont2_closure*)SN_fft_aux_afterif0_cont2.cls.get())->n = largs->n;
                    // Original sync was here
                } else {
                    if ((largs->r == 32)) {
                        fft_aux_afterif0_cont1_closure SN_fft_aux_afterif0_cont1c(largs->k);
                        spawn_next<fft_aux_afterif0_cont1_closure> SN_fft_aux_afterif0_cont1(SN_fft_aux_afterif0_cont1c);
                        cont sp4k;
                        SN_BIND_VOID(SN_fft_aux_afterif0_cont1, &sp4k);
                        fft_twiddle_32_closure sp4c(sp4k);
                        sp4c.a = 0;
                        sp4c.b = largs->m;
                        sp4c.in = largs->in;
                        sp4c.out = largs->out;
                        sp4c.W = largs->W;
                        sp4c.nW = largs->nW;
                        sp4c.nWdn = (largs->nW / largs->n);
                        sp4c.m = largs->m;
                        spawn<fft_twiddle_32_closure> sp4(sp4c);

                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->k1 = largs->k1;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->k0 = largs->k0;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->m = largs->m;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->r = largs->r;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->nW = largs->nW;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->W = largs->W;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->factors = largs->factors;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->out = largs->out;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->in = largs->in;
                        ((fft_aux_afterif0_cont1_closure*)SN_fft_aux_afterif0_cont1.cls.get())->n = largs->n;
                        // Original sync was here
                    } else {
                        fft_aux_afterif0_cont0_closure SN_fft_aux_afterif0_cont0c(largs->k);
                        spawn_next<fft_aux_afterif0_cont0_closure> SN_fft_aux_afterif0_cont0(SN_fft_aux_afterif0_cont0c);
                        cont sp5k;
                        SN_BIND_VOID(SN_fft_aux_afterif0_cont0, &sp5k);
                        fft_twiddle_gen_closure sp5c(sp5k);
                        sp5c.i = 0;
                        sp5c.i1 = largs->m;
                        sp5c.in = largs->in;
                        sp5c.out = largs->out;
                        sp5c.W = largs->W;
                        sp5c.nW = largs->nW;
                        sp5c.nWdn = (largs->nW / largs->n);
                        sp5c.r = largs->r;
                        sp5c.m = largs->m;
                        spawn<fft_twiddle_gen_closure> sp5(sp5c);

                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->k1 = largs->k1;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->k0 = largs->k0;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->m = largs->m;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->r = largs->r;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->nW = largs->nW;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->W = largs->W;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->factors = largs->factors;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->out = largs->out;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->in = largs->in;
                        ((fft_aux_afterif0_cont0_closure*)SN_fft_aux_afterif0_cont0.cls.get())->n = largs->n;
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
}
THREAD(fft_aux_afterif0_afterif2) {
    fft_aux_afterif0_afterif2_closure *largs = (fft_aux_afterif0_afterif2_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif1_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif0_afterif3) {
    fft_aux_afterif0_afterif3_closure *largs = (fft_aux_afterif0_afterif3_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif2_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif0_afterif4) {
    fft_aux_afterif0_afterif4_closure *largs = (fft_aux_afterif0_afterif4_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif3_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif0_afterif5) {
    fft_aux_afterif0_afterif5_closure *largs = (fft_aux_afterif0_afterif5_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif4_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif6) {
    fft_aux_afterif6_closure *largs = (fft_aux_afterif6_closure*)(args.get());
    fft_aux_afterif6_cont0_closure SN_fft_aux_afterif6_cont0c(largs->k);
    spawn_next<fft_aux_afterif6_cont0_closure> SN_fft_aux_afterif6_cont0(SN_fft_aux_afterif6_cont0c);
    for (largs->k1 = 0;(largs->k1 < largs->n);largs->k1 = (largs->k1 + largs->m)) {
        cont sp0k;
        SN_BIND_VOID(SN_fft_aux_afterif6_cont0, &sp0k);
        fft_aux_closure sp0c(sp0k);
        sp0c.n = largs->m;
        sp0c.in = (largs->out + largs->k1);
        sp0c.out = (largs->in + largs->k1);
        sp0c.factors = (largs->factors + 1);
        sp0c.W = largs->W;
        sp0c.nW = largs->nW;
        spawn<fft_aux_closure> sp0(sp0c);

    }
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->k1 = largs->k1;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->k0 = largs->k0;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->m = largs->m;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->r = largs->r;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->nW = largs->nW;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->W = largs->W;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->factors = largs->factors;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->out = largs->out;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->in = largs->in;
    ((fft_aux_afterif6_cont0_closure*)SN_fft_aux_afterif6_cont0.cls.get())->n = largs->n;
    // Original sync was here
    return;
}
THREAD(fft_aux_afterif7) {
    fft_aux_afterif7_closure *largs = (fft_aux_afterif7_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif6_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif8) {
    fft_aux_afterif8_closure *largs = (fft_aux_afterif8_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif7_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif9) {
    fft_aux_afterif9_closure *largs = (fft_aux_afterif9_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif8_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif10) {
    fft_aux_afterif10_closure *largs = (fft_aux_afterif10_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif9_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
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
}
THREAD(fft_aux_cont0) {
    fft_aux_cont0_closure *largs = (fft_aux_cont0_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif10_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_cont1) {
    fft_aux_cont1_closure *largs = (fft_aux_cont1_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif10_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_cont2) {
    fft_aux_cont2_closure *largs = (fft_aux_cont2_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif9_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_cont3) {
    fft_aux_cont3_closure *largs = (fft_aux_cont3_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif8_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_cont4) {
    fft_aux_cont4_closure *largs = (fft_aux_cont4_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif7_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_cont5) {
    fft_aux_cont5_closure *largs = (fft_aux_cont5_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif6_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(cilk_fft_cont0) {
    cilk_fft_cont0_closure *largs = (cilk_fft_cont0_closure*)(args.get());
    cilk_fft_cont1_closure SN_cilk_fft_cont1c(largs->k);
    spawn_next<cilk_fft_cont1_closure> SN_cilk_fft_cont1(SN_cilk_fft_cont1c);
    cont sp0k;
    SN_BIND_VOID(SN_cilk_fft_cont1, &sp0k);
    fft_aux_closure sp0c(sp0k);
    sp0c.n = largs->n;
    sp0c.in = largs->in;
    sp0c.out = largs->out;
    sp0c.factors = largs->factors;
    sp0c.W = largs->W;
    sp0c.nW = largs->n;
    spawn<fft_aux_closure> sp0(sp0c);

    ((cilk_fft_cont1_closure*)SN_cilk_fft_cont1.cls.get())->W = largs->W;
    // Original sync was here
    return;
}
THREAD(cilk_fft_cont1) {
    cilk_fft_cont1_closure *largs = (cilk_fft_cont1_closure*)(args.get());
    free(largs->W);
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(test_fft_cont0) {
    test_fft_cont0_closure *largs = (test_fft_cont0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(fft_aux_afterif0_cont0) {
    fft_aux_afterif0_cont0_closure *largs = (fft_aux_afterif0_cont0_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif5_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif0_cont1) {
    fft_aux_afterif0_cont1_closure *largs = (fft_aux_afterif0_cont1_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif5_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif0_cont2) {
    fft_aux_afterif0_cont2_closure *largs = (fft_aux_afterif0_cont2_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif4_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif0_cont3) {
    fft_aux_afterif0_cont3_closure *largs = (fft_aux_afterif0_cont3_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif3_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif0_cont4) {
    fft_aux_afterif0_cont4_closure *largs = (fft_aux_afterif0_cont4_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif2_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif0_cont5) {
    fft_aux_afterif0_cont5_closure *largs = (fft_aux_afterif0_cont5_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_afterif1_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(fft_aux_afterif6_cont0) {
    fft_aux_afterif6_cont0_closure *largs = (fft_aux_afterif6_cont0_closure*)(args.get());
    auto sp0c = std::make_shared<fft_aux_afterif0_closure>(largs->k);
    sp0c->n = largs->n;
    sp0c->in = largs->in;
    sp0c->out = largs->out;
    sp0c->factors = largs->factors;
    sp0c->W = largs->W;
    sp0c->nW = largs->nW;
    sp0c->r = largs->r;
    sp0c->m = largs->m;
    sp0c->k0 = largs->k0;
    sp0c->k1 = largs->k1;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
