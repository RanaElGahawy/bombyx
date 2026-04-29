#include <set>
#include <unordered_map>

#include "FlattenIR.hpp"
#include "IR.hpp"
#include "util.hpp"
#include "clang/AST/Expr.h"

using namespace llvm;

//////////////////////////////////////
// Restructure Loops with Syncs    //
////////////////////////////////////

static IRStmt *buildTailSpawnStmt(IRFunction *DstF,
                                  const std::vector<IRVarRef> &CallerVars) {
  std::vector<IRExpr *> Args;

  for (auto &DstV : DstF->Vars) {
    if (DstV.DeclLoc != IRVarDecl::ARG)
      continue;

    IRVarRef Match = nullptr;
    for (auto *CV : CallerVars) {
      if (CV->Name == DstV.Name && CV->Type == DstV.Type) {
        Match = CV;
        break;
      }
    }

    assert(Match && "Missing argument when building tail spawn");
    Args.push_back(new IdentIRExpr(Match));
  }

  auto *Spawn = new ISpawnIRExpr(DstF, Args);

  return new ExprWrapIRStmt(Spawn);
}

// Check if any block reachable from Entry (within the loop body, i.e. not
// crossing LoopHeader or AfterB) contains a SyncIRStmt terminator.
static bool loopBodyContainsSync(IRBasicBlock *BodyEntry,
                                 IRBasicBlock *LoopHeader,
                                 IRBasicBlock *AfterB) {
  std::vector<IRBasicBlock *> WL;
  std::set<IRBasicBlock *> Seen;
  WL.push_back(BodyEntry);
  while (!WL.empty()) {
    auto *B = WL.back();
    WL.pop_back();
    if (B == LoopHeader || B == AfterB || Seen.count(B))
      continue;
    Seen.insert(B);
    if (B->Term && isa<SyncIRStmt>(B->Term))
      return true;
    for (auto *S : B->Succs)
      WL.push_back(S);
  }
  return false;
}

// Collect all blocks reachable from Start, stopping at boundaries.
// Only includes blocks that belong to the specified parent function.
static void collectBlocks(IRBasicBlock *Start,
                          const std::set<IRBasicBlock *> &Boundaries,
                          std::vector<IRBasicBlock *> &Out,
                          std::set<IRBasicBlock *> &Seen,
                          IRFunction *ParentF = nullptr) {
  std::vector<IRBasicBlock *> WL;
  WL.push_back(Start);
  while (!WL.empty()) {
    auto *B = WL.back();
    WL.pop_back();
    if (Boundaries.count(B) || Seen.count(B))
      continue;
    // Skip blocks that have been moved to a different function
    if (ParentF && B->getParent() != ParentF)
      continue;
    Seen.insert(B);
    Out.push_back(B);
    for (auto *S : B->Succs)
      WL.push_back(S);
  }
}

// Collect all IRVarRef referenced in a set of blocks.
static void collectReferencedVars(const std::vector<IRBasicBlock *> &Blocks,
                                  std::set<IRVarRef> &Vars) {
  for (auto *B : Blocks) {
    auto Visitor = [&](auto &VR, bool lhs) { Vars.insert(VR); };
    for (auto &S : *B) {
      ExprIdentifierVisitor _(S.get(), Visitor);
    }
    if (B->Term) {
      ExprIdentifierVisitor _(B->Term, Visitor);
      // For LoopIRStmt, also collect vars from Init and Inc
      if (auto *LS = dyn_cast<LoopIRStmt>(B->Term)) {
        if (LS->Init) {
          ExprIdentifierVisitor _2(LS->Init, Visitor);
        }
        if (LS->Inc) {
          ExprIdentifierVisitor _3(LS->Inc, Visitor);
        }
      }
    }
  }
}

// Create a new task function with the given vars as ARGs.
// Returns the function and a remap from old vars to new vars.
static IRFunction *
createTaskFunction(IRProgram *P, const std::string &Name, IRType RetTy,
                   const std::vector<IRVarRef> &ArgVars,
                   std::unordered_map<IRVarRef, IRVarRef> &Remap) {
  IRFunction *F = P->createFunc(Name, RetTy);
  F->Info.IsTask = true;
  for (auto *OldVR : ArgVars) {
    F->Vars.push_back(IRVarDecl{
        .Type = OldVR->Type,
        .Name = OldVR->Name,
        .DeclLoc = IRVarDecl::ARG,
    });
    Remap[OldVR] = &F->Vars.back();
  }
  return F;
}

