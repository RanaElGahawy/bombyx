// ===========================================================================
// bombyx_mem_reader_shared — N memReader streams on ONE AXI master.
//
// Why this exists
// ---------------
// A `#pragma BOMBYX OVERLAP` wrapper currently exposes one m_axi master per
// collapsed read site (memrd0..4 here, plus the applyFn root port). Each master
// costs a SmartConnect slave port, an AXI ID namespace, and its own share of
// the interconnect. At 8 PEs that is 48 masters for the memReaders alone, which
// is what forces the 16 aggregating SmartConnects.
//
// This module serves all N read sites from a single master, using ARID to
// distinguish them. Downstream that is one slave port instead of N, so the
// interconnect shrinks by ~N x: smaller circuit, more PEs per device, and a
// shorter critical path through the aggregation tree.
//
// Ordering and why RREADY can stay tied high
// ------------------------------------------
// Each stream issues on its own AXI ID, so AXI requires that stream's responses
// to return in order even though the fabric may interleave across IDs — a plain
// FIFO per stream therefore recovers the continuation tag, with no reorder
// buffer and no reply tagging.
//
// Every stream's credit count equals its own reply-FIFO depth, so an accepted
// AR always has a landing slot *in the FIFO its reply will be routed to*. That
// holds no matter which stream's reply arrives next, so RREADY is unconditional
// and the shared R channel is never back-pressured. Without that property one
// slow stream would head-of-line block every other stream on the shared port,
// which is exactly the pathology this module exists to avoid.
//
// Mixed element widths
// --------------------
// The read sites do not agree on element width (32 and 64 bits here), so the
// shared bus is the widest and narrower streams issue narrow reads (ARSIZE <
// bus width). AXI returns a narrow read on the byte lanes selected by the
// address, so the low address bits are carried alongside the continuation tag
// and used to pick the element out of the bus word on the way out.
//
// Per-stream widths arrive as packed parameter vectors because Verilog-2001 has
// no array parameters: field i of each vector describes stream i.
// ===========================================================================

module bombyx_msr_fifo #(
  parameter WIDTH = 64,
  parameter DEPTH = 64        // power of two
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
  (* ram_style = "block" *)
  reg [WIDTH-1:0] mem [0:DEPTH-1];
  reg [AW:0] wptr, rptr;
  wire empty = (wptr == rptr);
  wire full  = (wptr[AW-1:0] == rptr[AW-1:0]) && (wptr[AW] != rptr[AW]);
  assign in_ready = !full;
  wire push = in_valid && in_ready;

  reg [WIDTH-1:0] outreg;
  reg             outreg_v;
  wire pop = !empty && (!outreg_v || out_ready);
  assign out_valid = outreg_v;
  assign out_data  = outreg;

  always @(posedge clk) begin
    if (push) mem[wptr[AW-1:0]] <= in_data;
    if (pop)  outreg <= mem[rptr[AW-1:0]];
  end

  always @(posedge clk) begin
    if (!rst_n) begin
      wptr <= 0; rptr <= 0; outreg_v <= 1'b0;
    end else begin
      if (push) wptr <= wptr + 1'b1;
      if (pop)  rptr <= rptr + 1'b1;
      if (pop)            outreg_v <= 1'b1;
      else if (out_ready) outreg_v <= 1'b0;
    end
  end
endmodule


