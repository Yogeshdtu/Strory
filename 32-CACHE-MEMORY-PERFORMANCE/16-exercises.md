# 16 — Exercises: cache & memory performance

## Prerequisites
- Poora folder 32 (`01`–`15`)

## Kaise use karein
- **Part A** — predict the behaviour / number, phir example chala ke verify.
- **Part B** — find-the-bug: har snippet mein ek cache/memory-level
  performance bug hai.
- **Part C** — reasoning: numbers se conclusion nikaalo.
- **Part D** — challenge: build + measure. **Real numbers likho** (CLAUDE.md
  Rule 2), aur agar compiler/hardware ne benchmark defeat kiya (jaisa examples
  `03` -O3, `06` prefetch, `07` blocking mein hua) to woh bhi likho — woh khud
  ek finding hai.

> Is repo ka box AMD Zen 2 ~2 GHz (throttled). **Ratios port karo, absolutes
> nahi.** `-O2` mandatory; `-O0` pe har number bekaar.

---

## Part A — Predict

### A1
```cpp
// 64 MiB int buffer, ek full pass:
for (i = 0; i < N; i += 1)   sum += a[i];    // version 1
for (i = 0; i < N; i += 16)  sum += a[i];    // version 2  (stride 64 B)
```
Per-access cost (ns) kaunsa zyada, aur kitne guna? (Example `01`/`02`.)

<details><summary>Answer</summary>

Version 2 per-access **~10× dheema** (~0.32 → ~3.3 ns is box). V1: har 64-B
line ke 16 accesses — ek miss, 15 hits, + prefetcher aage bhaagta → cost
amortized. V2: har access ek naya line = ek naya (mostly) miss. Total lines
touched dono mein same (64 MiB / 64 B), par V2 unhe 1/16 accesses mein
touch karta → per-access cost ~16× ka trend, prefetcher se ~10× rah jaata.
"Same data touched" ≠ "same time".
</details>

### A2
`double m[4096][4096]`. `for (i) for (j) s += m[i][j]` vs `for (j) for (i)
s += m[i][j]`. `-O2` pe ratio? `-O3 -march=native` pe?

<details><summary>Answer</summary>

`-O2`: column-major (`j` outer) **~10× slower** (example `03`: 0.25 vs 2.55
ns/elem) — stride 4096 B → new line + conflict-set + TLB pressure per
element. `-O3 -march=native`: GCC's `-ftree-loop-interchange` detects the bad
nest and **swaps it** → both ~equal (~0.25 ns/elem, ratio ~1.0). ⚠️ The
compiler fixed it — but only because it's a simple, provably-safe perfect
nest. Don't rely on it; write the right order.
</details>

### A3
4 threads, each incrementing its own `uint64_t` 80M times. Counters in a
packed array vs `alignas(64)` each. Ratio? Same ratio every run?

<details><summary>Answer</summary>

Padded: ~0.2-0.3 ns/inc, **stable**. Packed: ~1.8-3.7+ ns/inc → ratio
**~6× to ~44×, and it changes run-to-run** (example `04`). The packed
counters share a line → MESI ping-pong between cores on every increment. The
ratio swings because the OS schedules the 4 threads onto different physical
cores each run — same CCX (cheap bounce) vs cross-CCX (expensive). False
sharing isn't just slow, it's **jittery** — p99 poison.
</details>

### A4
`vector<Particle>` (8 float/uint fields, 32 B). (a) Sum `x+y+z` sequential.
(b) Same but random particle order, touch all 8 fields. AoS vs SoA winner
for each?

<details><summary>Answer</summary>

(a) **SoA wins ~2×** (example `05` T1) — `x[]` dense = 100% line use + 8-wide
vectorize; AoS pulls 32 B per particle, uses 12. (b) **AoS wins ~3×**
(example `05` T3) — random `ps[k]` = one 32-B struct = 1 cache line = 1 miss;
SoA's same particle lives in 8 separate arrays at 8 random offsets = 8 lines
= 8 misses. Layout must match the access pattern; "SoA always" is wrong.
</details>

### A5
Pointer-chase (one slot per 4 KiB page, random cycle), page count swept
16 → 8192. Where's the cliff and why? (Example `08`.)

<details><summary>Answer</summary>

Cliff around **2048 pages (~8 MiB working set)**: ~15 ns/hop at 1024 pages
jumps to **~95 ns/hop** at 2048+. That's where the working set crosses **both**
the L2 STLB reach (~6 MiB → every hop now page-walks) **and** L3 capacity
(8 MiB → every hop also DRAM-misses). With 4 KiB pages you can't separate the
two portably (page count and footprint grow together). 2 MiB huge pages would
push the TLB half of the cliff out to ~3 GiB.
</details>

