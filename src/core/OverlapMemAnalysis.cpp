#include "core/OverlapMemAnalysis.hpp"
#include "core/IR.hpp"

#include <cstdlib>
#include <optional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

// Recognise `base[idx]` where `base` is a pointer, i.e. a read of global
// memory. Subscripts of array-typed variables are local storage and stay
// inline; in this IR only pointers reach the m_axi space the wrapper's
// memReader serves.
IndexIRExpr *asGlobalLoad(IRExpr *E) {
  auto *IE = llvm::dyn_cast_or_null<IndexIRExpr>(E);
  if (!IE)
    return nullptr;
  auto *Base = llvm::dyn_cast<IdentIRExpr>(IE->Arr.get());
  if (!Base || !Base->Ident->Type->isPointerType())
    return nullptr;
  return IE;
}

// A decouplable load: a whole `Dest = <casts>(base[idx]);`.
//
// The load must be the entire right-hand side, modulo a chain of casts. That is
// what DAE already requires (DAE.cpp:19) and covers every access in an OVERLAP
// loop body written naturally; loads nested in a larger expression are left
// inline. Casts have to be tolerated because they are unavoidable in source
// like `uint32_t *nbrs = (uint32_t *)pGraph[2 * v];` and the implicit narrowing
// in `uint32_t deg = pGraph[2 * v + 1];`. MakeExplicit only recognises an
// ISpawnIRExpr that is the *bare* Src of a statement, so a wrapped site is
// split in two: the spawn writes a fresh temporary, and the cast is re-applied
// after the sync (see rewriteRound).
struct LoadSite {
  int Index;
  CopyIRStmt *Stmt;
  std::unique_ptr<IRExpr> *Slot; // the unique_ptr that owns Access
  IndexIRExpr *Access;
  IRType BaseTy; // type of the base pointer
  IRType ElemTy; // its pointee type
};

// The slot inside `Src` holding a global load, looking through casts, or null.
std::unique_ptr<IRExpr> *findLoadSlot(std::unique_ptr<IRExpr> &Src) {
  std::unique_ptr<IRExpr> *Slot = &Src;
  while (auto *CE = llvm::dyn_cast_or_null<CastIRExpr>(Slot->get()))
    Slot = &CE->E;
  return asGlobalLoad(Slot->get()) ? Slot : nullptr;
}

// Structural equality of two address expressions. Deliberately syntactic and
// conservative: anything not recognised compares unequal, so a caller can only
// ever conclude "these are provably the same expression", never the opposite.
bool structurallyEqual(const IRExpr *A, const IRExpr *B, clang::ASTContext &Ctx) {
  if (A == B)
    return true;
  if (!A || !B || A->getKind() != B->getKind())
    return false;
  // Source-level subexpressions reach the IR wrapped around a clang Expr.
  // clang's own folding-set profile is exactly the structural identity we want.
  if (auto *LA = llvm::dyn_cast<ASTLiteralIRExpr>(A)) {
    auto *LB = llvm::cast<ASTLiteralIRExpr>(B);
    if (!LA->getExpr() || !LB->getExpr())
      return false;
    llvm::FoldingSetNodeID IA, IB;
    LA->getExpr()->Profile(IA, Ctx, /*Canonical=*/true);
    LB->getExpr()->Profile(IB, Ctx, /*Canonical=*/true);
    return IA == IB;
  }
  if (auto *IA = llvm::dyn_cast<IdentIRExpr>(A))
    return IA->Ident == llvm::cast<IdentIRExpr>(B)->Ident;
  if (auto *LA = llvm::dyn_cast<IntLiteralIRExpr>(A))
    return LA->Lit == llvm::cast<IntLiteralIRExpr>(B)->Lit;
  if (auto *BA = llvm::dyn_cast<BinopIRExpr>(A)) {
    auto *BB = llvm::cast<BinopIRExpr>(B);
    return BA->Op == BB->Op && structurallyEqual(BA->Left.get(), BB->Left.get(), Ctx) &&
           structurallyEqual(BA->Right.get(), BB->Right.get(), Ctx);
  }
  if (auto *UA = llvm::dyn_cast<UnopIRExpr>(A)) {
    auto *UB = llvm::cast<UnopIRExpr>(B);
    return UA->Op == UB->Op && structurallyEqual(UA->Expr.get(), UB->Expr.get(), Ctx);
  }
  if (auto *CA = llvm::dyn_cast<CastIRExpr>(A)) {
    auto *CB = llvm::cast<CastIRExpr>(B);
    return CA->getCastType() == CB->getCastType() &&
           structurallyEqual(CA->E.get(), CB->E.get(), Ctx);
  }
  if (auto *XA = llvm::dyn_cast<IndexIRExpr>(A)) {
    auto *XB = llvm::cast<IndexIRExpr>(B);
    return structurallyEqual(XA->Arr.get(), XB->Arr.get(), Ctx) &&
           structurallyEqual(XA->Ind.get(), XB->Ind.get(), Ctx);
  }
  if (auto *DA = llvm::dyn_cast<DRefIRExpr>(A))
    return structurallyEqual(DA->Expr.get(),
                             llvm::cast<DRefIRExpr>(B)->Expr.get(), Ctx);
  return false;
}

