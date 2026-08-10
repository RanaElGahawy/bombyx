// ===========================================================================
// SystemC RTL testbench for the generated applyFn_overlap_wrapper.
//
// Drives the REAL Verilog — the bombyx-emitted wrapper plus the Vitis-HLS
// synthesised PEs — via Verilator. Unlike the behavioural model in
// ../overlap_model.cpp, nothing in the datapath is a stand-in: only the memory
// and the upstream scheduler are modelled.
//
// Purpose: reproduce and localise the reported deadlock (the PE absorbs some
// tasks, issues some reads, then blocks).
//
// Memory image (byte offsets; a "pointer" in a closure is one of these):
//   pGraph[2u]        = byte offset of u's neighbour list
//   pGraph[2u+1]      = degree of u
//   triangleCounts[u] = result written by applyFn_exit0
// Neighbour lists are sorted ascending, so applyFn's inner while loop is an
// ordered set intersection.
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
#include <string>
#include <vector>

static int MEM_LATENCY = 32;
static int NVERT = 8;
static int DEGREE = 3;
static int NTASK = 8;
static long long MAX_CYCLES = 100000;
static bool VERBOSE = false;

// ─── Device memory ──────────────────────────────────────────────────────────
static std::vector<uint8_t> Mem;
static uint64_t pGraphOff, triOff;
static std::vector<std::vector<uint32_t>> Adj;
static std::vector<uint32_t> Expected;

static void buildGraph(int V) {
  Adj.assign(V, {});
  for (int u = 0; u < V; u++)
    for (int k = 1; k <= DEGREE; k++) {
      int v = (u + k) % V;
      if (v != u)
        Adj[u].push_back((uint32_t)v);
    }
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
  triOff = cur;
  cur += 4ull * V;
  Mem.assign(cur + 4096, 0);

  for (int u = 0; u < V; u++) {
    uint64_t ptr = Adj[u].empty() ? 0 : listOff[u];
    std::memcpy(&Mem[pGraphOff + 16ull * u], &ptr, 8);
    uint64_t d = Adj[u].size();
    std::memcpy(&Mem[pGraphOff + 16ull * u + 8], &d, 8);
    for (size_t j = 0; j < Adj[u].size(); j++)
      std::memcpy(&Mem[listOff[u] + 4 * j], &Adj[u][j], 4);
  }

  Expected.assign(V, 0);
  for (int u = 0; u < V; u++) {
    const auto &nu = Adj[u];
    uint32_t count = 0;
    for (size_t i = 0; i < nu.size(); i++) {
      const auto &nv = Adj[nu[i]];
      size_t k = 0, l = 0;
      uint32_t c = 0;
      while (k < nu.size() && l < nv.size()) {
        if (nu[k] == nv[l]) { c++; k++; l++; }
        else if (nu[k] < nv[l]) k++;
        else l++;
      }
      count += c;
    }
    Expected[u] = count;
  }
}

static uint64_t memRead(uint64_t addr, unsigned bytes) {
  uint64_t v = 0;
  if (addr + bytes <= Mem.size())
    std::memcpy(&v, &Mem[addr], bytes);
  return v;
}

// ─── AXI read-slave model ───────────────────────────────────────────────────
// Accepts one AR per cycle, unlimited outstanding, returns the beat
// MEM_LATENCY cycles later. Single-beat only, which the PEs' scalar loads
// satisfy — asserted, not assumed.
struct AxiPort {
  std::string name;
  // Handles to the DUT's signals for this master.
  CData *arvalid, *arready, *rvalid, *rready, *rlast;
  QData *araddr;
  CData *arlen, *arsize, *arid, *rid;
  void *rdata;      // 32- or 64-bit depending on the PE's synthesised width
  unsigned dataBits;
  CData *awvalid, *awready, *wvalid, *wready, *wlast, *bvalid, *bready;
  QData *awaddr;
  void *wdata;
  CData *wstrb;

