#include <clang/AST/ExprCilk.h>
#include <clang/AST/StmtCilk.h>
#include <llvm/ADT/SetVector.h>
#include <set>
#include <unordered_map>

#include "IR.hpp"
#include "util.hpp"
#include "clang/AST/Expr.h"

using namespace llvm;

///////////////////////////////////////////////
// Phase 0: Restructure Loops with Syncs    //
// (Pre-pass before CreateContinuationFuns) //
/////////////////////////////////////////////

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
    if (!found) {
      // Fallback: use zero
      Args.push_back(new IntLiteralIRExpr(0));
    }
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

// Find all blocks that would loop back to the LoopHeader.
// These are blocks in the loop body whose Succs contain LoopHeader.
static std::vector<IRBasicBlock *> findLoopBackBlocks(IRBasicBlock *BodyEntry,
                                                      IRBasicBlock *LoopHeader,
                                                      IRBasicBlock *AfterB,
                                                      IRFunction *F) {
  std::vector<IRBasicBlock *> AllBody;
  std::set<IRBasicBlock *> Seen;
  std::set<IRBasicBlock *> Boundaries;
  Boundaries.insert(LoopHeader);
  Boundaries.insert(AfterB);
  collectBlocks(BodyEntry, Boundaries, AllBody, Seen, F);

  std::vector<IRBasicBlock *> BackBlocks;
  for (auto *B : AllBody) {
    if (B->Succs.contains(LoopHeader)) {
      BackBlocks.push_back(B);
    }
  }
  return BackBlocks;
}

