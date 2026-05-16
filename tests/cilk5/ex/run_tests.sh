#!/bin/bash

set -e

printf ">> nqueens \n"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality nqueens.cpp getoptions.c -o nqueens
./nqueens 13 
printf "\n >> fib \n"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality fib.cpp getoptions.c -o fib
./fib 35
printf "\n >> cilksort \n"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality cilksort.cpp getoptions.c -o cilksort
./cilksort -n 100000000 -c
printf "\n >> qsort \n"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality qsort.cpp -o qsort 
./qsort 80000000 -c
printf "\n >> matmul \n"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality matmul.cpp getoptions.c -o matmul
./matmul -n 1000 -c