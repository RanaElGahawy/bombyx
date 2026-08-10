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

// Collect the variables *read* (upward-exposed uses, lhs=false) by a single
// statement into Reads, and — if the statement is a scalar assignment — report
// the variable it *defines* (kills) via Def. Mirrors collectUsedVars' handling
// of the statements whose VisitStmt is unsafe or incomplete (ESpawn/Closure).
static void collectReadsAndDef(IRStmt *S, std::set<IRVarRef> &Reads,
                               IRVarRef &Def) {
  Def = nullptr;
  auto cb = [&](IRVarRef &VR, bool lhs) {
    if (!lhs)
      Reads.insert(VR);
  };
  if (auto *CDS = dyn_cast<ClosureDeclIRStmt>(S)) {
    if (CDS->SpawnCount)
      ExprIdentifierVisitor _(CDS->SpawnCount.get(), cb);
    for (auto &[CallerVar, _] : CDS->Caller2Callee)
      Reads.insert(CallerVar);
  } else if (auto *ES = dyn_cast<ESpawnIRStmt>(S)) {
    // The spawn-result destination is a write, but treating its idents as reads
    // only over-approximates liveness (keeps a var live), which is sound.
    if (ES->Dest)
      ExprIdentifierVisitor _(ES->Dest.get(), cb);
    for (auto &Arg : ES->Args)
      ExprIdentifierVisitor _(Arg.get(), cb);
  } else {
    // ExprIdentifierVisitor reports a CopyIRStmt's Dest with lhs=true, so it is
    // excluded from Reads by the callback; capture it as the killed var here.
    ExprIdentifierVisitor _(S, cb);
    if (auto *CS = dyn_cast<CopyIRStmt>(S))
      Def = CS->Dest;
  }
}

// Backward live-variable analysis. Returns the set of variables that are live
// on entry to F, i.e. read along some path from the entry block before being
// (re)defined. An ARG that is *not* in this set carries no meaningful incoming
// value: every use is preceded by a definition, so it can be localized.
static std::set<IRVarRef> computeLiveInEntry(IRFunction *F) {
  std::unordered_map<IRBasicBlock *, std::set<IRVarRef>> Gen, Kill, In, Out;

  for (auto &B : *F) {
    auto &gen = Gen[B.get()];
    auto &kill = Kill[B.get()];
    std::set<IRVarRef> defined; // defined so far scanning this block forward
    auto step = [&](IRStmt *S) {
      std::set<IRVarRef> reads;
      IRVarRef def = nullptr;
      collectReadsAndDef(S, reads, def);
      for (auto *r : reads)
        if (!defined.count(r))
          gen.insert(r); // upward-exposed use
      if (def) {
        kill.insert(def);
        defined.insert(def);
      }
    };
    for (auto &S : *B)
      step(S.get());
    if (B->Term) {
      // A null-valued ReturnIRStmt has no expression to visit.
      auto *RS = dyn_cast<ReturnIRStmt>(B->Term);
      if (!RS || RS->RetVal)
        step(B->Term);
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &B : *F) {
      std::set<IRVarRef> out;
      for (auto *Succ : B->Succs) {
        auto &si = In[Succ];
        out.insert(si.begin(), si.end());
      }
      std::set<IRVarRef> in = Gen[B.get()];
      for (auto *v : out)
        if (!Kill[B.get()].count(v))
          in.insert(v);
      if (in != In[B.get()] || out != Out[B.get()]) {
        In[B.get()] = std::move(in);
        Out[B.get()] = std::move(out);
        changed = true;
      }
    }
  }

  return In[F->getEntry()];
}

void PruneDeadSpawnArgs(IRProgram &P) {
  // Removing an ARG from a callee deletes the argument expressions that supplied
  // it in every caller, which can in turn make one of the caller's own ARGs dead
  // (e.g. a loop induction variable threaded around reentry -> continuation ->
  // reentry). Iterate to a fixpoint; each round strictly reduces the number of
  // ARGs, so this terminates.
  while (true) {
    // Step 1: for each task function compute the ARG vars that are dead on
    // entry — declared as ARGs but not live-in.
    std::unordered_map<IRFunction *, std::set<IRVarDecl *>> DeadByFn;

    for (auto &FPtr : P) {
      IRFunction *F = FPtr.get();
      if (!F->Info.IsTask)
        continue;
      std::set<IRVarRef> Live = computeLiveInEntry(F);
      std::set<IRVarDecl *> Dead;
      for (auto &Var : F->Vars)
        if (Var.DeclLoc == IRVarDecl::ARG && !Live.count(&Var))
          Dead.insert(&Var);
      if (!Dead.empty())
        DeadByFn[F] = std::move(Dead);
    }

    if (DeadByFn.empty())
      return;

    // Step 2: update every call site before touching the Vars list (the Dead
    // sets still hold valid pointers and DeclLocs are unchanged here, so the
    // ARG-prefix ordering the positional matching relies on is still intact).
    for (auto &FPtr : P) {
      for (auto &B : *FPtr) {
        for (auto &S : *B) {
          // ESpawnIRStmt: positional correspondence ES->Args[i] <-> i-th ARG.
          if (auto *ES = dyn_cast<ESpawnIRStmt>(S.get())) {
            auto It = DeadByFn.find(ES->Fn);
            if (It == DeadByFn.end())
              continue;
            auto &Dead = It->second;
            std::vector<std::unique_ptr<IRExpr>> NewArgs;
            auto ArgIt = ES->Args.begin();
            for (auto &Var : ES->Fn->Vars) {
              if (Var.DeclLoc != IRVarDecl::ARG)
                continue;
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

    // Step 3: for each dead ARG, either demote it to a function-local (if it is
    // still referenced in the body — its incoming value is dead but the name is
    // reused, e.g. a loop induction variable initialized before use) or erase it
    // entirely (if it is referenced nowhere). Demoted vars are spliced to the
    // end of the Vars list so the surviving ARGs remain a contiguous prefix,
    // which the positional arg emission downstream depends on.
    for (auto &[F, Dead] : DeadByFn) {
      std::set<IRVarDecl *> Used = collectUsedVars(F);
      for (auto It = F->Vars.begin(); It != F->Vars.end();) {
        if (It->DeclLoc == IRVarDecl::ARG && Dead.count(&*It)) {
          if (Used.count(&*It)) {
            It->DeclLoc = IRVarDecl::LOCAL;
            auto Node = It++;
            F->Vars.splice(F->Vars.end(), F->Vars, Node);
          } else {
            It = F->Vars.erase(It);
          }
        } else {
          ++It;
        }
      }
    }
  }
}
