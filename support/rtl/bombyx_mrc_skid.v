// ===========================================================================
// bombyx_mrc_skid -- generic one-beat skid buffer.
//
// Used by the OVERLAP wrapper top for its inbound task stream and its
// outbound result streams, INDEPENDENTLY of which shared-reader flavour the
// wrapper instantiates. It therefore lives in its own file and is emitted
// unconditionally; it used to sit inside bombyx_mem_reader_cached.v, which
// meant any design whose readers were all uncached (no ctx cache) emitted
// instantiations of a module that was never defined, and failed elaboration.
// ===========================================================================
// ===========================================================================
// bombyx_mrc_skid — two-entry skid buffer, registered in BOTH directions.
//
// i_ready is a flop output (~skid_v), not a function of o_ready, and o_valid is
// a flop. So neither the forward valid/data path nor the backward ready path
// carries combinational logic through the buffer.
//
// This is what breaks the handshake chains that dominated timing: the memory
// reader's taskIn_TREADY used to be
//
//     taskIn_TREADY[g] = rq_ready = !rq_v || hit_go[g] || ar_mine
//
// i.e. one stream's ready depended combinationally on every stream's CQ
// pointers and credit counters (via the shared AR arbiter) and then drove a
// 1024-bit register slice's clock enable in the upstream HLS PE -- measured at
// 12-13 logic levels and 4.4-4.8 ns, against a 3.333 ns budget.
//
// Costs one cycle of latency per interface; sustains one transfer per cycle,
// so no throughput is lost.
// ===========================================================================
module bombyx_mrc_skid #(
  parameter WIDTH = 32
) (
  input                  clk,
  input                  rst_n,
  input  [WIDTH-1:0]     i_data,
  input                  i_valid,
  output                 i_ready,
  output [WIDTH-1:0]     o_data,
  output                 o_valid,
  input                  o_ready
);
  reg [WIDTH-1:0] data_r, skid_d;
  reg             valid_r, skid_v;

  assign i_ready = ~skid_v;   // flop output: no path back from o_ready
  assign o_valid = valid_r;   // flop output
  assign o_data  = data_r;

  wire i_fire = i_valid & i_ready;
  wire o_fire = valid_r & o_ready;

  always @(posedge clk) begin
    if (!rst_n) begin
      valid_r <= 1'b0;
      skid_v  <= 1'b0;
    end else if (skid_v) begin
      // holding an overflow beat; i_ready is low so nothing new arrives
      if (o_fire) begin
        data_r  <= skid_d;
        valid_r <= 1'b1;
        skid_v  <= 1'b0;
      end
    end else begin
      if (i_fire && valid_r && !o_fire) begin
        skid_d <= i_data;      // output busy and not draining -> skid
        skid_v <= 1'b1;
      end else if (i_fire) begin
        data_r  <= i_data;     // output free or draining this cycle
        valid_r <= 1'b1;
      end else if (o_fire) begin
        valid_r <= 1'b0;
      end
    end
  end
endmodule