// Split an index into a variable part and a constant offset, so `2*v` and
// `2*v + 1` can be recognised as neighbours. Only a trailing integer literal
// added to or subtracted from the whole expression is peeled — enough for the
// affine subscripts these loops are written with, and nothing that could
// mis-report a larger distance as a smaller one.
std::optional<int64_t> asIntConst(const IRExpr *E, clang::ASTContext &Ctx) {
  if (auto *IL = llvm::dyn_cast_or_null<IntLiteralIRExpr>(E))
    return (int64_t)IL->Lit;
  if (auto *AL = llvm::dyn_cast_or_null<ASTLiteralIRExpr>(E))
    if (clang::Expr *Ex = AL->getExpr())
      if (auto V = Ex->getIntegerConstantExpr(Ctx))
        return V->getSExtValue();
  return std::nullopt;
}

std::pair<const IRExpr *, int64_t> splitConstOffset(const IRExpr *E,
                                                   clang::ASTContext &Ctx) {
  if (auto *BE = llvm::dyn_cast_or_null<BinopIRExpr>(E)) {
    bool Add = BE->Op == BinopIRExpr::BINOP_ADD;
    bool Sub = BE->Op == BinopIRExpr::BINOP_SUB;
    if (Add || Sub)
      if (auto C = asIntConst(BE->Right.get(), Ctx))
        return {BE->Left.get(), Add ? *C : -*C};
    if (Add)
      if (auto C = asIntConst(BE->Left.get(), Ctx))
        return {BE->Right.get(), *C};
  }
  return {E, 0};
}

// Variables read by a site's address expression (base pointer and subscript).
IRVarSetTy addressVars(IndexIRExpr *IE) {
  IRVarSetTy Vars;
  ExprIdentifierVisitor _(static_cast<IRExpr *>(IE),
                          [&](IRVarRef &VR, bool IsLHS) {
                            if (!IsLHS)
                              Vars.insert(VR);
                          });
  return Vars;
}

bool consumesAny(IRStmt *S, const IRVarSetTy &Pending) {
  bool Found = false;
  ExprIdentifierVisitor _(S, [&](IRVarRef &VR, bool IsLHS) {
    if (!IsLHS && Pending.count(VR))
      Found = true;
  });
  return Found;
}

// Blocks of an OVERLAP loop's body: everything reachable from the body entry
// without crossing back through the header or out to the after-loop block.
// Mirrors loopBodyContainsSync in FlattenIR.cpp:42.
std::vector<IRBasicBlock *> loopBodyBlocks(IRBasicBlock *Header) {
  std::vector<IRBasicBlock *> Body;
  if (Header->Succs.size() < 2)
    return Body;
  IRBasicBlock *After = Header->Succs[1];
  std::set<IRBasicBlock *> Seen;
  std::vector<IRBasicBlock *> WL{Header->Succs[0]};
  while (!WL.empty()) {
    auto *B = WL.back();
    WL.pop_back();
    if (B == Header || B == After || Seen.count(B))
      continue;
    Seen.insert(B);
    Body.push_back(B);
    for (auto *S : B->Succs)
      WL.push_back(S);
  }
  return Body;
}