// The main restructuring pass for a single function.
// Finds the OUTERMOST loop with sync and restructures it.
// Returns a list of newly created functions that also need processing.
// Only processes ONE loop per call — the caller iterates until no more remain.
//
// We process OUTERMOST first so that:
// 1. The outer while's back-edge blocks are replaced with spawns to
//    fn_reentry BEFORE inner loops are restructured.
// 2. The cloned body in fn_reentry still has inner loops as LoopIRStmt,
//    which get restructured in subsequent iterations.
// 3. Both the original and cloned inner loops get their own independent
//    fn_exit/fn_reentry chains — this is correct since they run in
//    separate function contexts.
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
  auto ExitNeeded = computeNeededVars(&F, AfterBlocks);
  std::string ExitName = F.getName() + "_exit" + std::to_string(LoopCounter);
  auto *ExitF = createTaskFunction(F.getParent(), ExitName, F.getReturnType(),
                                   ExitNeeded, ExitRemap);
  NewFunctions.push_back(ExitF);

  // Move after-loop blocks to ExitF
  moveAndRemapBlocks(AfterBlocks, &F, ExitF, ExitRemap);

  // === Step 3: Create fn_loop_reentry (loop re-entry) ===
  // fn_loop_reentry re-checks the condition.
  // If true: re-enters the loop body (same structure as original).
  // If false: spawns fn_exit.
  //
  // We need ALL vars from the parent function because the loop body
  // and all continuations may need them.
  std::unordered_map<IRVarRef, IRVarRef> ReentryRemap;
  std::string ReentryName =
      F.getName() + "_reentry" + std::to_string(LoopCounter);
  auto *ReentryF = createTaskFunction(F.getParent(), ReentryName,
                                      F.getReturnType(), AllVars, ReentryRemap);
  NewFunctions.push_back(ReentryF);

  // Build the reentry function body: clone the loop condition check
  // and the entire loop body structure.
  auto *ReentryEntry = ReentryF->createBlock();

  // Clone the loop condition
  auto *LoopTerm = dyn_cast<LoopIRStmt>(LoopHeader->Term);
  assert(LoopTerm);
  auto *ReentryCondExpr = LoopTerm->Cond->clone();
  // Remap the condition to reentry vars
  ExprIdentifierVisitor _rc(ReentryCondExpr, [&](auto &VR, bool lhs) {
    if (ReentryRemap.find(VR) != ReentryRemap.end())
      VR = ReentryRemap[VR];
  });

  auto *ReentryIf = new IfIRStmt(ReentryCondExpr);
  ReentryEntry->Term = (IRTerminatorStmt *)ReentryIf;

  // "then" branch: clone the loop body blocks into ReentryF.
  // We need to clone (not move) because the original body stays in F
  // for the first iteration.
  auto *ReentryBodyEntry = ReentryF->createBlock();
  ReentryEntry->Succs.insert(ReentryBodyEntry);

  // Clone all loop body blocks
  std::vector<IRBasicBlock *> OrigBodyBlocks;
  {
    std::set<IRBasicBlock *> Boundaries;
    Boundaries.insert(LoopHeader);
    Boundaries.insert(AfterB);
    std::set<IRBasicBlock *> Seen;
    collectBlocks(BodyB, Boundaries, OrigBodyBlocks, Seen, &F);
  }

  // Create cloned blocks in ReentryF and set up mapping
  std::unordered_map<IRBasicBlock *, IRBasicBlock *> BlockCloneMap;
  BlockCloneMap[BodyB] = ReentryBodyEntry;

  for (auto *OB : OrigBodyBlocks) {
    if (OB == BodyB) {
      // BodyB maps to ReentryBodyEntry (already created)
      OB->clone(ReentryBodyEntry);
      continue;
    }
    auto *CB = ReentryF->createBlock();
    OB->clone(CB);
    BlockCloneMap[OB] = CB;
  }

  // Fix successors in cloned blocks
  for (auto *OB : OrigBodyBlocks) {
    auto *CB = BlockCloneMap[OB];
    for (auto *Succ : OB->Succs) {
      if (Succ == LoopHeader) {
        // Back-edge: don't add (will be replaced with spawn to ReentryF)
        continue;
      }
      if (BlockCloneMap.count(Succ)) {
        CB->Succs.insert(BlockCloneMap[Succ]);
      }
      // Succs pointing to AfterB will be handled below
    }
  }

  // Remap vars in cloned blocks
  for (auto *OB : OrigBodyBlocks) {
    auto *CB = BlockCloneMap[OB];
    remapBlock(CB, ReentryRemap);
  }

  // Handle back-edges in cloned blocks: replace with spawn to ReentryF
  for (auto *OB : OrigBodyBlocks) {
    if (OB->Succs.contains(LoopHeader)) {
      auto *CB = BlockCloneMap[OB];
      // Add spawn to ReentryF at the end of CB
      auto ReentryVars = getArgVars(ReentryF);
      CB->pushStmtBack(buildSpawnStmt(ReentryF, ReentryVars));
      if (!CB->Term) {
        CB->Term = new ReturnIRStmt(nullptr);
      }
    }
  }

  // "else" branch: spawn fn_exit
  auto *ReentryElseB = ReentryF->createBlock();
  ReentryEntry->Succs.insert(ReentryElseB);
  {
    auto ReentryVars = getArgVars(ReentryF);
    ReentryElseB->pushStmtBack(buildSpawnStmt(ExitF, ReentryVars));
    ReentryElseB->Term = new ReturnIRStmt(nullptr);
  }

  // === Step 4: Modify original function ===
  // Convert LoopIRStmt → IfIRStmt
  auto *OrigCond = LoopTerm->Cond.release();
  delete LoopHeader->Term;
  LoopHeader->Term = new IfIRStmt(OrigCond);

  // The "then" successor (BodyB) stays in F.
  // The "else" successor was AfterB, but AfterB has been moved to ExitF.
  // Replace with a new block that spawns fn_exit.
  LoopHeader->Succs.remove(AfterB);
  auto *OrigElseB = F.createBlock();
  LoopHeader->Succs.insert(OrigElseB);
  {
    auto FVars = getAllVars(&F);
    OrigElseB->pushStmtBack(buildSpawnStmt(ExitF, FVars));
    OrigElseB->Term = new ReturnIRStmt(nullptr);
  }

  // Replace back-edges in original body with spawns to fn_loop_reentry
  auto BackBlocks = findLoopBackBlocks(BodyB, LoopHeader, AfterB, &F);
  for (auto *BB : BackBlocks) {
    BB->Succs.remove(LoopHeader);
    auto FVars = getAllVars(&F);
    BB->pushStmtBack(buildSpawnStmt(ReentryF, FVars));
    if (!BB->Term) {
      BB->Term = new ReturnIRStmt(nullptr);
    }
  }

  // === Step 5: Clean up stale successor edges ===
  // After moving blocks to fn_exit/fn_reentry, some blocks in F
  // may still have successor edges pointing to moved blocks.
  // Remove these to prevent Phase 1 from following dangling edges.
  for (auto &B : F) {
    std::vector<IRBasicBlock *> ToRemove;
    for (auto *Succ : B->Succs) {
      if (Succ->getParent() != &F) {
        ToRemove.push_back(Succ);
      }
    }
    for (auto *S : ToRemove) {
      B->Succs.remove(S);
    }
  }

  LoopCounter++;
  return NewFunctions;
}

