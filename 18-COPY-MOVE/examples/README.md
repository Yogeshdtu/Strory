# Examples — Folder 18 (Copy & Move Semantics)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_copy_semantics.cpp` | 01, 02 | `Str` deep-copy ctor + exception-safe self-guarded copy assign; distinct buffer addresses; pass-by-value copy |
| `02_rule_of_three.cpp` | 03 | correct `IntArray` triad (dtor + copy ctor + copy assign); counted `operator new[]` → `outstanding=0`; commented broken "dtor-only" version |
| `03_value_categories.cpp` | 04 | `probe(T&)` / `probe(const T&)` / `probe(T&&)` overload set reports what each expression binds to; `decltype(x)` vs `decltype((x))`; `std::move` on const → falls back to `const&` |
| `04_move_semantics.cpp` | 05, 06, 07 | `Buffer` copy vs move ctor/assign traces (same address moves source→dest); moved-from `size()==0`; temporary → move; moved-from reassigned |
| `05_std_move_demo.cpp` | 08 | `std::move` alone changes nothing; a move ctor consuming it empties the source; `std::move` on const → copy; **`return std::move(x)` → 1 intentional `-Wpessimizing-move`**; move into a container |
| `06_copy_elision.cpp` | 11 | RVO / NRVO print **one** ctor; `makeConditional` → COPY; run with `-fno-elide-constructors` to see NRVO's saved move (but guaranteed prvalue elision stays) |
| `07_perfect_forwarding.cpp` | 12 | `relayBad` (no forward → always lvalue) vs `relayGood` (`std::forward` preserves category); reference collapsing; generic `make<T>(Args&&...)` forwarding a moved string |
| `08_noexcept_vector.cpp` | 13 | **Measured**: identical classes ±`noexcept` on move ctor → `std::vector` growth MOVES vs COPIES → ~**3x** (`is_nothrow_move_constructible` 1 vs 0) |
| `09_copy_vs_move_bench.cpp` | 06, 14 | **Measured**: copy 1M `std::string`s (~177 ms) vs move the vector (~0.0001 ms); return 5M-int vector by value ≈ 8.5 ms/call (no copy) |

## Compile / run

```bash
./build.ps1 18-COPY-MOVE/examples/01_copy_semantics.cpp        # Windows
make FILE=18-COPY-MOVE/examples/01_copy_semantics.cpp          # Linux/Mac/Git-Bash
```

Benchmarks at **`-O2`**:

```bash
./build.ps1 fast 18-COPY-MOVE/examples/08_noexcept_vector.cpp
./build.ps1 fast 18-COPY-MOVE/examples/09_copy_vs_move_bench.cpp
```

Elision demo — run both:

```bash
g++ -std=c++20 -O2 18-COPY-MOVE/examples/06_copy_elision.cpp -o ce && ./ce
g++ -std=c++20 -O2 -fno-elide-constructors 18-COPY-MOVE/examples/06_copy_elision.cpp -o ce_off && ./ce_off
```

## Measured (GCC 15.1, `-O2`, x86-64) — sample runs

### `08_noexcept_vector.cpp`
```
is_nothrow_move_constructible:  WithNoexcept: 1   WithoutNoexcept: 0
push_back 200000 elements (256-int buffer each), no reserve:
  noexcept move    -> vector MOVES on realloc   ~152 ms
  non-noexcept move -> vector COPIES on realloc  ~468 ms   (~3x)
```

### `09_copy_vs_move_bench.cpp`
```
copy vs move  std::vector<std::string> of 1,000,000 x 50-char:
  copy (auto v = a)           :  ~177 ms
  move (auto v = std::move(a)):  ~0.0001 ms      ratio ~1,700,000x
returning std::vector<int>(5M) by value: ~8.5 ms/call  (NRVO/move, not a copy)
```

The **shape** reproduces everywhere: move is O(1), copy is O(n) + allocation;
`noexcept` move ctor is required for `std::vector` growth to move.

## Jaan-boojh kar warnings

- **`05_std_move_demo.cpp`** — **jaan-boojh kar** 1 warning: `-Wpessimizing-move`
  (section 4, the `makeBad` lambda `return std::move(x)`). That *is* the lesson —
  `return std::move(local)` disables NRVO. The file compiles and runs; the
  contrast with `makeGood()` (`return x` → NRVO, no move printed) is the point.
  All other files compile clean under `-Wall -Wextra -Wshadow -Wconversion
  -Wsign-conversion -Wpedantic`.

## Notes

- No `broken_on_purpose` file. `05` is a warning-demo, `02`'s broken version is
  `#if 0`-gated with an explanation comment.
- `02`/`03` etc. override `operator new[]` / `operator new` counters (folder 14
  technique) — MinGW-w64 has no ASan.
- `06_copy_elision.cpp` — guaranteed prvalue elision (C++17) is **not** disabled
  by `-fno-elide-constructors`; only the optional NRVO is.
