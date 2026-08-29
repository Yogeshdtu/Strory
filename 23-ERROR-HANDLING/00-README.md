# 23 — ERROR HANDLING (PHASE 13)

## Prerequisites
`22-MODERN-CPP`, `17-RAII`

## Yeh folder kyun
Errors handle karne ke kai tareeke hain. Aur HFT mein **exceptions aksar disabled**
hote hain — aapko pata hona chahiye kyun, aur alternative kya hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-error-handling-strategies.md` | Return codes, errno, exceptions, `expected`, assertions — poora landscape |
| 02 | `02-return-codes-and-errno.md` | C style, `errno`, kab abhi bhi dikhta hai |
| 03 | `03-exceptions-basics.md` | `throw`, `try`, `catch`, exception types |
| 04 | `04-stack-unwinding.md` | **Stack unwinding** — destructors chalte hain, RAII ka role |
| 05 | `05-exception-safety.md` | Basic / strong / nothrow guarantees, copy-and-swap idiom |
| 06 | `06-noexcept.md` | `noexcept`, `std::terminate`, move operations pe asar |
| 07 | `07-custom-exceptions.md` | Apni exception hierarchy, `std::exception` se derive |
| 08 | `08-exception-cost.md` | **Zero-cost model** — happy path free, throw path mehnga (measured) |
| 09 | `09-no-exceptions-hft.md` | **`-fno-exceptions`** — HFT mein kyun, kya kho jaata hai, alternatives |
| 10 | `10-std-expected.md` | **`std::expected` (C++23)** — exceptions ke bina error handling |
| 11 | `11-error-codes.md` | `std::error_code`, `error_condition`, custom categories |
| 12 | `12-assertions.md` | `assert`, `static_assert`, contracts, defensive programming |
| 13 | `13-undefined-behaviour.md` | **UB ka catalog** — kya UB hai, compiler kya assume karta hai |
| 14 | `14-exercises.md` | Practice + error handling design |

## Examples

| File | Kya |
|---|---|
| `examples/01_exceptions_basics.cpp` | throw/catch/unwinding |
| `examples/02_exception_safety.cpp` | Teenon guarantees |
| `examples/03_raii_unwinding.cpp` | Unwinding mein destructors |
| `examples/04_exception_cost.cpp` | Happy path vs throw path — measured |
| `examples/05_expected.cpp` | `std::expected` patterns |
| `examples/06_error_code.cpp` | `std::error_code` |
| `examples/07_no_exceptions.cpp` | `-fno-exceptions` build ke saath |

## Time
2 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../24-COMPILATION-LINKING/00-README.md`](../24-COMPILATION-LINKING/00-README.md)
