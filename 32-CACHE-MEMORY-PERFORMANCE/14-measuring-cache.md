# 14 — Measuring cache: `perf stat`, `perf c2c`, cachegrind

## Prerequisites
- `31-CPU-ARCHITECTURE/04-pipelining.md` (`perf stat` IPC intro)
- Lessons `04` (3 C's), `07` (false sharing), `11` (TLB), `13` (bandwidth)
- example `09_perf_analysis.sh`

## Yeh topic abhi kyun
Ab tak har lesson kehta hai "measure karo." Yeh lesson **kaise** — kaunse
tools, kaunse counters, aur unke numbers ka kya matlab. **Guess-based cache
optimization sabse bada time-waste hai** (aur aksar ulta, jaise example `06`).
`perf` (Linux) aur cachegrind ke bina aap andhere mein ho.

> Yeh repo ka box Windows/MinGW hai — `perf` nahi. WSL2 mein `perf`
> partially chalta (no PMU on some setups); best hai ek Linux dev box, ya
> VTune / AMD uProf / `xperf`+WPA Windows pe. Neeche mostly Linux `perf`
> assume karta hai kyunki HFT wahi use karta.

---

## Layer 1: `perf stat` — kya bottleneck hai

```bash
perf stat -e cycles,instructions,\
L1-dcache-loads,L1-dcache-load-misses,\
l2_rqsts.miss,\
LLC-loads,LLC-load-misses,\
dTLB-loads,dTLB-load-misses,\
branches,branch-misses,\
page-faults \
    ./your_prog
```

Reading it:
- **IPC** = instructions / cycles. Compute loop expected ~2-4. **< 1 → stalling
  on something** — chase which.
- **L1 miss rate** = `L1-dcache-load-misses / L1-dcache-loads`. > ~5-10% on a
  loop whose working set should fit L1 → access pattern or conflict problem.
- **LLC-load-misses** → these go to DRAM. This is the expensive one. High +
  low IPC → memory-bound (lesson 13). Divide by `instructions` for MPKI.
- **dTLB-load-misses** high → page walks (lesson 11), separate from data
  misses.
- **branch-misses** high + low IPC → not a cache problem, a branch problem
  (`31/07`).

### MPKI — the normalized metric
```
MPKI = (misses / instructions) × 1000     # misses per 1000 instructions
```
- L1d MPKI < ~10 : fine. ~10-30 : worth looking. > 30 : L1 access pattern
  is a problem.
- LLC MPKI < ~1 : fine. ~1-5 : memory pressure. > 5 : bandwidth/latency
  bound, restructure.

MPKI lets you compare across runs/inputs of different length. Raw miss counts
don't.

---

## Layer 2: top-down — which pipeline slot is wasted

```bash
perf stat --topdown -a ./your_prog
# or the fuller breakdown:
perf stat -M TopdownL1 ./your_prog
#   toplev.py -l3 ./your_prog     (Andi Kleen's pmu-tools, deeper)
```

Splits every issue slot into 4 buckets:
- **Retiring** — real work (good; want this high).
- **Bad Speculation** — mispredicted branches / machine clears.
- **Frontend Bound** — starved for instructions (I-cache miss, decode, iTLB).
- **Backend Bound** — execution can't keep up →
  - **Memory Bound** sub-bucket → cache/DRAM stalls (this folder's territory).
  - **Core Bound** → ALU ports / dependency chains (`31/04-06`).

"Backend Bound → Memory Bound" high = go do cache work. "Bad Speculation"
high = go do branch work. This one command aims you.

---

## Layer 3: localize — which line / instruction

```bash
perf record -e cache-misses -c 10000 -g ./your_prog     # sample every 10k misses
perf report                                             # top functions by misses
perf annotate <function>                                # per-instruction miss attribution
```

`perf annotate` shows you the exact `mov` that's missing. Often it's one
`load` in an inner loop — the array access you suspected, or a surprise
(a `std::vector::operator[]` bounds check, a `shared_ptr` refcount, a map
node deref).

For DRAM specifically: `perf record -e mem_load_retired.l3_miss` (Intel).

---

## Layer 4: false sharing — `perf c2c`

```bash
perf c2c record ./your_prog
perf c2c report --stdio
```

Shows **cache lines with cross-core contention**: the line address, the
offsets touched, which PIDs/TIDs, and **HITM** counts (loads that hit a
Modified line in another core's cache — the false/true sharing signature).

Reading it: a line with **multiple threads writing different offsets** +
high HITM = **false sharing** (lesson 07). Same offset = true sharing (real
contention, needs an algorithm fix). Note the offsets → pad those fields
apart.

---

## Layer 5: no root / deterministic — cachegrind

```bash
valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes ./your_prog
cg_annotate cachegrind.out.<pid>          # per-line D1/LL miss counts
```

- **Simulated** cache (not your real one — configurable sizes), **~40× slower**,
  but **deterministic** (same input → same numbers) and **no root / no PMU**
  needed. Great in CI, containers, or when `perf` is locked down.
- Gives per-line `D1mr` (L1 read miss), `DLmr` (LL read miss), etc.
- Caveat: no prefetcher model, no OoO/MLP — it over-counts stalls the real
  CPU would hide. Use for *relative* comparison and finding the hot line,
  not absolute ns.

---

## Other tools

| Tool | Platform | Notes |
|---|---|---|
| **Intel VTune** | Linux/Win | Best GUI, memory-access analysis, per-line, roofline |
| **AMD uProf** | Linux/Win | VTune equivalent for AMD |
| **`likwid`** | Linux | `likwid-perfctr` (groups: CACHE, MEM, TLB), `likwid-bench` |
| **`heaptrack` / `massif`** | Linux | allocation profiling (working-set size) |
| **`xperf` + WPA** | Windows | ETW-based, cache/DPC/context-switch |
| **`perf mem`** | Linux | per-load latency + data source (L1/L2/L3/DRAM/remote) |

---

## A measurement workflow

1. `perf stat` (IPC, MPKI, TLB, branch) → is it even memory?
2. `perf stat --topdown` → Backend/Memory Bound? confirm.
3. `perf record -e cache-misses` + `perf annotate` → which line.
4. Reason about the 3 C's (lesson 04): pass-1-vs-2, working-set-vs-cache,
   thread-scaling → which kind of miss.
5. Multi-threaded + suspicious? `perf c2c` → false sharing?
6. Apply **one** fix (lesson 15).
7. Re-measure the same counters. Keep only if the number moved. Explain what
   changed (CLAUDE.md workflow).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — optimizing without step 1
Guessing "it's cache" and adding prefetch/`alignas` blindly. Example `06`:
prefetch made it 3× *slower*. Measure the bottleneck first.

### Trap 2 — raw miss counts across different inputs
"10M misses" means nothing without instruction count / time. Use MPKI or
misses/sec.

### Trap 3 — `perf` under a VM / cloud without PMU
Many cloud instances don't expose the PMU → `perf stat` shows `<not
supported>`. Use cachegrind, or a metal instance, or vendor tools.

### Trap 4 — cachegrind numbers as absolute truth
No prefetcher, no MLP model → it says "1M LL misses = 100 ms" but the real
CPU hides half. Relative only.

### Trap 5 — measuring a debug build
`-O0` has stack traffic everywhere → every counter is garbage. Always `-O2`+
with `-g` for symbols (`31` folder learned this repeatedly).

### Trap 6 — one run
Cache/TLB numbers vary run-to-run (ASLR changes page colors, scheduler moves
threads, frequency scales — this box ±30%). Run 5-10×, report median/range.

---

## > **HFT relevance**

> - **Bake `perf stat` into the bench harness.** Every strategy build emits
>   IPC, L1d MPKI, LLC MPKI, dTLB MPKI, branch-miss rate for the tick loop.
>   A regression in any = a diff to review.
> - **`perf c2c` in CI-ish soak tests** — false sharing creeps in when
>   someone adds a field to a shared struct.
> - **`perf mem` for the p99 chase** — it tells you the *data source* of the
>   slow loads (L3 vs local DRAM vs remote DRAM) → points at working-set
>   growth or a NUMA mistake.
> - **VTune "Memory Access" analysis** once per big optimization pass —
>   roofline + per-object miss attribution.
> - **Deterministic gate**: cachegrind D1/LL miss counts on a fixed replay
>   input, checked in CI — catches "someone made the hot loop touch more
>   lines".

---

## Hands-on

```bash
# the script that ties it together (Linux):
bash 32-CACHE-MEMORY-PERFORMANCE/examples/09_perf_analysis.sh ./your_prog

# on this Windows box: build the folder-32 examples, run under WSL's perf,
# or use AMD uProf. The examples are designed to show clear counter signals:
#   03 (col-major) -> high LLC-load-misses + dTLB-load-misses
#   04 (false sharing) -> perf c2c HITM
#   08 (page stride) -> high dtlb_load_misses.walk_completed
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "profiler bola cache, fix laga do" | identify the *kind* of miss first (3 C's) |
| "10M misses = bad" | MPKI ya misses/sec — normalize |
| "cachegrind = ground truth" | simulated, no prefetch/MLP — relative only |
| "`perf` har jagah chalega" | many cloud VMs: no PMU |
| "ek run kaafi" | ±30% run-to-run; 5-10 runs, median |
| "-O0 profile theek hai" | stack traffic drowns everything; -O2 -g |

---

## Exercises

1. `perf stat` output: IPC 0.6, `L1-dcache-load-misses` MPKI 4, `LLC-load-
   misses` MPKI 12, `dTLB-load-misses` MPKI 0.2, `branch-misses` 0.5%.
   Diagnosis?

   <details><summary>Answer</summary>

   IPC 0.6 = badly stalled. L1d MPKI 4 is low (L1 is mostly fine). **LLC MPKI
   12 is high** → lots of loads going all the way to DRAM → **bandwidth/
   latency bound, working set >> L3**. TLB MPKI 0.2 and branch-miss 0.5% are
   both fine — not the problem. Conclusion: **capacity misses to DRAM**. Fix
   direction: shrink working set (smaller types, hot/cold split), block/tile
   the passes so data is L2/L3-resident, fuse passes. SIMD would not help
   (memory-bound, lesson 13).
   </details>

2. `perf stat --topdown` says: Retiring 15%, Bad Speculation 8%, Frontend
   Bound 12%, Backend Bound 65% (of which Memory Bound 55%). Aur `perf c2c`
   ek line pe 3 threads, offsets 0/24/48, HITM 5M. Kya karoge?

   <details><summary>Answer</summary>

   Backend/Memory Bound 55% dominates → it's a memory problem. `perf c2c`
   pinpoints it: one line, **three threads writing three different offsets**
   (0, 24, 48), 5M HITM → **false sharing** (lesson 07). Those three fields
   belong to different threads' hot state but share a 64-B line. Fix:
   `alignas(64)` / pad them apart (or per-thread-local + combine). Re-run
   `perf c2c` — HITM should drop to ~0 and Memory Bound should fall sharply.
   </details>

3. Aap ek fix (SoA conversion) lagate ho. Before: LLC MPKI 8, time 40 ms.
   After: LLC MPKI 3, time 38 ms. Kya conclude, kya next?

   <details><summary>Answer</summary>

   The SoA change **did** reduce DRAM misses (MPKI 8 → 3) but time barely
   moved (40 → 38 ms) → the loop wasn't actually bottlenecked on those misses,
   OR a new bottleneck surfaced. Check `--topdown` again: likely it's now
   **Core Bound** (dependency chain / ALU) or **Frontend Bound**, or it was
   partly branch-bound all along. Also verify the SoA loop actually
   vectorized (`perf annotate` / check for `ymm`). Next: re-run the full
   `perf stat` + topdown on the new build, find the *current* top bucket,
   attack that. Don't assume the first hypothesis was the whole story —
   measure each step.
   </details>

---

## Interview questions

1. `perf stat` — 5 counters you'd look at for a cache investigation, and what
   each tells you.
2. MPKI — formula aur kyun raw miss counts se better.
3. Top-down methodology — the 4 buckets, aur "Memory Bound" kya point karta.
4. `perf c2c` — kya measure karta, false vs true sharing signature.
5. cachegrind — kab use karo (vs `perf`), aur uske 2 limitations.
6. A measurement workflow — steps from "it's slow" to "verified fix".
7. Run-to-run variance ke sources (ASLR, scheduler, frequency) aur kaise handle karte.

---

## Next
→ [`15-cache-optimization-recipes.md`](15-cache-optimization-recipes.md)
