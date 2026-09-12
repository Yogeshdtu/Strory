# 14 — Intel VTune: top-down microarchitecture analysis

## Prerequisites
- `10-perf-basics.md` (`perf stat` top-down — VTune isko GUI + deep karta)
- `31-CPU-ARCHITECTURE` (pipeline: frontend, backend, retire, speculation)
- `11-perf-advanced.md` (PMU events)

## Yeh topic abhi kyun
`perf stat -M TopdownL1` (lesson 10) 4 numbers deta — Retiring / Frontend /
Backend / Bad-Spec. VTune wahi **top-down methodology** ko poore tree tak le
jaata (L1→L2→L3...), har node ke liye source line + fix suggestion, ek GUI
mein. Intel CPUs pe (aur ab kuch AMD support) yeh sabse powerful "kyun slow
hai, kis dhang se" tool hai. HFT shops isko heavily use karte.

> VTune Intel ka tool hai (ab **oneAPI** ka hissa, free). CLI `vtune`, GUI
> `vtune-gui`. Best on Intel; AMD pe `uprof` (AMD ka equivalent) ya `perf`.
> Yeh lesson **concepts** pe hai — exact GUI clicks version pe badalte.

---

## Top-down methodology (recap + deepen)

Har CPU cycle mein pipeline ke paas **issue slots** hote (e.g. 4-wide → 4
slots/cycle). Har slot in 4 categories mein:

```
                        ┌─ Retiring          slot ne useful uop retire kiya  ✅
   pipeline slot ───────┤─ Bad Speculation   uop issue hua par flush ho gaya (mispredict) ❌
                        ├─ Frontend Bound    frontend ne slot bhara hi nahi (fetch/decode starve) ❌
                        └─ Backend Bound     frontend ready tha, backend ne accept nahi kiya (stall) ❌
```

VTune (aur `perf`) PMU counters se in 4 ka **% breakdown** deta, phir
**recursively** har bucket ko tod-ta:

```
Backend Bound 62%
├── Memory Bound 48%
│   ├── L1 Bound          6%
│   ├── L2 Bound          3%
│   ├── L3 Bound         12%
│   ├── DRAM Bound       22%   <- yahan: working set >> LLC (folder 32)
│   │   ├── Memory Bandwidth  8%
│   │   └── Memory Latency   14%
│   └── Store Bound        5%
└── Core Bound 14%
    ├── Divider            9%   <- div/sqrt unit (folder 31/09)
    └── Port Utilization   5%
```

