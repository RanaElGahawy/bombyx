// ===========================================================================
// SystemC / Verilator testbench for pageRank_overlap's applyFn_overlap_wrapper.
//
// Drives the REAL Verilog — the bombyx-emitted wrapper plus the Vitis-HLS
// synthesised PEs (applyFn, applyFn_reentry1, applyFn_reentry1_cont0,
// applyFn_reentry1_cont1, applyFn_exit1) and the wrapper's own shared cached
// memory reader. Nothing in the datapath is a stand-in; only the memory, the
// upstream scheduler and the downstream sink buffer are modelled.
//
// It exists because the only other way to run this design is a 40-minute
// xclbin rebuild plus hw_emu, which is far too slow to iterate a codegen fix
// against.
//
// Device memory image (byte offsets; a "pointer" in a closure is one of these):
//   pGraph[2u]   = byte offset of u's neighbour list (uint64)
//   pGraph[2u+1] = degree of u                       (uint64)
//   pPrCurr[u], pPrNext[u], diffs[u]                 (float)
//
// The kernel under test computes, for each vertex u:
//   contributions = sum over neighbours v of  pPrCurr[v] / degree(v)
//   pPrNext[u]    = (1 - damping)/vertexCount + damping * contributions
//   diffs[u]      = |pPrNext[u] - pPrCurr[u]|
// and emits one argOut/argDataOut pair per vertex (the sync-counter decrement
// plus the diffs[] store the sink buffer would perform).
//
// The golden is computed here in the same order the hardware accumulates in —
// the merge ring is strictly serial per context — so agreement is expected to
// be BIT-EXACT, not approximate. Anything else is a real defect.
// ===========================================================================

#include <systemc.h>
#include <verilated.h>

