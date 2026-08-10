// ===========================================================================
// bombyx_mem_reader_cached — shared-port mem reader with a per-context line cache.
//
// Same shared-master idea as bombyx_mem_reader_shared (N read sites, one AXI
// master, ARID selects the stream), plus a private cache line per resident
// context in front of each stream.
//
// The context is allocated by the wrapper's admission control (a free queue of
// IDs replacing the in-flight counter), held for the whole residency of a merge
// loop, and returned at exit. It therefore identifies the loop, is dense
// (0..NUM_CTX-1), and carries no task data. That makes cache conflicts
// impossible by construction: a loop owns one entry, evicts only itself, and
// only when it advances past the line.
//
// Ordering — the reason for the completion queue
// ----------------------------------------------
// The wrapper's merge unit pairs the k-th reply of a stream with the k-th entry
// of that stream's state FIFO. A cache hit that returned immediately while an
// older miss was still outstanding would reorder replies and silently corrupt
// results. So every request — hit or miss — takes a slot in a per-stream
// in-order completion queue; a hit fills its slot at once, a miss when its R
// beat lands, and only the head is ever emitted.
//
// Misses of one stream all ride that stream's own ARID, so they return in
// order; a FIFO of slot indices therefore routes each R beat to the right slot
// with no searching.
//
// The bus is one line wide (256 bits), so a fill is a single beat and matches
// the RAMA/HBM width. A 32-bit element is taken from the line by byte offset.
// ===========================================================================

module bombyx_mrc_fifo #(
  parameter WIDTH = 64,
  parameter DEPTH = 64
) (
  input                  clk,
  input                  rst_n,
  input  [WIDTH-1:0]     in_data,
  input                  in_valid,
  output                 in_ready,
  output [WIDTH-1:0]     out_data,
  output                 out_valid,
  input                  out_ready
);
  localparam AW = $clog2(DEPTH);
  reg [WIDTH-1:0] mem [0:DEPTH-1];
  reg [AW:0] wptr, rptr;
  wire empty = (wptr == rptr);
  wire full  = (wptr[AW-1:0] == rptr[AW-1:0]) && (wptr[AW] != rptr[AW]);
  assign in_ready  = !full;
  assign out_valid = !empty;
  assign out_data  = mem[rptr[AW-1:0]];
  always @(posedge clk) if (in_valid && in_ready) mem[wptr[AW-1:0]] <= in_data;
  always @(posedge clk) begin
    if (!rst_n) begin wptr <= 0; rptr <= 0; end
    else begin
      if (in_valid  && in_ready)  wptr <= wptr + 1'b1;
      if (out_valid && out_ready) rptr <= rptr + 1'b1;
    end
  end
endmodule




