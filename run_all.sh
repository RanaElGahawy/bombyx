#!/bin/bash

set -e  # stop on first error

# Resolve project root dynamically
BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BOMBYX="$BASE/build/bin/bombyx-cc"

echo "Running all Bombyx tests..."

echo ">> nqueens"
$BOMBYX "$BASE/tests/im/nqueens.c" "$BASE/tests/ex/nqueens.cpp"
echo ">> fib"
$BOMBYX "$BASE/tests/im/fib.c" "$BASE/tests/ex/fib.cpp"
echo ">> listing_7"
$BOMBYX "$BASE/tests/im/listing_7.cpp" "$BASE/tests/ex/listing_7.cpp"
echo ">> listing_8"
$BOMBYX "$BASE/tests/im/listing_8.cpp" "$BASE/tests/ex/listing_8.cpp"
echo ">> listing_9"
$BOMBYX "$BASE/tests/im/listing_9.cpp" "$BASE/tests/ex/listing_9.cpp"
echo ">> listing_10"
$BOMBYX "$BASE/tests/im/listing_10.cpp" "$BASE/tests/ex/listing_10.cpp"
echo ">> listing_11"
$BOMBYX "$BASE/tests/im/listing_11.cpp" "$BASE/tests/ex/listing_11.cpp"
echo ">> listing_13"
$BOMBYX "$BASE/tests/im/listing_13.cpp" "$BASE/tests/ex/listing_13.cpp"
echo ">> test0"
$BOMBYX "$BASE/tests/im/test_0.cpp" "$BASE/tests/ex/test_0.cpp"
echo ">> test1"
$BOMBYX "$BASE/tests/im/test_1.cpp" "$BASE/tests/ex/test_1.cpp"
echo ">> test2"
$BOMBYX "$BASE/tests/im/test_2.cpp" "$BASE/tests/ex/test_2.cpp"
echo ">> test3"
$BOMBYX "$BASE/tests/im/test_3.cpp" "$BASE/tests/ex/test_3.cpp"
echo ">> Pagerank"
$BOMBYX "$BASE/Bombyx_OpenCilk_Examples/pageRank/main.cpp" \
        "$BASE/Bombyx_OpenCilk_Examples/pageRank/build/main.cpp"
echo ">> Randomwalk"
$BOMBYX "$BASE/Bombyx_OpenCilk_Examples/randomWalk/main.cpp" \
        "$BASE/Bombyx_OpenCilk_Examples/randomWalk/build/main.cpp"
echo ">> TriangleCount"
$BOMBYX "$BASE/Bombyx_OpenCilk_Examples/triangleCount/main.cpp" \
        "$BASE/Bombyx_OpenCilk_Examples/triangleCount/build/main.cpp"
echo "✅ Done!"