// Remap all var references in a block using the given map.
static void remapBlock(IRBasicBlock *B,
                       std::unordered_map<IRVarRef, IRVarRef> &Remap) {
  auto CB = [&](auto &VR, bool lhs) {
    if (Remap.find(VR) != Remap.end())
      VR = Remap[VR];
  };
  for (auto &S : *B) {
    ExprIdentifierVisitor e(S.get(), CB);
  }
  if (B->Term) {
    ExprIdentifierVisitor e(B->Term, CB);
    // For LoopIRStmt (for loops), also remap Init and Inc statements
    // which are stored as separate IRStmt* fields inside the terminator.
    if (auto *LS = dyn_cast<LoopIRStmt>(B->Term)) {
      if (LS->Init) {
        ExprIdentifierVisitor e2(LS->Init, CB);
      }
      if (LS->Inc) {
        ExprIdentifierVisitor e3(LS->Inc, CB);
      }
    }
  }
}

// Move blocks from SrcF to DstF and remap vars.
static void moveAndRemapBlocks(std::vector<IRBasicBlock *> &Blocks,
                               IRFunction *SrcF, IRFunction *DstF,
                               std::unordered_map<IRVarRef, IRVarRef> &Remap) {
  for (auto *B : Blocks) {
    SrcF->moveBlock(B, DstF);
    remapBlock(B, Remap);
  }
}

// Build an ISpawnIRExpr targeting DstF, passing all of DstF's ARG vars
// using references from the caller's var space (via reverse remap or
// direct match by name).
static IRStmt *buildSpawnStmt(IRFunction *DstF,
                              const std::vector<IRVarRef> &CallerVars) {
  std::vector<IRExpr *> Args;
  for (auto &DstV : DstF->Vars) {
    if (DstV.DeclLoc != IRVarDecl::ARG)
      continue;
    // Find matching var in caller by name+type
    bool found = false;
    for (auto *CV : CallerVars) {
      if (CV->Name == DstV.Name && CV->Type == DstV.Type) {
        Args.push_back(new IdentIRExpr(CV));
        found = true;
        break;
      }
    }
    // if (!found) {
    //   // Fallback: use zero
    //   Args.push_back(new IntLiteralIRExpr(0));
    // }
  }
  auto *Spawn = new ISpawnIRExpr(DstF, Args);
  return new ExprWrapIRStmt(Spawn);
}

// Get all vars from a function as a vector (for use as caller vars).
static std::vector<IRVarRef> getAllVars(IRFunction *F) {
  std::vector<IRVarRef> Result;
  for (auto &V : F->Vars) {
    Result.push_back(&V);
  }
  return Result;
}

// Get all ARG vars from a function as a vector.
static std::vector<IRVarRef> getArgVars(IRFunction *F) {
  std::vector<IRVarRef> Result;
  for (auto &V : F->Vars) {
    if (V.DeclLoc == IRVarDecl::ARG)
      Result.push_back(&V);
  }
  return Result;
}

// Determine which vars from ParentF are needed by a set of blocks,
// preserving the order they appear in ParentF->Vars.
static std::vector<IRVarRef>
computeNeededVars(IRFunction *ParentF,
                  const std::vector<IRBasicBlock *> &Blocks) {
  std::set<IRVarRef> Referenced;
  collectReferencedVars(Blocks, Referenced);

  std::vector<IRVarRef> Result;
  for (auto &V : ParentF->Vars) {
    if (Referenced.count(&V)) {
      Result.push_back(&V);
    }
  }
  return Result;
}

// Find blocks that form the "after loop" region: blocks reachable from
// AfterB that are in the same function and not part of the loop.
// Also stops at other loop headers to avoid consuming blocks that
// belong to an outer loop's body.
static std::vector<IRBasicBlock *>
collectAfterLoopBlocks(IRBasicBlock *AfterB, IRBasicBlock *LoopHeader,
                       IRFunction *F) {
  std::set<IRBasicBlock *> Boundaries;
  Boundaries.insert(LoopHeader);

  std::vector<IRBasicBlock *> Result;
  std::set<IRBasicBlock *> Seen;
  collectBlocks(AfterB, Boundaries, Result, Seen, F);
  return Result;
}

