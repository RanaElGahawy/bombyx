// ===========================================================================
// SystemC behavioural model of a #pragma BOMBYX OVERLAP dataflow wrapper.
//
// Purpose: measure the sustained comparison rate (II) of the collapsed
// applyFn loop nest against a memory with a realistic latency (>= 128 cycles),
// before committing to a multi-hour Vitis HLS + RTL run.
//
// Fidelity notes — what is real and what is modelled:
//   * PE BEHAVIOUR is real: every processing element is the C++ function
//     bombyx-cc generated for it, compiled and invoked here. No PE logic is
//     re-implemented, so a functional mismatch is a compiler bug, not a model
//     bug.
//   * WRAPPER STRUCTURE mirrors the generated Verilog one-for-one: per-level
//     loop-back FIFO + credit counter + 2:1 arbiter, and per-round state FIFO +
//     memory reader + in-order merge unit.
//   * PE TIMING is modelled: each PE is a pipeline of depth PE_LATENCY that
//     accepts one token every II cycles. A PE that writes N tokens to one
//     stream per invocation has II = N, which is what Vitis HLS produces.
//   * MEMORY is modelled: fixed MEM_LATENCY, unlimited outstanding reads, one
//     request accepted per cycle per reader — the optimistic case, so an II
//     measured here is a lower bound on the RTL's.
//
// The authority on the final number is the RTL run (verilator --sc over the
// generated wrapper with Vitis-synthesised PEs); this model exists to size the
// FIFOs and find structural stalls cheaply.
// ===========================================================================

#include <systemc.h>

#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "hls_stream.h"
#include "triangleDAEOptimalCompact_defs.h"

// The generated PE implementations.
namespace pes {
#include "triangleDAEOptimalCompact.cpp"
}

// ─── Tunables ───────────────────────────────────────────────────────────────
static int MEM_LATENCY = 128; // cycles from read request to reply
static int PE_LATENCY = 8;    // HLS pipeline depth of a PE
static int STATE_DEPTH = 512; // per-round state FIFO and per-level loopback FIFO
static int NTASK = 0;         // applyFn closures injected (0 => one per vertex)
// OverlapMemAnalysis emits one reader per read SITE, so a multi-site round has
// one request port and one reply channel per site and the merge unit joins them
// in the same cycle. The superseded one-reader-per-round shape measured 2.04
// cycles/comparison against this model's 1.13; it is no longer reproducible here
// because the generated PE signatures changed with it.

// ─── Primitives mirroring the generated RTL ─────────────────────────────────

template <typename T> struct Fifo {
  std::deque<T> q;
  size_t depth;
  std::string name;
  Fifo(size_t d = 16, std::string n = "") : depth(d), name(std::move(n)) {}
  bool empty() const { return q.empty(); }
  size_t room() const { return depth > q.size() ? depth - q.size() : 0; }
  const T &head() const { return q.front(); }
  void push(const T &t) { q.push_back(t); }
  T pop() {
    T t = q.front();
    q.pop_front();
    return t;
  }
};

// A fixed-latency pipeline: tokens pushed at cycle c emerge at cycle c+lat.
template <typename T> struct DelayLine {
  std::deque<std::pair<long long, T>> q;
  int lat;
  DelayLine(int l = 1) : lat(l) {}
  void push(long long now, const T &t) { q.push_back({now + lat, t}); }
  bool ready(long long now) const { return !q.empty() && q.front().first <= now; }
  T pop() {
    T t = q.front().second;
    q.pop_front();
    return t;
  }
  size_t size() const { return q.size(); }
};

// 2:1 round-robin arbiter (the generated <Top>_arb2).
struct Arb2 {
  bool pick_b = false;
  // Returns 0 to take from a, 1 from b, -1 if neither is available.
  int grant(bool a_valid, bool b_valid) {
    if (a_valid && b_valid) {
      int g = pick_b ? 1 : 0;
      pick_b = !pick_b;
      return g;
    }
    if (a_valid)
      return 0;
    if (b_valid)
      return 1;
    return -1;
  }
};

// ─── Device memory ──────────────────────────────────────────────────────────
// Byte-addressed image; a "pointer" in a closure is a byte offset into it,
// exactly as MEM_ARR_IN in the generated defs treats it.
static std::vector<uint8_t> DevMem;

