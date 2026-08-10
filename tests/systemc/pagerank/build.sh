#!/bin/bash
# Verilate pageRank_overlap's applyFn_overlap_wrapper + the Vitis-synthesised
# PEs, then build the SystemC testbench.
#
#   ./build.sh [<wrapper.v>] [<vitis_hls_output/applyFn_overlap_wrapper dir>]
#
# The PE netlists never change while only the wrapper's codegen is edited, so
# iterating a compiler fix is: regenerate the wrapper, re-run this, re-run
# ./tb_pagerank_overlap — about 40 s, against ~40 min for an xclbin + hw_emu.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KDIR="${KDIR:-/beta/shahawy/ASPLOS27/hardcilk_kernels_comp_out/pageRank_overlap_HardCilk}"
WRAP="${1:-$HERE/wrapper.v}"
PES="${2:-$KDIR/vitis_hls_output/applyFn_overlap_wrapper}"
SYSTEMC="${SYSTEMC_HOME:-/usr/local}"

verilator --cc --timing -Wno-fatal -Wno-lint --top-module applyFn_overlap_wrapper \
  -O2 --converge-limit 500 --build -j 8 --prefix Vwrap --trace --trace-depth 6 --public-flat-rw +define+FPADJ=${FPADJ:--1} ${GPARAMS:-} \
  "$WRAP" "$HERE/fp_ip_models.sv" "$PES"/*.v \
  > "$HERE/verilate.log" 2>&1 || { tail -40 "$HERE/verilate.log"; exit 1; }

g++ -O1 -std=c++20 -w -o "$HERE/tb_pagerank_overlap" "$HERE/tb_pagerank_overlap.cpp" \
  -I"$HERE/obj_dir" -I/usr/local/share/verilator/include \
  -I/usr/local/share/verilator/include/vltstd -I"$SYSTEMC/include" \
  /usr/local/share/verilator/include/verilated.cpp \
  /usr/local/share/verilator/include/verilated_threads.cpp \
  /usr/local/share/verilator/include/verilated_timing.cpp \
  /usr/local/share/verilator/include/verilated_dpi.cpp \
  /usr/local/share/verilator/include/verilated_vcd_c.cpp \
  "$HERE/obj_dir/libVwrap.a" \
  -L"$SYSTEMC/lib" -lsystemc -Wl,-rpath,"$SYSTEMC/lib"
echo "built $HERE/tb_pagerank_overlap"