module bombyx_mem_reader_shared #(
  parameter N               = 5,    // number of read-site streams
  parameter TASK_W          = 256,  // taskIn closure width (per stream)
  parameter ARG_W           = 256,  // argDataOut packet width (per stream)
  parameter ADDR_W          = 64,
  parameter BUS_W           = 64,   // shared m_axi data width == max element width
  parameter ID_W            = 3,    // must be >= $clog2(N)
  // Packed per-stream descriptors, 8 bits per field, stream i at [8*i +: 8].
  parameter [8*N-1:0] ELEM_LOG2_VEC = {8'd2, 8'd2, 8'd3, 8'd3, 8'd2},
  parameter [8*N-1:0] ELEM_W_VEC    = {8'd32, 8'd32, 8'd64, 8'd64, 8'd32},
  parameter [8*N-1:0] IDX_W_VEC     = {8'd62, 8'd62, 8'd61, 8'd61, 8'd62},
  // taskIn field offsets (identical across every generated memReader)
  parameter CONT_LSB        = 0,
  parameter CONT_W          = 64,
  parameter BASE_LSB        = 64,
  parameter IDX_LSB         = 128,
  // argDataOut field offsets
  parameter PKT_ADDR_LSB    = 0,    // echoes taskIn._cont
  parameter PKT_DATA_LSB    = 64,
  // Outstanding reads per stream == that stream's reply-buffer depth. Power of two.
  parameter OUTSTANDING     = 64,
  // 0: one AXI ID per stream, replies routed by RID -- lets the fabric return
  //    streams out of order relative to each other.
  // 1: every stream issues on ID 0. Responses on one ID are globally in order,
  //    so a single order FIFO recording the issuing stream routes them instead.
  //    Costs cross-stream head-of-line blocking, but presents the interconnect
  //    with a single-threaded master, which some SmartConnect configurations
  //    handle far better than a multi-ID one.
  parameter SINGLE_ID       = 0,
  // Depth of that order FIFO: must cover every stream's credits at once.
  parameter ORDER_DEPTH     = 512,
  // 0: every read is a full bus-width transfer (ARSIZE = log2(BUS_W/8)) at a
  //    bus-aligned address; a narrower stream takes its element out of the
  //    returned word by byte offset, which the reply path already does.
  // 1: narrower streams issue narrow reads (ARSIZE < bus width).
  //    Narrow transfers force SmartConnect down a much slower path, so 0 is the
  //    default and lets SUPPORTS_NARROW_BURST be declared 0 on the port.
  parameter NARROW_READS    = 0
) (
  input                       ap_clk,
  input                       ap_rst_n,

  // ---- N request streams, flattened (stream i occupies field i) -----------
  input  [N*TASK_W-1:0]       taskIn_TDATA,
  input  [N-1:0]              taskIn_TVALID,
  output [N-1:0]              taskIn_TREADY,

  output [N*64-1:0]           argOut_TDATA,
  output [N-1:0]              argOut_TVALID,
  input  [N-1:0]              argOut_TREADY,

  output [N*ARG_W-1:0]        argDataOut_TDATA,
  output [N-1:0]              argDataOut_TVALID,
  input  [N-1:0]              argDataOut_TREADY,

  // ---- one shared m_axi (read-only; write channel present but tied idle) --
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

  // ---- observability -----------------------------------------------------
  output [31:0]               dbg_ar_grants,
  output [31:0]               dbg_no_credit_stalls
);

  localparam CW       = $clog2(OUTSTANDING+1);
  localparam SEL_W    = (N > 1) ? $clog2(N) : 1;
  localparam LANE_W   = (BUS_W > 8) ? $clog2(BUS_W/8) : 1; // byte-offset bits kept
  localparam BUS_LOG2 = $clog2(BUS_W/8);                   // ARSIZE for a full beat

  // ---- per-stream request decode -----------------------------------------
  wire [ADDR_W-1:0] s_addr   [0:N-1];
  wire [CONT_W-1:0] s_cont   [0:N-1];
  wire [2:0]        s_arsize [0:N-1];
  wire [N-1:0]      s_want;              // has a task AND a credit
  reg  [CW-1:0]     credits  [0:N-1];

  genvar g;
  generate
    for (g = 0; g < N; g = g + 1) begin : gen_req
      localparam integer EL2 = ELEM_LOG2_VEC[8*g +: 8];
      localparam integer IXW = IDX_W_VEC[8*g +: 8];

      wire [TASK_W-1:0] t = taskIn_TDATA[g*TASK_W +: TASK_W];
      wire [ADDR_W-1:0] base = t[BASE_LSB +: ADDR_W];
      wire [IXW-1:0]    idx  = t[IDX_LSB  +: IXW];

      // Byte address: the HLS reader divides by the element size because its
      // m_axi adapter is indexed in user-data words; driving AXI directly we
      // want bytes.
      assign s_addr[g]   = base + ({{(ADDR_W-IXW){1'b0}}, idx} << EL2);
      assign s_cont[g]   = t[CONT_LSB +: CONT_W];
      assign s_arsize[g] = EL2[2:0];
      assign s_want[g]   = taskIn_TVALID[g] && (credits[g] != {CW{1'b0}});
    end
  endgenerate

  // ---- round-robin arbiter over streams that can issue --------------------
  // Rotating priority keeps one hot stream from starving the others on the
  // shared port; that fairness is the whole point of sharing rather than
  // letting a single stream monopolise the master.
  reg  [SEL_W-1:0] rr_ptr;
  reg  [SEL_W-1:0] grant_idx;
  reg              grant_val;

  integer k;
  // One bit wider than an index: rr_ptr + k reaches 2N-2 before the wrap, which
  // would alias back into range if it were truncated to SEL_W bits.
  reg [SEL_W:0] cand;
  always @* begin
    grant_val = 1'b0;
    grant_idx = {SEL_W{1'b0}};
    // Descending, so the lowest k that wants to issue wins: that is rr_ptr
    // first, then rr_ptr+1, ... i.e. rotating priority.
    for (k = N-1; k >= 0; k = k - 1) begin
      cand = {1'b0, rr_ptr} + k[SEL_W:0];
      if (cand >= N[SEL_W:0]) cand = cand - N[SEL_W:0];
      if (s_want[cand[SEL_W-1:0]]) begin
        grant_val = 1'b1;
        grant_idx = cand[SEL_W-1:0];
      end
    end
  end

  wire ar_fire = m_axi_gmem_ARVALID && m_axi_gmem_ARREADY;

  always @(posedge ap_clk) begin
    if (!ap_rst_n) rr_ptr <= {SEL_W{1'b0}};
    else if (ar_fire) begin
      if (grant_idx == (N[SEL_W-1:0] - 1'b1)) rr_ptr <= {SEL_W{1'b0}};
      else                                    rr_ptr <= grant_idx + 1'b1;
    end
  end

  // Full-width mode aligns the address down to the bus word; the byte offset it
  // discards is exactly the `lane` already carried in the continuation FIFO, so
  // the element is still recovered on the way out.
  wire [ADDR_W-1:0] ar_addr_aligned =
      s_addr[grant_idx] & ~((({{(ADDR_W-1){1'b0}}, 1'b1}) << BUS_LOG2) - 1'b1);

  assign m_axi_gmem_ARVALID = grant_val;
  assign m_axi_gmem_ARADDR  = NARROW_READS ? s_addr[grant_idx] : ar_addr_aligned;
  assign m_axi_gmem_ARID    = SINGLE_ID ? {ID_W{1'b0}}
                                        : {{(ID_W-SEL_W){1'b0}}, grant_idx};
  assign m_axi_gmem_ARSIZE  = NARROW_READS ? s_arsize[grant_idx]
                                           : BUS_LOG2[2:0];

  generate
    for (g = 0; g < N; g = g + 1) begin : gen_ready
      assign taskIn_TREADY[g] =
          m_axi_gmem_ARREADY && grant_val && (grant_idx == g[SEL_W-1:0]);
    end
  endgenerate

  // ---- reply routing ------------------------------------------------------
  wire         r_fire  = m_axi_gmem_RVALID && m_axi_gmem_RREADY;
  assign m_axi_gmem_RREADY = 1'b1;   // a credit guaranteed the slot at AR time

  // In SINGLE_ID mode every request rides ID 0, so RID carries no routing
  // information; the order FIFO does instead. One ID means responses arrive in
  // issue order globally, so its head always names the stream the current beat
  // belongs to.
  wire [SEL_W-1:0] order_sel;
  wire             order_valid;
  generate
    if (SINGLE_ID) begin : gen_order_fifo
      bombyx_msr_fifo #(.WIDTH(SEL_W), .DEPTH(ORDER_DEPTH)) u_order_q (
        .clk(ap_clk), .rst_n(ap_rst_n),
        .in_data(grant_idx), .in_valid(ar_fire), .in_ready(),
        .out_data(order_sel), .out_valid(order_valid), .out_ready(r_fire)
      );
    end else begin : gen_no_order_fifo
      assign order_sel   = {SEL_W{1'b0}};
      assign order_valid = 1'b0;
    end
  endgenerate

  wire [SEL_W-1:0] r_sel = SINGLE_ID ? order_sel : m_axi_gmem_RID[SEL_W-1:0];

  wire [N-1:0] out_fire;
  wire [N-1:0] data_q_ready_w;

  generate
    for (g = 0; g < N; g = g + 1) begin : gen_stream
      localparam integer EL2  = ELEM_LOG2_VEC[8*g +: 8];
      localparam integer ELW  = ELEM_W_VEC[8*g +: 8];

      wire fire_i = ar_fire && (grant_idx == g[SEL_W-1:0]);

      // Continuation tag + the address bits needed to pick a narrow read out of
      // the bus word. Pushed at AR, popped when the element is emitted; one ID
      // per stream keeps this strictly in order.
      wire [CONT_W+LANE_W-1:0] cont_q_data;
      wire                     cont_q_valid, cont_q_ready;
      bombyx_msr_fifo #(.WIDTH(CONT_W+LANE_W), .DEPTH(OUTSTANDING)) u_cont_q (
        .clk(ap_clk), .rst_n(ap_rst_n),
        .in_data({s_addr[g][LANE_W-1:0], s_cont[g]}),
        .in_valid(fire_i), .in_ready(cont_q_ready),
        .out_data(cont_q_data), .out_valid(cont_q_valid), .out_ready(out_fire[g])
      );

      wire [BUS_W-1:0] data_q_data;
      wire             data_q_valid, data_q_ready;
      bombyx_msr_fifo #(.WIDTH(BUS_W), .DEPTH(OUTSTANDING)) u_data_q (
        .clk(ap_clk), .rst_n(ap_rst_n),
        .in_data(m_axi_gmem_RDATA),
        .in_valid(r_fire && (r_sel == g[SEL_W-1:0])), .in_ready(data_q_ready),
        .out_data(data_q_data), .out_valid(data_q_valid), .out_ready(out_fire[g])
      );
      assign data_q_ready_w[g] = data_q_ready;

      // Narrow reads land on the byte lanes the address selects, so shift the
      // bus word down by the stored byte offset before taking the element.
      wire [LANE_W-1:0] lane = cont_q_data[CONT_W +: LANE_W];
      wire [BUS_W-1:0]  shifted = data_q_data >> {lane, 3'b000};
      wire [ELW-1:0]    elem = shifted[ELW-1:0];

      reg [ARG_W-1:0] pkt;
      always @* begin
        pkt = {ARG_W{1'b0}};
        pkt[PKT_ADDR_LSB +: CONT_W] = cont_q_data[CONT_W-1:0];
        pkt[PKT_DATA_LSB +: ELW]    = elem;
      end

      assign argDataOut_TDATA[g*ARG_W +: ARG_W] = pkt;
      assign argDataOut_TVALID[g] = data_q_valid && cont_q_valid;
      assign argOut_TDATA[g*64 +: 64] = cont_q_data[CONT_W-1:0];
      assign argOut_TVALID[g] = data_q_valid && cont_q_valid;

      assign out_fire[g] = argDataOut_TVALID[g] && argDataOut_TREADY[g]
                                               && argOut_TREADY[g];

      always @(posedge ap_clk) begin
        if (!ap_rst_n) credits[g] <= OUTSTANDING;
        else credits[g] <= credits[g] - (fire_i ? 1'b1 : 1'b0)
                                      + (out_fire[g] ? 1'b1 : 1'b0);
      end
    end
  endgenerate

  // ---- observability ------------------------------------------------------
  reg [31:0] ar_grants, no_credit_stalls;
  always @(posedge ap_clk) begin
    if (!ap_rst_n) begin
      ar_grants <= 32'd0; no_credit_stalls <= 32'd0;
    end else begin
      if (ar_fire) ar_grants <= ar_grants + 32'd1;
      // A stream had work but no credit and nothing else could issue either.
      if (!grant_val && (|taskIn_TVALID)) no_credit_stalls <= no_credit_stalls + 32'd1;
    end
  end
  assign dbg_ar_grants        = ar_grants;
  assign dbg_no_credit_stalls = no_credit_stalls;

  // ---- constant AXI fields ------------------------------------------------
  assign m_axi_gmem_ARLEN    = 8'd0;              // one beat per task
  assign m_axi_gmem_ARBURST  = 2'b01;             // INCR
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

  // synthesis translate_off
  always @(posedge ap_clk) if (ap_rst_n) begin
    if (r_fire && !data_q_ready_w[r_sel])
      $display("%t bombyx_mem_reader_shared: reply FIFO overflow on stream %0d (credit bug)",
               $time, r_sel);
    if (r_fire && (r_sel >= N))
      $display("%t bombyx_mem_reader_shared: reply with out-of-range RID %0d", $time, r_sel);
    // In SINGLE_ID mode routing comes from the order FIFO, so a beat arriving
    // before its head is registered would be misrouted. Read latency is many
    // cycles so this cannot happen in practice -- but it must not pass silently.
    if (SINGLE_ID && r_fire && !order_valid)
      $display("%t bombyx_mem_reader_shared: R beat with empty order FIFO", $time);
    if (m_axi_gmem_RVALID && m_axi_gmem_RRESP != 2'b00)
      $display("%t bombyx_mem_reader_shared: RRESP=%b", $time, m_axi_gmem_RRESP);
  end
  // synthesis translate_on

endmodule
