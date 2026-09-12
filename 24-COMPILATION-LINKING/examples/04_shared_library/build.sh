#!/usr/bin/env bash
set -e
CXX=${CXX:-g++}
STD=-std=c++20

OS=linux
case "$(uname -s 2>/dev/null)" in
    *NT*|*MINGW*|*MSYS*|*CYGWIN*) OS=windows;;
    Darwin) OS=macos;;
esac

echo "== target OS: $OS =="

if [ "$OS" = "windows" ]; then
    # ---- Windows: greet.dll + import library libgreet.dll.a ----
    $CXX $STD -O2 -c greet.cxx -o greet.o
    $CXX -shared greet.o -o greet.dll -Wl,--out-implib,libgreet.dll.a
    $CXX $STD -O2 main.cxx -L. -lgreet -o app.exe        # link against import lib
    echo "-- run (greet.dll isi folder mein hai, isliye milta hai) --"
    ./app.exe
    echo
    echo "-- app.exe kis DLL pe depend karta --"
    ( objdump -p app.exe 2>/dev/null | grep -i 'DLL Name' ) || true
else
    # ---- Linux/macOS: libgreet.so ----
    EXT=so; [ "$OS" = macos ] && EXT=dylib
    $CXX $STD -O2 -fPIC -c greet.cxx -o greet.o          # -fPIC: position-independent
    $CXX -shared greet.o -o "libgreet.$EXT" -Wl,-soname,libgreet.$EXT
    $CXX $STD -O2 main.cxx -L. -lgreet -Wl,-rpath,'$ORIGIN' -o app
    echo "-- run --"
    ./app
    echo
    echo "-- ldd: app kis .so pe depend karta --"
    ( ldd app 2>/dev/null | grep -i greet ) || true
    echo
    echo "-- library ke dynamic symbols --"
    ( nm -CD "libgreet.$EXT" 2>/dev/null | grep greet:: ) || true
fi

echo
echo "Note: shared lib ka code binary ke ANDAR nahi — run time pe loader"
echo "(ld.so / Windows loader) use dhoondke map karta hai. Isliye deploy pe"
echo "library saath bhejni padti hai aur uska path resolve hona chahiye."