  struct Pending { long long due; uint64_t addr; unsigned bytes; unsigned id; };
  // AXI orders responses only WITHIN an ID, so one queue per ID. The channel
  // split (`#pragma HLS ... channel = N`) gives a read-only PE one ID per read
  // site, so a slave that always answers with RID=0 never completes the reads
  // issued on the other IDs — the PE then waits forever.
  std::map<unsigned, std::vector<Pending>> q;
  size_t outstanding() const {
    size_t n = 0;
    for (auto &kv : q) n += kv.second.size();
    return n;
  }
  long long reads = 0, writes = 0;
  long long firstRead = -1, lastRead = -1;
  bool inFlightR = false;
  uint64_t rval = 0;
  unsigned rvalId = 0;
};

int sc_main(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--mem-latency") MEM_LATENCY = std::atoi(argv[++i]);
    else if (a == "--vertices") NVERT = std::atoi(argv[++i]);
    else if (a == "--degree") DEGREE = std::atoi(argv[++i]);
    else if (a == "--tasks") NTASK = std::atoi(argv[++i]);
    else if (a == "--max-cycles") MAX_CYCLES = std::atoll(argv[++i]);
    else if (a == "--verbose") VERBOSE = true;
  }
  buildGraph(NVERT);
  if (NTASK > NVERT) NTASK = NVERT;

  Verilated::commandArgs(argc, argv);
  Verilated::traceEverOn(true);
  Vwrap *d = new Vwrap("dut");
  VerilatedVcdC *vcd = nullptr;
  if (getenv("TB_VCD")) {
    vcd = new VerilatedVcdC;
    d->trace(vcd, 4);
    vcd->open(getenv("TB_VCD"));
  }

  // ── AXI masters exposed by the wrapper ────────────────────────────────────
  std::vector<AxiPort> ports;
  auto addPort = [&](const char *nm, CData *arv, CData *arr, QData *ara,
                     CData *arl, CData *ars, CData *ari, CData *rv, CData *rr,
                     CData *rl, CData *ri, void *rd, unsigned bits) {
    AxiPort p{};
    p.name = nm;
    p.arvalid = arv; p.arready = arr; p.araddr = ara;
    p.arlen = arl; p.arsize = ars; p.arid = ari;
    p.rvalid = rv; p.rready = rr; p.rlast = rl; p.rid = ri;
    p.rdata = rd; p.dataBits = bits;
    ports.push_back(p);
  };

