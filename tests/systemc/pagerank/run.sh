#!/bin/bash
# Regenerate pageRank_overlap's wrapper with the current bombyx-cc, rebuild the
# SystemC/Verilator testbench and sweep it. ~1 minute end to end.
#
# The PE netlists under $KDIR/vitis_hls_output are reused: bombyx's PE C++ for
# this kernel is unchanged by wrapper-side edits, so there is no need to re-run
# Vitis HLS to iterate on the wrapper generator.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOMBYX="$HERE/../../../build/bin/bombyx-cc"
SRC="${SRC:-/beta/shahawy/ASPLOS27/opencilk_kernels/pageRank/pageRank_overlap.cpp}"
KDIR="${KDIR:-/beta/shahawy/ASPLOS27/hardcilk_kernels_comp_out/pageRank_overlap_HardCilk}"

# Take the wrapper from the synthesised kernel tree, NOT a fresh bombyx run:
# build_hls.sh patches MEM_DATA_WIDTH_<pe> and MEM_ID_WIDTH_<pe> into that copy
# from the synthesised PEs, and a freshly generated wrapper still carries the
# placeholders (a 1-bit ARID on a PE that drives 2 bits hangs on its first read).
cp "$KDIR/vitis_hls_output/applyFn_overlap_wrapper/applyFn_overlap_wrapper.v" \
   "$HERE/wrapper.v"

"$HERE/build.sh" > /dev/null

fail=0
for V in 8 16 24 32 64; do
  for SEED in 1 7 99; do
    for LAT in 4 24 61; do
      out=$("$HERE/tb_pagerank_overlap" --vertices "$V" --seed "$SEED" \
              --mem-latency "$LAT" --max-cycles 400000 2>&1)
      v=$(echo "$out" | sed -n 's/.*VERDICT *: *//p')
      printf '  V=%-3s seed=%-3s memLat=%-3s -> %s\n' "$V" "$SEED" "$LAT" "$v"
      [ "$v" = "PASS" ] || { fail=1; echo "$out"; }
    done
  done
done
[ $fail -eq 0 ] && echo "pageRank_overlap wrapper: ALL PASS" || { echo "FAILED"; exit 1; }
