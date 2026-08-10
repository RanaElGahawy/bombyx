#pragma once
//
// Deep-state dataflow rewrite
// ===========================
//
// A HardCilk PE is emitted as one free-running flushable pipeline:
//
//     #pragma HLS PIPELINE II = 1 style = flp
//     <T>_task args = taskIn.read();
//     x = MEM_ARR_IN(mem_0, args.p, args.i, float);   // m_axi load
//     <build the outgoing closure from args and x>
//
// Vitis schedules the m_axi load at the port's `latency` (64 by default), so the
// pipeline is ~70-100 stages deep, and because the generated C reads the whole
// task struct and writes the whole struct back out, EVERY stage registers the
// entire closure. Measured on pageRank_overlap's applyFn_reentry1_cont1: 96
// stages, 73,504 register bits, 98.7% of them pipeline-stage registers. Across
// the eight applyFn PEs that is ~890k of the kernel's 1.06M flops, which is what
// pushes SLR1/SLR2 CLB occupancy to 78%/74% while LUTs sit at 30%.
//
// The fix is to stop carrying the closure through the latency. Split the PE into
// dataflow processes joined by FIFOs, and put the wide closure in a BRAM FIFO
// that the latency flows around rather than through:
//
//     issue  : args -> per-site address FIFOs (64b), carry FIFOs (narrow),
//                      ctx FIFO (the whole closure, BRAM)
//     load_k : address -> data                      <- the m_axi latency is here
//     compute: carry + data -> result (narrow)      <- float-op latency is here
//     merge  : ctx + result -> outgoing closure     <- ~0 latency, II=1
//
// Only narrow values cross the deep stages; the closure sits in BRAM until the
// last, shallow stage. Measured on applyFn (3 load sites, no post-load math),
// latency=32 num_read_outstanding=64: FF 32,397 -> 4,094 (-87%), LUT 3,194 ->
// 5,078, BRAM18K 6 -> 21. Estimated Fmax is unchanged (411 MHz).
//
// ORDER PRESERVATION -- why this is safe inside an OVERLAP ring
// ------------------------------------------------------------
// The generated OVERLAP wrapper pairs the k-th memory reply with the k-th entry
// of a side-channel context FIFO, positionally, with no tag comparison
// (u_ctxS / u_ctx_re_mem / u_ctxS_1 / u_ctx_iss_mem_1 / u_ctxB in
// HardCilkOverlapWrapperGen). A PE that let one task overtake another would
// desync those IDs and misattribute results SILENTLY. The rewrite preserves
// order because: every channel is a FIFO; every stage is unconditionally
// 1-in/1-out per iteration (guaranteed by the Straightline/SingleAssignment
// preconditions below); and each load site owns its own `channel =` sub-port and
// therefore its own AXI ID, so its read data returns in order. That is the same
// guarantee the flushable pipeline already relies on -- the rewrite does not
// weaken it.
//
#include "core/IR.hpp"

#include <map>
#include <string>
#include <vector>

