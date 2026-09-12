#!/usr/bin/env bash
set -e

# Windows/MinGW pe "MinGW Makefiles" generator; warna default (Unix Makefiles/Ninja)
GEN=()
case "$(uname -s 2>/dev/null)" in
    *NT*|*MINGW*|*MSYS*|*CYGWIN*) GEN=(-G "MinGW Makefiles");;
esac

echo "== configure (out-of-source: build/ mein) =="
cmake -S . -B build "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release -DENGINE_FAST_PATH=OFF

echo "== build =="
cmake --build build

echo "== run =="
if [ -x build/app ]; then ./build/app; else ./build/app.exe; fi

echo
echo "== ab fast-path ON karke reconfigure =="
cmake -S . -B build "${GEN[@]}" -DENGINE_FAST_PATH=ON
cmake --build build
if [ -x build/app ]; then ./build/app; else ./build/app.exe; fi

echo
echo "== install into ./stage =="
cmake --install build --prefix "$(pwd)/stage"
find stage -type f | sed 's/^/  /'