// ─── Graph under test ───────────────────────────────────────────────────────
struct Graph {
  uint32_t vertexCount = 0;
  uint64_t pGraphOff = 0;    // byte offset of the pGraph array
  uint64_t triCountsOff = 0; // byte offset of the result array
  std::vector<std::vector<uint32_t>> adj;
};

// Build a DAE-filtered graph: every vertex gets a sorted neighbour list, and
// the expected triangle count is computed by direct scalar intersection.
static Graph buildGraph(uint32_t V, uint32_t deg) {
  Graph G;
  G.vertexCount = V;
  G.adj.assign(V, {});
  for (uint32_t u = 0; u < V; u++)
    for (uint32_t k = 1; k <= deg; k++) {
      uint32_t v = (u + k * 3) % V;
      if (v != u)
        G.adj[u].push_back(v);
    }
  for (auto &a : G.adj) {
    std::sort(a.begin(), a.end());
    a.erase(std::unique(a.begin(), a.end()), a.end());
  }

  // Lay out: [pGraph: 2*V uint64][neighbour lists][triangleCounts: V uint32]
  const uint64_t base = 64;
  G.pGraphOff = base;
  uint64_t cur = base + 16ull * V;
  std::vector<uint64_t> listOff(V);
  for (uint32_t u = 0; u < V; u++) {
    listOff[u] = cur;
    cur += 4ull * G.adj[u].size();
    cur = (cur + 7) & ~7ull;
  }
  G.triCountsOff = cur;
  cur += 4ull * V;
  DevMem.assign(cur + 64, 0);

  for (uint32_t u = 0; u < V; u++) {
    uint64_t ptr = G.adj[u].empty() ? 0 : listOff[u];
    std::memcpy(&DevMem[G.pGraphOff + 16ull * u], &ptr, 8);
    uint64_t d = G.adj[u].size();
    std::memcpy(&DevMem[G.pGraphOff + 16ull * u + 8], &d, 8);
    for (size_t j = 0; j < G.adj[u].size(); j++)
      std::memcpy(&DevMem[listOff[u] + 4 * j], &G.adj[u][j], 4);
  }
  return G;
}

// The exact algorithm applyFn implements, run in plain C++ for the golden.
static uint64_t referenceTriangles(const Graph &G, std::vector<uint32_t> &per) {
  per.assign(G.vertexCount, 0);
  uint64_t total = 0;
  for (uint32_t u = 0; u < G.vertexCount; u++) {
    const auto &nu = G.adj[u];
    uint32_t count = 0;
    for (size_t i = 0; i < nu.size(); i++) {
      const auto &nv = G.adj[nu[i]];
      size_t k = 0, l = 0;
      uint32_t c_i = 0;
      while (k < nu.size() && l < nv.size()) {
        if (nu[k] == nv[l]) { c_i++; k++; l++; }
        else if (nu[k] < nv[l]) k++;
        else l++;
      }
      count += c_i;
    }
    per[u] = count;
    total += count;
  }
  return total;
}

// ─── The wrapper model ──────────────────────────────────────────────────────

