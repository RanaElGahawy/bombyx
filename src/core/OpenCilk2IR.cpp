#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/StmtVisitor.h>
#include <clang/Analysis/CFG.h>
#include <unordered_map>

#include "core/IR.hpp"
#include "core/OpenCilk2IR.hpp"
#include "core/desugarOpenCilk.hpp"
#include "clang/AST/ASTFwd.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprCilk.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCilk.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/Support/ErrorHandling.h"

std::set<std::string> GIgnoreFns;
std::vector<clang::RecordDecl *> GRecordDecls;
//////////////////////////////////
// Scan AST for relevant tasks //
////////////////////////////////
using FunLookupTy =
    std::unordered_map<const clang::FunctionDecl *, IRFunction *>;

// collect variables used inside the while condition
class CondVarCollector : public clang::RecursiveASTVisitor<CondVarCollector> {
public:
  std::set<clang::ValueDecl *> Vars;

  bool VisitDeclRefExpr(clang::DeclRefExpr *DRE) {
    if (auto *VD = llvm::dyn_cast<clang::ValueDecl>(DRE->getDecl())) {
      Vars.insert(VD);
    }
    return true;
  }
};

// Collect all variables that are assigned (written to) inside the while body
// but declared OUTSIDE it. These are the loop-carried variables that must be
// promoted to arguments when converting a while-with-sync to tail recursion.
// Variables declared inside the while body (like loop-local temporaries)
// are excluded since they don't carry state across iterations.
class LoopBodyVarCollector
    : public clang::RecursiveASTVisitor<LoopBodyVarCollector> {
public:
  std::set<clang::ValueDecl *> AssignedVars;
  std::set<clang::ValueDecl *> DeclaredVars;

  bool VisitVarDecl(clang::VarDecl *VD) {
    DeclaredVars.insert(VD);
    return true;
  }

  bool VisitBinaryOperator(clang::BinaryOperator *BO) {
    if (BO->isAssignmentOp()) {
      if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(
              BO->getLHS()->IgnoreParenImpCasts())) {
        if (auto *VD = llvm::dyn_cast<clang::ValueDecl>(DRE->getDecl())) {
          AssignedVars.insert(VD);
        }
      }
    }
    return true;
  }

  bool VisitUnaryOperator(clang::UnaryOperator *UO) {
    if (UO->isIncrementDecrementOp()) {
      if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(
              UO->getSubExpr()->IgnoreParenImpCasts())) {
        if (auto *VD = llvm::dyn_cast<clang::ValueDecl>(DRE->getDecl())) {
          AssignedVars.insert(VD);
        }
      }
    }
    return true;
  }

  // Get the loop-carried variables: assigned in body but not declared in body
  std::set<clang::ValueDecl *> getLoopCarriedVars() {
    std::set<clang::ValueDecl *> Result;
    for (auto *VD : AssignedVars) {
      if (DeclaredVars.find(VD) == DeclaredVars.end()) {
        Result.insert(VD);
      }
    }
    return Result;
  }
};

// ─── OVERLAP run-ahead legality ──────────────────────────────────────────────
//
// An OVERLAP loop is "run-ahead" when iteration i+1 may be issued without
// waiting for iteration i's body — including any loop nested inside it — to
// finish. That is what lets several neighbours' comparison streams be in flight
// at once and is the only way one task saturates the compare stage.
//
// It is legal exactly when reordering and overlapping the iterations cannot
// change the result. This analysis proves that conservatively: everything it
// does not understand is a rejection, never an assumption.
namespace {

// A loop-carried variable is one of these three, or the loop is rejected.
enum class CarriedKind { Induction, Reduction, Private, Rejected };

// Detects the shapes that disqualify run-ahead outright: a real cilk_spawn, a
// jump out of the body, or a call that can carry state between iterations
// behind the analysis' back. Stores are NOT rejected here — they are handled by
// checkMemoryIndependence below, which tries to prove them iteration-private.
class RunAheadBlockerVisitor
    : public clang::RecursiveASTVisitor<RunAheadBlockerVisitor> {
public:
  std::string Reason;

  // A callee can mutate an outer variable through a mutable reference
  // parameter, or store through a pointer parameter, and neither shows up in
  // this function's AST. The loop-carried-variable pass would then see nothing
  // and wrongly conclude the iterations are independent, so anything that hands
  // a callee a writable handle is rejected. Calls taking only values are fine.
  bool VisitCallExpr(clang::CallExpr *CE) {
    if (!Reason.empty())
      return true;
    const clang::FunctionDecl *FD = CE->getDirectCallee();
    if (!FD) {
      Reason = "the body makes an indirect call";
      return true;
    }
    const unsigned N = std::min<unsigned>(CE->getNumArgs(), FD->getNumParams());
    for (unsigned I = 0; I < N; ++I) {
      clang::QualType PT = FD->getParamDecl(I)->getType();
      if (PT->isLValueReferenceType() &&
          !PT->getPointeeType().isConstQualified()) {
        Reason = "the body passes an argument to '" + FD->getNameAsString() +
                 "' by mutable reference, so that call can carry state between "
                 "iterations";
        return true;
      }
      if (PT->isPointerType() && !PT->getPointeeType().isConstQualified()) {
        Reason = "the body passes a writable pointer to '" +
                 FD->getNameAsString() + "', whose stores are not visible here";
        return true;
      }
    }
    return true;
  }
  bool VisitUnaryOperator(clang::UnaryOperator *UO) {
    // Taking the address of a variable lets it be written through a handle
    // this analysis does not follow.
    if (UO->getOpcode() == clang::UO_AddrOf &&
        llvm::isa<clang::DeclRefExpr>(UO->getSubExpr()->IgnoreParenImpCasts()) &&
        Reason.empty())
      Reason = "the body takes the address of a variable";
    return true;
  }
  bool VisitCilkSpawnExpr(clang::CilkSpawnExpr *) {
    if (Reason.empty())
      Reason = "the body contains a cilk_spawn";
    return true;
  }
  bool VisitCilkSpawnStmt(clang::CilkSpawnStmt *) {
    if (Reason.empty())
      Reason = "the body contains a cilk_spawn";
    return true;
  }
  bool VisitBreakStmt(clang::BreakStmt *) {
    // A break belonging to a nested loop or switch is fine; one belonging to
    // this loop makes the trip count inexact. We cannot tell them apart here,
    // so the caller only runs this over statements that are not nested loops.
    if (Reason.empty())
      Reason = "the body can exit the loop early";
    return true;
  }
  bool VisitReturnStmt(clang::ReturnStmt *) {
    if (Reason.empty())
      Reason = "the body returns out of the loop";
    return true;
  }
  bool VisitGotoStmt(clang::GotoStmt *) {
    if (Reason.empty())
      Reason = "the body contains a goto";
    return true;
  }
  // Do not descend into nested loops/switches when looking for OUR break.
  bool TraverseForStmt(clang::ForStmt *S) { return descendSkippingBreaks(S); }
  bool TraverseWhileStmt(clang::WhileStmt *S) {
    return descendSkippingBreaks(S);
  }
  bool TraverseDoStmt(clang::DoStmt *S) { return descendSkippingBreaks(S); }
  bool TraverseSwitchStmt(clang::SwitchStmt *S) {
    return descendSkippingBreaks(S);
  }

private:
  // Walk a nested construct for memory/spawn blockers but ignore its `break`s,
  // which bind to it rather than to the OVERLAP loop.
  bool descendSkippingBreaks(clang::Stmt *S) {
    RunAheadBlockerVisitor Inner;
    for (clang::Stmt *C : S->children())
      if (C)
        Inner.TraverseStmt(C);
    // Inner's own break/return findings about ITS loop are not ours; only keep
    // the ones that are position-independent.
    if (Reason.empty() && !Inner.Reason.empty() &&
        Inner.Reason != "the body can exit the loop early")
      Reason = Inner.Reason;
    return true;
  }
};

// Classify how a loop-carried variable is updated in the body.
class CarriedUseVisitor : public clang::RecursiveASTVisitor<CarriedUseVisitor> {
public:
  clang::ValueDecl *Var = nullptr;
  unsigned Writes = 0;      // total writes to Var
  unsigned Reads = 0;       // reads of Var NOT part of its own update
  std::string Op;           // the single operator seen across all writes
  bool Inconsistent = false;// two different operators, or an unrecognised shape
  bool ConstStep = true;    // every update's RHS is loop-invariant of Var

  bool VisitDeclRefExpr(clang::DeclRefExpr *DRE) {
    if (DRE->getDecl() == Var)
      Reads++;
    return true;
  }

  bool VisitUnaryOperator(clang::UnaryOperator *UO) {
    if (!UO->isIncrementDecrementOp())
      return true;
    auto *DRE =
        llvm::dyn_cast<clang::DeclRefExpr>(UO->getSubExpr()->IgnoreParenImpCasts());
    if (!DRE || DRE->getDecl() != Var)
      return true;
    Writes++;
    Reads--; // the DeclRefExpr we just counted is part of the update
    note(UO->isIncrementOp() ? "+" : "-");
    return true;
  }

