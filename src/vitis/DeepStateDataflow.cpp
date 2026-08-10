#include "vitis/DeepStateDataflow.hpp"

#include "llvm/Support/Casting.h"

#include <functional>
#include <set>

using llvm::dyn_cast;
using llvm::isa;

namespace deepstate {

namespace {

// ─── small IR helpers ────────────────────────────────────────────────────────

/// The variable a statement assigns, or null if it assigns nothing. Assignments
/// reach the printer either as a CopyIRStmt (typed destination) or as a
/// StoreIRStmt whose destination is a plain ident; a StoreIRStmt with any other
/// destination is an m_axi write, which the ReadOnly precondition already
/// excludes.
IRVarRef stmtDefines(IRStmt *S) {
  if (auto *CS = dyn_cast<CopyIRStmt>(S))
    return CS->Dest;
  if (auto *SS = dyn_cast<StoreIRStmt>(S))
    if (auto *ID = dyn_cast<IdentIRExpr>(SS->Dest.get()))
      return ID->Ident;
  return nullptr;
}

IRExpr *stmtRHS(IRStmt *S) {
  if (auto *CS = dyn_cast<CopyIRStmt>(S))
    return CS->Src.get();
  if (auto *SS = dyn_cast<StoreIRStmt>(S))
    return SS->Src.get();
  return nullptr;
}

bool walk(IRExpr *E, const std::function<void(IRExpr *)> &Pre);

bool walkStmtExprs(IRStmt *S, const std::function<void(IRExpr *)> &Pre,
                   IRVarSetTy &DirectVarReads) {
  if (auto *ES = dyn_cast<ESpawnIRStmt>(S)) {
    for (auto &A : ES->Args)
      if (!walk(A.get(), Pre))
        return false;
    return true;
  }
  if (auto *SN = dyn_cast<SpawnNextIRStmt>(S)) {
    // handleSpawnNext copies closure fields straight from variable refs, with no
    // expression in between; they are reads all the same.
    if (SN->Decl)
      for (auto &[SrcVar, DstVar] : SN->Decl->Caller2Callee) {
        (void)DstVar;
        if (SrcVar && !SrcVar->IsEphemeral)
          DirectVarReads.insert(SrcVar);
      }
    return true;
  }
  if (auto *CDS = dyn_cast<ClosureDeclIRStmt>(S))
    return !CDS->SpawnCount || walk(CDS->SpawnCount.get(), Pre);
  if (auto *RS = dyn_cast<ReturnIRStmt>(S))
    return !RS->RetVal || walk(RS->RetVal.get(), Pre);
  if (auto *EW = dyn_cast<ExprWrapIRStmt>(S))
    return walk(EW->Expr.get(), Pre);
  if (IRExpr *RHS = stmtRHS(S))
    return walk(RHS, Pre);
  return true;
}

/// Walk every subexpression. Returns false if a node kind this analysis does not
/// model is reached, in which case the caller must give up rather than guess.
bool walk(IRExpr *E, const std::function<void(IRExpr *)> &Pre) {
  if (!E)
    return true;
  Pre(E);
  if (auto *CE = dyn_cast<CallIRExpr>(E)) {
    for (auto &A : CE->Args)
      if (!walk(A.get(), Pre))
        return false;
    return true;
  }
  if (auto *AE = dyn_cast<AccessIRExpr>(E))
    return walk(AE->Base.get(), Pre);
  if (auto *DE = dyn_cast<DRefIRExpr>(E))
    return walk(DE->Expr.get(), Pre);
  if (auto *IE = dyn_cast<IndexIRExpr>(E))
    return walk(IE->Arr.get(), Pre) && walk(IE->Ind.get(), Pre);
  if (auto *RE = dyn_cast<RefIRExpr>(E))
    return walk(RE->E.get(), Pre);
  if (auto *CastE = dyn_cast<CastIRExpr>(E))
    return walk(CastE->E.get(), Pre);
  if (auto *BE = dyn_cast<BinopIRExpr>(E))
    return walk(BE->Left.get(), Pre) && walk(BE->Right.get(), Pre);
  if (auto *UE = dyn_cast<UnopIRExpr>(E))
    return walk(UE->Expr.get(), Pre);
  if (isa<IdentIRExpr>(E) || isa<ASTLiteralIRExpr>(E) ||
      isa<IntLiteralIRExpr>(E) || isa<FIdentIRExpr>(E))
    return true;
  return false; // unmodelled kind
}

/// Every variable read by `E`, including the address subexpressions of any read
/// site inside it.
bool readsOf(IRExpr *E, IRVarSetTy &Out) {
  return walk(E, [&](IRExpr *N) {
    if (auto *ID = dyn_cast<IdentIRExpr>(N))
      Out.insert(ID->Ident);
  });
}

/// Every variable read by `E` treating a read site as a leaf. Once a site is
/// lifted into a load process its address is computed by `issue`, and the site
/// itself prints as the local holding the returned value, so the variables under
/// it are not reads of whichever stage consumes that value. Without this, a
/// pointer used only to form an address gets carried into `compute` and sits
/// there unused.
bool readsOutsideSites(IRExpr *E, const std::set<const IRExpr *> &Sites,
                       IRVarSetTy &Out) {
  if (!E || Sites.count(E))
    return true;
  if (auto *ID = dyn_cast<IdentIRExpr>(E)) {
    Out.insert(ID->Ident);
    return true;
  }
  if (auto *CE = dyn_cast<CallIRExpr>(E)) {
    for (auto &A : CE->Args)
      if (!readsOutsideSites(A.get(), Sites, Out))
        return false;
    return true;
  }
  if (auto *AE = dyn_cast<AccessIRExpr>(E))
    return readsOutsideSites(AE->Base.get(), Sites, Out);
  if (auto *DE = dyn_cast<DRefIRExpr>(E))
    return readsOutsideSites(DE->Expr.get(), Sites, Out);
  if (auto *IE = dyn_cast<IndexIRExpr>(E))
    return readsOutsideSites(IE->Arr.get(), Sites, Out) &&
           readsOutsideSites(IE->Ind.get(), Sites, Out);
  if (auto *RE = dyn_cast<RefIRExpr>(E))
    return readsOutsideSites(RE->E.get(), Sites, Out);
  if (auto *CastE = dyn_cast<CastIRExpr>(E))
    return readsOutsideSites(CastE->E.get(), Sites, Out);
  if (auto *BE = dyn_cast<BinopIRExpr>(E))
    return readsOutsideSites(BE->Left.get(), Sites, Out) &&
           readsOutsideSites(BE->Right.get(), Sites, Out);
  if (auto *UE = dyn_cast<UnopIRExpr>(E))
    return readsOutsideSites(UE->Expr.get(), Sites, Out);
  if (isa<ASTLiteralIRExpr>(E) || isa<IntLiteralIRExpr>(E) ||
      isa<FIdentIRExpr>(E))
    return true;
  return false;
}

bool containsSite(IRExpr *E, const std::set<const IRExpr *> &Sites,
                  bool &Found) {
  return walk(E, [&](IRExpr *N) {
    if (Sites.count(N))
      Found = true;
  });
}

/// If `E` is a read site, or a chain of casts over one, return that site. This
/// is the "trivial load" case: the destination variable is just the loaded
/// value, so it needs no compute stage and no stream of its own.
const IRExpr *bareSite(IRExpr *E, const std::set<const IRExpr *> &Sites) {
  while (auto *CastE = dyn_cast<CastIRExpr>(E))
    E = CastE->E.get();
  return Sites.count(E) ? E : nullptr;
}

std::string typeOfSite(const IRExpr *E) {
  if (auto *IE = dyn_cast<IndexIRExpr>(E))
    return IE->ArrType.getAsString();
  if (auto *DE = dyn_cast<DRefIRExpr>(E))
    return DE->PointeeType.getAsString();
  return {};
}

std::string cTypeOf(IRVarRef V) { return V->Type.getAsString(); }

Plan skip(const char *Why) {
  Plan P;
  P.Apply = false;
  P.SkipReason = Why;
  return P;
}

} // namespace

IRExpr *srcExpr(IRStmt *S) { return stmtRHS(S); }

Plan plan(IRFunction *Task, const std::vector<const IRExpr *> &SiteList,
          const std::map<const IRExpr *, unsigned> &SiteChan,
          unsigned TaskInBits, unsigned TaskOutBits, const Opts &O,
          bool ReadOnly) {
  if (O.M == Mode::None)
    return skip("disabled");
  // A read-only PE with no read site touches no memory at all, so there is
  // nothing to shorten. A writing PE has no site list (planMemChannels stops
  // collecting once it sees the store) but does have an m_axi port, and that
  // port's latency is exactly what sets its pipeline depth.
  if (SiteList.empty() && ReadOnly)
    return skip("no m_axi read site");

  std::set<const IRExpr *> Sites(SiteList.begin(), SiteList.end());

  // ── StateBits ──────────────────────────────────────────────────────────────
  // How much closure state the flushable pipeline registers: one copy per
  // stage, and the depth is Vitis's default m_axi latency plus a little fixed
  // overhead. Decided up front, and without reference to the body, so the
  // latency-only treatment does not depend on the code-splitting preconditions
  // that follow -- shortening a pipeline is safe whatever shape the body has.
  const unsigned DefaultMAxiLatency = 64;
  unsigned BaseDepth = DefaultMAxiLatency + 8;
  unsigned StateBits = ((TaskInBits + TaskOutBits) / 2) * BaseDepth;
  if (StateBits < O.MinStateBits)
    return skip("estimated pipeline state below threshold");

  if (O.M == Mode::Latency) {
    Plan LP;
    LP.ApplyLatency = true;
    LP.StateBitsBefore = StateBits;
    LP.EstDepth = BaseDepth;
    return LP;
  }

  // ── ReadOnly ───────────────────────────────────────────────────────────────
  // DATAFLOW only. The rewrite reprints the body across stage functions, and an
  // m_axi store has no stage it can safely live in: `merge` would have to reissue
  // it after the closure comes back out of the ctx FIFO, which is a different
  // memory ordering than the flat pipeline's. Mode::Latency has already returned
  // above, so a writing PE still gets its shortened latency.
  if (!ReadOnly)
    return skip("task writes m_axi");
  if (SiteList.empty())
    return skip("no m_axi read site");

  // ── SiteKind ───────────────────────────────────────────────────────────────
  // MEM_STRUCT sites (arrow accesses) are not lifted: the loaded field's C type
  // is not recoverable here, and no current kernel has one.
  for (const IRExpr *S : SiteList) {
    if (!isa<IndexIRExpr>(S) && !isa<DRefIRExpr>(S))
      return skip("read site is not MEM_ARR_IN / MEM_IN");
    if (typeOfSite(S).empty())
      return skip("read site has no printable element type");
  }

  // ── Straightline ───────────────────────────────────────────────────────────
  // Flatten the body. Control flow is rejected outright: a conditional closure
  // write would break the OVERLAP wrapper's 1-in/1-out contract, which is
  // enforced structurally by positional context FIFOs and therefore fails
  // silently rather than loudly.
  // SpawnNextIRStmt and ReturnIRStmt are terminators but are emitted as ordinary
  // body statements (the contStateOut push, the argOut send), so they belong in
  // the flattened list; If/Loop/Sync/Break/Continue are real control flow.
  auto isControlFlow = [](IRStmt *S) {
    return isa<IfIRStmt>(S) || isa<LoopIRStmt>(S) || isa<BreakIRStmt>(S) ||
           isa<ContinueIRStmt>(S) || isa<SyncIRStmt>(S);
  };
  std::vector<IRStmt *> Body;
  for (auto &B : *Task) {
    for (auto &S : *B) {
      IRStmt *St = S.get();
      if (St->Silent)
        continue;
      if (isControlFlow(St))
        return skip("body has control flow");
      if (isa<ASTStmtWrapIRStmt>(St))
        return skip("body has an opaque AST statement");
      if (isa<ScopeAnnotIRStmt>(St))
        continue;
      Body.push_back(St);
    }
    if (B->Term) {
      if (isControlFlow(B->Term))
        return skip("body has control flow");
      if (!B->Term->Silent &&
          (isa<SpawnNextIRStmt>(B->Term) || isa<ReturnIRStmt>(B->Term)))
        Body.push_back(B->Term);
    }
  }

  // ── SingleAssign ───────────────────────────────────────────────────────────
  // With statements split across stages, "the value of v" must be unambiguous.
  IRVarMapTy<unsigned> DefCount;
  for (IRStmt *S : Body)
    if (IRVarRef V = stmtDefines(S))
      DefCount[V]++;
  for (auto &[V, N] : DefCount)
    if (N > 1)
      return skip("a variable is assigned more than once");

  // ── AddrFromArgs ───────────────────────────────────────────────────────────
  // The issue stage runs with nothing but `args`, so every address must be
  // computable from ARG variables alone. bombyx already splits dependent loads
  // into separate memrd continuations, so this holds by construction; check it
  // anyway, because a violation would silently issue the wrong address.
  for (const IRExpr *S : SiteList) {
    IRVarSetTy Reads;
    bool OK = true;
    if (auto *IE = dyn_cast<IndexIRExpr>(S))
      OK = readsOf(IE->Arr.get(), Reads) && readsOf(IE->Ind.get(), Reads);
    else if (auto *DE = dyn_cast<DRefIRExpr>(S))
      OK = readsOf(DE->Expr.get(), Reads);
    if (!OK)
      return skip("read address uses an unmodelled expression");
    for (IRVarRef V : Reads)
      if (V->DeclLoc != IRVarDecl::ARG || DefCount.count(V))
        return skip("read address depends on a computed value");
  }

  // ── load-dependent variables ───────────────────────────────────────────────
  // Least fixpoint: a variable is load-dependent if its defining statement's RHS
  // contains a read site or reads a load-dependent variable.
  IRVarSetTy Dep;
  bool Changed = true;
  while (Changed) {
    Changed = false;
    for (IRStmt *S : Body) {
      IRVarRef D = stmtDefines(S);
      if (!D || Dep.count(D))
        continue;
      IRExpr *RHS = stmtRHS(S);
      if (!RHS)
        continue;
      bool Found = false;
      if (!containsSite(RHS, Sites, Found))
        return skip("statement uses an unmodelled expression");
      if (!Found) {
        IRVarSetTy Reads;
        if (!readsOf(RHS, Reads))
          return skip("statement uses an unmodelled expression");
        for (IRVarRef V : Reads)
          if (Dep.count(V))
            Found = true;
      }
      if (Found) {
        Dep.insert(D);
        Changed = true;
      }
    }
  }

  Plan P;

  // ── stage assignment ───────────────────────────────────────────────────────
  // A load-dependent definition whose RHS is exactly a load (possibly cast) is
  // "trivial": the variable aliases that load's value and no compute is needed.
  // Anything else goes to the compute stage.
  IRVarSetTy ComputeDefined;
  for (IRStmt *S : Body) {
    IRVarRef D = stmtDefines(S);
    if (D && Dep.count(D)) {
      IRExpr *RHS = stmtRHS(S);
      if (const IRExpr *BS = bareSite(RHS, Sites)) {
        P.TrivialLoadVar[D] = ""; // value variable filled in below
        (void)BS;
        continue;
      }
      ComputeStep Step;
      Step.S = S;
      Step.Def = D;
      Step.Local = "_r_" + GetSym(D->Name);
      Step.CTy = cTypeOf(D);
      P.Compute.push_back(Step);
      ComputeDefined.insert(D);
      continue;
    }
    P.MergeStmts.push_back(S);
  }

  // A merge statement must not read a load-dependent variable that the compute
  // stage has not produced, and must not read a compute result before compute
  // ran -- with single assignment and program order preserved within each stage,
  // the only way that happens is a merge statement reading a Dep variable, which
  // by the definition of Dep would itself have been Dep. Assert the invariant
  // rather than trust it.
  IRVarSetTy MergeReads;
  for (IRStmt *S : P.MergeStmts) {
    bool OK = walkStmtExprs(
        S,
        [&](IRExpr *N) {
          if (auto *ID = dyn_cast<IdentIRExpr>(N))
            MergeReads.insert(ID->Ident);
        },
        MergeReads);
    if (!OK)
      return skip("statement uses an unmodelled expression");
  }
  for (IRVarRef V : MergeReads)
    if (Dep.count(V) && !ComputeDefined.count(V) && !P.TrivialLoadVar.count(V))
      return skip("merge statement reads an unproduced value");
  // Only stream a compute result merge actually reads: an unread dataflow
  // channel is not merely wasteful, it never drains and stalls the region.
  IRVarSetTy ResultVars;
  for (auto &Step : P.Compute)
    if (MergeReads.count(Step.Def)) {
      Step.Streamed = true;
      ResultVars.insert(Step.Def);
    }

  // ── ComputeInputs: what the compute stage needs carried to it ──────────────
  // Walked in program order against the set of values already available inside
  // compute, because a step may read a variable it also defines -- `contributions
  // = contributions + load/v_size` reads the value that ARRIVED, not the one it
  // is about to produce, so that read has to be carried. Treating every
  // load-dependent variable as "already available" would silently leave such a
  // read pointing at nothing.
  IRVarSetTy CarrySet;
  IRVarSetTy Produced;
  for (auto &[V, Val] : P.TrivialLoadVar) {
    (void)Val;
    Produced.insert(V);
  }
  for (auto &Step : P.Compute) {
    IRVarSetTy Reads;
    if (!readsOutsideSites(stmtRHS(Step.S), Sites, Reads))
      return skip("compute statement uses an unmodelled expression");
    for (IRVarRef V : Reads) {
      if (Produced.count(V))
        continue; // an earlier load or compute step made it
      if (V->DeclLoc != IRVarDecl::ARG || DefCount.count(V) > 1)
        return skip("compute statement reads a value produced in merge");
      if (DefCount.count(V) && V != Step.Def)
        return skip("compute statement reads a value produced in merge");
      CarrySet.insert(V); // the value that arrived with the task
    }
    Produced.insert(Step.Def);
  }

  // ── materialise the plan ───────────────────────────────────────────────────
  unsigned K = 0;
  for (const IRExpr *S : SiteList) {
    Site Si;
    Si.E = S;
    auto It = SiteChan.find(S);
    Si.Chan = It == SiteChan.end() ? 0u : It->second;
    Si.ElemTy = typeOfSite(S);
    Si.AddrStream = "df_addr" + std::to_string(K);
    Si.DataStream = "df_data" + std::to_string(K);
    Si.ValueVar = "_d" + std::to_string(K);
    P.Sites.push_back(Si);
    ++K;
  }
  // Bind each trivially-loaded variable to its load's value variable.
  for (IRStmt *S : Body) {
    IRVarRef D = stmtDefines(S);
    if (!D || !P.TrivialLoadVar.count(D))
      continue;
    const IRExpr *BS = bareSite(stmtRHS(S), Sites);
    for (auto &Si : P.Sites)
      if (Si.E == BS)
        P.TrivialLoadVar[D] = Si.ValueVar;
  }
  for (IRVarRef V : CarrySet) {
    Xfer X;
    X.Var = V;
    X.Stream = "df_carry_" + GetSym(V->Name);
    X.Local = "_c_" + GetSym(V->Name);
    X.CTy = cTypeOf(V);
    P.Carry.push_back(X);
  }
  for (auto &Step : P.Compute) {
    if (!Step.Streamed)
      continue;
    Xfer X;
    X.Var = Step.Def;
    X.Stream = "df_res_" + GetSym(Step.Def->Name);
    X.Local = Step.Local;
    X.CTy = Step.CTy;
    P.Result.push_back(X);
  }

  // ── SplitSite: each site must be consumed by exactly one stage ─────────────
  // A site is consumed by a stage either directly (the site node appears in one
  // of its statements) or indirectly (the statement reads a variable that was
  // assigned straight from that load). Both count: whichever stage consumes it
  // is the one that must read the data stream.
  {
    IRVarMapTy<const IRExpr *> TrivialSiteOf;
    for (IRStmt *S : Body) {
      IRVarRef D = stmtDefines(S);
      if (D && P.TrivialLoadVar.count(D))
        TrivialSiteOf[D] = bareSite(stmtRHS(S), Sites);
    }
    std::set<const IRExpr *> InCompute, InMerge;
    auto note = [&](const std::vector<IRStmt *> &Stmts,
                    std::set<const IRExpr *> &Into) {
      for (IRStmt *S : Stmts) {
        IRVarSetTy Reads;
        walkStmtExprs(
            S,
            [&](IRExpr *N) {
              if (Sites.count(N))
                Into.insert(N);
              if (auto *ID = dyn_cast<IdentIRExpr>(N))
                Reads.insert(ID->Ident);
            },
            Reads);
        for (IRVarRef V : Reads)
          if (auto It = TrivialSiteOf.find(V); It != TrivialSiteOf.end())
            Into.insert(It->second);
      }
    };
    { std::vector<IRStmt *> CS;
      for (auto &Step : P.Compute) CS.push_back(Step.S);
      note(CS, InCompute); }
    note(P.MergeStmts, InMerge);
    for (auto &Si : P.Sites) {
      if (InCompute.count(Si.E) && InMerge.count(Si.E))
        return skip("a read site is consumed by both compute and merge");
      Si.InCompute = InCompute.count(Si.E) > 0;
    }
  }

  // Refine the depth estimate now the post-load arithmetic is known; the number
  // of compute statements is a deliberately crude stand-in for its latency.
  P.EstDepth = BaseDepth + 24 * (unsigned)P.Compute.size();
  P.StateBitsBefore = ((TaskInBits + TaskOutBits) / 2) * P.EstDepth;
  P.Apply = true;
  P.ApplyLatency = true;
  return P;
}

} // namespace deepstate
