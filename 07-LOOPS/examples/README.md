# Examples — Folder 07 (Loops)

| File | Kaunse lesson se | Kya dikhata hai |
|---|---|---|
| `01_loop_types.cpp` | 01, 02, 03 | `while` / `do-while` / `for`, `for`⟺`while` equivalence, `for(;;)` + `break` |
| `02_range_based.cpp` | 04 | `auto` vs `auto&` vs `const auto&`, structured bindings, C-array + **measured ~50x copy cost** |
| `03_nested_patterns.cpp` | 05 | Rectangle, triangle, pyramid, multiplication table, triple-nested O(n³) counter |
| `04_loop_bugs.cpp` | 07 | 7 loop bugs — off-by-one, infinite, unsigned underflow, float counter, etc. Har ek **safety-capped** (program hang nahi hoga). `[buggy]` vs `[fixed]`, notes file mein |
| `05_cache_locality.cpp` | 09 | Row-major vs column-major 2D traversal — **measured ~8x**. `-O2` zaroori |
| `06_loop_unroll.cpp` | 09 | Manual 4x unroll vs compiler — result **flags pe depend karta hai** (`-O2` pe unroll 2x tez, `-O3`/`-march=native` pe barabar ya slower) |

## Compile karne ka tarika

```bash
# repo root se (Windows / PowerShell):
./build.ps1 07-LOOPS/examples/01_loop_types.cpp

# Linux / Mac / Git-Bash:
make FILE=07-LOOPS/examples/01_loop_types.cpp

# manual (debug):
g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_loop_types.cpp -o out && ./out
```

**Benchmarks — `-O2` (ya zyada) ZAROORI:**
```bash
./build.ps1 fast 07-LOOPS/examples/05_cache_locality.cpp
make fast FILE=07-LOOPS/examples/02_range_based.cpp

# 06_loop_unroll -- SAARE flags try karo (yahi lesson hai):
g++ -std=c++20 -O2               07-LOOPS/examples/06_loop_unroll.cpp -o lu   && ./lu
g++ -std=c++20 -O3               07-LOOPS/examples/06_loop_unroll.cpp -o lu3  && ./lu3
g++ -std=c++20 -O2 -march=native 07-LOOPS/examples/06_loop_unroll.cpp -o lun  && ./lun
```
⚠️ `-O0` pe `05` aur `06` ke numbers **jhoothe** hain — optimizer off.

## Jaan-boojh kar warning

- **`04_loop_bugs.cpp`** — yeh file **jaan-boojh kar** 1 warning deti hai:
  `-Wtype-limits` (`comparison of unsigned expression >= 0 is always true`) BUG 3
  pe. Wahi lesson hai — compiler unsigned reverse-loop bug ko khud pakadta hai.
  File compile + run dono karti hai (har infinite loop safety-capped). Baaki 5
  files clean compile karti hain.

Koi file `*_broken_on_purpose.cpp` nahi — sab 6 compile aur run karti hain.

## Measured results (aapke machine pe alag ho sakte hain)

GCC 15.1.0, x86-64:

| Benchmark | flags | Result |
|---|---|---|
| `02` — `for (auto s : names)` vs `for (const auto& s : names)` | `-O2` | copy **~50x slower** (200k heap allocs) |
| `05` — row-major vs column-major (4096² int32) | `-O2` | column **~8x slower** |
| `06` — manual 4x unroll vs naive | `-O2` | unroll ~2.3x **faster** (dependency chain) |
| `06` — manual 4x unroll vs naive | `-O3` / `-O2 -march=native` | ~equal or unroll **slower** |

## Sab ek saath

```bash
for f in 07-LOOPS/examples/*.cpp; do
    echo "======== $f ========"
    g++ -std=c++20 -O2 -Wall -Wextra "$f" -o "/tmp/$(basename "$f" .cpp)" \
      && "/tmp/$(basename "$f" .cpp)"
done

# quick compile-check:
./build.ps1 folder 07-LOOPS          # Windows
make folder DIR=07-LOOPS             # Linux/Mac/Git-Bash
```
