# Examples — Folder 06 (Conditions)

| File | Kaunse lesson se | Kya dikhata hai |
|---|---|---|
| `01_if_else.cpp` | 01, 02, 03 | `if`/`else`/`else-if` chain, truthiness, braces trap, guard clause vs nested, ternary |
| `02_switch_demo.cpp` | 04 | `switch` basics, fallthrough bug, grouped cases, `[[fallthrough]]`, `case`+variable `{ }`, `enum class` switch |
| `03_condition_bugs.cpp` | 07 | 7 classic condition bugs — `[buggy]` vs `[fixed]` side by side, notes/answers file mein |
| `04_branch_benchmark.cpp` | 05, 08 | Sorted vs unsorted array — branch prediction ka **measured ~7x** asar + branchless |

## Compile karne ka tarika

```bash
# repo root se (Windows / PowerShell):
./build.ps1 06-CONDITIONS/examples/01_if_else.cpp

# Linux / Mac / Git-Bash:
make FILE=06-CONDITIONS/examples/01_if_else.cpp

# manual (debug):
g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_if_else.cpp -o out && ./out
```

**`04_branch_benchmark.cpp` — `-O2` ZAROORI hai:**
```bash
./build.ps1 fast 06-CONDITIONS/examples/04_branch_benchmark.cpp
make fast FILE=06-CONDITIONS/examples/04_branch_benchmark.cpp
g++ -std=c++20 -O2 04_branch_benchmark.cpp -o bb && ./bb
```
⚠️ `-O0` pe branch-prediction experiment **jhootha** hai (optimizer off → sab kuch
branch-heavy aur slow → sorted/unsorted ka farq meaningless).

## Jaan-boojh kar warnings

Yeh do files apne teaching bugs ke saath **jaan-boojh kar** warnings deti hain
(jaise folder 05 ka `05_precedence_traps.cpp`):

- **`02_switch_demo.cpp`** — section 2 (`break` missing) ka `-Wimplicit-fallthrough`
  locally `#pragma` se silence kiya hai, sirf bug ka *asar* dikhane ke liye. Baaki
  file clean compile karti hai.
- **`01_if_else.cpp`** — section 5 (dangling else) ka `-Wdangling-else` bhi locally
  silence kiya hai. Baaki clean.
- **`03_condition_bugs.cpp`** — yeh file **jaan-boojh kar** ~6 warnings deti hai
  (`-Wparentheses`, `-Wmisleading-indentation`, `-Wempty-body`, `-Wtype-limits`).
  Wahi lesson hai: compiler in bugs ko khud pakad leta hai. **Compile ke waqt
  warnings zaroor padho.** File compile aur run dono hoti hai (crash nahi karti).

Koi bhi file `*_broken_on_purpose.cpp` nahi hai — sab 4 compile aur run karti hain.

## Measured results (aapke machine pe alag ho sakte hain)

GCC 15.1.0, `-O2`, x86-64:

| Benchmark | Result |
|---|---|
| `04` — if-branch, UNSORTED data | ~1450 ms |
| `04` — if-branch, SORTED data | ~205 ms  (**~7x tez**, sirf order alag) |
| `04` — branchless, UNSORTED data | ~240 ms  (data order se independent) |
| (lesson 05) `switch` jump table vs O(n) linear scan | ~8x |

## Sab ek saath

```bash
# repo root se
for f in 06-CONDITIONS/examples/*.cpp; do
    echo "======== $f ========"
    g++ -std=c++20 -O2 -Wall -Wextra "$f" -o "/tmp/$(basename "$f" .cpp)" \
      && "/tmp/$(basename "$f" .cpp)"
done

# ya poore folder ka quick compile-check:
./build.ps1 folder 06-CONDITIONS          # Windows
make folder DIR=06-CONDITIONS             # Linux/Mac/Git-Bash
```