### A6
`for (i) s += table[idx[i]];` — `idx` random, table 256 MiB. You add
`__builtin_prefetch(&table[idx[i+32]])`. Speedup on a big OoO core?

<details><summary>Answer</summary>

**~1.1×** — marginal (example `06` S1). The `table[idx[i]]` loads are
independent across iterations → the OoO engine already keeps ~10 misses
in-flight (MLP) → effective ~9 ns/lookup (true DRAM ~80). SW prefetch just
nudges the overlap. On this class of core, prefetch on an independent gather
is nearly free-of-benefit. It wins on in-order/narrow cores, or when a
dependency chain / ROB-fill has killed the natural MLP — and it can *hurt*
(example `06` S2: 3× slower when memory is already saturated).
</details>

---

## Part B — Find the bug

### B1
```cpp
struct Node { int value; Node* next; };
// 1M nodes, each `new Node`, linked in order
long sum = 0;
for (Node* n = head; n; n = n->next) sum += n->value;
```

<details><summary>Answer</summary>

`n = n->next` is a **dependent load chain** — the next address isn't known
until the current node's load completes → zero MLP → each not-cached node =
full DRAM latency serially (~90 ns is box) → 1M × 90 ns ≈ 90 ms. Plus each
`Node` is a separate `malloc` (scattered) with 16 B `int` + 8 B `next` + ~16 B
header = mostly overhead. Fix: `std::vector<int>` (contiguous, ~0.3 ms), or
if you need list semantics, an arena (`vector<Node>`) with `uint32` index
links so traversal is sequential.
</details>

### B2
```cpp
struct alignas(64) Counters {
    std::atomic<uint64_t> a, b, c, d, e, f, g, h;   // 8 * 8 = 64 B
};
Counters g;
// 8 threads, thread i increments g's i-th field in a hot loop
```

<details><summary>Answer</summary>

`alignas(64)` aligned the **struct**, but all 8 atomics are **inside one
64-B line** (offsets 0,8,...,56). 8 threads writing 8 different offsets of
the **same line** → maximal false sharing → the line ping-pongs among 8
cores, throughput collapses (~10-40×), jittery. Fix: one line **per**
counter — `struct alignas(64) Pad { std::atomic<uint64_t> v; char
pad[56]; }; Pad g[8];` — or per-thread local `uint64_t` + a `relaxed` store
to a padded global at the end.
</details>

### B3
```cpp
float dot(const std::vector<float>& a, const std::vector<float>& b) {
    float s = 0;
    for (size_t i = 0; i < a.size(); i += 4)   // "unroll" by skipping?
        s += a[i]*b[i];
    return s;
}
```

<details><summary>Answer</summary>

This isn't an unroll — it **skips 3 of every 4 elements** (stride 4), so it
computes the wrong answer *and* has bad locality: each iteration touches a
new-ish region of the line, using 1 of 4 floats, wasting 75% of fetched
bandwidth, and defeating vectorization (stride-4 gather). A real unroll
processes all elements: `for (i += 4) { s0+=a[i]*b[i]; s1+=a[i+1]*b[i+1];
s2+=...; s3+=...; }` then `s0+s1+s2+s3`. Or just `for (i++) s += a[i]*b[i];`
and let `-O2` vectorize + the multiple-accumulator transform happen.
</details>

### B4
```cpp
// grid of cells, row-major logically
std::vector<std::vector<Cell>> grid(H, std::vector<Cell>(W));
for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
        process(grid[y][x]);
```

<details><summary>Answer</summary>

`vector<vector<Cell>>` = `H` separate heap allocations, rows **scattered
across the heap** in unpredictable order → row-to-row you jump to a random
address (the outer `vector` holds pointers, and each inner buffer is wherever
`malloc` put it). No cross-row spatial locality, prefetcher resets each row.
Fix: one flat `std::vector<Cell> grid(H*W);` with `grid[y*W + x]` — fully
contiguous, prefetcher streams the whole grid, and it's `H` fewer
allocations. (Also the inner-vector overhead: 24 B control block × H.)
</details>

### B5
```cpp
struct Order {                 // hot-path struct
    char client_id[32];        // set once at creation, never on hot path
    uint64_t recv_timestamp;   // logged, not used in matching
    double price;              // hot: every match
    uint32_t qty;              // hot: every match
    OrderState state;          // hot
};
std::vector<Order> book;       // matching loop scans price/qty/state
```