#define ADD_RD(NM, BITS)                                                       \
  addPort(#NM, &d->m_axi_gmem_##NM##_ARVALID, &d->m_axi_gmem_##NM##_ARREADY,   \
          &d->m_axi_gmem_##NM##_ARADDR, &d->m_axi_gmem_##NM##_ARLEN,           \
          &d->m_axi_gmem_##NM##_ARSIZE, &d->m_axi_gmem_##NM##_ARID,            \
          &d->m_axi_gmem_##NM##_RVALID, &d->m_axi_gmem_##NM##_RREADY,          \
          &d->m_axi_gmem_##NM##_RLAST, &d->m_axi_gmem_##NM##_RID,              \
          (void *)&d->m_axi_gmem_##NM##_RDATA, BITS)

  ADD_RD(applyFn, 64);
  ADD_RD(memrd0, 32);
  ADD_RD(memrd1, 64);
  ADD_RD(memrd2, 64);
  ADD_RD(memrd3, 32);
  ADD_RD(memrd4, 32);
  ADD_RD(applyFn_exit0, 32);

  // Write channel of the exit PE (the argOut write buffer's store).
  auto wr_awvalid = &d->m_axi_gmem_applyFn_exit0_AWVALID;
  auto wr_awready = &d->m_axi_gmem_applyFn_exit0_AWREADY;
  auto wr_awaddr = &d->m_axi_gmem_applyFn_exit0_AWADDR;
  auto wr_wvalid = &d->m_axi_gmem_applyFn_exit0_WVALID;
  auto wr_wready = &d->m_axi_gmem_applyFn_exit0_WREADY;
  auto wr_wdata = &d->m_axi_gmem_applyFn_exit0_WDATA;
  auto wr_wstrb = &d->m_axi_gmem_applyFn_exit0_WSTRB;
  auto wr_bvalid = &d->m_axi_gmem_applyFn_exit0_BVALID;
  auto wr_bready = &d->m_axi_gmem_applyFn_exit0_BREADY;

  // ── Task injection ────────────────────────────────────────────────────────
  // applyFn_task: { addr_t _cont; uint32_t u; addr_t pGraph;
  //                 addr_t triangleCounts; uint8_t _padding[4]; } packed.
  struct Task { uint64_t cont; uint32_t u; uint64_t pg; uint64_t tc; };
  std::vector<Task> tasks;
  for (int u = 0; u < NTASK; u++)
    tasks.push_back({(uint64_t)1 << 56 /* SPAWNERFUNCTION_CONT0_TAG */,
                     (uint32_t)u, pGraphOff, triOff});
  size_t nextTask = 0;

  auto packTask = [&](const Task &t, uint32_t *w /*8 words = 256b*/) {
    uint8_t b[32];
    std::memset(b, 0, sizeof b);
    std::memcpy(b + 0, &t.cont, 8);
    std::memcpy(b + 8, &t.u, 4);
    std::memcpy(b + 12, &t.pg, 8);
    std::memcpy(b + 20, &t.tc, 8);
    std::memcpy(w, b, 32);
  };

  // ── Reset ─────────────────────────────────────────────────────────────────
  d->ap_clk = 0;
  d->ap_rst_n = 0;
  d->taskIn_TVALID = 0;
  d->argOut_TREADY = 1;
  d->argDataOut_TREADY = 1;
  for (auto &p : ports) {
    *p.arready = 1;
    *p.rvalid = 0;
  }
  *wr_awready = 1;
  *wr_wready = 1;
  *wr_bvalid = 0;

  long long cyc = 0;
  // Two-phase clocking. `lowPhase` settles the combinational world with clk
  // low; the testbench then samples EXACTLY what the DUT will see, and `edge`
  // applies the rising edge with nothing evaluated in between. Sampling after
  // an intervening eval() drops the first beat of a burst and double-counts the
  // second — which is precisely how this testbench first mis-read a correct
  // result stream as a duplicate.
  auto lowPhase = [&]() {
    d->ap_clk = 0;
    d->eval();
    if (vcd) vcd->dump((uint64_t)(cyc * 10));
  };
  auto edge = [&]() {
    d->ap_clk = 1;
    d->eval();
    if (vcd) vcd->dump((uint64_t)(cyc * 10 + 5));
    cyc++;
  };
  auto tick = [&]() { lowPhase(); edge(); };
  for (int i = 0; i < 20; i++) tick();
  d->ap_rst_n = 1;

  // ── Instrumentation ───────────────────────────────────────────────────────
  long long tasksIn = 0, argOuts = 0, argDatas = 0;
  long long lastProgress = 0;
  long long prevActivity = -1;

  auto totalReads = [&]() {
    long long s = 0;
    for (auto &p : ports) s += p.reads;
    return s;
  };

  while (cyc < MAX_CYCLES) {
    lowPhase();

    // ---- drive taskIn ----
    if (nextTask < tasks.size()) {
      uint32_t w[8];
      packTask(tasks[nextTask], w);
      for (int i = 0; i < 8; i++) d->taskIn_TDATA[i] = w[i];
      d->taskIn_TVALID = 1;
    } else {
      d->taskIn_TVALID = 0;
    }

    // ---- AXI read slaves ----
    for (auto &p : ports) {
      *p.arready = 1;
      // Return a beat whose latency has elapsed.
      if (!p.inFlightR) {
        // Serve the oldest due request across IDs; within an ID the queue is
        // strictly FIFO, which is exactly AXI's ordering rule.
        long long best = -1;
        unsigned bestId = 0;
        for (auto &kv : p.q) {
          if (kv.second.empty()) continue;
          if (kv.second.front().due > cyc) continue;
          if (best < 0 || kv.second.front().due < best) {
            best = kv.second.front().due;
            bestId = kv.first;
          }
        }
        if (best >= 0) {
          auto pd = p.q[bestId].front();
          p.rval = memRead(pd.addr, pd.bytes);
          p.rvalId = pd.id;
          p.inFlightR = true;
        }
      }
      if (p.inFlightR) {
        *p.rvalid = 1;
        *p.rlast = 1;
        *p.rid = (CData)p.rvalId;
        if (p.dataBits <= 32) *(IData *)p.rdata = (IData)p.rval;
        else *(QData *)p.rdata = p.rval;
      } else {
        *p.rvalid = 0;
      }
    }
    // ---- exit0 write slave ----
    *wr_awready = 1;
    *wr_wready = 1;

    // Settle with clk low, then re-apply the stimulus so the sampled values are
    // the ones latched at the coming edge.
    d->eval();

    // ---- sample exactly what the DUT latches at the rising edge ----
    bool taskFire = d->taskIn_TVALID && d->taskIn_TREADY;
    bool argOutFire = d->argOut_TVALID && d->argOut_TREADY;
    bool argDataFire = d->argDataOut_TVALID && d->argDataOut_TREADY;
    // Capture payloads HERE. Reading them after the edge yields the NEXT beat's
    // data, which makes a correct back-to-back result stream look like a
    // duplicate with one beat missing.
    uint64_t argOutCap = d->argOut_TDATA;
    uint32_t argDataCap[8];
    for (int i = 0; i < 8; i++) argDataCap[i] = d->argDataOut_TDATA[i];

    struct ArCap { bool fire; uint64_t addr; unsigned bytes; unsigned id; };
    std::vector<ArCap> arcap(ports.size());
    for (size_t i = 0; i < ports.size(); i++) {
      AxiPort &p = ports[i];
      arcap[i].fire = *p.arvalid && *p.arready;
      arcap[i].addr = *p.araddr;
      arcap[i].bytes = 1u << *p.arsize;
      arcap[i].id = *p.arid;
      if (arcap[i].fire && *p.arlen != 0) {
        std::printf("[%lld] %s: multi-beat read (ARLEN=%u) not modelled\n", cyc,
                    p.name.c_str(), (unsigned)*p.arlen);
      }
    }
    std::vector<bool> rfire(ports.size());
    for (size_t i = 0; i < ports.size(); i++)
      rfire[i] = *ports[i].rvalid && *ports[i].rready;

    bool awFire = *wr_awvalid && *wr_awready;
    uint64_t awAddr = *wr_awaddr;
    bool wFire = *wr_wvalid && *wr_wready;
    uint32_t wData = (uint32_t)*wr_wdata;
    uint32_t wStrb = *wr_wstrb;

    // ---- advance the clock ----
    edge();

    // ---- commit ----
    if (taskFire) {
      tasksIn++;
      nextTask++;
      if (VERBOSE)
        std::printf("[%lld] taskIn u=%u\n", cyc, tasks[nextTask - 1].u);
    }
    if (argOutFire) {
      argOuts++;
      if (VERBOSE)
        std::printf("[%lld] argOut cont=0x%llx (tag=%u)\n", cyc,
                    (unsigned long long)argOutCap,
                    (unsigned)(argOutCap >> 56));
    }
    if (argDataFire) {
      argDatas++;
      // <type>_arg_out layout: { addr(8) data(...) size(4) allow(4) }
      uint8_t b[32];
      for (int i = 0; i < 8; i++)
        std::memcpy(b + 4 * i, &argDataCap[i], 4);
      uint64_t addr;
      uint32_t data;
      std::memcpy(&addr, b + 0, 8);
      std::memcpy(&data, b + 8, 4);
      if (addr + 4 <= Mem.size())
        std::memcpy(&Mem[addr], &data, 4);
      if (VERBOSE) {
        std::printf("[%lld] argDataOut addr=0x%lx data=%u raw=", cyc,
                    (unsigned long)addr, data);
        for (int i = 7; i >= 0; i--) std::printf("%08x", argDataCap[i]);
        std::printf("\n");
      }
    }
    for (size_t i = 0; i < ports.size(); i++) {
      if (arcap[i].fire) {
        ports[i].q[arcap[i].id].push_back(
            {cyc + MEM_LATENCY, arcap[i].addr, arcap[i].bytes, arcap[i].id});
        ports[i].reads++;
        if (ports[i].firstRead < 0) ports[i].firstRead = cyc;
        ports[i].lastRead = cyc;
        if (VERBOSE)
          std::printf("[%lld] %s AR addr=0x%lx size=%u id=%u\n", cyc,
                      ports[i].name.c_str(), (unsigned long)arcap[i].addr,
                      arcap[i].bytes, arcap[i].id);
      }
      if (rfire[i]) {
        auto &qq = ports[i].q[ports[i].rvalId];
        if (!qq.empty()) qq.erase(qq.begin());
        ports[i].inFlightR = false;
      }
    }
    if (awFire && VERBOSE)
      std::printf("[%lld] exit0 AW addr=0x%lx\n", cyc, (unsigned long)awAddr);
    if (wFire) {
      // Model the write: WSTRB selects bytes within the 32-bit bus.
      for (int i = 0; i < 4; i++)
        if (wStrb & (1u << i)) {
          uint64_t a = awAddr + i;
          if (a < Mem.size()) Mem[a] = (uint8_t)(wData >> (8 * i));
        }
      if (VERBOSE)
        std::printf("[%lld] exit0 W data=%u strb=%x\n", cyc, wData, wStrb);
    }

    // ---- stall detection ----
    long long activity = tasksIn + argOuts + argDatas + totalReads();
    if (activity != prevActivity) {
      prevActivity = activity;
      lastProgress = cyc;
    } else if (cyc - lastProgress > 4 * MEM_LATENCY + 2000) {
      std::printf("\n### STALL at cycle %lld — no AXI read, no task in, no "
                  "result out for %lld cycles\n",
                  cyc, cyc - lastProgress);
      break;
    }

    if (argOuts >= NTASK && argDatas >= NTASK) {
      // Let the last write drain.
      for (int i = 0; i < 64; i++) tick();
      break;
    }
  }

  // ── Report ────────────────────────────────────────────────────────────────
  std::printf("\n=== RTL wrapper testbench ===\n");
  std::printf("  cycles            : %lld\n", cyc);
  std::printf("  tasks injected    : %lld / %d\n", tasksIn, NTASK);
  std::printf("  argOut fired      : %lld\n", argOuts);
  std::printf("  argDataOut fired  : %lld\n", argDatas);
  std::printf("  taskIn_TREADY now : %d\n", (int)d->taskIn_TREADY);
  for (auto &p : ports)
    std::printf("  %-16s reads=%-6lld outstanding=%zu\n", p.name.c_str(),
                p.reads, p.outstanding());

  // The comparison rate: memrd3 serves the inner loop's `neighbors_u[k]` site,
  // exactly one read per comparison, so its issue rate IS the loop's II.
  for (auto &p : ports) {
    if (p.name != "memrd3") continue;
    if (p.reads > 1 && p.lastRead > p.firstRead)
      std::printf("  comparison II     : %.3f cycles/comparison (%lld "
                  "comparisons over %lld cycles)\n",
                  double(p.lastRead - p.firstRead) / double(p.reads - 1),
                  p.reads, p.lastRead - p.firstRead);
  }

  bool ok = true;
  uint64_t got = 0, exp = 0;
  for (int u = 0; u < NTASK; u++) {
    uint32_t v;
    std::memcpy(&v, &Mem[triOff + 4ull * u], 4);
    got += v;
    exp += Expected[u];
    if (v != Expected[u]) {
      ok = false;
      std::printf("    v%-3d got %-6u expected %-6u\n", u, v, Expected[u]);
    }
  }
  std::printf("  triangles         : got %llu expected %llu -> %s\n",
              (unsigned long long)got, (unsigned long long)exp,
              ok && argOuts >= NTASK ? "PASS" : "FAIL");
  std::printf("=============================\n");

  if (vcd) { vcd->close(); }
  delete d;
  return (ok && argOuts >= NTASK) ? 0 : 1;
}
