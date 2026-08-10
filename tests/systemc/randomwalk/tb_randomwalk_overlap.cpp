// ===========================================================================
// SystemC / Verilator testbench for randomWalk_overlap's
// applyFn_overlap_wrapper.
//
// Drives the REAL Verilog — the bombyx-emitted wrapper plus the Vitis-HLS
// synthesised PEs (applyFn, applyFn_reentry0, applyFn_reentry0_cont0,
// applyFn_reentry0_cont1, applyFn_reentry0_afterif0, applyFn_exit0) and the
// wrapper's own shared UNCACHED memory reader. Nothing in the datapath is a
// stand-in; only device memory, the upstream scheduler and the downstream
// notifier are modelled.
//
// Why randomWalk and not pageRank: pageRank's OVERLAP loop body is
// straight-line, so its collapsed chain is reentry -> cont0 -> cont1 -> back
// to the reentry. randomWalk's body contains an `if`, so the compiler emits a
// join task `applyFn_reentry0_afterif0` that owns the induction-variable
// increment and sits BETWEEN the last continuation and the loop head. That
// shape, and the uncached shared reader, are what this testbench covers.
//
// Device memory image (byte offsets; a "pointer" in a closure is one):
//   pGraph[2u]   = byte offset of u's neighbour list (uint64)
//   pGraph[2u+1] = degree of u                       (uint64)
//   global_buffer[u*walkLength + step]               (int32, pre-set to -1)
//
// The kernel is integer-only and strictly sequential per walk, so the golden
// below is a BIT-EXACT expectation, element for element, including the -1 tail
// left by a walk that stopped early. Anything else is a real defect.
// ===========================================================================

#include <systemc.h>
#include <verilated.h>

#include "Vwrap.h"
#include "Vwrap___024root.h"
#include "verilated_vcd_c.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// ─── knobs ──────────────────────────────────────────────────────────────────
static int MEM_LATENCY = 24;
static int NVERT = 16;
static int NTASK = 0; // 0 => all vertices
static int WALK_LEN = 32;
static long long MAX_CYCLES = 400000;
static bool VERBOSE = false;
static unsigned SEED = 1;
// The same threshold the host driver programs: (uint32)(0.15f * UINT32_MAX).
static uint32_t STOP_THRESH = (uint32_t)(0.15f * (double)UINT32_MAX);

// ─── device memory + graph ──────────────────────────────────────────────────
static std::vector<uint8_t> Mem;
static uint64_t pGraphOff, bufOff;
static std::vector<std::vector<uint32_t>> Adj;
static std::vector<int32_t> Gold;

// ─── the kernel's RNG, verbatim from randomWalk.cpp ─────────────────────────
static inline uint32_t xorshift32(uint32_t &x) {
  x ^= x >> 13;
  x ^= x << 17;
  x ^= x >> 5;
  return x;
}
static inline uint32_t uniform_u32(uint32_t &s, uint32_t n) {
  uint64_t prod = (uint64_t)xorshift32(s) * (uint64_t)n;
  return (uint32_t)(prod >> 32);
}

