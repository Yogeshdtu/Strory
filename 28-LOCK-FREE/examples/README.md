# Examples — Folder 28 (Lock-free programming)

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_spsc_ring_buffer.cpp` | 04 | **SPSC bounded ring**, power-of-two, `head_` (producer, release) / `tail_` (consumer, release), each side `acquire`-loads the other. No CAS, no mutex. 20 M msgs, ordering + no-loss checksum verified. Naive layout: **~19 M msg/s / ~52 ns/msg** at `-O2` |
| `02_spsc_optimized.cpp` | 05, 03 | **Optimization ladder**, same queue three ways: V0 naive → V1 +cache-line padding → V2 +cached opposite index. **Measured: V0 ~25–27 ns/msg, V1 ~24–32 ns/msg (padding alone ≈ noise / sometimes worse — producer still reloads `tail_` every push), V2 ~16–20 ns/msg (~1.3–1.6× — the real win).** CLAUDE.md Rule 2 result |
| `03_mpmc_queue.cpp` | 06, 02, 13 | **Vyukov bounded MPMC** — per-cell atomic `seq` turnstile, monotonic positions (no ABA), no allocation, `relaxed` position CAS + `release`/`acquire` cell `seq`. 3 producers + 3 consumers, 6 M items, sum/count invariant OK. **~6.5–7.5 M op/s vs `mutex+deque` ~3.9–4.3 M op/s (~1.5–1.75×)** |
| `04_lock_free_stack.cpp` | 08, 14, 09 | **Treiber stack** over a fixed `Node` pool, **ABA-safe** via a packed `{idx:32, tag:32}` `atomic<uint64_t>` head (tag++ every push). Multiset-of-popped == pushed verified, no ABA. ⚠️ **Negative result: ~7 M op/s vs `mutex+vector` ~40 M op/s → lock-free is ~5× SLOWER** (retry storm on one hot head; cache-friendly baseline). Lock-free ≠ fast |
| `05_seqlock.cpp` | 12, 11 | **Seqlock** — 1 writer (odd/even `seq`, release fences) + 4 readers (two `seq` reads bracketing the copy, `acquire` fence). 40-byte `Quote` (too big for a lock-free atomic). **torn = 0** (no half-old/half-new snapshot ever accepted). **~180 M reads/s vs `std::shared_mutex` ~2 M reads/s (~80–100×)**; retries ~600–850% because the writer spins flat-out (realistic cadence → ~0) |
| `06_mutex_vs_lockfree.cpp` | 14, 01, 05 | **The fair fight**: SPSC hand-off 3 ways — A `mutex+queue`, B `mutex+condition_variable`, C lock-free SPSC ring. Throughput (blast) + **paced per-message latency** (p50/p99/p99.9). **A p50 ~1.2 µs / 2.4 M/s; B p50 ~6 µs / 3.5 M/s (futex wake); C p50 ~0.4 µs / 15 M/s.** p99 tails OS-jitter-dominated on unpinned Windows. Here lock-free clearly wins (contrast `04`) |
| `07_false_sharing_fix.cpp` | 03 | **Pure false sharing** in an SPSC index pair — 2 threads, each only `fetch_add`s its own atomic (RFO/op, never reads the other → 100% false sharing). **ADJACENT (same line) ~24 ns/op vs PADDED `alignas(64)` ~6 ns/op → ~3.5–4×.** `std::hardware_destructive_interference_size` = 64 on this box (with `__cpp_lib_hardware_interference_size` guard) |

## Compile / run

```bash
./build.ps1 28-LOCK-FREE/examples/01_spsc_ring_buffer.cpp     # Windows (MinGW: -pthread optional)
make FILE=28-LOCK-FREE/examples/01_spsc_ring_buffer.cpp        # Linux/Mac/Git-Bash (-pthread)
```

Benchmarks at **`-O2`** (required — `-O0` hides both reordering and contention
behind loop overhead):
```bash
./build.ps1 fast 28-LOCK-FREE/examples/02_spsc_optimized.cpp
./build.ps1 fast 28-LOCK-FREE/examples/04_lock_free_stack.cpp
./build.ps1 fast 28-LOCK-FREE/examples/05_seqlock.cpp
./build.ps1 fast 28-LOCK-FREE/examples/06_mutex_vs_lockfree.cpp
./build.ps1 fast 28-LOCK-FREE/examples/07_false_sharing_fix.cpp
```

Whole folder:
```bash
./build.ps1 folder 28-LOCK-FREE       # 7/7 OK
```

**Race detection** (Linux — MinGW has no libtsan):
```bash
g++ -std=c++20 -O1 -g -fsanitize=thread 28-LOCK-FREE/examples/03_mpmc_queue.cpp -o mpmc_tsan && ./mpmc_tsan
```

## Measured (GCC 15.1.0, `-O2`, 8 logical cores, this box) — sample runs

### `01_spsc_ring_buffer.cpp` (naive layout)
```
20 M messages, cap 4096, 1 producer + 1 consumer
  throughput : ~19 M msg/s
  per message: ~52 ns
  checksum   : OK
