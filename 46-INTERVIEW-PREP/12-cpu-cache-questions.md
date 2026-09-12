# 12 — Layer 11: CPU (pipelines, branch prediction, cache, false sharing)

## Prerequisites
Folders `31-CPU-ARCHITECTURE`, `32-CACHE-MEMORY-PERFORMANCE`. This is
the "latency intuition" filter — know the numbers.

---

## A — The numbers (memorize these)

### A1. Rough latencies: register, L1, L2, L3, DRAM, branch mispredict,
syscall, SSD, network.
<details><summary>Answer</summary>
Order of magnitude (modern x86-64): register ~0; **L1 ~1 ns** (~4
cycles); **L2 ~4 ns** (~12 cyc); **L3 ~10–20 ns** (~40 cyc); **DRAM
~60–100 ns**; **branch mispredict ~3–5 ns** (~15–20 cyc pipeline flush);
**syscall ~0.1–0.3 µs**; context switch ~1–3 µs (+ cold cache);
**SSD read ~10–100 µs**; same-datacenter network RTT ~10–100 µs; kernel-
bypass wire-to-app ~1 µs. (`31`, `32`, `37/14`.)
</details>

### A2. Cache line size, and why it's the unit that matters.
<details><summary>Answer</summary>
64 bytes on mainstream x86-64 (and Apple M-series is 128). Memory moves
between cache levels a **whole line** at a time. So: reading 1 byte
brings in 64; touching two hot variables in the same line couples them
(false sharing); a struct that straddles a line boundary costs two line
fills. Design data in 64-byte units. (`32/01-03`, `43/08`.)
</details>