module bombyx_mem_reader_cached #(
  parameter N               = 5,
  parameter TASK_W          = 256,
  parameter ARG_W           = 256,
  parameter ADDR_W          = 64,
  parameter BUS_W           = 256,  // one line per beat
  parameter ID_W            = 3,
  parameter NUM_CTX         = 64,   // == wrapper's MAX_INFLIGHT_l1
  parameter CTX_W           = 6,
  parameter [8*N-1:0] ELEM_LOG2_VEC = {8'd2, 8'd2, 8'd3, 8'd3, 8'd2},
  parameter [8*N-1:0] ELEM_W_VEC    = {8'd32, 8'd32, 8'd64, 8'd64, 8'd32},
  parameter [8*N-1:0] IDX_W_VEC     = {8'd62, 8'd62, 8'd61, 8'd61, 8'd62},
  // per-stream cache enable: only the merge-loop sites are worth lines
  parameter [N-1:0]   CACHE_EN      = 5'b11000,
  parameter CONT_LSB        = 0,
  parameter CONT_W          = 64,
  parameter BASE_LSB        = 64,
  parameter IDX_LSB         = 128,
  parameter PKT_ADDR_LSB    = 0,
  parameter PKT_DATA_LSB    = 64,
  parameter OUTSTANDING     = 64,   // AXI credits per stream
  parameter CQ_DEPTH        = 128   // completion-queue slots per stream
) (
  input                       ap_clk,
  input                       ap_rst_n,

  input  [N*TASK_W-1:0]       taskIn_TDATA,
  input  [N-1:0]              taskIn_TVALID,
  output [N-1:0]              taskIn_TREADY,
  // context that owns each request, valid with taskIn
  input  [N*CTX_W-1:0]        taskIn_CTX,

  output [N*64-1:0]           argOut_TDATA,
  output [N-1:0]              argOut_TVALID,
  input  [N-1:0]              argOut_TREADY,

  output [N*ARG_W-1:0]        argDataOut_TDATA,
  output [N-1:0]              argDataOut_TVALID,
  input  [N-1:0]              argDataOut_TREADY,

  output                      m_axi_gmem_AWVALID,
  input                       m_axi_gmem_AWREADY,
  output [ADDR_W-1:0]         m_axi_gmem_AWADDR,
  output [ID_W-1:0]           m_axi_gmem_AWID,
  output [7:0]                m_axi_gmem_AWLEN,
  output [2:0]                m_axi_gmem_AWSIZE,
  output [1:0]                m_axi_gmem_AWBURST,
  output [1:0]                m_axi_gmem_AWLOCK,
  output [3:0]                m_axi_gmem_AWCACHE,
  output [2:0]                m_axi_gmem_AWPROT,
  output [3:0]                m_axi_gmem_AWQOS,
  output [3:0]                m_axi_gmem_AWREGION,
  output                      m_axi_gmem_AWUSER,
  output                      m_axi_gmem_WVALID,
  input                       m_axi_gmem_WREADY,
  output [BUS_W-1:0]          m_axi_gmem_WDATA,
  output [(BUS_W/8)-1:0]      m_axi_gmem_WSTRB,
  output                      m_axi_gmem_WLAST,
  output [ID_W-1:0]           m_axi_gmem_WID,
  output                      m_axi_gmem_WUSER,
  output                      m_axi_gmem_ARVALID,
  input                       m_axi_gmem_ARREADY,
  output [ADDR_W-1:0]         m_axi_gmem_ARADDR,
  output [ID_W-1:0]           m_axi_gmem_ARID,
  output [7:0]                m_axi_gmem_ARLEN,
  output [2:0]                m_axi_gmem_ARSIZE,
  output [1:0]                m_axi_gmem_ARBURST,
  output [1:0]                m_axi_gmem_ARLOCK,
  output [3:0]                m_axi_gmem_ARCACHE,
  output [2:0]                m_axi_gmem_ARPROT,
  output [3:0]                m_axi_gmem_ARQOS,
  output [3:0]                m_axi_gmem_ARREGION,
  output                      m_axi_gmem_ARUSER,
  input                       m_axi_gmem_RVALID,
  output                      m_axi_gmem_RREADY,
  input  [BUS_W-1:0]          m_axi_gmem_RDATA,
  input                       m_axi_gmem_RLAST,
  input  [ID_W-1:0]           m_axi_gmem_RID,
  input                       m_axi_gmem_RUSER,
  input  [1:0]                m_axi_gmem_RRESP,
  input                       m_axi_gmem_BVALID,
  output                      m_axi_gmem_BREADY,
  input  [1:0]                m_axi_gmem_BRESP,
  input  [ID_W-1:0]           m_axi_gmem_BID,
  input                       m_axi_gmem_BUSER,

  output [31:0]               dbg_cache_hits,
  output [31:0]               dbg_cache_misses
);

  localparam SEL_W    = (N > 1) ? $clog2(N) : 1;
  localparam LINE_LOG2 = $clog2(BUS_W/8);      // 5 for a 256-bit line
  localparam LANE_W   = LINE_LOG2;
  localparam TAG_W    = ADDR_W - LINE_LOG2;
  localparam CQ_AW    = $clog2(CQ_DEPTH);
  localparam CW       = $clog2(OUTSTANDING+1);

  // ---- input skid: decouple taskIn_TREADY from the arbiter ----------------
  // Everything downstream works off q_* instead of the ports, so the only thing
  // the upstream PE sees on its ready pin is this buffer's flop.
  wire [TASK_W-1:0] q_task  [0:N-1];
  wire [CTX_W-1:0]  q_ctx   [0:N-1];
  wire [N-1:0]      q_valid;
  wire [N-1:0]      q_ready;

  // ---- per-stream request decode -----------------------------------------
  wire [ADDR_W-1:0] s_addr [0:N-1];
  wire [CONT_W-1:0] s_cont [0:N-1];
  wire [CTX_W-1:0]  s_ctx  [0:N-1];

  genvar g;
  generate
    for (g = 0; g < N; g = g + 1) begin : gen_dec
      localparam integer EL2 = ELEM_LOG2_VEC[8*g +: 8];
      localparam integer IXW = IDX_W_VEC[8*g +: 8];

      bombyx_mrc_skid #(.WIDTH(TASK_W + CTX_W)) u_in_skid (
        .clk(ap_clk), .rst_n(ap_rst_n),
        .i_data({taskIn_CTX[g*CTX_W +: CTX_W], taskIn_TDATA[g*TASK_W +: TASK_W]}),
        .i_valid(taskIn_TVALID[g]), .i_ready(taskIn_TREADY[g]),
        .o_data({q_ctx[g], q_task[g]}),
        .o_valid(q_valid[g]), .o_ready(q_ready[g])
      );

      wire [TASK_W-1:0] t = q_task[g];
      wire [ADDR_W-1:0] base = t[BASE_LSB +: ADDR_W];
      wire [IXW-1:0]    idx  = t[IDX_LSB  +: IXW];
      assign s_addr[g] = base + ({{(ADDR_W-IXW){1'b0}}, idx} << EL2);
      assign s_cont[g] = t[CONT_LSB +: CONT_W];
      assign s_ctx[g]  = q_ctx[g];
    end
  endgenerate

  // ---- per-stream state ---------------------------------------------------
  wire [N-1:0] cq_full, cq_head_done, out_fire, miss_go, hit_go;
  wire [N-1:0] cred_ok;
  wire [N-1:0] want_ar;                 // wants to issue a miss this cycle
  wire [N-1:0] ar_accept_oh;            // stream served this cycle, one-hot
  wire         ar_skid_ready;           // AR output register can take a beat

  // Registered line address of each stream's staged request (see the staging
  // register in gen_stream below). The AR address is taken from here rather
  // than from the combinational decode, so the AXI address path starts at a
  // flop instead of at taskIn_TDATA.
  wire [TAG_W-1:0] s_rq_tag [0:N-1];

  // ---- registered arbitration --------------------------------------------
  // The round-robin used to be consumed combinationally, so one stream's
  // cq_wp/cq_rp reached EVERY other stream's credit counter in a single cycle:
  //   cq_full[g] -> want_ar[g] -> arbiter -> ar_mine[h] -> credits[h]/D.
  // That was the whole of the remaining critical path (7-8 levels, e.g.
  // gen_stream[4].cq_wp -> gen_stream[3].credits/D at -0.782 ns). Putting a flop
  // on the grant makes the arbiter a cone from flops to a flop, and ar_mine a
  // comparison against a registered index.
  //
  // Registering a grant is only safe if the winner still wants it a cycle later.
  // want_ar[g] is monotonic until it is served: it is
  //     rq_v && !rq_hit && !cq_full[g] && cred_ok[g]
  // and while a stream waits for its grant, rq_v holds (rq_go is miss_go, which
  // is ar_mine), cq_full[g] can only fall (cq_wp advances only on this stream's
  // own allocation, cq_rp only drains) and credits can only rise (they are spent
  // only by this stream's own ar_mine). So a registered grant can never point at
  // a stream that has withdrawn.
  reg  [SEL_W-1:0] rr_ptr, grant_idx;
  reg              grant_val;
  reg  [SEL_W-1:0] grant_idx_q;
  reg              grant_val_q;
  integer k;
  reg [SEL_W:0] cand;

  // The stream being accepted this cycle must be excluded from the next grant:
  // its want_ar is still high (rq_v updates on the same edge), so without the
  // mask it would be granted twice and issue a duplicate AR.
  wire [N-1:0] want_ar_eff = want_ar & ~ar_accept_oh;

  always @* begin
    grant_val = 1'b0; grant_idx = {SEL_W{1'b0}};
    for (k = N-1; k >= 0; k = k - 1) begin
      cand = {1'b0, rr_ptr} + k[SEL_W:0];
      if (cand >= N[SEL_W:0]) cand = cand - N[SEL_W:0];
      if (want_ar_eff[cand[SEL_W-1:0]]) begin
        grant_val = 1'b1; grant_idx = cand[SEL_W-1:0];
      end
    end
  end

  // Hold the registered grant until the AR skid takes it.
  always @(posedge ap_clk) begin
    if (!ap_rst_n) begin
      grant_val_q <= 1'b0;
      grant_idx_q <= {SEL_W{1'b0}};
    end else if (!grant_val_q || ar_skid_ready) begin
      grant_val_q <= grant_val;
      grant_idx_q <= grant_idx;
    end
  end

  wire [ADDR_W-1:0] ar_addr_line =
      {s_rq_tag[grant_idx_q], {LINE_LOG2{1'b0}}};

  // AR output register. The grant used to drive ARVALID/ARADDR straight onto the
  // bus, so the arbiter sat between the stream flops and an external pin, and
  // ARREADY came back combinationally into ar_mine -> rq_go -> taskIn_TREADY.
  // With the skid the arbiter's cone now starts at flops (rq_v, rq_hit,
  // cq_full, cred_ok_r) and ends at one, and ARVALID/ARADDR are registered.
  wire [SEL_W-1:0] ar_id_out;
  bombyx_mrc_skid #(.WIDTH(ADDR_W + SEL_W)) u_ar_skid (
    .clk(ap_clk), .rst_n(ap_rst_n),
    .i_data({ar_addr_line, grant_idx_q}),
    .i_valid(grant_val_q), .i_ready(ar_skid_ready),
    .o_data({m_axi_gmem_ARADDR, ar_id_out}),
    .o_valid(m_axi_gmem_ARVALID), .o_ready(m_axi_gmem_ARREADY)
  );

  // A request is committed when the skid takes it, not when it reaches the bus.
  // The skid is in-order, so per-stream AR order (and hence R order) is
  // unchanged; the at-most-two beats it holds are already counted against
  // credits, so the outstanding bound still holds.
  wire ar_accept = grant_val_q && ar_skid_ready;
  // one-hot of the stream accepted this cycle, for the arbiter mask above
  assign ar_accept_oh = ar_accept ? ({{(N-1){1'b0}}, 1'b1} << grant_idx_q)
                                  : {N{1'b0}};

  always @(posedge ap_clk) begin
    if (!ap_rst_n) rr_ptr <= {SEL_W{1'b0}};
    else if (ar_accept)
      rr_ptr <= (grant_idx_q == (N[SEL_W-1:0] - 1'b1)) ? {SEL_W{1'b0}}
                                                       : grant_idx_q + 1'b1;
  end

  assign m_axi_gmem_ARID    = {{(ID_W-SEL_W){1'b0}}, ar_id_out};
  assign m_axi_gmem_ARSIZE  = LINE_LOG2[2:0];

  wire         r_fire = m_axi_gmem_RVALID && m_axi_gmem_RREADY;
  wire [SEL_W-1:0] r_sel = m_axi_gmem_RID[SEL_W-1:0];
  assign m_axi_gmem_RREADY = 1'b1;      // a credit reserved the slot at AR time

  // Count requests, not cycles: several streams can serve one in the same
  // cycle, and only the cached streams are meaningful here.
  reg [31:0] hit_cnt, miss_cnt;
  integer c;
  reg [31:0] hit_n, miss_n;
  always @* begin
    hit_n = 32'd0; miss_n = 32'd0;
    for (c = 0; c < N; c = c + 1) if (CACHE_EN[c]) begin
      if (hit_go[c])  hit_n  = hit_n  + 32'd1;
      if (miss_go[c]) miss_n = miss_n + 32'd1;
    end
  end
  assign dbg_cache_hits   = hit_cnt;
  assign dbg_cache_misses = miss_cnt;
  always @(posedge ap_clk) begin
    if (!ap_rst_n) begin hit_cnt <= 0; miss_cnt <= 0; end
    else begin
      hit_cnt  <= hit_cnt  + hit_n;
      miss_cnt <= miss_cnt + miss_n;
    end
  end

  generate
    for (g = 0; g < N; g = g + 1) begin : gen_stream
      localparam integer ELW = ELEM_W_VEC[8*g +: 8];

      wire [TAG_W-1:0]  req_tag  = s_addr[g][ADDR_W-1:LINE_LOG2];
      wire [LANE_W-1:0] req_lane = s_addr[g][LANE_W-1:0];

      // ---- the private-per-context cache --------------------------------
      wire              c_hit;
      wire [BUS_W-1:0]  c_data;
      wire              fill_en;
      wire [CTX_W-1:0]  fill_ctx;
      wire [TAG_W-1:0]  fill_tag;
      wire [BUS_W-1:0]  fill_data;
      wire [CTX_W-1:0]  fill_ctx_q;   // straight off the miss FIFO
      wire [TAG_W-1:0]  fill_tag_q;

      if (CACHE_EN[g]) begin : gen_cache
        bombyx_ctx_cache #(
          .NUM_CTX(NUM_CTX), .CTX_W(CTX_W), .LINE_W(BUS_W), .TAG_W(TAG_W)
        ) u_cache (
          .clk(ap_clk), .rst_n(ap_rst_n),
          .lu_ctx(s_ctx[g]), .lu_tag(req_tag),
          .lu_hit(c_hit), .lu_data(c_data),
          .fill_en(fill_en), .fill_ctx(fill_ctx),
          .fill_tag(fill_tag), .fill_data(fill_data)
        );
      end else begin : gen_nocache
        assign c_hit  = 1'b0;
        assign c_data = {BUS_W{1'b0}};
      end

      // ---- stage 1 -> 2 staging register --------------------------------
      // The cache lookup and the issue decision used to resolve in one cycle:
      // tags[] LUTRAM read -> TAG_W-bit compare -> c_hit -> the cross-stream AR
      // arbiter -> the CQ write enable. That was 13 logic levels and the block's
      // critical path (-0.234 ns at 300 MHz out of context).
      //
      // Splitting it at c_hit puts the tag compare in cycle 1 and the
      // arbitration in cycle 2, with a flop between. Throughput is unchanged --
      // the stage accepts a new request in the same cycle it hands one on, so a
      // stream still issues one request per cycle -- at the cost of one extra
      // cycle of per-request latency.
      //
      // The captured (rq_hit, rq_data) pair is self-consistent: both are read
      // combinationally out of the same cache entry in the same cycle, so the
      // data always belongs to the tag that was compared. A fill landing after
      // the capture cannot invalidate it; at worst it makes a request that was
      // sampled as a miss fetch a line that has since arrived, which costs a
      // redundant read and is otherwise harmless.
      //
      // Ordering within a context is still safe: the wrapper's merge loop is
      // strictly serial per context (it cannot issue its next read until the
      // previous reply is delivered), and a context is only retired after its
      // last reply, so an extra stage in front of issue cannot reorder anything.
      reg               rq_v;
      reg [TAG_W-1:0]   rq_tag;
      reg [LANE_W-1:0]  rq_lane;
      reg [CONT_W-1:0]  rq_cont;
      reg [CTX_W-1:0]   rq_ctx;
      reg               rq_hit;
      reg [BUS_W-1:0]   rq_data;

      assign s_rq_tag[g] = rq_tag;

      wire rq_go    = hit_go[g] || miss_go[g];     // stage 2 consumes this cycle
      wire rq_ready = !rq_v || rq_go;              // ... so stage 1 may refill
      wire s1_fire  = q_valid[g] && rq_ready;
      // Drives the input skid, not the port: rq_ready still depends on the
      // arbiter, but that path now ends at the skid's flop instead of leaving
      // the module and clocking a wide register slice in the upstream PE.
      assign q_ready[g] = rq_ready;

      always @(posedge ap_clk) begin
        if (!ap_rst_n)     rq_v <= 1'b0;
        else if (s1_fire)  rq_v <= 1'b1;
        else if (rq_go)    rq_v <= 1'b0;
      end

      always @(posedge ap_clk) if (s1_fire) begin
        rq_tag  <= req_tag;
        rq_lane <= req_lane;
        rq_cont <= s_cont[g];
        rq_ctx  <= s_ctx[g];
        rq_hit  <= c_hit;
        rq_data <= c_data;
      end

      // ---- completion queue (in-order, hits and misses share it) ---------
      // The payload is split by *writer*, not by slot: hits are written at
      // allocate time (address cq_wp), fills when the R beat lands (address
      // missq_idx). One array taking both writes has two write ports in one
      // process, which defeats RAM inference -- Vivado then builds CQ_DEPTH x
      // BUS_W flip-flops plus a CQ_DEPTH:1 read mux per stream, which is what
      // over-utilised the device (2.6 M FF / 4 M LUT across 8 PEs). Split, each
      // array has exactly one write port and infers as a RAM. A miss slot's
      // hit-array entry is simply never read, so no storage is wasted that the
      // single-array version did not already write and discard.
      //
      // The payload is the ELEMENT, not the line. Both arrays used to hold whole
      // BUS_W lines and the element was taken out on the read side
      //     shifted = head_data >> {head_lane, 3'b000}
      // which put a 32:1 byte barrel shifter after the CQ_DEPTH:1 head mux, on
      // the emit path -- the largest remaining violation family (367 of the 5000
      // worst paths were cq_rp ==> u_memreader_cached). Extracting at write time
      // instead removes the shifter from that path and shrinks both arrays by
      // BUS_W/ELW (256 -> 32 or 64 bits), which also shortens the write-address
      // decode behind them (the f_idx ==> cq_fill_data family, another 224).
      //
      // Each write port has the lane of its own request in a flop already
      // (rq_lane for the hit port, f_lane off the miss FIFO for the fill port),
      // so both shifters sit on a flop -> shift -> RAM-write-data path with a
      // full period available, and neither is in series with an address decode.
      reg               cq_done [0:CQ_DEPTH-1];
      reg               cq_is_hit  [0:CQ_DEPTH-1];  // which array holds the payload
      reg [ELW-1:0]     cq_hit_data  [0:CQ_DEPTH-1]; // written only at cq_wp
      reg [ELW-1:0]     cq_fill_data [0:CQ_DEPTH-1]; // written only at missq_idx
      reg [CONT_W-1:0]  cq_cont [0:CQ_DEPTH-1];
      reg [CQ_AW:0]     cq_wp, cq_rp;

      wire cq_empty = (cq_wp == cq_rp);
      assign cq_full[g] = (cq_wp[CQ_AW-1:0] == cq_rp[CQ_AW-1:0]) &&
                          (cq_wp[CQ_AW] != cq_rp[CQ_AW]);
      assign cq_head_done[g] = !cq_empty && cq_done[cq_rp[CQ_AW-1:0]];

      // Credits bound the AXI outstanding of this stream. cred_ok is registered
      // rather than combinational off `credits`: it fed want_ar -> the shared
      // arbiter -> ar_mine -> every other stream's credit counter, which showed
      // up as a 7-level cross-stream path (gen_stream[0].credits ->
      // gen_stream[4].credits/D). Registering the *next* value keeps it exact --
      // at the coming edge `credits` equals credits_next, so this is not a
      // conservative approximation, just a retimed one.
      wire ar_mine = ar_accept && (grant_idx_q == g[SEL_W-1:0]);
      wire r_mine  = r_fire    && (r_sel     == g[SEL_W-1:0]);

      reg [CW-1:0] credits;
      reg          cred_ok_r;
      wire [CW-1:0] credits_next = credits - (ar_mine ? 1'b1 : 1'b0)
                                           + (r_mine  ? 1'b1 : 1'b0);
      assign cred_ok[g] = cred_ok_r;

      // A staged request is served this cycle if there is a completion slot,
      // and either it hit, or it can win the AR channel with a credit in hand.
      // Everything here starts at a flop (rq_*), not at the cache.
      assign want_ar[g] = rq_v && !rq_hit && !cq_full[g] && cred_ok[g];
      assign hit_go[g]  = rq_v && rq_hit && !cq_full[g];
      assign miss_go[g] = ar_mine;

      // miss-ordering FIFO: slot index awaiting an R beat, in issue order.
      // It also carries the request's LANE, so the fill port can extract the
      // element at write time (see the completion queue comment above). The
      // wrapper's merge loop is serial per context, so the lane cannot be
      // recovered from anywhere else once the request has left stage 2.
      wire [CQ_AW-1:0]  missq_idx;
      wire [LANE_W-1:0] fill_lane_q;
      wire              missq_valid;
      bombyx_mrc_fifo #(.WIDTH(CQ_AW+CTX_W+TAG_W+LANE_W), .DEPTH(OUTSTANDING)) u_missq (
        .clk(ap_clk), .rst_n(ap_rst_n),
        .in_data({rq_tag, rq_lane, rq_ctx, cq_wp[CQ_AW-1:0]}),
        .in_valid(ar_mine), .in_ready(),
        .out_data({fill_tag_q, fill_lane_q, fill_ctx_q, missq_idx}),
        .out_valid(missq_valid), .out_ready(r_mine)
      );

      // ---- fill stage ----------------------------------------------------
      // The R beat used to write cq_fill_data in the same cycle it arrived, so
      // the path ran RDATA / missq (LUTRAM async read) -> the CQ_DEPTH write
      // address decode -> a 256-bit distributed-RAM write port. That was the
      // second-largest family of violations (1109 of 5000 paths ended at
      // cq_fill_data). Registering the beat first splits it in two.
      //
      // The done bit moves with the data, so a slot is never marked complete
      // before its payload has landed. Only one R beat can arrive per cycle on
      // the shared channel, so this one-deep stage cannot overflow, and RREADY
      // stays tied high.
      reg                 f_v;
      reg [BUS_W-1:0]     f_data;
      reg [CQ_AW-1:0]     f_idx;
      reg                 f_cache_en;
      reg [CTX_W-1:0]     f_ctx;
      reg [TAG_W-1:0]     f_tag;
      reg [LANE_W-1:0]    f_lane;
      always @(posedge ap_clk) begin
        if (!ap_rst_n) begin
          f_v <= 1'b0; f_cache_en <= 1'b0;
        end else begin
          f_v        <= r_mine && missq_valid;
          f_cache_en <= r_mine;
        end
        f_data <= m_axi_gmem_RDATA;
        f_idx  <= missq_idx;
        f_ctx  <= fill_ctx_q;
        f_tag  <= fill_tag_q;
        f_lane <= fill_lane_q;
      end
      // f_data stays the full line: the cache stores lines, so only the CQ
      // payload is narrowed.
      // The cache fill rides the same stage. Landing a line a cycle later can
      // only turn a would-be hit into a redundant fetch, never a wrong result.
      assign fill_en   = f_cache_en;
      assign fill_ctx  = f_ctx;
      assign fill_tag  = f_tag;
      assign fill_data = f_data;

      // Pointers and the done bits: control only, no wide payload.
      // hit_go and ar_mine are mutually exclusive (want_ar requires !c_hit), so
      // a slot is allocated as exactly one of hit or miss.
      integer q;
      always @(posedge ap_clk) begin
        if (!ap_rst_n) begin
          cq_wp <= 0; cq_rp <= 0;
          for (q = 0; q < CQ_DEPTH; q = q + 1) cq_done[q] <= 1'b0;
        end else begin
          if (hit_go[g] || ar_mine) begin
            cq_done[cq_wp[CQ_AW-1:0]] <= hit_go[g];
            cq_wp <= cq_wp + 1'b1;
          end
          if (f_v) cq_done[f_idx] <= 1'b1;
          if (out_fire[g]) cq_rp <= cq_rp + 1'b1;
        end
      end

      // Element extraction, one per write port. Both operands are flops, so
      // these are the only combinational logic between a flop and the array's
      // write-data pins.
      wire [BUS_W-1:0] hit_shifted  = rq_data >> {rq_lane, 3'b000};
      wire [BUS_W-1:0] fill_shifted = f_data  >> {f_lane,  3'b000};

      // Write port A: allocate. Needs no reset -- a slot is only ever read
      // after cq_done marks it, and cq_done is only set by a write through here
      // or through port B below.
      always @(posedge ap_clk) begin
        if (hit_go[g] || ar_mine) begin
          cq_is_hit   [cq_wp[CQ_AW-1:0]] <= hit_go[g];
          cq_hit_data [cq_wp[CQ_AW-1:0]] <= hit_shifted[ELW-1:0];
          cq_cont     [cq_wp[CQ_AW-1:0]] <= rq_cont;
        end
      end

      // Write port B: miss fill, from the registered fill stage.
      always @(posedge ap_clk) begin
        if (f_v) cq_fill_data[f_idx] <= fill_shifted[ELW-1:0];
      end

      always @(posedge ap_clk) begin
        if (!ap_rst_n) begin
          credits   <= OUTSTANDING;
          cred_ok_r <= 1'b1;
        end else begin
          credits   <= credits_next;
          cred_ok_r <= (credits_next != {CW{1'b0}});
        end
      end

      // ---- emit ---------------------------------------------------------
      // The element is already extracted, so the emit path is just the two
      // narrow head muxes and the is_hit select -- no barrel shifter.
      wire [ELW-1:0] head_elem = cq_is_hit[cq_rp[CQ_AW-1:0]]
                                   ? cq_hit_data [cq_rp[CQ_AW-1:0]]
                                   : cq_fill_data[cq_rp[CQ_AW-1:0]];

      reg [ARG_W-1:0] pkt;
      always @* begin
        pkt = {ARG_W{1'b0}};
        pkt[PKT_ADDR_LSB +: CONT_W] = cq_cont[cq_rp[CQ_AW-1:0]];
        pkt[PKT_DATA_LSB +: ELW]    = head_elem;
      end

      // Output skid. argOut and argDataOut are one logical channel split in two
      // (same valid, and out_fire already required both readies), so a single
      // buffer carries the pair. This registers the emit valid -- which came
      // straight off cq_rp/cq_done through the CQ_DEPTH:1 head mux and the byte
      // barrel shifter -- and stops the downstream PE's TREADY from reaching
      // back into cq_rp.
      wire [63:0]      head_cont = cq_cont[cq_rp[CQ_AW-1:0]];
      wire             e_ready;
      wire [ARG_W+64-1:0] e_out;

      bombyx_mrc_skid #(.WIDTH(ARG_W + 64)) u_out_skid (
        .clk(ap_clk), .rst_n(ap_rst_n),
        .i_data({pkt, head_cont}),
        .i_valid(cq_head_done[g]), .i_ready(e_ready),
        .o_data(e_out),
        .o_valid(argDataOut_TVALID[g]),
        .o_ready(argDataOut_TREADY[g] && argOut_TREADY[g])
      );
      assign argOut_TDATA[g*64 +: 64]           = e_out[63:0];
      assign argDataOut_TDATA[g*ARG_W +: ARG_W] = e_out[ARG_W+64-1:64];
      assign argOut_TVALID[g] = argDataOut_TVALID[g];
      assign out_fire[g] = cq_head_done[g] && e_ready;
    end
  endgenerate

  // ---- constant AXI fields -----------------------------------------------
  assign m_axi_gmem_ARLEN    = 8'd0;
  assign m_axi_gmem_ARBURST  = 2'b01;
  assign m_axi_gmem_ARLOCK   = 2'b00;
  assign m_axi_gmem_ARCACHE  = 4'b0011;
  assign m_axi_gmem_ARPROT   = 3'b000;
  assign m_axi_gmem_ARQOS    = 4'b0000;
  assign m_axi_gmem_ARREGION = 4'b0000;
  assign m_axi_gmem_ARUSER   = 1'b0;
  assign m_axi_gmem_AWVALID  = 1'b0;
  assign m_axi_gmem_AWADDR   = {ADDR_W{1'b0}};
  assign m_axi_gmem_AWID     = {ID_W{1'b0}};
  assign m_axi_gmem_AWLEN    = 8'd0;
  assign m_axi_gmem_AWSIZE   = 3'd0;
  assign m_axi_gmem_AWBURST  = 2'b01;
  assign m_axi_gmem_AWLOCK   = 2'b00;
  assign m_axi_gmem_AWCACHE  = 4'b0011;
  assign m_axi_gmem_AWPROT   = 3'b000;
  assign m_axi_gmem_AWQOS    = 4'b0000;
  assign m_axi_gmem_AWREGION = 4'b0000;
  assign m_axi_gmem_AWUSER   = 1'b0;
  assign m_axi_gmem_WVALID   = 1'b0;
  assign m_axi_gmem_WDATA    = {BUS_W{1'b0}};
  assign m_axi_gmem_WSTRB    = {(BUS_W/8){1'b0}};
  assign m_axi_gmem_WLAST    = 1'b0;
  assign m_axi_gmem_WID      = {ID_W{1'b0}};
  assign m_axi_gmem_WUSER    = 1'b0;
  assign m_axi_gmem_BREADY   = 1'b1;

endmodule