// One reader task per round:
//
//     ElemT memrd<N>(ElemT *base, size_t idx) { return base[idx]; }
//
// The name is deliberately short and NOT derived from the enclosing function.
// A dependent spawn's stream port is named
// `taskGlobalOut_<callee>_depends_<cont>`, so the reader's name is concatenated
// with an already long continuation name. Vitis HLS aborts building the
// synthesis data model on over-long port names, with
// `SsdmCdfg.cpp: INTERNAL_ERROR: Port name change` (and a rename message whose
// "from" and "to" are identical). Measured on this design: a 68-character port
// name synthesises, an 80-character one does not — `<fn>_memrd` pushed
// countIntersections' reader port to 80. Keeping the reader at `memrd<N>` keeps
// every generated port comfortably under the known-good length. Do not make
// these names descriptive again; PrintHardCilk warns if any port creeps back up.
//
// Address arithmetic stays in the caller, so one body serves every site in the
// round. A round gets its own reader (rather than all rounds sharing one)
// because rounds are separate reply channels in the lowered design: each has
// its own state FIFO and merge unit. Sharing would need an arbiter plus a
// round tag on every reply, and could not serve rounds whose element widths
// differ — which is exactly applyFn's case (`uint32_t *neighbors_u` in round 0,
// `uint64_t *pGraph` in round 1).
IRFunction *makeMemReader(IRProgram &P, clang::ASTContext &Ctx, IRFunction *F,
                          IRType BaseTy, int RoundNo) {
  static int NextReaderId = 0;
  std::string Name = "memrd" + std::to_string(NextReaderId++);
  (void)RoundNo;

  IRFunction *MemRd = P.createFunc(Name, BaseTy->getPointeeType());
  MemRd->Info.IsTask = true;
  MemRd->Info.IsMemReader = true;
  MemRd->Info.RootFun = F->Info.RootFun;

  MemRd->Vars.push_back(IRVarDecl{
      .Type = BaseTy, .Name = PutSym("base"), .DeclLoc = IRVarDecl::ARG});
  IRVarRef BaseArg = &MemRd->Vars.back();
  MemRd->Vars.push_back(IRVarDecl{.Type = Ctx.getSizeType(),
                                  .Name = PutSym("idx"),
                                  .DeclLoc = IRVarDecl::ARG});
  IRVarRef IdxArg = &MemRd->Vars.back();

  IRBasicBlock *B = MemRd->createBlock();
  // IndexIRExpr's third argument is the ELEMENT type, not the pointer type —
  // the HLS printer emits it straight into MEM_ARR_IN's type slot and into the
  // `idx * sizeof(...)` stride (VitisHLSTarget handleArray). Passing the
  // pointer type here made the reader assign a `T *` to a `T` reply field and
  // stride by sizeof(T *), which is how OpenCilk2IR builds a subscript too
  // (OpenCilk2IR.cpp:1028).
  B->Term = new ReturnIRStmt(new IndexIRExpr(new IdentIRExpr(BaseArg),
                                             new IdentIRExpr(IdxArg),
                                             BaseTy->getPointeeType()));

  return MemRd;
}

// All global loads in B, in program order.
std::vector<LoadSite> collectSites(IRBasicBlock *B) {
  std::vector<LoadSite> Sites;
  int Idx = 0;
  for (auto &S : *B) {
    if (auto *CS = llvm::dyn_cast<CopyIRStmt>(S.get()))
      if (auto *Slot = findLoadSlot(CS->Src)) {
        auto *IE = llvm::cast<IndexIRExpr>(Slot->get());
        IRType BaseTy = llvm::cast<IdentIRExpr>(IE->Arr.get())->Ident->Type;
        Sites.push_back({Idx, CS, Slot, IE, BaseTy, BaseTy->getPointeeType()});
      }
    Idx++;
  }
  return Sites;
}