  bool VisitBinaryOperator(clang::BinaryOperator *BO) {
    if (!BO->isAssignmentOp())
      return true;
    auto *DRE =
        llvm::dyn_cast<clang::DeclRefExpr>(BO->getLHS()->IgnoreParenImpCasts());
    if (!DRE || DRE->getDecl() != Var)
      return true;
    Writes++;
    Reads--; // the LHS reference
    switch (BO->getOpcode()) {
    case clang::BO_AddAssign: note("+"); break;
    case clang::BO_SubAssign: note("-"); break;
    case clang::BO_MulAssign: note("*"); break;
    case clang::BO_AndAssign: note("&"); break;
    case clang::BO_OrAssign:  note("|"); break;
    case clang::BO_XorAssign: note("^"); break;
    case clang::BO_Assign: {
      // `x = x <op> e` is the same reduction written out longhand.
      auto *RHS =
          llvm::dyn_cast<clang::BinaryOperator>(BO->getRHS()->IgnoreParenImpCasts());
      if (!RHS) {
        Inconsistent = true;
        break;
      }
      auto *L =
          llvm::dyn_cast<clang::DeclRefExpr>(RHS->getLHS()->IgnoreParenImpCasts());
      auto *R =
          llvm::dyn_cast<clang::DeclRefExpr>(RHS->getRHS()->IgnoreParenImpCasts());
      const bool LIsVar = L && L->getDecl() == Var;
      const bool RIsVar = R && R->getDecl() == Var;
      if (!LIsVar && !RIsVar) {
        Inconsistent = true;
        break;
      }
      Reads--; // the self-reference inside the RHS
      switch (RHS->getOpcode()) {
      case clang::BO_Add: note("+"); break;
      case clang::BO_Mul: note("*"); break;
      case clang::BO_And: note("&"); break;
      case clang::BO_Or:  note("|"); break;
      case clang::BO_Xor: note("^"); break;
      // Subtraction is associative only with the variable on the left.
      case clang::BO_Sub:
        if (LIsVar)
          note("-");
        else
          Inconsistent = true;
        break;
      default: Inconsistent = true; break;
      }
      break;
    }
    default: Inconsistent = true; break;
    }
    return true;
  }

private:
  void note(llvm::StringRef O) {
    if (Op.empty())
      Op = O.str();
    else if (Op != O)
      Inconsistent = true;
  }
};

// ─── Cross-iteration memory dependence ───────────────────────────────────────
//
// A store in the body does not by itself prevent run-ahead. What prevents it is
// two iterations touching the same address. This pass proves they cannot, for
// the shape that actually appears in these kernels:
//
//     for (i = ...)  { ...  base[f(i)] = e;  ... }
//
// It has to establish three things:
//
//   1. `base` is fixed for the whole loop, so `base[x]` names the same object
//      every iteration.
//   2. `f` is injective in the induction variable — affine with a non-zero
//      coefficient — so distinct iterations pick distinct slots of that object.
//   3. Nothing else in the body can reach a slot a different iteration writes.
//
// (3) is where aliasing enters, and it is answered by provenance rather than by
// type: every access is traced back to the named object its address derives
// from, and two accesses conflict only if they share that root or either root
// is unknown. A pointer *loaded out of* another object inherits that object's
// root — `nbrs = (uint32_t *)pGraph[2 * v]` roots at `pGraph` — which is the
// assumption HardCilk already makes by giving each pointer argument its own
// memory space, and which the surrounding kernels already depend on (sibling
// applyFn tasks would otherwise race today). A pointer that cannot be traced to
// a named object at all is unknown and conflicts with everything.
//
// Everything not understood is a rejection, never an assumption.

// Parentheses and casts are transparent to an address computation.
const clang::Expr *stripAddr(const clang::Expr *E) {
  return E ? E->IgnoreParenCasts() : nullptr;
}

// Syntactic equality, over the expression shapes that appear in a subscript.
// Anything unrecognised compares unequal, so a caller can only ever conclude
// "provably the same address", never "provably different".
bool sameAddrExpr(const clang::Expr *A, const clang::Expr *B) {
  A = stripAddr(A);
  B = stripAddr(B);
  if (!A || !B)
    return false;
  if (A == B)
    return true;
  if (A->getStmtClass() != B->getStmtClass())
    return false;
  if (auto *DA = llvm::dyn_cast<clang::DeclRefExpr>(A))
    return DA->getDecl() ==
           llvm::cast<clang::DeclRefExpr>(B)->getDecl();
  if (auto *IA = llvm::dyn_cast<clang::IntegerLiteral>(A))
    return llvm::APInt::isSameValue(
        IA->getValue(), llvm::cast<clang::IntegerLiteral>(B)->getValue());
  if (auto *BA = llvm::dyn_cast<clang::BinaryOperator>(A)) {
    auto *BB = llvm::cast<clang::BinaryOperator>(B);
    return BA->getOpcode() == BB->getOpcode() &&
           sameAddrExpr(BA->getLHS(), BB->getLHS()) &&
           sameAddrExpr(BA->getRHS(), BB->getRHS());
  }
  if (auto *UA = llvm::dyn_cast<clang::UnaryOperator>(A)) {
    auto *UB = llvm::cast<clang::UnaryOperator>(B);
    return UA->getOpcode() == UB->getOpcode() &&
           sameAddrExpr(UA->getSubExpr(), UB->getSubExpr());
  }
  if (auto *SA = llvm::dyn_cast<clang::ArraySubscriptExpr>(A)) {
    auto *SB = llvm::cast<clang::ArraySubscriptExpr>(B);
    return sameAddrExpr(SA->getBase(), SB->getBase()) &&
           sameAddrExpr(SA->getIdx(), SB->getIdx());
  }
  return false;
}

// Membership test tolerating the const-qualified pointers the AST hands back
// for a const expression; the sets themselves are keyed on mutable decls.
bool containsDecl(const std::set<clang::ValueDecl *> &S, const clang::Decl *D) {
  auto *VD = llvm::dyn_cast_or_null<clang::ValueDecl>(D);
  return VD && S.count(const_cast<clang::ValueDecl *>(VD));
}

// The named object an address derives from, or null when that cannot be
// determined. `Written` is the set of variables the body assigns: a pointer
// reassigned inside the loop has no single provenance, so it is unknown.
const clang::ValueDecl *addrRoot(const clang::Expr *E,
                                 const std::set<clang::ValueDecl *> &Written,
                                 unsigned Depth = 0) {
  E = stripAddr(E);
  if (!E || Depth > 8)
    return nullptr;
  if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(E)) {
    const clang::ValueDecl *D = DRE->getDecl();
    if (containsDecl(Written, D))
      return nullptr;
    // A pointer initialised from another object inherits its provenance
    // (`int *my_walk = &global_buffer[u * walkLength];` roots at
    // `global_buffer`); a parameter or global is a root of its own.
    if (auto *VD = llvm::dyn_cast<clang::VarDecl>(D))
      if (VD->hasInit())
        if (auto *R = addrRoot(VD->getInit(), Written, Depth + 1))
          return R;
    return D;
  }
  // A pointer read out of an object belongs to that object's world.
  if (auto *ASE = llvm::dyn_cast<clang::ArraySubscriptExpr>(E))
    return addrRoot(ASE->getBase(), Written, Depth + 1);
  if (auto *UO = llvm::dyn_cast<clang::UnaryOperator>(E)) {
    if (UO->getOpcode() == clang::UO_AddrOf ||
        UO->getOpcode() == clang::UO_Deref)
      return addrRoot(UO->getSubExpr(), Written, Depth + 1);
    return nullptr;
  }
  // Pointer arithmetic: the pointer operand carries the provenance.
  if (auto *BO = llvm::dyn_cast<clang::BinaryOperator>(E)) {
    if (BO->getOpcode() == clang::BO_Add || BO->getOpcode() == clang::BO_Sub) {
      if (BO->getLHS()->getType()->isPointerType())
        return addrRoot(BO->getLHS(), Written, Depth + 1);
      if (BO->getRHS()->getType()->isPointerType())
        return addrRoot(BO->getRHS(), Written, Depth + 1);
    }
  }
  return nullptr;
}

// How a subscript expression depends on the induction variable.
//   Invariant — same value in every iteration
//   Linear    — a*i + b with a != 0, so distinct iterations give distinct slots
//   Unknown   — anything else, including a dependence on another carried
//               variable or on an inner loop's counter
enum class IdxKind { Invariant, Linear, Unknown };

bool isNonZeroIntLiteral(const clang::Expr *E) {
  auto *IL = llvm::dyn_cast_or_null<clang::IntegerLiteral>(stripAddr(E));
  return IL && !IL->getValue().isZero();
}

IdxKind classifyIndex(const clang::Expr *E, const clang::ValueDecl *IndVar,
                      const LoopBodyVarCollector &LC) {
  E = stripAddr(E);
  if (!E)
    return IdxKind::Unknown;
  if (llvm::isa<clang::IntegerLiteral>(E) ||
      llvm::isa<clang::CharacterLiteral>(E))
    return IdxKind::Invariant;
  if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(E)) {
    auto *D = DRE->getDecl();
    if (D == IndVar)
      return IdxKind::Linear;
    // Declared inside the body (an inner loop's counter, a temporary) or
    // assigned by the body: not fixed across iterations.
    if (containsDecl(LC.DeclaredVars, D) || containsDecl(LC.AssignedVars, D))
      return IdxKind::Unknown;
    return IdxKind::Invariant;
  }
  if (auto *UO = llvm::dyn_cast<clang::UnaryOperator>(E)) {
    if (UO->getOpcode() == clang::UO_Plus || UO->getOpcode() == clang::UO_Minus)
      return classifyIndex(UO->getSubExpr(), IndVar, LC);
    return IdxKind::Unknown;
  }
  auto *BO = llvm::dyn_cast<clang::BinaryOperator>(E);
  if (!BO)
    return IdxKind::Unknown;
  const IdxKind L = classifyIndex(BO->getLHS(), IndVar, LC);
  const IdxKind R = classifyIndex(BO->getRHS(), IndVar, LC);
  if (L == IdxKind::Unknown || R == IdxKind::Unknown)
    return IdxKind::Unknown;
  const bool BothInvariant =
      L == IdxKind::Invariant && R == IdxKind::Invariant;
  switch (BO->getOpcode()) {
  case clang::BO_Add:
  case clang::BO_Sub:
    if (BothInvariant)
      return IdxKind::Invariant;
    // `i - i` cancels the induction variable out; only one linear term is a
    // shifted copy of it.
    if (L == IdxKind::Linear && R == IdxKind::Linear)
      return IdxKind::Unknown;
    return IdxKind::Linear;
  case clang::BO_Mul: {
    if (BothInvariant)
      return IdxKind::Invariant;
    if (L == IdxKind::Linear && R == IdxKind::Linear)
      return IdxKind::Unknown;
    // Scaling stays injective only by a non-zero constant; a runtime-invariant
    // scale could be zero, collapsing every iteration onto one slot.
    const clang::Expr *Scale =
        L == IdxKind::Linear ? BO->getRHS() : BO->getLHS();
    return isNonZeroIntLiteral(Scale) ? IdxKind::Linear : IdxKind::Unknown;
  }
  case clang::BO_Shl:
    if (BothInvariant)
      return IdxKind::Invariant;
    // `i << k` for a loop-invariant k is a non-zero scale.
    return L == IdxKind::Linear && R == IdxKind::Invariant ? IdxKind::Linear
                                                           : IdxKind::Unknown;
  default:
    return IdxKind::Unknown;
  }
}