<details><summary>Answer</summary>

`sizeof(Order)` ≈ 32 + 8 + 8 + 4 + 4 = 56 B → ~1.14 orders per cache line.
The matching loop only needs `price` (8) + `qty` (4) + `state` (~4) = 16 B,
but every order access drags in the 40 B of `client_id` + `recv_timestamp`
too → ~2.5× the misses it needs. **Hot/cold split**: `struct OrderHot {
double price; uint32_t qty; OrderState state; };` (16 B → 4 per line) in the
scanned array; `struct OrderCold { char client_id[32]; uint64_t recv_ts; };`
in a parallel array at the same index, touched only on create/log. Matching
loop now ~4× denser.
</details>

### B6
```cpp
// transpose
for (int i = 0; i < N; ++i)
    for (int j = 0; j < N; ++j)
        b[j*N + i] = a[i*N + j];        // N = 4096
```

<details><summary>Answer</summary>

`a[i*N + j]` reads sequentially (good), but `b[j*N + i]` writes with **stride
N** (4096 elements = a new line + often a new page every write) → every store
is a write-miss (RFO) to a cold, far-apart line, and `b`'s working set thrashes
L2/STLB. Naive transpose is cache-hostile on one side no matter which loop is
outer. Fix: **blocked transpose** — process 32×32 (or 64×64) tiles:
```cpp
for (ii; ii<N; ii+=B) for (jj; jj<N; jj+=B)
  for (i=ii..ii+B) for (j=jj..jj+B) b[j*N+i] = a[i*N+j];
```
A tile of `a` and the matching tile of `b` both fit in cache → reads and
writes stay local within the tile.
</details>

---

## Part C — Reasoning

### C1
`perf stat`: IPC 0.7, L1d MPKI 3, LLC MPKI 11, dTLB MPKI 0.1, branch-miss
0.4%. Working set ~60 MiB, single sequential pass. Diagnosis + top 2 fixes.

<details><summary>Answer</summary>

Low IPC + **LLC MPKI 11** (high) + everything else fine → **capacity misses
to DRAM**, memory/bandwidth bound (60 MiB >> 8 MiB L3, single pass so no
reuse to exploit). Fixes: (1) **shrink the data** — smaller element types
(double→float→int16 if precision allows), drop struct padding, hot/cold
split, SoA if only some fields are read → linear reduction in bytes moved
→ linear speedup (lesson 13). (2) **fuse** this pass with the adjacent
producer/consumer so the 60 MiB is streamed once, not multiple times. NOT
SIMD (memory-bound), NOT blocking (no reuse in one pass).
</details>

### C2
A blocked matmul at N=768 runs at 5.1 GFLOP/s; plain `ikj` loop order (no
blocking) runs at 5.9 GFLOP/s on the same box. Both beat naive `ijk`
(1.4 GFLOP/s). What does this tell you, and when *would* blocking win?

<details><summary>Answer</summary>

The big win (~4×) was **loop order** (`ijk`→`ikj`: B and C accessed row-wise,
vectorizable). Naive 64×64 blocking on top is ~13% *slower* here (example
`07`) because: (a) `ikj` already auto-vectorizes and is cache-friendly, (b)
at N=768 the L3 (8 MiB) absorbs B's reuse, (c) the block loops add overhead
without a tuned inner microkernel. Blocking wins when: the working set is
genuinely >> L2/L3 (large N), **and** you also do register tiling + a
hand-vectorized microkernel (what real BLAS does — that gets another ~10×).
Naive tiling alone ≠ profit.
</details>

### C3
You convert an AoS to SoA. Before: LLC MPKI 8, time 40 ms. After: LLC MPKI 3,
time 39 ms. Explain, and what next.

<details><summary>Answer</summary>

The SoA change genuinely cut DRAM misses (8→3 MPKI) but time barely moved →
the loop **wasn't bottlenecked on those misses**, or a different bottleneck
now dominates. Re-run `perf stat --topdown`: likely now **Core Bound** (a
dependency chain / ALU port limit) or **Frontend Bound**, or it was partly
branch-bound all along. Also verify the SoA loop actually vectorized (`perf
annotate` — look for `ymm`/`vfmadd`); if not, a branch or a non-inlined call
in the body is blocking it. Attack the *current* top bucket; don't assume
the first hypothesis explained everything.
</details>

### C4
Little's Law: DRAM latency 90 ns, line 64 B, peak BW 40 GB/s. How many lines
must be "in flight" to saturate BW? A single core has ~10 fill buffers —
what fraction of peak can it reach, and what's the implication for a
single-threaded latency budget?

