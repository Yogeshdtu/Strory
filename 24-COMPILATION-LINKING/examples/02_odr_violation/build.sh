#!/usr/bin/env bash
# ODR violation demos. Yeh script FAIL hone ke liye hai (pehla demo) —
# error dekhna hi seekh hai. `set -e` isliye nahi lagaya.
CXX=${CXX:-g++}
STD=-std=c++20

echo "############################################################"
echo "# DEMO 1: non-inline function 'venue_name' do headers se     #"
echo "#         -> LINKER PAKAD leta hai: 'multiple definition'    #"
echo "############################################################"
$CXX $STD -c a.cxx -o a.o
$CXX $STD -c b.cxx -o b.o
$CXX $STD -c main.cxx -o main.o
echo "--- link (yahan FAIL hoga) ---"
$CXX a.o b.o main.o -o bad 2>&1 | sed 's/^/  /'
echo "  (exit: $? — 'multiple definition of venue_name()' expected)"
echo
echo "  FIX: shared.hpp mein sirf declaration rakho + body ek .cxx mein,"
echo "       ya function ko 'inline' karo."
echo

echo "############################################################"
echo "# DEMO 2: 'Config' struct ke DO alag layouts (silent ODR)   #"
echo "#         -> linker CHUP; sirf -flto -Wodr pakadta hai       #"
echo "############################################################"
$CXX $STD -c silent_a.cxx -o silent_a.o
$CXX $STD -c silent_b.cxx -o silent_b.o
$CXX $STD -c silent_main.cxx -o silent_main.o
echo "--- plain link (koi error nahi — YEH problem hai) ---"
$CXX silent_a.o silent_b.o silent_main.o -o silent_bad && ./silent_bad
echo "  ^ do alag sizeof(Config) — dono TUs apna hi layout maan rahi hain."
echo
echo "--- ab -flto -Wodr ke saath (yeh pakadta hai) ---"
$CXX $STD -flto -Wodr silent_a.cxx silent_b.cxx silent_main.cxx -o silent_lto 2>&1 | sed 's/^/  /'
echo "  (LTO ke paas dono TUs ka IR hota hai -> mismatched 'Config' detect)"
