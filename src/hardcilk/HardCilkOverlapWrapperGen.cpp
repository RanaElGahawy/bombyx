#include "hardcilk/HardCilkOverlapWrapperGen.hpp"
#include "core/IR.hpp"
#include "llvm/Support/MathExtras.h"

#include "llvm/ADT/STLExtras.h"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// ─── Overlap-loop topology extracted from the IR ─────────────────────────────

namespace {

struct ReplyField {
  std::string Name;   // continuation-closure field the reply lands in
  uint64_t OffBits;   // its bit offset inside the <cont>_task struct
  uint64_t WidthBits; // its width in bits
};

// One round of decoupled loads: a task issues some reads, its continuation
// consumes their replies. A loop body with dependent loads (`v = nbrs[i]` then
// `pGraph[2*v]`) has one round per dependence level, chained
// reentry -> cont0 -> cont1 -> ... Each round is an independent in-order reply
// channel with its own reader, state FIFO and merge unit.
struct OverlapRound {
  IRFunction *Issuer = nullptr; // task that issues this round's reads
  IRFunction *Cont = nullptr;   // continuation the replies complete
  // One reader per read SITE, parallel to Replies: site k is served by
  // MemReaders[k] on its own request port and its own reply channel, so all N
  // replies of an iteration arrive in the same cycle. See the rationale in
  // OverlapMemAnalysis::rewriteRound — a shared reader forces the issuing PE to
  // II = N and serialises N replies down one channel.
  std::vector<IRFunction *> MemReaders;
  std::vector<ReplyField> Replies; // reply slot per site, in issue order
  std::vector<uint64_t> ReplyDataBits; // payload width of each site's reply
};

// One loop level of the collapsed nest. A plain OVERLAP loop has exactly one;
// an OVERLAP loop with an unannotated inner loop (the intersection `while`
// inside `applyFn`'s OVERLAP `for`) has one per level, ordered outermost first.
//
// Each level is a ring: Head -> round chain -> Tail, closed by BackSrc's
// tail-spawn of Head. Tokens enter the ring from EntrySrc and leave it through
// Head's spawn of ExitTarget. Nesting is expressed by the outer level's Tail
// tail-spawning the inner level's Head, and the inner level's ExitTarget
// tail-spawning the outer level's Head (which makes it the outer BackSrc).
struct OverlapLevel {
  IRFunction *Head = nullptr;       // loop head: condition + issue round 0
  IRFunction *EntrySrc = nullptr;   // internal node that first enters Head;
                                    // null on an escaping level (wrapper taskIn)
  IRFunction *BackSrc = nullptr;    // internal node that closes this ring
  IRFunction *ExitTarget = nullptr; // Head's non-reader spawn target
  IRFunction *Tail = nullptr;       // last continuation of this level's chain
  IRFunction *TailSpawn = nullptr;  // what Tail tail-spawns (inner Head or Head)
  // Plain forwarding PEs between Tail and TailSpawn, in order. A loop body
  // containing an `if` compiles to a join task (`<head>_afterif0`) that carries
  // the induction-variable increment and hands the closure back to the head, so
  // the tail spawns THAT rather than a head. Such a PE issues no reads and owns
  // no continuation, so it is not a round — but it still has to be instantiated
  // in the ring, or the loop-back edge is left dangling and the loop retires
  // exactly one iteration before deadlocking.
  std::vector<IRFunction *> Relays;
  unsigned FirstRound = 0;          // index into OverlapTopology::Rounds
  unsigned NumRounds = 0;
};

struct OverlapTopology {
  IRFunction *Root = nullptr;    // loop initializer (reads external closure,
                                 // sets up loop state, spawns the reentry)
  IRFunction *Reentry = nullptr; // outermost OVERLAP reentry
  IRFunction *Exit = nullptr;    // outermost level's loop-exit task
  // Every round of every level, level 0's first. Round 0 therefore keeps the
  // unsuffixed net names a single-level loop has always used.
  std::vector<OverlapRound> Rounds;
  std::vector<OverlapLevel> Levels; // outermost first
  IRFunction *Tail = nullptr;     // last task in level 0's chain
  IRFuncSetTy Internal;// all PEs collapsed into the wrapper
  // When the loop body escapes (see OverlapGroup::Escapes) the wrapper stands
  // in for the reentry alone: there is no internal root, no internal loop-back
  // FIFO and no internal exit PE. The tail instead hands off to a real spawned
  // task through top-level ports, and the loop-back edge and the exit run
  // through the scheduler.
  bool Escapes = false;
  IRFunction *EscapeSpawn = nullptr; // real task the tail dependently spawns
  IRFunction *EscapeCont = nullptr;  // that spawn's external continuation
};

// Width in bits of a task's AXI-Stream closure (== its <name>_task struct).
static uint64_t taskWidthBits(const HCTaskInfo &Info) {
  return (uint64_t)(Info.TaskSize + Info.TaskPadding) * 8;
}

// Bit offset of an ARG field inside a <task>_task struct, matching the packed
// layout emitted by VitisHLSTarget::PrintDef: [ _counter(4, cont only) ]
// [ _cont(8) ] [ ARG vars in declaration order ] [ padding ].
static std::optional<ReplyField> fieldOffset(IRFunction *Task,
                                             const HCTaskInfo &Info,
                                             const std::string &Field) {
  uint64_t Off = (Info.IsCont ? 4u : 0u) + 8u; // bytes
  for (auto &Var : Task->Vars) {
    if (Var.DeclLoc != IRVarDecl::ARG)
      continue;
    HardCilkType *HCT = clangTypeToHardCilk(Var.Type);
    uint64_t Sz = (uint64_t)hardCilkTypeSize(HCT);
    delete HCT;
    if (GetSym(Var.Name) == Field)
      return ReplyField{Field, Off * 8, Sz * 8};
    Off += Sz;
  }
  return std::nullopt;
}

// Width in bits of a memReader-style <type>_arg_out packet (addr + data + size
// + allow, padded to a power-of-two byte width) — mirrors PrintDef.
static uint64_t argOutWidthBits(uint64_t DataBytes) {
  uint64_t Bytes = 8 /*addr*/ + DataBytes + 4 /*size*/ + 4 /*allow*/;
  uint64_t Padded = (uint64_t)llvm::NextPowerOf2(Bytes - 1);
  return Padded * 8;
}

// Targets of *dependent* spawns (ESpawnIRStmt with a SpawnNext) in F's body:
// the memory requests, as opposed to a plain tail-spawn token hand-off.
static void collectDepSpawnTargets(IRFunction *F, IRFuncSetTy &Out) {
  for (auto &B : *F)
    for (auto &S : *B)
      if (auto *ES = dyn_cast<ESpawnIRStmt>(S.get()))
        if (ES->SN && ES->Fn)
          Out.insert(ES->Fn);
}

// Discover the collapsed subsystem's task graph, one OverlapLevel per loop
// level of the nest. Returns nullopt if the group has no usable loop head.
static std::optional<OverlapTopology>
findOverlapTopology(IRProgram &P, const TaskInfosTy &TaskInfos,
                    const OverlapGroup &Grp) {
  OverlapTopology T;
  T.Internal = Grp.Internal;
  T.Escapes = Grp.Escapes;
  T.EscapeCont = Grp.EscapeCont;

  // Deterministic order: Grp.Internal is a std::set ordered by POINTER, so every
  // scan that picks "the" node must go through a name-sorted view.
  std::vector<IRFunction *> Nodes(Grp.Internal.begin(), Grp.Internal.end());
  llvm::sort(Nodes, [](IRFunction *A, IRFunction *B) {
    return A->getName() < B->getName();
  });

  // The loop heads: internal, non-continuation tasks that own a spawn_next. A
  // plain OVERLAP loop has exactly one; a nest has one per level.
  std::vector<IRFunction *> Heads;
  for (IRFunction *F : Nodes) {
    auto It = TaskInfos.find(F);
    if (It == TaskInfos.end())
      continue;
    if (!It->second.IsCont && !F->Info.SpawnNextList.empty())
      Heads.push_back(F);
  }
  if (Heads.empty())
    return std::nullopt;

  // Internal tail-spawn predecessors of each node (plain spawns only: a
  // dependent spawn is a memory request, not a token hand-off).
  IRFuncMapTy<std::vector<IRFunction *>> Preds;
  for (IRFunction *F : Nodes) {
    IRFuncSetTy Dep;
    collectDepSpawnTargets(F, Dep);
    std::vector<IRFunction *> Succs(F->Info.SpawnList.begin(),
                                    F->Info.SpawnList.end());
    llvm::sort(Succs, [](IRFunction *A, IRFunction *B) {
      return A->getName() < B->getName();
    });
    for (IRFunction *S : Succs)
      if (Grp.Internal.count(S) && !Dep.count(S))
        Preds[S].push_back(F);
  }

  // Order the heads outermost-first: breadth-first from the group's entry, so a
  // head reached only by passing through another head comes later.
  std::vector<IRFunction *> Ordered;
  {
    IRFuncSetTy Seen;
    std::deque<IRFunction *> Q;
    IRFunction *Seed = Grp.Escapes ? Grp.Reentry : Grp.Entry;
    if (!Seed)
      Seed = Heads.front();
    Q.push_back(Seed);
    Seen.insert(Seed);
    while (!Q.empty()) {
      IRFunction *F = Q.front();
      Q.pop_front();
      if (llvm::is_contained(Heads, F) && !llvm::is_contained(Ordered, F))
        Ordered.push_back(F);
      IRFuncSetTy Next(F->Info.SpawnList.begin(),
                                  F->Info.SpawnList.end());
      Next.insert(F->Info.SpawnNextList.begin(), F->Info.SpawnNextList.end());
      std::vector<IRFunction *> NextV(Next.begin(), Next.end());
      llvm::sort(NextV, [](IRFunction *A, IRFunction *B) {
        return A->getName() < B->getName();
      });
      for (IRFunction *S : NextV)
        if (Grp.Internal.count(S) && Seen.insert(S).second)
          Q.push_back(S);
    }
    for (IRFunction *H : Heads)
      if (!llvm::is_contained(Ordered, H))
        Ordered.push_back(H);
  }

  // Walk each level's spawn_next chain, collecting its rounds.
  for (IRFunction *H : Ordered) {
    OverlapLevel L;
    L.Head = H;
    L.FirstRound = (unsigned)T.Rounds.size();

    IRFunction *Issuer = H;
    IRFuncSetTy Visited;
    while (Issuer && !Issuer->Info.SpawnNextList.empty()) {
      if (!Visited.insert(Issuer).second)
        return std::nullopt; // cycle in the spawn_next chain; bail rather than
                             // emit a wrapper that cannot be wired
      OverlapRound R;
      R.Issuer = Issuer;
      R.Cont = *Issuer->Info.SpawnNextList.begin();
      auto ContIt = TaskInfos.find(R.Cont);
      if (ContIt == TaskInfos.end())
        return std::nullopt;

      IRFunction *Real = nullptr;
      for (auto &B : *Issuer) {
        for (auto &S : *B) {
          auto *ES = dyn_cast<ESpawnIRStmt>(S.get());
          if (!ES || !ES->SN || !ES->Fn)
            continue;
          // A dependent spawn of anything but a synthesised reader is real task
          // parallelism: the merge unit cannot complete a continuation from it,
          // so the chain ends here and the spawn becomes a wrapper output.
          if (!ES->Fn->Info.IsMemReader) {
            Real = ES->Fn;
            continue;
          }
          if (auto *Id = dyn_cast<IdentIRExpr>(ES->Dest.get())) {
            std::string FieldName = GetSym(Id->Ident->Name);
            if (auto RF = fieldOffset(R.Cont, ContIt->second, FieldName)) {
              R.Replies.push_back(*RF);
              R.MemReaders.push_back(ES->Fn);
            }
          }
        }
      }
      if (Real) {
        // This task consumes the previous round's replies but issues no round of
        // its own; it is the tail, handing the loop body to the scheduler.
        if (!T.Escapes || !R.MemReaders.empty())
          return std::nullopt;
        T.EscapeSpawn = Real;
        break;
      }
      if (R.MemReaders.empty() || R.Replies.size() != R.MemReaders.size())
        return std::nullopt;

      for (size_t k = 0; k < R.MemReaders.size(); k++) {
        uint64_t Bits = 0;
        auto MemIt = TaskInfos.find(R.MemReaders[k]);
        if (MemIt != TaskInfos.end() && MemIt->second.RetTy)
          Bits = (uint64_t)hardCilkTypeSize(MemIt->second.RetTy.get()) * 8;
        if (Bits == 0)
          Bits = R.Replies[k].WidthBits;
        R.ReplyDataBits.push_back(Bits);
      }

      T.Rounds.push_back(R);
      Issuer = R.Cont;
    }
    L.NumRounds = (unsigned)T.Rounds.size() - L.FirstRound;
    if (L.NumRounds == 0)
      return std::nullopt;
    L.Tail = Issuer;

    // The head's non-reader spawn target is this level's loop exit. A round now
    // has one reader per read site, so exclude all of them.
    for (IRFunction *G : H->Info.SpawnList)
      if (!G->Info.IsMemReader)
        L.ExitTarget = G;
    if (!L.ExitTarget)
      return std::nullopt;

    // What the tail hands the token to: its own head (this level closes here) or
    // the next level's head (nesting).
    for (IRFunction *G : L.Tail->Info.SpawnList)
      if (Grp.Internal.count(G))
        L.TailSpawn = G;

    // The tail does not always spawn a head directly. A loop body with an `if`
    // is split at the join, so the chain reaches the head through one or more
    // plain forwarding tasks (`<head>_afterif0` and friends), each of which
    // carries real work — the induction-variable increment lives there. Peel
    // them off into Relays and let TailSpawn name the head they lead to, so the
    // ring-closing logic downstream keeps its simple "tail spawns a head" shape.
    while (L.TailSpawn && !llvm::is_contained(Heads, L.TailSpawn)) {
      IRFunction *R = L.TailSpawn;
      // Anything with its own spawn_next is a round, not a relay: it would need
      // a state FIFO, a reader and a merge unit, none of which are built here.
      // Bail rather than emit a wrapper that silently drops it.
      if (!R->Info.SpawnNextList.empty() ||
          llvm::is_contained(L.Relays, R))
        return std::nullopt;
      L.Relays.push_back(R);
      L.TailSpawn = nullptr;
      for (IRFunction *G : R->Info.SpawnList)
        if (Grp.Internal.count(G))
          L.TailSpawn = G;
    }
    if (!L.TailSpawn && !L.Relays.empty())
      return std::nullopt; // relay chain leaves the group: not a closed ring

    // Split this head's internal predecessors into the one that enters the ring
    // and the one that closes it. On an escaping level there is no internal
    // predecessor at all — the wrapper's taskIn feeds the head directly.
    {
      std::vector<IRFunction *> &Ps = Preds[H];
      // The back edge is the predecessor reachable FROM this head; the entry is
      // the one that is not. `Tail` and this level's exit chain are downstream.
      IRFuncSetTy Down;
      std::deque<IRFunction *> Q{H};
      Down.insert(H);
      while (!Q.empty()) {
        IRFunction *F = Q.front();
        Q.pop_front();
        IRFuncSetTy Next(F->Info.SpawnList.begin(),
                                    F->Info.SpawnList.end());
        Next.insert(F->Info.SpawnNextList.begin(), F->Info.SpawnNextList.end());
        for (IRFunction *S : Next)
          if (Grp.Internal.count(S) && Down.insert(S).second)
            Q.push_back(S);
      }
      for (IRFunction *Pr : Ps) {
        if (Down.count(Pr))
          L.BackSrc = L.BackSrc ? L.BackSrc : Pr;
        else
          L.EntrySrc = L.EntrySrc ? L.EntrySrc : Pr;
      }
      // A single predecessor that is downstream means the ring closes on itself
      // and the level is entered from outside the wrapper (escaping case).
    }

    T.Levels.push_back(L);
  }

  if (T.Levels.empty())
    return std::nullopt;
  T.Reentry = T.Levels.front().Head;
  T.Exit = T.Levels.front().ExitTarget;
  T.Tail = T.Levels.front().Tail;
  if (T.Escapes && !T.EscapeSpawn)
    return std::nullopt;

  // The loop root is the OVERLAP loop's own initializer (the code before the
  // while: it reads the external closure, sets up loop state, and spawns the
  // reentry) — NOT the whole-program root. This is exactly computeOverlapGroups'
  // Entry, so the wrapper stays consistent with the task descriptor.
  if (!T.Escapes) {
    T.Root = T.Levels.front().EntrySrc;
    if (!T.Root && Grp.Entry && Grp.Entry != T.Reentry)
      T.Root = Grp.Entry;
    if (!T.Root)
      return std::nullopt;
  }
  return T;
}

// ─── AXI-Stream port helpers ─────────────────────────────────────────────────

// Vitis HLS names an `axis` port `p` as p_TDATA / p_TVALID / p_TREADY. We keep a
// single struct type per port, so the data bus is the full closure width.

static void emitAxisWire(llvm::raw_ostream &O, const std::string &Name,
                         uint64_t Width) {
  O << "  wire [" << (Width - 1) << ":0] " << Name << "_TDATA;\n";
  O << "  wire " << Name << "_TVALID;\n";
  O << "  wire " << Name << "_TREADY;\n";
}

// ─── AXI4 master (m_axi) passthrough ─────────────────────────────────────────
//
// A memory-accessing PE (`void *mem`) is synthesized by Vitis HLS with a flat
// AXI4 master bundle. We expose each such bundle straight through to the wrapper
// top so the surrounding system can connect it to memory. The signal set and
// widths mirror the Vitis-generated RTL.

struct AxiSig {
  const char *Name;
  bool MasterOut; // true: driven by the PE (master); becomes a wrapper output
  const char *W;  // width token, see axiWidth()
};

static const AxiSig AXI_SIGS[] = {
    {"AWVALID", true, "1"},   {"AWREADY", false, "1"},
    {"AWADDR", true, "ADDR"}, {"AWID", true, "ID"},
    {"AWLEN", true, "8"},     {"AWSIZE", true, "3"},
    {"AWBURST", true, "2"},   {"AWLOCK", true, "2"},
    {"AWCACHE", true, "4"},   {"AWPROT", true, "3"},
    {"AWQOS", true, "4"},     {"AWREGION", true, "4"},
    {"AWUSER", true, "1"},    {"WVALID", true, "1"},
    {"WREADY", false, "1"},   {"WDATA", true, "DATA"},
    {"WSTRB", true, "STRB"},  {"WLAST", true, "1"},
    {"WID", true, "ID"},      {"WUSER", true, "1"},
    {"ARVALID", true, "1"},   {"ARREADY", false, "1"},
    {"ARADDR", true, "ADDR"}, {"ARID", true, "ID"},
    {"ARLEN", true, "8"},     {"ARSIZE", true, "3"},
    {"ARBURST", true, "2"},   {"ARLOCK", true, "2"},
    {"ARCACHE", true, "4"},   {"ARPROT", true, "3"},
    {"ARQOS", true, "4"},     {"ARREGION", true, "4"},
    {"ARUSER", true, "1"},    {"RVALID", false, "1"},
    {"RREADY", true, "1"},    {"RDATA", false, "DATA"},
    {"RLAST", false, "1"},    {"RID", false, "ID"},
    {"RUSER", false, "1"},    {"RRESP", false, "2"},
    {"BVALID", false, "1"},   {"BREADY", true, "1"},
    {"BRESP", false, "2"},    {"BID", false, "ID"},
    {"BUSER", false, "1"},
};

// Verilog width prefix for a signal token, using the wrapper's AXI parameters.
static std::string axiWidth(const std::string &Tok,
                            const std::string &DataParam,
                            const std::string &IdParam = "MEM_ID_WIDTH") {
  if (Tok == "1")    return "";
  if (Tok == "8")    return "[7:0] ";
  if (Tok == "3")    return "[2:0] ";
  if (Tok == "2")    return "[1:0] ";
  if (Tok == "4")    return "[3:0] ";
  if (Tok == "ADDR") return "[MEM_ADDR_WIDTH-1:0] ";
  if (Tok == "ID")   return "[" + IdParam + "-1:0] ";
  if (Tok == "DATA") return "[" + DataParam + "-1:0] ";
  if (Tok == "STRB") return "[(" + DataParam + "/8)-1:0] ";
  return "";
}

// Width prefix for the shared reader's single m_axi master, whose ID and data
// widths are the wrapper's MEM_SHARED_* parameters rather than the per-PE ones.
static std::string sharedAxiWidth(const std::string &Tok) {
  if (Tok == "ID")
    return "[MEM_SHARED_ID_WIDTH-1:0] ";
  return axiWidth(Tok, "MEM_SHARED_DATA_WIDTH");
}

// ─── Shared-reader RTL library ───────────────────────────────────────────────
//
// The hand-written reader modules are shipped as library files under
// support/rtl/ (they are working, measured RTL with their own standalone
// testbench — see pe_perf_analysis/rtl) and appended verbatim to the wrapper
// output, with their module names prefixed by the wrapper name so that two
// OVERLAP wrappers in one design do not collide.

static std::string replaceAll(std::string S, const std::string &From,
                              const std::string &To) {
  for (size_t P = 0; (P = S.find(From, P)) != std::string::npos;
       P += To.size())
    S.replace(P, From.size(), To);
  return S;
}

static std::string readRtlLib(const std::string &File) {
  namespace fs = std::filesystem;
  fs::path Path = fs::path(BOMBYX_SUPPORT_DIR) / "rtl" / File;
  std::ifstream In(Path);
  if (!In) {
    PANIC("#pragma BOMBYX OVERLAP: cannot read shared-reader RTL library "
          "'%s'; the wrapper's read sites have nothing to serve them",
          Path.string().c_str());
  }
  std::stringstream SS;
  SS << In.rdbuf();
  return SS.str();
}

} // namespace

