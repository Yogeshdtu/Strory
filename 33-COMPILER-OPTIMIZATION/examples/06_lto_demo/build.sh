#!/usr/bin/env bash
# build.sh -- LTO demo (Linux / Git Bash). Build both ways, run both.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p _out

echo "== NO LTO =="
g++ -std=c++20 -O2 -Wall -Wextra -c main.cxx  -o _out/main_nolto.o
g++ -std=c++20 -O2 -Wall -Wextra -c mathx.cxx -o _out/mathx_nolto.o
g++ -std=c++20 -O2 _out/main_nolto.o _out/mathx_nolto.o -o _out/demo_nolto
./_out/demo_nolto

echo
echo "== WITH LTO =="
g++ -std=c++20 -O2 -flto -D__LTO_BUILD__ -Wall -Wextra -c main.cxx  -o _out/main_lto.o
g++ -std=c++20 -O2 -flto -Wall -Wextra -c mathx.cxx -o _out/mathx_lto.o
g++ -std=c++20 -O2 -flto _out/main_lto.o _out/mathx_lto.o -o _out/demo_lto
./_out/demo_lto

echo
echo "(Compare ns/elem. -flto inlines hot_transform across the TU boundary.)"
