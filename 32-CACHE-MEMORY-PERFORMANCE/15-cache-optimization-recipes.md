# 15 — Cache optimization recipes

## Prerequisites
- Poora folder 32 (`01`–`14`)
- `31-CPU-ARCHITECTURE` (branch, SIMD, NUMA)

## Yeh topic abhi kyun
Yeh checklist hai — jab profiler ne bataya "memory-bound" (lesson 14), to
kaunse fixes, kis order mein, aur har ek ka **trade-off** (CLAUDE.md rule 13:
har technique ke saath uski keemat). Recipes ~impact-order mein — pehle
saste, bade wins.

---

## Recipe 0: Measure first (always)

`perf stat` → `--topdown` → `perf record`/`annotate` (lesson 14). Confirm:
- Memory-bound? (not branch, not core-bound)
- Which **kind** of miss? (compulsory / capacity / conflict / coherence —
  lesson 04)
- Which **line** in which loop?

Skip this and you'll do example `06` (prefetch → 3× slower).

---

## Recipe 1: Fix access order (loop interchange)

**When:** strided/column access to row-major data (or vice versa).
**Do:** swap loop nesting so the innermost loop walks memory contiguously.
```cpp
for (j) for (i) sum += m[i][j];   // ❌ stride N
for (i) for (j) sum += m[i][j];   // ✅ stride 1
```
**Impact:** example `03` — ~10×. Biggest bang, near-zero cost.
**Trade-off:** sometimes the "right" order for cache conflicts with the order
an algorithm needs (e.g. a reduction axis) — then you need blocking or a
transpose. `-O3` may do it for you (`-ftree-loop-interchange`) but only for
simple provably-safe nests — don't rely on it.

---

## Recipe 2: Shrink the working set

**When:** capacity misses (every pass slow, working set > cache).
**Do:**
- **Smaller types:** `int32`→`int16`, `double`→`float`/half, `enum : uint8_t`,
  pack bools into a bitset. Bandwidth-bound loops scale ~linearly with byte
  count (lesson 13 — `double`→`float` on a reduction ≈ 2×).
- **Indices instead of pointers:** `uint32` handle vs 8-byte pointer (lesson
  08).
- **Drop padding:** reorder struct fields large→small; `static_assert` the
  size.
- **Hot/cold split** (Recipe 6).
**Impact:** often 1.5–4× on memory-bound loops.
**Trade-off:** smaller types → precision/range limits (check!), extra
pack/unpack code, possible alignment fiddliness. Indices → need generation
counters for safety.

---

## Recipe 3: Blocking / tiling

**When:** capacity misses AND the data is reused within the computation
(matmul, stencil, convolution, transpose, n-body).
**Do:** process the problem in cache-sized tiles; finish all reuse of a tile
before moving on.
```cpp
for (ii = 0; ii < N; ii += BS)
  for (jj = 0; jj < N; jj += BS)
    for (i = ii; i < ii+BS; ++i)
      for (j = jj; j < jj+BS; ++j) ...   // BS chosen so a tile fits L1/L2
```
**Impact:** large at big N (naive matmul → blocked can be 5–20×+).
**Trade-off:** ⚠️ example `07` — **naive blocking was ~10-15% *slower* than
a good loop order (ikj)** on this box, because ikj already auto-vectorizes
and L3 absorbed the reuse. Blocking only pays when (a) the working set is
genuinely >> L2/L3, **and** (b) you also tune the inner microkernel
(register tiling, vectorized). Do Recipe 1 first; add blocking only if the
profiler still shows capacity misses after.

---

## Recipe 4: SoA / AoSoA

**When:** you scan a few fields over many records; and/or you want the loop
to vectorize.
**Do:** field-arrays instead of a struct array (lesson 09).
**Impact:** example `05` — ~2–2.4× (line utilization + vectorization).
**Trade-off:** ⚠️ random whole-record access → SoA is *worse* (N misses vs 1
— example `05` T3). Insert/erase touches N arrays. Passing a single record
around is awkward. Use **AoSoA** if you need both scan-vectorize and
whole-record locality.

---

## Recipe 5: Kill pointer chasing

**When:** `perf annotate` shows the stall on a `->next` / tree-node / hash-
node load; dependent-miss chains.
**Do:** flat containers (`vector`), arena + index links, B-tree/`btree_map`
instead of `map`, open-addressing hash (`flat_hash_map`) instead of chaining,
CSR instead of adjacency lists (lesson 08).
**Impact:** 5–300× on traversal-heavy code (dependent misses are the worst).
**Trade-off:** flat structures → O(n) insert/erase (shift), rebuild cost;
arena → lifetime management, generation counters; open-addressing → rehash
invalidation.

---

## Recipe 6: Hot/cold splitting

**When:** a struct has a few fields touched every iteration and many touched
rarely; the cold fields waste line bandwidth on the hot path.
**Do:**
```cpp
struct OrderHot  { double px; uint32_t qty; uint16_t flags; };   // in the hot array
struct OrderCold { char client[32]; uint64_t recv_ts; ... };     // parallel array, same index
```
**Impact:** more hot records per line → fewer misses; can be 1.5–3×.
**Trade-off:** two arrays to keep in sync; a full-record view needs both;
slight code complexity.