// The next round of loads issuable from B: a prefix of the sites that is
// independent (no site's address reads another site's result), uniform in
// element type, and not interrupted by a statement consuming a result.
//
// Anything that stops the round is simply deferred to the next one, which is
// always safe: deferring a load only delays it past a sync it already had to
// follow.
std::vector<LoadSite> nextRound(IRBasicBlock *B) {
  std::vector<LoadSite> Sites = collectSites(B);
  if (Sites.empty())
    return {};

  IRVarSetTy Defs;
  for (const LoadSite &S : Sites)
    Defs.insert(S.Stmt->Dest);

  std::vector<LoadSite> Round;
  IRVarSetTy Pending;
  const clang::Type *RoundElem = nullptr;
  size_t Next = 0;
  for (int K = Sites.front().Index; K <= Sites.back().Index; K++) {
    if (Next < Sites.size() && Sites[Next].Index == K) {
      const LoadSite &S = Sites[Next];
      // A load whose address needs another load's result belongs to a later
      // round.
      bool Dependent = false;
      for (IRVarRef V : addressVars(S.Access))
        if (Defs.count(V))
          Dependent = true;
      if (Dependent)
        break;
      // One reader (one reply channel) per round, so a round holds a single
      // element type.
      if (RoundElem && RoundElem != S.ElemTy.getTypePtr())
        break;
      RoundElem = S.ElemTy.getTypePtr();
      Round.push_back(S);
      Pending.insert(S.Stmt->Dest);
      Next++;
      continue;
    }
    // Every load in the round is issued before the sync, so nothing between
    // the first and last of them may consume a load result.
    if (consumesAny(B->getAt(K), Pending))
      break;
  }
  return Round;
}

// Rewrite one round's loads into dependent spawns of a fresh reader, then close
// the round with a sync. Returns the remainder block, which holds the code that
// runs once the replies have landed.
IRBasicBlock *rewriteRound(IRProgram &P, clang::ASTContext &Ctx, IRFunction *F,
                           IRBasicBlock *B, std::vector<LoadSite> &Round,
                           int RoundNo, int &TmpCounter) {
  // ONE READER PER SITE, not one per round.
  //
  // A round's sites are independent by construction, so they can be fetched in
  // parallel — but only if each has its own request port and its own reply
  // channel. With a single shared reader the issuing PE writes N requests into
  // ONE stream, which Vitis HLS lowers to II = N, and the reader's single
  // in-order reply channel then carries N replies per iteration. That caps the
  // loop at N cycles per iteration however much concurrency is available:
  // measured 2.04 cycles per comparison for triangleDAE's 2-site inner round
  // (bombyx/tests/systemc). One reader per site makes the issuer II = 1 and lets
  // the merge unit gather all N replies in the same cycle — measured 1.13.
  //
  // Per-site readers also need no reply tagging and no arbiter, and they let
  // sites of different element widths share a round.
  std::vector<IRFunction *> MemRds;
  for (LoadSite &L : Round)
    MemRds.push_back(makeMemReader(P, Ctx, F, L.BaseTy, RoundNo));

  // Spatial reuse: do two of this round's reads land inside one AXI beat? They
  // do when they share a base pointer and their subscripts differ only by a
  // small constant, which is `pGraph[2*v]` / `pGraph[2*v+1]`.
  //
  // With one reader per site the two reads now go down SEPARATE channels, so a
  // per-port `#pragma HLS cache` can no longer turn the second into a hit on the
  // line the first brought in — each reader sees one address per iteration and
  // has no reuse of its own. The hint is therefore only set when a single
  // reader still serves several sites, which per-site splitting makes
  // impossible; it is kept so that the mechanism (and computeCacheableReaders)
  // stays live if a future round ever shares a reader again. The throughput the
  // split buys is worth strictly more than the one saved round trip.
  if (MemRds.size() == 1) {
    const uint64_t BeatBytes = 64;
    uint64_t ElemSize = Ctx.getTypeSizeInChars(Round.front().ElemTy).getQuantity();
    for (size_t I = 0; I < Round.size() && !MemRds[0]->Info.SpatialReuseHint; I++)
      for (size_t J = I + 1; J < Round.size(); J++) {
        auto [BaseI, OffI] = splitConstOffset(Round[I].Access->Ind.get(), Ctx);
        auto [BaseJ, OffJ] = splitConstOffset(Round[J].Access->Ind.get(), Ctx);
        if (!structurallyEqual(Round[I].Access->Arr.get(),
                               Round[J].Access->Arr.get(), Ctx) ||
            !structurallyEqual(BaseI, BaseJ, Ctx))
          continue;
        uint64_t Dist = (uint64_t)std::llabs(OffJ - OffI) * ElemSize;
        if (Dist != 0 && Dist < BeatBytes) {
          MemRds[0]->Info.SpatialReuseHint = true;
          break;
        }
      }
  }

  std::vector<IRStmt *> Fixups;
  size_t SiteIdx = 0;
  for (LoadSite &L : Round) {
    IRFunction *MemRd = MemRds[SiteIdx++];
    std::vector<IRExpr *> Args{L.Access->Arr.release(), L.Access->Ind.release()};
    if (L.Slot == &L.Stmt->Src) {
      // Bare `Dest = base[idx];` — the spawn simply replaces the load.
      L.Stmt->Src.reset(new ISpawnIRExpr(MemRd, Args));
      continue;
    }
    // Cast-wrapped. Land the reply in a temporary and re-apply the cast in the
    // remainder block, so the statement MakeExplicit sees is a bare spawn.
    F->Vars.push_back(IRVarDecl{.Type = L.ElemTy,
                                .Name = PutSym("__bombyx_memrd" +
                                               std::to_string(TmpCounter++)),
                                .DeclLoc = IRVarDecl::LOCAL});
    IRVarRef Tmp = &F->Vars.back();
    L.Slot->reset(new IdentIRExpr(Tmp)); // drops the now-childless load
    Fixups.push_back(new CopyIRStmt(L.Stmt->Dest, L.Stmt->Src.release()));
    L.Stmt->Dest = Tmp;
    L.Stmt->Src.reset(new ISpawnIRExpr(MemRd, Args));
  }

  // Close the round with a sync, exactly as DAEAtInd does.
  IRBasicBlock *RB = B->splitAt(Round.back().Index + 1);
  for (auto It = Fixups.rbegin(); It != Fixups.rend(); ++It)
    RB->pushStmtFront(*It);
  for (IRBasicBlock *S : B->Succs)
    RB->Succs.insert(S);
  B->Succs.clear();
  B->Succs.insert(RB);
  B->Term = new SyncIRStmt();
  return RB;
}

