#!/bin/bash

set -e

CLANGXX="xcrun /opt/opencilk/bin/clang++"

COMMON_FLAGS="-fopencilk -Wno-backend-plugin"
CXX_FLAGS="$COMMON_FLAGS -Wno-parentheses-equality"

compile_with_getoptions() {
  SRC="$1"
  OUT="$2"
  EXTRA_FLAGS="$3"

  $CLANGXX $COMMON_FLAGS -x c++ -c getoptions.c -o getoptions.o
  $CLANGXX $CXX_FLAGS $EXTRA_FLAGS -c "$SRC" -o "$OUT.o"
  $CLANGXX $COMMON_FLAGS "$OUT.o" getoptions.o -o "$OUT"
}

printf ">> nqueens \n"
compile_with_getoptions nqueens.cpp nqueens ""
./nqueens 13

printf "\n >> fib \n"
compile_with_getoptions fib.cpp fib ""
./fib 35

printf "\n >> cilksort \n"
compile_with_getoptions cilksort.cpp cilksort ""
./cilksort -n 100000000 -c

printf "\n >> qsort \n"
$CLANGXX $CXX_FLAGS qsort.cpp -o qsort
./qsort 80000000 -c

printf "\n >> matmul \n"
compile_with_getoptions matmul.cpp matmul ""
./matmul -n 1000 -c

printf "\n >> rectmul \n"
compile_with_getoptions rectmul.cpp rectmul "-Wdeprecated -Wunneeded-internal-declaration"
./rectmul -benchmark long -c

printf "\n >> rectmulred \n"
compile_with_getoptions rectmulred.cpp rectmulred ""
./rectmulred -benchmark long -c

printf "\n >> lu \n"
compile_with_getoptions lu.cpp lu ""
./lu -n 1024 -c

printf "\n >> heat \n"
compile_with_getoptions heat.cpp heat ""
./heat -benchmark long

printf "\n >> strassen \n"
compile_with_getoptions strassen.cpp strassen ""
./strassen -n 1024 -c

printf "\n >> fft \n"
compile_with_getoptions fft.cpp fft ""
./fft -c

printf "\n >> cholesky \n"
compile_with_getoptions cholesky.cpp fft ""
./cholesky -c