Har leaf ke saath VTune: **which functions / source lines** contribute, aur
often a text hint ("consider blocking for cache", "high divider usage —
replace with reciprocal multiply").

---

## VTune ke analysis types

| Type | Kya dhoondta | Kab |
|---|---|---|
| **Hotspots** (user-mode sampling ya HW) | time kahan (functions, lines, call tree) | pehla look — `perf record` jaisa, better UI |
| **Microarchitecture Exploration** | top-down tree (upar wala) | "kyun slow, kis dhang" — the killer feature |
| **Memory Access** | DRAM bandwidth, NUMA, cache misses per data structure, latency histogram | memory-bound confirm + which object (folder 32) |
| **Threading** | lock contention, wait time, imbalance, spin | "kyun scale nahi hota" (folder 26–28) |
| **HPC / Vectorization** | SIMD efficiency, FLOPS, vector vs scalar time | numeric kernels (folder 33/05) |
| **I/O** | I/O wait, PCIe, storage | I/O-bound services |
| **Anomaly Detection** | fine-grained per-iteration latency outliers | **jitter / tail** analysis (lesson 06) — HFT relevant |

---

## Ek VTune session (typical)

```bash
# 1. hotspots — where's the time
vtune -collect hotspots -result-dir r_hs -- ./app
vtune -report hotspots -result-dir r_hs

# 2. microarch — WHY (top-down)
vtune -collect uarch-exploration -result-dir r_ua -- ./app
vtune-gui r_ua        # open the top-down tree, click down to the hot leaf

# 3. if memory-bound:
vtune -collect memory-access -result-dir r_ma -- ./app
#    -> per-object DRAM traffic, NUMA remote %, latency

# 4. if threading issue:
vtune -collect threading -result-dir r_th -- ./app
#    -> lock wait time, which lock, which call stack
```

GUI mein: **summary** page top-down %, **bottom-up** page functions sorted,
**source/assembly** view line-level with the counter breakdown, **timeline**
view per-thread activity over time (jitter, imbalance dikhta).

---

## VTune vs `perf` — kab kaunsa

| | **`perf`** | **VTune** |
|---|---|---|
| Cost | free, always there on Linux | free (oneAPI), heavier install |
| Platform | Linux, any CPU (basic events) | Intel best; some AMD; Linux + Windows |
| Top-down | `-M TopdownL1/L2` (numbers) | full recursive tree + hints + source (GUI) |
| Memory analysis | `perf mem`, `perf c2c` (raw) | per-object, NUMA map, latency histogram (polished) |
| Learning curve | steep CLI | GUI-guided, hint-driven |
| Automation / CI | scriptable, tiny | scriptable (`vtune -report`), bigger |
| "quick check on a server" | **`perf`** | — |
| "deep dive, why is this uarch-bound" | — | **VTune** |

Practically: `perf` for the daily 80%, VTune when `perf` says "backend
bound" and you need to know *exactly* which sub-category and which line, or
for polished memory/threading analysis.

---

## Other platform equivalents

| Platform / vendor | Tool |
|---|---|
| Intel | **VTune** (oneAPI), Intel Advisor (vectorization/roofline) |
| AMD | **AMD uProf** (top-down, IBS-based) |
| Apple Silicon / macOS | **Instruments** (Time Profiler, System Trace, Counters) |
| ARM (servers) | **Arm Streamline** / `perf` + Arm PMU, `topdown-tool` |
| NVIDIA GPU | Nsight Systems / Nsight Compute |
| Cross, open | `perf` + `toplev` (pmu-tools — Andi Kleen's top-down script) |

**`toplev.py`** (from `andikleen/pmu-tools`) — brings VTune-style recursive
top-down to plain `perf` on Linux:
```bash
toplev.py -l3 --no-desc ./app        # 3 levels deep, like VTune's tree
```

---

## > **HFT relevance**

> - **`uarch-exploration` on the release build** replaying a captured market
>   session → the top-down tree tells you if the hot path is retiring-bound
>   (good, only algo/SIMD left), DRAM-latency-bound (fix locality — folder
>   32), divider-bound (folder 31/09), or frontend-bound (code too big —
>   PGO/LTO — folder 33/10–11).
> - **Memory Access → per-object** — "the order book's `PriceLevel` array is
>   90% of DRAM traffic" → that's the structure to shrink / re-layout.
> - **Anomaly Detection / timeline** — per-iteration outliers = jitter
>   sources visualised (lesson 06).
> - **Threading** — the housekeeping/aggregator threads' lock waits, and
>   whether the hot thread ever blocks.
> - Numbers are **relative guidance** — VTune's cost model is a model. Final
>   proof: change one thing, re-measure end-to-end p99 (lesson 01).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — top-down % as literal time
"48% Memory Bound" ≈ 48% of pipeline **slots** wasted due to memory, not
48% of wall time. It's a strong direction signal, not a stopwatch.

### Trap 2 — `-O0` / no symbols
Same as `perf` — profile the optimized build with `-g`; otherwise every
line is "hot" and the top-down is dominated by abstraction overhead.

### Trap 3 — VTune's own overhead on tiny events
Sampling-based collections have skid and rate limits like `perf`. For
sub-µs per-iteration analysis use the anomaly/fine-grained modes, not plain
hotspots.

### Trap 4 — trusting hints blindly
"Consider loop blocking" is a heuristic. Verify the hypothesis (is it
actually DRAM-bound? what's the working set?) before spending days on the
suggested fix.

### Trap 5 — Intel tool on AMD and puzzled by gaps
Many Intel-specific PMU events don't exist on AMD → tree nodes show "N/A".
Use uProf or `toplev` on AMD.

### Trap 6 — collecting on a noisy shared box
Co-tenant load pollutes the counters (esp. shared LLC / memory bandwidth).
Isolated / quiet machine for meaningful uarch numbers (lesson 09).

---

## Hands-on

VTune Windows aur Linux dono pe chalta (Intel oneAPI se free download).
Agar Intel CPU hai:
```bash
vtune -collect hotspots -- ./your_app
vtune-gui                       # open the result, explore
```
No VTune / AMD box → `perf` + `toplev`:
```bash
pip install --user pmu-tools    # ya git clone andikleen/pmu-tools
~/pmu-tools/toplev.py -l3 ./35-PROFILING-BENCHMARKING/examples/... 
# (Linux; is repo ke examples release build pe)
```
Windows + non-Intel → is folder ke `perf`/valgrind lessons WSL mein, ya
VTune ka evaluation.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "top-down % = % of time" | % of pipeline **slots**; direction not stopwatch |
| "VTune replaces perf" | perf for daily/quick + servers; VTune for deep uarch dives |
| "VTune only on Windows" | Linux + Windows; Intel CPUs best |
| "the hint is the fix" | hint = hypothesis; verify then act |
| "any box is fine for uarch numbers" | quiet/isolated — shared LLC pollutes counters |
| "no VTune = no top-down" | `toplev.py` (pmu-tools) on plain perf |

---

## Exercises

1. VTune microarch summary: `Retiring 55%`, `Bad Speculation 30%`, `Frontend
   8%`, `Backend 7%`. Where's the problem, and what's the fix direction?

   <details><summary>Answer</summary>

   `Bad Speculation 30%` — nearly a third of pipeline slots are spent on
   work that gets **thrown away by branch mispredicts** (or memory-ordering
   machine clears). That's huge (healthy is <5–10%). `Retiring 55%` is
   okay-ish but capped by the wasted 30%. Fix direction (folder 33/08):
   drill into the tree — VTune will show "Branch Mispredict" and the hot
   branches + source lines. Options: (a) make the branch **predictable**
   (sort the data so it's not 50/50 — the classic sorted-vs-unsorted demo),
   (b) **branchless** — `cmov` / arithmetic / masks / `std::min` (c)
   `[[likely]]`/`[[unlikely]]` + restructure so the common path is
   straight-line, (d) **PGO** (folder 33/11) — feeds real branch
   probabilities to the compiler, which is exactly what mispredict-heavy
   code needs. Re-measure: Bad Speculation should drop and Retiring rise.
   </details>

2. Top-down: `Backend Bound 70%` → `Memory Bound 60%` → `DRAM Bound 45%` →
   `Memory Bandwidth 38%`, `Memory Latency 7%`. Contrast with a different
   app where DRAM Bound splits as `Bandwidth 5%`, `Latency 40%`. Different
   fixes — what?

   <details><summary>Answer</summary>

   **App 1 — Bandwidth-bound (38%):** the memory *bus* is saturated —
   you're moving more bytes/sec than DRAM can supply. Adding parallelism or
   prefetch **won't help** (the wall is total bytes). Fixes: **move fewer
   bytes** — smaller data types, compression, SoA to only touch needed
   fields (folder 32/05), blocking/tiling to reuse data in cache before
   evicting, avoid streaming through huge arrays repeatedly. Multiple cores
   hitting this share the same wall.
   **App 2 — Latency-bound (40%):** the bus has headroom, but individual
   accesses stall ~200 cyc and there isn't enough independent work to hide
   them (low MLP). Fixes: **overlap more misses** — software prefetch ahead
   (folder 32/06), restructure to have many independent pointer chases in
   flight, increase the OoO window's useful work, batch/gather. Or remove
   the pointer-chasing pattern (linked list → array, hash → open
   addressing). Prefetch helps here but hurt App 1.
   The split tells you which of the two very different playbooks to open.
   </details>

3. Your `perf stat -M TopdownL1` on a Linux server says "Backend Bound
   65%". You don't have VTune. How do you get VTune-like depth?

   <details><summary>Answer</summary>

   (1) **`perf stat -M TopdownL2`** (or `-M TopdownL3` on newer kernels/CPUs)
   — deeper metric groups (`tma_memory_bound`, `tma_dram_bound`,
   `tma_l3_bound`, `tma_core_bound`, `tma_divider`, ...). Same numbers VTune
   shows, just CLI. (2) **`toplev.py -l3` / `-l4`** from `andikleen/pmu-tools`
   — a script that programs the right PMU event groups and prints the
   recursive top-down tree with descriptions, very close to VTune's tree,
   Linux + any CPU with the events. `toplev.py --run-sample` can even
   auto-drill and sample the hot spot for the worst node. (3) Targeted `perf
   stat -e cycle_activity.stalls_l3_miss,cycle_activity.stalls_l2_miss,
   ...` to quantify each sub-bucket, and `perf record -e mem_load_retired.
   l3_miss:pp -g` to find *which code/data* causes the DRAM bound. (4)
   `perf mem` / `perf c2c` for the memory / sharing detail VTune's Memory
   Access view gives. So: `TopdownL2/L3` + `toplev` + `perf mem` ≈ VTune's
   microarch + memory analysis, for free, on Linux.
   </details>

---

## Interview questions

1. Top-down 4 categories — Retiring / Bad-Spec / Frontend / Backend — har ek kya.
2. "Backend Bound" ko aage kaise tod-te (Memory vs Core; DRAM vs L3; Bandwidth vs Latency).
3. Bandwidth-bound vs latency-bound — alag fixes, ek line har ek.
4. VTune vs `perf` — kab kaunsa (daily vs deep dive).
5. `toplev.py` — kya deta jo plain `perf stat` nahi.
6. Top-down % ko "% of time" kyun nahi padhna.

---

## Next
→ [`15-sanitizers.md`](15-sanitizers.md)
