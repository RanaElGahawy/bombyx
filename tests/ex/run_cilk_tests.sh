#!/bin/bash

set -e  # stop on first error (remove if you want all tests to continue)

CLANG="xcrun /opt/opencilk/bin/clang++"
FLAGS="-fopencilk -Wno-backend-plugin"
BASE="/Users/ranaelgahawy/Desktop/bombyx/tests"

echo "=== Running OpenCilk tests ==="

run_test () {
  NAME=$1

  echo ">> $NAME"

  $CLANG $NAME.cpp $FLAGS -o $NAME
  ./$NAME > ${NAME}.txt || echo "⚠️ $NAME crashed"

  diff "$BASE/ex/${NAME}.txt" "$BASE/im/${NAME}.txt" || echo "❌ $NAME output differs"
}

# Tests with expected outputs
run_test "nqueens"
run_test "fib"
run_test "listing_7"
run_test "listing_8"
run_test "listing_9"
run_test "listing_10"
run_test "listing_11"
run_test "listing_13"
run_test "test_0"
run_test "test_1"
run_test "test_2"
run_test "test_3"

echo "✅ Done"