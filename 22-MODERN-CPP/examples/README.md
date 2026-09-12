# Examples — Folder 22 (Modern C++ 11 → 23)

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_lambdas_all.cpp` | 06, 01 | every lambda feature: capture modes, init-capture (incl. move-capture a `unique_ptr`), `mutable`, generic + `[]<class T>` (C++20), trailing return, `constexpr` lambda, IIFE, `[this]`, pack capture; **closure layout** (`sizeof([x1,x2]{})`=16, `sizeof([]{})`=1, `sizeof(std::function)`=32) |
| `02_structured_bindings.cpp` | 03 | unpack tuple / struct / `std::array` / pair; by-value vs `auto&`; `map` iteration `[key, value]`; `if`-with-initializer + binding; `std::tie` into existing vars |
| `03_ranges_pipelines.cpp` | 08, 04 | `filter\|transform\|take` lazy pipeline, adaptors (drop/reverse/take_while/iota/keys), lazy-eval call-counter proof, `std::ranges::sort/max_element/count_if` + **projections**, **measured** pipeline vs hand loop |
| `04_coroutines_generator.cpp` | 09, 04 | hand-rolled `Generator<T>` (promise_type, coroutine_handle, lazy pull); `fib(n)`, lazy `fib(1e6)` take-5, a line splitter. (C++20 has no `std::generator` — that's C++23) |
| `05_modules_demo/` | 10 | **multi-file** C++20 module: `geometry.ixx` (interface, exports `Point`/`distance`/`norm`/`cosAngle`; `dot` module-private) + `main.cxx` (`import geometry;`). Built separately — see its own `README.md` / `build.sh` |
| `06_spaceship.cpp` | 11 | `= default` `<=>` (all six operators); custom `<=>` by key; `strong` / `weak` / `partial` ordering (NaN → all comparisons false); custom `<=>` needs a hand-written `==` |
| `07_format_print.cpp` | 13 | `std::format` basics, positional args, full spec mini-language (align/fill/width/precision/bases/sign), a formatted table, a **custom `std::formatter<Price>`**, `std::format_to_n` into a `char[64]` (no allocation) |
| `08_legacy_to_modern.cpp` | 14, 01 | one program twice: `namespace legacy` (C++98 idioms — iterator loops, functor structs, raw owning pointers, verbose map find/insert) vs `namespace modern` (range-for, lambdas, ranges pipelines, `max_element` + projection, `operator[]`, structured bindings) — same output |
| `09_cpp23_in_practice.cpp23.cpp` | 15, 05 | **C++23, GCC 16.2 pe verified:** deducing `this` (ek getter teen overloads ki jagah + recursive lambda), `static operator()`, `m[r, c]` + `std::mdspan` view, `std::generator` lazy tick replay, `views::enumerate/zip/pairwise/chunk` + `ranges::to` + `fold_left`, `std::expected` chain, `std::move_only_function` (unique_ptr capture), `to_underlying`/`unreachable`/`byteswap` (wire bytes)/`contains`/`auto(x)`/`if consteval`, aur **measured** `std::flat_map` vs `std::map` (lookup 64 / 4k / 262k keys + random insert) — `-O2` pe chalao |
| `10_cpp23_what_happens_next.cpp23.cpp` | 15 | lesson 15 ke "What happens next?" ke 4 sawaalon ka jawab-program: moved-from string, generator body kab chalti hai, moved-from `move_only_function`, `flat_map::emplace` duplicate key. Pehle khud jawab likho, phir chalao |

> `*.cpp23.cpp` files ko `build.ps1` / `Makefile` apne aap `-std=c++23 -lstdc++exp` se build karte hain (`-lstdc++exp` MinGW pe `std::print` ke liye zaroori).

## Compile / run

```bash
./build.ps1 22-MODERN-CPP/examples/01_lambdas_all.cpp     # Windows (debug -O0 + heavy warnings)
make FILE=22-MODERN-CPP/examples/01_lambdas_all.cpp       # Linux/Mac/Git-Bash
```

Benchmark at **`-O2`**:
```bash
./build.ps1 fast 22-MODERN-CPP/examples/03_ranges_pipelines.cpp
```

The **modules demo builds separately** (not via `./build.ps1 folder` /
`checkall`, which only see `*.cpp`):
```bash
cd 22-MODERN-CPP/examples/05_modules_demo && ./build.sh    # or .\build.ps1
```

## Measured (GCC 15.1.0, `-O2`, x86-64, this box) — sample runs

### `01_lambdas_all.cpp`
```
sizeof([x1(int), x2(double)]{...}) = 16
sizeof([]{...})                    = 1     (empty closure)
sizeof(std::function<int()>)       = 32    (type-erased wrapper)
```

### `03_ranges_pipelines.cpp` (5,000,000 int64, "sum 3*x for even x")
```
ranges pipeline : ~6.5 ms
hand loop       : ~10 ms      <- the pipeline is ~1.5x FASTER
```
Surprising-but-real: `if (x%2==0) s += x*3;` is a **conditional accumulate** GCC
won't vectorize; the pipeline's structure lets it emit **branchless masked SIMD**.
(CLAUDE.md Rule 2 — measure, don't assume abstraction = slower.)

## Notes / jaan-boojh kar cheezein

- **No `broken_on_purpose` file.** Several examples have commented-out lines
  showing what *doesn't* compile (`// std::format(fmt, x); // ERROR ...`).
- `04_coroutines_generator.cpp` — C++20 ships no `std::generator`, so the example
  **hand-rolls** `Generator<T>` with a full `promise_type`. C++23 has
  `std::generator`; the frame cost is unchanged.
- `05_modules_demo/` uses `.ixx` / `.cxx` extensions on purpose so the repo's
  `*.cpp` compile-check skips them (a module interface unit and an `import`ing TU
  don't build under a plain `g++ -std=c++20 file.cpp`). Verified with
  `-fmodules-ts` on MinGW-w64 GCC 15.1.0.
- `07_format_print.cpp` uses `std::format` (C++20) + `std::fputs`; `std::print`
  (C++23) may be missing on this libstdc++.
- All `.cpp` compile clean under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wsign-conversion -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion`
  (`./build.ps1 folder 22-MODERN-CPP`).
