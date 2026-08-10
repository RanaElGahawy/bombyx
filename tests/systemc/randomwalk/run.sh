#!/bin/bash
# Regenerate randomWalk_overlap with the CURRENT bombyx-cc, synthesise its PEs
# if they are stale, rebuild the Verilator testbench and sweep it.
#
# Everything is regenerated into a scratch tree — never read from a checked-in
# copy — so a stale artifact can never make a broken compiler look fixed.
# Vitis HLS is only re-run when the generated PE C++ differs from what the
# cached netlists were built from, so the common "I edited the wrapper
# generator" loop stays at ~1 minute; a PE-side change costs one ~15 min
# synthesis and is then cached again.
#
#   ./run.sh              # incremental
#   FORCE_HLS=1 ./run.sh  # re-synthesise unconditionally
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOMBYX="${BOMBYX:-$HERE/../../../build/bin/bombyx-cc}"
SRC="${SRC:-/beta/shahawy/ASPLOS27/opencilk_kernels/randomWalk/randomWalk_overlap.cpp}"
GEN="${GEN:-$HERE/gen}"
SYN="${SYN:-$HERE/syn}"          # cached synthesised PE tree
HLS_SETTINGS="${HLS_SETTINGS:-/alpha/tools/Xilinx/Vitis_HLS/2024.1/settings64.sh}"

rm -rf "$GEN"; mkdir -p "$GEN"
"$BOMBYX" -t hardcilk -d "$GEN" "$SRC" > "$GEN/gen.log" 2>&1 || { cat "$GEN/gen.log"; exit 1; }
G="$GEN/randomWalk_overlap_HardCilk"

# Re-synthesise only when the PE C++ actually changed. The wrapper is NOT part
# of that test: it is re-emitted every run and copied over the cached tree, so
# a wrapper-generator edit never triggers HLS.
need_hls=0
[ -n "$FORCE_HLS" ] && need_hls=1
[ -d "$SYN/vitis_hls_output/applyFn_overlap_wrapper" ] || need_hls=1
diff -q "$G/randomWalk_overlap.cpp" "$SYN/randomWalk_overlap.cpp" >/dev/null 2>&1 || need_hls=1

if [ $need_hls -eq 1 ]; then
  echo "PE C++ changed (or no cache) — running Vitis HLS, this takes ~15 min..."
  rm -rf "$SYN"; cp -r "$G" "$SYN"
  ( . "$HLS_SETTINGS" && cd "$SYN" && ./build_hls.sh -j8 ) > "$SYN/hls.log" 2>&1 \
    || { tail -30 "$SYN/hls.log"; exit 1; }
else
  echo "PE netlists up to date; reusing $SYN"
fi

# Always take the wrapper from THIS run's bombyx output, patched for the
# synthesised PEs' m_axi data/ID widths exactly as build_hls.sh does. A wrapper
# still carrying the placeholder widths hangs on its first read.
PES="$SYN/vitis_hls_output/applyFn_overlap_wrapper"
cp "$G/applyFn_overlap_wrapper.v" "$HERE/wrapper.v"
for PE_V in "$PES"/*.v; do
  K=$(basename "$PE_V" .v)
  if [ "$K" = applyFn_overlap_wrapper ]; then continue; fi
  W=$(grep -oE 'C_M_AXI_GMEM_DATA_WIDTH[[:space:]]*=[[:space:]]*[0-9]+' "$PE_V" | head -1 | grep -oE '[0-9]+$' || true)
  I=$(grep -oE 'C_M_AXI_GMEM_ID_WIDTH[[:space:]]*=[[:space:]]*[0-9]+' "$PE_V" | head -1 | grep -oE '[0-9]+$' || true)
  if [ -n "$W" ]; then
    sed -i -E "s/(parameter[[:space:]]+MEM_DATA_WIDTH_${K}[[:space:]]*=[[:space:]]*)[0-9]+/\\1${W}/" "$HERE/wrapper.v"
  fi
  if [ -n "$I" ]; then
    sed -i -E "s/(parameter[[:space:]]+MEM_ID_WIDTH_${K}[[:space:]]*=[[:space:]]*)[0-9]+/\\1${I}/" "$HERE/wrapper.v"
  fi
done

"$HERE/build.sh" "$HERE/wrapper.v" "$PES" > /dev/null

fail=0
for V in 8 16 24 64; do
  for SEED in 1 7 99; do
    for LAT in 4 24 61; do
      out=$("$HERE/tb_randomwalk_overlap" --vertices "$V" --seed "$SEED" \
              --mem-latency "$LAT" --max-cycles 900000 2>&1)
      v=$(echo "$out" | sed -n 's/.*VERDICT *: *//p')
      ex=$(echo "$out" | sed -n 's/.*walk bit-exact *: *//p')
      printf '  V=%-3s seed=%-3s memLat=%-3s -> %-5s %s\n' "$V" "$SEED" "$LAT" "$v" "$ex"
      [ "$v" = "PASS" ] || { fail=1; echo "$out"; }
    done
  done
done
[ $fail -eq 0 ] && echo "randomWalk_overlap: ALL PASS" || { echo "FAILED"; exit 1; }
