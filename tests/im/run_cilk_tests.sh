#!/bin/bash

set -e  # stop on first error (you can remove if you want all to run)

CLANG="xcrun /opt/opencilk/bin/clang"
CLANGCPP="xcrun /opt/opencilk/bin/clang++"
FLAGS="-fopencilk"

echo "=== Running OpenCilk tests ==="

echo ">> nqueens"
$CLANG nqueens.c $FLAGS -o nq
./nq > nq.txt

echo ">> fib"
$CLANG fib.c $FLAGS -o fib
./fib > fib.txt

echo ">> listing_7"
$CLANGCPP listing_7.cpp $FLAGS -o listing_7
./listing_7 > listing_7.txt

echo ">> listing_8"
$CLANGCPP listing_8.cpp $FLAGS -o listing_8
./listing_8 > listing_8.txt

echo ">> listing_9"
$CLANGCPP listing_9.cpp $FLAGS -o listing_9
./listing_9 > listing_9.txt

echo ">> listing_10"
$CLANGCPP listing_10.cpp $FLAGS -o listing_10
./listing_10 > listing_10.txt

echo ">> listing_11"
$CLANGCPP listing_11.cpp $FLAGS -o listing_11
./listing_11 > listing_11.txt

echo ">> listing_13"
$CLANGCPP listing_13.cpp $FLAGS -o listing_13
./listing_13 > listing_13.txt

echo "✅ Done"