// ─── Wrapper emission ────────────────────────────────────────────────────────

bool PrintHardCilkOverlapWrapper(const std::string &AppName, IRProgram &P,
                                 const TaskInfosTy &TaskInfos,
                                 const OverlapGroup &Grp,
                                 llvm::raw_ostream &O) {
  auto TopoOpt = findOverlapTopology(P, TaskInfos, Grp);
  if (!TopoOpt)
    return false;
  OverlapTopology T = *TopoOpt;

  const HCTaskInfo &ReInfo = TaskInfos.at(T.Reentry);
  const HCTaskInfo &ExitInfo = TaskInfos.at(T.Exit);

  uint64_t W_RE = taskWidthBits(ReInfo);
  uint64_t W_EXIT = taskWidthBits(ExitInfo);
  // When the loop body escapes there is no internal root: the wrapper is
  // entered with the reentry's own closure, both from the loop initializer and
  // from the external continuation that closes the loop.
  uint64_t W_ROOT = T.Root ? taskWidthBits(TaskInfos.at(T.Root)) : W_RE;

  const unsigned NR = (unsigned)T.Rounds.size();
  // Per-round net/instance names. Round 0 keeps the unsuffixed names a
  // single-round loop has always used, so a one-round wrapper is emitted
  // exactly as before; later rounds are suffixed.
  auto sfx = [](unsigned K) {
    return K == 0 ? std::string() : "_" + std::to_string(K);
  };

  // One site of a round: its own reader PE, request port and reply channel.
  struct SiteGen {
    IRFunction *Mem;
    std::string MemName;
    uint64_t W_MEM, W_ARGDATA, ReplyBits;
    std::string Inst;    // u_memreader<S><suffix>
    std::string MemReq;  // issuer -> reader
    std::string MemData;  // reader.argDataOut -> reply FIFO
    std::string MemDataQ; // reply FIFO -> merge
    std::string MemArgOut;
  };
  struct RoundGen {
    const OverlapRound *R;
    std::string Cont;
    uint64_t W_CONT;
    unsigned N; // sites (== replies) in this round
    std::string S; // name suffix
    // Net names. Round 0 keeps the historical `re_*` names (its issuer really
    // is the reentry); later rounds are issued by the previous continuation.
    std::string StateIn, StateQ, ContIn;
    std::vector<SiteGen> Sites;
  };
  std::vector<RoundGen> RG;
  for (unsigned K = 0; K < NR; K++) {
    const OverlapRound &R = T.Rounds[K];
    const std::string S = sfx(K);
    RoundGen G;
    G.R = &R;
    G.Cont = R.Cont->getName();
    G.W_CONT = taskWidthBits(TaskInfos.at(R.Cont));
    G.N = (unsigned)R.Replies.size();
    G.S = S;
    G.StateIn = K == 0 ? "re_state" : "iss_state" + S;
    G.StateQ = "state_q" + S;
    G.ContIn = "cont_in" + S;
    for (unsigned k = 0; k < G.N; k++) {
      // Site 0 keeps the historical unsuffixed per-round net names, so a
      // single-site round is wired exactly as before.
      const std::string KS = k == 0 ? std::string() : "_s" + std::to_string(k);
      SiteGen SG;
      SG.Mem = R.MemReaders[k];
      SG.MemName = SG.Mem->getName();
      SG.W_MEM = taskWidthBits(TaskInfos.at(SG.Mem));
      SG.ReplyBits = R.ReplyDataBits[k];
      SG.W_ARGDATA = argOutWidthBits(SG.ReplyBits / 8);
      SG.Inst = "u_memreader" + S + KS;
      SG.MemReq = (K == 0 ? "re_mem" : "iss_mem" + S) + KS;
      SG.MemData = "mem_data" + S + KS;
      SG.MemDataQ = "mem_data" + S + KS + "_q";
      SG.MemArgOut = "mem_argOut" + S + KS;
      G.Sites.push_back(SG);
    }
    RG.push_back(std::move(G));
  }

  // ── The shared reader: every read site of every round on ONE m_axi master ──
  //
  // The per-site HLS memReader PEs are not instantiated. Instead all sites are
  // served by one hand-written Verilog reader (support/rtl): each site keeps its
  // own request/reply streams, multiplexed onto a single AXI master with
  // ARID = site index and per-stream credits, so no site can head-of-line block
  // another. Where the memory-dependence analysis proves a site's data is
  // read-only for the FPGA (computeReadOnlyReaders), a per-context line cache
  // sits in front of that site inside the reader; the owning context ID is
  // allocated by the wrapper's admission control (see the free queue below).
  //
  // Measured on triangleDAEOptimalCompact/synth_k16 (8 PEs): 0.165 cmp/cyc/PE
  // with one HLS reader+master per site, 0.464 with the shared cached reader —
  // 2.8x at a quarter of the memory masters.
  struct GlobalSite {
    const RoundGen *G;
    const SiteGen *SG;
    unsigned Round;   // index into RG
    unsigned Level;   // index into T.Levels
    uint64_t ElemLog2, IdxW;
    bool Cached;
  };
  std::vector<GlobalSite> Sites;
  {
    // Level of each round (rounds are stored level 0 first).
    std::vector<unsigned> RoundLevel(NR, 0);
    for (unsigned L = 0; L < (unsigned)T.Levels.size(); L++)
      for (unsigned K = T.Levels[L].FirstRound;
           K < T.Levels[L].FirstRound + T.Levels[L].NumRounds; K++)
        RoundLevel[K] = L;

    const IRFuncSetTy ReadOnly = computeReadOnlyReaders(P);
    const unsigned Innermost = (unsigned)T.Levels.size() - 1;
    for (unsigned K = 0; K < NR; K++)
      for (const SiteGen &SG : RG[K].Sites) {
        GlobalSite S;
        S.G = &RG[K];
        S.SG = &SG;
        S.Round = K;
        S.Level = RoundLevel[K];
        if (SG.ReplyBits < 8 || !llvm::isPowerOf2_64(SG.ReplyBits)) {
          PANIC("#pragma BOMBYX OVERLAP: read site '%s' has a %llu-bit "
                "element, which the shared reader cannot slice from a line",
                SG.MemName.c_str(), (unsigned long long)SG.ReplyBits);
        }
        S.ElemLog2 = llvm::Log2_64(SG.ReplyBits / 8);
        // The reader forms `base + (idx << ElemLog2)`; idx is the closure's
        // 64-bit index field, of which only the bits that survive the shift
        // are meaningful.
        S.IdxW = 64 - S.ElemLog2;
        // CACHE_EN: read-only proven by the whole-program dependence pass, and
        // the site sits on the innermost loop level. The innermost gate is a
        // payoff heuristic, not a correctness condition: the inner sites are
        // the ones a resident merge loop walks repeatedly (85 % of traffic in
        // triangleDAE, 80–88 % hit), while outer-level sites run once per
        // residency and would only burn BRAM lines. An escaping wrapper has no
        // internal admission control, so it has no context IDs to index a
        // cache with and always takes the uncached reader.
        //
        // The ctx FIFOs also require the instrumented ring to close on itself
        // (tail spawns its own head): a ring closed through a NESTED level
        // returns tokens out of order, which would desynchronise the ID FIFOs.
        // The innermost level always closes on itself, but keep the check as a
        // guard against future topologies.
        // Relays.empty() as well: the ctx FIFOs were reasoned about for a ring
        // whose tail spawns the head directly. A relay chain keeps the ring
        // one-in-one-out, so the IDs would still track, but silently switching
        // a design to the cached reader is not this path's job — leave a
        // branching loop body on the uncached one until it is measured.
        S.Cached = !T.Escapes && S.Level == Innermost &&
                   T.Levels[Innermost].Relays.empty() &&
                   T.Levels[Innermost].TailSpawn == T.Levels[Innermost].Head &&
                   ReadOnly.count(SG.Mem);
        Sites.push_back(S);
      }
  }
  const unsigned NSites = (unsigned)Sites.size();
  bool AnyCache = false;
  for (const GlobalSite &S : Sites)
    AnyCache |= S.Cached;
  // Levels that need the context-ID plumbing (free queue, widened FIFOs, the
  // ctx side-FIFOs). Only a level that owns a cached site pays for it.
  std::vector<bool> LevelInstr(T.Levels.size(), false);
  for (const GlobalSite &S : Sites)
    if (S.Cached)
      LevelInstr[S.Level] = true;

  // The reader has ONE closure layout and ONE reply-packet width across its
  // sites; every synthesised memReader shares them by construction ( _cont /
  // base / idx at fixed offsets, reply packet power-of-two padded).
  const uint64_t RD_TASK_W = Sites.front().SG->W_MEM;
  const uint64_t RD_ARG_W = Sites.front().SG->W_ARGDATA;
  for (const GlobalSite &S : Sites) {
    if (S.SG->W_MEM != RD_TASK_W || S.SG->W_ARGDATA != RD_ARG_W) {
      PANIC("#pragma BOMBYX OVERLAP: read sites disagree on closure/reply "
            "width (%llu/%llu vs %llu/%llu); the shared reader needs one "
            "layout",
            (unsigned long long)RD_TASK_W, (unsigned long long)RD_ARG_W,
            (unsigned long long)S.SG->W_MEM,
            (unsigned long long)S.SG->W_ARGDATA);
    }
  }
  // Cached: the bus is one cache line = the 256-bit HBM width, so a fill is a
  // single beat. Uncached: the widest element across the sites.
  uint64_t RD_BUS_W = 256;
  if (!AnyCache) {
    RD_BUS_W = 0;
    for (const GlobalSite &S : Sites)
      RD_BUS_W = std::max(RD_BUS_W, S.SG->ReplyBits);
  }
  // ARID = site index. The exported port takes this width verbatim (the
  // architecture generator reads it from the wrapper), and its export config
  // widens to the bucket's widest master — NEVER let a PE-side ID exceed the
  // exported wId, or the protocol converter inserts one IdSerialize per
  // downstream ID (2 outstanding reads each; measured 4.2x throughput loss).
  const unsigned RD_ID_W = std::max(1u, llvm::Log2_32_Ceil(NSites));
  // Per-site context-FIFO head net, for the sites that have one.
  auto siteCtxHead = [](const SiteGen &SG) { return SG.MemReq + "_ctx_head"; };

  const std::string TopName = Grp.WrapperName;
  const std::string Root = T.Root ? T.Root->getName() : std::string();
  const std::string Re = T.Reentry->getName();
  const std::string Exit = T.Exit->getName();
  // The task that closes the loop back to the reentry: the last continuation
  // in the chain.
  const std::string Tail = T.Tail->getName();
  const std::string TailInst = "u_cont" + sfx(NR - 1);

  // Per-level net/instance naming. Level 0 keeps the historical unsuffixed names
  // a single-level loop has always used, so a one-level wrapper is emitted
  // byte-for-byte as before; inner levels are suffixed.
  const unsigned NL = (unsigned)T.Levels.size();
  auto lsfx = [](unsigned L) {
    return L == 0 ? std::string() : "_l" + std::to_string(L);
  };
  // Wire that feeds level L's ring from outside it (root PE for level 0, the
  // enclosing level's tail for an inner level).
  auto entryW = [&](unsigned L) {
    return L == 0 ? std::string("root_out") : "lvlin" + lsfx(L);
  };
  auto inW = [&](unsigned L) { return "re_in" + lsfx(L); };
  auto loopW = [&](unsigned L) { return "cont_loop" + lsfx(L); };
  // Feed of the i-th relay PE on the path from level L's tail to its head.
  auto relayW = [&](unsigned L, unsigned I) {
    return "relay" + lsfx(L) + "_" + std::to_string(I);
  };
  auto loopQW = [&](unsigned L) { return "cont_loop_q" + lsfx(L); };
  auto exitW = [&](unsigned L) { return "re_exit" + lsfx(L); };
  auto headWidth = [&](unsigned L) {
    return taskWidthBits(TaskInfos.at(T.Levels[L].Head));
  };
  // The level whose head a given node tail-spawns, or NL if none.
  auto levelOfHead = [&](IRFunction *F) {
    for (unsigned L = 0; L < NL; L++)
      if (T.Levels[L].Head == F)
        return L;
    return NL;
  };
  // A task that neither spawns nor owns a spawn_next is a leaf: it reports its
  // result through argOut/argDataOut rather than handing on a token.
  auto isLeaf = [](IRFunction *F) {
    return F->Info.SpawnList.empty() && F->Info.SpawnNextList.empty();
  };

  // ── Exit task's argOut/argDataOut destinations ─────────────────────────────
  // The exit routes its return value to one of several continuations by the
  // closure's _cont tag (see the HLS `if (_cont_tag == <DEST>_TAG)` chain). Each
  // destination D has ports named `argOut_<D>`/`argDataOut_<D>` when the exit has
  // more than one send target (else the plain `argOut`/`argDataOut`), matching
  // VitisHLSTarget's emission. Destinations inside the collapsed loop are sunk;
  // destinations outside it are the real loop result and are exposed on the top.
  // The top ports follow the same convention among the EXTERNAL set only, so a
  // single external destination keeps the generic `argOut`/`argDataOut` names.
  struct ExitDest {
    IRFunction *F;
    std::string ExitArgOut;  // port name on the exit PE
    std::string ExitArgData;
    bool External;
    std::string TopArgOut;   // top-level port name (external destinations only)
    std::string TopArgData;
  };
  const bool ExitHasArgData =
      T.Exit->Info.SpawnList.empty() && T.Exit->Info.SpawnNextList.empty() &&
      !ExitInfo.SendArgList.empty() &&
      ((ExitInfo.RetTy && !typeIsVoid(*ExitInfo.RetTy)) ||
       ExitInfo.GenerateArgOutWriteBuffer);
  uint64_t exitRetBytes =
      (ExitInfo.RetTy && !typeIsVoid(*ExitInfo.RetTy))
          ? (uint64_t)hardCilkTypeSize(ExitInfo.RetTy.get())
          : (uint64_t)ExitInfo.BufferedArgumentBits / 8;
  uint64_t W_EXITARG = argOutWidthBits(exitRetBytes);

  const bool MultiExit = ExitInfo.SendArgList.size() > 1;
  std::vector<IRFunction *> ExitDestFns(ExitInfo.SendArgList.begin(),
                                        ExitInfo.SendArgList.end());
  std::sort(ExitDestFns.begin(), ExitDestFns.end(),
            [](IRFunction *A, IRFunction *B) {
              return A->getName() < B->getName();
            });
  unsigned ExtCount = 0;
  for (IRFunction *D : ExitDestFns)
    if (!T.Internal.count(D))
      ExtCount++;
  const bool MultiExt = ExtCount > 1;

  std::vector<ExitDest> ExitDests;
  for (IRFunction *D : ExitDestFns) {
    const std::string DN = D->getName();
    ExitDest ED;
    ED.F = D;
    ED.ExitArgOut = MultiExit ? "argOut_" + DN : "argOut";
    ED.ExitArgData = MultiExit ? "argDataOut_" + DN : "argDataOut";
    ED.External = !T.Internal.count(D);
    ED.TopArgOut = MultiExt ? "argOut_" + DN : "argOut";
    ED.TopArgData = MultiExt ? "argDataOut_" + DN : "argDataOut";
    ExitDests.push_back(ED);
  }
  // An escaping wrapper does not instantiate the exit: the reentry's spawn of
  // it leaves through a top-level task port and the scheduler delivers it.
  if (T.Escapes)
    ExitDests.clear();

  // Widths of the escaping wrapper's hand-off ports.
  uint64_t W_ESCSPAWN = 0, W_ESCSN = 0;
  if (T.Escapes) {
    W_ESCSPAWN = taskWidthBits(TaskInfos.at(T.EscapeSpawn));
    const HCTaskInfo &ECI = TaskInfos.at(T.EscapeCont);
    unsigned Beats = closureWriteBeats(ECI);
    uint64_t DataBytes = Beats > 1 ? closureBeatBytes(ECI)
                                   : (uint64_t)(ECI.TaskSize + ECI.TaskPadding);
    W_ESCSN = argOutWidthBits(DataBytes);
  }
  // Per-destination internal wire base names (unique regardless of dedup).
  auto exitWire = [&](const ExitDest &ED) {
    return "exit_" + ED.F->getName();
  };

  // ── File header ────────────────────────────────────────────────────────────
  O << "// ===========================================================================\n";
  O << "// Generated by bombyx-cc — #pragma BOMBYX OVERLAP Verilog wrapper\n";
  O << "// Application : " << AppName << "\n";
  O << "//\n";
  O << "// In-order streaming continuation delivery. The reentry PE pushes each\n";
  O << "// iteration's continuation state onto STATE_FIFO and issues " << RG[0].N
    << " memory\n";
  O << "// read(s); the single in-order memReader reply channel is gathered "
    << RG[0].N << " at a\n";
  O << "// time and merged with the head of STATE_FIFO to drive the continuation PE.\n";
  if (NR > 1) {
    O << "//\n";
    O << "// The loop body has " << NR << " dependence levels of loads, so the chain is\n";
    O << "// " << Re;
    for (auto &G : RG)
      O << " -> " << G.Cont;
    O << ", each stage issuing its own round of reads.\n";
    O << "// Every round is an independent in-order reply channel with its own\n";
    O << "// memReader, STATE_FIFO and merge unit; no arbitration or reply tagging is\n";
    O << "// needed, and rounds whose element widths differ are served naturally.\n";
  }
  O << "//\n";
  O << "// PE AXI-Stream ports follow the Vitis HLS convention: for an `axis` port\n";
  O << "// `p`, the RTL exposes p_TDATA / p_TVALID / p_TREADY, plus ap_clk / ap_rst_n.\n";
  O << "//\n";
  O << "// External interface (everything not internal to the collapsed tasks):\n";
  O << "//   * taskIn_*  — root task closure in (== root PE's taskIn)\n";
  O << "//   * argOut_*  — loop completion out (== exit PE's argOut)\n";
  O << "//   * m_axi_gmem_shared_* — ONE AXI4 memory master serving every read site\n";
  O << "//     (ARID = site index), via the shared "
    << (AnyCache ? "context-cached " : "") << "reader below\n";
  O << "//   * m_axi_gmem_<pe>_* — one AXI4 memory master per remaining\n";
  O << "//     memory-accessing PE (inline loads/stores), passed straight through\n";
  O << "//     (Vitis default bundle name `gmem`). The per-PE s_axi_control offset\n";
  O << "//     slave is tied idle (offset 0).\n";
  O << "// ===========================================================================\n\n";

  // ── Generic FIFO ───────────────────────────────────────────────────────────
  // The storage array is read SYNCHRONOUSLY so it infers a block RAM. An
  // asynchronous read (`assign out_data = mem[rptr]`) forces distributed RAM:
  // at the depth/width these FIFOs run (the loopback and state FIFOs each hold
  // a full task closure per in-flight iteration) that is thousands of LUTs, and
  // the write/read pointers then fan out to every one of those LUTRAM address
  // pins. Vivado declines to replicate such nets, leaving one dense unroutable
  // blob -- the design congests and route_design fails with node overlaps at
  // single-digit device utilization. Block RAM keeps the pointers at a fanout
  // of a handful of primitives.
  //
  // Synchronous read costs a cycle, so a one-entry output register restores
  // first-word fall-through: `out_valid`/`out_data` present the head without a
  // read request, exactly as the async-read version did. Both consumers (the
  // reentry arbiter and the merge unit) are latency-insensitive valid/ready
  // handshakes, and `out_data` stays stable while `out_valid && !out_ready`,
  // which the merge unit relies on to hold the state head across all NREPLY
  // gathers. Capacity becomes DEPTH+1 (array + output register), so it never
  // drops below the depth the admission-control credit bound assumes.
  uint64_t FifoMaxWidth = W_RE;
  for (auto &G : RG)
    FifoMaxWidth = std::max(FifoMaxWidth, G.W_CONT);
  // Inner levels' loop-back FIFOs carry their own head's closure.
  for (unsigned L = 1; L < NL; L++)
    FifoMaxWidth = std::max(FifoMaxWidth, headWidth(L));
  const uint64_t FifoDefaultDepth = 512;
  // Only force block RAM when the array is big enough to be worth a BRAM;
  // small FIFOs are cheaper and faster left in LUTRAM.
  const bool FifoUseBlockRAM = (FifoMaxWidth * FifoDefaultDepth) >= 4096;
  O << "module " << TopName << "_fifo #(\n";
  O << "  parameter WIDTH = 32,\n";
  O << "  parameter DEPTH = 512\n";
  O << ") (\n";
  O << "  input                  clk,\n";
  O << "  input                  rst_n,\n";
  O << "  input  [WIDTH-1:0]     in_data,\n";
  O << "  input                  in_valid,\n";
  O << "  output                 in_ready,\n";
  O << "  output [WIDTH-1:0]     out_data,\n";
  O << "  output                 out_valid,\n";
  O << "  input                  out_ready\n";
  O << ");\n";
  O << "  localparam AW = $clog2(DEPTH); // DEPTH must be a power of two\n";
  if (FifoUseBlockRAM)
    O << "  (* ram_style = \"block\" *)\n";
  O << "  reg [WIDTH-1:0] mem [0:DEPTH-1];\n";
  O << "  reg [AW:0] wptr, rptr;\n";
  O << "  wire ram_empty = (wptr == rptr);\n";
  // ── Registered in_ready ────────────────────────────────────────────────────
  // in_ready used to be `!full`, i.e. a comparison of the read and write
  // pointers driven straight out of the module. On randomWalk_overlap that made
  // rptr_reg -> LUT6 -> LUT4 -> contStateOut TREADY the head of EVERY one of the
  // 5000 worst setup paths (WNS -0.480 at 300 MHz): the PE turns that TREADY
  // into its pipeline's global stall term, which then fans out to the clock
  // enable of every pipeline-stage register (measured fo=650). Two levels of
  // combinational logic and ~0.6 ns of route were spent before the stall term
  // even existed. Driving in_ready from a flop hands the PE a full cycle of that
  // budget back; it is the same reason mrc_skid was written with a flopped
  // i_ready.
  //
  // The flop must be conservative, since the producer sees a ready that is one
  // cycle stale: assert it when the occupancy AFTER this cycle would reach
  // DEPTH-1, so a push that slips through on the stale value can only take the
  // array to DEPTH-1. The array therefore holds at most DEPTH-1 entries instead
  // of DEPTH, and total capacity (array + output register) is DEPTH rather than
  // DEPTH+1 -- still not below the DEPTH that the admission-control credit bound
  // assumes, which is the property the header above is protecting.
  O << "  reg  ready_r;\n";
  O << "  assign in_ready = ready_r;\n";
  O << "  wire push = in_valid && in_ready;\n\n";
  O << "  // Output register: holds the head so the RAM's registered read is hidden.\n";
  O << "  reg [WIDTH-1:0] outreg;\n";
  O << "  reg             outreg_v;\n";
  O << "  // Refill whenever the array has data and the output register is free.\n";
  O << "  // Gating on !ram_empty also guarantees the read address never equals a\n";
  O << "  // write address in the same cycle, so there is no read/write collision.\n";
  O << "  wire pop = !ram_empty && (!outreg_v || out_ready);\n";
  O << "  assign out_valid = outreg_v;\n";
  O << "  assign out_data  = outreg;\n\n";
  O << "  // Occupancy of the storage array at the end of this cycle; see the\n";
  O << "  // registered-in_ready note above.\n";
  O << "  wire [AW:0] occ_next = (wptr + {{AW{1'b0}}, push}) -\n";
  O << "                         (rptr + {{AW{1'b0}}, pop});\n\n";
  O << "  // Storage: no reset, so this infers a block RAM cleanly.\n";
  O << "  always @(posedge clk) begin\n";
  O << "    if (push) mem[wptr[AW-1:0]] <= in_data;\n";
  O << "    if (pop)  outreg <= mem[rptr[AW-1:0]];\n";
  O << "  end\n\n";
  O << "  always @(posedge clk) begin\n";
  O << "    if (!rst_n) begin\n";
  O << "      wptr <= 0; rptr <= 0; outreg_v <= 1'b0; ready_r <= 1'b0;\n";
  O << "    end else begin\n";
  O << "      if (push) wptr <= wptr + 1'b1;\n";
  O << "      if (pop)  rptr <= rptr + 1'b1;\n";
  O << "      if (pop)            outreg_v <= 1'b1;\n";
  O << "      else if (out_ready) outreg_v <= 1'b0;\n";
  // `occ_next < DEPTH-1`, written without a DEPTH-wide constant so it needs no
  // width cast (a `localparam [AW:0] = DEPTH-1` truncates a 32-bit integer and
  // Verilator rejects it, which would take the SystemC harness out of service).
  // DEPTH is a power of two, so occ_next == DEPTH sets the top bit and
  // occ_next == DEPTH-1 sets every bit below it.
  O << "      ready_r <= !(occ_next[AW] | (&occ_next[AW-1:0]));\n";
  O << "    end\n";
  O << "  end\n";
  O << "endmodule\n\n";

  if (!T.Escapes) {
    // ── 2:1 round-robin stream arbiter (loopback + initial into reentry) ────────
    O << "module " << TopName << "_arb2 #(parameter WIDTH = 32) (\n";
    O << "  input               clk,\n";
    O << "  input               rst_n,\n";
    O << "  input  [WIDTH-1:0]  a_data,  input a_valid,  output a_ready,\n";
    O << "  input  [WIDTH-1:0]  b_data,  input b_valid,  output b_ready,\n";
    O << "  output [WIDTH-1:0]  o_data,  output o_valid, input  o_ready\n";
    O << ");\n";
    O << "  reg sel; // 0: prefer a, 1: prefer b\n";
    O << "  wire pick_a = a_valid && (!b_valid || sel == 1'b0);\n";
    O << "  assign o_valid = a_valid || b_valid;\n";
    O << "  assign o_data  = pick_a ? a_data : b_data;\n";
    O << "  assign a_ready = pick_a && o_ready;\n";
    O << "  assign b_ready = !pick_a && o_valid && o_ready;\n";
    O << "  always @(posedge clk) if (!rst_n) sel <= 0;\n";
    O << "    else if (o_valid && o_ready) sel <= pick_a ? 1'b1 : 1'b0;\n";
    O << "endmodule\n\n";
  }

  // ── Shared-reader library modules ──────────────────────────────────────────
  // Appended verbatim from support/rtl (working, measured RTL — see the
  // standalone testbench in pe_perf_analysis/rtl), with the module names
  // prefixed per wrapper so two OVERLAP wrappers in one design do not collide.
  // The skid buffer is used by the wrapper TOP (inbound task stream, outbound
  // result streams) regardless of which shared-reader flavour the readers use,
  // so it must be emitted unconditionally. It used to live inside
  // bombyx_mem_reader_cached.v, which meant a wrapper whose readers were all
  // uncached instantiated <top>_mrc_skid without ever defining it and failed
  // Vivado elaboration with "Module not found". randomWalk_overlap hit this;
  // pageRank_overlap did not, only because its reader is cached.
  {
    std::string Skid = readRtlLib("bombyx_mrc_skid.v");
    Skid = replaceAll(Skid, "bombyx_mrc_skid", TopName + "_mrc_skid");
    O << Skid << "\n";
  }

  if (AnyCache) {
    std::string Lib = readRtlLib("bombyx_mem_reader_cached.v") +
                      readRtlLib("bombyx_ctx_cache.v");
    Lib = replaceAll(Lib, "bombyx_mem_reader_cached",
                     TopName + "_memreader_cached");
    Lib = replaceAll(Lib, "bombyx_ctx_cache", TopName + "_ctx_cache");
    Lib = replaceAll(Lib, "bombyx_mrc_fifo", TopName + "_mrc_fifo");
    // The skid MODULE is emitted above under the per-wrapper name; the reader
    // instantiates it by its library name, so those instantiations have to be
    // renamed too. Without this the wrapper instantiates `bombyx_mrc_skid`
    // while only defining `<top>_mrc_skid`, and elaboration fails with
    // "Module not found" (Verilator: MODMISSING).
    Lib = replaceAll(Lib, "bombyx_mrc_skid", TopName + "_mrc_skid");
    O << Lib << "\n";
  } else {
    std::string Lib = readRtlLib("bombyx_mem_reader_shared.v");
    Lib = replaceAll(Lib, "bombyx_mem_reader_shared",
                     TopName + "_memreader_shared");
    Lib = replaceAll(Lib, "bombyx_msr_fifo", TopName + "_msr_fifo");
    Lib = replaceAll(Lib, "bombyx_mrc_skid", TopName + "_mrc_skid");
    O << Lib << "\n";
  }

  // Collect the collapsed PEs that expose an AXI4 memory master, so their
  // bundles can be surfaced to the wrapper top. Each is passed through under a
  // unique, PE-named prefix (`m_axi_mem_<task>`).
  struct MemPE {
    IRFunction *F;
    std::string Inst;      // instance label in the wrapper
    std::string TopPrefix; // top-level port prefix
    std::string DataParam; // this PE's own AXI data-width parameter
    std::string IdParam;   // this PE's own AXI ID-width parameter
  };
  std::vector<MemPE> MemPEs;
  // Each collapsed mem PE gets its OWN AXI data-width parameter,
  // MEM_DATA_WIDTH_<pe>. The compiler no longer forces max_widen_bitwidth on the
  // sub-PEs, so Vitis HLS synthesizes each at its natural m_axi width. The
  // parameter default below is a placeholder; build_hls.sh reads the synthesized
  // C_M_AXI_GMEM_DATA_WIDTH from each PE's RTL and patches the matching parameter
  // here post-synthesis, so the wrapper's master width equals the collapsed PE's.
  auto memDataParam = [](IRFunction *F) {
    return "MEM_DATA_WIDTH_" + F->getName();
  };
  // ... and its own ID-width parameter, for the same reason. A PE's AXI ID
  // width follows its channel count (planMemChannels): Vitis HLS synthesises
  // C_M_AXI_GMEM_ID_WIDTH = clog2(channels), so a PE with 3 read sites drives a
  // 2-bit ARID. A fixed 1-bit wrapper port silently truncates it, R beats come
  // back tagged with the wrong channel, and the PE hangs on its first read --
  // which is exactly what a third read site in pageRank's applyFn produced.
  // build_hls.sh patches this from the synthesized value, like the data width.
  auto memIdParam = [](IRFunction *F) {
    return "MEM_ID_WIDTH_" + F->getName();
  };
  auto considerMem = [&](IRFunction *F, const std::string &Inst) {
    auto It = TaskInfos.find(F);
    if (It != TaskInfos.end() && It->second.HasAXI)
      MemPEs.push_back({F, Inst, "m_axi_gmem_" + F->getName(), memDataParam(F),
                        memIdParam(F)});
  };
  // Order matters only in that it fixes the top-level port order; walking the
  // levels reproduces the historical root/reentry/rounds/exit order exactly when
  // there is a single level.
  considerMem(T.Root, "u_root");
  for (unsigned L = 0; L < NL; L++) {
    const OverlapLevel &Lv = T.Levels[L];
    considerMem(Lv.Head, "u_reentry" + lsfx(L));
    // Read sites are served by the single shared reader on its own master —
    // no per-site memReader PE, so no per-site m_axi bundle on the top.
    for (unsigned K = Lv.FirstRound; K < Lv.FirstRound + Lv.NumRounds; K++)
      considerMem(T.Rounds[K].Cont, "u_cont" + sfx(K));
    for (unsigned I = 0; I < Lv.Relays.size(); I++)
      considerMem(Lv.Relays[I], "u_relay" + lsfx(L) + "_" + std::to_string(I));
  }
  // Only PEs the wrapper actually INSTANTIATES may surface a bundle. An
  // escaping wrapper does not instantiate its exit — the exit spawn leaves on a
  // top-level task port for the scheduler to deliver — so surfacing that PE's
  // AXI master would put an undriven m_axi bundle on the kernel boundary.
  if (!T.Escapes)
    for (unsigned L = 0; L < NL; L++)
      considerMem(T.Levels[L].ExitTarget, "u_exit" + lsfx(L));

  // ── Top wrapper ────────────────────────────────────────────────────────────
  // External ports mirror the interface the collapsed subsystem would present
  // as a single task: `taskIn` (root closure in), `argOut` (completion out), and
  // one AXI4 memory master per memory-accessing PE. All streams internal to the
  // collapsed tasks are wired privately below.
  O << "module " << TopName << " #(\n";
  // With the context cache in front of the merge-loop sites the throughput
  // optimum moves from shallow to 128 (measured: 64 -> 0.278, 128 -> 0.464,
  // 256 -> 0.458, 512 -> 0.332 cmp/cyc/PE) — past 128 the extra resident loops
  // re-saturate the port and round-trip latency climbs again.
  O << "  parameter STATE_DEPTH     = " << (AnyCache ? 128 : 64) << ",\n";
  // Reply-skew buffering, see the reply FIFO instantiation below.
  // Measured on pageRank_overlap with the SystemC/Verilator harness in
  // tests/systemc/pagerank (V=1024, 32- and 128-cycle memory latency, steady-
  // state contributions/PE/cycle):
  //   REPLY_DEPTH            8..128 all identical -> 16 (was 64)
  //   MEM_SHARED_OUTSTANDING 8:0.223  16:0.441  32:0.479  64:0.479  128:0.479
  //                          -> 32 is the knee (was 64)
  //   STATE_DEPTH            16:0.049 32:0.137 64:0.299 128:0.479 256:0.479
  //                          -> 128 is the knee, and it still is at 128-cycle
  //                             latency (128:0.2359 vs 256:0.2365), so the
  //                             doubled context cache and state FIFOs that 256
  //                             costs buy 0.3%.
  // The two reductions are free throughput-wise and cut the reply FIFOs 4x and
  // the per-stream miss FIFO 2x.
  O << "  parameter REPLY_DEPTH     = 16,\n";
  // Decouples the reentry PE from the exit PE. The exit PE is the one stage a
  // task passes through exactly once, and it is typically the slowest: it
  // stores the result and then RELOADS it to compute the reported delta, so
  // each task costs it a full store->load memory round trip and it accepts a
  // new task only every ~130 cycles. Without a queue here that stall reaches
  // straight back into the reentry PE -- which is a single II=1 pipeline
  // serving BOTH the loop-continue and the loop-exit paths -- so one retiring
  // task freezes the whole merge ring. Measured on pageRank_overlap: re_exit
  // backpressured 87% of cycles and every downstream stage starved ~95%.
  // 2 already saturates now that the exit PE is II=2 rather than II=142; 16 is
  // cheap insurance for a kernel whose exit stage is slower.
  O << "  parameter EXIT_DEPTH      = 16,\n";
  O << "  parameter MEM_ADDR_WIDTH  = 64,\n";
  O << "  parameter MEM_ID_WIDTH    = 1,\n";
  // The shared reader's one master. ID width covers one ARID per read site;
  // data width is one cache line (the HBM width) when any site is cached, else
  // the widest element. OUTSTANDING is the per-stream AXI credit, which also
  // bounds each stream's share of the reply storage.
  O << "  parameter MEM_SHARED_ID_WIDTH    = " << RD_ID_W << ",\n";
  O << "  parameter MEM_SHARED_DATA_WIDTH  = " << RD_BUS_W << ",\n";
  O << "  parameter MEM_SHARED_OUTSTANDING = 32";
  if (AnyCache)
    // The completion queue holds one slot per in-flight request of a stream.
    // A merge loop is strictly serial per context -- it cannot issue its next
    // read until the previous reply is delivered -- so a stream can never have
    // more requests in flight than there are resident contexts, and NUM_CTX is
    // STATE_DEPTH. Anything above that is storage the design cannot reach:
    // measured peak occupancy with CQ_DEPTH=256 and a 128-cycle memory latency
    // was exactly 128 on the cached streams (and OUTSTANDING+2 on the others).
    O << ",\n  parameter CQ_DEPTH        = STATE_DEPTH";
  // One AXI data-width parameter per collapsed mem PE. The default is a
  // placeholder patched by build_hls.sh to the synthesized C_M_AXI_GMEM_DATA_WIDTH.
  for (auto &M : MemPEs) {
    O << ",\n  parameter " << M.DataParam << " = 32";
    O << ",\n  parameter " << M.IdParam << " = 1";
  }
  O << "\n) (\n";

  // Build the port list, then emit comma-joined so there is never a trailing
  // comma regardless of how many memory bundles are present.
  std::vector<std::string> Ports;
  Ports.push_back("input                       ap_clk");
  Ports.push_back("input                       ap_rst_n");
  Ports.push_back(
      "// Root task closure injected by the host / upstream scheduler.\n"
      "  input  [" + std::to_string(W_ROOT - 1) + ":0]  taskIn_TDATA");
  Ports.push_back("input                       taskIn_TVALID");
  Ports.push_back("output                      taskIn_TREADY");
  // Loop result out: for every EXTERNAL exit destination, an argOut (closure
  // address) and, when the exit carries a value/buffered store, an argDataOut
  // (the return payload). Internal destinations stay inside the wrapper.
  bool FirstExt = true;
  for (auto &ED : ExitDests) {
    if (!ED.External)
      continue;
    std::string Lead = FirstExt ? "// Loop result out (exit task -> external "
                                  "continuation).\n  "
                                : "";
    FirstExt = false;
    Ports.push_back(Lead + "output [63:0]               " + ED.TopArgOut +
                    "_TDATA");
    Ports.push_back("output                      " + ED.TopArgOut + "_TVALID");
    Ports.push_back("input                       " + ED.TopArgOut + "_TREADY");
    if (ExitHasArgData) {
      Ports.push_back("output [" + std::to_string(W_EXITARG - 1) +
                      ":0]              " + ED.TopArgData + "_TDATA");
      Ports.push_back("output                      " + ED.TopArgData +
                      "_TVALID");
      Ports.push_back("input                       " + ED.TopArgData +
                      "_TREADY");
    }
  }
  if (T.Escapes) {
    // Loop exit: spawned by the reentry, delivered by the scheduler.
    Ports.push_back(
        "// Loop exit task out (reentry -> '" + Exit + "').\n"
        "  output [" + std::to_string(W_EXIT - 1) + ":0]  taskGlobalOut_" +
        Exit + "_TDATA");
    Ports.push_back("output                      taskGlobalOut_" + Exit +
                    "_TVALID");
    Ports.push_back("input                       taskGlobalOut_" + Exit +
                    "_TREADY");
    // Hand-off of the loop body to a real spawned task, with its continuation
    // allocated and released through the ordinary scheduler mechanics.
    const std::string SpawnPort = "taskGlobalOut_" + T.EscapeSpawn->getName() +
                                  "_depends_" + T.EscapeCont->getName();
    Ports.push_back(
        "// Real dependent spawn out ('" + T.EscapeSpawn->getName() +
        "'), with its continuation '" + T.EscapeCont->getName() + "'.\n"
        "  output [" + std::to_string(W_ESCSPAWN - 1) + ":0]  " + SpawnPort +
        "_TDATA");
    Ports.push_back("output                      " + SpawnPort + "_TVALID");
    Ports.push_back("input                       " + SpawnPort + "_TREADY");
    Ports.push_back("input  [63:0]               closureIn_TDATA");
    Ports.push_back("input                       closureIn_TVALID");
    Ports.push_back("output                      closureIn_TREADY");
    const std::string SNPort = "spawnNext_" + T.EscapeCont->getName();
    Ports.push_back("output [" + std::to_string(W_ESCSN - 1) + ":0]  " +
                    SNPort + "_TDATA");
    Ports.push_back("output                      " + SNPort + "_TVALID");
    Ports.push_back("input                       " + SNPort + "_TREADY");
  }
  {
    // The one memory master serving every read site (ARID = site index).
    bool First = true;
    for (auto &S : AXI_SIGS) {
      std::string Decl = std::string(S.MasterOut ? "output " : "input  ") +
                         sharedAxiWidth(S.W) + "m_axi_gmem_shared_" + S.Name;
      if (First) {
        Decl = "// Shared AXI4 memory master for all " +
               std::to_string(NSites) + " read site(s).\n  " + Decl;
        First = false;
      }
      Ports.push_back(Decl);
    }
  }
  for (auto &M : MemPEs) {
    bool First = true;
    for (auto &S : AXI_SIGS) {
      std::string Decl = std::string(S.MasterOut ? "output " : "input  ") +
                         axiWidth(S.W, M.DataParam, M.IdParam) +
                         M.TopPrefix + "_" + S.Name;
      if (First) {
        Decl = "// AXI4 memory master for PE '" + M.F->getName() + "'.\n  " +
               Decl;
        First = false;
      }
      Ports.push_back(Decl);
    }
  }
  for (size_t i = 0; i < Ports.size(); ++i)
    O << "  " << Ports[i] << (i + 1 < Ports.size() ? ",\n" : "\n");
  O << ");\n\n";

  if (AnyCache) {
    // Context-cache sizing, DERIVED from STATE_DEPTH: the cache needs one
    // entry per resident context (MAX_INFLIGHT = STATE_DEPTH - 8, rounded up
    // to the power of two = STATE_DEPTH). Undersizing falls off a cliff —
    // measured 71.5 % hit at 64 entries vs 2.5 % at 32 for ~46 resident
    // loops — because below the resident count the loops evict each other
    // before any reuse.
    O << "  // Context-ID space: one private cache line per resident loop.\n";
    O << "  // NUM_CTX/CTX_W follow STATE_DEPTH: contexts = MAX_INFLIGHT\n";
    O << "  // rounded up to a power of two. Undersizing collapses the hit\n";
    O << "  // rate (measured 71.5% at 64 entries vs 2.5% at 32).\n";
    O << "  localparam NUM_CTX = STATE_DEPTH;\n";
    O << "  localparam CTX_W   = $clog2(STATE_DEPTH);\n\n";
  }

  // Internal AXI-Stream nets.
  emitAxisWire(O, "re_in", W_RE);           // into reentry
  if (!T.Escapes) {
    emitAxisWire(O, "root_out", W_RE);      // root -> reentry
    emitAxisWire(O, "cont_loop", W_RE);     // continuation -> loopback FIFO
    emitAxisWire(O, "cont_loop_q", W_RE);   // loopback FIFO -> reentry arbiter
  }
  // Inner levels: their own ring nets. The entry net is driven by the enclosing
  // level's tail rather than by a root PE.
  for (unsigned L = 1; L < NL; L++) {
    const uint64_t W = headWidth(L);
    O << "  // Level " << L << " ring (head '" << T.Levels[L].Head->getName()
      << "').\n";
    emitAxisWire(O, inW(L), W);
    emitAxisWire(O, entryW(L), W);
    emitAxisWire(O, loopW(L), W);
    emitAxisWire(O, loopQW(L), W);
    emitAxisWire(O, exitW(L),
                 taskWidthBits(TaskInfos.at(T.Levels[L].ExitTarget)));
  }
  // Feeds of the relay PEs that carry each level's token from its tail back to
  // its head (the `afterif` joins of a branching loop body).
  for (unsigned L = 0; L < NL; L++)
    for (unsigned I = 0; I < T.Levels[L].Relays.size(); I++)
      emitAxisWire(O, relayW(L, I),
                   taskWidthBits(TaskInfos.at(T.Levels[L].Relays[I])));
  auto emitSiteWires = [&](const RoundGen &G) {
    for (auto &SG : G.Sites) {
      emitAxisWire(O, SG.MemReq, SG.W_MEM);       // issuer -> reader
      emitAxisWire(O, SG.MemData, SG.W_ARGDATA);  // reader -> reply FIFO
      emitAxisWire(O, SG.MemDataQ, SG.W_ARGDATA); // reply FIFO -> merge
      O << "  wire [63:0] " << SG.MemArgOut << "_TDATA;  wire "
        << SG.MemArgOut << "_TVALID;  wire " << SG.MemArgOut << "_TREADY;\n";
    }
  };
  for (auto &SG : RG[0].Sites)
    emitAxisWire(O, SG.MemReq, SG.W_MEM);
  emitAxisWire(O, "re_exit", W_EXIT);         // reentry -> exit
  emitAxisWire(O, RG[0].StateIn, RG[0].W_CONT); // reentry -> STATE_FIFO
  emitAxisWire(O, RG[0].StateQ, RG[0].W_CONT);  // STATE_FIFO -> merge
  for (auto &SG : RG[0].Sites) {
    emitAxisWire(O, SG.MemData, SG.W_ARGDATA);  // reader -> reply FIFO
    emitAxisWire(O, SG.MemDataQ, SG.W_ARGDATA); // reply FIFO -> merge
    O << "  wire [63:0] " << SG.MemArgOut << "_TDATA;  wire "
      << SG.MemArgOut << "_TVALID;  wire " << SG.MemArgOut << "_TREADY;\n";
  }
  emitAxisWire(O, RG[0].ContIn, RG[0].W_CONT);     // merge -> continuation
  for (unsigned K = 1; K < NR; K++) {
    O << "  // Round " << K << ": issued by " << RG[K].R->Issuer->getName()
      << ".\n";
    emitAxisWire(O, RG[K].StateIn, RG[K].W_CONT);
    emitAxisWire(O, RG[K].StateQ, RG[K].W_CONT);
    emitAxisWire(O, RG[K].ContIn, RG[K].W_CONT);
    emitSiteWires(RG[K]);
  }
  // One argOut (closure address) and, when the exit carries a value/buffered
  // store, one argDataOut (payload) wire trio per exit destination. External
  // destinations drive the top ports; internal ones are sunk (the streaming
  // loop's own continuation is fed by the merge unit, not by the exit).
  for (auto &ED : ExitDests) {
    const std::string W = exitWire(ED);
    O << "  wire [63:0] " << W << "_argOut_TDATA; wire " << W
      << "_argOut_TVALID; wire " << W << "_argOut_TREADY;\n";
    if (ExitHasArgData)
      O << "  wire [" << (W_EXITARG - 1) << ":0] " << W << "_argData_TDATA; wire "
        << W << "_argData_TVALID; wire " << W << "_argData_TREADY;\n";
  }
  O << "\n";

  // Emit the AXI4 bundle connections for a memory PE. Vitis HLS names the
  // default m_axi bundle `gmem` (independent of the `void *mem` arg name) and,
  // for a pointer argument, also emits an `s_axi_control` slave that holds the
  // bundle's base-address offset. We pass the m_axi bundle out to the wrapper
  // top and tie the control slave idle: with the offset register left at its
  // reset value (0) the AXI address equals the pointer carried in the task
  // closure, which is exactly what the streaming design needs. The caller has
  // already written the PE's stream ports and a trailing comma.
  static const char *SAXI_TIE_IN[] = {
      "AWVALID", "AWADDR", "WVALID", "WDATA",  "WSTRB",
      "ARVALID", "ARADDR", "RREADY", "BREADY"};
  // Every MemPE gets a top-level AXI bundle from considerMem; this records that
  // the matching INSTANCE actually had that bundle wired. A PE whose bundle is
  // surfaced but not connected reads ARREADY as undriven and stalls forever
  // after swallowing its first closures — a silent hardware hang, and exactly
  // what happened when the root PE's instantiation omitted emitAxiConns.
  IRFuncSetTy AxiWired;
  auto emitAxiConns = [&](IRFunction *F) {
    for (auto &M : MemPEs) {
      if (M.F != F)
        continue;
      AxiWired.insert(F);
      std::vector<std::string> Conns;
      for (auto &S : AXI_SIGS)
        Conns.push_back("    .m_axi_gmem_" + std::string(S.Name) + "(" +
                        M.TopPrefix + "_" + S.Name + ")");
      // Tie the control-slave request inputs inactive (outputs left open).
      for (const char *Sig : SAXI_TIE_IN) {
        std::string V =
            (std::string(Sig).find("VALID") != std::string::npos ||
             std::string(Sig).find("READY") != std::string::npos)
                ? "1'b0"
                // Plain-Verilog tie-off: a 1-bit zero driving a wider input
                // port zero-extends, so unused control-slave inputs read as 0.
                : "1'b0";
        Conns.push_back("    .s_axi_control_" + std::string(Sig) + "(" + V + ")");
      }
      for (size_t i = 0; i < Conns.size(); ++i)
        O << Conns[i] << (i + 1 < Conns.size() ? ",\n" : "\n");
    }
  };

  // Queue in front of a level's exit PE. See the EXIT_DEPTH parameter comment:
  // the exit PE retires each task through a store->reload round trip and is far
  // slower than the merge ring, and the reentry PE that feeds it is one II=1
  // pipeline carrying both the continue and the exit path, so an unbuffered
  // exit stalls the entire loop. `exit_fire` deliberately stays on the
  // enqueue side: the admission credit and the context ID belong to the LOOP,
  // and the loop is finished the moment its exit closure is handed off.
  auto emitExitFifo = [&](unsigned L, uint64_t Width) {
    const std::string W = exitW(L);
    O << "  // ── Exit queue: decouples the merge ring from the exit PE ───────────────\n";
    O << "  wire [" << (Width - 1) << ":0] " << W << "_q_TDATA;\n";
    O << "  wire " << W << "_q_TVALID;\n";
    O << "  wire " << W << "_q_TREADY;\n";
    O << "  " << TopName << "_fifo #(.WIDTH(" << Width
      << "), .DEPTH(EXIT_DEPTH)) u_" << W << "_fifo (\n";
    O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
    O << "    .in_data(" << W << "_TDATA), .in_valid(" << W
      << "_TVALID), .in_ready(" << W << "_TREADY),\n";
    O << "    .out_data(" << W << "_q_TDATA), .out_valid(" << W
      << "_q_TVALID), .out_ready(" << W << "_q_TREADY)\n";
    O << "  );\n\n";
  };

  if (!T.Escapes) {
    // Inbound skid on the scheduler -> PE task stream. The scheduler sits in the
    // middle die while half the PEs are floorplanned into the neighbouring SLR,
    // so this net crosses an SLL. Buffering it here makes the crossing
    // flop-to-flop in both directions: the incoming data lands directly in the
    // skid's register, and taskIn_TREADY becomes a flop output instead of the
    // root PE's combinational ready.
    //
    // The buffer is W_ROOT wide -- the root closure's real width -- not a fixed
    // 256. A narrower skid silently truncates the closure: the surplus bits of
    // taskIn_TDATA are dropped on the way in and the root PE's upper taskIn bits
    // are left undriven, so every field above the cut reads as 0 (or X in a
    // four-state simulator). For pageRank_overlap, whose applyFn_task is 512
    // bits, that erased `u`, `damping`, `diffs` and half of `pPrNext` -- every
    // task then ran as vertex 0 with damping 0, and the X addresses hung the
    // AXI master in hardware.
    O << "  // ── Inbound skid: registers the scheduler -> PE task stream ─────────────\n";
    O << "  wire [" << (W_ROOT - 1) << ":0] task_in_q_TDATA;\n";
    O << "  wire         task_in_q_TVALID;\n";
    O << "  wire         task_in_q_TREADY;\n";
    O << "  " << TopName << "_mrc_skid #(.WIDTH(" << W_ROOT
      << ")) u_task_in_skid (\n";
    O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
    O << "    .i_data(taskIn_TDATA), .i_valid(taskIn_TVALID), .i_ready(taskIn_TREADY),\n";
    O << "    .o_data(task_in_q_TDATA), .o_valid(task_in_q_TVALID), .o_ready(task_in_q_TREADY)\n";
    O << "  );\n\n";
    // Root PE.
    O << "  // ── Root PE (loop initializer) ───────────────────────────────────────────\n";
    O << "  " << Root << " u_root (\n";
    O << "    .ap_clk(ap_clk), .ap_rst_n(ap_rst_n),\n";
    O << "    .taskIn_TDATA(task_in_q_TDATA), .taskIn_TVALID(task_in_q_TVALID), .taskIn_TREADY(task_in_q_TREADY),\n";
    O << "    .taskGlobalOut_" << Re << "_TDATA(root_out_TDATA),\n";
    O << "    .taskGlobalOut_" << Re << "_TVALID(root_out_TVALID),\n";
    // The loop initializer may itself read memory — the code before the loop
    // (`neighbors_u = pGraph[2*u]`) is outside OverlapMemAnalysis' scope, so it
    // stays an inline load on the root's own m_axi master. considerMem already
    // surfaced the bundle on the wrapper top; without these connections ARREADY
    // is undriven and the root swallows closures and never completes a read.
    const bool RootHasAXI = TaskInfos.at(T.Root).HasAXI;
    O << "    .taskGlobalOut_" << Re << "_TREADY(root_out_TREADY)"
      << (RootHasAXI ? ",\n" : "\n");
    emitAxiConns(T.Root);
    O << "  );\n\n";
  } else {
    // Escaping loop: the wrapper is entered with the reentry's own closure —
    // from the loop initializer on the first iteration and from the external
    // continuation that closes the loop on every later one. Both arrive on the
    // single taskIn port through the scheduler, so there is no internal root,
    // no loop-back FIFO and no arbiter. The scheduler's queues absorb the cycle
    // that the loop-back FIFO and its credit counter cover in the internal case.
    O << "  // ── taskIn feeds the reentry directly ────────────────────────────────────\n";
    O << "  // Entered with the reentry closure by the loop initializer and, on every\n";
    O << "  // later iteration, by the external continuation; the scheduler serialises\n";
    O << "  // both onto this port.\n";
    O << "  assign re_in_TDATA  = taskIn_TDATA;\n";
    O << "  assign re_in_TVALID = taskIn_TVALID;\n";
    O << "  assign taskIn_TREADY = re_in_TREADY;\n\n";
  }

  // One ring per loop level: buffering + admission control + head PE + the
  // level's rounds. A single-level loop emits exactly what it always has.
  uint64_t ReplyOff = 8ull * 8; // arg_out.data byte offset (after 8-byte addr)
  for (unsigned L = 0; L < NL; L++) {
    const OverlapLevel &Lv = T.Levels[L];
    const std::string LS = lsfx(L);
    const std::string HeadN = Lv.Head->getName();
    const std::string ExitN = Lv.ExitTarget->getName();
    const uint64_t W_HEAD = headWidth(L);
    const RoundGen &R0 = RG[Lv.FirstRound];

    const bool Instr = LevelInstr[L];
    if (!T.Escapes) {
      if (Instr) {
        // Context-ID plumbing, declarations first: every net is declared here,
        // before its first use, because Verilog would otherwise create an
        // implicit 1-bit net at the first reference and then reject the real
        // declaration. The logic that drives them follows this level's rounds.
        O << "  // ── Context-ID plumbing: declarations (logic follows the "
             "rounds) ─────────\n";
        O << "  wire [" << W_HEAD << "+CTX_W-1:0] " << inW(L) << "_wide;\n";
        O << "  wire [" << W_HEAD << "+CTX_W-1:0] " << loopQW(L) << "_wide;\n";
        O << "  wire [CTX_W-1:0] ctx_in" << LS << "       = " << inW(L)
          << "_wide[" << W_HEAD << " +: CTX_W];\n";
        O << "  wire [CTX_W-1:0] ctx_loopback" << LS << " = " << loopQW(L)
          << "_wide[" << W_HEAD << " +: CTX_W];\n";
        O << "  wire [CTX_W-1:0] ctx_retire" << LS
          << ";                // ID of the loop that is exiting\n";
        O << "  wire [CTX_W-1:0] ctxB" << LS << "_head;\n";
        for (unsigned K = Lv.FirstRound; K < Lv.FirstRound + Lv.NumRounds;
             K++) {
          const RoundGen &G = RG[K];
          O << "  wire [" << G.W_CONT << "+CTX_W-1:0] " << G.StateQ
            << "_wide;\n";
          O << "  wire [CTX_W-1:0] ctx_state" << G.S << " = " << G.StateQ
            << "_wide[" << G.W_CONT << " +: CTX_W];\n";
          O << "  wire [CTX_W-1:0] ctxS" << G.S << "_head;\n";
          for (const GlobalSite &S : Sites)
            if (S.Round == K && S.Cached)
              O << "  wire [CTX_W-1:0] " << siteCtxHead(*S.SG) << ";\n";
        }
        // The narrow TDATA views stay in place for the head PE / merge units;
        // they are simply slices of the widened buses.
        O << "  assign " << inW(L) << "_TDATA = " << inW(L) << "_wide["
          << (W_HEAD - 1) << ":0];\n";
        O << "  assign " << loopQW(L) << "_TDATA = " << loopQW(L) << "_wide["
          << (W_HEAD - 1) << ":0];\n";
        for (unsigned K = Lv.FirstRound; K < Lv.FirstRound + Lv.NumRounds; K++)
          O << "  assign " << RG[K].StateQ << "_TDATA = " << RG[K].StateQ
            << "_wide[" << (RG[K].W_CONT - 1) << ":0];\n";
        O << "\n";
      }

      // Loopback FIFO: the edge that closes this level's ring (continuation →
      // head for the innermost level, inner-loop exit → head for an enclosing
      // one) is a dataflow cycle. Under back-pressure an unbuffered cycle
      // DEADLOCKS: every stage fills and waits on the next. This FIFO gives the
      // ring a place to always retire its loop-back token so the cycle can
      // drain. It must be deep enough to hold every task in flight (bounded by
      // the memory latency), so it shares STATE_DEPTH with the state FIFO.
      if (NL == 1)
        O << "  // ── Loopback FIFO (breaks the "
             "reentry->...->continuation->reentry cycle) ──\n";
      else
        O << "  // ── Loopback FIFO (breaks the " << HeadN << "->...->"
          << Lv.BackSrc->getName() << "->" << HeadN << " cycle) ──\n";
      if (Instr) {
        // Widened by CTX_W: a re-entering iteration keeps the ID its loop was
        // given at admission. ctxB holds the ID across the tail continuation
        // PE (which has no field for it) — see the plumbing after the rounds.
        O << "  " << TopName << "_fifo #(.WIDTH(" << W_HEAD
          << "+CTX_W), .DEPTH(STATE_DEPTH)) u_loop_fifo" << LS << " (\n";
        O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
        O << "    .in_data({ctxB" << LS << "_head, " << loopW(L)
          << "_TDATA}), .in_valid(" << loopW(L) << "_TVALID), .in_ready("
          << loopW(L) << "_TREADY),\n";
        O << "    .out_data(" << loopQW(L) << "_wide), .out_valid(" << loopQW(L)
          << "_TVALID), .out_ready(" << loopQW(L) << "_TREADY)\n";
        O << "  );\n\n";
      } else {
        O << "  " << TopName << "_fifo #(.WIDTH(" << W_HEAD
          << "), .DEPTH(STATE_DEPTH)) u_loop_fifo" << LS << " (\n";
        O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
        O << "    .in_data(" << loopW(L) << "_TDATA), .in_valid(" << loopW(L)
          << "_TVALID), .in_ready(" << loopW(L) << "_TREADY),\n";
        O << "    .out_data(" << loopQW(L) << "_TDATA), .out_valid("
          << loopQW(L) << "_TVALID), .out_ready(" << loopQW(L)
          << "_TREADY)\n";
        O << "  );\n\n";
      }

      // Admission control (credit counter): a loopback FIFO only prevents
      // deadlock while it never fills. We therefore cap the number of tasks
      // concurrently inside this ring below STATE_DEPTH. A task takes a credit
      // when it is admitted to the head and returns it when it leaves through
      // the head's exit spawn. With in-flight < STATE_DEPTH the loopback and
      // state FIFOs can never fill, so the ring always drains. MAX_INFLIGHT
      // still leaves ample parallelism to hide memory latency (which needs only
      // ~latency outstanding iterations).
      //
      // Each level needs its OWN counter: they observe different admission and
      // departure edges, and a shared counter would leak credits on whichever
      // departure path it does not watch.
      //
      // On an instrumented level the counter additionally hands each admitted
      // loop a context ID from a free queue. Because the ID is held for the
      // loop's whole residency and returned only at exit, it identifies the
      // loop — which is what lets the reader's cache give every loop a private
      // line with no tags-per-way and no possibility of two loops colliding.
      O << "  // ── Admission control: keep in-flight tasks < buffer depth ───────────────\n";
      O << "  localparam MAX_INFLIGHT" << LS << " = STATE_DEPTH - 8;\n";
      O << "  localparam IW" << LS << " = $clog2(STATE_DEPTH+1);\n";
      O << "  reg  [IW" << LS << "-1:0] inflight" << LS << ";\n";
      if (Instr) {
        O << "  // Free queue of context IDs; the old in-flight bound still "
             "applies.\n";
        O << "  //\n";
        O << "  // The array has exactly ONE write port, in its own process, so it\n";
        O << "  // infers as distributed RAM. It used to be broadside-initialised to\n";
        O << "  // the identity permutation in the reset branch, which is a second\n";
        O << "  // write source: Vivado then dissolved it into NUM_CTX*CTX_W flops read\n";
        O << "  // through a NUM_CTX:1 mux, and the MUXF7/MUXF8 tree that produced was\n";
        O << "  // a failing path into the context RAM's write port.\n";
        O << "  //\n";
        O << "  // The identity is instead served straight from the read pointer for the\n";
        O << "  // first pass over the queue -- the same values the initialisation would\n";
        O << "  // have stored -- and the RAM is read only once every slot has been\n";
        O << "  // written back by a retiring loop. That is safe, and safe across a\n";
        O << "  // re-reset, because the queue never holds more than NUM_CTX IDs: slot k\n";
        O << "  // is always read before it is written, and reaching rp == NUM_CTX resets\n";
        O << "  // wp to NUM_CTX too, so a RAM read can only be *consumed* (credit_ok\n";
        O << "  // requires !empty, i.e. wp > rp) after a retire of this run wrote it.\n";
        O << "  reg  [CTX_W-1:0] ctxq" << LS << " [0:NUM_CTX-1];\n";
        O << "  reg  [IW" << LS << "-1:0] ctxq" << LS << "_wp, ctxq" << LS
          << "_rp;\n";
        O << "  reg              ctxq" << LS << "_init;\n";
        O << "  reg              ctxq" << LS << "_primed;\n";
        O << "  wire             ctxq" << LS << "_empty = (ctxq" << LS
          << "_wp == ctxq" << LS << "_rp);\n";
        O << "  wire [CTX_W-1:0] ctx_alloc" << LS << "  = ctxq" << LS
          << "_primed\n";
        O << "                                 ? ctxq" << LS << "[ctxq" << LS
          << "_rp[IW" << LS << "-1:0] % NUM_CTX]\n";
        O << "                                 : ctxq" << LS << "_rp[CTX_W-1:0];\n";
        O << "  wire credit_ok" << LS << " = ctxq" << LS << "_init && !ctxq"
          << LS << "_empty\n";
        O << "                      && (inflight" << LS << " < MAX_INFLIGHT"
          << LS << "[IW" << LS << "-1:0]);\n";
      } else {
        O << "  wire credit_ok" << LS << " = (inflight" << LS
          << " < MAX_INFLIGHT" << LS << "[IW" << LS << "-1:0]);\n";
      }
      O << "  wire arb_a_ready" << LS << ";\n";
      O << "  wire admit_fire" << LS << " = " << entryW(L)
        << "_TVALID && credit_ok" << LS << " && arb_a_ready" << LS << ";\n";
      O << "  wire exit_fire" << LS << "  = " << exitW(L) << "_TVALID && "
        << exitW(L) << "_TREADY;\n";
      O << "  assign " << entryW(L) << "_TREADY = arb_a_ready" << LS
        << " && credit_ok" << LS << ";\n";
      if (Instr) {
        // Sole write port, in its own process: one write port per array is what
        // lets it infer as a RAM instead of dissolving into registers.
        O << "  always @(posedge ap_clk)\n";
        O << "    if (exit_fire" << LS << ") ctxq" << LS << "[ctxq" << LS
          << "_wp[IW" << LS << "-1:0] % NUM_CTX] <= ctx_retire" << LS << ";\n";
        O << "  always @(posedge ap_clk) begin\n";
        O << "    if (!ap_rst_n) begin\n";
        O << "      inflight" << LS << " <= 0; ctxq" << LS << "_wp <= 0; ctxq"
          << LS << "_rp <= 0; ctxq" << LS << "_init <= 1'b0;\n";
        O << "      ctxq" << LS << "_primed <= 1'b0;\n";
        O << "    end else begin\n";
        O << "      if (!ctxq" << LS << "_init) begin\n";
        O << "        // every ID starts free\n";
        O << "        ctxq" << LS << "_wp <= NUM_CTX[IW" << LS << "-1:0];\n";
        O << "        ctxq" << LS << "_init <= 1'b1;\n";
        O << "      end else begin\n";
        O << "        if (admit_fire" << LS << ") begin\n";
        O << "          ctxq" << LS << "_rp <= ctxq" << LS << "_rp + 1'b1;\n";
        O << "          // from here on every slot has been written back\n";
        O << "          if (ctxq" << LS << "_rp == (NUM_CTX[IW" << LS
          << "-1:0] - 1'b1)) ctxq" << LS << "_primed <= 1'b1;\n";
        O << "        end\n";
        O << "        if (exit_fire" << LS << ") ctxq" << LS << "_wp <= ctxq"
          << LS << "_wp + 1'b1;\n";
        O << "      end\n";
        O << "      inflight" << LS << " <= inflight" << LS << " + (admit_fire"
          << LS << " ? 1'b1 : 1'b0)\n";
        O << "                                 - (exit_fire" << LS
          << " ? 1'b1 : 1'b0);\n";
        O << "    end\n";
        O << "  end\n\n";
      } else {
        O << "  always @(posedge ap_clk) begin\n";
        O << "    if (!ap_rst_n) inflight" << LS << " <= 0;\n";
        O << "    else inflight" << LS << " <= inflight" << LS
          << " + (admit_fire" << LS << " ? 1'b1 : 1'b0) - (exit_fire" << LS
          << " ? 1'b1 : 1'b0);\n";
        O << "  end\n\n";
      }

      // Arbiter: ring entry (credit-gated) + loop-back (buffered) -> head.
      O << "  // ── reentry.taskIn arbiter: initial spawn + continuation loopback ─────────\n";
      if (Instr) {
        // The arbiter carries the context alongside the closure, so a
        // re-entering iteration keeps the ID its loop was given at admission.
        O << "  " << TopName << "_arb2 #(.WIDTH(" << W_HEAD
          << "+CTX_W)) u_re_arb" << LS << " (\n";
        O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
        O << "    .a_data({ctx_alloc" << LS << ", " << entryW(L)
          << "_TDATA}),\n";
        O << "    .a_valid(" << entryW(L) << "_TVALID && credit_ok" << LS
          << "), .a_ready(arb_a_ready" << LS << "),\n";
        O << "    .b_data({ctx_loopback" << LS << ", " << loopQW(L)
          << "_TDATA}),\n";
        O << "    .b_valid(" << loopQW(L) << "_TVALID), .b_ready(" << loopQW(L)
          << "_TREADY),\n";
        O << "    .o_data(" << inW(L) << "_wide), .o_valid(" << inW(L)
          << "_TVALID), .o_ready(" << inW(L) << "_TREADY)\n";
        O << "  );\n\n";
      } else {
        O << "  " << TopName << "_arb2 #(.WIDTH(" << W_HEAD << ")) u_re_arb"
          << LS << " (\n";
        O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
        O << "    .a_data(" << entryW(L) << "_TDATA), .a_valid(" << entryW(L)
          << "_TVALID && credit_ok" << LS << "), .a_ready(arb_a_ready" << LS
          << "),\n";
        O << "    .b_data(" << loopQW(L) << "_TDATA), .b_valid(" << loopQW(L)
          << "_TVALID), .b_ready(" << loopQW(L) << "_TREADY),\n";
        O << "    .o_data(" << inW(L) << "_TDATA), .o_valid(" << inW(L)
          << "_TVALID), .o_ready(" << inW(L) << "_TREADY)\n";
        O << "  );\n\n";
      }
    }

    // Head PE. It issues this level's round 0 reads and pushes its continuation
    // state; if the loop exits it spawns the exit task instead.
    O << "  // ── Reentry PE (condition + issue reads + push state) ────────────────────\n";
    O << "  " << HeadN << " u_reentry" << LS << " (\n";
    O << "    .ap_clk(ap_clk), .ap_rst_n(ap_rst_n),\n";
    O << "    .taskIn_TDATA(" << inW(L) << "_TDATA), .taskIn_TVALID(" << inW(L)
      << "_TVALID), .taskIn_TREADY(" << inW(L) << "_TREADY),\n";
    // One dependent-spawn port per read site: each site has its own reader, so
    // the PE writes one request per stream rather than N into one.
    for (auto &SG : R0.Sites) {
      O << "    .taskGlobalOut_" << SG.MemName << "_depends_" << R0.Cont
        << "_TDATA(" << SG.MemReq << "_TDATA),\n";
      O << "    .taskGlobalOut_" << SG.MemName << "_depends_" << R0.Cont
        << "_TVALID(" << SG.MemReq << "_TVALID),\n";
      O << "    .taskGlobalOut_" << SG.MemName << "_depends_" << R0.Cont
        << "_TREADY(" << SG.MemReq << "_TREADY),\n";
    }
    O << "    .taskGlobalOut_" << ExitN << "_TDATA(" << exitW(L) << "_TDATA),\n";
    O << "    .taskGlobalOut_" << ExitN << "_TVALID(" << exitW(L)
      << "_TVALID),\n";
    O << "    .taskGlobalOut_" << ExitN << "_TREADY(" << exitW(L)
      << "_TREADY),\n";
    O << "    .contStateOut_" << R0.Cont << "_TDATA(" << R0.StateIn
      << "_TDATA),\n";
    O << "    .contStateOut_" << R0.Cont << "_TVALID(" << R0.StateIn
      << "_TVALID),\n";
    // A loop head can also hold an inline load (one OverlapMemAnalysis did not
    // decouple, e.g. in the loop condition), so it needs its bundle wired too.
    const bool HeadHasAXI = TaskInfos.at(Lv.Head).HasAXI;
    O << "    .contStateOut_" << R0.Cont << "_TREADY(" << R0.StateIn
      << "_TREADY)" << (HeadHasAXI ? ",\n" : "\n");
    emitAxiConns(Lv.Head);
    O << "  );\n\n";

    // One (state FIFO + memReader + merge unit + continuation PE) stage per
    // round of dependent loads. Round 0's replies complete the head's
    // continuation; each later round is issued by the previous continuation and
    // completes the next one. The level's last continuation hands the token on:
    // back to its own head, or into the nested level's ring.
    const unsigned KEnd = Lv.FirstRound + Lv.NumRounds;
    for (unsigned K = Lv.FirstRound; K < KEnd; K++) {
      const RoundGen &G = RG[K];
      const std::string &S = G.S;

      // State FIFO. On an instrumented level it is widened by CTX_W so the
      // owning loop's context ID returns with the joined reply.
      O << "  // ── Continuation-state FIFO (holds loop-carried state per iteration) ──────\n";
      if (Instr) {
        O << "  " << TopName << "_fifo #(.WIDTH(" << G.W_CONT
          << "+CTX_W), .DEPTH(STATE_DEPTH)) u_state_fifo" << S << " (\n";
        O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
        O << "    .in_data({ctxS" << S << "_head, " << G.StateIn
          << "_TDATA}), .in_valid(" << G.StateIn << "_TVALID), .in_ready("
          << G.StateIn << "_TREADY),\n";
        O << "    .out_data(" << G.StateQ << "_wide), .out_valid(" << G.StateQ
          << "_TVALID), .out_ready(" << G.StateQ << "_TREADY)\n";
        O << "  );\n\n";
      } else {
        O << "  " << TopName << "_fifo #(.WIDTH(" << G.W_CONT
          << "), .DEPTH(STATE_DEPTH)) u_state_fifo" << S << " (\n";
        O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
        O << "    .in_data(" << G.StateIn << "_TDATA), .in_valid(" << G.StateIn
          << "_TVALID), .in_ready(" << G.StateIn << "_TREADY),\n";
        O << "    .out_data(" << G.StateQ << "_TDATA), .out_valid(" << G.StateQ
          << "_TVALID), .out_ready(" << G.StateQ << "_TREADY)\n";
        O << "  );\n\n";
      }

      // Read sites: each keeps its own request port and its own in-order reply
      // channel (so the issuing PE stays II = 1 and all N replies of an
      // iteration arrive in the same cycle), but they are all served by the
      // single shared reader instantiated after the levels — stream index =
      // ARID on its one master.
      for (auto &SG : G.Sites) {
        O << "  // ── Read site for reply field '" << G.R->Replies[&SG - &G.Sites[0]].Name
          << "': stream " << SG.MemReq << " on the shared reader ──\n";
        O << "  assign " << SG.MemArgOut
          << "_TREADY = 1'b1; // reply address unused in streaming mode\n";
        // Reply-skew FIFO. The merge below can only fire when EVERY site's
        // reply is present, so without this the first reply to arrive is held
        // in its memReader's output register until the slowest sibling catches
        // up. A memReader is a `flp` pipeline: blocking its argDataOut write
        // stalls the whole pipeline, which stops it accepting new requests AND
        // stops it issuing new AXI reads. One site's latency tail therefore
        // throttles read issue on every site in the round, and no amount of
        // NUM_READ_OUTSTANDING helps because the reads are never issued.
        // Buffering each reply decouples the sites: each runs at its own rate
        // and the merge absorbs the skew.
        O << "  " << TopName << "_fifo #(.WIDTH(" << SG.W_ARGDATA
          << "), .DEPTH(REPLY_DEPTH)) " << SG.Inst << "_rq (\n";
        O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
        O << "    .in_data(" << SG.MemData << "_TDATA), .in_valid("
          << SG.MemData << "_TVALID), .in_ready(" << SG.MemData
          << "_TREADY),\n";
        O << "    .out_data(" << SG.MemDataQ << "_TDATA), .out_valid("
          << SG.MemDataQ << "_TVALID), .out_ready(" << SG.MemDataQ
          << "_TREADY)\n";
        O << "  );\n\n";
      }

      // Merge unit: parallel join of the N reply channels with the state head.
      O << "  // ── Merge unit: join " << G.N
        << " parallel reply channel(s) + STATE_FIFO head ──────\n";
      O << "  // Each site has its own in-order channel, so the k-th reply of an\n";
      O << "  // iteration is the head of channel k. All " << G.N
        << " are consumed in ONE cycle\n";
      O << "  // and drive continuation field(s) '" << G.R->Replies[0].Name << "'";
      for (unsigned k = 1; k < G.N; k++)
        O << (k == 1 ? " ... '" : ", '") << G.R->Replies[k].Name << "'";
      O << ".\n";
      // The continuation is driven from a STABLE output register (outbuf): once
      // cont_in_TVALID is asserted, TDATA/TVALID hold until the PE accepts, as
      // AXI-Stream requires. Because the join can fire on the same cycle outbuf
      // drains (out_ready), no reader is ever back-pressured: one closure
      // completed per cycle => II 1 per loop iteration.
      O << "  reg  [" << (G.W_CONT - 1) << ":0] outbuf" << S
        << ";    // stable joined closure\n";
      O << "  reg  outbuf_v" << S << ";\n";
      for (unsigned k = 0; k < G.N; k++) {
        const SiteGen &SG = G.Sites[k];
        O << "  wire [" << (SG.ReplyBits - 1) << ":0] reply_data" << S << "_"
          << k << " = " << SG.MemDataQ << "_TDATA["
          << (ReplyOff + SG.ReplyBits - 1) << ":" << ReplyOff << "];\n";
      }
      // Compose the closure by overlaying each site's reply on the state head.
      O << "  reg  [" << (G.W_CONT - 1) << ":0] joined" << S << ";\n";
      O << "  always @* begin\n";
      O << "    joined" << S << " = " << G.StateQ << "_TDATA;\n";
      for (unsigned k = 0; k < G.N; k++) {
        const ReplyField &RF = G.R->Replies[k];
        O << "    joined" << S << "[" << (RF.OffBits + RF.WidthBits - 1) << ":"
          << RF.OffBits << "] = reply_data" << S << "_" << k << "; // "
          << RF.Name << "\n";
      }
      O << "  end\n";
      O << "  // Output register is free to load when empty or draining this cycle.\n";
      O << "  wire out_ready" << S << " = !outbuf_v" << S << " || " << G.ContIn
        << "_TREADY;\n";
      O << "  wire all_replies" << S << " = ";
      for (unsigned k = 0; k < G.N; k++)
        O << (k ? " && " : "") << G.Sites[k].MemDataQ << "_TVALID";
      O << ";\n";
      O << "  wire gather" << S << "    = all_replies" << S << " && "
        << G.StateQ << "_TVALID && out_ready" << S << ";\n";
      for (unsigned k = 0; k < G.N; k++)
        O << "  assign " << G.Sites[k].MemDataQ << "_TREADY = gather" << S
          << ";\n";
      O << "  assign " << G.StateQ << "_TREADY  = gather" << S << ";\n";
      O << "  assign " << G.ContIn << "_TVALID  = outbuf_v" << S << ";\n";
      O << "  assign " << G.ContIn << "_TDATA   = outbuf" << S << ";\n";
      O << "  always @(posedge ap_clk) begin\n";
      O << "    if (!ap_rst_n) begin\n";
      O << "      outbuf_v" << S << " <= 1'b0;\n";
      O << "    end else begin\n";
      O << "      if (" << G.ContIn << "_TVALID && " << G.ContIn
        << "_TREADY) outbuf_v" << S << " <= 1'b0;\n";
      O << "      if (gather" << S << ") begin outbuf" << S << " <= joined" << S
        << "; outbuf_v" << S << " <= 1'b1; end\n";
      O << "    end\n";
      O << "  end\n\n";

      // Continuation PE. The level's last one hands the token on; an
      // intermediate one instead issues the next round.
      const bool IsTail = (K + 1 == KEnd);
      // Where the tail's token goes. NOT assumed to be this level's head: with a
      // nested loop the outer level's tail enters the INNER level's ring, and
      // wiring it to the head's port name would emit a port that does not exist
      // on the PE.
      // A branching loop body reaches the head through relay PEs, so the tail's
      // spawn target is the first relay rather than the head itself.
      IRFunction *TailDst =
          !IsTail             ? nullptr
          : !Lv.Relays.empty() ? Lv.Relays.front()
                              : Lv.TailSpawn;
      std::string TailWire;
      if (TailDst) {
        const unsigned DL = levelOfHead(TailDst);
        TailWire = !Lv.Relays.empty() ? relayW(L, 0)
                   : (TailDst == Lv.Head) ? loopW(L)
                   : (DL < NL)            ? entryW(DL)
                                          : std::string();
      }
      O << "  // ── Continuation PE ("
        << (IsTail ? (TailDst && TailDst != Lv.Head
                          ? "loop body; hands off to '" + TailDst->getName() +
                                "'"
                          : "loop body; loops back to reentry")
                   : "issues round " + std::to_string(K + 1))
        << ") ───────────────────\n";
      O << "  " << G.Cont << " u_cont" << S << " (\n";
      O << "    .ap_clk(ap_clk), .ap_rst_n(ap_rst_n),\n";
      O << "    .taskIn_TDATA(" << G.ContIn << "_TDATA), .taskIn_TVALID("
        << G.ContIn << "_TVALID), .taskIn_TREADY(" << G.ContIn << "_TREADY),\n";
      const bool ContHasAXI = TaskInfos.at(G.R->Cont).HasAXI;
      if (IsTail && T.Escapes) {
        // Hands the loop body to a real spawned task. Its continuation is
        // allocated and released through the ordinary scheduler mechanics
        // (closureIn / spawnNext), because that continuation lives outside the
        // wrapper — the merge unit cannot complete it from a memReader reply.
        const std::string SpawnPort = "taskGlobalOut_" +
                                      T.EscapeSpawn->getName() + "_depends_" +
                                      T.EscapeCont->getName();
        const std::string SNPort = "spawnNext_" + T.EscapeCont->getName();
        O << "    ." << SpawnPort << "_TDATA(" << SpawnPort << "_TDATA),\n";
        O << "    ." << SpawnPort << "_TVALID(" << SpawnPort << "_TVALID),\n";
        O << "    ." << SpawnPort << "_TREADY(" << SpawnPort << "_TREADY),\n";
        O << "    .closureIn_TDATA(closureIn_TDATA), .closureIn_TVALID(closureIn_TVALID), .closureIn_TREADY(closureIn_TREADY),\n";
        O << "    ." << SNPort << "_TDATA(" << SNPort << "_TDATA),\n";
        O << "    ." << SNPort << "_TVALID(" << SNPort << "_TVALID),\n";
        O << "    ." << SNPort << "_TREADY(" << SNPort << "_TREADY)"
          << (ContHasAXI ? ",\n" : "\n");
      } else if (IsTail) {
        const std::string DN = TailDst->getName();
        O << "    .taskGlobalOut_" << DN << "_TDATA(" << TailWire
          << "_TDATA),\n";
        O << "    .taskGlobalOut_" << DN << "_TVALID(" << TailWire
          << "_TVALID),\n";
        // The continuation tail-spawns its successor, forwarding _cont through
        // that task struct, so it has no argOut port of its own.
        O << "    .taskGlobalOut_" << DN << "_TREADY(" << TailWire << "_TREADY)"
          << (ContHasAXI ? ",\n" : "\n");
      } else {
        const RoundGen &NG = RG[K + 1];
        for (auto &SG : NG.Sites) {
          O << "    .taskGlobalOut_" << SG.MemName << "_depends_" << NG.Cont
            << "_TDATA(" << SG.MemReq << "_TDATA),\n";
          O << "    .taskGlobalOut_" << SG.MemName << "_depends_" << NG.Cont
            << "_TVALID(" << SG.MemReq << "_TVALID),\n";
          O << "    .taskGlobalOut_" << SG.MemName << "_depends_" << NG.Cont
            << "_TREADY(" << SG.MemReq << "_TREADY),\n";
        }
        O << "    .contStateOut_" << NG.Cont << "_TDATA(" << NG.StateIn
          << "_TDATA),\n";
        O << "    .contStateOut_" << NG.Cont << "_TVALID(" << NG.StateIn
          << "_TVALID),\n";
        O << "    .contStateOut_" << NG.Cont << "_TREADY(" << NG.StateIn
          << "_TREADY)" << (ContHasAXI ? ",\n" : "\n");
      }
      emitAxiConns(G.R->Cont);
      O << "  );\n\n";
    }

    // ── Relay PEs: tail -> ... -> head ────────────────────────────────────
    // The `afterif` joins of a branching loop body. Each is strictly
    // one-in-one-out and issues no reads, so it needs no state FIFO, reader or
    // merge unit — but it does own the induction-variable increment, so the
    // ring must run through it. Omitting it leaves the loop-back edge dangling
    // and the loop retires exactly one iteration before deadlocking.
    for (unsigned I = 0; I < Lv.Relays.size(); I++) {
      IRFunction *RP = Lv.Relays[I];
      const bool Last = (I + 1 == Lv.Relays.size());
      IRFunction *Dst = Last ? Lv.TailSpawn : Lv.Relays[I + 1];
      const unsigned DL = levelOfHead(Dst);
      const std::string DstWire = !Last            ? relayW(L, I + 1)
                                  : (Dst == Lv.Head) ? loopW(L)
                                                     : entryW(DL);
      const std::string DN = Dst->getName();
      O << "  // ── Relay PE '" << RP->getName() << "' -> '" << DN << "' ──\n";
      O << "  " << RP->getName() << " u_relay" << LS << "_" << I << " (\n";
      O << "    .ap_clk(ap_clk), .ap_rst_n(ap_rst_n),\n";
      O << "    .taskIn_TDATA(" << relayW(L, I) << "_TDATA), .taskIn_TVALID("
        << relayW(L, I) << "_TVALID), .taskIn_TREADY(" << relayW(L, I)
        << "_TREADY),\n";
      const bool RelayHasAXI = TaskInfos.at(RP).HasAXI;
      O << "    .taskGlobalOut_" << DN << "_TDATA(" << DstWire << "_TDATA),\n";
      O << "    .taskGlobalOut_" << DN << "_TVALID(" << DstWire
        << "_TVALID),\n";
      O << "    .taskGlobalOut_" << DN << "_TREADY(" << DstWire << "_TREADY)"
        << (RelayHasAXI ? ",\n" : "\n");
      emitAxiConns(RP);
      O << "  );\n\n";
    }

    if (Instr) {
      // ── Context-ID plumbing: the logic ─────────────────────────────────────
      // The reader's requests are emitted by the HLS PEs, which have no field
      // for the ID, so it travels a parallel wrapper-owned path. Every PE in
      // the ring is strictly one-in-one-out, so a plain FIFO alongside each
      // keeps the ID in step with the closure it belongs to:
      //
      //   re_in fire            push ID into the round-0 ctx FIFOs
      //   iss_state fire        ctxS head -> the (widened) state FIFO
      //   site request accepted that site's ctx head -> the reader, then pop
      //   re_exit fire          the iteration made no request: pop the round-0
      //                         FIFOs and return the ID to the free queue
      //   gather                ID out of the state FIFO -> the next round's
      //                         FIFOs (or ctxB after the last round)
      //   cont_loop fire        ctxB head -> the (widened) loop FIFO
      //
      // The per-site FIFOs are pushed at re_in/gather, NOT at iss_state: the
      // HLS PE may emit a memory request before its state, and the reader must
      // never wait on an ID that has not been pushed yet — that deadlocks.
      O << "  // ── Context-ID plumbing: logic (declarations precede the "
           "ring) ──────────\n";
      O << "  wire re_in_fire" << LS << " = " << inW(L) << "_TVALID && "
        << inW(L) << "_TREADY;\n";
      O << "  wire cont_loop_fire" << LS << " = " << loopW(L) << "_TVALID && "
        << loopW(L) << "_TREADY;\n";
      const unsigned KEnd2 = Lv.FirstRound + Lv.NumRounds;
      for (unsigned K = Lv.FirstRound; K < KEnd2; K++) {
        const RoundGen &G = RG[K];
        O << "  wire " << G.StateIn << "_fire = " << G.StateIn << "_TVALID && "
          << G.StateIn << "_TREADY;\n";
        for (const GlobalSite &S : Sites)
          if (S.Round == K && S.Cached)
            O << "  wire " << S.SG->MemReq << "_fire = " << S.SG->MemReq
              << "_TVALID && " << S.SG->MemReq << "_TREADY;\n";
      }
      // The exiting iteration's ID is at the head of the first round's state
      // ctx FIFO (the ring is in order), and exit_fire pops it below.
      O << "  assign ctx_retire" << LS << " = ctxS" << RG[Lv.FirstRound].S
        << "_head;\n\n";
      for (unsigned K = Lv.FirstRound; K < KEnd2; K++) {
        const RoundGen &G = RG[K];
        const bool First = (K == Lv.FirstRound);
        // Source of this round's IDs: ring entry for the round the head
        // issues, the previous round's state FIFO (at its gather) after that.
        const std::string SrcData =
            First ? "ctx_in" + LS : "ctx_state" + RG[K - 1].S;
        const std::string SrcFire =
            First ? "re_in_fire" + LS : "gather" + RG[K - 1].S;
        // An exiting iteration issues no round at all, so only the FIFOs the
        // head feeds directly need the exit pop.
        const std::string ExitPop =
            First ? " || exit_fire" + LS : std::string();
        O << "  " << TopName << "_fifo #(.WIDTH(CTX_W), .DEPTH(STATE_DEPTH)) "
          << "u_ctxS" << G.S << " (\n";
        O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
        O << "    .in_data(" << SrcData << "), .in_valid(" << SrcFire
          << "), .in_ready(),\n";
        O << "    .out_data(ctxS" << G.S << "_head), .out_valid(),\n";
        O << "    .out_ready(" << G.StateIn << "_fire" << ExitPop << "));\n";
        for (const GlobalSite &S : Sites) {
          if (S.Round != K || !S.Cached)
            continue;
          O << "  " << TopName
            << "_fifo #(.WIDTH(CTX_W), .DEPTH(STATE_DEPTH)) u_ctx_"
            << S.SG->MemReq << " (\n";
          O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
          O << "    .in_data(" << SrcData << "), .in_valid(" << SrcFire
            << "), .in_ready(),\n";
          O << "    .out_data(" << siteCtxHead(*S.SG) << "), .out_valid(),\n";
          O << "    .out_ready(" << S.SG->MemReq << "_fire" << ExitPop
            << "));\n";
        }
      }
      // ctxB bypasses the tail continuation PE on the way to the loop FIFO.
      O << "  " << TopName << "_fifo #(.WIDTH(CTX_W), .DEPTH(STATE_DEPTH)) "
        << "u_ctxB" << LS << " (\n";
      O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
      O << "    .in_data(ctx_state" << RG[KEnd2 - 1].S << "), .in_valid(gather"
        << RG[KEnd2 - 1].S << "), .in_ready(),\n";
      O << "    .out_data(ctxB" << LS << "_head), .out_valid(),\n";
      O << "    .out_ready(cont_loop_fire" << LS << "));\n\n";
    }
  }

  // ── The shared reader instance ─────────────────────────────────────────────
  {
    // Concatenations are MSB-left, so stream j sits at [j*W +: W] when the
    // sites are listed last-first.
    auto vec = [&](auto Field) {
      std::string R = "{";
      for (size_t i = Sites.size(); i-- > 0;)
        R += Field(Sites[i]) + (i ? ", " : "");
      return R + "}";
    };
    auto vec8 = [&](auto Field) {
      std::string R = "{";
      for (size_t i = Sites.size(); i-- > 0;)
        R += "8'd" + std::to_string(Field(Sites[i])) + (i ? ", " : "");
      return R + "}";
    };
    std::string CacheBits;
    for (size_t i = Sites.size(); i-- > 0;)
      CacheBits += Sites[i].Cached ? '1' : '0';

    O << "  // ────────────────────────────────────────────────────────────────────────\n";
    O << "  // All " << NSites
      << " read site(s) on one master; ARID = stream index, per-stream\n";
    O << "  // credits equal the per-stream reply depth so RREADY stays high "
         "and no\n";
    O << "  // site can head-of-line block another.";
    if (AnyCache) {
      O << " Each cached site holds one private\n";
      O << "  // line per resident context; every request, hit or miss, takes "
           "a slot in\n";
      O << "  // that stream's in-order completion queue, so a hit can never "
           "overtake an\n";
      O << "  // older miss (the merge unit pairs the k-th reply with the k-th "
           "state\n";
      O << "  // entry — reordering here silently corrupts results).\n";
    } else {
      O << "\n";
    }
    O << "  // ────────────────────────────────────────────────────────────────────────\n";
    const std::string RdMod =
        TopName + (AnyCache ? "_memreader_cached" : "_memreader_shared");
    O << "  " << RdMod << " #(\n";
    O << "    .N(" << NSites << "), .TASK_W(" << RD_TASK_W << "), .ARG_W("
      << RD_ARG_W << "),\n";
    O << "    .ADDR_W(MEM_ADDR_WIDTH), .BUS_W(MEM_SHARED_DATA_WIDTH),\n";
    O << "    .ID_W(MEM_SHARED_ID_WIDTH),\n";
    if (AnyCache)
      O << "    .NUM_CTX(NUM_CTX), .CTX_W(CTX_W),\n";
    O << "    .ELEM_LOG2_VEC("
      << vec8([](const GlobalSite &S) { return S.ElemLog2; }) << "),\n";
    O << "    .ELEM_W_VEC("
      << vec8([](const GlobalSite &S) { return S.SG->ReplyBits; }) << "),\n";
    O << "    .IDX_W_VEC(" << vec8([](const GlobalSite &S) { return S.IdxW; })
      << "),\n";
    if (AnyCache)
      O << "    .CACHE_EN(" << NSites << "'b" << CacheBits << "),\n";
    // The memReader closure layout is fixed by PrintDef: _cont at byte 0,
    // base at byte 8, idx at byte 16; the reply packet is addr then data.
    O << "    .CONT_LSB(0), .BASE_LSB(64), .IDX_LSB(128),\n";
    O << "    .PKT_ADDR_LSB(0), .PKT_DATA_LSB(64),\n";
    O << "    .OUTSTANDING(MEM_SHARED_OUTSTANDING)"
      << (AnyCache ? ", .CQ_DEPTH(CQ_DEPTH)" : "") << "\n";
    O << "  ) u_memreader_" << (AnyCache ? "cached" : "shared") << " (\n";
    O << "    .ap_clk(ap_clk), .ap_rst_n(ap_rst_n),\n\n";
    O << "    .taskIn_TDATA("
      << vec([](const GlobalSite &S) { return S.SG->MemReq + "_TDATA"; })
      << "),\n";
    O << "    .taskIn_TVALID("
      << vec([](const GlobalSite &S) { return S.SG->MemReq + "_TVALID"; })
      << "),\n";
    O << "    .taskIn_TREADY("
      << vec([](const GlobalSite &S) { return S.SG->MemReq + "_TREADY"; })
      << "),\n";
    if (AnyCache)
      O << "    .taskIn_CTX("
        << vec([&](const GlobalSite &S) {
             return S.Cached ? siteCtxHead(*S.SG)
                             : std::string("{CTX_W{1'b0}}");
           })
        << "),\n";
    O << "\n";
    O << "    .argOut_TDATA("
      << vec([](const GlobalSite &S) { return S.SG->MemArgOut + "_TDATA"; })
      << "),\n";
    O << "    .argOut_TVALID("
      << vec([](const GlobalSite &S) { return S.SG->MemArgOut + "_TVALID"; })
      << "),\n";
    O << "    .argOut_TREADY("
      << vec([](const GlobalSite &S) { return S.SG->MemArgOut + "_TREADY"; })
      << "),\n\n";
    O << "    .argDataOut_TDATA("
      << vec([](const GlobalSite &S) { return S.SG->MemData + "_TDATA"; })
      << "),\n";
    O << "    .argDataOut_TVALID("
      << vec([](const GlobalSite &S) { return S.SG->MemData + "_TVALID"; })
      << "),\n";
    O << "    .argDataOut_TREADY("
      << vec([](const GlobalSite &S) { return S.SG->MemData + "_TREADY"; })
      << "),\n\n";
    for (auto &S : AXI_SIGS)
      O << "    .m_axi_gmem_" << S.Name << "(m_axi_gmem_shared_" << S.Name
        << "),\n";
    if (AnyCache)
      O << "    .dbg_cache_hits(), .dbg_cache_misses()\n";
    else
      O << "    .dbg_ar_grants(), .dbg_no_credit_stalls()\n";
    O << "  );\n\n";
  }

  if (T.Escapes) {
    O << "  // ── Loop exit leaves the wrapper (the exit PE is an external task) ───────\n";
    O << "  assign taskGlobalOut_" << Exit << "_TDATA  = re_exit_TDATA;\n";
    O << "  assign taskGlobalOut_" << Exit << "_TVALID = re_exit_TVALID;\n";
    O << "  assign re_exit_TREADY = taskGlobalOut_" << Exit << "_TREADY;\n\n";
  } else {
    for (unsigned L = 0; L < NL; L++) {
      const OverlapLevel &Lv = T.Levels[L];
      const std::string LS = lsfx(L);
      IRFunction *EF = Lv.ExitTarget;
      const bool ExitHasAXI = TaskInfos.at(EF).HasAXI;

      if (!isLeaf(EF)) {
        // An inner level's exit hands the token back to an enclosing level's
        // head, closing that level's ring through its loopback FIFO.
        IRFunction *Dst = nullptr;
        for (IRFunction *S : EF->Info.SpawnList)
          if (T.Internal.count(S))
            Dst = S;
        const unsigned DL = Dst ? levelOfHead(Dst) : NL;
        if (!Dst || DL >= NL)
          return false;
        emitExitFifo(L, W_EXIT);
        O << "  // ── Exit PE (level " << L << " termination -> level " << DL
          << " loopback) ─────────────\n";
        O << "  " << EF->getName() << " u_exit" << LS << " (\n";
        O << "    .ap_clk(ap_clk), .ap_rst_n(ap_rst_n),\n";
        O << "    .taskIn_TDATA(" << exitW(L) << "_q_TDATA), .taskIn_TVALID("
          << exitW(L) << "_q_TVALID), .taskIn_TREADY(" << exitW(L)
          << "_q_TREADY),\n";
        O << "    .taskGlobalOut_" << Dst->getName() << "_TDATA(" << loopW(DL)
          << "_TDATA),\n";
        O << "    .taskGlobalOut_" << Dst->getName() << "_TVALID(" << loopW(DL)
          << "_TVALID),\n";
        O << "    .taskGlobalOut_" << Dst->getName() << "_TREADY(" << loopW(DL)
          << "_TREADY)" << (ExitHasAXI ? ",\n" : "\n");
        emitAxiConns(EF);
        O << "  );\n\n";
        continue;
      }

      // Leaf exit: the subsystem's real result, reported through argOut /
      // argDataOut. Only the outermost level can own one.
      if (EF != T.Exit)
        return false;
      emitExitFifo(L, W_EXIT);
      O << "  // ── Exit PE (loop termination) ───────────────────────────────────────────\n";
      O << "  " << Exit << " u_exit" << LS << " (\n";
      O << "    .ap_clk(ap_clk), .ap_rst_n(ap_rst_n),\n";
      O << "    .taskIn_TDATA(" << exitW(L) << "_q_TDATA), .taskIn_TVALID("
        << exitW(L) << "_q_TVALID), .taskIn_TREADY(" << exitW(L) << "_q_TREADY)"
        << (ExitDests.empty() && !ExitHasAXI ? "\n" : ",\n");
      // One (argOut[, argDataOut]) pair per destination, named on the exit PE by
      // the MultiExit convention and wired to the per-destination internal trio.
      for (size_t i = 0; i < ExitDests.size(); ++i) {
        const ExitDest &ED = ExitDests[i];
        const std::string W = exitWire(ED);
        bool LastGroup = (i + 1 == ExitDests.size());
        O << "    ." << ED.ExitArgOut << "_TDATA(" << W << "_argOut_TDATA), ."
          << ED.ExitArgOut << "_TVALID(" << W << "_argOut_TVALID), ."
          << ED.ExitArgOut << "_TREADY(" << W << "_argOut_TREADY)"
          << ((ExitHasArgData || !LastGroup || ExitHasAXI) ? ",\n" : "\n");
        if (ExitHasArgData)
          O << "    ." << ED.ExitArgData << "_TDATA(" << W
            << "_argData_TDATA), ." << ED.ExitArgData << "_TVALID(" << W
            << "_argData_TVALID), ." << ED.ExitArgData << "_TREADY(" << W
            << "_argData_TREADY)" << ((!LastGroup || ExitHasAXI) ? ",\n" : "\n");
      }
      emitAxiConns(EF);
      O << "  );\n\n";

      // Route each destination: external -> top ports; internal -> sink.
      for (auto &ED : ExitDests) {
        const std::string W = exitWire(ED);
        if (ED.External) {
          O << "  // '" << ED.F->getName()
            << "' is outside the collapsed loop: expose the result.\n";
          // Outbound skid: same SLR crossing as the inbound task stream, in the
          // other direction (PE -> notifier/scheduler).
          //
          // argOut and argData are INDEPENDENT streams and must each get their
          // own buffer. They were previously merged into one skid latched on
          // argOut's valid, on the assumption that a PE raises both in the same
          // cycle. It does not: the exit PE writes argOut as soon as the
          // continuation address is known, and argData only once the payload
          // has come out of its memory loads and float pipeline. On
          // pageRank_overlap that is a 141-cycle gap, so the merged skid
          // sampled argData while it was still undriven (a zero addr/payload)
          // and then, because it also drove argData_TREADY, consumed and
          // discarded the real beat when it finally arrived — every diffs[]
          // store was written to address 0 with value 0.
          //
          // Joining them properly is not an option either: the two writes are
          // in different stages of the same II=1 pipeline, so holding argOut
          // until argData catches up stalls the stage that produces argData and
          // deadlocks the PE. They are separate channels downstream anyway —
          // argOut decrements a sync counter, argData drives a write buffer —
          // so there is nothing to pair.
          //
          // Widths come from W_EXITARG, the arg_out closure's real width; a
          // hard-coded 256 is right only for arg types that happen to pack into
          // 32 bytes and truncates the rest without a warning.
          O << "  // ── Outbound skids: register the PE -> notifier result streams ─────────\n";
          O << "  // argOut and argData are independent channels with independent\n";
          O << "  // valids (the PE raises them cycles apart), so each is buffered\n";
          O << "  // on its own; merging them drops one of the two.\n";
          O << "  wire [63:0] " << W << "_out_q;\n";
          O << "  wire " << W << "_out_q_valid;\n";
          O << "  wire " << W << "_out_q_ready;\n";
          O << "  " << TopName << "_mrc_skid #(.WIDTH(64)) u_" << W
            << "_out_skid (\n";
          O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
          O << "    .i_data(" << W << "_argOut_TDATA),\n";
          O << "    .i_valid(" << W << "_argOut_TVALID), .i_ready(" << W
            << "_out_q_ready),\n";
          O << "    .o_data(" << W << "_out_q), .o_valid(" << W
            << "_out_q_valid),\n";
          O << "    .o_ready(" << ED.TopArgOut << "_TREADY)\n";
          O << "  );\n";
          O << "  assign " << ED.TopArgOut << "_TDATA  = " << W << "_out_q;\n";
          O << "  assign " << ED.TopArgOut << "_TVALID = " << W
            << "_out_q_valid;\n";
          O << "  assign " << W << "_argOut_TREADY = " << W << "_out_q_ready;\n";
          if (ExitHasArgData) {
            O << "  wire [" << (W_EXITARG - 1) << ":0] " << W << "_argdata_q;\n";
            O << "  wire " << W << "_argdata_q_valid;\n";
            O << "  wire " << W << "_argdata_q_ready;\n";
            O << "  " << TopName << "_mrc_skid #(.WIDTH(" << W_EXITARG
              << ")) u_" << W << "_argdata_skid (\n";
            O << "    .clk(ap_clk), .rst_n(ap_rst_n),\n";
            O << "    .i_data(" << W << "_argData_TDATA),\n";
            O << "    .i_valid(" << W << "_argData_TVALID), .i_ready(" << W
              << "_argdata_q_ready),\n";
            O << "    .o_data(" << W << "_argdata_q), .o_valid(" << W
              << "_argdata_q_valid),\n";
            O << "    .o_ready(" << ED.TopArgData << "_TREADY)\n";
            O << "  );\n";
            O << "  assign " << ED.TopArgData << "_TDATA  = " << W
              << "_argdata_q;\n";
            O << "  assign " << ED.TopArgData << "_TVALID = " << W
              << "_argdata_q_valid;\n";
            O << "  assign " << W << "_argData_TREADY = " << W
              << "_argdata_q_ready;\n";
          }
        } else {
          O << "  // '" << ED.F->getName()
            << "' is internal to the loop (fed by the merge unit); sink the "
               "exit's path.\n";
          O << "  assign " << W << "_argOut_TREADY = 1'b1;\n";
          if (ExitHasArgData)
            O << "  assign " << W << "_argData_TREADY = 1'b1;\n";
        }
      }
      O << "\n";
    }
  }
  O << "endmodule\n";

  // Guard: no memory-accessing PE may be left with a dangling bundle.
  for (auto &M : MemPEs) {
    if (AxiWired.count(M.F))
      continue;
    PANIC("#pragma BOMBYX OVERLAP: wrapper '%s' surfaces an AXI bundle for PE "
          "'%s' but its instance '%s' has no m_axi connections; the PE would "
          "stall on an undriven ARREADY",
          TopName.c_str(), M.F->getName().c_str(), M.Inst.c_str());
  }
  return true;
}
