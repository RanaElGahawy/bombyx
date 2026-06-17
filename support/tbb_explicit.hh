#pragma once
// Support header for the bombyx TBB (classic task API, TBB <= 2020) backend.
//
// The generated code models Cilk1-style CPS directly on top of tbb::task:
//   * every task function becomes a tbb::task subclass whose fields are the
//     closure arguments plus an untyped result destination pointer (bx_out),
//   * spawn_next closures become continuation tasks (allocate_continuation),
//     join counting is done by the TBB reference-count machinery,
//   * SEND_ARGUMENT delivers a result through the bound destination pointer;
//     the matching child_done of cilk_explicit.hh is implicit in TBB (a task
//     completing decrements its successor's reference count).

#ifndef TBB_SUPPRESS_DEPRECATED_MESSAGES
#define TBB_SUPPRESS_DEPRECATED_MESSAGES 1
#endif

#include <tbb/task.h>
#include <tbb/task_scheduler_init.h>

#include <type_traits>

#define SEND_ARGUMENT(dest, n)                                                 \
  do {                                                                         \
    if ((dest) != nullptr)                                                     \
      *static_cast<std::decay_t<decltype(n)> *>(dest) = (n);                   \
  } while (0)
