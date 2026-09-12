# Examples — Folder 36 (ultra-low-latency C++)

> Portable `.cpp` — sab MinGW/Windows x86-64 pe chalte, Linux nahi chahiye.
> **`-O2` mandatory** (`-O0` benchmarks bekaar — 35/09). `./build.ps1 folder
> 36-LOW-LATENCY-CPP` → **12/12 OK** under strict flags.

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz effective, **Windows x64 + MinGW-w64
GCC 15.1.0**. Plain `-O2` = **SSE2 baseline**. TSC ~2.0 GHz
(`ticks_per_ns ≈ 1.996`). **Quote the RATIOS / shapes.**

**⚠️ `max` on the per-op-timed loops (01–05, 09, 11) is OS-interrupt noise**
— this is an unpinned Windows desktop, so a scheduler tick (~64 µs every
~1–15 ms) lands on the per-op `rdtsc()` window somewhere in a 200k-sample
run every time. The **p50 / p99 / p99.9** are the real signal (folder 35/06);
a pinned Linux isolated core would have a tight `max` too. This contrast is
itself a lesson (folder 36 lessons 03, 19).

## Compile / run

```bash
./build.ps1 fast 36-LOW-LATENCY-CPP/examples/01_allocation_cost.cpp
# ... 02 .. 12 likewise
./build.ps1 asm  36-LOW-LATENCY-CPP/examples/06_branchless.cpp     # cmov vs jg
./build.ps1 asm  36-LOW-LATENCY-CPP/examples/12_hot_cold_split.cpp # cold code placement
```

## Examples

| File | Lesson(s) | Kya dikhata / measured (this box, ~2 GHz) |
|---|---|---|
| `01_allocation_cost.cpp` | 04 | `new`/`delete` **latency distribution** for 3 workloads. **new[64] p50 40 / p99.9 120 ns; new[64 KB] p50 90 / p99.9 160; mixed 8–8192 B churn p50 80 / p99 561 / p99.9 2585 ns** — the tail is the disaster, and mixed-size churn is the worst (fragmentation + size-class thrash). Lists the "hidden" allocations. |
| `02_memory_pool.cpp` | 06 | Builds a `FixedPool` (intrusive free list). Same churn pattern: **`new[64]` p50 70 / p99.9 180 ns vs `FixedPool::allocate` p50 20 / p99 20 / p99.9 30 ns** — the pool tail is **flat** (no `mmap`/coalesce/size-class/lock). Trade-offs: one size, fixed cap → overflow policy, per-thread. |
| `03_object_pool.cpp` | 07 | `ObjectPool<Order, Cap>`, two strategies. **`new Order()` p50 50 ns; pool + construct p50 30 (ctor cost visible); pool + recycle + reset p50 20 ns** — recycle skips the ctor, does a cheap explicit reset instead. Recycle danger: stale fields. |
| `04_arena_allocator.cpp` | 08 | Bump `Arena` — `allocate` = align+bump, `reset` = one store. 12 temps/message: **new/delete per msg p50 501 ns vs arena + reset p50 30 ns (~16×)**, p99.9 flat. Trade-offs: no individual free, non-trivial dtors, overflow policy, escaped pointers = UAF. |
| `05_pmr_containers.cpp` | 08, 09 | `std::pmr::monotonic_buffer_resource` on a stack buffer + `null_memory_resource` upstream. **`std::vector<int>` p50 270 ns (every message hits the heap) vs `pmr::vector` on stack buffer p50 40 ns, 0 global `new` over 200k messages.** The `null_memory_resource` guard throws `std::bad_alloc` on an unexpected allocation — a runtime tripwire. |
| `06_branchless.cpp` | 12 | Branch vs branchless, RANDOM vs SORTED, + switch vs lookup table. **RANDOM: mask/ternary ~0.26 vs branchy ~0.32 ns/elem** (mask SIMD-ized). **SORTED: all ~same as their RANDOM number** → `-O2` **if-converted the "branchy" version too** (no real branch; `./build.ps1 asm` → `cmovg`). **switch vs table: ~12× (3.99 vs 0.34)** on random data — the switch's jump table is an unpredictable indirect jump. |
| `07_dispatch_comparison.cpp` | 13 | 5 mechanisms, HOMO vs HETERO data, 4M calls. **HOMO**: virtual 2.48 / variant 0.72 / fn-table 1.27 / switch 0.77 / CRTP 0.60 ns. **HETERO**: virtual **7.02** / variant 4.66 / fn-table 6.18 / switch 4.53 / CRTP **0.59** ns. Heterogeneous data wrecks the indirect-call predictor; `variant`/`switch` degrade less (inlined body); CRTP is flat (monomorphic). |
| `08_std_function_cost.cpp` | 14 | templated / raw-fn-ptr / `function_ref` **all ~1.50 ns/call** (the loop is *latency-bound* on a carried hash → OoO hides the call overhead — Rule 2); **`std::function` ~3.13 ns (~2×)**; a 64-byte capture → **`[ctor heap bytes: 64]`** (SBO overflow → `operator new` in the ctor, proven with a global `new` counter). Hand-rolled `function_ref` shown (2 words, no alloc ever). |
| `09_ring_buffer.cpp` | 15 | SPSC ring: power-of-2 `& mask`, `alignas(CL)` on `head_`/`tail_`/`buf_`, **cached opposite index**. **try_push / try_pop p50 20 ns, p99/p99.9 30 ns** — a few ns: one relaxed load, mask, store, release-store. No `%`, no branch, no alloc, no lock. |
| `10_batching.cpp` | 16 | Per-item vs batches of B, the throughput/latency curve. **B=1: 6.71 ns/item, 149 M/s, HoL 6.7 ns → B=1024: 1.51 ns/item, 661 M/s, HoL 3095 ns.** The amortization knee is ~B=16–32; head-of-line latency grows ~linearly with B. |
| `11_page_fault_warmup.cpp` | 18 | COLD first-write vs WARM re-write of a 78 MB buffer. **COLD mean 271 / p99 2875 / p99.9 3246 ns vs WARM mean 21 / p99 30 / p99.9 240 ns** — ~13× on the mean, ~95× on p99. Every new page = a minor fault. Prints the Linux (`MAP_POPULATE`/`mlockall`) + Windows (`VirtualLock`) hardening. |
| `12_hot_cold_split.cpp` | 21 | A (cold body inline) / B (plain fn) / C (`[[gnu::cold]] [[gnu::noinline]]` + `[[unlikely]]`), cold path ~1/10000. **All ~1.49 ns/iter — no measurable difference** (Rule 2 — kept, not hidden). The hot loop already fits L1i, so layout didn't matter here; the payoff needs a genuinely I-cache-bound hot path (`perf` → Frontend Bound) where split + PGO can be 10–30%. |

