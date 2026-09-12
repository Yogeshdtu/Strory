#!/usr/bin/env bash
set -e
CXX=${CXX:-g++}
STD=-std=c++20
AR=${AR:-ar}

# Windows (MinGW/MSYS) pe g++ output ".exe" banata hai
EXE=app
case "$(uname -s 2>/dev/null)" in *NT*|*MINGW*|*MSYS*|*CYGWIN*) EXE=app.exe;; esac

echo "== 1. har source ko .o mein compile =="
$CXX $STD -O2 -c add.cxx    -o add.o
$CXX $STD -O2 -c mul.cxx    -o mul.o
$CXX $STD -O2 -c unused.cxx -o unused.o

echo "== 2. .o files ko ek archive (.a) mein pack =="
rm -f libcalc.a
$AR rcs libcalc.a add.o mul.o unused.o        # r=insert, c=create, s=index
$AR t libcalc.a                                # archive ke members list karo

echo
echo "== 3. app ko static library ke saath link =="
$CXX $STD -O2 -c main.cxx -o main.o
$CXX main.o -L. -lcalc -o "$EXE"               # -L. : yahan dhoondo; -lcalc : libcalc.a
# (ya seedha:  $CXX main.o libcalc.a -o "$EXE" )

echo "== 4. run =="
"./$EXE"

echo
echo "== 5. inspection =="
echo "-- app mein calc:: symbols (add/mul/dot present; huge_unused nahi hona chahiye) --"
nm -C "$EXE" | grep -E 'calc::' || true
echo
echo "-- app ki size (static: library code binary ke ANDAR hai) --"
size "$EXE" 2>/dev/null || true
echo
echo "-- app ka calc runtime dependency? (nahi — static) --"
( ldd "$EXE" 2>/dev/null || true ) | grep -i calc || echo "  (calc ka koi runtime dependency nahi — expected)"
