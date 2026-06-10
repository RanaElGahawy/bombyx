#pragma once
#include <clang/AST/Expr.h>

// Returns true if E contains an embedded assignment operator at any depth.
bool containsAssignment(clang::Expr *E);

struct WhileCondDesugar {
  clang::Expr *OuterCond = nullptr;
  clang::BinaryOperator *AssignBO = nullptr;
  clang::BinaryOperator *CompBO = nullptr;
};

WhileCondDesugar analyzeCondForDesugar(clang::Expr *Cond);
