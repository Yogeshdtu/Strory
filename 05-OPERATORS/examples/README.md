# Examples — Folder 05 (Operators)

| File | Kaunse lesson se | Kya dikhata hai |
|---|---|---|
| `01_arithmetic.cpp` | 01 | 4 traps: integer division, modulo, div-by-zero, overflow + **division cost benchmark** |
| `02_bitwise_basics.cpp` | 05 | Har bitwise operator, binary visualisation ke saath |
| `03_bit_manipulation.cpp` | 06 | Toolkit, `<bit>` (C++20), alignment, endianness + **popcount benchmark** |
| `04_short_circuit.cpp` | 04 | Short-circuit, `&` vs `&&`, safety + **condition ordering benchmark** |
| `05_precedence_traps.cpp` | 09 | 5 classic precedence traps (compiler warnings ke saath) |
| `06_bit_flags.cpp` | 05, 06 | HFT-style order flags + **honest memory/padding comparison** |

## Compile karne ka tarika

```bash
g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_bitwise_basics.cpp -o bits && ./bits
```

**Benchmarks ke liye `-O2` zaroori hai:**
```bash
g++ -std=c++20 -O2 01_arithmetic.cpp -o arith && ./arith
g++ -std=c++20 -O2 04_short_circuit.cpp -o sc && ./sc
```

**`03_bit_manipulation.cpp` ke liye `-march=native` bhi:**
```bash
g++ -std=c++20 -O2 -march=native 03_bit_manipulation.cpp -o bitman && ./bitman
```
⚠️ `-march=native` ke bina `POPCNT`/`LZCNT` instructions available nahi hongi aur
compiler slow fallback code banayega — benchmark ka fark kam dikhega.

**`05_precedence_traps.cpp` — warnings zaroor padho:**
```bash
g++ -std=c++20 -Wall -Wextra -Wparentheses 05_precedence_traps.cpp -o prec
```
Compiler exactly wahi 4 traps pakadta hai jo lesson mein padhaye gaye hain.

## Measured results (aapke machine pe alag ho sakte hain)

| Benchmark | Result |
|---|---|
| `x / d` vs `x * (1/d)` | ~1.9x faster |
| Manual popcount vs `std::popcount` | ~23x faster |
| `cheap && expensive` vs ulta | ~13.7x faster |
| `bool[24]` vs `uint32_t` flags | 50% kam memory |

## Sab ek saath

```bash
for f in *.cpp; do
    echo "════════ $f ════════"
    g++ -std=c++20 -O2 -march=native -Wall -Wextra "$f" -o "/tmp/$(basename $f .cpp)" \
      && "/tmp/$(basename $f .cpp)"
done
```
