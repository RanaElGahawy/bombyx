#!/bin/bash

set -e  # stop on first error (remove if you want all tests to continue)

CLANG="/opt/opencilk/bin/clang++"
FLAGS="-fopencilk -Wno-backend-plugin -Wno-parentheses-equality"
BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "=== Running OpenCilk tests ==="

run_test () {
  NAME=$1
  BASE_NAME="${NAME%_cilk1}"

  echo ">> $NAME"

  $CLANG $NAME.cpp $FLAGS -o $NAME
  ./$NAME > ${NAME}.txt

  diff "$BASE/ex/${NAME}.txt" "$BASE/im/${BASE_NAME}.txt" || echo "❌ $NAME output differs"
}

# Tests with expected outputs
run_test "nqueens_cilk1"
run_test "fib_cilk1"
run_test "listing_7_cilk1"
run_test "listing_8_cilk1"
run_test "listing_9_cilk1"
run_test "listing_10_cilk1"
run_test "listing_11_cilk1"
run_test "listing_13_cilk1"
run_test "test_0_cilk1"
run_test "test_1_cilk1"
run_test "test_2_cilk1"
run_test "test_3_cilk1"
run_test "test_4_cilk1"
run_test "test_5_cilk1"
run_test "test_6_cilk1"
run_test "test_7_cilk1"
run_test "test_8_cilk1"

echo "✅ Done"