/////////////////////////////
// CreateContinuationFuns //
///////////////////////////

struct CreateContinuationFuns {
  struct ContFun {
    IRFunction *F;
    std::set<IRVarRef> Args;
    std::set<IRVarRef> Locals;
  };

  std::vector<SetVector<IRBasicBlock *>> Paths;
  std::unordered_map<IRBasicBlock *, int> PathLookup;
  std::vector<ContFun> ContFuns;

private:
  IRBasicBlock *duplicateBasicBlock(IRBasicBlock *B,
                                    SetVector<IRBasicBlock *> &CurrPath) {
    IRBasicBlock *CloneBB = B->getParent()->createBlock();
    B->clone(CloneBB);

    for (IRBasicBlock *Succ : B->Succs) {
      CloneBB->Succs.insert(Succ);
    }

    B->iteratePreds([&](IRBasicBlock *Pred) -> void {
      if (CurrPath.contains(Pred)) {
        Pred->Succs.remove(B);
        Pred->Succs.insert(CloneBB);
      }
    });
    return CloneBB;
  }

  void createSyncPaths(IRFunction &F) {
    Paths.resize(1);

    std::vector<std::pair<IRBasicBlock *, int>> WorkList;
    WorkList.push_back(std::make_pair(F.getEntry(), 0));

    int Fresh = 0;
    while (!WorkList.empty()) {
      auto [B, CurrLevel] = WorkList.back();
      WorkList.pop_back();

      if (PathLookup.find(B) != PathLookup.end()) {
        if (PathLookup[B] == CurrLevel) {
          continue;
        } else {
          B = duplicateBasicBlock(B, Paths[CurrLevel]);
        }
      }

      Paths[CurrLevel].insert(B);
      PathLookup.insert(std::make_pair(B, CurrLevel));
      INFO {
        if (B->Term && isa<SyncIRStmt>(B->Term)) {
          llvm::outs() << "Sync is terminator & Current level = " << CurrLevel
                       << "\n";
        } else if (B->Term && isa<LoopIRStmt>(B->Term)) {
          llvm::outs() << "Loop is terminator & Current level = " << CurrLevel
                       << "\n";
        }
      }
      if (B->Term && isa<SyncIRStmt>(B->Term)) {
        IRBasicBlock *SISucc = *(B->Succs.begin());
        if (PathLookup.find(SISucc) != PathLookup.end()) {
          continue;
        }
        Fresh++;
        Paths.resize(Fresh + 1);
        CurrLevel = Fresh;
      }

      for (auto *Succ : B->Succs) {
        WorkList.push_back(std::make_pair(Succ, CurrLevel));
      }
    }

    int i = 0;
    INFO {
      for (auto &Path : Paths) {
        llvm::outs() << "path " << i << ": ";
        for (auto &B : Path) {
          llvm::outs() << "BB" << B->getInd() << ", ";
        }
        llvm::outs() << "\n";
        i++;
      }
    }
  }

  void analyzeStmt(IRStmt *S, std::set<IRVarRef> &free,
                   std::set<IRVarRef> &refd) {
    std::set<IRVarRef> V;
    ExprIdentifierVisitor _(S, [&](auto &VR, bool lhs) {
      if (!lhs) {
        V.insert(VR);
      }
    });
    for (auto *D : V) {
      if (refd.find(D) == refd.end()) {
        free.insert(D);
        refd.insert(D);
      }
    }

    if (auto *CS = dyn_cast<CopyIRStmt>(S)) {
      if (refd.find(CS->Dest) == refd.end()) {
        refd.insert(CS->Dest);
        free.erase(CS->Dest);
      }
    }
  }

