#include "cilk_explicit.hh"
/*
 * Heat diffusion (Jacobi-type iteration)
 *
 * Usage: see function usage();
 * 
 * Volker Strumpen, Boston                                 August 1996
 *
 * Copyright (c) 1996 Massachusetts Institute of Technology
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
 */

#include <cilk/cilk.h>

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <sys/time.h>

#include "getoptions.h"

extern int errno;

#if CILKSAN
#include "cilksan.h"
#endif

/* Define ERROR_SUMMARY if you want to check your numerical results */
#undef ERROR_SUMMARY

void allcgrid(double **neww, double **old, int lb, int ub);
void initgrid(double **old, int lb, int ub);
void compstripe(double **neww, double **old, int lb, int ub);
THREAD(divide);
int heat();
THREAD(heat_exit0);
THREAD(heat_reentry0);
THREAD(divide_cont0);
THREAD(divide_cont1);
THREAD(heat_cont0);
THREAD(heat_cont1);
THREAD(heat_reentry0_cont0);

CLOSURE_DEF(divide,
    int lb;
    int ub;
    double **neww;
    double **old;
    int mode;
    int timestep;
);
CLOSURE_DEF(heat_exit0,
    double **old;
    double **neww;
    int c;
    int l;
);
CLOSURE_DEF(heat_reentry0,
    double **old;
    double **neww;
    int c;
    int l;
);
CLOSURE_DEF(divide_cont0,
    int l;
    int r;
);
CLOSURE_DEF(divide_cont1,
    int l;
    int r;
);
CLOSURE_DEF(heat_cont0,
    double **old;
    double **neww;
);
CLOSURE_DEF(heat_cont1,
    double **old;
    double **neww;
    int l;
);
CLOSURE_DEF(heat_reentry0_cont0,
    double **old;
    double **neww;
    int c;
    int l;
);
unsigned long long todval (struct timeval *tp) {
    return tp->tv_sec * 1000 * 1000 + tp->tv_usec;
}

#define f(x,y)     (sin(x)*sin(y))
#define randa(x,t) (0.0)
#define randb(x,t) (exp(-2*(t))*sin(x))
#define randc(y,t) (0.0)
#define randd(y,t) (exp(-2*(t))*sin(y))
#define solu(x,y,t) (exp(-2*(t))*sin(x)*sin(y))

/* apparently register storage specifier is deprecated */
#define register

int nx, ny, nt;
double xu, xo, yu, yo, tu, to;
double dx, dy, dt;

double dtdxsq, dtdysq;
double t;

int leafmaxcol;


/*****************   Allocation of grid partition  ********************/



/*****************   Initialization of grid partition  ********************/




/***************** Five-Point-Stencil Computation ********************/




/***************** Decomposition of 2D grids in stripes ********************/

#define ALLC       0
#define INIT       1
#define COMP       2





int usage(void) {

  fprintf(stderr, "\nUsage: heat [<cilk-options>] [<options>}\n\n");
  fprintf(stderr, "This program uses a Jacobi-type iteration to "
      "solve a finite-difference\n");
  fprintf(stderr, "approximation of parabolic partial differential "
      "equations that models\n");
  fprintf(stderr, "for example the heat diffusion problem.\n\n");
  fprintf(stderr, "Optional parameter: \n");
  fprintf(stderr, "   -g #     "
      "granularity (columns per partition)  default: 10\n");   
  fprintf(stderr, "   -nx #    "
      "total number of columns              default: 4096\n");
  fprintf(stderr, "   -ny #    "
      "total number of rows                 default: 512\n");
  fprintf(stderr, "   -nt #    "
      "total time steps                     default: 100\n");
  fprintf(stderr, "   -f filename    parameter file for nx, ny, ...\n");
  fprintf(stderr, "   -benchmark short/medium/long\n");
  /*
     fprintf(stderr, "   -xu #    lower x coordinate default: 0.0\n");
     fprintf(stderr, "   -xo #    upper x coordinate default: 1.570796326794896558\n");
     fprintf(stderr, "   -yu #    lower y coordinate default: 0.0\n");
     fprintf(stderr, "   -yo #    upper y coordinate default: 1.570796326794896558\n");
     fprintf(stderr, "   -tu #    start time         default: 0.0\n");
     fprintf(stderr, "   -to #    end time           default: 0.0000001\n");
     */
  return 1;
}

void read_heatparams(char *filefn) {

  FILE *f;
  int l;

  if ((f = fopen(filefn, "r")) == NULL) {
    printf("\n Can't open %s\n", filefn);
    exit(0);
  }
  l = fscanf(f, "%d %d %d %lf %lf %lf %lf %lf %lf",
      &nx, &ny, &nt, &xu, &xo, &yu, &yo, &tu, &to);
  if (l != 9)
    printf("\n Warning: fscanf errno %d", errno);
  fclose(f);

}

