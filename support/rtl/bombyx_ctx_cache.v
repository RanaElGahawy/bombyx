// ===========================================================================
// bombyx_ctx_cache — one private line per resident context.
//
// Why not index by address
// ------------------------
// Collisions in this workload are not between addresses within one walk; they
// are between the ~46 concurrent merge loops, each walking an unrelated region.
// An address-indexed cache spends its associativity doing nothing but keeping
// those contexts apart: measured on the real trace, 64 entries/stream gives
// 48.7 % direct-mapped, 75.7 % 8-way, 80.9 % fully associative.
//
// Indexing by context instead makes conflicts impossible by construction. Each
// resident loop owns exactly one entry, evicts only itself, and does so exactly
// when it advances past the line — the moment that line is dead. Measured
// ceiling for that binding on the same trace: **88.2 %** of consecutive
// same-context requests hit the same line, a further 8.2 % hit the next one
// (so a next-line prefetch would reach ~96 %).
//
// Cost is one BRAM read and one comparator: no CAM, no way mux, no replacement
// policy, no priority encoder.
//
// Tags are kept full, so a context ID that is freed and reallocated needs no
// invalidation — the stale entry simply misses, because the new owner's line
// address cannot match. Only a truncated tag would require a valid-bit clear at
// allocation.
//
// Read-only data is assumed (neighbour lists are not written during the merge),
// so there is no coherence obligation.
// ===========================================================================

module bombyx_ctx_cache #(
  parameter NUM_CTX = 64,                 // entries == number of resident contexts
  parameter CTX_W   = 6,                  // $clog2(NUM_CTX)
  parameter LINE_W  = 256,                // line width in bits
  parameter TAG_W   = 32                  // line-address tag
) (
  input                    clk,
  input                    rst_n,

  // ---- lookup: combinational, same cycle as the request ------------------
  input  [CTX_W-1:0]       lu_ctx,
  input  [TAG_W-1:0]       lu_tag,
  output                   lu_hit,
  output [LINE_W-1:0]      lu_data,

  // ---- fill ---------------------------------------------------------------
  input                    fill_en,
  input  [CTX_W-1:0]       fill_ctx,
  input  [TAG_W-1:0]       fill_tag,
  input  [LINE_W-1:0]      fill_data
);

  reg [TAG_W-1:0]  tags  [0:NUM_CTX-1];
  reg              valid [0:NUM_CTX-1];
  reg [LINE_W-1:0] data  [0:NUM_CTX-1];

  integer i;
  initial begin
    for (i = 0; i < NUM_CTX; i = i + 1) begin
      valid[i] = 1'b0;
      tags[i]  = {TAG_W{1'b0}};
      data[i]  = {LINE_W{1'b0}};
    end
  end

  assign lu_hit  = valid[lu_ctx] && (tags[lu_ctx] == lu_tag);
  assign lu_data = data[lu_ctx];

  always @(posedge clk) begin
    if (!rst_n) begin
      for (i = 0; i < NUM_CTX; i = i + 1) valid[i] <= 1'b0;
    end else if (fill_en) begin
      valid[fill_ctx] <= 1'b1;
      tags [fill_ctx] <= fill_tag;
    end
  end

  always @(posedge clk) if (fill_en) data[fill_ctx] <= fill_data;

endmodule
