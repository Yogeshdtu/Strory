# Examples — Folder 47 (Coding problems)

10 **runnable reference solutions**, ek har category se. Har file: clean
implementation + assertion-based `main()` jo chalti hai + complexity notes +
"interviewer kya dekh raha" comment. Sab strict warnings ke neeche compile
hoti hain:

```bash
./build.ps1 folder 47-CODING-PROBLEMS        # 10/10 OK
```

Yeh files problem statements ki jagah nahi leti — woh `../01`…`../10` mein hain,
aur long-form solutions `../11-solutions/` mein. Yeh **verified, working code** ka
reference hai jab tumhara version compile na ho ya galat answer de.

---

| File | Problem (source) | Kya test karta |
|---|---|---|
| `01_basics_kata.cpp` | `01` A3/B7/B8/B12 | reverse-int overflow guard (check **before** the multiply), `nCr` multiply-then-divide, exact `isqrt`, `powmod` with `__uint128_t` |
| `02_arrays_strings_kata.cpp` | `02` B1/B12/C1/C3 | product-except-self (prefix+suffix), subarray-sum-k (prefix+hashmap, why sliding window fails), first-missing-positive (index-as-hash, O(1) space), trap-rain-water (two-pointer) |
| `03_arena_and_pool.cpp` | `03` B1/B2 | bump allocator (align-up, O(1) reset), fixed-size pool (intrusive free list, placement-new, exhaustion→nullptr, LIFO reuse) |
| `04_scope_guard_and_result.cpp` | `04` B8, `04` C5 | `ScopeGuard` (dismiss, move transfers duty), `Result<T,E>` (tagged union, no heap/exceptions, `map` passes errors through) |
| `05_flat_map.cpp` | `05` C1 | `std::map` API over a sorted `vector` — O(log n) find, ordered iteration, contiguous storage; O(n) insert cost |
| `06_crtp_and_detect.cpp` | `06` B3/B8 | CRTP (no vtable — `sizeof` proves it; the base-pointer stride trap), capability detection (`requires` vs `void_t`) |
| `07_blocking_queue.cpp` | `07` B1 | bounded blocking queue (mutex + 2 CVs, predicate wait, `close()`); N-producer/M-consumer checksum balances |
| `08_seqlock.cpp` | `08` #7 | seqlock — 1 writer / 3 readers, 2M writes, **torn reads = 0**; odd/even sequence, acquire/release fences |
| `09_optimize_row_vs_col.cpp` | `09` #1 | row-major vs column-major sum; **measured ~45–70× at `-O2`** (Rule 2: bigger than the textbook 7× — cache + TLB + vectorization stack) |
| `10_order_book_ops.cpp` | `10` #3/#4/#5 | price-indexed book: O(1) add/cancel + cached BBO + bounded rewalk, `match()` by price priority emitting fills, `crossed()` invariant |

## Verified

- `./build.ps1 folder 47-CODING-PROBLEMS` → **10/10 OK** (strict: `-Wall -Wextra
  -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-align
  -Wnull-dereference -Wdouble-promotion`).
- Har binary chalti hai aur `"... ALL PASS"` print karti hai.
- `07`/`08` threaded — `-O2 -pthread` pe bhi run-verified (`08` at `-O2`
  specifically, taaki fences ka reordering-protection actually test ho).
- `09` ka benchmark `./build.ps1 fast 09_optimize_row_vs_col.cpp` se chalao —
  `-O0` pe benchmark meaningless.

## Aur examples

`46-INTERVIEW-PREP/examples/` mein 8 aur coding problems (reverse list, LRU,
lock-free SPSC ring, fixed-point price parse, object pool, L2 top-of-book, atoi
overflow-clamp, O(1) moving average) — same style, same verification.
