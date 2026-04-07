#!/bin/bash

set -e  # stop on first error (you can remove if you want all to run)

CLANG="xcrun /opt/opencilk/bin/clang++"
FLAGS="-fopencilk"

echo "=== Running OpenCilk tests ==="

echo ">> nqueens"
$CLANG nqueens.cpp $FLAGS -o nq
./nq

echo ">> fib"
$CLANG fib.cpp $FLAGS -o fib
./fib

echo ">> listing_7"
$CLANG listing_7.cpp $FLAGS -o listing_7
./listing_7

echo ">> listing_8"
$CLANG listing_8.cpp $FLAGS -o listing_8
./listing_8

echo ">> listing_9"
$CLANG listing_9.cpp $FLAGS -o listing_9
./listing_9 || echo "⚠️ listing_9 crashed (bus error)"

echo ">> listing_10"
$CLANG listing_10.cpp $FLAGS -o listing_10
./listing_10

echo ">> listing_11"
$CLANG listing_11.cpp $FLAGS -o listing_11
./listing_11

echo "✅ Done"