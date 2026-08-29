# 21 — TEMPLATES & GENERIC PROGRAMMING (PHASE 11)

## Prerequisites
`19-STL`, `18-COPY-MOVE`

## Yeh folder kyun
Templates hi C++ ki **zero-cost abstraction** ka mechanism hain. HFT mein virtual
functions ki jagah templates use hote hain — kyunki woh compile time pe resolve ho
jaate hain.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-why-templates.md` | Code duplication ka problem, generic programming ka idea |
| 02 | `02-function-templates.md` | Function templates, deduction, explicit instantiation |
| 03 | `03-class-templates.md` | Class templates, member functions, CTAD (C++17) |
| 04 | `04-template-parameters.md` | Type params, **non-type params**, template template params |
| 05 | `05-specialization.md` | Full specialization, **partial specialization** |
| 06 | `06-variadic-templates.md` | Parameter packs, recursion, **fold expressions** (C++17) |
| 07 | `07-if-constexpr.md` | `if constexpr` — compile-time branching |
| 08 | `08-type-traits-deep.md` | `<type_traits>` poora, apne traits likhna |
| 09 | `09-sfinae.md` | **SFINAE**, `enable_if`, detection idiom |
| 10 | `10-concepts.md` | **Concepts (C++20)** — `requires`, constraints, SFINAE ka replacement |
| 11 | `11-crtp-deep.md` | CRTP, static polymorphism, mixins |
| 12 | `12-tag-dispatch.md` | Tag dispatch, overload-based dispatch |
| 13 | `13-template-metaprogramming.md` | TMP, compile-time computation, type lists |
| 14 | `14-two-phase-lookup.md` | Two-phase lookup, `typename`/`template` disambiguators |
| 15 | `15-instantiation-and-bloat.md` | Instantiation model, **code bloat**, compile times, `extern template` |
| 16 | `16-templates-in-hft.md` | **Zero-cost abstraction in practice** — compile-time dispatch, policy classes |
| 17 | `17-exercises.md` | Practice + template puzzles |

## Examples

| File | Kya |
|---|---|
| `examples/01_function_templates.cpp` | Basics |
| `examples/02_class_templates.cpp` | Generic container |
| `examples/03_variadic.cpp` | Parameter packs + fold expressions |
| `examples/04_if_constexpr.cpp` | Compile-time branching |
| `examples/05_sfinae.cpp` | SFINAE aur enable_if |
| `examples/06_concepts.cpp` | C++20 concepts |
| `examples/07_crtp_policy.cpp` | CRTP + policy-based design |
| `examples/08_compile_time_dispatch.cpp` | Virtual vs template dispatch — assembly comparison |

## Time
3–4 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../22-MODERN-CPP/00-README.md`](../22-MODERN-CPP/00-README.md)
