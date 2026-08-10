// ===========================================================================
// Behavioural stand-ins for the Xilinx Floating-Point Operator IP cores that
// Vitis HLS instantiates inside applyFn_exit1 and applyFn_reentry1_cont1.
//
// The synthesised PE sources ship only the HLS *wrapper* around each core
// (applyFn_exit1_fadd_...v) plus a .tcl that would regenerate the core itself,
// so the `_ip` module is missing from the RTL. These models supply it.
//
// Contract taken from the HLS wrapper (see applyFn_exit1_fadd_..._1.v):
//   * the wrapper registers din into din0_buf1/din1_buf1 (1 cycle) and drives
//     aclken = ce_r, so the core must contribute NUM_STAGE-1 cycles for the
//     wrapper's advertised NUM_STAGE latency to hold;
//   * aclken gates the whole pipeline, which is what the wrapper relies on
//     when HLS pulses `ce` only on the cycles the operator is live.
//
// The arithmetic itself goes through DPI-C so it is true IEEE-754 binary32 —
// SystemVerilog `shortreal` is widened to double by Verilator, which would
// silently produce results that differ from the software golden in the last
// bits and defeat the whole point of a bit-exact comparison.
// ===========================================================================

import "DPI-C" function int unsigned bombyx_fp_add(input int unsigned a, input int unsigned b);
import "DPI-C" function int unsigned bombyx_fp_sub(input int unsigned a, input int unsigned b);
import "DPI-C" function int unsigned bombyx_fp_mul(input int unsigned a, input int unsigned b);
import "DPI-C" function int unsigned bombyx_fp_div(input int unsigned a, input int unsigned b);
import "DPI-C" function int unsigned bombyx_fp_uitofp32(input int unsigned a);
import "DPI-C" function int unsigned bombyx_fp_uitofp64(input longint unsigned a);

// Generic 2-input core: LAT cycles of aclken-gated pipeline.
module bombyx_fp_bin2 #(parameter LAT = 6, parameter OP = 0) (
  input  wire        aclk,
  input  wire        aclken,
  input  wire        s_axis_a_tvalid,
  input  wire [31:0] s_axis_a_tdata,
  input  wire        s_axis_b_tvalid,
  input  wire [31:0] s_axis_b_tdata,
  output wire        m_axis_result_tvalid,
  output wire [31:0] m_axis_result_tdata
);
  reg [31:0] pipe [0:LAT-1];
  reg        vpipe [0:LAT-1];
  wire [31:0] res = (OP == 0) ? bombyx_fp_add(s_axis_a_tdata, s_axis_b_tdata)
                  : (OP == 1) ? bombyx_fp_sub(s_axis_a_tdata, s_axis_b_tdata)
                  : (OP == 2) ? bombyx_fp_mul(s_axis_a_tdata, s_axis_b_tdata)
                              : bombyx_fp_div(s_axis_a_tdata, s_axis_b_tdata);
  integer i;
  always @(posedge aclk) if (aclken) begin
    pipe[0]  <= res;
    vpipe[0] <= s_axis_a_tvalid & s_axis_b_tvalid;
    for (i = 1; i < LAT; i = i + 1) begin
      pipe[i]  <= pipe[i-1];
      vpipe[i] <= vpipe[i-1];
    end
  end
  assign m_axis_result_tdata  = pipe[LAT-1];
  assign m_axis_result_tvalid = vpipe[LAT-1];
endmodule

// Generic unsigned-integer -> float core.
module bombyx_fp_u2f #(parameter LAT = 4, parameter IN_W = 32) (
  input  wire            aclk,
  input  wire            aclken,
  input  wire            s_axis_a_tvalid,
  input  wire [IN_W-1:0] s_axis_a_tdata,
  output wire            m_axis_result_tvalid,
  output wire [31:0]     m_axis_result_tdata
);
  reg [31:0] pipe [0:LAT-1];
  reg        vpipe [0:LAT-1];
  wire [63:0] wide = {{(64-IN_W){1'b0}}, s_axis_a_tdata};
  wire [31:0] res  = (IN_W <= 32) ? bombyx_fp_uitofp32(wide[31:0])
                                  : bombyx_fp_uitofp64(wide);
  integer i;
  always @(posedge aclk) if (aclken) begin
    pipe[0]  <= res;
    vpipe[0] <= s_axis_a_tvalid;
    for (i = 1; i < LAT; i = i + 1) begin
      pipe[i]  <= pipe[i-1];
      vpipe[i] <= vpipe[i-1];
    end
  end
  assign m_axis_result_tdata  = pipe[LAT-1];
  assign m_axis_result_tvalid = vpipe[LAT-1];
endmodule

// ---- the eight cores the two PEs instantiate ------------------------------
// LAT = NUM_STAGE - 1 (the HLS wrapper owns the remaining input register).
// FPADJ lets a build shift every core's latency together, so the assumption can
// be checked against the design rather than trusted.
`ifndef FPADJ
 `define FPADJ 0
`endif

`define FP_BIN2(NAME, LATV, OPV)                                              \
module NAME (                                                                 \
  input wire aclk, input wire aclken,                                         \
  input wire s_axis_a_tvalid, input wire [31:0] s_axis_a_tdata,               \
  input wire s_axis_b_tvalid, input wire [31:0] s_axis_b_tdata,               \
  output wire m_axis_result_tvalid, output wire [31:0] m_axis_result_tdata);  \
  bombyx_fp_bin2 #(.LAT(LATV), .OP(OPV)) u (.*);                              \
endmodule

`FP_BIN2(applyFn_exit1_fadd_32ns_32ns_32_7_full_dsp_1_ip, 6+`FPADJ, 0)
`FP_BIN2(applyFn_exit1_fsub_32ns_32ns_32_7_full_dsp_1_ip, 6+`FPADJ, 1)
`FP_BIN2(applyFn_exit1_fmul_32ns_32ns_32_4_max_dsp_1_ip, 3+`FPADJ, 2)
`FP_BIN2(applyFn_exit1_fdiv_32ns_32ns_32_12_no_dsp_1_ip, 11+`FPADJ, 3)
`FP_BIN2(applyFn_reentry1_cont1_fadd_32ns_32ns_32_7_full_dsp_1_ip, 6+`FPADJ, 0)
`FP_BIN2(applyFn_reentry1_cont1_fdiv_32ns_32ns_32_12_no_dsp_1_ip, 11+`FPADJ, 3)

module applyFn_exit1_uitofp_32ns_32_5_no_dsp_1_ip (
  input wire aclk, input wire aclken,
  input wire s_axis_a_tvalid, input wire [31:0] s_axis_a_tdata,
  output wire m_axis_result_tvalid, output wire [31:0] m_axis_result_tdata);
  bombyx_fp_u2f #(.LAT(4+`FPADJ), .IN_W(32)) u (.*);
endmodule

module applyFn_reentry1_cont1_uitofp_64ns_32_5_no_dsp_1_ip (
  input wire aclk, input wire aclken,
  input wire s_axis_a_tvalid, input wire [63:0] s_axis_a_tdata,
  output wire m_axis_result_tvalid, output wire [31:0] m_axis_result_tdata);
  bombyx_fp_u2f #(.LAT(4+`FPADJ), .IN_W(64)) u (.*);
endmodule