#include "Vwrap.h"
#include "Vwrap___024root.h"
#include "verilated_vcd_c.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// ─── DPI-C: exact binary32 arithmetic for the FP IP models ──────────────────
static inline uint32_t f2b(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static inline float    b2f(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
extern "C" {
uint32_t bombyx_fp_add(uint32_t a, uint32_t b) { return f2b(b2f(a) + b2f(b)); }
uint32_t bombyx_fp_sub(uint32_t a, uint32_t b) { return f2b(b2f(a) - b2f(b)); }
uint32_t bombyx_fp_mul(uint32_t a, uint32_t b) { return f2b(b2f(a) * b2f(b)); }
uint32_t bombyx_fp_div(uint32_t a, uint32_t b) { return f2b(b2f(a) / b2f(b)); }
uint32_t bombyx_fp_uitofp32(uint32_t a) { return f2b((float)a); }
uint32_t bombyx_fp_uitofp64(uint64_t a) { return f2b((float)a); }
}

// ─── knobs ──────────────────────────────────────────────────────────────────
static int  MEM_LATENCY = 24;
static int  NVERT       = 16;
static int  NTASK       = 0;      // 0 => all vertices
static long long MAX_CYCLES = 400000;
static bool VERBOSE     = false;
static bool TRACE_CLOSURES = false;
static unsigned SEED    = 1;
static const float DAMPING = 0.85f;

// ─── device memory + graph ──────────────────────────────────────────────────
static std::vector<uint8_t> Mem;
static uint64_t pGraphOff, prCurrOff, prNextOff, diffsOff;
static std::vector<std::vector<uint32_t>> Adj;
static std::vector<float> GoldPrNext, GoldDiffs, PrCurr;

// Irregular, densely indexed, connected — a regular clique cannot distinguish a
// correct result from one that permutes or drops vertices (every rank is then
// identical), which is exactly the trap the system-level verification hit.
static void buildGraph(int V) {
  Adj.assign(V, {});
  auto addEdge = [&](int a, int b) {
    if (a == b) return;
    Adj[a].push_back(b);
    Adj[b].push_back(a);
  };
  unsigned s = SEED;
  auto rnd = [&]() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; };
  std::vector<int> perm(V);
  for (int i = 0; i < V; i++) perm[i] = i;
  for (int i = V - 1; i > 0; i--) std::swap(perm[i], perm[rnd() % (i + 1)]);
  for (int i = 0; i + 1 < V; i++) addEdge(perm[i], perm[i + 1]); // spanning path
  int extra = V * 2;
  for (int e = 0; e < extra; e++) addEdge(rnd() % V, rnd() % V);
  for (auto &a : Adj) {
    std::sort(a.begin(), a.end());
    a.erase(std::unique(a.begin(), a.end()), a.end());
  }

  const uint64_t base = 4096;
  pGraphOff = base;
  uint64_t cur = base + 16ull * V;
  std::vector<uint64_t> listOff(V);
  for (int u = 0; u < V; u++) {
    listOff[u] = cur;
    cur += 4ull * Adj[u].size();
    cur = (cur + 63) & ~63ull;
  }
  prCurrOff = cur;      cur += 4ull * V; cur = (cur + 63) & ~63ull;
  prNextOff = cur;      cur += 4ull * V; cur = (cur + 63) & ~63ull;
  diffsOff  = cur;      cur += 4ull * V; cur = (cur + 63) & ~63ull;
  Mem.assign(cur + 4096, 0);

  for (int u = 0; u < V; u++) {
    uint64_t ptr = Adj[u].empty() ? 0 : listOff[u];
    std::memcpy(&Mem[pGraphOff + 16ull * u], &ptr, 8);
    uint64_t d = Adj[u].size();
    std::memcpy(&Mem[pGraphOff + 16ull * u + 8], &d, 8);
    for (size_t j = 0; j < Adj[u].size(); j++)
      std::memcpy(&Mem[listOff[u] + 4 * j], &Adj[u][j], 4);
  }

  PrCurr.assign(V, 1.0f / (float)V);
  for (int u = 0; u < V; u++)
    std::memcpy(&Mem[prCurrOff + 4ull * u], &PrCurr[u], 4);

  // Golden, accumulated in neighbour order — the order the serial merge ring
  // retires in, so this is a bit-exact expectation.
  GoldPrNext.assign(V, 0.f);
  GoldDiffs.assign(V, 0.f);
  for (int u = 0; u < V; u++) {
    float contributions = 0.f;
    for (uint32_t v : Adj[u])
      contributions = contributions + (PrCurr[v] / (float)(uint64_t)Adj[v].size());
    GoldPrNext[u] = (1.f - DAMPING) / (float)(uint32_t)V + DAMPING * contributions;
    GoldDiffs[u]  = std::fabs(GoldPrNext[u] - PrCurr[u]);
  }
}

static void memRead(uint64_t addr, unsigned bytes, uint8_t *out) {
  std::memset(out, 0, bytes);
  if (addr + bytes <= Mem.size()) std::memcpy(out, &Mem[addr], bytes);
}

// ─── AXI read slave ─────────────────────────────────────────────────────────
// One request queue per ARID: AXI orders responses only within an ID, and both
// the shared reader (ARID = stream index) and the HLS `channel = N` split rely
// on that. A slave answering everything with RID=0 would hang the other IDs.
struct AxiRd {
  std::string name;
  CData *arvalid, *arready, *rvalid, *rready, *rlast, *arlen, *arsize, *arid, *rid;
  QData *araddr;
  void  *rdata;
  unsigned dataBytes;
  struct Pending { long long due; uint64_t addr; unsigned bytes; unsigned id; };
  std::map<unsigned, std::vector<Pending>> q;
  long long reads = 0;
  bool inFlight = false;
  unsigned curId = 0;
  uint8_t curData[64];
  size_t outstanding() const {
    size_t n = 0; for (auto &kv : q) n += kv.second.size(); return n;
  }
};

// ─── AXI write slave (only applyFn_exit1 stores) ────────────────────────────
struct AxiWr {
  CData *awvalid, *awready, *wvalid, *wready, *wlast, *bvalid, *bready, *wstrb;
  QData *awaddr;
  void  *wdata;
  unsigned dataBytes;
  std::vector<uint64_t> awq;
  long long writes = 0;
  int bpend = 0;
};

int sc_main(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--mem-latency") MEM_LATENCY = std::atoi(argv[++i]);
    else if (a == "--vertices") NVERT = std::atoi(argv[++i]);
    else if (a == "--tasks") NTASK = std::atoi(argv[++i]);
    else if (a == "--seed") SEED = (unsigned)std::atoi(argv[++i]);
    else if (a == "--max-cycles") MAX_CYCLES = std::atoll(argv[++i]);
    else if (a == "--verbose") VERBOSE = true;
    else if (a == "--closures") TRACE_CLOSURES = true;
  }
  buildGraph(NVERT);
  if (NTASK <= 0 || NTASK > NVERT) NTASK = NVERT;

  Verilated::commandArgs(argc, argv);
  Verilated::traceEverOn(true);
  Vwrap *d = new Vwrap("dut");
  VerilatedVcdC *vcd = nullptr;
  if (getenv("TB_VCD")) {
    vcd = new VerilatedVcdC;
    d->trace(vcd, 6);
    vcd->open(getenv("TB_VCD"));
  }

  std::vector<AxiRd> rd;
  auto addRd = [&](const char *nm, CData *arv, CData *arr, QData *ara, CData *arl,
                   CData *ars, CData *ari, CData *rv, CData *rr, CData *rl,
                   CData *ri, void *rdp, unsigned bytes) {
    AxiRd p{};
    p.name = nm;
    p.arvalid = arv; p.arready = arr; p.araddr = ara; p.arlen = arl;
    p.arsize = ars; p.arid = ari; p.rvalid = rv; p.rready = rr; p.rlast = rl;
    p.rid = ri; p.rdata = rdp; p.dataBytes = bytes;
    rd.push_back(p);
  };
#define ADD_RD(NM, BYTES)                                                      \
  addRd(#NM, &d->m_axi_gmem_##NM##_ARVALID, &d->m_axi_gmem_##NM##_ARREADY,      \
        &d->m_axi_gmem_##NM##_ARADDR, &d->m_axi_gmem_##NM##_ARLEN,              \
        &d->m_axi_gmem_##NM##_ARSIZE, &d->m_axi_gmem_##NM##_ARID,               \
        &d->m_axi_gmem_##NM##_RVALID, &d->m_axi_gmem_##NM##_RREADY,             \
        &d->m_axi_gmem_##NM##_RLAST, &d->m_axi_gmem_##NM##_RID,                 \
        (void *)&d->m_axi_gmem_##NM##_RDATA, BYTES)

  ADD_RD(shared, 32);                    // 256-bit cached line reader
  ADD_RD(applyFn, 8);                    // pGraph[2u], pGraph[2u+1]
  ADD_RD(applyFn_reentry1_cont1, 4);     // pPrCurr[v]
  ADD_RD(applyFn_exit1, 4);              // pPrNext[u], pPrCurr[u] readback

  AxiWr wr{};
  wr.awvalid = &d->m_axi_gmem_applyFn_exit1_AWVALID;
  wr.awready = &d->m_axi_gmem_applyFn_exit1_AWREADY;
  wr.awaddr  = &d->m_axi_gmem_applyFn_exit1_AWADDR;
  wr.wvalid  = &d->m_axi_gmem_applyFn_exit1_WVALID;
  wr.wready  = &d->m_axi_gmem_applyFn_exit1_WREADY;
  wr.wlast   = &d->m_axi_gmem_applyFn_exit1_WLAST;
  wr.wstrb   = &d->m_axi_gmem_applyFn_exit1_WSTRB;
  wr.wdata   = (void *)&d->m_axi_gmem_applyFn_exit1_WDATA;
  wr.bvalid  = &d->m_axi_gmem_applyFn_exit1_BVALID;
  wr.bready  = &d->m_axi_gmem_applyFn_exit1_BREADY;
  wr.dataBytes = 4;

  // ── task injection: applyFn_task, packed, 64 bytes ────────────────────────
  //   _cont(8) vertexCount(4) pGraph(8) pPrCurr(8) pPrNext(8) diffs(8) u(4)
  //   damping(4) _padding[12]
  // The continuation tag must be SPAWNERFUNCTION_REENTRY0_CONT0_TAG (1) or the
  // exit PE routes the result to the internal port and nothing leaves.
  const uint64_t CONT = (1ull << 56) | 0x9bc00ull;
  auto packTask = [&](uint32_t u, uint8_t *b) {
    std::memset(b, 0, 64);
    uint32_t vc = (uint32_t)NVERT;
    float dmp = DAMPING;
    std::memcpy(b + 0,  &CONT, 8);
    std::memcpy(b + 8,  &vc, 4);
    std::memcpy(b + 12, &pGraphOff, 8);
    std::memcpy(b + 20, &prCurrOff, 8);
    std::memcpy(b + 28, &prNextOff, 8);
    std::memcpy(b + 36, &diffsOff, 8);
    std::memcpy(b + 44, &u, 4);
    std::memcpy(b + 48, &dmp, 4);
  };
  int nextTask = 0;

  // ── reset ─────────────────────────────────────────────────────────────────
  d->ap_clk = 0;
  d->ap_rst_n = 0;
  d->taskIn_TVALID = 0;
  d->argOut_TREADY = 1;
  d->argDataOut_TREADY = 1;
  for (auto &p : rd) { *p.arready = 1; *p.rvalid = 0; }
  *wr.awready = 1; *wr.wready = 1; *wr.bvalid = 0;

  long long cyc = 0;
  auto lowPhase = [&]() {
    d->ap_clk = 0; d->eval();
    if (vcd) vcd->dump((uint64_t)(cyc * 10));
  };
  auto edge = [&]() {
    d->ap_clk = 1; d->eval();
    if (vcd) vcd->dump((uint64_t)(cyc * 10 + 5));
    cyc++;
  };
  auto tick = [&]() { lowPhase(); edge(); };
  for (int i = 0; i < 20; i++) tick();
  d->ap_rst_n = 1;

  long long tasksIn = 0, argOuts = 0, argDatas = 0;
  long long lastProgress = 0, prevActivity = -1;
  int drain = -1;
  // Throughput accounting. One `cont_loop` handshake == one neighbour
  // contribution retired by the merge ring, which is the unit of useful work
  // this design does: the loop body is exactly `contributions += pPrCurr[v] /
  // v_size` for one neighbour. Counting handshakes rather than dividing the
  // total by the run length also gives the steady-state rate, which is what a
  // buffer-size sweep has to compare -- end-to-end includes fill and drain, and
  // those do not scale with the buffers.
  long long contribs = 0;
  long long firstContribCyc = -1, lastContribCyc = -1;
  long long totalContribs = 0;
  for (int u = 0; u < NTASK; u++) totalContribs += (long long)Adj[u].size();
  const long long warmLo = totalContribs / 4, warmHi = (3 * totalContribs) / 4;
  long long warmLoCyc = -1, warmHiCyc = -1;
  std::vector<const char *> stageName;
  std::vector<long long> stageFire, stageBp, stageStarve;
  std::vector<int> diffWritten(NVERT, 0);
  long long nextWritten = 0;
  // Both results now leave as buffered stores on argDataOut -- pPrNext[u] then
  // diffs[u] -- so a completed task is TWO argData beats, not one.
  const int ARGDATA_PER_TASK = 2;
  bool stalled = false;

  auto totalReads = [&]() {
    long long s = 0; for (auto &p : rd) s += p.reads; return s;
  };

  while (cyc < MAX_CYCLES) {
    lowPhase();

    // ---- drive taskIn ----
    if (nextTask < NTASK) {
      uint8_t b[64];
      packTask((uint32_t)nextTask, b);
      for (int i = 0; i < 16; i++) {
        uint32_t w; std::memcpy(&w, b + 4 * i, 4);
        d->taskIn_TDATA[i] = w;
      }
      d->taskIn_TVALID = 1;
    } else {
      d->taskIn_TVALID = 0;
    }

    // ---- read slaves ----
    for (auto &p : rd) {
      *p.arready = 1;
      if (!p.inFlight) {
        long long best = -1; unsigned bestId = 0;
        for (auto &kv : p.q) {
          if (kv.second.empty()) continue;
          if (kv.second.front().due > cyc) continue;
          if (best < 0 || kv.second.front().due < best) {
            best = kv.second.front().due; bestId = kv.first;
          }
        }
        if (best >= 0) {
          auto pd = p.q[bestId].front();
          memRead(pd.addr, pd.bytes, p.curData);
          p.curId = pd.id;
          p.inFlight = true;
        }
      }
      if (p.inFlight) {
        *p.rvalid = 1; *p.rlast = 1; *p.rid = (CData)p.curId;
        std::memcpy(p.rdata, p.curData, p.dataBytes);
      } else {
        *p.rvalid = 0;
      }
    }
    *wr.awready = 1;
    *wr.wready = 1;
    // BVALID only after a burst has actually landed. Holding it high
    // unconditionally hands the HLS write engine responses it never asked for.
    *wr.bvalid = wr.bpend > 0;

    d->eval();

    // ---- per-stage occupancy: where does the ring actually spend its time ----
    // For each internal handshake: fired / backpressured (valid & !ready) /
    // starved (!valid). A stage that is mostly backpressured is the bottleneck;
    // one that is mostly starved is waiting on something upstream. This is what
    // says whether a buffer-depth change can help at all.
    {
      struct St { const char *n; CData v, r; };
#define STAGE(NM)                                                              \
  {#NM, d->rootp->applyFn_overlap_wrapper__DOT__##NM##_TVALID,                  \
        d->rootp->applyFn_overlap_wrapper__DOT__##NM##_TREADY}
      St st[] = {STAGE(root_out), STAGE(re_in),    STAGE(re_mem),
                 STAGE(cont_in),  STAGE(iss_mem_1), STAGE(cont_in_1),
                 STAGE(cont_loop), STAGE(re_exit)};
#undef STAGE
      const int NST = sizeof(st) / sizeof(st[0]);
      if (stageFire.empty()) {
        stageName.assign(NST, nullptr);
        stageFire.assign(NST, 0); stageBp.assign(NST, 0); stageStarve.assign(NST, 0);
      }
      for (int i = 0; i < NST; i++) {
        stageName[i] = st[i].n;
        if (st[i].v && st[i].r) stageFire[i]++;
        else if (st[i].v)       stageBp[i]++;
        else                    stageStarve[i]++;
      }
    }

    // ---- steady-state throughput probe ----
    if (d->rootp->applyFn_overlap_wrapper__DOT__cont_loop_TVALID &&
        d->rootp->applyFn_overlap_wrapper__DOT__cont_loop_TREADY) {
      contribs++;
      if (firstContribCyc < 0) firstContribCyc = cyc;
      lastContribCyc = cyc;
      if (contribs == warmLo) warmLoCyc = cyc;
      if (contribs == warmHi) warmHiCyc = cyc;
    }

    // ---- sample exactly what the DUT latches at the coming edge ----
    bool taskFire = d->taskIn_TVALID && d->taskIn_TREADY;
    bool argOutFire = d->argOut_TVALID && d->argOut_TREADY;
    bool argDataFire = d->argDataOut_TVALID && d->argDataOut_TREADY;
    uint64_t argOutCap = d->argOut_TDATA;
    uint32_t argDataCap[8];
    for (int i = 0; i < 8; i++) argDataCap[i] = d->argDataOut_TDATA[i];

    struct ArCap { bool fire; uint64_t addr; unsigned bytes; unsigned id; };
    std::vector<ArCap> arc(rd.size());
    for (size_t i = 0; i < rd.size(); i++) {
      auto &p = rd[i];
      arc[i].fire  = *p.arvalid && *p.arready;
      arc[i].addr  = *p.araddr;
      arc[i].bytes = 1u << *p.arsize;
      arc[i].id    = *p.arid;
      if (arc[i].fire && *p.arlen != 0)
        std::printf("[%lld] %s: multi-beat read (ARLEN=%u) not modelled\n", cyc,
                    p.name.c_str(), (unsigned)*p.arlen);
      if (arc[i].fire && arc[i].bytes > p.dataBytes) arc[i].bytes = p.dataBytes;
    }
    std::vector<bool> rfire(rd.size());
    for (size_t i = 0; i < rd.size(); i++) rfire[i] = *rd[i].rvalid && *rd[i].rready;

    bool awFire = *wr.awvalid && *wr.awready;
    uint64_t awAddr = *wr.awaddr;
    bool wFire = *wr.wvalid && *wr.wready;
    bool bFire = *wr.bvalid && *wr.bready;
    uint32_t wData = *(IData *)wr.wdata;
    uint32_t wStrb = *wr.wstrb;

    // ---- internal closure trace (optional, needs --public-flat-rw) ----------
    if (TRACE_CLOSURES) {
      auto dumpMemRd = [&](const char *nm, const uint32_t *td, bool fire) {
        if (!fire) return;
        uint8_t b[32];
        for (int i = 0; i < 8; i++) std::memcpy(b + 4 * i, &td[i], 4);
        uint64_t cont, base, idx;
        std::memcpy(&cont, b + 0, 8);
        std::memcpy(&base, b + 8, 8);
        std::memcpy(&idx,  b + 16, 8);
        std::printf("[%lld] %-10s cont=0x%llx base=0x%llx idx=%llu\n", cyc, nm,
                    (unsigned long long)cont, (unsigned long long)base,
                    (unsigned long long)idx);
      };
      dumpMemRd("re_mem", d->rootp->applyFn_overlap_wrapper__DOT__re_mem_TDATA.data(),
                d->rootp->applyFn_overlap_wrapper__DOT__re_mem_TVALID &&
                d->rootp->applyFn_overlap_wrapper__DOT__re_mem_TREADY);
      dumpMemRd("iss_mem_1", d->rootp->applyFn_overlap_wrapper__DOT__iss_mem_1_TDATA.data(),
                d->rootp->applyFn_overlap_wrapper__DOT__iss_mem_1_TVALID &&
                d->rootp->applyFn_overlap_wrapper__DOT__iss_mem_1_TREADY);
      // Field tables are the packed struct layouts from
      // pageRank_overlap_defs.h; `off` is the byte offset, `w` the byte width,
      // and 'f' marks a float so the value prints readably.
      struct Fld { const char *n; int off, w; char k; };
      // applyFn_reentry1_task (no _counter)
      static const Fld RE[] = {
          {"cont", 0, 8, 'x'},   {"vc", 8, 4, 'u'},     {"pGraph", 12, 8, 'x'},
          {"pPrCurr", 20, 8, 'x'}, {"pPrNext", 28, 8, 'x'}, {"diffs", 36, 8, 'x'},
          {"u", 44, 4, 'u'},     {"damping", 48, 4, 'f'}, {"contrib", 52, 4, 'f'},
          {"n_size", 56, 8, 'u'}, {"neighbors", 64, 8, 'x'}, {"i", 72, 4, 'u'},
          {"v", 76, 4, 'u'},     {nullptr, 0, 0, 0}};
      // applyFn_reentry1_cont0/cont1_task (leading uint32 _counter shifts by 4)
      static const Fld CT[] = {
          {"cnt", 0, 4, 'u'},    {"cont", 4, 8, 'x'},   {"vc", 12, 4, 'u'},
          {"pGraph", 16, 8, 'x'}, {"pPrCurr", 24, 8, 'x'}, {"pPrNext", 32, 8, 'x'},
          {"diffs", 40, 8, 'x'}, {"u", 48, 4, 'u'},     {"damping", 52, 4, 'f'},
          {"contrib", 56, 4, 'f'}, {"n_size", 60, 8, 'u'}, {"neighbors", 68, 8, 'x'},
          {"i", 76, 4, 'u'},     {"v", 80, 4, 'u'},     {"v_size", 84, 8, 'u'},
          {nullptr, 0, 0, 0}};
      // applyFn_exit1_task
      static const Fld EX[] = {
          {"cont", 0, 8, 'x'},   {"vc", 8, 4, 'u'},     {"pPrCurr", 12, 8, 'x'},
          {"pPrNext", 20, 8, 'x'}, {"diffs", 28, 8, 'x'}, {"u", 36, 4, 'u'},
          {"damping", 40, 4, 'f'}, {"contrib", 44, 4, 'f'}, {nullptr, 0, 0, 0}};
      auto dumpClosure = [&](const char *nm, const uint32_t *td, int words,
                             const Fld *tbl, bool fire) {
        if (!fire) return;
        std::vector<uint8_t> b(4 * words);
        for (int i = 0; i < words; i++) std::memcpy(&b[4 * i], &td[i], 4);
        std::printf("[%lld] %-10s", cyc, nm);
        for (const Fld *f = tbl; f->n; f++) {
          uint64_t val = 0;
          std::memcpy(&val, &b[f->off], f->w);
          if (f->k == 'f') std::printf(" %s=%g", f->n, (double)b2f((uint32_t)val));
          else if (f->k == 'u') std::printf(" %s=%llu", f->n, (unsigned long long)val);
          else std::printf(" %s=0x%llx", f->n, (unsigned long long)val);
        }
        std::printf("\n");
      };
#define DUMP(NM, TBL, WORDS)                                                   \
  dumpClosure(#NM, d->rootp->applyFn_overlap_wrapper__DOT__##NM##_TDATA.data(), \
              WORDS, TBL,                                                       \
              d->rootp->applyFn_overlap_wrapper__DOT__##NM##_TVALID &&          \
                  d->rootp->applyFn_overlap_wrapper__DOT__##NM##_TREADY)
      DUMP(root_out, RE, 32);
      DUMP(re_in, RE, 32);
      DUMP(cont_in, CT, 32);
      DUMP(cont_in_1, CT, 32);
      DUMP(cont_loop, RE, 32);
      DUMP(re_exit, EX, 16);
#undef DUMP
      // The exit PE's two result valids, ahead of the wrapper's outbound skids.
      // They are printed independently on purpose: the wrapper used to buffer
      // them as one channel gated on argOut's valid, which is only correct if
      // they rise together. They do not — on this design argData trails argOut
      // by ~140 cycles — so this line is the direct evidence for that defect and
      // the check that any future merge attempt would break.
      {
        int ov = d->rootp->applyFn_overlap_wrapper__DOT__exit_spawnerFunction_reentry0_cont0_argOut_TVALID;
        int dv = d->rootp->applyFn_overlap_wrapper__DOT__exit_spawnerFunction_reentry0_cont0_argData_TVALID;
        static int pov = -1, pdv = -1;
        if (ov != pov || dv != pdv) {
          std::printf("[%lld] exit PE valids: argOut=%d argData=%d\n", cyc, ov, dv);
          pov = ov; pdv = dv;
        }
      }
      if (d->rootp->applyFn_overlap_wrapper__DOT__exit_spawnerFunction_reentry0_cont0_argData_TVALID) {
        const uint32_t *td =
            d->rootp->applyFn_overlap_wrapper__DOT__exit_spawnerFunction_reentry0_cont0_argData_TDATA.data();
        uint8_t b[32];
        for (int i = 0; i < 8; i++) std::memcpy(b + 4 * i, &td[i], 4);
        uint64_t addr; uint32_t data, sz, allow;
        std::memcpy(&addr, b + 0, 8);
        std::memcpy(&data, b + 8, 4);
        std::memcpy(&sz, b + 12, 4);
        std::memcpy(&allow, b + 16, 4);
        std::printf("[%lld] exitArgDat addr=0x%llx data=%g size=%u allow=%u\n",
                    cyc, (unsigned long long)addr, (double)b2f(data), sz, allow);
      }
    }

    edge();

    // ---- commit ----
    if (taskFire) {
      if (VERBOSE) std::printf("[%lld] taskIn u=%d\n", cyc, nextTask);
      tasksIn++; nextTask++;
    }
    if (argOutFire) {
      argOuts++;
      if (VERBOSE)
        std::printf("[%lld] argOut cont=0x%llx tag=%u\n", cyc,
                    (unsigned long long)argOutCap, (unsigned)(argOutCap >> 56));
    }
    if (argDataFire) {
      argDatas++;
      // float_arg_out: { addr(8) data(4) size(4) allow(4) _padding[12] }
      uint8_t b[32];
      for (int i = 0; i < 8; i++) std::memcpy(b + 4 * i, &argDataCap[i], 4);
      uint64_t addr; uint32_t data;
      std::memcpy(&addr, b + 0, 8);
      std::memcpy(&data, b + 8, 4);
      if (addr + 4 <= Mem.size()) std::memcpy(&Mem[addr], &data, 4);
      if (addr >= diffsOff && addr < diffsOff + 4ull * NVERT &&
          ((addr - diffsOff) % 4) == 0)
        diffWritten[(addr - diffsOff) / 4]++;
      if (addr >= prNextOff && addr < prNextOff + 4ull * NVERT) nextWritten++;
      if (VERBOSE)
        std::printf("[%lld] argDataOut addr=0x%llx data=%g\n", cyc,
                    (unsigned long long)addr, (double)b2f(data));
    }
    for (size_t i = 0; i < rd.size(); i++) {
      if (arc[i].fire) {
        rd[i].q[arc[i].id].push_back(
            {cyc + MEM_LATENCY, arc[i].addr, arc[i].bytes, arc[i].id});
        rd[i].reads++;
        if (VERBOSE)
          std::printf("[%lld] %s AR addr=0x%llx bytes=%u id=%u\n", cyc,
                      rd[i].name.c_str(), (unsigned long long)arc[i].addr,
                      arc[i].bytes, arc[i].id);
      }
      if (rfire[i]) {
        auto &qq = rd[i].q[rd[i].curId];
        if (!qq.empty()) qq.erase(qq.begin());
        rd[i].inFlight = false;
      }
    }
    if (bFire && wr.bpend > 0) wr.bpend--;
    if (awFire) wr.awq.push_back(awAddr);
    if (wFire) {
      uint64_t a = wr.awq.empty() ? 0 : wr.awq.front();
      if (!wr.awq.empty()) wr.awq.erase(wr.awq.begin());
      for (unsigned i = 0; i < wr.dataBytes; i++)
        if (wStrb & (1u << i)) {
          uint64_t t = a + i;
          if (t < Mem.size()) Mem[t] = (uint8_t)(wData >> (8 * i));
        }
      wr.writes++;
      wr.bpend++;
      if (VERBOSE)
        std::printf("[%lld] exit1 W addr=0x%llx data=%g strb=%x\n", cyc,
                    (unsigned long long)a, (double)b2f(wData), wStrb);
    }

    // ---- stall detection ----
    long long activity = tasksIn + argOuts + argDatas + totalReads();
    if (activity != prevActivity) { prevActivity = activity; lastProgress = cyc; }
    else if (cyc - lastProgress > 4 * MEM_LATENCY + 3000) {
      std::printf("\n### STALL at cycle %lld — no AXI read, no task in, no "
                  "result out for %lld cycles\n", cyc, cyc - lastProgress);
      stalled = true;
      break;
    }

    // Drain inside the main loop, not with bare tick()s, so tracing and the AXI
    // slaves keep running while the last results settle.
    if (argOuts >= NTASK && argDatas >= (long long)NTASK * ARGDATA_PER_TASK &&
        drain < 0)
      drain = 2000;
    if (drain > 0 && --drain == 0) break;
  }

  // ── report ────────────────────────────────────────────────────────────────
  std::printf("\n=== pageRank_overlap wrapper testbench ===\n");
  std::printf("  vertices/tasks    : %d / %d\n", NVERT, NTASK);
  std::printf("  cycles            : %lld\n", cyc);
  std::printf("  tasks injected    : %lld\n", tasksIn);
  std::printf("  argOut fired      : %lld / %d\n", argOuts, NTASK);
  std::printf("  argDataOut fired  : %lld / %d\n", argDatas,
              NTASK * ARGDATA_PER_TASK);
  std::printf("  pPrNext stores    : %lld\n", nextWritten);
  std::printf("  exit1 stores      : %lld\n", wr.writes);
  std::printf("  contributions     : %lld / %lld\n", contribs, totalContribs);
  if (cyc > 0)
    std::printf("  PERF end-to-end   : %.4f contributions/PE/cycle\n",
                (double)contribs / (double)cyc);
  if (warmLoCyc > 0 && warmHiCyc > warmLoCyc)
    std::printf("  PERF steady-state : %.4f contributions/PE/cycle "
                "(middle half: %lld contributions over %lld cycles)\n",
                (double)(warmHi - warmLo) / (double)(warmHiCyc - warmLoCyc),
                warmHi - warmLo, warmHiCyc - warmLoCyc);
  std::printf("  taskIn_TREADY now : %d\n", (int)d->taskIn_TREADY);
  for (auto &p : rd)
    std::printf("  %-24s reads=%-7lld outstanding=%zu\n", p.name.c_str(),
                p.reads, p.outstanding());

  if (!stageFire.empty()) {
    std::printf("  --- ring occupancy (%% of simulated cycles) ---\n");
    for (size_t i = 0; i < stageFire.size(); i++) {
      double t = double(stageFire[i] + stageBp[i] + stageStarve[i]);
      std::printf("  %-12s fire %5.1f%%  backpressured %5.1f%%  starved %5.1f%%\n",
                  stageName[i], 100.0 * stageFire[i] / t,
                  100.0 * stageBp[i] / t, 100.0 * stageStarve[i] / t);
    }
  }

  int badNext = 0, badDiff = 0;
  int shown = 0;
  for (int u = 0; u < NTASK; u++) {
    uint32_t gotN, gotD;
    std::memcpy(&gotN, &Mem[prNextOff + 4ull * u], 4);
    std::memcpy(&gotD, &Mem[diffsOff + 4ull * u], 4);
    uint32_t expN = f2b(GoldPrNext[u]), expD = f2b(GoldDiffs[u]);
    if (gotN != expN) badNext++;
    if (gotD != expD) badDiff++;
    if ((gotN != expN || gotD != expD) && shown < 12) {
      std::printf("    v%-3d deg=%-3zu pPrNext got %.9g (0x%08x) exp %.9g (0x%08x)"
                  "  diffs got %.9g exp %.9g\n",
                  u, Adj[u].size(), (double)b2f(gotN), gotN,
                  (double)GoldPrNext[u], expN, (double)b2f(gotD),
                  (double)GoldDiffs[u]);
      shown++;
    }
  }
  bool ok = !stalled && argOuts >= NTASK &&
            argDatas >= (long long)NTASK * ARGDATA_PER_TASK &&
            badNext == 0 && badDiff == 0;
  std::printf("  pPrNext bit-exact : %d / %d\n", NTASK - badNext, NTASK);
  std::printf("  diffs   bit-exact : %d / %d\n", NTASK - badDiff, NTASK);
  std::printf("  VERDICT           : %s\n", ok ? "PASS" : "FAIL");
  std::printf("==========================================\n");

  if (vcd) vcd->close();
  delete d;
  return ok ? 0 : 1;
}