// The main restructuring pass for a single function.
// Finds the OUTERMOST loop with sync and restructures it.
// Returns a list of newly created functions that also need processing.
// Only processes ONE loop per call — the caller iterates until no more remain.
//
// We process OUTERMOST first so that:
// 1. The outer while's body is moved into fn_reentry BEFORE inner loops
//    are restructured; subsequent iterations of the worklist then see those
//    inner loops inside fn_reentry and handle them there.
// 2. Each loop's body lives in exactly one place (its own fn_reentry),
//    so no code is duplicated and inner loops are restructured exactly
//    once, in the function that now owns them.
static std::vector<IRFunction *> restructureLoopsWithSync(IRFunction &F) {
  static int LoopCounter = 0;
  std::vector<IRFunction *> NewFunctions;

  // Find the FIRST (outermost) loop-with-sync in F.
  IRBasicBlock *TargetHeader = nullptr;
  for (auto &B : F) {
    if (B->Term && isa<LoopIRStmt>(B->Term)) {
      auto *BodyB = B->Succs[0];
      auto *AfterB = B->Succs[1];
      if (loopBodyContainsSync(BodyB, B.get(), AfterB)) {
        TargetHeader = B.get();
        break;
      }
    }
  }

  if (!TargetHeader)
    return NewFunctions;

  auto *LoopHeader = TargetHeader;
  auto *BodyB = LoopHeader->Succs[0];
  auto *AfterB = LoopHeader->Succs[1];

  if (!loopBodyContainsSync(BodyB, LoopHeader, AfterB))
    return NewFunctions;

  INFO {
    llvm::outs() << "Restructuring loop in " << F.getName() << " at BB"
                 << LoopHeader->getInd() << "\n";
  }

  // === Step 1: Collect all vars used anywhere in the function ===
  // All vars that might be needed by the extracted functions.
  auto AllVars = getAllVars(&F);

  // === Step 2: Create fn_exit (post-loop code) ===
  auto AfterBlocks = collectAfterLoopBlocks(AfterB, LoopHeader, &F);

  std::unordered_map<IRVarRef, IRVarRef> ExitRemap;
  auto ExitNeeded = getAllVars(&F);
  std::string ExitName = F.getName() + "_exit" + std::to_string(LoopCounter);
  auto *ExitF = createTaskFunction(F.getParent(), ExitName, F.getReturnType(),
                                   ExitNeeded, ExitRemap);
  NewFunctions.push_back(ExitF);

  // Move after-loop blocks to ExitF
  moveAndRemapBlocks(AfterBlocks, &F, ExitF, ExitRemap);

  // === Step 3: Create fn_loop_reentry (loop re-entry) ===
  // fn_loop_reentry checks the condition and runs ALL iterations of
  // the loop.
  //
  // We need ALL vars from the parent function because the loop body
  // and all continuations may need them.
  std::unordered_map<IRVarRef, IRVarRef> ReentryRemap;
  std::string ReentryName =
      F.getName() + "_reentry" + std::to_string(LoopCounter);
  auto *ReentryF = createTaskFunction(F.getParent(), ReentryName,
                                      F.getReturnType(), AllVars, ReentryRemap);
  NewFunctions.push_back(ReentryF);

  auto *LoopTerm = dyn_cast<LoopIRStmt>(LoopHeader->Term);
  assert(LoopTerm);
  auto *ReentryCondExpr = LoopTerm->Cond.release();
  ExprIdentifierVisitor _rc(ReentryCondExpr, [&](auto &VR, bool lhs) {
    if (ReentryRemap.find(VR) != ReentryRemap.end())
      VR = ReentryRemap[VR];
  });

  auto *ReentryEntry = ReentryF->createBlock();
  std::vector<IRBasicBlock *> OrigBodyBlocks;
  {
    std::set<IRBasicBlock *> Boundaries;
    Boundaries.insert(LoopHeader);
    Boundaries.insert(AfterB);
    std::set<IRBasicBlock *> Seen;
    collectBlocks(BodyB, Boundaries, OrigBodyBlocks, Seen, &F);
  }

  moveAndRemapBlocks(OrigBodyBlocks, &F, ReentryF, ReentryRemap);

  auto *ReentryIf = new IfIRStmt(ReentryCondExpr);
  ReentryEntry->Term = (IRTerminatorStmt *)ReentryIf;
  ReentryEntry->Succs.insert(BodyB);

  auto ReentryArgVars = getArgVars(ReentryF);
  std::set<IRBasicBlock *> HandledBackEdges;
  for (auto *B : OrigBodyBlocks) {
    if (B->Succs.contains(LoopHeader)) {
      B->Succs.remove(LoopHeader);
      B->pushStmtBack(buildTailSpawnStmt(ReentryF, ReentryArgVars));
      if (!B->Term) {
        B->Term = new ReturnIRStmt(nullptr);
      }
      HandledBackEdges.insert(B);
    }
  }
  for (auto *B : OrigBodyBlocks) {
    if (HandledBackEdges.count(B))
      continue;
    if (!B->Succs.empty())
      continue;
    if (B->Term) {
      auto *RS = dyn_cast<ReturnIRStmt>(B->Term);
      if (!RS)
        continue;
      if (RS->RetVal)
        continue;
      delete B->Term;
      B->Term = nullptr;
    }
    B->pushStmtBack(buildTailSpawnStmt(ReentryF, ReentryArgVars));
    B->Term = new ReturnIRStmt(nullptr);
  }

  // "else" successor of ReentryEntry: spawn ExitF to leave the loop.
  auto *ReentryElseB = ReentryF->createBlock();
  ReentryEntry->Succs.insert(ReentryElseB);
  {
    ReentryElseB->pushStmtBack(buildTailSpawnStmt(ExitF, ReentryArgVars));
    ReentryElseB->Term = new ReturnIRStmt(nullptr);
  }

  // === Step 4: Replace LoopHeader in F with a stub that spawns ReentryF
  while (!LoopHeader->Succs.empty()) {
    LoopHeader->Succs.remove(*LoopHeader->Succs.begin());
  }
  delete LoopHeader->Term;
  LoopHeader->Term = nullptr;
  {
    auto FVars = getAllVars(&F);
    LoopHeader->pushStmtBack(buildTailSpawnStmt(ReentryF, FVars));
    LoopHeader->Term = new ReturnIRStmt(nullptr);
  }

  {
    std::vector<IRBasicBlock *> Snapshot;
    for (auto &Bp : F) {
      Snapshot.push_back(Bp.get());
    }
    for (auto *B : Snapshot) {
      std::vector<IRBasicBlock *> Stale;
      for (auto *Succ : B->Succs) {
        if (Succ->getParent() != &F) {
          Stale.push_back(Succ);
        }
      }
      for (auto *S : Stale) {
        IRFunction *DstF = S->getParent();
        auto FVars = getAllVars(&F);

        IRBasicBlock *Trampoline = F.createBlock();
        Trampoline->pushStmtBack(buildTailSpawnStmt(DstF, FVars));
        Trampoline->Term = new ReturnIRStmt(nullptr);

        size_t slot = 0;
        bool found = false;
        for (auto *Succ : B->Succs) {
          if (Succ == S) {
            found = true;
            break;
          }
          slot++;
        }
        assert(found);

        if (slot == 0 && B->Succs.size() == 2) {
          IRBasicBlock *Other = B->Succs[1];
          B->Succs.clear();
          B->Succs.insert(Trampoline);
          B->Succs.insert(Other);
        } else {
          B->Succs.remove(S);
          B->Succs.insert(Trampoline);
        }
      }
    }
  }

  LoopCounter++;
  return NewFunctions;
}

