#ifndef BOMBYX_CORE_OVERLAPMEMANALYSIS_HPP
#define BOMBYX_CORE_OVERLAPMEMANALYSIS_HPP

#include "core/IR.hpp"
#include "clang/AST/ASTContext.h"

// Automatic decoupled-access lowering for `#pragma BOMBYX OVERLAP` loops.
//
// Rewrites plain global-memory subscripts in an OVERLAP loop body into
// dependent spawns of a synthesised memory-reader task plus a sync, so the
// user writes `listA[i]` rather than `cilk_spawn accessMemory(listA, i)` and
// keeps cilk_spawn/cilk_sync for real tasks.
//
// This is the manual `#pragma BOMBYX DAE` transformation (see DAE.cpp),
// generalised: triggered by OVERLAP rather than a hand-placed marker, applied
// to a whole batch of independent loads at once, and sharing a single reader
// task across every load site in a loop — the OVERLAP wrapper generator
// instantiates exactly one `u_memreader`, so per-site reader tasks would not
// be accepted.
//
// Scope is the loop *reentry* function only (Info.IsOverlapReentry): it holds
// the per-iteration body. Loop entry and exit loads run once per task, so
// decoupling them would only widen the closure.
//
// Must run after FlattenIR (which splits the loop and sets the flag) and
// before MakeExplicit (which turns the emitted ISpawnIRExpr into an
// ESpawnIRStmt with its SpawnNext, and splits on the emitted sync).
void OverlapMemAnalysis(IRProgram &P, clang::ASTContext &Ctx);

#endif