// Every store and every subscript read in the body. A store's own subscript is
// also collected as an access, which is harmless: it compares equal to itself.
struct MemAccessCollector
    : public clang::RecursiveASTVisitor<MemAccessCollector> {
  std::vector<const clang::Expr *> Stores; // destination lvalue of each store
  std::vector<const clang::ArraySubscriptExpr *> Accesses;
  bool OpaqueAccess = false; // a dereference this pass cannot describe

  bool VisitBinaryOperator(clang::BinaryOperator *BO) {
    if (!BO->isAssignmentOp())
      return true;
    const clang::Expr *L = BO->getLHS()->IgnoreParenImpCasts();
    // A scalar assignment is loop-carried state, classified by CarriedUseVisitor.
    if (!llvm::isa<clang::DeclRefExpr>(L))
      Stores.push_back(L);
    return true;
  }
  bool VisitUnaryOperator(clang::UnaryOperator *UO) {
    if (UO->isIncrementDecrementOp()) {
      const clang::Expr *S = UO->getSubExpr()->IgnoreParenImpCasts();
      if (!llvm::isa<clang::DeclRefExpr>(S))
        Stores.push_back(S);
    } else if (UO->getOpcode() == clang::UO_Deref) {
      OpaqueAccess = true;
    }
    return true;
  }
  bool VisitArraySubscriptExpr(clang::ArraySubscriptExpr *ASE) {
    Accesses.push_back(ASE);
    return true;
  }
};

// Empty when no two iterations can touch the same address; otherwise the reason
// run-ahead is rejected.
std::string checkMemoryIndependence(clang::Stmt *Body,
                                    const clang::ValueDecl *IndVar,
                                    const LoopBodyVarCollector &LC) {
  MemAccessCollector MC;
  MC.TraverseStmt(Body);
  if (MC.Stores.empty())
    return ""; // read-only body: nothing to disambiguate
  if (MC.OpaqueAccess)
    return "the body dereferences a pointer directly, so its accesses cannot "
           "be compared";
  if (!IndVar)
    return "the body stores to memory and the induction variable could not be "
           "identified";

  struct StoreInfo {
    const clang::ArraySubscriptExpr *Access;
    const clang::ValueDecl *Root;
  };
  std::vector<StoreInfo> Stores;

  for (const clang::Expr *S : MC.Stores) {
    auto *ASE = llvm::dyn_cast_or_null<clang::ArraySubscriptExpr>(stripAddr(S));
    if (!ASE)
      return "the body stores to a destination that is not a subscript";
    const clang::ValueDecl *Root = addrRoot(ASE->getBase(), LC.AssignedVars);
    if (!Root)
      return "the body stores through a pointer whose provenance is unknown";
    if (classifyIndex(ASE->getIdx(), IndVar, LC) != IdxKind::Linear)
      return "the body stores to '" + Root->getName().str() +
             "' at an index that is not injective in the induction variable, "
             "so two iterations may write the same address";
    Stores.push_back({ASE, Root});
  }

  // Two stores into the same object must target the identical address, else
  // their affine ranges can overlap across iterations (`out[i]` and `out[i+1]`
  // collide; `out[i]` twice does not).
  for (size_t I = 0; I < Stores.size(); ++I)
    for (size_t J = I + 1; J < Stores.size(); ++J)
      if (Stores[I].Root == Stores[J].Root &&
          !sameAddrExpr(Stores[I].Access, Stores[J].Access))
        return "the body stores to '" + Stores[I].Root->getName().str() +
               "' at two different addresses, which may collide across "
               "iterations";

  // No other access may reach a slot a different iteration writes. Sharing the
  // store's root is allowed only at the identical address, which is this
  // iteration's own slot.
  for (const clang::ArraySubscriptExpr *A : MC.Accesses) {
    const clang::ValueDecl *Root = addrRoot(A->getBase(), LC.AssignedVars);
    for (const StoreInfo &S : Stores) {
      if (Root && Root != S.Root)
        continue; // provably different objects
      if (sameAddrExpr(A, S.Access))
        continue; // this iteration's own slot
      return Root ? ("the body accesses '" + Root->getName().str() +
                     "' at an address another iteration writes")
                  : ("the body accesses memory through a pointer whose "
                     "provenance is unknown, which may alias the store to '" +
                     S.Root->getName().str() + "'");
    }
  }
  return "";
}

} // namespace

// Return the definition FunctionDecl if available, otherwise the decl itself.
static clang::FunctionDecl *toDefinition(clang::FunctionDecl *FD) {
  if (auto *Def = FD->getDefinition())
    return Def;
  return FD;
}

class CilkAnalyzeVisitor
    : public clang::RecursiveASTVisitor<CilkAnalyzeVisitor> {
  FunctionDecl *CurrF = nullptr;
  bool WhileCtx = false;
  bool SyncInWhile = false;

public:
  std::set<FunctionDecl *> Tasks;
  std::set<FunctionDecl *> TaskCallers;
  std::unordered_map<FunctionDecl *, std::vector<WhileStmt *>> WhileWithSync;

  explicit CilkAnalyzeVisitor() {}

  bool VisitFunctionDecl(FunctionDecl *Decl) {
    if (GIgnoreFns.find(Decl->getName().str()) != GIgnoreFns.end()) {
      return false;
    }
    CurrF = toDefinition(Decl);
    return true;
  }

  void HandleSpawn(CallExpr *Expr) {
    if (Expr->getDirectCallee()) {
      Tasks.insert(toDefinition(Expr->getDirectCallee()));
      TaskCallers.insert(CurrF);
    } else {
      PANIC("cannot deduce destination of spawn");
    }
  }

  bool VisitCallExpr(CallExpr *Expr) {
    auto *Callee = Expr->getDirectCallee();
    if (Callee && Tasks.find(toDefinition(Callee)) != Tasks.end()) {
      HandleSpawn(Expr);
    }
    return true;
  }

  bool VisitCilkSpawnExpr(CilkSpawnExpr *Expr) {
    if (auto *CExpr = dyn_cast<CallExpr>(Expr->getSpawnedExpr())) {
      HandleSpawn(CExpr);
    } else {
      PANIC("unrecognized spawned expression, should be a function call");
    }
    return true;
  }

  bool VisitCilkSpawnStmt(CilkSpawnStmt *Stmt) {
    if (auto *CExpr = dyn_cast<CallExpr>(Stmt->getSpawnedStmt())) {
      HandleSpawn(CExpr);
    } else {
      PANIC("unrecognized spawned expression, should be a function call");
    }
    return true;
  }

  bool VisitWhileStmt(WhileStmt *WS) {

    bool SavedWhileCtx = WhileCtx;
    bool SavedSyncInWhile = SyncInWhile;
    WhileCtx = true;
    SyncInWhile = false;
    TraverseStmt(WS->getBody());
    if (SyncInWhile) {
      Tasks.insert(CurrF);
      WhileWithSync[CurrF].push_back(WS);
    }

    WhileCtx = SavedWhileCtx;
    SyncInWhile = SavedSyncInWhile;
    return true;
  }

  bool VisitCilkSyncStmt(CilkSyncStmt *Node) {
    if (WhileCtx == true) {
      SyncInWhile = true;
      return true;
    }
    return true;
  }
};

////////////////////////
// Convert AST to IR //
//////////////////////

static BinopIRExpr::BinopOp astOpToIROp(clang::BinaryOperatorKind K) {
  switch (K) {
  case clang::BO_LT:
    return BinopIRExpr::BINOP_LT;
  case clang::BO_GT:
    return BinopIRExpr::BINOP_GT;
  case clang::BO_LE:
    return BinopIRExpr::BINOP_LE;
  case clang::BO_GE:
    return BinopIRExpr::BINOP_GE;
  case clang::BO_EQ:
    return BinopIRExpr::BINOP_EQ;
  case clang::BO_NE:
    return BinopIRExpr::BINOP_NEQ;
  default:
    llvm_unreachable("Unsupported comparison op in desugared condition");
  }
}

struct SpawnFinder : clang::RecursiveASTVisitor<SpawnFinder> {
  bool Found = false;
  bool VisitCilkSpawnExpr(CilkSpawnExpr *) { return Found = true, false; }
  bool VisitCilkSpawnStmt(CilkSpawnStmt *) { return Found = true, false; }
};
static bool containsSpawn(clang::Stmt *S) {
  SpawnFinder F;
  F.TraverseStmt(S);
  return F.Found;
}

struct VarRefCollector : clang::RecursiveASTVisitor<VarRefCollector> {
  std::set<const clang::NamedDecl *> Refs;
  bool VisitDeclRefExpr(clang::DeclRefExpr *E) {
    Refs.insert(E->getDecl());
    return true;
  }
};
static std::unordered_map<const clang::NamedDecl *, std::string>
buildRenames(clang::Stmt *S,
             const std::unordered_map<ASTVarRef, IRVarRef> &VarLookup) {
  VarRefCollector Collector;
  Collector.TraverseStmt(S);
  std::unordered_map<const clang::NamedDecl *, std::string> Renames;
  for (auto &[ASTDecl, IRVar] : VarLookup)
    if (Collector.Refs.count(ASTDecl))
      Renames[ASTDecl] = GetSym(IRVar->Name);
  return Renames;
}

class Stmt2IRVisitor : public clang::StmtVisitor<Stmt2IRVisitor> {
private:
  IRBasicBlock *CurrB;
  std::vector<IRExpr *> ExprStack;

