#include "cilk_explicit.hh"
// -*- C++ -*-

/*
 * qsort.cpp
 *
 * An implementation of quicksort using Intel(R) Cilk(TM) Plus parallelization.
 *
 * Copyright (C) 2009-2010 Intel Corporation. All Rights Reserved.
 *
 * The source code contained or described herein and all
 * documents related to the source code ("Material") are owned by
 * Intel Corporation or its suppliers or licensors. Title to the
 * Material remains with Intel Corporation or its suppliers and
 * licensors. The Material is protected by worldwide copyright
 * laws and treaty provisions.  No part of the Material may be
 * used, copied, reproduced, modified, published, uploaded,
 * posted, transmitted, distributed,  or disclosed in any way
 * except as expressly provided in the license provided with the
 * Materials.  No license under any patent, copyright, trade
 * secret or other intellectual property right is granted to or
 * conferred upon you by disclosure or delivery of the Materials,
 * either expressly, by implication, inducement, estoppel or
 * otherwise, except as expressly provided in the license
 * provided with the Materials.
 */

#include <algorithm>
#include <cilk/cilk.h>
#include <functional>
#include <iostream>
#include <iterator>
#include <random>
#include <sys/time.h>

#if CILKSAN
#include "cilksan.h"
#endif

// Sort the range between bidirectional iterators begin and end.
// end is one past the final element in the range.
// Use the Quick Sort algorithm, using recursive divide and conquer.
// This function is NOT the same as the Standard C Library qsort() function.
// This implementation is pure C++ code before Intel(R) Cilk(TM) Plus
// conversion.
THREAD(sample_qsort);
unsigned long long todval(struct timeval *tp);
int qmain(int n);
THREAD(sample_qsort_afterif0);
THREAD(sample_qsort_cont0);
THREAD(sample_qsort_cont1);
THREAD(qmain_cont0);

CLOSURE_DEF(sample_qsort,
    int *begin;
    int *end;
);
CLOSURE_DEF(sample_qsort_afterif0,
    int *begin;
    int *end;
    int *middle;
);
CLOSURE_DEF(sample_qsort_cont0,
    int *begin;
    int *end;
    int *middle;
);
CLOSURE_DEF(sample_qsort_cont1,
    int *begin;
    int *end;
    int *middle;
);
CLOSURE_DEF(qmain_cont0,
    int n;
    int *a;
    struct timeval t1;
    struct timeval t2;
);




// A simple test harness


int main(int argc, char *argv[]) {

  int n = 10 * 1000 * 1000;
  if (argc > 1) {
    n = std::atoi(argv[1]);
    if (n <= 0) {
      std::cerr << "Invalid argument" << std::endl;
      std::cerr << "Usage: qsort N" << std::endl;
      std::cerr << "       N = number of elements to sort" << std::endl;
      return 1;
    }
  }
  int ret = qmain(n);

  return ret;
}