## Notes / jaan-boojh kar cheezein

- **`keep()` / `volatile` sinks in every timing example** — without them
  `-O2` deletes the loop (35/09). The latency-distribution examples time
  each op with a fenced `rdtsc()` (~18 ns self-cost) and sort the samples
  for percentiles — fine for p50/p99/p99.9 of a tens-to-hundreds-of-ns
  operation; **`max` catches an OS tick and is not the allocator/pool
  signal**.
- **`06_branchless.cpp` — Rule 2**: the "branchy" `if (a[i] > t) s += a[i]`
  gave *identical* sorted vs unsorted times → `-O2` if-converted it to
  `cmov`, so there was no branch to mispredict. This is the taught lesson
  (folder 33/08): always check the asm before attributing a result to
  branch prediction.
- **`08_std_function_cost.cpp` — Rule 2**: templated / fn-ptr / `function_ref`
  tie at ~1.5 ns *because the loop is latency-bound* on a carried hash; a
  throughput loop would let `templated` pull ahead (inline → vectorize).
  What the loop is bound by decides whether inlining matters.
- **`12_hot_cold_split.cpp` — Rule 2**: A/B/C measured **no difference** —
  the micro-bench's hot loop fits L1i, so hot/cold layout had nothing to
  fix. Kept in the folder deliberately: a technique that "should" help
  showing nothing in a given context is the norm; measure in situ (35/11).
- **`02_memory_pool` `FixedPool` uses `std::memcpy` for the free-list
  pointer read/write** — avoids a strict-aliasing question; `-O2` turns it
  into a single `mov`.
- **The churn loops (01–03) keep `live.size()` at a fixed `TARGET`** (free
  one, alloc one) so the pools never overflow and the allocator sees a
  realistic steady state.
- **`11_page_fault_warmup` is portable** — the Linux `mmap(MAP_POPULATE)` /
  `mlockall` and Windows `VirtualLock` calls are shown in `puts` output as
  guidance; the measured core (cold vs warm first-touch) runs everywhere.
- **No `broken_on_purpose` file.** `06` contains "buggy benchmark"-style
  contrasts but every `.cpp` compiles and runs.
- The 12 `.cpp` compile clean under `-std=c++20 -Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wnull-dereference
  -Wdouble-promotion`.
