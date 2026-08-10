#!/bin/bash
# Build and run the SystemC behavioural model of the OVERLAP wrapper.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE="$(cd "$HERE/../.." && pwd)"
ROOT="$(cd "$BASE/.." && pwd)"
SYSTEMC="${SYSTEMC_HOME:-/usr/local}"
GEN="$HERE/gen"

# Regenerate the PEs the model links against, so the model always reflects the
# current compiler rather than a stale copy.
rm -rf "$GEN"; mkdir -p "$GEN"
"$BASE/build/bin/bombyx-cc" -t hardcilk -d "$GEN" \
  "$ROOT/Bombyx_OpenCilk_Examples/triangleDAEFull/triangleDAEOptimalCompact.cpp" \
  > "$GEN/gen.log" 2>&1

D="$GEN/triangleDAEOptimalCompact_HardCilk"
g++ -O2 -std=c++20 -w \
  -I"$HERE" -I"$D" -I"$SYSTEMC/include" \
  -o "$HERE/overlap_model" "$HERE/overlap_model.cpp" \
  -L"$SYSTEMC/lib" -L"$SYSTEMC/lib-linux64" -lsystemc -Wl,-rpath,"$SYSTEMC/lib"
echo "built $HERE/overlap_model"