  std::unordered_map<ASTVarRef, IRVarRef> &VarLookup;
  std::set<FunctionDecl *> Tasks;
  std::set<ASTVarRef> LoopCarriedVars;
  IRFunction *F;
  bool ExprCtx = false;
  bool CallCtx = false;
  bool SpawnCtx = false;
  bool SyncNext = false;
  bool WhileCtx = false;
  bool SyncInWhile = false;
  // Set by the `__bombyx_overlap_here` marker label; consumed by the next
  // VisitWhileStmt/VisitForStmt to flag that loop's LoopIRStmt as OVERLAP.
  bool NextLoopOverlap = false;
  // Set alongside NextLoopOverlap by the `__bombyx_overlap_reassoc_here` marker
  // (`#pragma BOMBYX OVERLAP REASSOC`): the programmer permits the loop's
  // reduction to be reassociated, which is what lets a floating-point
  // accumulator run ahead.
  bool NextLoopReassoc = false;
  // Stack of switch join blocks for break-in-switch handling.
  // A nullptr entry acts as a loop barrier (breaks inside loops don't exit the
  // switch).
  std::vector<IRBasicBlock *> SwitchBreakStack;

  /* Basicially get a Clang expression visit it then in the visit appropriate
  function and convert it to an IR and return it throught the stack ExprStack
  all this while we mark that we are in the context of this expression in
  case of nested calls inside */
  IRExpr *getExpr(Expr *E) {
    assert(E);
    ExprCtx = true;
    Stmt2IRVisitor::Visit(E);
    assert(ExprStack.back());
    auto *IRE = ExprStack.back();
    ExprStack.pop_back();
    ExprCtx = false;
    return IRE;
  }

  /* Come back here; most probably when waiting  for a cilk sync.
  Check when cilk sync is set to true */
  void pushIRStmt(IRStmt *S) {
    CurrB->pushStmtBack(S);
    if (SyncNext) {
      VisitCilkSyncStmt(nullptr);
      SyncNext = false;
    }
  }

  void handleStmt(Stmt *S) {
    assert(S);
    Stmt2IRVisitor::Visit(S);
    if (ExprStack.size() > 0) {
      // should not be expecting to process statements inside expressions
      assert(ExprStack.size() == 1);
      auto *E = ExprStack.back();
      ExprStack.pop_back();
      auto *EW = new ExprWrapIRStmt(E);
      // assert(isa<CallIRExpr>(EW->Expr.get()));
      pushIRStmt((IRStmt *)EW);
    }
  }

  // If the `__bombyx_overlap_here` marker preceded this loop, flag its
  // LoopIRStmt for OVERLAP lowering. We additionally confirm the loop actually
  // carries a loop-carried dependency (a variable written in the body but
  // declared outside it); without one, streaming continuations buy nothing, so
  // we warn and leave the loop on the normal path.
  void consumeOverlapFlag(LoopIRStmt *LS, clang::Stmt *Body,
                          clang::Stmt *LoopStmt = nullptr) {
    if (!NextLoopOverlap)
      return;
    NextLoopOverlap = false;
    const bool Reassoc = NextLoopReassoc;
    NextLoopReassoc = false;
    LoopBodyVarCollector LC;
    if (Body)
      LC.TraverseStmt(Body);
    auto Carried = LC.getLoopCarriedVars();
    if (Carried.empty()) {
      llvm::errs() << "warning: #pragma BOMBYX OVERLAP loop has no loop-carried "
                      "dependency; ignoring OVERLAP\n";
      return;
    }
    LS->Overlap = true;
    analyzeRunAhead(LS, Body, LoopStmt, Carried, LC, Reassoc);
  }

  // Decide whether this OVERLAP loop's iterations may be issued ahead of one
  // another. Rejection is not an error — the loop simply keeps strictly serial
  // outer iteration — but it costs roughly the memory latency per iteration, so
  // the reason is always reported.
  //
  // `Reassoc` is `#pragma BOMBYX OVERLAP REASSOC`: the programmer's assertion
  // that this loop's reduction may be reassociated, which is the only thing
  // that unblocks a floating-point accumulator (see below).
  void analyzeRunAhead(LoopIRStmt *LS, clang::Stmt *Body,
                       clang::Stmt *LoopStmt,
                       const std::set<clang::ValueDecl *> &Carried,
                       const LoopBodyVarCollector &LC, bool Reassoc) {
    auto reject = [&](const llvm::Twine &Why) {
      llvm::errs() << "note: #pragma BOMBYX OVERLAP loop cannot run ahead: "
                   << Why
                   << "; falling back to serial outer iteration\n";
    };

    if (!Body)
      return reject("the loop has no body");

    // (1) An exact, entry-known trip count. Only a `for` gives one; a `while`
    //     whose bound is recomputed per iteration does not, and the join unit
    //     needs to know how many retires to wait for.
    auto *FS = llvm::dyn_cast_or_null<clang::ForStmt>(LoopStmt);
    if (!FS || !FS->getInit() || !FS->getCond() || !FS->getInc())
      return reject("its trip count is not known on entry (not a counted for "
                    "loop)");

    // (2) No real spawn, no early exit, and no call that could mutate state
    //     behind this analysis' back.
    RunAheadBlockerVisitor BV;
    BV.TraverseStmt(Body);
    if (!BV.Reason.empty())
      return reject(BV.Reason);

    // (3) The induction variable, taken from the increment. Needed both by the
    //     memory pass below and by the carried-variable classification.
    clang::ValueDecl *IndVar = nullptr;
    if (auto *Inc = llvm::dyn_cast<clang::UnaryOperator>(
            FS->getInc()->IgnoreParenImpCasts())) {
      if (auto *D = llvm::dyn_cast<clang::DeclRefExpr>(
              Inc->getSubExpr()->IgnoreParenImpCasts()))
        IndVar = D->getDecl();
    } else if (auto *Inc = llvm::dyn_cast<clang::BinaryOperator>(
                   FS->getInc()->IgnoreParenImpCasts())) {
      if (auto *D = llvm::dyn_cast<clang::DeclRefExpr>(
              Inc->getLHS()->IgnoreParenImpCasts()))
        IndVar = D->getDecl();
    }

    // (4) No two iterations may touch the same address. A store is allowed when
    //     it is provably iteration-private; see checkMemoryIndependence.
    if (std::string Why = checkMemoryIndependence(Body, IndVar, LC);
        !Why.empty())
      return reject(Why);

    // (5) Classify every loop-carried variable. Exactly one may be a reduction;
    //     one must be the induction variable; nothing else is allowed.
    clang::ValueDecl *RedVar = nullptr;
    std::string RedOp;
    for (clang::ValueDecl *VD : Carried) {
      if (VD == IndVar)
        continue; // the induction variable is advanced by the wrapper itself

      CarriedUseVisitor CV;
      CV.Var = VD;
      CV.TraverseStmt(Body);
      if (CV.Writes == 0)
        continue; // read-only: loop-invariant, safe
      if (CV.Inconsistent || CV.Op.empty())
        return reject("'" + VD->getName() +
                      "' is carried but is not a recognised reduction");
      if (CV.Reads > 0)
        return reject("'" + VD->getName() +
                      "' is read outside its own reduction update, so "
                      "iterations are not independent");
      // Subtraction reorders only if it is really `x = x - e` accumulating a
      // negated sum; treat it as the associative `+` of negated terms.
      std::string Op = CV.Op == "-" ? "+" : CV.Op;
      if (Op != "+" && Op != "*" && Op != "&" && Op != "|" && Op != "^")
        return reject("'" + VD->getName() + "' uses non-associative operator '" +
                      CV.Op + "'");
      // Floating point + and * are NOT associative: run-ahead reorders the
      // accumulation and would change the result bit-for-bit. REASSOC is the
      // programmer taking responsibility for that difference; it is not a
      // proof, so the reorder is still announced.
      if (VD->getType()->isFloatingType()) {
        if (!Reassoc)
          return reject("'" + VD->getName() +
                        "' is a floating-point reduction, which is not "
                        "associative (add REASSOC to the pragma to allow "
                        "reordering it)");
        llvm::errs() << "note: #pragma BOMBYX OVERLAP REASSOC: reordering the "
                        "floating-point reduction '"
                     << VD->getName()
                     << "'; results may differ from the serial order and are "
                        "not bit-reproducible run to run\n";
      }
      if (RedVar)
        return reject("more than one reduction ('" + RedVar->getName() +
                      "' and '" + VD->getName() + "')");
      RedVar = VD;
      RedOp = Op;
    }

    if (!RedVar) {
      // Nothing to accumulate: the iterations are already independent, so
      // run-ahead needs no join unit at all.
      LS->RunAhead = true;
      return;
    }
    LS->RunAhead = true;
    LS->ReductionVar = RedVar->getName().str();
    LS->ReductionOp = RedOp;
  }

  void handleAssign(IRExpr *Dest, IRExpr *Src) {
    if (auto *DI = dyn_cast<IdentIRExpr>(Dest)) {
      auto *CS = new CopyIRStmt(DI->Ident, Src);
      delete DI;
      pushIRStmt((IRStmt *)CS);
    } else if (auto *LDest = dyn_cast<IRLvalExpr>(Dest)) {
      auto *SS = new StoreIRStmt(LDest, Src);
      pushIRStmt((IRStmt *)SS);
    } else {
      llvm_unreachable("Unsupported LHS of assign");
    }
  }

  void handleArrayInitList(IRVarRef VR, QualType VarTy, InitListExpr *ILE) {
    auto *ArrTy = dyn_cast<ConstantArrayType>(VarTy->getAsArrayTypeUnsafe());
    if (!ArrTy) {
      PANIC("unsupported: non-constant array initializer list");
    }

    if (!ILE->isSemanticForm() && ILE->getSemanticForm()) {
      ILE = ILE->getSemanticForm();
    }

    const auto ElemTy = ArrTy->getElementType();
    const uint64_t ElemCount = ArrTy->getSize().getZExtValue();

    for (uint64_t I = 0; I < ElemCount; ++I) {
      Expr *Init = nullptr;
      if (I < ILE->getNumInits()) {
        Init = ILE->getInit(I);
      } else if (ILE->hasArrayFiller()) {
        Init = ILE->getArrayFiller();
      }

      IRExpr *Src =
          Init ? getExpr(Init) : static_cast<IRExpr *>(new IntLiteralIRExpr(0));
      auto *Dest =
          new IndexIRExpr(new IdentIRExpr(VR), new IntLiteralIRExpr(I), ElemTy);
      handleAssign(Dest, Src);
    }
  }