//////////////////////////////////////
// Restructure Ifs with Syncs      //
////////////////////////////////////

// Compute the set of blocks reachable from Start within F (forward walk),
// excluding Start itself.
static void forwardReachable(IRBasicBlock *Start, IRFunction *F,
                             std::set<IRBasicBlock *> &Out) {
  std::vector<IRBasicBlock *> WL;
  for (auto *S : Start->Succs) {
    WL.push_back(S);
  }
  while (!WL.empty()) {
    auto *B = WL.back();
    WL.pop_back();
    if (Out.count(B))
      continue;
    if (B->getParent() != F)
      continue;
    Out.insert(B);
    for (auto *S : B->Succs)
      WL.push_back(S);
  }
}

static IRBasicBlock *findIfMerge(IRBasicBlock *IfHeader, IRFunction *F) {
  if (IfHeader->Succs.size() < 2)
    return nullptr;
  IRBasicBlock *ThenB = IfHeader->Succs[0];
  IRBasicBlock *ElseB = IfHeader->Succs[1];

  std::set<IRBasicBlock *> ThenReach;
  ThenReach.insert(ThenB);
  forwardReachable(ThenB, F, ThenReach);

  // BFS from ElseB, return the first block already in ThenReach.
  std::vector<IRBasicBlock *> WL;
  std::set<IRBasicBlock *> Seen;
  WL.push_back(ElseB);
  while (!WL.empty()) {
    auto *B = WL.back();
    WL.pop_back();
    if (Seen.count(B))
      continue;
    if (B->getParent() != F)
      continue;
    Seen.insert(B);
    if (ThenReach.count(B))
      return B;
    for (auto *S : B->Succs)
      WL.push_back(S);
  }
  return nullptr;
}