const char *specifiers[] = { "-g", "-nx", "-ny", "-nt", "-xu", "-xo", "-yu", "-yo", "-tu", "-to", "-f", "-benchmark", "-h", 0};
int opt_types[] = {INTARG, INTARG, INTARG, INTARG, DOUBLEARG, DOUBLEARG, DOUBLEARG, DOUBLEARG, DOUBLEARG, DOUBLEARG, STRINGARG, BENCHMARK, BOOLARG, 0 };

int main(int argc, char *argv[]) { 

  int ret, benchmark, help;
  char filename[100];

  nx = 512;
  ny = 512;
  nt = 100;
  xu = 0.0;
  xo = 1.570796326794896558;
  yu = 0.0;
  yo = 1.570796326794896558;
  tu = 0.0;
  to = 0.0000001;
  leafmaxcol = 10;
  filename[0]=0;

  // use the math related function before parallel region;
  // there is some benigh race in initalization code for the math functions.
  fprintf(stderr, "Testing exp: %f\n", randb(nx, nt)); 

  get_options(argc, argv, specifiers, opt_types, &leafmaxcol, 
              &nx, &ny, &nt, &xu, &xo, &yu, &yo, &tu, &to, 
              filename, &benchmark, &help);

  if (help) return usage();

  if (benchmark) {
    switch (benchmark) {
      case 1:      /* short benchmark options -- a little work*/
        nx = 512;
        ny = 512;
        nt = 1;
        xu = 0.0;
        xo = 1.570796326794896558;
        yu = 0.0;
        yo = 1.570796326794896558;
        tu = 0.0;
        to = 0.0000001;
        leafmaxcol = 10;
        filename[0]=0;
        break;
      case 2:      /* standard benchmark options*/
        nx = 4096;
        ny = 512;
        nt = 40;
        xu = 0.0;
        xo = 1.570796326794896558;
        yu = 0.0;
        yo = 1.570796326794896558;
        tu = 0.0;
        to = 0.0000001;
        leafmaxcol = 10;
        filename[0]=0;
        break;
      case 3:      /* long benchmark options -- a lot of work*/
        nx = 4096;
        ny = 1024;
        nt = 100;
        xu = 0.0;
        xo = 1.570796326794896558;
        yu = 0.0;
        yo = 1.570796326794896558;
        tu = 0.0;
        to = 0.0000001;
        leafmaxcol = 1;
        filename[0]=0;
        break;
    }
  }

  if (filename[0]) read_heatparams(filename);

  dx = (xo - xu) / (nx - 1);
  dy = (yo - yu) / (ny - 1);
  dt = (to - tu) / nt;	/* nt effective time steps! */

  dtdxsq = dt / (dx * dx);
  dtdysq = dt / (dy * dy);

  struct timeval t1, t2;
  gettimeofday(&t1,0);

  ret = heat();

  gettimeofday(&t2,0);
  unsigned long long runtime_ms = (todval(&t2)-todval(&t1))/1000;
  printf("%f\n", runtime_ms/1000.0);

  fprintf(stderr, "\nCilk Example: heat\n");
  fprintf(stderr, "\n   dx = %f", dx);
  fprintf(stderr, "\n   dy = %f", dy);
  fprintf(stderr, "\n   dt = %f", dt);

  fprintf(stderr, "\n\n Stability Value for explicit method must be > 0:  %f\n\n",
      0.5 - (dt / (dx * dx) + dt / (dy * dy)));
  fprintf(stderr, "Options: granularity = %d\n", leafmaxcol);
  fprintf(stderr, "         nx          = %d\n", nx);
  fprintf(stderr, "         ny          = %d\n", ny);
  fprintf(stderr, "         nt          = %d\n", nt);

  return 0;
}


