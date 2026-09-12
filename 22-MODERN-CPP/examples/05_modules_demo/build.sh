#!/usr/bin/env bash
# build the C++20 modules demo (NOT picked up by ../../Makefile / build.ps1 folder)
# needs GCC with -fmodules-ts (or Clang with -fmodules / MSVC /std:c++20)
set -euo pipefail
cd "$(dirname "$0")"
g++ -std=c++20 -fmodules-ts -Wall -Wextra -c geometry.ixx -o geometry.o
g++ -std=c++20 -fmodules-ts -Wall -Wextra main.cxx geometry.o -o modules_demo
echo "built modules_demo"
./modules_demo