static bool branchContainsSync(IRBasicBlock *Start, IRBasicBlock *Stop,
                               IRFunction *F) {
  std::vector<IRBasicBlock *> WL;
  std::set<IRBasicBlock *> Seen;
  WL.push_back(Start);
  while (!WL.empty()) {
    auto *B = WL.back();
    WL.pop_back();
    if (B == Stop || Seen.count(B))
      continue;
    if (B->getParent() != F)
      continue;
    Seen.insert(B);
    if (B->Term && isa<SyncIRStmt>(B->Term))
      return true;
    for (auto *S : B->Succs)
      WL.push_back(S);
  }
  return false;
}

static void collectBranchBlocks(IRBasicBlock *Start, IRBasicBlock *Merge,
                                IRFunction *F,
                                std::vector<IRBasicBlock *> &Out) {
  std::set<IRBasicBlock *> Seen;
  std::vector<IRBasicBlock *> WL;
  WL.push_back(Start);
  while (!WL.empty()) {
    auto *B = WL.back();
    WL.pop_back();
    if (B == Merge || Seen.count(B))
      continue;
    if (B->getParent() != F)
      continue;
    Seen.insert(B);
    Out.push_back(B);
    for (auto *S : B->Succs)
      WL.push_back(S);
  }
}

static std::vector<IRBasicBlock *>
findBranchExitBlocks(const std::vector<IRBasicBlock *> &BranchBlocks,
                     IRBasicBlock *Merge) {
  std::vector<IRBasicBlock *> Result;
  for (auto *B : BranchBlocks) {
    if (B->Succs.contains(Merge)) {
      Result.push_back(B);
    }
  }
  return Result;
}