// Irregular, densely indexed, connected. A regular clique cannot distinguish a
// correct walk from one that samples the wrong neighbour — every neighbour is
// then equivalent — which is exactly the trap the system-level verification of
// pageRank hit with the stock synth_k* graphs.
static void buildGraph(int V) {
  Adj.assign(V, {});
  auto addEdge = [&](int a, int b) {
    if (a == b)
      return;
    Adj[a].push_back(b);
    Adj[b].push_back(a);
  };
  unsigned s = SEED;
  auto rnd = [&]() {
    s = s * 1103515245u + 12345u;
    return (s >> 16) & 0x7fff;
  };
  std::vector<int> perm(V);
  for (int i = 0; i < V; i++)
    perm[i] = i;
  for (int i = V - 1; i > 0; i--)
    std::swap(perm[i], perm[rnd() % (i + 1)]);
  for (int i = 0; i + 1 < V; i++)
    addEdge(perm[i], perm[i + 1]); // spanning path: no isolated vertex
  for (int e = 0; e < V * 2; e++)
    addEdge(rnd() % V, rnd() % V);
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
  bufOff = cur;
  cur += 4ull * V * WALK_LEN;
  cur = (cur + 63) & ~63ull;
  Mem.assign(cur + 4096, 0);

  for (int u = 0; u < V; u++) {
    uint64_t ptr = Adj[u].empty() ? 0 : listOff[u];
    std::memcpy(&Mem[pGraphOff + 16ull * u], &ptr, 8);
    uint64_t d = Adj[u].size();
    std::memcpy(&Mem[pGraphOff + 16ull * u + 8], &d, 8);
    for (size_t j = 0; j < Adj[u].size(); j++)
      std::memcpy(&Mem[listOff[u] + 4 * j], &Adj[u][j], 4);
  }
  // The host fills the walk buffer with -1; an untouched element must stay -1,
  // which is what makes "produced nothing" distinguishable from "produced the
  // wrong thing".
  const int32_t minus1 = -1;
  for (int i = 0; i < V * WALK_LEN; i++)
    std::memcpy(&Mem[bufOff + 4ull * i], &minus1, 4);

  // Golden: applyFn from randomWalk.cpp, run in software.
  Gold.assign((size_t)V * WALK_LEN, -1);
  for (int u = 0; u < V; u++) {
    uint32_t seed = 3735928559U ^ (uint32_t)u;
    uint32_t rng_s = seed ? seed : 1U;
    int32_t *w = &Gold[(size_t)u * WALK_LEN];
    uint32_t current = (uint32_t)u;
    w[0] = (int32_t)current;
    int done = 0;
    for (int step = 1; step < WALK_LEN && done == 0; step++) {
      if (xorshift32(rng_s) < STOP_THRESH)
        done = 1;
      uint32_t degree = (uint32_t)Adj[current].size();
      if (degree == 0) {
        w[step] = (int32_t)current;
        done = 1;
      } else {
        uint32_t k = uniform_u32(rng_s, degree);
        current = Adj[current][k];
        w[step] = (int32_t)current;
      }
    }
  }
}

static void memRead(uint64_t addr, unsigned bytes, uint8_t *out) {
  std::memset(out, 0, bytes);
  if (addr + bytes <= Mem.size())
    std::memcpy(out, &Mem[addr], bytes);
}

// ─── AXI read slave ─────────────────────────────────────────────────────────
// One request queue per ARID: AXI orders responses only within an ID, and the
// shared reader routes replies to its streams by RID. A slave answering
// everything with RID=0 would misroute every reply.
struct AxiRd {
  std::string name;
  CData *arvalid, *arready, *rvalid, *rready, *rlast, *arlen, *arsize, *arid,
      *rid;
  QData *araddr;
  void *rdata;
  unsigned dataBytes;
  struct Pending {
    long long due;
    uint64_t addr;
    unsigned bytes;
    unsigned id;
  };
  std::map<unsigned, std::vector<Pending>> q;
  long long reads = 0;
  bool inFlight = false;
  unsigned curId = 0;
  uint8_t curData[64];
  size_t outstanding() const {
    size_t n = 0;
    for (auto &kv : q)
      n += kv.second.size();
    return n;
  }
};

// ─── AXI write slave ────────────────────────────────────────────────────────
struct AxiWr {
  std::string name;
  CData *awvalid, *awready, *wvalid, *wready, *wlast, *bvalid, *bready, *wstrb;
  QData *awaddr;
  void *wdata;
  unsigned dataBytes;
  std::vector<uint64_t> awq;
  long long writes = 0;
  int bpend = 0;
};