---

## Recipe 7: Prefetch the un-prefetchable

**When:** measured MLP deficit (not a big-OoO-core independent gather —
example `06` showed that's ~1.1×) AND you know addresses ahead.
**Do:** `__builtin_prefetch(&table[idx[i+D]], rw, locality)`, sweep `D`.
**Impact:** modest on big cores; larger on in-order/narrow cores or genuine
dependency-limited MLP.
**Trade-off:** ⚠️ example `06` S2 — prefetch made an already-saturated loop
**3× slower**. Extra uops, bandwidth contention, cache pollution. Measure
before/after; keep only if > ~5% and stable.

---

## Recipe 8: Align / pad shared data

**When:** multi-threaded, throughput drops or gets jittery as threads
increase; `perf c2c` shows HITM.
**Do:** `alignas(64)` hot fields written by different threads; pad ring
`head`/`tail` to separate lines; per-thread-local accumulators + combine
(lesson 07).
**Impact:** example `04` — removes a ~6–40× (and jittery) penalty.
**Trade-off:** memory overhead (60 B padding per field) — only for genuinely
contended hot fields, not every struct.

---

## Recipe 9: Huge pages (+ pre-fault + mlock)

**When:** `dTLB-load-misses` / `dtlb_load_misses.walk_completed` high; large
(multi-MB) working set with poor page locality (lesson 11).
**Do:** explicit `MAP_HUGETLB` 2 MiB arenas for hot regions, `memset`/touch
to pre-fault at startup, `mlockall`. THP set to `madvise`/`never`.
**Impact:** collapses thousands of translations → a handful; removes page-
walk stalls.
**Trade-off:** boot-time reservation, memory waste if under-used, NUMA
first-touch care needed. THP `always` brings `khugepaged` jitter — avoid on
latency boxes.

---

## Recipe 10: NT stores for write-only bulk

**When:** streaming output larger than LLC that you won't re-read soon
(lesson 12) — packet TX buffers, snapshots, big fills.
**Do:** `_mm256_stream_si256` + `_mm_sfence()` after the batch.
**Impact:** removes the RFO (≈ half the write bandwidth back) + no cache
pollution → your hot data survives.
**Trade-off:** ⚠️ if you *do* re-read it soon → full DRAM miss. Weakly
ordered — `sfence` mandatory. Small copies → overhead dominates.

---

## Recipe 11: NUMA placement

**When:** multi-socket; `perf mem` shows remote-DRAM data sources; bandwidth
lower than one node's peak.
**Do:** first-touch each region from the thread that'll use it, `numactl
--membind` / `--cpunodebind`, pin threads (`31/14`, `29/15`).
**Impact:** remote → local ≈ +bandwidth, −latency (~1.5× on the affected
accesses).
**Trade-off:** rigid placement; a thread that migrates loses it; needs a
NUMA-aware allocator / explicit `numa_alloc_onnode`.

---

## The order to apply them

```
0. Measure (perf stat / topdown / annotate)   <- always
1. Loop interchange (access order)            <- cheapest big win
2. Shrink working set (types, padding, indices)
6. Hot/cold split
4. SoA / AoSoA (also enables SIMD)
5. Kill pointer chasing (data structure swap)
3. Blocking / tiling                          <- only if still capacity-bound
8. Align/pad shared (if multi-threaded + c2c)
9. Huge pages + prefault + mlock              <- infra-level
11. NUMA placement                            <- multi-socket
10. NT stores (write-only bulk)
7. SW prefetch                                <- last; measure hard
-- re-measure the same counters after EACH; keep only what moved the number,
   explain what changed.
```

---

## ⚠️ Meta-traps

### Trap 1 — applying recipes without Recipe 0
Every "surprising" result in folder 32 (`03` -O3 interchange, `06` prefetch
3× slower, `07` blocking slower) came from the compiler/hardware not matching
intuition. Measure.

### Trap 2 — stacking fixes without re-measuring
Apply 5 things, time it once, "it's 2× faster" — you don't know which 2 of
the 5 helped and which hurt. One at a time.

### Trap 3 — optimizing a non-bottleneck loop
The loop you find easy to optimize may not be the one on the critical path.
`perf record` first.

### Trap 4 — micro-optimizing when the algorithm is wrong
No amount of cache tuning fixes an O(n²) where O(n log n) exists, or 3 passes
that should be 1. Algorithm/passes first, then cache.

### Trap 5 — portability of the tuning
`D` for prefetch, `BS` for blocking, `alignas` sizes — all machine-specific.
Parameterize, and re-tune on the production box (frequency-locked, SMT-off,
`31/12-13`).

---

## > **HFT relevance**

> - **Steady-state target: hot loop is neither memory- nor branch-bound.**
>   Working set in L1/L2, no pointer chase, no unpredictable hot branch,
>   vectorized where it's compute. `perf stat` MPKI ~0 on the tick path.
> - **Recipe order matters for HFT too** — interchange + shrink + hot/cold +
>   flat structures get you most of the way; blocking/prefetch/NT are
>   situational.
> - **Every fix earns its place with a number** — before/after `perf stat`,
>   and a one-line "what changed and why" (this repo's workflow, CLAUDE.md).
> - **Re-tune on prod hardware.** The dev laptop's ratios port; the constants
>   (`BS`, `D`, thread counts) don't.
> - **Infra recipes (8-11) are one-time setup** — huge pages, mlock, NUMA
>   pinning, padded rings — do them once, correctly, at startup.

---

## Hands-on

```bash
# every recipe has a measuring example in this folder:
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/03_matrix_traversal.cpp  # R1
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/05_aos_vs_soa.cpp        # R4
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/07_matrix_blocking.cpp   # R1 vs R3
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/04_false_sharing.cpp     # R8
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/06_prefetch.cpp          # R7 (cautionary)
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/08_tlb_hugepages.cpp     # R9

bash 32-CACHE-MEMORY-PERFORMANCE/examples/09_perf_analysis.sh ./your_prog      # R0
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "cache recipes = always apply all" | measure, then the 1-2 that fit the miss kind |
| "blocking always helps" | example 07 — slower than good loop order without a tuned kernel |
| "prefetch is safe to add" | example 06 — 3× slower; last resort, measure |
| "stack fixes, time once" | one at a time, re-measure the counter |
| "tuning constants port" | BS/D/thread-count are machine-specific; re-tune |
| "cache tuning fixes bad big-O" | algorithm/passes first |

---

## Exercises

1. Profiler: memory-bound, LLC MPKI 9, working set 40 MiB (>> L3), single
   pass over a `struct{double a,b,c,d,e,f,g,h;}` array, loop uses only `a`
   and `c`. Rank the recipes you'd try, in order.

   <details><summary>Answer</summary>

   (1) **R4 SoA** — you touch 2 of 8 fields; splitting `a[]` and `c[]` into
   dense arrays cuts the bytes moved by ~4× and lets the loop vectorize.
   Biggest win here. (2) **R2 shrink** — do `a`/`c` need `double`? `float`
   halves it again. (3) **R6 hot/cold** is subsumed by the SoA split (b,d,e,
   f,g,h go to their own arrays / a cold struct). (4) **R1** — check the loop
   is already sequential (it is, single pass). (5) Blocking (R3) — no reuse
   in a single pass, skip. (6) Prefetch (R7) — sequential, HW handles it,
   skip. Re-measure LLC MPKI after the SoA+float change — should drop toward
   ~2.
   </details>

2. Aap R1 (interchange) + R4 (SoA) + R7 (prefetch) ek saath lagate ho, time
   40 ms → 12 ms. Manager khush. Kya galat hai is process mein?

   <details><summary>Answer</summary>

   You applied three changes and measured once → you don't know the
   attribution. R1 and R4 very likely did the work (interchange + SoA are
   classic big wins); R7 (prefetch) — based on example `06` — may have done
   nothing or even cost a bit, hidden in the noise of the big win. Correct
   process: apply R1, measure; apply R4, measure; apply R7, measure. If R7
   moved the number < ~5% or made it worse, **remove it** — it's extra code,
   extra uops, and machine-specific `D` tuning that'll rot. Keep only what's
   demonstrably pulling weight, and record the per-step deltas.
   </details>

3. Ek hot lookup structure `std::map<uint64_t, Order*>` (500k entries), `perf
   annotate` stalls on the tree-node loads. Recipe(s) aur trade-offs?

   <details><summary>Answer</summary>

   **R5 kill pointer chasing.** Options: (a) `absl::btree_map<uint64_t,
   uint32_t>` — ~3-4 misses/lookup vs ~19, ordered kept, small-shift
   insert/erase. (b) `absl::flat_hash_map<uint64_t, uint32_t>` — ~1 miss,
   fastest, but loses ordering (if you need ordered iteration this breaks).
   (c) if order IDs are dense-ish → a direct `std::vector<uint32_t>` indexed
   by `id - base` — 1 miss, but memory = range not count, and sparse IDs
   waste it. Also **R2**: store `uint32_t` pool indices, not 8-byte `Order*`.
   Trade-offs: hash loses ordering; btree keeps it at ~2× the hash's miss
   count; direct-index needs dense IDs. Pick per the actual access needs;
   measure lookups in misses (`perf stat -e mem_load_retired.l3_miss`).
   </details>

---

## Interview questions

1. The recipe application order — why interchange before blocking, why
   prefetch last.
2. Blocking — when it helps, and the example-`07` caveat (when it doesn't).
3. SoA — the win, and the case where it backfires.
4. "Apply one fix, re-measure" — why stacking fixes is a process bug.
5. Hot/cold splitting — mechanism and cost.
6. Which recipes are one-time infra setup vs per-loop code changes.
7. Why tuning constants (BS, prefetch D) don't port between machines.

---

## Next
→ [`16-exercises.md`](16-exercises.md)