namespace deepstate {

/// What to do about a deep-state PE.
enum class Mode {
  /// Leave every PE exactly as it was: one flushable pipeline at the default
  /// m_axi latency. Byte-identical output to a compiler without this pass.
  None,
  /// Keep the flushable pipeline but shorten it, by telling Vitis the m_axi
  /// read returns sooner than its 64-cycle default. Pipeline depth, and hence
  /// the number of copies of the closure, falls proportionally. Measured on
  /// pageRank_overlap at latency = 16: applyFn 32,397 -> 12,189 FF and
  /// applyFn_reentry1_cont1 74,313 -> 37,353 FF, with LUT counts falling too
  /// (3,194 -> 2,522 and 2,601 -> 1,929) and BRAM unchanged. This is a pragma
  /// change only -- no new RTL, no new structure.
  Latency,
  /// Split the PE into dataflow stages so the closure sits in a BRAM FIFO
  /// instead of in pipeline registers (the -90% result described above).
  ///
  /// Three things had to be true at once for this to work, and all three are
  /// now emitted (verified on pageRank_overlap: 45/45 sweep points bit-exact,
  /// 0.3796 contributions/PE/cycle vs the flat pipeline's 0.399, FF 32,397 ->
  /// 4,585 and 74,313 -> 5,832):
  ///
  ///   1. Every stage body must be a FINITE function, not `for (;;)`. The
  ///      `start_for_<stage>` FIFO that gates a downstream process is written
  ///      by the *upstream process completing an invocation*
  ///      (`df_issue_U0.start_write`), not once at reset. A stage that never
  ///      returns never produces a start token, so merge never starts and the
  ///      region deadlocks with `df_ctx_read` low forever. This was the bug.
  ///   2. `config_dataflow -start_fifo_depth N` in the PE's TCL. The default of
  ///      2 caps the region at two invocations in flight, i.e. one task per
  ///      (region latency / 2), which measured 10x below the flat pipeline. No
  ///      source pragma exists; see Opts::Inflight and PrintVitisHLSTaskTcl.
  ///   3. A LARGE m_axi `latency` (Opts::DFMAxiLatency), not the small one
  ///      Mode::Latency wants. See that field.
  ///
  /// Ruled out by experiment and NOT needed: `hls::task`/`hls_thread_local`
  /// (produces numerically identical RTL once the bodies return), pipeline
  /// `style=`, and `max_read_burst_length`.
  Dataflow,
};

/// Tunables. Defaults are the measured operating point (see the sweep above).
struct Opts {
  Mode M = Mode::Latency;
  /// Rewrite a PE only when the flushable pipeline would register at least this
  /// many bits of closure state. avg(taskIn, taskOut) width * estimated depth.
  unsigned MinStateBits = 20000;
  /// `latency =` on the m_axi ports of a PE kept as a flushable pipeline
  /// (Mode::Latency, and Mode::Dataflow PEs that failed a precondition),
  /// replacing Vitis's default of 64. Shortening it is the whole point of that
  /// mode: the pipeline depth, and hence the number of copies of the closure
  /// registered in it, falls proportionally. It is only a scheduling hint: if
  /// memory actually takes longer the pipeline stalls, it does not misbehave,
  /// and the OVERLAP wrapper's own outstanding-request machinery absorbs the
  /// rest.
  unsigned MAxiLatency = 16;
  /// `latency =` on the m_axi ports of a PE actually rewritten into a DATAFLOW
  /// region. Deliberately the OPPOSITE of MAxiLatency, and deliberately a
  /// separate knob rather than a reuse of it:
  ///
  ///   * In the flat pipeline the declared latency multiplies the closure, so
  ///     small is good.
  ///   * In the dataflow form the load process registers nothing but a 64-bit
  ///     address, so depth there is nearly free — and that depth is exactly
  ///     what absorbs a late memory reply. Declaring 16 while memory answers in
  ///     24 or 61 cycles makes the load stage's fixed schedule stall on every
  ///     reply, and measured throughput then tracks memory latency
  ///     (0.0699 / 0.0553 / 0.0273 contributions/PE/cycle at memLat 4 / 24 /
  ///     61). At 64 it becomes latency-independent, and with the start-FIFO
  ///     depth raised too, 0.4364 / 0.3796 / 0.3049 — ~10x. The cost is ~1,200
  ///     FF and ~2,300 LUT across pageRank_overlap's two DATAFLOW PEs, in the
  ///     m_axi read pipeline rather than in the closure.
  unsigned DFMAxiLatency = 64;
  /// Depth of every dataflow FIFO, `num_read_outstanding` on the m_axi ports,
  /// and the `config_dataflow -start_fifo_depth` written into the PE's TCL.
  /// Bounds the tasks in flight inside the PE. The start-FIFO depth must not
  /// exceed the channel depth, or the start tokens let `issue` run further
  /// ahead than the ctx FIFO can hold; using one number for both is the
  /// simplest way to keep that true. 0 is rejected by Vitis HLS 2024.1.
  unsigned Inflight = 64;
};

/// One m_axi read site lifted into its own `load` process.
struct Site {
  const IRExpr *E = nullptr; ///< the IndexIRExpr / DRefIRExpr the printer emits
  unsigned Chan = 0;         ///< m_axi channel (== AXI ID) serving it
  std::string ElemTy;        ///< C type of the loaded value
  std::string AddrStream;    ///< df_addr<k>
  std::string DataStream;    ///< df_data<k>
  std::string ValueVar;      ///< _d<k>, the name the loaded value prints as
  /// Which stage consumes the value. A site read by both stages would have to
  /// be forwarded through compute; no kernel needs that, so it is rejected
  /// (SplitSite) rather than half-supported.
  bool InCompute = false;
};

/// A variable handed between stages on its own narrow stream. Per-variable
/// streams rather than one packed struct: it costs a few small FIFOs and saves
/// inventing struct types that would also have to reach the defs header.
struct Xfer {
  IRVarRef Var = nullptr;
  std::string Stream; ///< df_carry_<name> / df_res_<name>
  std::string Local;  ///< _c_<name> / _r_<name>
  std::string CTy;    ///< C type
};

/// One statement evaluated in the `compute` stage, with the variable it defines.
struct ComputeStep {
  IRStmt *S = nullptr;
  IRVarRef Def = nullptr;
  std::string Local; ///< _r_<name>, the local compute assigns
  std::string CTy;
  /// True when `merge` reads this value, so it needs a stream out of compute.
  /// A purely internal temporary is left inside compute -- streaming it would
  /// create a dataflow channel nobody reads, which deadlocks the region.
  bool Streamed = false;
};

struct Plan {
  /// Emit the dataflow rewrite for this PE.
  bool Apply = false;
  /// Emit a shortened m_axi latency for this PE, keeping the flushable
  /// pipeline. Decided from the state estimate alone, so it does not depend on
  /// any of the code-splitting preconditions below.
  bool ApplyLatency = false;
  /// Why the PE was left alone. Always set when !Apply, for the report.
  std::string SkipReason;