THREAD(sample_qsort) {
    int *middle;
    sample_qsort_closure *largs = (sample_qsort_closure*)(args.get());
    if ((largs->begin != largs->end)) {
        (--largs->end);
        middle = std::partition(largs->begin,largs->end,[end = largs->end](int x) {
{
    return x < *end;
}
});
        std::swap(*(largs->end),*(middle));
        sample_qsort_cont0_closure SN_sample_qsort_cont0c(largs->k);
        spawn_next<sample_qsort_cont0_closure> SN_sample_qsort_cont0(SN_sample_qsort_cont0c);
        cont sp0k;
        SN_BIND_VOID(SN_sample_qsort_cont0, &sp0k);
        sample_qsort_closure sp0c(sp0k);
        sp0c.begin = largs->begin;
        sp0c.end = middle;
        spawn<sample_qsort_closure> sp0(sp0c);

        cont sp1k;
        SN_BIND_VOID(SN_sample_qsort_cont0, &sp1k);
        sample_qsort_closure sp1c(sp1k);
        sp1c.begin = (++middle);
        sp1c.end = (++largs->end);
        spawn<sample_qsort_closure> sp1(sp1c);

        ((sample_qsort_cont0_closure*)SN_sample_qsort_cont0.cls.get())->middle = middle;
        ((sample_qsort_cont0_closure*)SN_sample_qsort_cont0.cls.get())->end = largs->end;
        ((sample_qsort_cont0_closure*)SN_sample_qsort_cont0.cls.get())->begin = largs->begin;
        // Original sync was here
    } else {
        auto sp2c = std::make_shared<sample_qsort_afterif0_closure>(largs->k);
        sp2c->begin = largs->begin;
        sp2c->end = largs->end;
        sp2c->middle = middle;
        cilk_spawn taskSpawn(sp2c->getTask(), sp2c);
        return;
    }
    return;
}
unsigned long long todval(struct timeval *tp) {
    return (((tp->tv_sec * 1000) * 1000) + tp->tv_usec);
}
int qmain(int n) {
    int *a;
    int i;
    std::mt19937 gen(0);
    struct timeval t1;
    a = new int [n];
    qmain_cont0_closure SN_qmain_cont0c(CONT_DUMMY);
    spawn_next<qmain_cont0_closure> SN_qmain_cont0(SN_qmain_cont0c);
    for (i = 0;(i < n);(++i)) {
        a[i] = i;
    }
    std::shuffle(a,(a + n),gen);
    std::cerr << "Sorting " << n << " integers" << std::endl;
    gettimeofday(&(t1),0);
    cont sp0k;
    SN_BIND_VOID(SN_qmain_cont0, &sp0k);
    sample_qsort_closure sp0c(sp0k);
    sp0c.begin = a;
    sp0c.end = (a + n);
    spawn<sample_qsort_closure> sp0(sp0c);

    ((qmain_cont0_closure*)SN_qmain_cont0.cls.get())->t1 = t1;
    ((qmain_cont0_closure*)SN_qmain_cont0.cls.get())->a = a;
    ((qmain_cont0_closure*)SN_qmain_cont0.cls.get())->n = n;
    // Original sync was here
    return 0;
}
THREAD(sample_qsort_afterif0) {
    sample_qsort_afterif0_closure *largs = (sample_qsort_afterif0_closure*)(args.get());
    return;
}
THREAD(sample_qsort_cont0) {
    sample_qsort_cont0_closure *largs = (sample_qsort_cont0_closure*)(args.get());
    sample_qsort_cont1_closure SN_sample_qsort_cont1c(largs->k);
    spawn_next<sample_qsort_cont1_closure> SN_sample_qsort_cont1(SN_sample_qsort_cont1c);
    ((sample_qsort_cont1_closure*)SN_sample_qsort_cont1.cls.get())->middle = largs->middle;
    ((sample_qsort_cont1_closure*)SN_sample_qsort_cont1.cls.get())->end = largs->end;
    ((sample_qsort_cont1_closure*)SN_sample_qsort_cont1.cls.get())->begin = largs->begin;
    // Original sync was here
    return;
}
THREAD(sample_qsort_cont1) {
    sample_qsort_cont1_closure *largs = (sample_qsort_cont1_closure*)(args.get());
    auto sp0c = std::make_shared<sample_qsort_afterif0_closure>(largs->k);
    sp0c->begin = largs->begin;
    sp0c->end = largs->end;
    sp0c->middle = largs->middle;
    cilk_spawn taskSpawn(sp0c->getTask(), sp0c);
    return;
    return;
}
THREAD(qmain_cont0) {
    unsigned long long runtime_ms;
    int i0;
    qmain_cont0_closure *largs = (qmain_cont0_closure*)(args.get());
    gettimeofday(&(largs->t2),0);
    runtime_ms = ((todval(&(largs->t2)) - todval(&(largs->t1))) / 1000);
    std::cout << runtime_ms / 1000. << "\n";
    for (i0 = 0;(i0 < (largs->n - 1));(++i0)) {
        if (((largs->a[i0] >= largs->a[(i0 + 1)]) || (largs->a[i0] != i0))) {
            std::cerr << "Sort failed at location i=" << i0 << " a[i] = " << largs->a[i0] << " a[i+1] = " << largs->a[i0 + 1] << std::endl;
            delete [] largs->a;
            SEND_ARGUMENT(largs->k, 1);
        } else {
        }
    }
    std::cerr << "Sort succeeded." << std::endl;
    delete [] largs->a;
    SEND_ARGUMENT(largs->k, 0);
    return;
}