  void analyzePath(ContFun &CF, SetVector<IRBasicBlock *> &path,
                   std::set<IRVarRef> *inFrees) {
    std::set<IRVarRef> &free = CF.Args;
    std::set<IRVarRef> &refd = CF.Locals;

    if (inFrees) {
      for (auto &v : *inFrees) {
        free.insert(v);
      }
    }

    for (auto &bb : path) {
      for (auto &S : *bb) {
        analyzeStmt(S.get(), free, refd);
      }
      if (bb->Term) {
        // Skip ReturnIRStmt with null RetVal — ExprIdentifierVisitor
        // would crash trying to visit the null expression.
        if (auto *RS = dyn_cast<ReturnIRStmt>(bb->Term)) {
          if (!RS->RetVal)
            continue;
        }
        analyzeStmt(bb->Term, free, refd);
      }
    }
    for (auto *v : free) {
      if (refd.find(v) != refd.end()) {
        refd.erase(v);
      }
    }
  }

public:
  CreateContinuationFuns(IRFunction &F) {
    INFO {
      F.cleanVars();
      outs() << F.getName() << " (init):\n";
      F.printVars(outs());
    }
    createSyncPaths(F);

    if (Paths.size() <= 1) {
      return;
    }

    std::deque<IRBasicBlock *> WorkList;
    for (auto &B : F) {
      if (PathLookup[B.get()] == 0)
        continue;
      if (B->Succs.empty()) {
        WorkList.push_back(B.get());
      }
    }

    for (int p = 0; p < Paths.size() - 1; p++) {
      std::string CfName = F.getName() + "_cont" + std::to_string(p);
      ContFun CF =
          ContFun{.F = F.getParent()->createFunc(CfName, F.getReturnType()),
                  .Args = std::set<IRVarRef>(),
                  .Locals = std::set<IRVarRef>()};
      CF.F->Info.IsTask = true;
      ContFuns.push_back(CF);
    }
    std::vector<bool> visited(Paths.size() - 1, 0);

    while (!WorkList.empty()) {
      auto *bb = WorkList.front();
      WorkList.pop_front();

      assert(PathLookup.find(bb) != PathLookup.end());
      if (PathLookup[bb] == 0)
        continue;
      int path = PathLookup[bb] - 1;
      if (visited[path]) {
        continue;
      }

      std::set<IRVarRef> *inFrees = NULL;
      if (!bb->Succs.empty()) {
        if (auto *succBb = *(bb->Succs.begin())) {
          assert(PathLookup.find(succBb) != PathLookup.end());
          inFrees = &(ContFuns[PathLookup[succBb] - 1].Args);
        }
      }

      analyzePath(ContFuns[path], Paths[path + 1], inFrees);
      visited[path] = true;

      auto *startBb = Paths[path + 1][0];

      startBb->iteratePreds(
          [&](IRBasicBlock *Pred) -> void { WorkList.push_front(Pred); });
    }

    for (int p = 0; p < Paths.size(); p++) {
      auto &Path = Paths[p];
      std::unordered_map<IRVarRef, IRVarRef> Remap;

      if (p > 0) {
        auto &CF = ContFuns[p - 1];
        for (auto *Arg : CF.Args) {
          CF.F->Vars.push_back(IRVarDecl{
              .Type = Arg->Type,
              .Name = Arg->Name,
              .DeclLoc = IRVarDecl::ARG,
          });
          Remap[Arg] = &(CF.F->Vars.back());
        }
        for (auto *Local : CF.Locals) {
          CF.F->Vars.push_back(IRVarDecl{
              .Type = Local->Type,
              .Name = Local->Name,
              .DeclLoc = IRVarDecl::LOCAL,
          });
          Remap[Local] = &(CF.F->Vars.back());
        }
      }

      for (auto *B : Path) {
        IRFunction *SpawnNextDest = nullptr;
        if (B->Term) {
          if (isa<SyncIRStmt>(B->Term)) {
            auto *succBb = *(B->Succs.begin());
            assert(succBb);
            assert(PathLookup.find(succBb) != PathLookup.end());
            assert(PathLookup[succBb] > 0);
            delete B->Term;
            SpawnNextDest = ContFuns[PathLookup[succBb] - 1].F;
            B->Term = new SpawnNextIRStmt(SpawnNextDest);
            B->Succs.clear();
          }
        }
        if (p > 0) {
          B->getParent()->moveBlock(B, ContFuns[p - 1].F);
          auto &CF = ContFuns[p - 1];
          auto RemapCB = [&](auto &VR, bool lhs) {
            if (Remap.find(VR) == Remap.end()) {
              // Variable not found in remap — it was referenced in a
              // statement (e.g., a spawn to fn_exit/fn_reentry) but
              // not detected by analyzePath. Add it as an ARG.
              CF.F->Vars.push_back(IRVarDecl{
                  .Type = VR->Type,
                  .Name = VR->Name,
                  .DeclLoc = IRVarDecl::ARG,
              });
              Remap[VR] = &CF.F->Vars.back();
            }
            VR = Remap[VR];
          };
          for (auto &S : *B) {
            ExprIdentifierVisitor e(S.get(), RemapCB);
          }
          if (B->Term) {
            ExprIdentifierVisitor e(B->Term, RemapCB);
          }
        }
        if (SpawnNextDest) {
          B->getParent()->Info.SpawnNextList.insert(SpawnNextDest);
        }
      }
    }

    F.cleanVars();

    INFO {
      outs() << F.getName() << ":\n";
      F.printVars(outs());

      int I = 0;
      for (auto &CF : ContFuns) {
        outs() << "ContF" << CF.F->getInd() << ":\n";
        CF.F->printVars(outs());
        I++;
      }
    }
  }
};