  std::vector<Site> Sites;
  /// Statements evaluated in the `compute` stage, in program order: the
  /// definitions of load-dependent variables that are not a bare load. Empty
  /// when every load-dependent variable is assigned straight from a load, in
  /// which case there is no compute stage at all and `merge` consumes the data
  /// streams directly (this is the pageRank `applyFn` shape).
  std::vector<ComputeStep> Compute;
  /// Statements evaluated in `merge`: everything else, in program order.
  std::vector<IRStmt *> MergeStmts;
  /// ARG values the compute stage needs that are not themselves load-dependent.
  std::vector<Xfer> Carry;
  /// Compute results merge consumes; parallel to the Streamed ComputeSteps.
  std::vector<Xfer> Result;
  /// Load-dependent vars assigned straight from a load: these alias the load's
  /// value variable and need no stream of their own.
  IRVarMapTy<std::string> TrivialLoadVar;

  unsigned StateBitsBefore = 0;
  unsigned EstDepth = 0;

  bool hasCompute() const { return !Compute.empty(); }
};

/// Decide whether `Task` can and should be rewritten.
///
/// `Sites` must be the ordered read-site list from planMemChannels (the same
/// nodes the printer emits as MEM_* macros) with their channel assignment;
/// passing the printer's own list is what keeps the two in step. `ReadOnly` is
/// planMemChannels' verdict (!Blocked): true when the task provably performs no
/// m_axi store. When it is false the caller has no site list to give, so
/// `Sites` is empty and only Mode::Latency can produce a plan.
///
/// The read-only requirement belongs to the DATAFLOW rewrite alone. Mode::Latency
/// changes nothing but the `latency =` scheduling hint on the m_axi port, which
/// is safe whether the port is read, written, or both -- and a WRITING PE is
/// exactly the case that needs it most, since the store keeps the whole closure
/// alive across the declared latency. randomWalk_overlap's three applyFn PEs all
/// store, and gating latency behind read-only left them at the 64-cycle default:
/// 71/73/68 stages, 169k register bits, and a stall-enable net fanning out to
/// every stage's CE.
///
/// Preconditions checked (each records a SkipReason):
///   StateBits      -- the estimated saving clears Opts::MinStateBits. Checked
///                     first, and for both modes: it is the only gate
///                     Mode::Latency has.
///   ReadOnly       -- DATAFLOW only: at least one read site, and no m_axi store
///   SiteKind       -- every site is MEM_ARR_IN / MEM_IN; MEM_STRUCT sites are
///                     not lifted (the field type is not available here)
///   Straightline   -- no LoopIRStmt and no IfIRStmt anywhere in the body. A
///                     conditional closure write would break the wrapper's
///                     1-in/1-out contract, which is enforced structurally and
///                     fails silently.
///   AddrFromArgs   -- every read address depends only on ARG variables and
///                     literals, so `issue` needs nothing but `args`
///   SingleAssign   -- every variable is assigned at most once, so "the value of
///                     v" is unambiguous once statements are split across stages
///   ComputeInputs  -- every variable a compute statement reads is either
///                     load-dependent or an unassigned ARG (carried)
/// The right-hand side of an assignment statement (CopyIRStmt / StoreIRStmt to a
/// plain ident), or null. Exposed so the emitter can print a compute step's
/// value expression without its destination.
IRExpr *srcExpr(IRStmt *S);

Plan plan(IRFunction *Task, const std::vector<const IRExpr *> &Sites,
          const std::map<const IRExpr *, unsigned> &SiteChan,
          unsigned TaskInBits, unsigned TaskOutBits, const Opts &O,
          bool ReadOnly);

} // namespace deepstate
