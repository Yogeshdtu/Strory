#!/usr/bin/env bash
# Multi-file build — har .cxx apni .o mein, phir sab link.
set -e
CXX=${CXX:-g++}
STD=-std=c++20
WARN="-Wall -Wextra -Wpedantic -Wshadow -Wconversion"

echo "== compile (har TU alag) =="
$CXX $STD $WARN -c mathx.cxx -o mathx.o
$CXX $STD $WARN -c stats.cxx -o stats.o
$CXX $STD $WARN -c main.cxx  -o main.o

echo "== link =="
$CXX mathx.o stats.o main.o -o app

echo "== run =="
./app

echo
echo "Tip: sirf ek file badli? Sirf uski .o rebuild karo, phir dobara link."
echo "     touch mathx.cxx && $CXX $STD -c mathx.cxx -o mathx.o && $CXX *.o -o app"