SC_MODULE(OverlapWrapper) {
  sc_in<bool> clk;

  Graph G;
  std::vector<uint32_t> expectedPer;
  uint64_t expectedTotal = 0;

  // Level 0 ring nets.
  Fifo<applyFn_task> taskIn{64, "taskIn"};
  Fifo<applyFn_reentry0_task> root_out{4, "root_out"};
  Fifo<applyFn_reentry0_task> cont_loop{(size_t)STATE_DEPTH, "cont_loop"};
  Fifo<applyFn_reentry0_task> re_in{4, "re_in"};
  Fifo<applyFn_exit0_task> re_exit{4, "re_exit"};
  // Level 1 ring nets.
  Fifo<applyFn_reentry0_reentry1_task> lvlin_l1{4, "lvlin_l1"};
  Fifo<applyFn_reentry0_reentry1_task> cont_loop_l1{(size_t)STATE_DEPTH,
                                                    "cont_loop_l1"};
  Fifo<applyFn_reentry0_reentry1_task> re_in_l1{4, "re_in_l1"};
  Fifo<applyFn_reentry0_exit1_task> re_exit_l1{4, "re_exit_l1"};

  // Round 0: memrd0 -> applyFn_reentry0_cont0
  Fifo<memrd0_task> re_mem{16, "re_mem"};
  Fifo<applyFn_reentry0_cont0_task> re_state{(size_t)STATE_DEPTH, "re_state"};
  Fifo<uint32_t_arg_out> mem_data{16, "mem_data"};
  Fifo<applyFn_reentry0_cont0_task> cont_in{4, "cont_in"};
  // Round 1: memrd1 -> applyFn_reentry0_cont1
  Fifo<memrd1_task> iss_mem_1{16, "iss_mem_1"};
  Fifo<applyFn_reentry0_cont1_task> iss_state_1{(size_t)STATE_DEPTH,
                                                "iss_state_1"};
  Fifo<uint64_t_arg_out> mem_data_1{16, "mem_data_1"};
  Fifo<memrd2_task> iss_mem_1b{16, "iss_mem_1b"};
  Fifo<uint64_t_arg_out> mem_data_1b{16, "mem_data_1b"};
  Fifo<applyFn_reentry0_cont1_task> cont_in_1{4, "cont_in_1"};
  // Round 2: memrd2 -> applyFn_reentry0_reentry1_cont0
  Fifo<memrd3_task> iss_mem_2{16, "iss_mem_2"};
  Fifo<applyFn_reentry0_reentry1_cont0_task> iss_state_2{(size_t)STATE_DEPTH,
                                                         "iss_state_2"};
  Fifo<uint32_t_arg_out> mem_data_2{16, "mem_data_2"};
  // Per-site readers: site 1's own request queue, reader and reply channel.
  Fifo<memrd4_task> iss_mem_2b{16, "iss_mem_2b"};
  Fifo<uint32_t_arg_out> mem_data_2b{16, "mem_data_2b"};
  Fifo<applyFn_reentry0_reentry1_cont0_task> cont_in_2{4, "cont_in_2"};

  // Memory read pipelines (unlimited outstanding, fixed latency).
  DelayLine<uint32_t_arg_out> mrd0{MEM_LATENCY};
  DelayLine<uint64_t_arg_out> mrd1{MEM_LATENCY};
  DelayLine<uint64_t_arg_out> mrd1b{MEM_LATENCY};
  DelayLine<uint32_t_arg_out> mrd2{MEM_LATENCY};
  DelayLine<uint32_t_arg_out> mrd2b{MEM_LATENCY};

  // PE output pipelines, one per PE, modelling HLS pipeline depth.
  DelayLine<applyFn_reentry0_task> pl_root{PE_LATENCY};
  DelayLine<std::pair<int, applyFn_reentry0_task>> pl_unused{PE_LATENCY};

  Arb2 arb0, arb1;
  long long inflight0 = 0, inflight1 = 0;
  long long MAX_INFLIGHT = STATE_DEPTH - 8;

  // Merge-unit state, one per round.
  struct Merge {
    unsigned rcount = 0;
    bool outbuf_v = false;
  };
  Merge mg0, mg1, mg2;

  // PE issue-interval counters (cycles until the PE can accept again).
  std::map<std::string, int> peBusy;

  // ── Instrumentation ──
  long long cyc = 0;
  long long comparisons = 0;   // inner-loop iterations retired (cont_2 fires)
  long long firstCompare = -1, lastCompare = -1;
  long long mem2Reqs = 0;
  long long tasksDone = 0;
  long long stall_reentry1_noinput = 0, stall_reentry1_nomem = 0;
  long long stall_merge2_nostate = 0, stall_merge2_noreply = 0;
  long long stall_cont2_backpressure = 0;
  std::vector<uint32_t> gotPer;
  uint64_t gotTotal = 0;

  void dumpOccupancy() {
    auto p = [](const char *n, size_t q, size_t d) {
      std::printf("    %-16s %5zu / %-5zu%s\n", n, q, d, q >= d ? "  FULL" : "");
    };
    std::printf("  FIFO occupancy at stall:\n");
    p("taskIn", taskIn.q.size(), taskIn.depth);
    p("root_out", root_out.q.size(), root_out.depth);
    p("re_in", re_in.q.size(), re_in.depth);
    p("re_mem", re_mem.q.size(), re_mem.depth);
    p("re_state", re_state.q.size(), re_state.depth);
    p("mem_data", mem_data.q.size(), mem_data.depth);
    p("cont_in", cont_in.q.size(), cont_in.depth);
    p("iss_mem_1", iss_mem_1.q.size(), iss_mem_1.depth);
    p("iss_state_1", iss_state_1.q.size(), iss_state_1.depth);
    p("mem_data_1", mem_data_1.q.size(), mem_data_1.depth);
    p("cont_in_1", cont_in_1.q.size(), cont_in_1.depth);
    p("lvlin_l1", lvlin_l1.q.size(), lvlin_l1.depth);
    p("re_in_l1", re_in_l1.q.size(), re_in_l1.depth);
    p("iss_mem_2", iss_mem_2.q.size(), iss_mem_2.depth);
    p("iss_state_2", iss_state_2.q.size(), iss_state_2.depth);
    p("mem_data_2", mem_data_2.q.size(), mem_data_2.depth);
    p("cont_in_2", cont_in_2.q.size(), cont_in_2.depth);
    p("cont_loop_l1", cont_loop_l1.q.size(), cont_loop_l1.depth);
    p("re_exit_l1", re_exit_l1.q.size(), re_exit_l1.depth);
    p("cont_loop", cont_loop.q.size(), cont_loop.depth);
    p("re_exit", re_exit.q.size(), re_exit.depth);
    std::printf("    inflight0 %lld / %lld, inflight1 %lld / %lld\n", inflight0,
                MAX_INFLIGHT, inflight1, MAX_INFLIGHT);
  }

  SC_CTOR(OverlapWrapper) {
    SC_METHOD(tick);
    sensitive << clk.pos();
    dont_initialize();
  }

  // Can this PE accept a token this cycle?
  bool peReady(const std::string &n) {
    auto it = peBusy.find(n);
    return it == peBusy.end() || it->second <= 0;
  }
  void peFire(const std::string &n, int ii) { peBusy[n] = ii; }

  // Gather one reply into a round's merge unit. Returns true when the closure
  // is complete and was pushed to the continuation's input.
  template <typename ContT, typename ReplyT>
  void mergeStep(Merge &M, Fifo<ContT> &state, Fifo<ReplyT> &reply,
                 Fifo<ContT> &out, unsigned nreply,
                 void (*apply)(ContT &, unsigned, const ReplyT &),
                 long long *noState, long long *noReply) {
    if (out.room() == 0)
      return;
    if (state.empty()) {
      if (!reply.empty() && noState)
        (*noState)++;
      return;
    }
    if (reply.empty()) {
      if (noReply)
        (*noReply)++;
      return;
    }
    ReplyT r = reply.pop();
    // The state head is held across all NREPLY gathers, exactly as the RTL's
    // out_data holds while out_valid && !out_ready.
    ContT &head = const_cast<ContT &>(state.head());
    apply(head, M.rcount, r);
    M.rcount++;
    if (M.rcount == nreply) {
      ContT done = state.pop();
      M.rcount = 0;
      out.push(done);
    }
  }

  void tick();
  void report();
};

