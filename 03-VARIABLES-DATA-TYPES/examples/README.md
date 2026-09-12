# Examples — Folder 03 (Variables & Data Types)

| File | Kaunse lesson se | Kya dikhata hai |
|---|---|---|
| `01_first_variable.cpp` | 02, 04 | Variable ka har hissa, memory model, copy vs link |
| `02_all_types.cpp` | 01, 05, 06 | Saare fundamental types ki sizes aur ranges |
| `03_integer_overflow.cpp` | 05 | ⚠️ Overflow, signed/unsigned traps, division traps |
| `04_float_traps.cpp` | 06 | ⚠️ `0.1+0.2`, NaN, precision loss, finance rule |
| `05_initialization.cpp` | 10 | Saare init forms, narrowing, Most Vexing Parse, vector trap |
| `06_conversions.cpp` | 13 | Implicit conversions ke saare bugs, safe bit_cast |
| `07_fixed_width.cpp` | 09 | `<cstdint>`, wire protocol struct, `static_assert` |
| `08_const_constexpr.cpp` | 11 | `const` vs `constexpr` vs `consteval`, compile-time tables |
| `09_char_demo.cpp` | 07 | `char` ek number hai, signedness, ASCII tricks |
| `10_auto_demo.cpp` | 12 | `auto` deduction, copy traps, const dropping |

## Compile karne ka tarika

```bash
g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_first_variable.cpp -o v && ./v
```

## Sab ek saath

```bash
for f in *.cpp; do
    echo "════════ $f ════════"
    g++ -std=c++20 -Wall -Wextra "$f" -o "/tmp/$(basename $f .cpp)" && \
    "/tmp/$(basename $f .cpp)"
done
```

## Extra experiments

### 1. UBSan se signed overflow pakdo
```bash
g++ -std=c++20 -fsanitize=undefined -g 03_integer_overflow.cpp -o ovf && ./ovf
```

### 2. Strict warnings ke saath conversions dekho
```bash
g++ -std=c++20 -Wall -Wextra -Wconversion -Wsign-conversion \
    06_conversions.cpp -o conv
```
Kitni warnings aayi?

### 3. Verify karo ki `constexpr` compile time pe chala
```bash
g++ -std=c++20 -O2 -S 08_const_constexpr.cpp -o - | grep 3628800
```
Agar `3628800` literally assembly mein dikhe — factorial compile time pe calculate hua,
runtime pe nahi. 🎉

### 4. `-O0` vs `-O2` pe UB ka fark
```cpp
// ub_test.cpp
#include <iostream>
#include <limits>
bool check(int x) { return x + 1 > x; }
int main() {
    std::cout << check(5) << " " << check(std::numeric_limits<int>::max()) << "\n";
}
```
```bash
g++ -O0 ub_test.cpp -o ub0 && ./ub0
g++ -O2 ub_test.cpp -o ub2 && ./ub2
```
Alag output aaya? Yeh signed overflow UB ka asli khatra hai.

### 5. Struct padding experiment
```bash
g++ -std=c++20 -Wpadded 07_fixed_width.cpp -o fw 2>&1 | head -20
```
`-Wpadded` batata hai kahan-kahan padding daali gayi.
