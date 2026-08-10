#!/bin/bash
# Buffer-depth sweep for applyFn_overlap_wrapper.
#
# Overrides the wrapper's top-level parameters with Verilator -G, so no
# regeneration is needed per point, and reports steady-state throughput in
# contributions/PE/cycle plus a correctness verdict for each. A configuration
# that is faster but not bit-exact is not a configuration.
#
#   ./sweep_buffers.sh [vertices] [mem-latency]
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
V="${1:-512}"
LAT="${2:-32}"

run() { # <label> <-G args...>
  local label="$1"; shift
  GPARAMS="$*" "$HERE/build.sh" > /dev/null
  local out
  out=$("$HERE/tb_pagerank_overlap" --vertices "$V" --mem-latency "$LAT" \
          --max-cycles 2000000 2>&1)
  local ss e2e verdict
  ss=$(echo "$out" | sed -n 's/.*PERF steady-state *: *\([0-9.]*\).*/\1/p')
  e2e=$(echo "$out" | sed -n 's/.*PERF end-to-end *: *\([0-9.]*\).*/\1/p')
  verdict=$(echo "$out" | sed -n 's/.*VERDICT *: *//p')
  printf '%-52s steady=%-8s end2end=%-8s %s\n' "$label" "$ss" "$e2e" "$verdict"
}

echo "=== V=$V, memory latency=$LAT cycles, 1 wrapper PE ==="
echo "--- EXIT_DEPTH (queue in front of the exit PE) ---"
for E in 2 8 32 128 512; do
  run "EXIT_DEPTH=$E (others default)" -GEXIT_DEPTH=$E
done
echo "--- STATE_DEPTH (state FIFOs, loopback, contexts, cache entries) ---"
for S in 16 32 64 128 256; do
  run "STATE_DEPTH=$S" -GSTATE_DEPTH=$S -GEXIT_DEPTH=128
done
echo "--- REPLY_DEPTH (per-round reply FIFO) ---"
for R in 8 16 32 64 128; do
  run "REPLY_DEPTH=$R" -GREPLY_DEPTH=$R -GEXIT_DEPTH=128
done
echo "--- MEM_SHARED_OUTSTANDING (AXI credits per read stream) ---"
for M in 8 16 32 64 128; do
  run "MEM_SHARED_OUTSTANDING=$M" -GMEM_SHARED_OUTSTANDING=$M -GEXIT_DEPTH=128
done