// Restructure the first if-with-sync found in F.
static std::vector<IRFunction *> restructureIfsWithSync(IRFunction &F) {
  static int IfCounter = 0;
  std::vector<IRFunction *> NewFunctions;

  // Step 1: find an if-with-sync.
  IRBasicBlock *IfHeader = nullptr;
  IRBasicBlock *Merge = nullptr;
  for (auto &B : F) {
    if (!(B->Term && isa<IfIRStmt>(B->Term)))
      continue;
    if (B->Succs.size() < 2)
      continue;
    IRBasicBlock *ThenB = B->Succs[0];
    IRBasicBlock *ElseB = B->Succs[1];

    IRBasicBlock *M = findIfMerge(B.get(), &F);
    if (!M)
      continue; // branches don't merge (both return); nothing to extract.

    bool thenHasSync = branchContainsSync(ThenB, M, &F);
    bool elseHasSync = branchContainsSync(ElseB, M, &F);
    if (!thenHasSync && !elseHasSync)
      continue;

    if (M == B.get())
      continue;

    IfHeader = B.get();
    Merge = M;
    break;
  }

  if (!IfHeader)
    return NewFunctions;

  INFO {
    llvm::outs() << "Restructuring if-with-sync in " << F.getName() << " at BB"
                 << IfHeader->getInd() << " (merge=BB" << Merge->getInd()
                 << ")\n";
  }

  // Step 2: collect after-if blocks (merge and everything past it).
  std::vector<IRBasicBlock *> AfterIfBlocks;
  {
    std::set<IRBasicBlock *> Boundaries; // none; stop only at function boundary
    std::set<IRBasicBlock *> Seen;
    collectBlocks(Merge, Boundaries, AfterIfBlocks, Seen, &F);
  }

  // Step 3: find the exit points of each branch (blocks that currently
  // flow to Merge).
  std::vector<IRBasicBlock *> ThenBranchBlocks;
  std::vector<IRBasicBlock *> ElseBranchBlocks;
  collectBranchBlocks(IfHeader->Succs[0], Merge, &F, ThenBranchBlocks);
  collectBranchBlocks(IfHeader->Succs[1], Merge, &F, ElseBranchBlocks);

  auto ThenExits = findBranchExitBlocks(ThenBranchBlocks, Merge);
  auto ElseExits = findBranchExitBlocks(ElseBranchBlocks, Merge);

  // Special case: ThenB or ElseB is itself the merge (i.e. one branch
  // is "empty" and the if has no effective else body). Then IfHeader
  // itself is the "exit" for that branch. We need the IfHeader's Succs
  // updated directly.
  bool ThenIsMerge = (IfHeader->Succs[0] == Merge);
  bool ElseIsMerge = (IfHeader->Succs[1] == Merge);

  // Step 4: create fn_afterif_k with all of F's vars as args.
  auto AllVars = getAllVars(&F);
  std::unordered_map<IRVarRef, IRVarRef> AfterIfRemap;
  std::string AfterIfName =
      F.getName() + "_afterif" + std::to_string(IfCounter);
  auto *AfterIfF = createTaskFunction(F.getParent(), AfterIfName,
                                      F.getReturnType(), AllVars, AfterIfRemap);
  NewFunctions.push_back(AfterIfF);

  // Move after-if blocks to AfterIfF and remap vars.
  moveAndRemapBlocks(AfterIfBlocks, &F, AfterIfF, AfterIfRemap);

  // Step 5: redirect each branch's exit to spawn AfterIfF + return.
  auto FVars = getAllVars(&F);
  auto makeAfterIfStub = [&]() {
    auto *Stub = F.createBlock();
    Stub->pushStmtBack(buildTailSpawnStmt(AfterIfF, FVars));
    Stub->Term = new ReturnIRStmt(nullptr);
    return Stub;
  };

  auto redirectExit = [&](IRBasicBlock *B) {
    B->Succs.remove(Merge);

    if (B->Term && isa<SyncIRStmt>(B->Term)) {
      B->Succs.insert(makeAfterIfStub());
      return;
    }

    B->pushStmtBack(buildTailSpawnStmt(AfterIfF, FVars));
    if (!B->Term) {
      B->Term = new ReturnIRStmt(nullptr);
    }
  };

  for (auto *B : ThenExits)
    redirectExit(B);
  for (auto *B : ElseExits)
    redirectExit(B);

  auto redirectDeadEnds = [&](const std::vector<IRBasicBlock *> &BranchBlocks,
                              const std::vector<IRBasicBlock *> &Exits) {
    std::set<IRBasicBlock *> AlreadyHandled(Exits.begin(), Exits.end());
    for (auto *B : BranchBlocks) {
      // llvm::errs() << "  checking BB" << B->getInd()
      //              << " succs=" << B->Succs.size()
      //              << " term=" << (B->Term ? B->Term->getKind() : -1) <<
      //              "\n";
      if (AlreadyHandled.count(B))
        continue;
      if (!B->Succs.empty())
        continue;

      if (B->Term) {
        if (isa<SyncIRStmt>(B->Term)) {
          B->Succs.insert(makeAfterIfStub());
          continue;
        }

        auto *RS = dyn_cast<ReturnIRStmt>(B->Term);
        if (!RS)
          continue;
        if (RS->RetVal)
          continue;

        delete B->Term;
        B->Term = nullptr;
      }
      B->pushStmtBack(buildTailSpawnStmt(AfterIfF, FVars));
      B->Term = new ReturnIRStmt(nullptr);
    }
  };
  redirectDeadEnds(ThenBranchBlocks, ThenExits);
  redirectDeadEnds(ElseBranchBlocks, ElseExits);

  // Handle the "empty branch" case: branch target IS the merge.

  if (ThenIsMerge) {
    auto *StubB = F.createBlock();
    IRBasicBlock *ElseTarget = nullptr;
    for (auto *S : IfHeader->Succs) {
      if (S != Merge) {
        ElseTarget = S;
        break;
      }
    }
    while (!IfHeader->Succs.empty()) {
      IfHeader->Succs.remove(*IfHeader->Succs.begin());
    }
    IfHeader->Succs.insert(StubB);
    if (ElseTarget)
      IfHeader->Succs.insert(ElseTarget);
    StubB->pushStmtBack(buildTailSpawnStmt(AfterIfF, FVars));
    StubB->Term = new ReturnIRStmt(nullptr);
  }
  if (ElseIsMerge) {
    auto *StubB = F.createBlock();
    IfHeader->Succs.remove(Merge);
    IfHeader->Succs.insert(StubB);
    StubB->pushStmtBack(buildTailSpawnStmt(AfterIfF, FVars));
    StubB->Term = new ReturnIRStmt(nullptr);
  }

  // Step 6: clean up any stale successor edges in F.
  {
    std::vector<IRBasicBlock *> Snapshot;
    for (auto &Bp : F) {
      Snapshot.push_back(Bp.get());
    }
    for (auto *B : Snapshot) {
      std::vector<IRBasicBlock *> Stale;
      for (auto *Succ : B->Succs) {
        if (Succ->getParent() != &F)
          Stale.push_back(Succ);
      }
      for (auto *S : Stale) {
        IRFunction *DstF = S->getParent();
        auto FVarsLocal = getAllVars(&F);

        IRBasicBlock *Trampoline = F.createBlock();
        Trampoline->pushStmtBack(buildTailSpawnStmt(DstF, FVarsLocal));
        Trampoline->Term = new ReturnIRStmt(nullptr);

        size_t slot = 0;
        bool found = false;
        for (auto *Succ : B->Succs) {
          if (Succ == S) {
            found = true;
            break;
          }
          slot++;
        }
        assert(found);

        if (slot == 0 && B->Succs.size() == 2) {
          IRBasicBlock *Other = B->Succs[1];
          B->Succs.clear();
          B->Succs.insert(Trampoline);
          B->Succs.insert(Other);
        } else {
          B->Succs.remove(S);
          B->Succs.insert(Trampoline);
        }
      }
    }
  }

  IfCounter++;
  return NewFunctions;
}

