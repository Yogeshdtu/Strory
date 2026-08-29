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
| 15 | `15-exercises.md` | Practice + refactoring exercises |

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
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../23-ERROR-HANDLING/00-README.md`](../23-ERROR-HANDLING/00-README.md)