### A3. 3 C's of cache misses.
<details><summary>Answer</summary>
**Compulsory** (first access to a line — unavoidable, but prefetch/blocking
help), **Capacity** (working set > cache size — reduce data / tile),
**Conflict** (too many hot lines map to the same set — critical-stride /
power-of-two-dimension poison; pad / change layout). Plus **Coherence**
misses (a line you had was invalidated by another core's write). (`32/`.)
</details>

---

## B — Pipeline & branch prediction

### B1. Branch predictor — what happens on a mispredict.
<details><summary>Answer</summary>
The CPU speculatively executes down the predicted path. On a mispredict:
flush the ~15–20 in-flight instructions in the pipeline, restart fetch
from the correct target → ~15–20 cycle bubble (~5 ns). Modern predictors
(TAGE/perceptron) are ~95–99% accurate on predictable branches → nearly
free; a genuinely random 50/50 branch in a hot loop is expensive.
(`31/04`, measured ~6–7× in `31` ex 03.)
</details>

### B2. Sorted vs unsorted array — the classic benchmark, why.
<details><summary>Answer</summary>
Summing only elements `> threshold`: on a **sorted** array the `if`
becomes a long run of falses then a long run of trues → predictor nails
it → fast. On an **unsorted** array it's ~random → constant mispredicts →
several× slower. The fix is often **branchless** (`sum += (x > t) * x` or
a `cmov`) so there's no branch to mispredict — but only for genuinely
unpredictable branches (`31/07`, `36/06`, `45/12` B).
</details>

### B3. Branchless code — always faster?
<details><summary>Answer</summary>
No. Branchless (`cmov` / mask / arithmetic select) computes **both**
sides and has a longer dependency chain. For a **predictable** branch
(99% one way), the branch is ~free and branchless is *slower*. Branchless
wins only when the branch is genuinely unpredictable. Measure. (`31/07-08`,
`45/12` C.)
</details>

### B4. `[[likely]]` / `[[unlikely]]` / `__builtin_expect` — what do they
actually do?
<details><summary>Answer</summary>
They hint **code layout**, not the hardware predictor: the compiler puts
the likely path fall-through (contiguous, cache-friendly) and the
unlikely path out of line (`.text.unlikely`). Measured ~1.3× on a
frontend-bound loop (`33`); **PGO does this better** from real profiles.
On a modern predictor the runtime prediction is unaffected. (`33/`,
`43/04-05`.)
</details>

### B5. ILP / superscalar / out-of-order — one sentence each.
<details><summary>Answer</summary>
**Superscalar** — multiple execution units, several instructions issued
per cycle. **ILP** (instruction-level parallelism) — independent
instructions the CPU can run in parallel; a long dependency chain kills
it. **Out-of-order** — the CPU reorders independent instructions around a
stalled one (e.g. a cache miss) to keep units busy — but it can't hide a
**pointer-chase** (each load depends on the previous). (`31/`.)
</details>

---

## C — Cache-friendly design

### C1. Row-major vs column-major traversal — expected difference.
<details><summary>Answer</summary>
For a row-major 2D array, iterating rows-then-columns walks memory
sequentially (each cache line fully used, prefetcher happy).
Columns-then-rows strides by row-length → a new line per element,
prefetch defeated → measured ~10× slower for a large matrix (`32` ex
03). "Iterate in memory order."
</details>

### C2. AoS vs SoA — when each wins.
<details><summary>Answer</summary>
**AoS** (`struct Order{...}; vector<Order>`) — wins for **random access
to whole records** (one line brings the whole object). **SoA**
(`struct { vector<price>; vector<qty>; ... }`) — wins for **scanning one
or a few fields across many records** (no wasted bytes per line, and it
vectorizes). Measured: scan-few SoA ~2×, random-whole-record AoS ~3×
(`32/09-10`, `43/07`). "Group by what you do together."
</details>

### C3. Data prefetching — hardware vs software.
<details><summary>Answer</summary>
**Hardware** prefetchers detect sequential / strided patterns and pull
lines ahead automatically — free if your access pattern is regular.
**Software** (`__builtin_prefetch(addr, rw, locality)`) — for
irregular-but-predictable patterns (e.g. you know the next node's address
a few iterations early in a hash probe). Easy to get wrong (too early =
evicted, too late = no help); measure. (`32/`, `43`.)
</details>

### C4. TLB and huge pages.
<details><summary>Answer</summary>
The TLB caches virtual→physical translations. A miss → a page-table walk
(up to 4 dependent memory accesses). L1 dTLB covers only ~256 KiB with 4
KiB pages; a large working set thrashes it. **2 MiB huge pages** → 512×
the reach per entry → far fewer walks. HFT: explicit hugetlbfs +
pre-fault + `mlock` (THP's `khugepaged` adds jitter). (`32/11`.)
</details>

---

## D — False sharing & coherence

### D1. False sharing — definition + fix + detection.
<details><summary>Answer</summary>
Two threads write **different** variables that happen to share a 64-byte
cache line → each write invalidates the other core's copy → the line
ping-pongs between cores (coherence traffic), 10–100× slower on that
access though the program is correct. **Fix:** `alignas(64)` + padding so
each hot per-thread variable owns its line. **Detect:** `perf c2c`
(HITM events on one line, different offsets). (`43/08`, `45/12` B9.)
</details>

### D2. MESI (cache coherence) — enough to explain false sharing.
<details><summary>Answer</summary>
Each cache line in each core is Modified / Exclusive / Shared / Invalid.
A core wanting to **write** must get the line **Exclusive** — it sends an
invalidate to all other cores holding it (they go to Invalid). If two
cores keep writing the same line, they keep stealing Exclusive from each
other → the ping-pong. Reads can share (Shared) cheaply; it's the
concurrent **writes** that hurt. (`31`, `27`.)
</details>

### D3. `std::hardware_destructive_interference_size` — what and caveat.
<details><summary>Answer</summary>
A `constexpr size_t` meant to be "the cache line size to separate hot
variables" (and `..._constructive_..._size` for "keep together"). Caveat:
some toolchains warn it's not ABI-stable / give a conservative value
(often 64, sometimes 128 for x86 due to the L2 spatial prefetcher pulling
line pairs). Many HFT codebases just `#define CACHELINE 64` and assert.
(`43/08`, `32/`.)
</details>

### D4. Why can out-of-order execution not hide a linked-list traversal?
<details><summary>Answer</summary>
Each `node = node->next` **depends** on the previous load's result — a
serial dependency chain. The CPU can't start load N+1 until load N
returns, and each is a likely cache miss (~60–100 ns). No ILP to exploit.
This is why flat arrays / indices beat pointer structures for hot
traversal — the addresses are known ahead, loads run in parallel, the
prefetcher engages. (`31/`, `32/`, `20`.)
</details>

---

## Interview tips for Layer 11

- Know the latency numbers cold (A1) — you'll use them in design rounds
  and "estimate the latency of X" questions.
- Branch prediction: "mispredict ≈ 15–20 cycle flush; predictable
  branches are ~free; branchless only helps unpredictable ones."
- False sharing: definition + `alignas(64)` fix + `perf c2c` detection.
- "Iterate in memory order", "group data by what you access together"
  (AoS vs SoA), "pointer chasing can't be pipelined."

## Next
→ [`13-performance-questions.md`](13-performance-questions.md)