// Reply field placement, mirroring the merge unit's bit-slice assignments.
static void applyR0(applyFn_reentry0_cont0_task &c, unsigned,
                    const uint32_t_arg_out &r) {
  c.v = (uint32_t)r.data;
}
static void applyR1(applyFn_reentry0_cont1_task &c, unsigned k,
                    const uint64_t_arg_out &r) {
  // Round 1's first read is cast-wrapped (`(uint32_t *)pGraph[2*v]`), so
  // OverlapMemAnalysis lands it in a __bombyx_memrd temporary and re-applies the
  // cast after the sync — matching nxt_1[607:544] in the generated merge unit.
  if (k == 0)
    c.__bombyx_memrd0 = (uint64_t)r.data;
  else
    c.degree_v = (uint32_t)r.data;
}
static void applyR2(applyFn_reentry0_reentry1_cont0_task &c, unsigned k,
                    const uint32_t_arg_out &r) {
  if (k == 0)
    c.a_i = (uint32_t)r.data;
  else
    c.b_j = (uint32_t)r.data;
}

void OverlapWrapper::tick() {
  cyc++;
  for (auto &kv : peBusy)
    if (kv.second > 0)
      kv.second--;

  void *MEM = DevMem.data();

  // ── Memory reply pipelines -> merge input FIFOs ──────────────────────────
  if (mrd0.ready(cyc) && mem_data.room()) mem_data.push(mrd0.pop());
  if (mrd1.ready(cyc) && mem_data_1.room()) mem_data_1.push(mrd1.pop());
  if (mrd1b.ready(cyc) && mem_data_1b.room()) mem_data_1b.push(mrd1b.pop());
  if (mrd2.ready(cyc) && mem_data_2.room()) mem_data_2.push(mrd2.pop());
  if (mrd2b.ready(cyc) && mem_data_2b.room()) mem_data_2b.push(mrd2b.pop());


  // ── Merge units: join all of a round's parallel reply channels in one cycle ─
  // Round 2 (a_i, b_j) — the comparison round.
  if (cont_in_2.room() && !iss_state_2.empty()) {
    if (!mem_data_2.empty() && !mem_data_2b.empty()) {
      applyFn_reentry0_reentry1_cont0_task c = iss_state_2.pop();
      applyR2(c, 0, mem_data_2.pop());
      applyR2(c, 1, mem_data_2b.pop());
      cont_in_2.push(c);
    } else {
      stall_merge2_noreply++;
    }
  } else if ((!mem_data_2.empty() || !mem_data_2b.empty()) &&
             iss_state_2.empty()) {
    stall_merge2_nostate++;
  }
  // Round 1 (neighbors_v, degree_v).
  if (cont_in_1.room() && !iss_state_1.empty() && !mem_data_1.empty() &&
      !mem_data_1b.empty()) {
    applyFn_reentry0_cont1_task c = iss_state_1.pop();
    applyR1(c, 0, mem_data_1.pop());
    applyR1(c, 1, mem_data_1b.pop());
    cont_in_1.push(c);
  }
  // Round 0 (v) — a single site, so the join is degenerate.
  if (cont_in.room() && !re_state.empty() && !mem_data.empty()) {
    applyFn_reentry0_cont0_task c = re_state.pop();
    applyR0(c, 0, mem_data.pop());
    cont_in.push(c);
  }

  // ── memReader PEs: one per read site, one request per cycle each ─────────
  if (!re_mem.empty() && peReady("memrd0")) {
    hls::stream<memrd0_task> ti; hls::stream<uint64_t> ao;
    hls::stream<uint32_t_arg_out> ad;
    ti.write(re_mem.pop());
    pes::memrd0(MEM, ti, ao, ad);
    mrd0.push(cyc, ad.read());
    peFire("memrd0", 1);
  }
  if (!iss_mem_1.empty() && peReady("memrd1")) {
    hls::stream<memrd1_task> ti; hls::stream<uint64_t> ao;
    hls::stream<uint64_t_arg_out> ad;
    ti.write(iss_mem_1.pop());
    pes::memrd1(MEM, ti, ao, ad);
    mrd1.push(cyc, ad.read());
    peFire("memrd1", 1);
  }
  if (!iss_mem_1b.empty() && peReady("memrd2")) {
    hls::stream<memrd2_task> ti; hls::stream<uint64_t> ao;
    hls::stream<uint64_t_arg_out> ad;
    ti.write(iss_mem_1b.pop());
    pes::memrd2(MEM, ti, ao, ad);
    mrd1b.push(cyc, ad.read());
    peFire("memrd2", 1);
  }
  if (!iss_mem_2.empty() && peReady("memrd3")) {
    hls::stream<memrd3_task> ti; hls::stream<uint64_t> ao;
    hls::stream<uint32_t_arg_out> ad;
    ti.write(iss_mem_2.pop());
    pes::memrd3(MEM, ti, ao, ad);
    mrd2.push(cyc, ad.read());
    peFire("memrd3", 1);
  }
  if (!iss_mem_2b.empty() && peReady("memrd4")) {
    hls::stream<memrd4_task> ti; hls::stream<uint64_t> ao;
    hls::stream<uint32_t_arg_out> ad;
    ti.write(iss_mem_2b.pop());
    pes::memrd4(MEM, ti, ao, ad);
    mrd2b.push(cyc, ad.read());
    peFire("memrd4", 1);
  }

  // ── Level 1 tail continuation: the COMPARISON stage ──────────────────────
  // Retires one inner-loop iteration (one a_i vs b_j comparison) per firing.
  if (!cont_in_2.empty() && peReady("cont2")) {
    if (cont_loop_l1.room() >= 1) {
      hls::stream<applyFn_reentry0_reentry1_cont0_task> ti;
      hls::stream<applyFn_reentry0_reentry1_task> to;
      ti.write(cont_in_2.pop());
      pes::applyFn_reentry0_reentry1_cont0(ti, to);
      cont_loop_l1.push(to.read());
      comparisons++;
      if (firstCompare < 0)
        firstCompare = cyc;
      lastCompare = cyc;
      peFire("cont2", 1);
    } else {
      stall_cont2_backpressure++;
    }
  }

  // ── Level 1 head: issues one read per site + 1 state per comparison ──────
  if (!re_in_l1.empty() && peReady("reentry1")) {
    // Each site has its own output stream, so the PE writes ONE token per stream
    // and Vitis HLS can pipeline it at II = 1. (With a shared reader it wrote
    // both requests to one stream, forcing II = 2.)
    if (iss_mem_2.room() >= 1 && iss_mem_2b.room() >= 1 &&
        iss_state_2.room() >= 1 && re_exit_l1.room() >= 1) {
      hls::stream<applyFn_reentry0_reentry1_task> ti;
      hls::stream<memrd3_task> tm3;
      hls::stream<memrd4_task> tm4;
      hls::stream<applyFn_reentry0_exit1_task> te;
      hls::stream<applyFn_reentry0_reentry1_cont0_task> ts;
      ti.write(re_in_l1.pop());
      pes::applyFn_reentry0_reentry1(ti, tm3, tm4, te, ts);
      while (!tm3.empty()) { iss_mem_2.push(tm3.read()); mem2Reqs++; }
      while (!tm4.empty()) { iss_mem_2b.push(tm4.read()); mem2Reqs++; }
      while (!ts.empty()) iss_state_2.push(ts.read());
      while (!te.empty()) re_exit_l1.push(te.read());
      peFire("reentry1", 1);
    } else {
      stall_reentry1_nomem++;
    }
  } else if (re_in_l1.empty()) {
    stall_reentry1_noinput++;
  }

  // ── Level 1 arbiter + credit ─────────────────────────────────────────────
  {
    bool a_ok = !lvlin_l1.empty() && inflight1 < MAX_INFLIGHT;
    bool b_ok = !cont_loop_l1.empty();
    if (re_in_l1.room()) {
      int g = arb1.grant(a_ok, b_ok);
      if (g == 0) { re_in_l1.push(lvlin_l1.pop()); inflight1++; }
      else if (g == 1) { re_in_l1.push(cont_loop_l1.pop()); }
    }
  }

  // ── Level 1 exit -> level 0 loopback ─────────────────────────────────────
  if (!re_exit_l1.empty() && peReady("exit1") && cont_loop.room()) {
    hls::stream<applyFn_reentry0_exit1_task> ti;
    hls::stream<applyFn_reentry0_task> to;
    ti.write(re_exit_l1.pop());
    pes::applyFn_reentry0_exit1(ti, to);
    cont_loop.push(to.read());
    inflight1--;
    peFire("exit1", 1);
  }

  // ── Level 0 round 1 continuation -> level 1 entry ────────────────────────
  if (!cont_in_1.empty() && peReady("cont1") && lvlin_l1.room()) {
    hls::stream<applyFn_reentry0_cont1_task> ti;
    hls::stream<applyFn_reentry0_reentry1_task> to;
    ti.write(cont_in_1.pop());
    pes::applyFn_reentry0_cont1(ti, to);
    lvlin_l1.push(to.read());
    peFire("cont1", 1);
  }

  // ── Level 0 round 0 continuation -> round 1 ──────────────────────────────
  if (!cont_in.empty() && peReady("cont0") && iss_mem_1.room() >= 1 &&
      iss_mem_1b.room() >= 1 && iss_state_1.room() >= 1) {
    hls::stream<applyFn_reentry0_cont0_task> ti;
    hls::stream<memrd1_task> tm1;
    hls::stream<memrd2_task> tm2;
    hls::stream<applyFn_reentry0_cont1_task> ts;
    ti.write(cont_in.pop());
    pes::applyFn_reentry0_cont0(ti, tm1, tm2, ts);
    while (!tm1.empty()) iss_mem_1.push(tm1.read());
    while (!tm2.empty()) iss_mem_1b.push(tm2.read());
    while (!ts.empty()) iss_state_1.push(ts.read());
    peFire("cont0", 1);
  }

  // ── Level 0 head ─────────────────────────────────────────────────────────
  if (!re_in.empty() && peReady("reentry0") && re_mem.room() >= 1 &&
      re_state.room() >= 1 && re_exit.room() >= 1) {
    hls::stream<applyFn_reentry0_task> ti;
    hls::stream<memrd0_task> tm;
    hls::stream<applyFn_exit0_task> te;
    hls::stream<applyFn_reentry0_cont0_task> ts;
    ti.write(re_in.pop());
    pes::applyFn_reentry0(ti, tm, te, ts);
    while (!tm.empty()) re_mem.push(tm.read());
    while (!ts.empty()) re_state.push(ts.read());
    while (!te.empty()) re_exit.push(te.read());
    peFire("reentry0", 1);
  }

  // ── Level 0 arbiter + credit ─────────────────────────────────────────────
  {
    bool a_ok = !root_out.empty() && inflight0 < MAX_INFLIGHT;
    bool b_ok = !cont_loop.empty();
    if (re_in.room()) {
      int g = arb0.grant(a_ok, b_ok);
      if (g == 0) { re_in.push(root_out.pop()); inflight0++; }
      else if (g == 1) { re_in.push(cont_loop.pop()); }
    }
  }

  // ── Root PE ──────────────────────────────────────────────────────────────
  if (!taskIn.empty() && peReady("root") && root_out.room()) {
    hls::stream<applyFn_task> ti;
    hls::stream<applyFn_reentry0_task> to;
    ti.write(taskIn.pop());
    pes::applyFn(MEM, MEM, ti, to);
    root_out.push(to.read());
    peFire("root", 1);
  }

  // ── Exit PE (leaf): writes triangleCounts[u] ─────────────────────────────
  if (!re_exit.empty() && peReady("exit0")) {
    hls::stream<applyFn_exit0_task> ti;
    hls::stream<uint64_t> a0, a1, a2;
    hls::stream<uint32_t_arg_out> d0, d1, d2;
    ti.write(re_exit.pop());
    pes::applyFn_exit0(MEM, ti, a0, d0, a1, d1, a2, d2);
    // The argOut write buffer performs the store the descriptor's
    // generateArgOutWriteBuffer provisions for.
    while (!d2.empty()) {
      uint32_t_arg_out w = d2.read();
      std::memcpy(&DevMem[w.addr], &w.data, 4);
    }
    while (!d0.empty()) d0.read();
    while (!d1.empty()) d1.read();
    while (!a0.empty()) a0.read();
    while (!a1.empty()) a1.read();
    while (!a2.empty()) a2.read();
    inflight0--;
    tasksDone++;
    peFire("exit0", 1);
  }
}