// Redirect successor edges that point to blocks in a different function.
static void cleanStaleSuccessorEdges(IRProgram &P) {
  for (auto &F : P) {
    std::vector<IRBasicBlock *> Snapshot;
    for (auto &Bp : *F) {
      Snapshot.push_back(Bp.get());
    }
    for (auto *B : Snapshot) {
      std::vector<IRBasicBlock *> Stale;
      for (auto *Succ : B->Succs) {
        if (Succ->getParent() != F.get()) {
          Stale.push_back(Succ);
        }
      }
      for (auto *S : Stale) {
        IRFunction *DstF = S->getParent();
        auto FVars = getAllVars(F.get());

        IRBasicBlock *Trampoline = F->createBlock();
        Trampoline->pushStmtBack(buildTailSpawnStmt(DstF, FVars));
        Trampoline->Term = new ReturnIRStmt(nullptr);

        size_t slot = 0;
        bool found = false;
        for (auto *Succ : B->Succs) {
          if (Succ == S) {
            found = true;
            break;
          }
          slot++;
        }
        assert(found);

        if (slot == 0 && B->Succs.size() == 2) {
          IRBasicBlock *Other = B->Succs[1];
          B->Succs.clear();
          B->Succs.insert(Trampoline);
          B->Succs.insert(Other);
        } else {
          B->Succs.remove(S);
          B->Succs.insert(Trampoline);
        }
      }
    }
  }
}

void FlattenIR(IRProgram &P) {
  std::vector<IRFunction *> WorkList;
  for (auto &F : P) {
    WorkList.push_back(F.get());
  }

  while (!WorkList.empty()) {
    auto *F = WorkList.back();
    WorkList.pop_back();

    auto LoopNewFuncs = restructureLoopsWithSync(*F);
    if (!LoopNewFuncs.empty()) {
      WorkList.push_back(F);
      for (auto *NF : LoopNewFuncs)
        WorkList.push_back(NF);
      continue;
    }

    auto IfNewFuncs = restructureIfsWithSync(*F);
    if (!IfNewFuncs.empty()) {
      WorkList.push_back(F);
      for (auto *NF : IfNewFuncs)
        WorkList.push_back(NF);
      continue;
    }
  }

  cleanStaleSuccessorEdges(P);
}