  void handleVarInit(IRVarRef VR, VarDecl *VD) {
    if (!VD->hasInit()) {
      return;
    }

    Expr *Init = VD->getInit()->IgnoreParenImpCasts();

    if (auto *CE = llvm::dyn_cast<clang::CXXConstructExpr>(Init)) {
      if (CE->getNumArgs() == 0)
        return;
      // Preserve constructor-initialization syntax .
      // If the constructor is a real (non-elidable, non-copy) call, skip the
      // CopyIRStmt and let printLocals emit "Type name(args);" from ASTDecl.
      if (!CE->isElidable() && !CE->getConstructor()->isCopyConstructor())
        return;
      if (CE->getNumArgs() == 1)
        Init = CE->getArg(0)->IgnoreParenImpCasts();
    }

    if (auto *ILE = llvm::dyn_cast<InitListExpr>(Init)) {
      if (VD->getType()->isArrayType()) {
        handleArrayInitList(VR, VD->getType(), ILE);
        return;
      }

      if (auto *RD = VD->getType()->getAsRecordDecl()) {
        auto FieldIt = RD->field_begin();
        for (unsigned I = 0; I < ILE->getNumInits(); ++I, ++FieldIt) {
          if (FieldIt == RD->field_end())
            break;
          IRExpr *Src = getExpr(ILE->getInit(I));
          auto *Dest = new AccessIRExpr(VR, FieldIt->getName().str(), false);
          handleAssign(Dest, Src);
        }
        return;
      }
    }

    auto *IE = getExpr(VD->getInit());
    pushIRStmt(new CopyIRStmt(VR, IE));
  }

  // Emit `if (Cond) break;` in CurrB, then advance CurrB to the continuation
  // block.  The BreakIRStmt has no explicit successor — downstream passes
  // resolve it to the enclosing loop's join block.
  void emitIfBreak(IRExpr *Cond) {
    auto *CheckB = CurrB;
    auto *BreakB = F->createBlock();
    auto *ContB = F->createBlock();

    CheckB->Term = new IfIRStmt(Cond);
    CheckB->Succs.insert(BreakB); // true  → break
    CheckB->Succs.insert(ContB);  // false → continue body

    CurrB = BreakB;
    CurrB->Term = new BreakIRStmt();

    CurrB = ContB;
  }

  // Emit the desugared prefix for a loop body that was produced by
  // analyzeCondForDesugar:
  //   1. AssignBO as a statement   (e.g. h = *curr_high)
  //   2. if (!(clean_cond)) break; (e.g. if (!(h > pivot)) break;)
  // Advances CurrB to the block where the original loop body should follow.
  void emitDesugaredBodyPrefix(const WhileCondDesugar &DS) {
    // Emit the assignment (e.g. h = *curr_high) as a regular statement.
    handleStmt(DS.AssignBO);

    // Recover the IR variable that was just assigned.
    auto *AssignedLHS = DS.AssignBO->getLHS()->IgnoreParenImpCasts();
    auto *AssignedDRE = dyn_cast<DeclRefExpr>(AssignedLHS);
    assert(AssignedDRE && "Desugared assignment LHS must be a simple variable");

    IRExpr *AssignedVarExpr;
    auto It = VarLookup.find(AssignedDRE->getDecl());
    if (It != VarLookup.end())
      AssignedVarExpr = new IdentIRExpr(It->second);
    else
      AssignedVarExpr = new ASTLiteralIRExpr(AssignedDRE);

    IRExpr *NegBreakCond;
    if (DS.CompBO) {
      // Clean condition: assigned_var op CompBO->RHS
      IRExpr *RHSExpr = getExpr(DS.CompBO->getRHS());
      auto Op = astOpToIROp(DS.CompBO->getOpcode());
      auto *CleanCond = new BinopIRExpr(Op, AssignedVarExpr, RHSExpr);
      NegBreakCond = new UnopIRExpr(UnopIRExpr::UNOP_L_NOT, CleanCond);
    } else {
      // Condition is just the assignment: break if the assigned value is falsy.
      NegBreakCond = new UnopIRExpr(UnopIRExpr::UNOP_L_NOT, AssignedVarExpr);
    }

    emitIfBreak(NegBreakCond);
  }

public:
  Stmt2IRVisitor(IRFunction *F,
                 std::unordered_map<ASTVarRef, IRVarRef> &VarLookup,
                 std::set<FunctionDecl *> Tasks)
      : F(F), VarLookup(VarLookup), Tasks(Tasks) {
    CurrB = F->createBlock();
  }

  IRBasicBlock *getCurrBlock() { return CurrB; }

  void VisitStmt(Stmt *S) {
    llvm::errs() << "Unhandled statement node: " << S->getStmtClassName()
                 << "\n";
    llvm_unreachable("Unhandled statement node");
  }
  void VisitExpr(Expr *E) {
    llvm::errs() << "Unhandled expr node: " << E->getStmtClassName() << "\n";
    llvm_unreachable("Unhandled expr node");
  }

  void HandleCilkSpawn(IRExpr *SpawnedE) {
    if (auto *CallE = dyn_cast<CallIRExpr>(SpawnedE)) {
      std::vector<IRExpr *> Args;
      for (auto &Arg : CallE->Args) {
        Args.push_back(Arg.get());
        Arg.release();
      }
      IRFunRef FR = CallE->Fn;
      delete CallE;
      auto *SE = new ISpawnIRExpr(FR, Args);
      ExprStack.push_back((IRExpr *)SE);
    } else {
      PANIC("unsupported: cilk spawn on non call expression");
    }
  }

  ////////////
  // Stmts //
  //////////

  void VisitCompoundStmt(CompoundStmt *Node) {
    for (auto &child : Node->children()) {
      handleStmt(child);
    }
  }

  void VisitLabelStmt(LabelStmt *Node) {
    if (Node->getDecl()->getName() == "__bombyx_dae_here") {
      pushIRStmt(new ScopeAnnotIRStmt(ScopeAnnot::SA_DAE_HERE));
      handleStmt(Node->getSubStmt());
      return;
    }
    if (Node->getDecl()->getName() == "__bombyx_overlap_here" ||
        Node->getDecl()->getName() == "__bombyx_overlap_reassoc_here") {
      // Marker injected by `#pragma BOMBYX OVERLAP`; it labels the loop that
      // follows. Flag the next loop statement so it is lowered to in-order
      // streaming continuations. The `_reassoc_` spelling additionally carries
      // the REASSOC clause, which permits reordering a floating-point
      // reduction so the loop can still run ahead.
      NextLoopOverlap = true;
      NextLoopReassoc =
          Node->getDecl()->getName() == "__bombyx_overlap_reassoc_here";
      handleStmt(Node->getSubStmt());
      return;
    }
    if (!containsSpawn(Node)) {
      pushIRStmt(new ASTStmtWrapIRStmt(Node, buildRenames(Node, VarLookup)));
      return;
    }
    PANIC("unsupported: label statement with cilk_spawn")
  }

  void VisitGotoStmt(GotoStmt *Node) {
    pushIRStmt(new ASTStmtWrapIRStmt(Node));
  }

  void VisitNullStmt(NullStmt *Node) {}

  void VisitDeclStmt(DeclStmt *Node) {
    for (auto *D : Node->decls()) {
      auto *VD = dyn_cast<VarDecl>(D);
      if (!VD)
        continue; // skip implicit decls like lambda closure types
      Sym VDS = PutSym(VD->getName().str());
      F->Vars.push_back(IRVarDecl{
          .Type = VD->getType(),
          .Name = VDS,
          .DeclLoc = IRVarDecl::LOCAL,
          .ASTDecl = VD,
      });
      IRVarRef VR = &F->Vars.back();
      VarLookup[VD] = VR;
      handleVarInit(VR, VD);
    }
  }

  void VisitIfStmt(IfStmt *IS) {
    // ??
    assert(!IS->hasInitStorage());

    auto *Cond = getExpr(IS->getCond());

    auto *IRS = new IfIRStmt(Cond);
    CurrB->Term = (IRTerminatorStmt *)IRS;
    IRBasicBlock *BranchB = CurrB;
    CurrB = F->createBlock();
    BranchB->Succs.insert(CurrB);
    IRBasicBlock *JoinB = F->createBlock();
    handleStmt(IS->getThen());
    CurrB->Succs.insert(JoinB);

    if (IS->hasElseStorage()) {
      CurrB = F->createBlock();
      BranchB->Succs.insert(CurrB);
      handleStmt(IS->getElse());
      CurrB->Succs.insert(JoinB);
    } else {
      BranchB->Succs.insert(JoinB);
    }
    CurrB = JoinB;
  }

