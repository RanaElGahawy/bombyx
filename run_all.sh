#!/bin/bash

set -e  # stop on first error

# Resolve project root dynamically
BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BOMBYX="$BASE/build/bin/bombyx-cc"

echo "Running all Bombyx tests..."

run_test() {
  NAME="$1"
  IN="$2"
  OUT="$3"

  echo ">> $NAME"
  "$BOMBYX" "$BASE/$IN" "$BASE/$OUT"
}

# Main tests
run_test "nqueens"   "tests/im/nqueens.c"      "tests/ex/nqueens.cpp"
run_test "fib"       "tests/im/fib.c"          "tests/ex/fib.cpp"
run_test "listing_7" "tests/im/listing_7.cpp"  "tests/ex/listing_7.cpp"
run_test "listing_8" "tests/im/listing_8.cpp"  "tests/ex/listing_8.cpp"
run_test "listing_9" "tests/im/listing_9.cpp"  "tests/ex/listing_9.cpp"
run_test "listing_10" "tests/im/listing_10.cpp" "tests/ex/listing_10.cpp"
run_test "listing_11" "tests/im/listing_11.cpp" "tests/ex/listing_11.cpp"
run_test "listing_13" "tests/im/listing_13.cpp" "tests/ex/listing_13.cpp"

run_test "test0" "tests/im/test_0.cpp" "tests/ex/test_0.cpp"
run_test "test1" "tests/im/test_1.cpp" "tests/ex/test_1.cpp"
run_test "test2" "tests/im/test_2.cpp" "tests/ex/test_2.cpp"
run_test "test3" "tests/im/test_3.cpp" "tests/ex/test_3.cpp"
run_test "test4" "tests/im/test_4.cpp" "tests/ex/test_4.cpp"
run_test "test5" "tests/im/test_5.cpp" "tests/ex/test_5.cpp"
run_test "test6" "tests/im/test_6.cpp" "tests/ex/test_6.cpp"
run_test "test7" "tests/im/test_7.cpp" "tests/ex/test_7.cpp"
run_test "test8" "tests/im/test_8.cpp" "tests/ex/test_8.cpp"

# Bombyx OpenCilk examples
run_test "Pagerank" \
  "Bombyx_OpenCilk_Examples/pageRank/main.cpp" \
  "Bombyx_OpenCilk_Examples/pageRank/output/main.cpp"

run_test "Randomwalk" \
  "Bombyx_OpenCilk_Examples/randomWalk/main.cpp" \
  "Bombyx_OpenCilk_Examples/randomWalk/output/main.cpp"

run_test "TriangleCount" \
  "Bombyx_OpenCilk_Examples/triangleCount/main.cpp" \
  "Bombyx_OpenCilk_Examples/triangleCount/output/main.cpp"

run_test "BarnesHut" \
  "Bombyx_OpenCilk_Examples/barnes-hut/main.cpp" \
  "Bombyx_OpenCilk_Examples/barnes-hut/output/main.cpp"

# Cilk5 tests
run_test "cilk5/nqueens" \
  "tests/cilk5/im/nqueens.c" \
  "tests/cilk5/ex/nqueens.cpp"

run_test "cilk5/fib" \
  "tests/cilk5/im/fib.cpp" \
  "tests/cilk5/ex/fib.cpp"

run_test "cilk5/cilksort" \
  "tests/cilk5/im/cilksort.cpp" \
  "tests/cilk5/ex/cilksort.cpp"

run_test "cilk5/qsort" \
  "tests/cilk5/im/qsort.cpp" \
  "tests/cilk5/ex/qsort.cpp"

run_test "cilk5/matmul" \
  "tests/cilk5/im/matmul.c" \
  "tests/cilk5/ex/matmul.cpp"

run_test "cilk5/rectmul" \
  "tests/cilk5/im/rectmul.c" \
  "tests/cilk5/ex/rectmul.cpp"

run_test "cilk5/rectmulred" \
  "tests/cilk5/im/rectmulred.c" \
  "tests/cilk5/ex/rectmulred.cpp"

run_test "cilk5/lu" \
  "tests/cilk5/im/lu.c" \
  "tests/cilk5/ex/lu.cpp"

run_test "cilk5/heat" \
  "tests/cilk5/im/heat.c" \
  "tests/cilk5/ex/heat.cpp"

run_test "cilk5/strassen" \
  "tests/cilk5/im/strassen.c" \
  "tests/cilk5/ex/strassen.cpp"

run_test "cilk5/fft" \
  "tests/cilk5/im/fft.cpp" \
  "tests/cilk5/ex/fft.cpp"

run_test "cilk5/cholesky" \
  "tests/cilk5/im/cholesky.cpp" \
  "tests/cilk5/ex/cholesky.cpp"

echo "✅ Done!"