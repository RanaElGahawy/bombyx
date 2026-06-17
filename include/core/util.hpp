#pragma once
#include <clang/AST/Expr.h>
#include <clang/AST/Stmt.h>

#include <deque>
#include <stdio.h>

extern int VERBOSITY;

#define DBG if (VERBOSITY >= 2)
#define INFO if (VERBOSITY >= 1)
#define WARN

#define BRED "\e[1;31m"
#define BHGREEN "\e[1;92m"
#define BHBLK "\e[1;90m"
#define COLOR_RESET "\e[0m"
#define PANIC(...)                                                             \
  fprintf(stderr,                                                              \
          "[" BRED "panic" COLOR_RESET "@" BHBLK "%s:%d" COLOR_RESET "(%s)] ", \
          __FILE__, __LINE__, __func__);                                       \
  fprintf(stderr, __VA_ARGS__);                                                \
  fprintf(stderr, "\n");                                                       \
  exit(EXIT_FAILURE);

// Like PANIC but does not abort: print a red "critical" diagnostic and keep
// going so the rest of the output can still be inspected for debugging.
#define CRITICAL(...)                                                          \
  do {                                                                         \
    fprintf(stderr,                                                            \
            "[" BRED "critical" COLOR_RESET "@" BHBLK "%s:%d" COLOR_RESET      \
            "(%s)] ",                                                          \
            __FILE__, __LINE__, __func__);                                     \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
  } while (0)

using namespace clang;