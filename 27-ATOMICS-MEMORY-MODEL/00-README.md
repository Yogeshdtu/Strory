# 27 — ATOMICS & MEMORY MODEL (PHASE 17)

## Prerequisites
`26-CONCURRENCY` (poora)

## Yeh folder kyun
**Yeh course ka sabse mushkil folder hai.**

Prerequisites: concurrency → data races → atomics → happens-before → memory ordering.
Isi order mein chalenge. Jaldi mat karna.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-data-race-definition.md` | **Formal definition** — do threads, ek memory location, ek write, no sync = UB |
| 02 | `02-atomicity.md` | Atomic kya hai, non-atomic operations kaise toot sakti hain |
| 03 | `03-std-atomic.md` | `std::atomic<T>`, supported operations, `is_lock_free` |
| 04 | `04-atomic-operations.md` | `load`, `store`, `exchange`, `fetch_add`, RMW operations |
| 05 | `05-compare-exchange.md` | **CAS** — `compare_exchange_weak` vs `strong`, spurious failure, CAS loops |
| 06 | `06-why-memory-ordering.md` | Compiler reordering + CPU reordering — kyun problem hai |
| 07 | `07-memory-order-relaxed.md` | `relaxed` — sirf atomicity, koi ordering nahi, counters ke liye |
| 08 | `08-acquire-release.md` | **`acquire`/`release`** — synchronizes-with, publish/subscribe pattern |
| 09 | `09-seq-cst.md` | `seq_cst` — total order, default, aur uski cost |
| 10 | `10-happens-before.md` | **happens-before, synchronizes-with, sequenced-before** — formal model |
| 11 | `11-fences.md` | `atomic_thread_fence`, standalone barriers, compiler barriers |
| 12 | `12-hardware-memory-models.md` | **x86 TSO vs ARM weak ordering** — same code, alag behaviour |
| 13 | `13-atomic-ref.md` | `std::atomic_ref` (C++20) |
| 14 | `14-lock-free-definitions.md` | Lock-free vs wait-free vs obstruction-free — exact meanings |
| 15 | `15-aba-problem.md` | **ABA problem**, tagged pointers, solutions |
| 16 | `16-litmus-tests.md` | Classic litmus tests — store buffering, message passing, IRIW |
| 17 | `17-exercises.md` | Practice + reasoning problems |

## Examples

| File | Kya |
|---|---|
| `examples/01_atomic_counter.cpp` | Atomic vs non-atomic counter |
| `examples/02_cas_loop.cpp` | CAS loop patterns |
| `examples/03_relaxed_ordering.cpp` | Relaxed counter |
| `examples/04_acquire_release.cpp` | Publish/subscribe pattern |
| `examples/05_reordering_demo.cpp` | Reordering ko observe karna |
| `examples/06_memory_order_bench.cpp` | relaxed vs acq_rel vs seq_cst — measured cost |
| `examples/07_aba_problem.cpp` | ABA reproduce karna |
| `examples/08_litmus_tests.cpp` | Store buffering, message passing |

## Time
3–4 hafte

## Status
✅ **COMPLETE (Batch 8 — PHASE 17).** 16 lessons (`01`–`16`) + `17-exercises.md` +
8 examples (`examples/`), sab compile-verified (`./build.ps1 folder
27-ATOMICS-MEMORY-MODEL` → 8/8 OK). Benchmarks (`05`, `06`) real measured numbers
ke saath — machine state ke hisaab se absolute ns badalte hain, ratios stable
(seq_cst store ~18× a relaxed store; loads/RMW order-insensitive on x86; SB
outcome rel/acq pe *zyada*, seq_cst pe hamesha 0).

## Next
→ [`../28-LOCK-FREE/00-README.md`](../28-LOCK-FREE/00-README.md)
