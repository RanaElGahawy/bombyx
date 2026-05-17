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
printf "\n >> rectmul \n"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality -Wdeprecated -Wunneeded-internal-declaration rectmul.cpp getoptions.c -o rectmul
./rectmul -benchmark long -c
printf "\n >> rectmulred \n"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality rectmulred.cpp getoptions.c -o rectmulred
./rectmulred -benchmark long -c
printf "\n >> lu \n"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality lu.cpp  getoptions.c -o lu
./lu -n 1024 -c
printf "\n >> heat \n"
xcrun /opt/opencilk/bin/clang++ -fopencilk -Wno-backend-plugin -Wno-parentheses-equality heat.cpp  getoptions.c -o heat
./heat -benchmark long