//////////////////////////
// FinalizeExplicitCPS //
////////////////////////

struct FinalizeExplicitCPS {
  std::vector<std::pair<IRBasicBlock *, IRStmt *>> ClosureDeclWorkList;

  class ScopeStartMapper : public ScopedIRTraverser {
  private:
    std::unordered_map<IRBasicBlock *, IRBasicBlock *> &ScopeStarts;
    std::vector<IRBasicBlock *> ScopeStack;
    bool pushNext = false;

    void handleScope(ScopeEvent SE) override {
      if (SE == ScopeEvent::Open || SE == ScopeEvent::Else) {
        pushNext = true;
      }
      if (SE == ScopeEvent::Close || SE == ScopeEvent::Else) {
        ScopeStack.pop_back();
      }
    }

    void visitBlock(IRBasicBlock *B) override {
      if (pushNext) {
        ScopeStack.push_back(B);
        pushNext = false;
      }
      ScopeStarts[B] = ScopeStack.back();
    }

  public:
    ScopeStartMapper(
        std::unordered_map<IRBasicBlock *, IRBasicBlock *> &ScopeStarts)
        : ScopeStarts(ScopeStarts) {}

    void reset(IRBasicBlock *Entry) {
      ScopeStack.clear();
      ScopeStack.push_back(Entry);
    }
  };

  void PlaceClosureDecl(IRBasicBlock *StartB, IRStmt *CDS) {
    size_t ind = 0;
    do {
      ind = 0;
      for (auto it = StartB->begin(); it != StartB->end(); ind++, it++) {
        if (isa<ESpawnIRStmt>(it->get())) {
          goto found_decl_loc;
        }
      }
      if (StartB->Succs.size() != 1) {
        break;
      }
      StartB = StartB->Succs[0];
    } while (true);

  found_decl_loc:
    StartB->insertAt(std::min(ind, StartB->lenInsns()), CDS);
  }

  void CreateClosureDecls(
      IRFunction *F,
      std::unordered_map<IRBasicBlock *, IRBasicBlock *> &ScopeStarts) {
    for (auto &B : *F) {
      if (B->Term) {
        if (auto *SNTerm = dyn_cast<SpawnNextIRStmt>(B->Term)) {
          auto *StartB = ScopeStarts[B.get()];
          assert(StartB);

          auto *DeclS = new ClosureDeclIRStmt(SNTerm->Fn);
          SNTerm->Decl = DeclS;
          for (auto &DestVar : SNTerm->Fn->Vars) {
            if (DestVar.DeclLoc == IRVarDecl::ARG) {
              for (auto &SrcVar : F->Vars) {
                if (SrcVar.Name == DestVar.Name &&
                    SrcVar.Type == DestVar.Type) {
                  DeclS->addCallerToCaleeVarMapping(&SrcVar, &DestVar);
                }
              }
            }
          }
          ClosureDeclWorkList.push_back(std::make_pair(StartB, DeclS));
        }
      }
    }
  }

  SpawnNextIRStmt *DFSTillSpawnNext(IRBasicBlock *StartB) {
    std::vector<IRBasicBlock *> WorkList;
    std::set<IRBasicBlock *> SeenList;
    WorkList.push_back(StartB);

    SpawnNextIRStmt *FoundSpawnNext = nullptr;

    while (!WorkList.empty()) {
      IRBasicBlock *B = WorkList.back();
      WorkList.pop_back();
      SeenList.insert(B);

      if (B->Term && isa<SpawnNextIRStmt>(B->Term)) {
        if (FoundSpawnNext) {
          PANIC("Ambiguous spawnNext for spawn!");
        } else {
          FoundSpawnNext = dyn_cast<SpawnNextIRStmt>(B->Term);
        }
      }

      for (auto &Succ : B->Succs) {
        if (SeenList.find(Succ) == SeenList.end()) {
          WorkList.push_back(Succ);
        }
      }
    }

    return FoundSpawnNext;
  }

