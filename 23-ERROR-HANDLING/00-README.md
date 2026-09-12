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
✅ **COMPLETE (Batch 8 — PHASE 13).** 13 lessons (`01`–`13`) + `14-exercises.md`
+ 7 examples. Sab `.cpp` `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wsign-conversion -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion` pe
clean (`./build.ps1 folder 23-ERROR-HANDLING`).

- `04_exception_cost` (`-O2`, is box): happy path try/catch vs return-code
  **~1.0x** (zero-cost model); **~6000+ ns per throw+catch** vs ~1.5 ns return
  (**~4000x**); 0.1% error rate → try/catch ~8 ns/iter vs ~1.5 ns (**~5x**).
- `05_expected` — builds under **`-std=c++20`** (hand-rolled `Expected<T,E>`,
  same API) **and `-std=c++23`** (real `std::expected`); same output.
- `07_no_exceptions` — compiles + runs **with and without `-fno-exceptions`**
  (no `throw` in the file).
- `02_exception_safety` — leak-on-throw vs RAII rollback shown via a live-object
  counter, not a claim.

**Coverage:** error-handling landscape (return codes / `errno` / exceptions /
`expected` / `error_code` / assertions) · exceptions mechanics (`throw`/`try`/
`catch`, catch order, rethrow, `exception_ptr`) · **stack unwinding** (ctor-mid
throw, `noexcept` boundary → `terminate`, `-fno-exceptions` RAII) · **exception
safety** (basic/strong/nothrow, copy-and-swap, `noexcept` move + `vector`
realloc) · `noexcept` deep · custom exception hierarchy (`throw_with_nested`) ·
**measured exception cost** (zero-cost model + µs throw) · **`-fno-exceptions`**
(why HFT, what's lost, the toolkit) · **`std::expected`** (monadic
`and_then`/`transform`/`or_else`/`transform_error`) · `std::error_code` /
`error_condition` / custom categories · assertions / `static_assert` /
`std::unreachable` / `[[assume]]` / contracts · **UB catalog** + sanitizers.

## Next
→ [`../24-COMPILATION-LINKING/00-README.md`](../24-COMPILATION-LINKING/00-README.md)
