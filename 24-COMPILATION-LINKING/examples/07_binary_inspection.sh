#!/usr/bin/env bash
# ============================================================
# 07 — Binary inspection tools ka tour (lesson 08, 14).
# Ek chhota program build karke uspe nm / objdump / readelf / size /
# strings / c++filt / strip chalata hai. Har tool ka ek line matlab.
#
#   bash 07_binary_inspection.sh
#
# Windows/MinGW: binutils (nm, objdump, size, strings, strip, c++filt)
# ship karte hain; `readelf` PE/COFF pe kaam nahi karta (ELF-only) —
# tab objdump ke equivalents use karo. `ldd` Linux; Windows pe
# `objdump -p | grep 'DLL Name'` ya `ntldd`.
# ============================================================
set -u
CXX=${CXX:-g++}
here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

EXE=prog
case "$(uname -s 2>/dev/null)" in *NT*|*MINGW*|*MSYS*|*CYGWIN*) EXE=prog.exe;; esac

have() { command -v "$1" >/dev/null 2>&1; }
rule() { printf '\n============ %s ============\n' "$1"; }

# ---- 1. ek chhota multi-symbol program ----
cat > "$tmp/lib.cxx" <<'EOF'
namespace demo {
int  add(int a, int b) { return a + b; }
long factorial(int n) { long r = 1; for (int i = 2; i <= n; ++i) r *= i; return r; }
}
const char* kGreeting = "inspect-me-string-CONSTANT";
static int internal_only(int x) { return x * 7; }   // internal linkage
int use_internal(int x) { return internal_only(x); }
EOF
cat > "$tmp/main.cxx" <<'EOF'
#include <cstdio>
namespace demo { int add(int,int); long factorial(int); }
int use_internal(int);
extern const char* kGreeting;
int main() {
    std::printf("%s %d %ld %d\n", kGreeting, demo::add(2,3), demo::factorial(6), use_internal(3));
}
EOF

cd "$tmp"
$CXX -std=c++20 -O2 -g -c lib.cxx  -o lib.o
$CXX -std=c++20 -O2 -g -c main.cxx -o main.o
$CXX lib.o main.o -o "$EXE"
echo "built $tmp/$EXE"
./"$EXE"

# ---- 2. file: yeh hai kya? ----
rule "file  — binary ka type/arch/OS"
if have file; then file "$EXE"; else echo "(file not installed)"; fi

# ---- 3. size: sections ka byte breakdown ----
rule "size  — text / data / bss (code vs init-data vs zero-init)"
if have size; then size "$EXE" lib.o; else echo "(size not installed)"; fi
echo "  text = code+rodata | data = initialized globals | bss = zero-init globals (file mein 0 bytes)"

# ---- 4. nm: symbol table ----
rule "nm  — symbol table (T=text/defined, U=undefined, t=local, R=rodata, D=data)"
if have nm; then
    echo "-- lib.o ke symbols (demangled) --"
    nm -C lib.o
    echo
    echo "-- main.o mein UNDEFINED (linker ko chahiye) --"
    nm -C main.o | grep ' U ' || true
    echo
    echo "-- 'demo::' symbols final binary mein --"
    nm -C "$EXE" | grep 'demo::' || true
else echo "(nm not installed)"; fi

# ---- 5. c++filt: mangled <-> demangled ----
rule "c++filt  — mangled name ko padhne-layak banao"
if have c++filt; then
    echo '_ZN4demo3addEii   ->' "$(echo _ZN4demo3addEii | c++filt)"
    echo '_ZN4demo9factorialEi ->' "$(echo _ZN4demo9factorialEi | c++filt)"
else echo "(c++filt not installed)"; fi

# ---- 6. objdump -d: disassembly ----
rule "objdump -d  — machine code (demo::add ka disassembly)"
if have objdump; then
    objdump -d -C "$EXE" | grep -A12 '<demo::add(int, int)>:' || \
    objdump -d -C "$EXE" | grep -A12 'add(int, int)' || echo "(symbol inline ho gaya?)"
else echo "(objdump not installed)"; fi

# ---- 7. objdump -h: sections ----
rule "objdump -h  — section headers (.text .data .rodata .bss ...)"
if have objdump; then objdump -h "$EXE" | awk 'NR<=5 || /\.(text|data|rodata|bss|eh_frame)/'; fi

# ---- 8. objdump -p / readelf -d / ldd: dynamic dependencies ----
rule "dynamic dependencies (yeh binary run time pe kya load karega)"
if [ "$EXE" = "prog.exe" ]; then
    have objdump && objdump -p "$EXE" | grep -i 'DLL Name' || true
    have ntldd  && ntldd  "$EXE" || echo "  (ntldd nahi — objdump -p upar)"
else
    have readelf && readelf -d "$EXE" | grep -E 'NEEDED|RUNPATH|SONAME' || true
    have ldd && ldd "$EXE" || true
fi

# ---- 9. readelf -h / -S (ELF only) ----
rule "readelf -h / -S  — ELF header + sections (Linux binaries)"
if have readelf && [ "$EXE" = "prog" ]; then
    readelf -h "$EXE" | grep -E 'Class|Machine|Type|Entry'
    readelf -S "$EXE" | grep -E '\.text|\.data|\.bss|\.rodata' || true
else
    echo "  (readelf ELF-only; is Windows binary pe skip — objdump -h upar use karo)"
fi

# ---- 10. strings: printable text ----
rule "strings  — binary ke andar readable text (constants, paths, messages)"
if have strings; then strings "$EXE" | grep -i 'inspect-me' || true; fi
echo "  (rev-eng / secrets-leak check ke liye classic)"

# ---- 11. strip: symbols hata ke size ghatao ----
rule "strip  — debug/symbol info hatao (release binaries)"
cp "$EXE" "${EXE}.stripped"
if have strip; then
    strip "${EXE}.stripped"
    if have size; then
        echo "-- pehle --"; size "$EXE"
        echo "-- strip ke baad --"; size "${EXE}.stripped"
    fi
    if have nm; then
        echo "stripped binary ka nm: $(nm -C "${EXE}.stripped" 2>&1 | head -1)"
    fi
fi

rule "DONE"
echo "cheat-sheet:"
echo "  file X            type/arch"
echo "  size X            code/data/bss bytes"
echo "  nm -C X           symbols (T defined, U undefined, t local)"
echo "  c++filt <sym>     demangle"
echo "  objdump -d -C X   disassemble"
echo "  objdump -h X      sections   (Windows/PE)"
echo "  readelf -a X      everything (Linux/ELF)"
echo "  ldd X / objdump -p X   shared-lib deps"
echo "  strings X         embedded text"
echo "  strip X           drop symbols"