void allcgrid(double **neww, double **old, int lb, int ub) {
    int j;
    double **rne;
    double **rol;
    j = lb;
    rol = (old + lb);
    for (rne = (neww + lb);(j < ub);(rne++)) {
        *(rol) = ((double *) malloc((ny * sizeof(double))));
        *(rne) = ((double *) malloc((ny * sizeof(double))));
        (j++);
        (rol++);
    }
}
void initgrid(double **old, int lb, int ub) {
    int a;
    int b;
    int llb;
    int lub;
    llb = (lb == 0) ? 1 : lb;
    lub = (ub == nx) ? nx - 1 : ub;
    a = llb;
    for (b = 0;(a < lub);(a++)) {
        old[a][b] = 0.;
    }
    a = llb;
    for (b = (ny - 1);(a < lub);(a++)) {
        old[a][b] = (exp(((-2) * 0)) * sin((xu + (a * dx))));
    }
    if ((lb == 0)) {
        a = 0;
        for (b = 0;(b < ny);(b++)) {
            old[a][b] = 0.;
        }
    }
    if ((ub == nx)) {
        a = (nx - 1);
        for (b = 0;(b < ny);(b++)) {
            old[a][b] = (exp(((-2) * 0)) * sin((yu + (b * dy))));
        }
    }
    for (a = llb;(a < lub);(a++)) {
        for (b = 1;(b < (ny - 1));(b++)) {
            old[a][b] = (sin((xu + (a * dx))) * sin((yu + (b * dy))));
        }
    }
}
void compstripe(double **neww, double **old, int lb, int ub) {
    int a;
    int b;
    int llb;
    int lub;
    llb = (lb == 0) ? 1 : lb;
    lub = (ub == nx) ? nx - 1 : ub;
    for (a = llb;(a < lub);(a++)) {
        for (b = 1;(b < (ny - 1));(b++)) {
            neww[a][b] = (((dtdxsq * ((old[(a + 1)][b] - (2 * old[a][b])) + old[(a - 1)][b])) + (dtdysq * ((old[a][(b + 1)] - (2 * old[a][b])) + old[a][(b - 1)]))) + old[a][b]);
        }
    }
    a = llb;
    for (b = (ny - 1);(a < lub);(a++)) {
        neww[a][b] = (exp(((-2) * t)) * sin((xu + (a * dx))));
    }
    a = llb;
    for (b = 0;(a < lub);(a++)) {
        neww[a][b] = 0.;
    }
    if ((lb == 0)) {
        a = 0;
        for (b = 0;(b < ny);(b++)) {
            neww[a][b] = 0.;
        }
    }
    if ((ub == nx)) {
        a = (nx - 1);
        for (b = 0;(b < ny);(b++)) {
            neww[a][b] = (exp(((-2) * t)) * sin((yu + (b * dy))));
        }
    }
}
THREAD(divide) {
    int l;
    int r;
    divide_closure *largs = (divide_closure*)(args.get());
    if (((largs->ub - largs->lb) > leafmaxcol)) {
        l = 0;
        divide_cont0_closure SN_divide_cont0c(largs->k);
        spawn_next<divide_cont0_closure> SN_divide_cont0(SN_divide_cont0c);
        cont sp0k;
        SN_BIND(SN_divide_cont0, &sp0k, l);
        divide_closure sp0c(sp0k);
        sp0c.lb = largs->lb;
        sp0c.ub = ((largs->ub + largs->lb) / 2);
        sp0c.neww = largs->neww;
        sp0c.old = largs->old;
        sp0c.mode = largs->mode;
        sp0c.timestep = largs->timestep;
        spawn<divide_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND(SN_divide_cont0, &sp1k, r);
        divide_closure sp1c(sp1k);
        sp1c.lb = ((largs->ub + largs->lb) / 2);
        sp1c.ub = largs->ub;
        sp1c.neww = largs->neww;
        sp1c.old = largs->old;
        sp1c.mode = largs->mode;
        sp1c.timestep = largs->timestep;
        spawn<divide_closure> sp1(sp1c);

        // Original sync was here
    } else {
        switch (largs->mode) {
case 2:
  if (largs->timestep % 2)
      compstripe(largs->neww, largs->old, largs->lb, largs->ub);
  else
      compstripe(largs->old, largs->neww, largs->lb, largs->ub);
  SEND_ARGUMENT(largs->k, 1);
  return;
case 0:
  allcgrid(largs->neww, largs->old, largs->lb, largs->ub);
  SEND_ARGUMENT(largs->k, 1);
  return;
case 1:
  initgrid(largs->old, largs->lb, largs->ub);
  SEND_ARGUMENT(largs->k, 1);
  return;
};
        SEND_ARGUMENT(largs->k, 0);
    }
}
int heat() {
    double **old;
    double **neww;
    int l;
    old = ((double **) malloc((nx * sizeof(double *))));
    neww = ((double **) malloc((nx * sizeof(double *))));
    heat_cont0_closure SN_heat_cont0c(CONT_DUMMY);
    spawn_next<heat_cont0_closure> SN_heat_cont0(SN_heat_cont0c);
    cont sp0k;
    SN_BIND_VOID(SN_heat_cont0, &sp0k);
    divide_closure sp0c(sp0k);
    sp0c.lb = 0;
    sp0c.ub = nx;
    sp0c.neww = neww;
    sp0c.old = old;
    sp0c.mode = 0;
    sp0c.timestep = 0;
    spawn<divide_closure> sp0(sp0c);

    ((heat_cont0_closure*)SN_heat_cont0.cls.get())->neww = neww;
    ((heat_cont0_closure*)SN_heat_cont0.cls.get())->old = old;
    // Original sync was here
    return 0;
}
THREAD(heat_exit0) {
    heat_exit0_closure *largs = (heat_exit0_closure*)(args.get());
    SEND_ARGUMENT(largs->k, 0);
}
THREAD(heat_reentry0) {
    heat_reentry0_closure *largs = (heat_reentry0_closure*)(args.get());
    if ((largs->c <= nt)) {
        t = tu + largs->c * dt;
        heat_reentry0_cont0_closure SN_heat_reentry0_cont0c(largs->k);
        spawn_next<heat_reentry0_cont0_closure> SN_heat_reentry0_cont0(SN_heat_reentry0_cont0c);
        cont sp0k;
        SN_BIND(SN_heat_reentry0_cont0, &sp0k, l);
        divide_closure sp0c(sp0k);
        sp0c.lb = 0;
        sp0c.ub = nx;
        sp0c.neww = largs->neww;
        sp0c.old = largs->old;
        sp0c.mode = 2;
        sp0c.timestep = largs->c;
        spawn<divide_closure> sp0(sp0c);

        ((heat_reentry0_cont0_closure*)SN_heat_reentry0_cont0.cls.get())->c = largs->c;
        ((heat_reentry0_cont0_closure*)SN_heat_reentry0_cont0.cls.get())->neww = largs->neww;
        ((heat_reentry0_cont0_closure*)SN_heat_reentry0_cont0.cls.get())->old = largs->old;
        // Original sync was here
    } else {
        auto sp1c = std::make_shared<heat_exit0_closure>(largs->k);
        sp1c->old = largs->old;
        sp1c->neww = largs->neww;
        sp1c->c = largs->c;
        sp1c->l = largs->l;
        cilk_spawn taskSpawn(sp1c->getTask(), sp1c);
        return;
    }
}
THREAD(divide_cont0) {
    divide_cont0_closure *largs = (divide_cont0_closure*)(args.get());
    divide_cont1_closure SN_divide_cont1c(largs->k);
    spawn_next<divide_cont1_closure> SN_divide_cont1(SN_divide_cont1c);
    ((divide_cont1_closure*)SN_divide_cont1.cls.get())->r = largs->r;
    ((divide_cont1_closure*)SN_divide_cont1.cls.get())->l = largs->l;
    // Original sync was here
    return;
}
THREAD(divide_cont1) {
    int _tmp;
    divide_cont1_closure *largs = (divide_cont1_closure*)(args.get());
    _tmp = (largs->l + largs->r);
    SEND_ARGUMENT(largs->k, _tmp);
}
THREAD(heat_cont0) {
    int l;
    heat_cont0_closure *largs = (heat_cont0_closure*)(args.get());
    heat_cont1_closure SN_heat_cont1c(largs->k);
    spawn_next<heat_cont1_closure> SN_heat_cont1(SN_heat_cont1c);
    cont sp0k;
    SN_BIND(SN_heat_cont1, &sp0k, l);
    divide_closure sp0c(sp0k);
    sp0c.lb = 0;
    sp0c.ub = nx;
    sp0c.neww = largs->neww;
    sp0c.old = largs->old;
    sp0c.mode = 1;
    sp0c.timestep = 0;
    spawn<divide_closure> sp0(sp0c);

    ((heat_cont1_closure*)SN_heat_cont1.cls.get())->neww = largs->neww;
    ((heat_cont1_closure*)SN_heat_cont1.cls.get())->old = largs->old;
    // Original sync was here
    return;
}
THREAD(heat_cont1) {
    int c;
    heat_cont1_closure *largs = (heat_cont1_closure*)(args.get());
    c = 1;
    auto sp0c = std::make_shared<heat_reentry0_closure>(largs->k);
    sp0c->old = largs->old;
    sp0c->neww = largs->neww;
    sp0c->c = c;
    sp0c->l = largs->l;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
THREAD(heat_reentry0_cont0) {
    heat_reentry0_cont0_closure *largs = (heat_reentry0_cont0_closure*)(args.get());
    (largs->c++);
    auto sp0c = std::make_shared<heat_reentry0_closure>(largs->k);
    sp0c->old = largs->old;
    sp0c->neww = largs->neww;
    sp0c->c = largs->c;
    sp0c->l = largs->l;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
}