```

### `02_spsc_optimized.cpp` (30 M msgs) — the ladder
```
  V0 naive         : ~25-27 ns/msg
  V1 +padding      : ~24-32 ns/msg   <- padding ALONE ~= noise (producer still reloads tail_)
  V2 +cached idx   : ~16-20 ns/msg   <- ~1.3-1.6x over V0, the real win
```

### `03_mpmc_queue.cpp` (3P + 3C, 6 M items)
```
  lock-free (Vyukov) : ~6.5-7.5 M op/s
  mutex + std::deque : ~3.9-4.3 M op/s
  ratio              : ~1.5-1.75x    checksum OK
```

### `04_lock_free_stack.cpp` (4 threads x (1.5 M push + 1.5 M pop))  ⚠️ lock-free LOSES
```
  lock-free (tagged) : ~7 M op/s
  mutex + vector     : ~40 M op/s
  ratio              : ~0.18x   (lock-free ~5x SLOWER — retry storm on one head)
  correctness        : OK, no ABA
```

### `05_seqlock.cpp` (1 writer max-rate + 4 readers x 5 M reads, Quote = 40 B)
```
  seqlock       : ~180 M reads/s   torn=0   retries ~600-850% (writer spinning flat-out)
  shared_mutex  : ~2 M reads/s     torn=0   retries 0%
  ratio         : ~80-100x
```

### `06_mutex_vs_lockfree.cpp` (SPSC hand-off, paced latency pass)
```
  A mutex+queue    : ~2.4 M msg/s   p50 ~1.2 us
  B mutex+cv       : ~3.5 M msg/s   p50 ~6 us     (futex wake per message)
  C lock-free SPSC : ~15  M msg/s   p50 ~0.4 us
  (p99/p99.9 all ~100-700 us — OS scheduler jitter, threads not pinned)
```

### `07_false_sharing_fix.cpp` (2 threads x 200 M fetch_add each)
```
  ADJACENT (same line)   : ~24 ns/op
  PADDED   (own lines)   :  ~6 ns/op
  speedup from padding   : ~3.5-4x
```

## Notes / jaan-boojh kar cheezein

- **No `broken_on_purpose` file.** Every example is a *correct* program that
  demonstrates a property: `01`/`03`/`05`/`06` verify an invariant (checksum /
  torn=0 / sum-count); `02` shows an optimization ladder with an anti-intuitive
  middle rung; `04` reproduces a **negative** result (lock-free losing to a
  mutex); `07` isolates false sharing.
- **CLAUDE.md Rule 2 results — measured, not assumed:**
  - `02` V1: cache-line padding of `head_`/`tail_` **alone** gave ~no speedup on
    this box (the producer still `acquire`-loads the constantly-written `tail_`
    every push, so padding doesn't remove the cross-core read). The **cached
    opposite index** (V2) is what moves the number. Padding is still necessary so
    the caches/indices don't collide — it's just not the bottleneck here.
  - `04`: a correct, ABA-safe lock-free Treiber stack is **~5× slower** than
    `mutex + std::vector` under 4-thread contention — one hot head + CAS loops =
    retry storm; the mutex baseline is tiny and cache-friendly. Lock-free is a
    progress guarantee, not a speed claim.
- **DWCAS (`cmpxchg16b` / `__atomic_*_16`) does not link on this MinGW.** `04`
  packs a 32-bit pool index + 32-bit tag into a `std::atomic<uint64_t>` for the
  ABA-safe head — lock-free everywhere, no 128-bit CAS needed.
- **TSan / UBSan not available on this MinGW.** The invariant checks (checksums,
  `torn == 0`, multiset equality) still catch a lot. On Linux, run the stress
  examples under `-fsanitize=thread` (command above).
- **`06` p99 tails** are dominated by OS scheduler jitter (threads not pinned,
  Windows desktop) — the meaningful comparison is **p50 hand-off + throughput**,
  where lock-free SPSC clearly leads.
- **`05` retry rate** is high (~600–850%) only because the writer spins at max
  rate; slow it to a realistic market-data cadence and the retry rate falls
  toward 0 while throughput stays ~the same.
- All 7 compile clean under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wsign-conversion -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion`
  (`./build.ps1 folder 28-LOCK-FREE`).
