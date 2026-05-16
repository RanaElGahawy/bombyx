#include "desugarOpenCilk.hpp"
#include "iostream"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"

using namespace clang;

bool containsAssignment(Expr *E) {
  if (!E)
    return false;
  E = E->IgnoreParenImpCasts();
  if (auto *BO = dyn_cast<BinaryOperator>(E)) {
    if (BO->isAssignmentOp())
      return true;
    return containsAssignment(BO->getLHS()) || containsAssignment(BO->getRHS());
  }
  return false;
}

static BinaryOperator *extractAssignFromCondExpr(Expr *E,
                                                 BinaryOperator **CompBO) {
  E = E->IgnoreParenImpCasts();
  auto *BO = dyn_cast<BinaryOperator>(E);
  if (!BO)
    return nullptr;

  // Pattern: bare assignment used as condition
  if (BO->getOpcode() == BO_Assign) {
    *CompBO = nullptr;
    return BO;
  }

  // Pattern: (VAR = EXPR) op RHS
  if (BO->isComparisonOp()) {
    auto *LHS = BO->getLHS()->IgnoreParenImpCasts();
    if (auto *Inner = dyn_cast<BinaryOperator>(LHS)) {
      if (Inner->getOpcode() == BO_Assign) {
        *CompBO = BO;
        return Inner;
      }
    }
  }

  return nullptr;
}

WhileCondDesugar analyzeCondForDesugar(Expr *Cond) {
  WhileCondDesugar Result;
  Cond = Cond->IgnoreParenImpCasts();

  // Pattern: OUTER_COND && ASSIGN_PART  (assignment only on the RHS)
  if (auto *AndBO = dyn_cast<BinaryOperator>(Cond)) {
    if (AndBO->getOpcode() == BO_LAnd) {
      Expr *LHS = AndBO->getLHS();
      Expr *RHS = AndBO->getRHS();
      if (!containsAssignment(LHS) && containsAssignment(RHS)) {
        BinaryOperator *CompBO = nullptr;
        if (auto *AssignBO = extractAssignFromCondExpr(RHS, &CompBO)) {
          Result.OuterCond = LHS;
          Result.AssignBO = AssignBO;
          Result.CompBO = CompBO;
          return Result;
        }
      }
    }
  }

  // Pattern: bare assignment or (ASSIGN) op RHS
  BinaryOperator *CompBO = nullptr;
  if (auto *AssignBO = extractAssignFromCondExpr(Cond, &CompBO)) {
    Result.OuterCond = nullptr;
    Result.AssignBO = AssignBO;
    Result.CompBO = CompBO;
    return Result;
  }

  return Result;
}