int sc_main(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--mem-latency")
      MEM_LATENCY = std::atoi(argv[++i]);
    else if (a == "--vertices")
      NVERT = std::atoi(argv[++i]);
    else if (a == "--tasks")
      NTASK = std::atoi(argv[++i]);
    else if (a == "--walk-length")
      WALK_LEN = std::atoi(argv[++i]);
    else if (a == "--seed")
      SEED = (unsigned)std::atoi(argv[++i]);
    else if (a == "--max-cycles")
      MAX_CYCLES = std::atoll(argv[++i]);
    else if (a == "--verbose")
      VERBOSE = true;
  }
  buildGraph(NVERT);
  if (NTASK <= 0 || NTASK > NVERT)
    NTASK = NVERT;

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
  auto addRd = [&](const char *nm, CData *arv, CData *arr, QData *ara,
                   CData *arl, CData *ars, CData *ari, CData *rv, CData *rr,
                   CData *rl, CData *ri, void *rdp, unsigned bytes) {
    AxiRd p{};
    p.name = nm;
    p.arvalid = arv; p.arready = arr; p.araddr = ara; p.arlen = arl;
    p.arsize = ars; p.arid = ari; p.rvalid = rv; p.rready = rr; p.rlast = rl;
    p.rid = ri; p.rdata = rdp; p.dataBytes = bytes;
    rd.push_back(p);
  };