  // drop statements after a return
  void VisitForStmt(ForStmt *FS) {
    WhileCondDesugar DS;
    if (FS->getCond())
      DS = analyzeCondForDesugar(FS->getCond());

    auto *ForB = F->createBlock();
    auto *BodyB = F->createBlock();
    auto *IncB = F->createBlock();
    auto *JoinB = F->createBlock();

    CurrB->Succs.insert(ForB);
    IncB->Succs.insert(ForB);
    ForB->Succs.insert(BodyB);
    ForB->Succs.insert(JoinB);

    CurrB = ForB;
    if (FS->getInit())
      handleStmt(FS->getInit());
    assert(CurrB == ForB);
    IRStmt *InitS = nullptr;
    if (CurrB->begin() != CurrB->end() && CurrB->back()) {
      InitS = CurrB->back().get();
      InitS->setSilent();
    }

    CurrB = IncB;
    if (FS->getInc())
      handleStmt(FS->getInc());
    assert(CurrB == IncB);
    IRStmt *IncS = nullptr;
    if (CurrB->begin() != CurrB->end() && CurrB->back()) {
      IncS = CurrB->back().get();
      IncS->setSilent();
    }

    IRExpr *Cond = nullptr;
    if (DS.AssignBO) {
      // Use only the part of the condition before && as the loop guard;
      // the assignment and inner check move into the body.
      Cond = DS.OuterCond ? getExpr(DS.OuterCond)
                          : static_cast<IRExpr *>(new IntLiteralIRExpr(1));
    } else if (FS->getCond()) {
      Cond = getExpr(FS->getCond());
    } else {
      // for(;;) — no condition means loop forever.
      Cond = new IntLiteralIRExpr(1);
    }

    auto *ForS = new LoopIRStmt(Cond, IncS, InitS);
    consumeOverlapFlag(ForS, FS->getBody(), FS);
    ForB->Term = (IRTerminatorStmt *)ForS;

    CurrB = BodyB;

    if (DS.AssignBO)
      emitDesugaredBodyPrefix(
          DS); // advances CurrB to the post-break-check block

    SwitchBreakStack.push_back(
        nullptr); // loop masks any enclosing switch break
    handleStmt(FS->getBody());
    SwitchBreakStack.pop_back();
    CurrB->Succs.insert(IncB);

    CurrB = JoinB;
  }

  void VisitReturnStmt(ReturnStmt *RS) {
    assert(!CurrB->Term);
    IRExpr *RE = nullptr;
    if (RS->getRetValue()) {
      RE = getExpr(RS->getRetValue());
    }
    auto *RetS = new ReturnIRStmt(RE);
    CurrB->Term = (IRTerminatorStmt *)RetS;
  }

  void VisitWhileStmt(WhileStmt *WS) {
    WhileCondDesugar DS = analyzeCondForDesugar(WS->getCond());

    auto *WhileB = F->createBlock();
    auto *BodyB = F->createBlock();
    auto *JoinB = F->createBlock();

    CurrB->Succs.insert(WhileB);
    WhileB->Succs.insert(BodyB);
    WhileB->Succs.insert(JoinB);

    IRExpr *LoopCond;
    if (DS.AssignBO) {
      // Desugared: the outer condition (before &&) guards the loop header;
      // nullptr means while(1) — the assignment+break inside the body exits.
      LoopCond = DS.OuterCond ? getExpr(DS.OuterCond)
                              : static_cast<IRExpr *>(new IntLiteralIRExpr(1));
    } else {
      LoopCond = getExpr(WS->getCond());
    }

    auto *WhileS = new LoopIRStmt(LoopCond, nullptr, nullptr);
    consumeOverlapFlag(WhileS, WS->getBody(), WS);
    WhileB->Term = (IRTerminatorStmt *)WhileS;

    CurrB = BodyB;

    if (DS.AssignBO)
      emitDesugaredBodyPrefix(
          DS); // advances CurrB to the post-break-check block

    // Save and restore WhileCtx/SyncInWhile so that nested while
    // loops do not clobber the outer loop's state.
    bool SavedWhileCtx = WhileCtx;
    bool SavedSyncInWhile = SyncInWhile;
    WhileCtx = true;
    SyncInWhile = false;
    SwitchBreakStack.push_back(
        nullptr); // loop masks any enclosing switch break
    handleStmt(WS->getBody());
    SwitchBreakStack.pop_back();
    bool ThisSyncInWhile = SyncInWhile;
    WhileCtx = SavedWhileCtx;
    SyncInWhile = SavedSyncInWhile;

    if (ThisSyncInWhile && SavedWhileCtx)
      SyncInWhile = true;

    CurrB->Succs.insert(WhileB);
    CurrB = JoinB;
  }

  void VisitDoStmt(DoStmt *DS) {
    if (!containsSpawn(DS)) {
      pushIRStmt(new ASTStmtWrapIRStmt(DS, buildRenames(DS, VarLookup)));
      return;
    }

    auto *DoAnnot = new ScopeAnnotIRStmt(ScopeAnnot::SA_DO);
    pushIRStmt((IRStmt *)DoAnnot);

    auto *LoopB = F->createBlock();
    auto *JoinB = F->createBlock();

    CurrB->Succs.insert(LoopB);

    CurrB = LoopB;
    handleStmt(DS->getBody());
    CurrB->Succs.insert(LoopB);
    CurrB->Succs.insert(JoinB);

    IRExpr *Cond = getExpr(DS->getCond());
    auto *WhileS = new LoopIRStmt(Cond, nullptr, nullptr);
    CurrB->Term = (IRTerminatorStmt *)WhileS;

    CurrB = JoinB;
  }

  void VisitBreakStmt(BreakStmt *Node) {
    assert(!CurrB->Term);
    if (!SwitchBreakStack.empty() && SwitchBreakStack.back() != nullptr) {
      // Break inside a switch case — jump directly to the switch join block.
      CurrB->Succs.insert(SwitchBreakStack.back());
      CurrB = F->createBlock(); // dead continuation block
    } else {
      CurrB->Term = new BreakIRStmt();
    }
  }

  void VisitSwitchStmt(SwitchStmt *SS) {
    pushIRStmt(new ASTStmtWrapIRStmt(SS, buildRenames(SS, VarLookup)));
  }

  void VisitCilkSyncStmt(CilkSyncStmt *Node) {
    assert(!CurrB->Term);
    if (WhileCtx == true) {
      SyncInWhile = true;
    }
    auto *SyncS = new SyncIRStmt();
    CurrB->Term = (IRTerminatorStmt *)SyncS;
    auto *JoinB = F->createBlock();
    CurrB->Succs.insert(JoinB);
    CurrB = JoinB;
  }

  void VisitCilkSpawnStmt(CilkSpawnStmt *Stmt) {
    Expr *E = dyn_cast<Expr>(Stmt->getSpawnedStmt());
    if (!E) {
      PANIC("unrecognized expression in cilk spawn statement");
    }
    SpawnCtx = true;
    auto *SpawnE = getExpr(E);
    SpawnCtx = false;
    HandleCilkSpawn(SpawnE);
  }

  ////////////
  // Exprs //
  //////////

  void VisitImplicitCastExpr(ImplicitCastExpr *Node) {
    ExprStack.push_back(getExpr(Node->getSubExpr()));
  }

  void VisitParenExpr(ParenExpr *Node) {
    ExprStack.push_back(getExpr(Node->getSubExpr()));
  }

  void VisitMemberExpr(MemberExpr *Node) {
    IRExpr *BaseExpr = getExpr(Node->getBase());
    auto *BaseLval = dyn_cast<IRLvalExpr>(BaseExpr);
    if (!BaseLval) {
      llvm::errs() << "MemberExpr base kind: "
                   << Node->getBase()->getStmtClassName() << "\n";
      llvm::errs() << "MemberExpr text: ";
      Node->printPretty(llvm::errs(), nullptr, PrintingPolicy(LangOptions()));
      llvm::errs() << "\n";
      PANIC("unsupported: non-lvalue base in member expression");
    }
    auto *AE = new AccessIRExpr(
        BaseLval, Node->getMemberDecl()->getName().str(), Node->isArrow());
    ExprStack.push_back(AE);
  }

  void VisitConstantExpr(ConstantExpr *Node) { Visit(Node->getSubExpr()); }

  void VisitIntegerLiteral(IntegerLiteral *Node) {
    auto *LE = new ASTLiteralIRExpr(Node);
    ExprStack.push_back((IRExpr *)LE);
  }

  void VisitFloatingLiteral(FloatingLiteral *Node) {
    auto *LE = new ASTLiteralIRExpr(Node);
    ExprStack.push_back((IRExpr *)LE);
  }

  void VisitStringLiteral(StringLiteral *Node) {
    auto *LE = new ASTLiteralIRExpr(Node);
    ExprStack.push_back((IRExpr *)LE);
  }