// Decouple every round of loads reachable from B, in dependence order.
// Returns the number of rounds emitted.
int decoupleFrom(IRProgram &P, clang::ASTContext &Ctx, IRFunction *F,
                 IRBasicBlock *B, int RoundNo, int &TmpCounter) {
  int Emitted = 0;
  while (B) {
    std::vector<LoadSite> Round = nextRound(B);
    if (Round.empty())
      break;
    B = rewriteRound(P, Ctx, F, B, Round, RoundNo + Emitted, TmpCounter);
    Emitted++;
  }
  return Emitted;
}

} // namespace

void OverlapMemAnalysis(IRProgram &P, clang::ASTContext &Ctx) {
  // Snapshot: synthesising a reader push_backs into the program's function
  // vector, which reallocates and would invalidate an in-flight iterator.
  std::vector<IRFunction *> Funcs;
  for (auto &FPtr : P)
    Funcs.push_back(FPtr.get());

  for (IRFunction *F : Funcs) {
    // Two scopes, matching the two shapes an OVERLAP loop can have once
    // FlattenIR has run:
    //
    //  * A loop containing a real cilk_sync is already split, and its
    //    per-iteration body lives in the reentry task. Decouple that whole
    //    function.
    //  * A purely serial loop has no sync yet, so FlattenIR left it intact.
    //    Decouple its body in place; the syncs emitted here are what make the
    //    *second* FlattenIR pass split it into reentry/exit at all
    //    (FlattenIR.cpp:267).
    //
    // Either way, code outside the per-iteration body — the loop initialiser
    // and the exit — is left alone: those loads run once per task, so
    // decoupling them would only widen the closure.
    std::vector<IRBasicBlock *> Scope;
    if (F->Info.IsOverlapReentry) {
      for (auto &B : *F)
        Scope.push_back(B.get());
    } else {
      for (auto &B : *F)
        if (auto *L = llvm::dyn_cast_or_null<LoopIRStmt>(B->Term))
          if (L->Overlap)
            for (IRBasicBlock *BB : loopBodyBlocks(B.get()))
              Scope.push_back(BB);
    }
    if (Scope.empty())
      continue;

    int RoundNo = 0;
    int TmpCounter = 0;
    for (IRBasicBlock *B : Scope)
      RoundNo += decoupleFrom(P, Ctx, F, B, RoundNo, TmpCounter);
  }
}
