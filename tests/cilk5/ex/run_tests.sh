#!/bin/bash

set -e

printf ">> nqueens"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality nqueens.cpp getoptions.c -o nqueens
./nqueens 13 
printf "\n >> fib"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality fib.cpp getoptions.c -o fib
./fib 35
printf "\n >> cilksort"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality cilksort.cpp getoptions.c -o cilksort
./cilksort -n 100000000 -c
printf "\n >> qsort"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality qsort.cpp -o qsort 
./qsort 80000000 -c