  void VisitGNUNullExpr(GNUNullExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitInitListExpr(InitListExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitCXXConstructExpr(clang::CXXConstructExpr *Node) {
    if (Node->getNumArgs() == 0) {
      ExprStack.push_back(new ASTLiteralIRExpr(Node));
      return;
    }

    if (Node->isElidable() && Node->getNumArgs() == 1) {
      ExprStack.push_back(getExpr(Node->getArg(0)));
      return;
    }

    if (Node->getConstructor()->isCopyConstructor() &&
        Node->getNumArgs() == 1) {
      ExprStack.push_back(getExpr(Node->getArg(0)));
      return;
    }

    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitBinaryOperator(BinaryOperator *Node) {
    if (Node->getOpcode() == clang::BO_Comma) {
      if (!ExprCtx) {
        // for (i=0, j=0; ...) or for (...; i++, j++)
        // emit each operand as its own statement in order.
        handleStmt(Node->getLHS());
        handleStmt(Node->getRHS());
      } else {
        // x = (a++, b+1)
        // LHS is a side-effect statement, RHS is the value.
        ExprCtx = false;
        handleStmt(Node->getLHS());
        ExprCtx = true;
        ExprStack.push_back(getExpr(Node->getRHS()));
      }
      return;
    }

    // Assignment used as an expression: a = b - (c = d)
    if (Node->isAssignmentOp() && ExprCtx) {
      ExprCtx = false;
      handleStmt(Node);
      ExprCtx = true;
      ExprStack.push_back(getExpr(Node->getLHS()));
      return;
    }

    IRExpr *Left = getExpr(Node->getLHS());
    IRExpr *Right = getExpr(Node->getRHS());

    BinopIRExpr::BinopOp Op;
    switch (Node->getOpcode()) {
    case clang::BO_Assign:
      break;
    case clang::BO_MulAssign:
    case clang::BO_Mul:
      Op = BinopIRExpr::BINOP_MUL;
      break;
    case clang::BO_DivAssign:
    case clang::BO_Div:
      Op = BinopIRExpr::BINOP_DIV;
      break;
    case clang::BO_RemAssign:
    case clang::BO_Rem:
      Op = BinopIRExpr::BINOP_MOD;
      break;
    case clang::BO_AddAssign:
    case clang::BO_Add:
      Op = BinopIRExpr::BINOP_ADD;
      break;
    case clang::BO_SubAssign:
    case clang::BO_Sub:
      Op = BinopIRExpr::BINOP_SUB;
      break;
    case clang::BO_ShlAssign:
    case clang::BO_Shl:
      Op = BinopIRExpr::BINOP_SHL;
      break;
    case clang::BO_ShrAssign:
    case clang::BO_Shr:
      Op = BinopIRExpr::BINOP_SHR;
      break;
    case clang::BO_LT:
      Op = BinopIRExpr::BINOP_LT;
      break;
    case clang::BO_GT:
      Op = BinopIRExpr::BINOP_GT;
      break;
    case clang::BO_LE:
      Op = BinopIRExpr::BINOP_LE;
      break;
    case clang::BO_GE:
      Op = BinopIRExpr::BINOP_GE;
      break;
    case clang::BO_EQ:
      Op = BinopIRExpr::BINOP_EQ;
      break;
    case clang::BO_NE:
      Op = BinopIRExpr::BINOP_NEQ;
      break;
    case clang::BO_AndAssign:
    case clang::BO_And:
      Op = BinopIRExpr::BINOP_AND;
      break;
    case clang::BO_OrAssign:
    case clang::BO_Or:
      Op = BinopIRExpr::BINOP_OR;
      break;
    case clang::BO_XorAssign:
    case clang::BO_Xor:
      Op = BinopIRExpr::BINOP_XOR;
      break;
    case clang::BO_LAnd:
      Op = BinopIRExpr::BINOP_LAND;
      break;
    case clang::BO_LOr:
      Op = BinopIRExpr::BINOP_LOR;
      break;
    default: {
      llvm::errs() << "Unknown binary operator: " << Node->getOpcodeStr()
                   << "\n";
      llvm_unreachable("Unknown binary operator");
    }
    }

    if (Node->getOpcode() == clang::BO_Assign) {
      assert(!ExprCtx);
      if (dyn_cast<ASTLiteralIRExpr>(Left)) {
        // Global or untracked variable — emit the whole assignment opaquely.
        delete Left;
        delete Right;
        pushIRStmt(new ExprWrapIRStmt(new ASTLiteralIRExpr(Node)));
      } else {
        handleAssign(Left, Right);
      }
    } else if (Node->getOpcode() > clang::BO_Assign &&
               Node->getOpcode() <= clang::BO_OrAssign) {
      assert(!ExprCtx);
      auto *DI = dyn_cast<IdentIRExpr>(Left);
      if (!DI) {
        // compound assignment opaquely so VarRenamePrinterHelper
        delete Left;
        delete Right;
        pushIRStmt(new ExprWrapIRStmt(new ASTLiteralIRExpr(Node)));
      } else {
        auto *LeftC = new IdentIRExpr(DI->Ident);
        BinopIRExpr *BE = new BinopIRExpr(Op, LeftC, Right);
        handleAssign(Left, (IRExpr *)BE);
      }
    } else {
      BinopIRExpr *BE = new BinopIRExpr(Op, Left, Right);
      ExprStack.push_back((IRExpr *)BE);
    }
  }

  void VisitUnaryOperator(UnaryOperator *Node) {
    IRExpr *SE = getExpr(Node->getSubExpr());

    UnopIRExpr::UnopOp Op;
    switch (Node->getOpcode()) {
    case clang::UO_AddrOf: {
      auto *RE = new RefIRExpr(SE);
      ExprStack.push_back((IRExpr *)RE);
      return;
    }
    case clang::UO_Deref: {
      QualType PointeeType = (Node->getSubExpr()->getType())->getPointeeType();
      auto *DR = new DRefIRExpr(SE, PointeeType);
      ExprStack.push_back((IRExpr *)DR);
      return;
    }
    case clang::UO_LNot:
      Op = UnopIRExpr::UNOP_L_NOT;
      break;
    case clang::UO_Not:
      Op = UnopIRExpr::UNOP_NOT;
      break;
    case clang::UO_Minus:
      Op = UnopIRExpr::UNOP_NEG;
      break;
    case clang::UO_PreDec:
      Op = UnopIRExpr::UNOP_PREDEC;
      break;
    case clang::UO_PostDec:
      Op = UnopIRExpr::UNOP_POSTDEC;
      break;
    case clang::UO_PreInc:
      Op = UnopIRExpr::UNOP_PREINC;
      break;
    case clang::UO_PostInc:
      Op = UnopIRExpr::UNOP_POSTINC;
      break;
    default: {
      llvm::errs() << "Unknown unary operator: "
                   << Node->getOpcodeStr(Node->getOpcode()) << "\n";
      llvm_unreachable("Unknown unary operator");
    }
    }

    UnopIRExpr *UE = new UnopIRExpr(Op, SE);
    ExprStack.push_back((IRExpr *)UE);
  }

  void VisitArraySubscriptExpr(ArraySubscriptExpr *Node) {
    auto *Arr = getExpr(Node->getBase());

    if (auto *ArrLval = dyn_cast<IRLvalExpr>(Arr)) {
      auto *Ind = getExpr(Node->getIdx());
      IRType ArrType = (Node->getBase()->getType())->getPointeeType();
      IndexIRExpr *IE = new IndexIRExpr(ArrLval, Ind, ArrType);
      ExprStack.push_back((IRExpr *)IE);
    } else {
      // Emit the whole subscript expression opaquely
      // VarRenamePrinterHelper will rename any tracked variables inside.
      delete Arr;
      ExprStack.push_back(new ASTLiteralIRExpr(Node));
    }
  }

  void VisitCallExpr(CallExpr *Node) {
    // Calls to non-identifier-named functions (operators, destructors, etc.)
    // cannot be represented in the IR and are emitted opaquely.
    auto *DirectCallee = Node->getDirectCallee();
    if (DirectCallee && !DirectCallee->getDeclName().isIdentifier()) {
      ExprStack.push_back(new ASTLiteralIRExpr(Node));
      return;
    }

    CallCtx = true;
    auto *FnExpr = getExpr(Node->getCallee());
    CallCtx = false;

    std::vector<IRExpr *> Args;
    for (auto *ArgE : Node->arguments()) {
      Args.push_back(getExpr(ArgE));
    }

    if (auto *FnIdentE = dyn_cast<FIdentIRExpr>(FnExpr)) {
      IRFunRef FnIdent = FnIdentE->FR;
      delete FnIdentE;
      CallIRExpr *CE = new CallIRExpr(FnIdent, Args);
      ExprStack.push_back((IRExpr *)CE);
    } else {
      llvm::errs()
          << "Unsupported: non function identifier used for call expression.";
      llvm_unreachable("Non function identifier used for call expression.");
    }

    // the first function has cilk_spawn but the second doesn't
    // (implicit cilk spawn, so we are adding a sync)
    if (!SpawnCtx && Node->getDirectCallee() &&
        Tasks.find(toDefinition(Node->getDirectCallee())) != Tasks.end()) {
      auto *SpawnedE = ExprStack.back();
      ExprStack.pop_back();
      HandleCilkSpawn(SpawnedE);
      SyncNext = true;
    }
  }

  void VisitConditionalOperator(ConditionalOperator *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitLambdaExpr(LambdaExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitCXXNewExpr(CXXNewExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitCXXDeleteExpr(CXXDeleteExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitExprWithCleanups(ExprWithCleanups *Node) {
    ExprStack.push_back(getExpr(Node->getSubExpr()));
  }

  void VisitMaterializeTemporaryExpr(MaterializeTemporaryExpr *Node) {
    ExprStack.push_back(getExpr(Node->getSubExpr()));
  }

  void VisitCStyleCastExpr(CStyleCastExpr *Node) {
    auto *E = getExpr(Node->getSubExpr());
    auto *CastE = new CastIRExpr(Node->getType(), E);
    ExprStack.push_back(CastE);
  }

  void VisitCilkSpawnExpr(CilkSpawnExpr *Node) {
    SpawnCtx = true;
    auto *SpawnE = getExpr(Node->getSpawnedExpr());
    SpawnCtx = false;
    HandleCilkSpawn(SpawnE);
  }

  void VisitDeclRefExpr(DeclRefExpr *DRE) {
    ASTVarRef VR = DRE->getDecl();
    // printf("looking for %p\n", VR);
    // for (auto it = VarLookup.begin(); it != VarLookup.end(); it++) {
    //   auto &[k, v] = *it;
    //   printf("%p -> %s\n", k, v->Name.c_str());
    // }
    if (VarLookup.find(VR) != VarLookup.end()) {
      auto *IS = new IdentIRExpr(VarLookup[VR]);
      ExprStack.push_back((IRExpr *)IS);
    } else if (CallCtx) {
      IRFunRef FR(VR);
      auto *FS = new FIdentIRExpr(FR);
      ExprStack.push_back((IRExpr *)FS);
    } else {
      // TODO put a warning here instead
      // llvm::errs() << "Could not find variable: " << VR->getDeclName();
      // abort();
      ExprStack.push_back(new ASTLiteralIRExpr(DRE));
    }
  }
};

// Collects direct call targets in a function body that are not
// Tasks/TaskCallers and have a definition — these are candidates for inlining
// into task IR.
class InlinableCollector
    : public clang::RecursiveASTVisitor<InlinableCollector> {
  std::set<FunctionDecl *> const &Tasks;
  std::set<FunctionDecl *> const &TaskCallers;

public:
  std::vector<FunctionDecl *> Found;

  InlinableCollector(std::set<FunctionDecl *> const &Tasks,
                     std::set<FunctionDecl *> const &TaskCallers)
      : Tasks(Tasks), TaskCallers(TaskCallers) {}

  bool VisitCallExpr(CallExpr *CE) {
    auto *Callee = CE->getDirectCallee();
    if (!Callee || !Callee->getBody())
      return true;
    if (!Callee->getDeclName().isIdentifier())
      return true;
    Callee = toDefinition(Callee);
    if (Tasks.count(Callee) || TaskCallers.count(Callee))
      return true;
    if (GIgnoreFns.count(Callee->getName().str()))
      return true;
    Found.push_back(Callee);
    return true;
  }
};

class Cilk2IRVisitor : public clang::RecursiveASTVisitor<Cilk2IRVisitor> {
private:
  clang::ASTContext *Context;
  IRProgram &P;
  std::set<FunctionDecl *> &Tasks;
  std::set<FunctionDecl *> &TaskCallers;
  std::unordered_map<FunctionDecl *, std::vector<WhileStmt *>> &WhileWithSync;
  std::set<FunctionDecl *> &InlinableFns;

public:
  FunLookupTy FunLookup;
  explicit Cilk2IRVisitor(
      clang::ASTContext *Context, IRProgram &P, std::set<FunctionDecl *> &Tasks,
      std::set<FunctionDecl *> &TaskCallers,
      std::unordered_map<FunctionDecl *, std::vector<WhileStmt *>>
          &WhileWithSync,
      std::set<FunctionDecl *> &InlinableFns)
      : Context(Context), P(P), Tasks(Tasks), TaskCallers(TaskCallers),
        WhileWithSync(WhileWithSync), InlinableFns(InlinableFns) {}

  bool VisitFunctionDecl(clang::FunctionDecl *Decl) {
    if (Tasks.find(Decl) == Tasks.end() &&
        TaskCallers.find(Decl) == TaskCallers.end() &&
        InlinableFns.find(Decl) == InlinableFns.end()) {
      return true;
    }
    // Only process the actual definition, not a forward declaration.
    if (Decl->doesThisDeclarationHaveABody()) {
      GSymTable.DupCnt.clear();
      GSymTable.DupCnt["k"] = 0;
      IRFunction *F =
          P.createFunc(Decl->getName().str(), Decl->getDeclaredReturnType());
      F->Info.RootFun = Decl;
      std::unordered_map<ASTVarRef, IRVarRef> VarLookup;
      for (auto *Param : Decl->parameters()) {
        Sym PSym = PutSym(Param->getName().str());
        auto OrigTy = Param->getOriginalType();
        F->Vars.push_back(IRVarDecl{
            .Type =
                OrigTy->getAs<clang::TypedefType>() ? OrigTy : Param->getType(),
            .Name = PSym,
            .DeclLoc = IRVarDecl::ARG,
            .ASTDecl = Param,
        });
        VarLookup[Param] = &F->Vars.back();
      }
      Stmt2IRVisitor sv(F, VarLookup, Tasks);
      sv.Visit(Decl->getBody());

      // Register all redeclarations (forward decls + definition) so that
      // FinalizeVisitor can find the IRFunction regardless of which decl
      // pointer appears at a call/spawn site.
      for (auto *RD : Decl->redecls())
        FunLookup[RD] = F;
    } else if (Tasks.find(Decl) != Tasks.end() && !Decl->getBody()) {
      PANIC("unsupported: forward declaration of task function with no "
            "definition");
    }
    return true;
  }

  bool VisitRecordDecl(clang::RecordDecl *RD) {
    if (RD->isThisDeclarationADefinition()) {
      GRecordDecls.push_back(RD);
    }
    return true;
  }
};

class FinalizeVisitor : public IRExprVisitor<FinalizeVisitor> {
  FunLookupTy &FunLookup;

public:
  FinalizeVisitor(FunLookupTy &FunLookup) : FunLookup(FunLookup) {}

  void VisitISpawn(ISpawnIRExpr *Node) {
    if (auto *VarRef = std::get_if<ASTVarRef>(&Node->Fn)) {
      auto *FR = dyn_cast<FunctionDecl>(*VarRef);
      assert(FR);
      if (FunLookup.find(FR) != FunLookup.end()) {
        auto *SpawnDest = FunLookup[FR];
        Node->Fn = SpawnDest;
        SpawnDest->Info.IsTask = true;
      }
    } else {
      llvm_unreachable("Expected an AST variable reference in ISpawnIRExpr");
    }
  }

  void VisitCall(CallIRExpr *Node) {
    if (auto *VarRef = std::get_if<ASTVarRef>(&Node->Fn)) {
      if (auto *FR = dyn_cast<FunctionDecl>(*VarRef)) {
        auto It = FunLookup.find(FR);
        if (It != FunLookup.end())
          Node->Fn = It->second;
      }
    }
    for (auto &Arg : Node->Args)
      Visit(Arg.get());
  }
};

void finalizeFunction(IRFunction *F, FunLookupTy &FunLookup) {
  FinalizeVisitor FV(FunLookup);
  for (auto &B : *F) {
    for (auto &S : *B.get()) {
      FV.VisitStmt(S.get());
    }
    if (B->Term && isa<ReturnIRStmt>(B->Term)) {
      B->Succs.clear();
      //  B->Succs.insert(F->Exit);
    }
  }
}

void OpenCilk2IR(IRProgram &P, clang::ASTContext *Context, SourceManager &SM,
                 DriverCallersTy &DriverCallers) {
  CilkAnalyzeVisitor AVisitor;

  // Pass 1: identify Tasks and TaskCallers
  auto Decls = Context->getTranslationUnitDecl()->decls();
  for (auto &Decl : Decls) {
    if (!SM.isInMainFile(Decl->getLocation()))
      continue;
    AVisitor.TraverseDecl(Decl);
  }

  DBG {
    llvm::outs() << "tasks\n";
    for (auto *Task : AVisitor.Tasks) {
      llvm::outs() << Task->getName() << "\n";
    }
    llvm::outs() << "taskcallers\n";
    for (auto *Task : AVisitor.TaskCallers) {
      llvm::outs() << Task->getName() << "\n";
    }
  }

  // Build DriverCallers: scan IGNORE'd functions for calls to
  // Tasks/TaskCallers.
  {
    struct Scanner : clang::RecursiveASTVisitor<Scanner> {
      const std::set<FunctionDecl *> &Tasks;
      const std::set<FunctionDecl *> &TaskCallers;
      DriverCallersTy &Result;
      const clang::FunctionDecl *CallerFD;
      Scanner(const std::set<FunctionDecl *> &T,
              const std::set<FunctionDecl *> &TC, DriverCallersTy &R,
              const clang::FunctionDecl *C)
          : Tasks(T), TaskCallers(TC), Result(R), CallerFD(C) {}
      bool VisitCallExpr(clang::CallExpr *CE) {
        auto *Callee = CE->getDirectCallee();
        if (!Callee)
          return true;
        auto *Def = toDefinition(Callee);
        if (Tasks.count(Def) || TaskCallers.count(Def))
          Result[Def->getCanonicalDecl()] = CallerFD->getCanonicalDecl();
        return true;
      }
    };
    for (auto &D : Decls) {
      auto *FD = clang::dyn_cast<clang::FunctionDecl>(D);
      if (!FD || !FD->hasBody() || !SM.isInMainFile(FD->getLocation()))
        continue;
      if (!FD->getDeclName().isIdentifier() ||
          !GIgnoreFns.count(FD->getName().str()))
        continue;
      auto *Def = toDefinition(FD);
      Scanner sc(AVisitor.Tasks, AVisitor.TaskCallers, DriverCallers, Def);
      sc.TraverseStmt(Def->getBody());
    }
  }

  // Pass 2: transitively collect inlinable functions — functions called
  // (not spawned) from within Tasks/TaskCallers that need to appear in the IR.
  std::set<FunctionDecl *> InlinableFns;
  {
    std::vector<FunctionDecl *> Worklist;
    std::set<FunctionDecl *> Seen;
    auto enqueue = [&](FunctionDecl *FD) {
      if (Seen.insert(FD).second)
        Worklist.push_back(FD);
    };
    for (auto *F : AVisitor.Tasks)
      enqueue(F);
    for (auto *F : AVisitor.TaskCallers)
      enqueue(F);

    while (!Worklist.empty()) {
      FunctionDecl *FD = Worklist.back();
      Worklist.pop_back();
      if (!FD->getBody())
        continue;
      InlinableCollector IC(AVisitor.Tasks, AVisitor.TaskCallers);
      IC.TraverseDecl(FD);
      for (auto *Callee : IC.Found) {
        if (InlinableFns.insert(Callee).second)
          enqueue(Callee);
      }
    }
  }

  DBG {
    llvm::outs() << "inlinables\n";
    for (auto *F : InlinableFns)
      llvm::outs() << F->getName() << "\n";
  }

  // reserve all real local variable names so that PutSym's never collides with
  // an existing name
  {
    struct VarNameCollector : clang::RecursiveASTVisitor<VarNameCollector> {
      bool VisitVarDecl(clang::VarDecl *VD) {
        ReserveName(VD->getName().str());
        return true;
      }
    } VNC;
    for (auto *FD : AVisitor.Tasks) {
      for (auto *P : FD->parameters())
        ReserveName(P->getName().str());
      if (FD->getBody())
        VNC.TraverseStmt(FD->getBody());
    }
    for (auto *FD : AVisitor.TaskCallers) {
      for (auto *P : FD->parameters())
        ReserveName(P->getName().str());
      if (FD->getBody())
        VNC.TraverseStmt(FD->getBody());
    }
    for (auto *FD : InlinableFns) {
      for (auto *P : FD->parameters())
        ReserveName(P->getName().str());
      if (FD->getBody())
        VNC.TraverseStmt(FD->getBody());
    }
  }

  // Pass 3: convert Tasks, TaskCallers, and InlinableFns to IR
  Cilk2IRVisitor Visitor(Context, P, AVisitor.Tasks, AVisitor.TaskCallers,
                         AVisitor.WhileWithSync, InlinableFns);
  for (auto &Decl : Decls) {
    if (!SM.isInMainFile(Decl->getLocation()))
      continue;
    Visitor.TraverseDecl(Decl);
  }

  for (auto &F : P) {
    finalizeFunction(F.get(), Visitor.FunLookup);
  }
}