<details><summary>Answer</summary>

Lines/sec at peak = 40e9 / 64 ≈ 625 M. Concurrency = 625e6 × 90e-9 ≈ **56
lines in flight**. One core with ~10 LFBs → ~10/56 ≈ **~18% of socket peak**
(~7 GB/s). Implication: never size a single-threaded budget against the
datasheet BW — one thread streaming gets ~10-15 GB/s, and a *latency-bound*
(dependent-miss) single thread gets ~1/90ns ≈ 11 M lines/s ≈ 0.7 GB/s. To
use full memory BW you need many cores; to go fast single-threaded you must
keep the working set in cache.
</details>

---

## Part D — Challenge (build + measure, real numbers)

### D1 — Stencil blocking
Write a 2D 5-point stencil (`out[y][x] = 0.2*(in[y][x] + in[y±1][x] +
in[y][x±1])`) on a 4096×4096 `float` grid. Version A: naive double loop.
Version B: blocked in 2D tiles (try 64×64, 256×256). Measure ns/cell for
each. Then try fusing 3 stencil iterations per tile (with halo). Report the
numbers and which won — and if blocking *didn't* help (like example `07`),
say so and explain (L3 size, auto-vectorization).

### D2 — AoS → SoA particle sim, and the reverse
Take a 4M-particle sim. Implement (a) `integrate` (pos += vel*dt) and (b)
`query_nearest(point)` which random-accesses particles and reads all fields.
Do both AoS and SoA. Measure. You should find SoA wins (a), AoS wins (b) —
report the ratios. Then implement AoSoA (W=8) and show it's within ~10-20%
of the best of each. Real numbers.

### D3 — False-sharing histogram
8 threads build a shared 4096-bucket `uint32` histogram over random keys.
Version A: one shared `std::atomic<uint32_t> hist[4096]`. Version B:
per-thread private `uint32_t local[4096]`, merged at the end. Measure total
time and — importantly — run each **10 times** and report the range. Version
A should be both slower and much higher-variance. Explain the variance
(scheduling / core placement, lesson 07).

### D4 — Working-set latency curve
Reproduce the classic memory-latency curve: pointer-chase (Sattolo single
cycle) through a buffer, sweeping buffer size from 4 KiB to 256 MiB (×2 each
step). Plot ns/access vs size. You should see plateaus at ~L1 (~1-2 ns),
~L2 (~4-7 ns), ~L3 (~15-25 ns), DRAM (~90-100 ns). Mark where your box's
L1/L2/L3 sizes are (from `31/examples/07_cpu_info.cpp`). Do it once with
4 KiB pages and note where TLB effects add a bump (lesson 11).

### D5 — NT store bandwidth
`for (i) out[i] = c;` over a 512 MiB buffer. Version A: normal stores.
Version B: `_mm256_stream_si256` + `_mm_sfence()`. Measure GB/s for each, and
also measure a *second* hot loop's time before/after each fill (to show
Version A polluted the cache and Version B didn't). Report both effects with
real numbers.

---

## Interview questions (folder-wide)

1. Memory hierarchy latencies (cycles + ns), aur ek `load`'s full path.
2. Cache line — 64 B kyun, aur "unit of transfer / coherence" ke implications.
3. Set-associative cache — index/tag/offset, aur conflict miss + critical stride.
4. The 3 C's + coherence — har ek ka ek fix.
5. Spatial vs temporal locality — aur loop interchange / fusion / tiling se kaunsa.
6. HW prefetcher kya pakadta / nahi; SW prefetch kab (aur example `06` ka result).
7. False sharing — mechanism, detection (`perf c2c`), fix, aur jitter kyun.
8. Pointer-chasing DRAM latency ko hide kyun nahi karta (MLP).
9. AoS vs SoA — teen access patterns, teen winners.
10. Data-oriented design — one-line philosophy, aur `virtual` hot loop ki cost.
11. TLB / page walk / huge pages — reach math, aur THP ke downsides.
12. Store buffer / RFO / NT stores — write-side machinery.
13. Latency-bound vs bandwidth-bound — kaise batao, aur SIMD kab bekaar.
14. `perf stat` + top-down — memory-bound confirm karne ke steps.
15. Optimization recipe order — interchange → shrink → SoA → ... → prefetch, aur kyun.

---

## Next
→ [`../33-COMPILER-OPTIMIZATION/00-README.md`](../33-COMPILER-OPTIMIZATION/00-README.md)
