# 07 — LOOPS (PHASE 3)

## Prerequisites
`06-CONDITIONS`

## Yeh folder kyun
Programming ka chautha building block — **repetition**.

Loops ke saath cache locality bhi shuru hoti hai — jo poore performance track ka
foundation hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-while-loop.md` | `while`, loop condition, infinite loops, jab tak condition sach |
| 02 | `02-do-while-loop.md` | `do-while`, kam se kam ek baar, `;` kahan lagta hai |
| 03 | `03-for-loop.md` | `for` ka anatomy — init, condition, increment; scope of loop variable |
| 04 | `04-range-based-for.md` | `for (auto& x : container)` — C++11, aur `auto` vs `auto&` vs `const auto&` |
| 05 | `05-nested-loops.md` | Nested loops, complexity, 2D traversal |
| 06 | `06-break-continue.md` | `break`, `continue`, labelled break ka absence, `goto` kyun nahi |
| 07 | `07-loop-bugs.md` | Off-by-one, infinite loops, unsigned underflow, iterator invalidation preview |
| 08 | `08-loop-patterns.md` | Accumulator, search, filter, min/max, two-pointer intro |
| 09 | `09-loop-performance.md` | **Cache locality**, row-major vs column-major, loop unrolling intro, vectorization preview |
| 10 | `10-exercises.md` | Practice + pattern printing + performance experiments |

## Examples

| File | Kya |
|---|---|
| `examples/01_loop_types.cpp` | while/do-while/for comparison |
| `examples/02_range_based.cpp` | Range-for aur copy traps |
| `examples/03_nested_patterns.cpp` | Pattern printing |
| `examples/04_loop_bugs.cpp` | Off-by-one aur infinite loop bugs |
| `examples/05_cache_locality.cpp` | Row vs column traversal — measured |
| `examples/06_loop_unroll.cpp` | Manual unrolling vs compiler |

## Time
5–7 din

## Status
✅ **COMPLETE** (Batch 3). 9 lessons + exercises + 6 compile-verified examples.
Measured benchmarks: range-`for` copy cost ~50x (`02`), cache locality row vs
column ~8x (`05`), loop unroll flag-dependent (`06`) — sab GCC 15.1 pe real numbers.

## Next
→ [`../08-FUNCTIONS/00-README.md`](../08-FUNCTIONS/00-README.md)
