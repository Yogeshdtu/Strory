#!/usr/bin/env bash
# Static init order fiasco — link order badalne se behaviour badal sakta hai.
set -e
CXX=${CXX:-g++}
STD=-std=c++20
EXE=fiasco
case "$(uname -s 2>/dev/null)" in *NT*|*MINGW*|*MSYS*|*CYGWIN*) EXE=fiasco.exe;; esac

$CXX $STD -Wall -Wextra -c config.cxx -o config.o
$CXX $STD -Wall -Wextra -c logger.cxx -o logger.o
$CXX $STD -Wall -Wextra -c main.cxx   -o main.o

echo "############ link order A: config.o logger.o ############"
$CXX config.o logger.o main.o -o "$EXE"
"./$EXE" || echo "  (non-zero exit / crash — fiasco!)"

echo
echo "############ link order B: logger.o config.o ############"
$CXX logger.o config.o main.o -o "$EXE"
"./$EXE" || echo "  (non-zero exit / crash — fiasco!)"

echo
echo "Dhyaan do: 'BADLOG' line 'BadLogger::BadLogger()' SE PEHLE aayi kisi order"
echo "mein? Ya crash? Woh UB hai — link/TU order ne decide kiya. FIXED side hamesha"
echo "sahi, kyunki construct-on-first-use order ko use se bandhta hai."