void OverlapWrapper::report() {
  gotPer.assign(G.vertexCount, 0);
  gotTotal = 0;
  for (uint32_t u = 0; u < G.vertexCount; u++) {
    uint32_t c;
    std::memcpy(&c, &DevMem[G.triCountsOff + 4ull * u], 4);
    gotPer[u] = c;
    gotTotal += c;
  }

  const double span =
      (lastCompare > firstCompare) ? double(lastCompare - firstCompare) : 0;
  const double ii = comparisons > 1 ? span / double(comparisons - 1) : 0;

  std::printf("\n=== OVERLAP wrapper II report ===\n");
  std::printf("  memory latency        : %d cycles\n", MEM_LATENCY);
  std::printf("  PE pipeline depth     : %d cycles\n", PE_LATENCY);
  std::printf("  STATE_DEPTH           : %d\n", STATE_DEPTH);
  std::printf("  applyFn tasks injected: %d\n", NTASK);
  std::printf("  tasks completed       : %lld\n", tasksDone);
  std::printf("  total cycles          : %lld\n", cyc);
  std::printf("  comparisons retired   : %lld\n", comparisons);
  std::printf("  memrd2 requests       : %lld  (%.2f per comparison)\n",
              mem2Reqs,
              comparisons ? double(mem2Reqs) / double(comparisons) : 0.0);
  std::printf("  --> comparison II     : %.3f cycles/comparison\n", ii);
  std::printf("  stalls: reentry1 no-input %lld, reentry1 no-mem-room %lld,\n"
              "          merge2 no-state %lld, merge2 no-reply %lld,\n"
              "          cont2 back-pressured %lld\n",
              stall_reentry1_noinput, stall_reentry1_nomem,
              stall_merge2_nostate, stall_merge2_noreply,
              stall_cont2_backpressure);

  bool ok = (gotTotal == expectedTotal);
  for (uint32_t u = 0; u < G.vertexCount && ok; u++)
    ok = (gotPer[u] == expectedPer[u]);
  std::printf("  functional            : got total %llu, expected %llu -> %s\n",
              (unsigned long long)gotTotal, (unsigned long long)expectedTotal,
              ok ? "PASS" : "FAIL");
  if (!ok) {
    for (uint32_t u = 0; u < G.vertexCount && u < 16; u++)
      std::printf("    v%-3u got %-6u expected %-6u\n", u, gotPer[u],
                  expectedPer[u]);
  }
  std::printf("=================================\n");
  if (!ok || tasksDone != NTASK)
    std::exit(1);
}

