#include "core/PruneDeadSpawnArgs.hpp"
#include "core/IR.hpp"

#include <set>
#include <unordered_map>
#include <vector>

// Collect every IRVarDecl* that is referenced by any IdentIRExpr anywhere in F.
// Uses the built-in IRExprVisitor to recurse into all expression sub-trees, and
// manually visits ClosureDeclIRStmt (not covered by VisitStmt) to pick up the
// source-side vars of Caller2Callee mappings.
namespace {
struct VarCollector : IRExprVisitor<VarCollector> {
  std::set<IRVarDecl *> Used;
  void VisitIdent(IdentIRExpr *Node) { Used.insert(Node->Ident); }
};
} // namespace

static std::set<IRVarDecl *> collectUsedVars(IRFunction *F) {
  VarCollector VC;
  auto visitStmt = [&](IRStmt *S) {
    if (auto *CDS = dyn_cast<ClosureDeclIRStmt>(S)) {
      if (CDS->SpawnCount)
        VC.Visit(CDS->SpawnCount.get());
      for (auto &[CallerVar, _] : CDS->Caller2Callee)
        VC.Used.insert(CallerVar);
    } else if (auto *ES = dyn_cast<ESpawnIRStmt>(S)) {
      // VisitStmt for ESpawn dereferences Dest without null-check; do it here.
      if (ES->Dest)
        VC.Visit(ES->Dest.get());
      for (auto &Arg : ES->Args)
        VC.Visit(Arg.get());
    } else {
      VC.VisitStmt(S);
    }
  };
  for (auto &B : *F) {
    for (auto &S : *B)
      visitStmt(S.get());
    if (B->Term)
      visitStmt(B->Term);
  }
  return VC.Used;
}

void PruneDeadSpawnArgs(IRProgram &P) {
  // Step 1: for each task function compute the set of dead ARG vars.
  std::unordered_map<IRFunction *, std::set<IRVarDecl *>> DeadByFn;

  for (auto &FPtr : P) {
    IRFunction *F = FPtr.get();
    if (!F->Info.IsTask)
      continue;
    std::set<IRVarDecl *> Used = collectUsedVars(F);
    std::set<IRVarDecl *> Dead;
    for (auto &Var : F->Vars)
      if (Var.DeclLoc == IRVarDecl::ARG && !Used.count(&Var))
        Dead.insert(&Var);
    if (!Dead.empty())
      DeadByFn[F] = std::move(Dead);
  }

  if (DeadByFn.empty())
    return;

  // Step 2: update every call site before touching the Vars list (the Dead
  // sets still hold valid pointers at this point).
  for (auto &FPtr : P) {
    for (auto &B : *FPtr) {
      for (auto &S : *B) {
        // ESpawnIRStmt: positional correspondence ES->Args[i] <-> i-th ARG var.
        if (auto *ES = dyn_cast<ESpawnIRStmt>(S.get())) {
          auto It = DeadByFn.find(ES->Fn);
          if (It == DeadByFn.end())
            continue;
          auto &Dead = It->second;
          std::vector<std::unique_ptr<IRExpr>> NewArgs;
          auto ArgIt = ES->Args.begin();
          for (auto &Var : ES->Fn->Vars) {
            if (Var.DeclLoc != IRVarDecl::ARG)
              break;
            assert(ArgIt != ES->Args.end());
            if (!Dead.count(&Var))
              NewArgs.push_back(std::move(*ArgIt));
            ++ArgIt;
          }
          ES->Args = std::move(NewArgs);
        }
        // ClosureDeclIRStmt: remove entries whose callee-side var is dead.
        if (auto *CDS = dyn_cast<ClosureDeclIRStmt>(S.get())) {
          auto It = DeadByFn.find(CDS->Fn);
          if (It == DeadByFn.end())
            continue;
          auto &Dead = It->second;
          for (auto MapIt = CDS->Caller2Callee.begin();
               MapIt != CDS->Caller2Callee.end();) {
            if (Dead.count(MapIt->second))
              MapIt = CDS->Caller2Callee.erase(MapIt);
            else
              ++MapIt;
          }
        }
      }
    }
  }

  // Step 3: erase dead ARG vars from each function's Vars list.
  // std::list iterators to other elements remain valid after erase.
  for (auto &[F, Dead] : DeadByFn) {
    for (auto It = F->Vars.begin(); It != F->Vars.end();) {
      if (It->DeclLoc == IRVarDecl::ARG && Dead.count(&*It))
        It = F->Vars.erase(It);
      else
        ++It;
    }
  }
}