  void MakeSpawnsExplicit(IRFunction *F) {
    for (auto &B : *F) {
      for (auto &S : *B) {
        IRLvalExpr *Dest = nullptr;
        ISpawnIRExpr *IS = nullptr;
        bool Local = false;
        if (auto CS = dyn_cast<CopyIRStmt>(S.get())) {
          IS = dyn_cast<ISpawnIRExpr>(CS->Src.get());
          if (IS) {
            CS->Dest->IsEphemeral = true;
            Dest = new IdentIRExpr(CS->Dest);
            CS->Src.release();
          }
          Local = true;
        } else if (auto SS = dyn_cast<StoreIRStmt>(S.get())) {
          IS = dyn_cast<ISpawnIRExpr>(SS->Src.get());
          if (IS) {
            Dest = SS->Dest.release();
            SS->Src.release();
          }
        } else if (auto EWS = dyn_cast<ExprWrapIRStmt>(S.get())) {
          IS = dyn_cast<ISpawnIRExpr>(EWS->Expr.get());
          if (IS) {
            EWS->Expr.release();
          }
        }

        assert(!Dest || IS);
        if (Dest || IS) {
          SpawnNextIRStmt *SN = DFSTillSpawnNext(B.get());
          if (!SN && Dest) {
            PANIC("Spawn has a return value, but no corresponding spawn "
                  "next..");
          }

          std::vector<IRExpr *> Args;
          for (auto &Arg : IS->Args) {
            Args.push_back(Arg.get());
            Arg.release();
          }

          IRFunction *Fn = nullptr;
          if (auto SFn = std::get_if<IRFunction *>(&IS->Fn)) {
            Fn = *SFn;
          } else {
            PANIC("Implicit spawn destination still unknown, needs to be known "
                  "for explicit conversion");
          }

          delete IS;

          S = std::make_unique<ESpawnIRStmt>(Dest, Fn, SN, Args, Local);
          F->Info.SpawnList.insert(Fn);
        }
      }
    }
  }

  FinalizeExplicitCPS(IRFunction *F) {
    std::unordered_map<IRBasicBlock *, IRBasicBlock *> ScopeStarts;
    ScopeStartMapper SSM(ScopeStarts);
    SSM.reset(F->getEntry());
    SSM.traverse(*F);
    CreateClosureDecls(F, ScopeStarts);
    MakeSpawnsExplicit(F);
    for (auto &[StartB, CDS] : ClosureDeclWorkList) {
      PlaceClosureDecl(StartB, CDS);
    }
  }
};

void MakeExplicit(IRProgram &P) {
  // Phase 0: Restructure loops with syncs.
  // This pre-pass converts loops-with-syncs into separate functions
  // (fn_loop_reentry, fn_exit) so that the original sync-splitting
  // in CreateContinuationFuns doesn't encounter loop back-edges.
  {
    std::vector<IRFunction *> WorkList;
    for (auto &F : P) {
      WorkList.push_back(F.get());
    }
    // Process iteratively: restructuring creates new functions that
    // may themselves contain loops with syncs (e.g., fn_loop_reentry
    // for nested loops).
    while (!WorkList.empty()) {
      auto *F = WorkList.back();
      WorkList.pop_back();
      auto NewFuncs = restructureLoopsWithSync(*F);
      if (!NewFuncs.empty()) {
        // This function had a loop restructured. It may have more
        // loops-with-syncs, so re-add it to the worklist.
        WorkList.push_back(F);
      }
      for (auto *NF : NewFuncs) {
        WorkList.push_back(NF);
      }
    }
  }

  // Clean up ALL stale successor edges across ALL functions before Phase 1.
  // After Phase 0, some functions may have successor edges pointing to
  // blocks that were moved to other functions during restructuring.
  for (auto &F : P) {
    for (auto &B : *F) {
      std::vector<IRBasicBlock *> ToRemove;
      for (auto *Succ : B->Succs) {
        if (Succ->getParent() != F.get()) {
          ToRemove.push_back(Succ);
        }
      }
      for (auto *S : ToRemove) {
        B->Succs.remove(S);
      }
    }
  }

  // Phase 1: Create continuation functions at sync points.
  std::vector<IRFunction *> WorkList;
  for (auto &F : P) {
    WorkList.push_back(F.get());
  }

  for (auto &F : WorkList) {
    CreateContinuationFuns CCF(*F);
    std::vector<IRFunction *> FWorkList{F};
    for (auto &CF : CCF.ContFuns) {
      FWorkList.push_back(CF.F);
    }
    // Phase 2: Finalize explicit CPS for each function.
    for (auto *F : FWorkList) {
      FinalizeExplicitCPS FC(F);
    }
  }
}