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
//    fn_exit/fn_reentry chains
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
  // Remove these to prevent later passes from following dangling edges.
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

// Remove successor edges that point to blocks in a different function.
// This can happen after restructuring moves blocks between functions.
static void cleanStaleSuccessorEdges(IRProgram &P) {
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
}

void FlattenIR(IRProgram &P) {
  // Restructure loops whose bodies contain syncs into separate task
  // functions (fn_reentry, fn_exit) with explicit spawn/return instead
  // of back-edges. Process iteratively: restructuring creates new
  // functions that may themselves contain loops with syncs (e.g.,
  // fn_loop_reentry for nested loops).
  std::vector<IRFunction *> WorkList;
  for (auto &F : P) {
    WorkList.push_back(F.get());
  }

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

  // Clean up ALL stale successor edges across ALL functions.
  // After restructuring, some functions may have successor edges pointing
  // to blocks that were moved to other functions.
  cleanStaleSuccessorEdges(P);
}