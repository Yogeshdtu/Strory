# 13 — Layer 12: profiling, optimization, latency vs throughput

## Prerequisites
Folders `35-PROFILING-BENCHMARKING`, `43-HFT-OPTIMIZATION`, `45/11`.
Often a live "optimize this code" round — the *process* is graded more
than a clever trick.

---

## A — Methodology

### A1. "Optimize this function" — your first 3 moves.
<details><summary>Answer</summary>
(1) **Measure a baseline** — a reproducible number (`-O2`, fixed input,
warm, p50/p99 not mean). (2) **Profile** — where does the time go?
(per-stage `rdtsc`, or `perf record`/`report`/`annotate`). Don't guess.
(3) **Form a hypothesis with a mechanism** — "the `std::map` node
allocation dominates; a flat array removes it" — then change **one**
thing and re-measure. Explain what changed. (`43/01`.)
</details>

### A2. Latency vs throughput — how do they trade off?
<details><summary>Answer</summary>
**Batching** amortizes a fixed per-op cost (syscall, lock, cache-cold
touch) over many items → higher **throughput**, but the first item waits
for the batch to fill → higher **latency** (head-of-line). Measured: B=1
~6.7 ns/item, 6.7 ns HoL → B=1024 ~1.5 ns/item, **~3 µs** HoL (`36/17`).
HFT optimizes **tail latency** (p99.9) and uses *opportunistic* batching
(take what's already queued, never wait). (`43`, `36`.)
</details>

### A3. Why p99.9, not mean?
<details><summary>Answer</summary>
The mean hides the tail. In HFT a trade that's late p99.9 of the time
still loses money 1-in-1000 — and at millions of events, that's constant.
Also **fan-out amplification**: if a request touches 100 services each
with a p99 spike, the overall p99 is much worse ("the tail at scale").
And **coordinated omission** — naive load tests under-count latency
because they stop sending while the system is stalled. Use HdrHistogram,
correct for it. (`35/04-05`.)
</details>

### A4. When do you *stop* optimizing?
<details><summary>Answer</summary>
When: you're at the **noise floor** (run-to-run variance ≥ the gain);
the requirement is met with headroom ("fast enough" is venue-defined);
the effort/payoff knee has passed (next 5% costs 2 weeks); or the change
adds risk (UB, unmaintainable hand-vectorized code, bus-factor). Opportunity
cost — that time could kill a bigger bottleneck elsewhere. (`43/16`.)
</details>

### A5. A change made it 5% faster on average but p99 got worse. Ship?
<details><summary>Answer</summary>
No — investigate first. A "5% mean" often hides one improvement plus a
regression that only shows in the tail (e.g. an occasional realloc, a
rare lock, a cold path now taken more). One concept per commit, track p50
**and** p99/p99.9, and keep a correctness gate. (`43/17` A2.)
</details>

---

## B — Reading a profiler

### B1. `perf stat`: IPC 0.3, LLC-load-misses huge, branch-misses low,
frontend fine. Diagnosis?
<details><summary>Answer</summary>
**Memory / backend bound.** Low IPC because the CPU is stalled waiting on
DRAM. Levers: struct layout / SoA (fewer bytes per element), cache
blocking, prefetch, preallocation (kill scattered heap), reduce working
set. PGO / branch hints would be the wrong lever here. (`43/03`, `43/17`
B1.)
</details>

### B2. `perf stat`: IPC 2.6 (good), branch-misses 9% (high), caches fine.
Diagnosis?
<details><summary>Answer</summary>
**Bad speculation** — branch mispredicts. Levers: hot/cold path split +
`[[likely]]`, branchless for the *genuinely unpredictable* branch,
data-driven `switch` → lookup table, sort/partition data so branches
become predictable. (`43/17` B2, `36/06`.)
</details>

### B3. `perf annotate`: 42% of samples on one `idiv`. Fix options.
<details><summary>Answer</summary>
Runtime integer division is ~20–40 cycles, not pipelined. Options
(`43/10`): make the divisor a compile-time constant (compiler → magic
multiply); power-of-two → shift; loop-invariant divisor → reciprocal
multiply; or **algebra it away** (`a/b > c` → `a > b*c`). Watch for a
*hidden* `%` in the loop (a known benchmark trap). (`43/17` B3.)
</details>

### B4. Profile is flat and clean but the service is slow / spiky.
<details><summary>Answer</summary>
The cost is **off-CPU** — `perf record` only samples running code. Use
`offcputime` (bcc/bpftrace) or `perf sched timehist`/`latency` to see
where threads **block** (futex, I/O, `nanosleep`), and `perf stat -e
page-faults,context-switches,cpu-migrations` for per-run misconfig
signals. (`45/11`.)
</details>

### B5. Micro-benchmark shows 10× but the full pipeline barely moves.
<details><summary>Answer</summary>
The optimized stage wasn't the bottleneck (**Amdahl**), or the
micro-bench didn't reflect reality — the input was hot in cache in
isolation but cold in the real workload, or a lookup table that fit L1
alone now evicts the working set. Always measure in the **full**
pipeline. (`43/17` C2, `43/15`.)
</details>

---

## C — Benchmarking correctly

### C1. Common benchmark mistakes — name 4.
<details><summary>Answer</summary>
(1) **`-O0`** — meaningless; use `-O2`+. (2) **Dead-code elimination** —
the compiler deletes work whose result is unused (`DoNotOptimize` /
`ClobberMemory`). (3) **Constant folding / hoisting** — a loop-invariant
computed once. (4) **One run** — report min-of-N or a distribution, not a
single sample; warm up first; watch timer overhead > the operation;
alignment/layout noise; frequency scaling (report cycles/op). (`35/01-04`.)
</details>

### C2. Why is a `-O0` benchmark worthless?
<details><summary>Answer</summary>
`-O0` keeps every variable in memory, does no inlining, no register
allocation, no vectorization — it measures the debug build, which is
~6–20× slower and has a completely different bottleneck profile than
what ships. HFT benchmarks are `-O2`/`-O3` (and note that `-O0` also
often *hides* concurrency bugs by changing timing). (`33/01`, `35`.)
</details>

### C3. A surprising benchmark result contradicts your expectation. What
do you do?
<details><summary>Answer</summary>
**Don't hide it — teach it.** It's usually the better lesson. Examples
from this course: `-O3` loop interchange came out *slower*; hot/cold path
splitting gave ~1% (frontend wasn't the bottleneck); a float reduction
refuses to vectorize at `-O2` without `-ffast-math`. Verify the
measurement is sound (Rule 2), then explain the mechanism. (`43/01`
Rule 2, `35`.)
</details>

### C4. `rdtsc` for timing — the pitfalls.
<details><summary>Answer</summary>
`rdtsc` counts **reference cycles**, not wall time and not core cycles —
convert with a calibrated ticks/ns factor. It can be reordered by the CPU
→ fence (`lfence; rdtsc; lfence` or `rdtscp`). It varies slightly per
core → pin the thread. Subtract the measurement's own cost (~20 ns
fenced). Needs an invariant TSC. (`34/11`, `35/03`.)
</details>

---

## D — HFT optimization patterns

### D1. Name 5 hot-path rules.
<details><summary>Answer</summary>
(1) **No allocation** — pools / arenas / `reserve`, pre-fault + `mlock`.
(2) **No syscalls** — batch, `io_uring`/bypass, `rdtsc` for time.
(3) **No locks** — shared-nothing, SPSC queues, seqlock.
(4) **Cache-friendly data** — flat arrays, SoA where scanning, hot/cold
split, 64-byte alignment, no false sharing.
(5) **No indirect calls** on the hot path — compile-time dispatch (CRTP /
templates / `variant`), devirtualize. Plus: branch-predictable or
branchless, fixed-point not float, measure everything. (`36`, `43`.)
</details>

### D2. "This 400-line hand-vectorized order-book scan is 1.4× faster."
Keep or revert?
<details><summary>Answer</summary>
Depends: what % of the hot path is it (1.4× on 5% = 2% global)? Is it
tested, documented, with a scalar fallback? Who maintains it? If it's
un-owned, un-portable, and a small slice → revert to the clear version;
bus-factor risk > 2%. If it's 40% of the hot path, well-tested, and
someone owns it → keep, but onboard a second engineer. (`43/17` E3.)
</details>

### D3. A change is 12% faster but relies on a strict-aliasing violation.
Review?
<details><summary>Answer</summary>
**Reject.** UB — it may silently produce wrong results on the next
compiler version / `-O3` / different inlining (wrong answer, not a
crash). 12% isn't worth a correctness gamble. `memcpy` / `std::bit_cast`
usually gets the same codegen legally — write that. (`43/17` E2, `25/10`,
`45/12` E8.)
</details>

### D4. Lookup table vs recompute — the trade-off.
<details><summary>Answer</summary>
A table replaces computation with a memory access. Wins if the compute is
expensive and the table stays in L1. **Loses** if the table is large
enough to evict the actual working set — then every table access *and*
every data access misses. A micro-bench with only the table in cache
lies. Measure in the full pipeline. (`43/11`, `43/17` C2.)
</details>

---

## Interview tips for Layer 12

- The **process** is the answer: baseline → profile → one hypothesis
  with a mechanism → change one → re-measure → explain. Say it every
  time.
- Map `perf stat` signals to problem classes (memory-bound / bad-spec /
  divide / off-CPU) and to fixes.
- p99.9 not mean; opportunistic batching; "fast enough is venue-defined."
- Never claim a speedup without a measured number — this is the whole
  ethos.
- A surprising result is a teaching opportunity, not something to bury.

## Next
→ [`14-hft-architecture-questions.md`](14-hft-architecture-questions.md)
