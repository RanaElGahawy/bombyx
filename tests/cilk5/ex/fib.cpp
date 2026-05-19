#include "cilk_explicit.hh"
/*
 * Copyright (c) 1994-2003 Massachusetts Institute of Technology
 * Copyright (c) 2003 Bradley C. Kuszmaul
 * Copyright (c) 2013 I-Ting Angelina Lee and Tao B. Schardl
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

unsigned long long todval(struct timeval *tp);
THREAD(fib);
int main(int argc, char **argv);
THREAD(fib_cont0);
THREAD(fib_cont1);
THREAD(main_cont0);

CLOSURE_DEF(fib,
    int n;
);
CLOSURE_DEF(fib_cont0,
    int x;
    int y;
);
CLOSURE_DEF(fib_cont1,
    int x;
    int y;
);
CLOSURE_DEF(main_cont0,
    int result;
    struct timeval t1;
    struct timeval t2;
);


#ifdef SERIAL
#include <cilk/cilk_stub.h>
#endif




unsigned long long todval(struct timeval *tp) {
    return (((tp->tv_sec * 1000) * 1000) + tp->tv_usec);
}
THREAD(fib) {
    int x;
    int y;
    fib_closure *largs = (fib_closure*)(args.get());
    if ((largs->n < 2)) {
        SEND_ARGUMENT(largs->k, largs->n);
    } else {
        fib_cont0_closure SN_fib_cont0c(largs->k);
        spawn_next<fib_cont0_closure> SN_fib_cont0(SN_fib_cont0c);
        cont sp0k;
        SN_BIND(SN_fib_cont0, &sp0k, x);
        fib_closure sp0c(sp0k);
        sp0c.n = (largs->n - 1);
        spawn<fib_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND(SN_fib_cont0, &sp1k, y);
        fib_closure sp1c(sp1k);
        sp1c.n = (largs->n - 2);
        spawn<fib_closure> sp1(sp1c);

        // Original sync was here
    }
}
int main(int argc, char **argv) {
    int n;
    int result;
    struct timeval t1;
    main_cont0_closure SN_main_cont0c(CONT_DUMMY);
    spawn_next<main_cont0_closure> SN_main_cont0(SN_main_cont0c);
    if ((argc != 2)) {
        fprintf(stderr,"Usage: fib [<cilk options>] <n>\n");
        exit(1);
    }
    n = atoi(argv[1]);
    gettimeofday(&(t1),0);
    cont sp0k;
    SN_BIND(SN_main_cont0, &sp0k, result);
    fib_closure sp0c(sp0k);
    sp0c.n = n;
    spawn<fib_closure> sp0(sp0c);

    ((main_cont0_closure*)SN_main_cont0.cls.get())->t1 = t1;
    // Original sync was here
    return 0;
}
THREAD(fib_cont0) {
    fib_cont0_closure *largs = (fib_cont0_closure*)(args.get());
    fib_cont1_closure SN_fib_cont1c(largs->k);
    spawn_next<fib_cont1_closure> SN_fib_cont1(SN_fib_cont1c);
    ((fib_cont1_closure*)SN_fib_cont1.cls.get())->y = largs->y;
    ((fib_cont1_closure*)SN_fib_cont1.cls.get())->x = largs->x;
    // Original sync was here
    return;
}
THREAD(fib_cont1) {
    fib_cont1_closure *largs = (fib_cont1_closure*)(args.get());
    SEND_ARGUMENT(largs->k, (largs->x + largs->y));
}
THREAD(main_cont0) {
    unsigned long long runtime_ms;
    main_cont0_closure *largs = (main_cont0_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    printf("%f\n",(runtime_ms / 1000.));
    fprintf(stderr,"Result: %d\n",largs->result);
    SEND_ARGUMENT(largs->k, 0);
}
