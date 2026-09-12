# 22 — MODERN C++ (11 → 23) (PHASE 12)

## Prerequisites
`21-TEMPLATES`, `18-COPY-MOVE`

## Yeh folder kyun
Ab tak humne modern C++ ko topic-wise use kiya hai. Yeh folder ek **systematic audit**
hai — standard by standard, feature by feature.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-cpp11-features.md` | auto, lambdas, move, `nullptr`, range-for, `constexpr`, threads, smart pointers |
| 02 | `02-cpp14-features.md` | Generic lambdas, return type deduction, `make_unique`, digit separators |
| 03 | `03-cpp17-features.md` | Structured bindings, `if constexpr`, `optional`/`variant`/`any`, filesystem, fold expressions |
| 04 | `04-cpp20-features.md` | Concepts, ranges, coroutines, modules, `<=>`, designated init, `std::span` |
| 05 | `05-cpp23-features.md` | `std::expected`, `std::print`, deducing `this`, `std::mdspan` |
| 06 | `06-lambdas-deep.md` | **Lambdas poora** — captures, `mutable`, generic, init-capture, closures ka layout |
| 07 | `07-constexpr-family.md` | `constexpr`/`consteval`/`constinit` deep, compile-time containers |
| 08 | `08-ranges-deep.md` | **Ranges** — views, adaptors, pipelines, lazy evaluation, performance |
| 09 | `09-coroutines.md` | **Coroutines deep** — `co_await`/`co_yield`/`co_return`, promise types, generators |
| 10 | `10-modules.md` | **Modules** — `import`/`export`, `#include` se fark, build impact |
| 11 | `11-spaceship-operator.md` | `<=>`, comparison categories, auto-generated comparisons |
| 12 | `12-attributes.md` | `[[nodiscard]]`, `[[likely]]`/`[[unlikely]]`, `[[assume]]` (C++23) |
| 13 | `13-format-and-print.md` | `std::format`, `std::print`, custom formatters |
| 14 | `14-migration-guide.md` | Purana C++ → modern C++, before/after examples |
| 15 | `15-cpp23-in-practice.md` | C++23 chala ke + naap ke: deducing `this`, `std::generator`, `mdspan`, ranges additions, `move_only_function`, `import std;` setup, **measured** `flat_map` vs `map` |
| 16 | `16-exercises.md` | Practice + refactoring exercises |

## Examples

| File | Kya |
|---|---|
| `examples/01_lambdas_all.cpp` | Har lambda feature |
| `examples/02_structured_bindings.cpp` | C++17 |
| `examples/03_ranges_pipelines.cpp` | Ranges deep |
| `examples/04_coroutines_generator.cpp` | Ek generator |
| `examples/05_modules_demo/` | Modules example (multi-file) |
| `examples/06_spaceship.cpp` | `<=>` demo |
| `examples/07_format_print.cpp` | C++20/23 formatting |
| `examples/08_legacy_to_modern.cpp` | Refactoring before/after |

## Time
3–4 hafte

## Status
✅ **COMPLETE (Batch 7 — PHASE 12).** 15 lessons (`01`–`15`) + `16-exercises.md`
+ 10 examples (9 `.cpp` + the multi-file `05_modules_demo/`). Sab `.cpp` `-Wall
-Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion` pe clean (`./build.ps1
folder 22-MODERN-CPP`); modules demo `-fmodules-ts` pe separately verified.

**Deepening (gap fix, GCC 16.2):** `15-cpp23-in-practice.md` + do C++23 examples
(`09_cpp23_in_practice.cpp23.cpp`, `10_cpp23_what_happens_next.cpp23.cpp`). `*.cpp23.cpp`
files ko `build.ps1`/`Makefile` apne aap `-std=c++23 -lstdc++exp` se build karte hain.

- `01_lambdas_all`: `sizeof([x1,x2]{})`=16, `sizeof([]{})`=1, `sizeof(std::function)`=32.
- `03_ranges_pipelines` (5M int64): ranges pipeline ~6.5 ms vs hand loop ~10 ms
  — **pipeline ~1.5× FASTER** (hand loop's conditional accumulate doesn't
  vectorize; pipeline → branchless masked SIMD). Surprising-but-real, taught not
  hidden.
- `05_modules_demo/`: working `export module` / `import` (GCC 15 `-fmodules-ts`).

**Coverage:** systematic standard-by-standard audit — C++11 (move, `auto`,
lambdas, `nullptr`, smart pointers, threads) · C++14 (generic lambdas,
`make_unique`, relaxed `constexpr`) · C++17 (`optional`/`variant`/`string_view`,
structured bindings, `if constexpr`, guaranteed copy elision) · C++20 (concepts,
ranges, coroutines, modules, `<=>`, designated init, `<bit>`, `std::format`) ·
C++23 (`std::expected`, `std::print`, `std::generator`, deducing `this`, `flat_map`).
Deep dives: lambdas · `constexpr`/`consteval`/`constinit` · ranges · coroutines ·
modules · `<=>` · attributes · `std::format` · a legacy→modern migration guide.

## Next
→ [`../23-ERROR-HANDLING/00-README.md`](../23-ERROR-HANDLING/00-README.md)