#define ADD_RD(NM, BYTES)                                                      \
  addRd(#NM, &d->m_axi_gmem_##NM##_ARVALID, &d->m_axi_gmem_##NM##_ARREADY,     \
        &d->m_axi_gmem_##NM##_ARADDR, &d->m_axi_gmem_##NM##_ARLEN,             \
        &d->m_axi_gmem_##NM##_ARSIZE, &d->m_axi_gmem_##NM##_ARID,              \
        &d->m_axi_gmem_##NM##_RVALID, &d->m_axi_gmem_##NM##_RREADY,            \
        &d->m_axi_gmem_##NM##_RLAST, &d->m_axi_gmem_##NM##_RID,                \
        (void *)&d->m_axi_gmem_##NM##_RDATA, BYTES)

  // The one shared uncached reader: pGraph[2c], pGraph[2c+1] and nbrs[k], all
  // on a 64-bit bus. Full-width mode means ARADDR is already bus-aligned and
  // the reader extracts the element by byte lane, so returning the 8 bytes at
  // ARADDR is the correct model.
  ADD_RD(shared, 8);
  // The three collapsed PEs that touch memory do so only to STORE the walk,
  // but their m_axi bundles carry a read channel too; model it so an
  // unexpected read is visible rather than a silent hang.
  ADD_RD(applyFn, 4);
  ADD_RD(applyFn_reentry0_cont0, 4);
  ADD_RD(applyFn_reentry0_cont1, 4);

  std::vector<AxiWr> wr;
  auto addWr = [&](const char *nm, CData *awv, CData *awr, QData *awa,
                   CData *wv, CData *wrdy, CData *wl, CData *ws, void *wd,
                   CData *bv, CData *br, unsigned bytes) {
    AxiWr p{};
    p.name = nm;
    p.awvalid = awv; p.awready = awr; p.awaddr = awa; p.wvalid = wv;
    p.wready = wrdy; p.wlast = wl; p.wstrb = ws; p.wdata = wd;
    p.bvalid = bv; p.bready = br; p.dataBytes = bytes;
    wr.push_back(p);
  };
#define ADD_WR(NM, BYTES)                                                      \
  addWr(#NM, &d->m_axi_gmem_##NM##_AWVALID, &d->m_axi_gmem_##NM##_AWREADY,     \
        &d->m_axi_gmem_##NM##_AWADDR, &d->m_axi_gmem_##NM##_WVALID,            \
        &d->m_axi_gmem_##NM##_WREADY, &d->m_axi_gmem_##NM##_WLAST,             \
        &d->m_axi_gmem_##NM##_WSTRB, (void *)&d->m_axi_gmem_##NM##_WDATA,      \
        &d->m_axi_gmem_##NM##_BVALID, &d->m_axi_gmem_##NM##_BREADY, BYTES)

  ADD_WR(applyFn, 4);                 // my_walk[0]        = u
  ADD_WR(applyFn_reentry0_cont0, 4);  // my_walk[step]     = current (dead end)
  ADD_WR(applyFn_reentry0_cont1, 4);  // my_walk[step]     = nbrs[k]

  // ── task injection: applyFn_task, packed, 64 bytes ────────────────────────
  //   _cont(8) vertexCount(4) u(4) pGraph(8) global_buffer(8) stop_thresh(8)
  //   walkLength(4) _padding[20]
  // The continuation tag must be ONEWALKPERNODE_CONT0_TAG (1), or applyFn_exit0
  // routes the completion to one of its internal ports (which the wrapper
  // sinks) and no argOut ever leaves.
  const uint64_t CONT = (1ull << 56) | 0x9bc00ull;
  auto packTask = [&](uint32_t u, uint8_t *b) {
    std::memset(b, 0, 64);
    uint32_t vc = (uint32_t)NVERT;
    uint64_t st = STOP_THRESH;
    uint32_t wl = (uint32_t)WALK_LEN;
    std::memcpy(b + 0, &CONT, 8);
    std::memcpy(b + 8, &vc, 4);
    std::memcpy(b + 12, &u, 4);
    std::memcpy(b + 16, &pGraphOff, 8);
    std::memcpy(b + 24, &bufOff, 8);
    std::memcpy(b + 32, &st, 8);
    std::memcpy(b + 40, &wl, 4);
  };
  int nextTask = 0;

  // ── reset ─────────────────────────────────────────────────────────────────
  d->ap_clk = 0;
  d->ap_rst_n = 0;
  d->taskIn_TVALID = 0;
  d->argOut_TREADY = 1;
  for (auto &p : rd) {
    *p.arready = 1;
    *p.rvalid = 0;
  }
  for (auto &p : wr) {
    *p.awready = 1;
    *p.wready = 1;
    *p.bvalid = 0;
  }

  long long cyc = 0;
  auto lowPhase = [&]() {
    d->ap_clk = 0;
    d->eval();
    if (vcd)
      vcd->dump((uint64_t)(cyc * 10));
  };
  auto edge = [&]() {
    d->ap_clk = 1;
    d->eval();
    if (vcd)
      vcd->dump((uint64_t)(cyc * 10 + 5));
    cyc++;
  };
  for (int i = 0; i < 20; i++) {
    lowPhase();
    edge();
  }
  d->ap_rst_n = 1;

  long long tasksIn = 0, argOuts = 0, badTag = 0;
  long long lastProgress = 0, prevActivity = -1;
  int drain = -1;
  bool stalled = false;
  // Walk steps retired: one m_axi store == one element of some walk.
  long long stores = 0, firstStoreCyc = -1, lastStoreCyc = -1;

  auto totalReads = [&]() {
    long long s = 0;
    for (auto &p : rd)
      s += p.reads;
    return s;
  };
  auto totalWrites = [&]() {
    long long s = 0;
    for (auto &p : wr)
      s += p.writes;
    return s;
  };

  while (cyc < MAX_CYCLES) {
    lowPhase();

    // ---- drive taskIn ----
    if (nextTask < NTASK) {
      uint8_t b[64];
      packTask((uint32_t)nextTask, b);
      for (int i = 0; i < 16; i++) {
        uint32_t w;
        std::memcpy(&w, b + 4 * i, 4);
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
        long long best = -1;
        unsigned bestId = 0;
        for (auto &kv : p.q) {
          if (kv.second.empty() || kv.second.front().due > cyc)
            continue;
          if (best < 0 || kv.second.front().due < best) {
            best = kv.second.front().due;
            bestId = kv.first;
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
        *p.rvalid = 1;
        *p.rlast = 1;
        *p.rid = (CData)p.curId;
        std::memcpy(p.rdata, p.curData, p.dataBytes);
      } else {
        *p.rvalid = 0;
      }
    }
    for (auto &p : wr) {
      *p.awready = 1;
      *p.wready = 1;
      // BVALID only after a burst has actually landed: holding it high hands
      // the HLS write engine responses it never asked for.
      *p.bvalid = p.bpend > 0;
    }

    d->eval();

    // ---- sample exactly what the DUT latches at the coming edge ----
    bool taskFire = d->taskIn_TVALID && d->taskIn_TREADY;
    bool argOutFire = d->argOut_TVALID && d->argOut_TREADY;
    uint64_t argOutCap = d->argOut_TDATA;

    struct ArCap {
      bool fire;
      uint64_t addr;
      unsigned bytes, id;
    };
    std::vector<ArCap> arc(rd.size());
    for (size_t i = 0; i < rd.size(); i++) {
      auto &p = rd[i];
      arc[i].fire = *p.arvalid && *p.arready;
      arc[i].addr = *p.araddr;
      arc[i].bytes = 1u << *p.arsize;
      arc[i].id = *p.arid;
      if (arc[i].fire && *p.arlen != 0)
        std::printf("[%lld] %s: multi-beat read (ARLEN=%u) not modelled\n", cyc,
                    p.name.c_str(), (unsigned)*p.arlen);
      if (arc[i].fire && arc[i].bytes > p.dataBytes)
        arc[i].bytes = p.dataBytes;
    }
    std::vector<bool> rfire(rd.size());
    for (size_t i = 0; i < rd.size(); i++)
      rfire[i] = *rd[i].rvalid && *rd[i].rready;

    struct WrCap {
      bool aw, w, b;
      uint64_t awAddr;
      uint32_t data, strb;
    };
    std::vector<WrCap> wc(wr.size());
    for (size_t i = 0; i < wr.size(); i++) {
      auto &p = wr[i];
      wc[i].aw = *p.awvalid && *p.awready;
      wc[i].w = *p.wvalid && *p.wready;
      wc[i].b = *p.bvalid && *p.bready;
      wc[i].awAddr = *p.awaddr;
      wc[i].data = *(IData *)p.wdata;
      wc[i].strb = *p.wstrb;
    }

    edge();

    // ---- commit ----
    if (taskFire) {
      if (VERBOSE)
        std::printf("[%lld] taskIn u=%d\n", cyc, nextTask);
      tasksIn++;
      nextTask++;
    }
    if (argOutFire) {
      argOuts++;
      if ((argOutCap >> 56) != 1)
        badTag++;
      if (VERBOSE)
        std::printf("[%lld] argOut cont=0x%llx tag=%u\n", cyc,
                    (unsigned long long)argOutCap, (unsigned)(argOutCap >> 56));
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
        if (!qq.empty())
          qq.erase(qq.begin());
        rd[i].inFlight = false;
      }
    }
    for (size_t i = 0; i < wr.size(); i++) {
      auto &p = wr[i];
      if (wc[i].b && p.bpend > 0)
        p.bpend--;
      if (wc[i].aw)
        p.awq.push_back(wc[i].awAddr);
      if (wc[i].w) {
        uint64_t a = p.awq.empty() ? 0 : p.awq.front();
        if (!p.awq.empty())
          p.awq.erase(p.awq.begin());
        for (unsigned k = 0; k < p.dataBytes; k++)
          if (wc[i].strb & (1u << k)) {
            uint64_t t = a + k;
            if (t < Mem.size())
              Mem[t] = (uint8_t)(wc[i].data >> (8 * k));
          }
        p.writes++;
        p.bpend++;
        stores++;
        if (firstStoreCyc < 0)
          firstStoreCyc = cyc;
        lastStoreCyc = cyc;
        if (VERBOSE)
          std::printf("[%lld] %s W addr=0x%llx (elem %lld) data=%d strb=%x\n",
                      cyc, p.name.c_str(), (unsigned long long)a,
                      (long long)(((int64_t)a - (int64_t)bufOff) / 4),
                      (int)wc[i].data, wc[i].strb);
      }
    }

    // ---- stall detection ----
    long long activity = tasksIn + argOuts + totalReads() + totalWrites();
    if (activity != prevActivity) {
      prevActivity = activity;
      lastProgress = cyc;
    } else if (cyc - lastProgress > 4 * MEM_LATENCY + 3000) {
      std::printf("\n### STALL at cycle %lld — no AXI traffic, no task in, no "
                  "result out for %lld cycles\n",
                  cyc, cyc - lastProgress);
      stalled = true;
      break;
    }

    // Drain inside the main loop so the AXI slaves keep running while the last
    // stores settle.
    if (argOuts >= NTASK && drain < 0)
      drain = 2000;
    if (drain > 0 && --drain == 0)
      break;
  }

  // ── report ────────────────────────────────────────────────────────────────
  std::printf("\n=== randomWalk_overlap wrapper testbench ===\n");
  std::printf("  vertices/tasks    : %d / %d   walkLength %d\n", NVERT, NTASK,
              WALK_LEN);
  std::printf("  cycles            : %lld\n", cyc);
  std::printf("  tasks injected    : %lld\n", tasksIn);
  std::printf("  argOut fired      : %lld / %d%s\n", argOuts, NTASK,
              badTag ? "   (WRONG CONT TAG)" : "");
  std::printf("  walk stores       : %lld\n", stores);
  for (auto &p : wr)
    std::printf("  %-26s writes=%lld\n", p.name.c_str(), p.writes);
  for (auto &p : rd)
    std::printf("  %-26s reads=%-7lld outstanding=%zu\n", p.name.c_str(),
                p.reads, p.outstanding());
  std::printf("  taskIn_TREADY now : %d\n", (int)d->taskIn_TREADY);
  if (firstStoreCyc >= 0 && lastStoreCyc > firstStoreCyc)
    std::printf("  PERF              : %.4f walk-steps/cycle (%lld stores over "
                "%lld cycles)\n",
                (double)stores / (double)(lastStoreCyc - firstStoreCyc), stores,
                lastStoreCyc - firstStoreCyc);

  // ── bit-exact comparison, element for element ─────────────────────────────
  long long exact = 0, total = (long long)NTASK * WALK_LEN;
  int shown = 0;
  for (int u = 0; u < NTASK; u++) {
    for (int s = 0; s < WALK_LEN; s++) {
      int32_t got;
      std::memcpy(&got, &Mem[bufOff + 4ull * ((size_t)u * WALK_LEN + s)], 4);
      int32_t exp = Gold[(size_t)u * WALK_LEN + s];
      if (got == exp) {
        exact++;
      } else if (shown < 10) {
        std::printf("    mismatch v%-3d step %-3d got %d expected %d\n", u, s,
                    got, exp);
        shown++;
      }
    }
  }
  // The buffer starts as all -1, so "matched only where the golden is also -1"
  // means nothing was computed at all. Report that separately: it is a very
  // different failure from a wrong walk.
  long long goldMinus1 = 0;
  for (int u = 0; u < NTASK; u++)
    for (int s = 0; s < WALK_LEN; s++)
      if (Gold[(size_t)u * WALK_LEN + s] == -1)
        goldMinus1++;
  if (stores == 0)
    std::printf("  !! ZERO stores — the design produced nothing; the %lld "
                "'exact' elements are only the untouched -1 fill agreeing with "
                "the golden's own -1 tails\n",
                goldMinus1);

  bool ok = !stalled && argOuts == NTASK && badTag == 0 && exact == total;
  std::printf("  walk bit-exact    : %lld / %lld\n", exact, total);
  std::printf("  VERDICT           : %s\n", ok ? "PASS" : "FAIL");
  std::printf("===========================================\n");

  if (vcd)
    vcd->close();
  delete d;
  return ok ? 0 : 1;
}