int sc_main(int argc, char *argv[]) {
  uint32_t V = 64, DEG = 24;
  long long maxCycles = 40000000;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto val = [&]() { return std::atoi(argv[++i]); };
    if (a == "--mem-latency") MEM_LATENCY = val();
    else if (a == "--pe-latency") PE_LATENCY = val();
    else if (a == "--state-depth") STATE_DEPTH = val();
    else if (a == "--vertices") V = val();
    else if (a == "--degree") DEG = val();
    else if (a == "--max-cycles") maxCycles = val();
  }

  sc_clock clk("clk", 1, SC_NS);
  OverlapWrapper dut("dut");
  dut.clk(clk);

  dut.G = buildGraph(V, DEG);
  dut.expectedTotal = referenceTriangles(dut.G, dut.expectedPer);
  NTASK = (int)dut.G.vertexCount;

  // Inject one applyFn closure per vertex. The scheduler would deliver these;
  // the wrapper's credit counter throttles admission.
  for (uint32_t u = 0; u < dut.G.vertexCount; u++) {
    applyFn_task t{};
    t._cont = ((addr_t)SPAWNERFUNCTION_CONT0_TAG << 56);
    t.u = u;
    t.pGraph = dut.G.pGraphOff;
    t.triangleCounts = dut.G.triCountsOff;
    dut.taskIn.q.push_back(t);
  }
  dut.taskIn.depth = dut.G.vertexCount + 8;

  // Run until every task has retired, or the deadlock guard trips.
  // Progress is measured in COMPARISONS, not completed tasks: with many tasks
  // interleaved, none retires for a long time while the pipeline is perfectly
  // busy, so a task-based guard reports a deadlock that is not there.
  long long lastProgress = 0, lastDone = 0;
  while (dut.tasksDone < NTASK && dut.cyc < maxCycles) {
    sc_start(1000, SC_NS);
    if (dut.comparisons != lastDone) {
      lastDone = dut.comparisons;
      lastProgress = dut.cyc;
    } else if (dut.cyc - lastProgress > 20000) {
      std::printf("DEADLOCK: no comparison retired for 20000 cycles "
                  "(%lld/%d tasks done)\n",
                  dut.tasksDone, NTASK);
      dut.dumpOccupancy();
      break;
    }
  }
  dut.report();
  return 0;
}
