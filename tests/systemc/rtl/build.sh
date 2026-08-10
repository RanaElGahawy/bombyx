#!/bin/bash
# Verilate the generated wrapper + Vitis-synthesised PEs, then build the SystemC TB.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
D="${1:-/beta/shahawy/compiler/temp_outputs/triangleDAEOptimalCompact_HardCilk}"
SYSTEMC="${SYSTEMC_HOME:-/usr/local}"

verilator --cc --timing -Wno-fatal -Wno-lint --top-module applyFn_overlap_wrapper \
  -O2 --build -j 8 --prefix Vwrap --trace --trace-depth 4 \
  "$D/applyFn_overlap_wrapper.v" "$D"/vitis_hls_output/applyFn_overlap_wrapper/*.v \
  > "$HERE/verilate.log" 2>&1

g++ -O1 -std=c++20 -w -o "$HERE/tb_wrapper" "$HERE/tb_wrapper.cpp" \
  -I"$HERE/obj_dir" -I/usr/local/share/verilator/include \
  -I/usr/local/share/verilator/include/vltstd -I"$SYSTEMC/include" \
  /usr/local/share/verilator/include/verilated.cpp \
  /usr/local/share/verilator/include/verilated_threads.cpp \
  /usr/local/share/verilator/include/verilated_timing.cpp \
  /usr/local/share/verilator/include/verilated_vcd_c.cpp \
  "$HERE/obj_dir/libVwrap.a" \
  -L"$SYSTEMC/lib" -lsystemc -Wl,-rpath,"$SYSTEMC/lib"
echo "built $HERE/tb_wrapper"
