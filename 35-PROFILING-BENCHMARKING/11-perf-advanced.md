# 11 — `perf` advanced: annotate, counters, precise events

## Prerequisites
- `10-perf-basics.md`
- `34-ASSEMBLY` (asm padhna — annotate ka output asm hai)
- `31-CPU-ARCHITECTURE/09` (latency/throughput), `32` (cache), `27` (false sharing)

## Yeh topic abhi kyun
`perf report` (lesson 10) function-level batata "40% `parse` mein." Ab —
`parse` ke **andar kaunsi line / instruction**, aur **kyun** (cache miss?
mispredict? divider? port pressure?). Iske liye `perf annotate`, precise
events (PEBS/IBS), aur specialised sub-tools (`perf mem`, `perf c2c`).

---

## `perf annotate` — line/instruction level

```bash
perf record -F 4000 -g -- ./app
perf annotate --stdio -l hot_function        # -l = source line numbers
perf annotate hot_function                   # interactive (TUI)
```

Output: source interleaved with asm, har instruction ke aage **% of the
function's samples**:

```
       :      for (int i = 0; i < n; ++i)
  0.21 :  4a1: add    $0x1,%rax
  0.05 :  4a5: cmp    %rdx,%rax
  0.12 :  4a8: je     4d0
       :          s += a[i] % m;
 52.30 :  4aa: idiv   %rcx              <- 52% yahan: integer divide
  1.80 :  4ae: add    %rdx,%r8
```

**52% on one `idiv`** → the modulo. Fix (folder 34/08): agar `m` compile-
time hota compiler reciprocal-multiply karta; runtime `m` → `libdivide` ya
algorithm change. `06_perf_workflow.sh` exactly yeh demo karta (`sum_mod`).

### Kya dekhna
- **Ek instruction pe bada %** — that's the bottleneck instruction:
  - `div`/`idiv`, `sqrt`, `vdivps` → expensive arithmetic (folder 31/09).
  - `mov (reg), reg` (a load) pe bada % → **cache miss** on that load
    (folder 32).
  - a branch (`je`/`jne`) pe bada % → **mispredict** stall (folder 33/08).
  - `lock ...` / `xchg` pe → atomic contention (folder 27).
- **% spread evenly** across many instructions → the loop is just doing that
  much work; look at algorithm / SIMD, not one instr.
- **Skid**: `cycles` (non-precise) attributes the sample to an instruction a
  few past the real culprit — the % may sit on the instruction *after* the
  slow load. Use precise events (below).

---

## Precise events: PEBS / IBS

Normal sampling: interrupt fires "around" the event → **skid** (attribution
off by a few / dozens of instructions).

**PEBS** (Intel, Processor Event-Based Sampling) / **IBS** (AMD,
Instruction-Based Sampling): hardware records the **exact** instruction
pointer (and often the data address, latency) at the event.

```bash
perf record -e cycles:pp -g -- ./app         # :pp = precise (PEBS/IBS)
perf record -e cycles:ppp -- ./app           # :ppp = max precision
perf annotate --stdio hot_function           # now the % sits on the RIGHT instr
```

`:p` = some skid reduction, `:pp` = precise, `:ppp` = precise + no skid
(where HW supports). **Always use `:pp` for annotate-level work** — non-
precise annotate can point you at the wrong line.

---

## Hardware counters — beyond the basics

`perf list` se CPU-specific events. Useful ones for C++ perf work:

### Memory / cache
```bash
perf stat -e L1-dcache-load-misses,l2_rqsts.miss,LLC-load-misses,\
dTLB-load-misses,dTLB-store-misses ./app
perf stat -e cycle_activity.stalls_l3_miss,cycle_activity.stalls_l2_miss ./app
```
`cycle_activity.stalls_l3_miss` = cycles the core was stalled specifically
waiting on an L3 miss (DRAM). Divide by total cycles → **fraction of time
lost to DRAM**. This is the number that says "memory-bound" quantitatively.

### Frontend
```bash
perf stat -e frontend_retired.latency_ge_16,icache_64b.iftag_miss,\
itlb_misses.walk_completed ./app
```
Big `icache`/`itlb` misses → code too big / bad layout → PGO, function
ordering, `[[unlikely]]` cold-splitting (folder 33/08, 11).

### Bad speculation
```bash
perf stat -e br_misp_retired.all_branches,\
br_misp_retired.near_taken,machine_clears.count ./app
```

### Ports / execution (Intel)
```bash
perf stat -e uops_dispatched_port.port_0,...,exe_activity.bound_on_stores ./app
```
Port pressure → one execution unit saturated (e.g. all your work is FP-mul
on one port). Rare; usually you're memory-bound first.

---

## `perf mem` — which loads/stores are slow, and where from

```bash
perf mem record -- ./app
perf mem report --stdio
```
Per memory access: **latency**, and **where it was served from** (L1 / L2 /
L3 / local DRAM / remote DRAM / from another core's cache). Finds:
- The specific data structure causing DRAM trips.
- Remote-NUMA accesses (folder 32) — "served from remote DRAM" rows.
- The exact `struct` field, often.

---

## `perf c2c` — cache-to-cache / false sharing

```bash
perf c2c record -- ./app
perf c2c report --stdio
```
Detects **false sharing** (folder 27): two threads writing different
variables that live on the **same cache line** → the line ping-pongs
between cores' caches (HITM = "hit modified in another core"). Output shows
the offending cache line, the two offsets, and the two code locations.
Fix: pad / align to 64 B (`alignas(64)`), or split per-thread.

This is *the* tool for "why does my multithreaded code not scale" when the
answer is contention on shared cache lines.

---

## LBR — cheap call graphs + branch info

```bash
perf record --call-graph lbr -- ./app        # Last Branch Records (HW ring)
```
- **LBR** stores the last ~16–32 taken branches in hardware — call stacks
  reconstructed cheaply (no frame pointers, no DWARF unwind), but **limited
  depth**. Great for shallow hot paths, low overhead.
- `perf record -b` (branch stack) → analyse branch behaviour, build
  **basic-block level** profiles, feed **AutoFDO** (folder 33/11 — PGO
  without an instrumented build).

---

## `perf script` — raw samples out

```bash
perf script                                  # every sample: time, pid, ip, stack
perf script | ./stackcollapse-perf.pl | ./flamegraph.pl > fg.svg   # lesson 12
perf script -F time,event,ip,sym             # custom fields
```
`perf script` is the bridge to flame graphs, custom analysis, Firefox
Profiler / Speedscope imports.

---

## Counting vs sampling — the deeper distinction

- **Counting** (`perf stat`) — exact totals. Use for: "did this event count
  go down after my fix?" Deterministic (modulo multiplexing — see below).
- **Sampling** (`perf record`) — statistical. Use for: "where / which line."
  Error ~`1/√(samples in that bucket)`; a function with 25 samples is
  ±20%.
- **Counter multiplexing** — the CPU has a fixed number of programmable
  counters (~4–8). Ask for more events than that and `perf` **time-slices**
  them → each event measured only part of the time, then scaled up. `perf
  stat` shows `[ 62.50% ]` next to multiplexed events. Fewer events per run
  = more accurate; or use `--no-scale` and interpret carefully.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — non-precise annotate
`perf record -e cycles` (no `:pp`) → annotate % skids past the real
instruction. Always `cycles:pp` for instruction-level work.

### Trap 2 — counter multiplexing ignored
Asked for 12 events, CPU has 4 → each seen 33% of the time, scaled ×3.
Noisy. Split into 3 runs of 4 events, or accept the `[%]` and don't
over-interpret.

### Trap 3 — `perf c2c` / `perf mem` without precise-capable HW / kernel
Older CPUs / VMs → limited or no data. Check `dmesg` / `perf mem record`
warnings.

### Trap 4 — annotate on an inlined function
Hot code inlined into 5 callers → its samples spread across 5 places, none
individually hot. Look at the *source line* view, or build with
`-fno-inline` for a diagnostic pass (not for the final number).

### Trap 5 — reading port-pressure counters first
99% of C++ perf problems are memory or branches or algorithm. Port pressure
is a last-5% concern — don't start there.

### Trap 6 — `stalls_l3_miss` as "cycles lost" literally
It's cycles the *allocator/backend* was stalled with an outstanding L3
miss — overlapping misses share stall cycles. It's a strong *relative*
signal ("this went from 40% to 10% of cycles"), not a literal subtractable
time.

---

## Hands-on (Linux)

```bash
bash 35-PROFILING-BENCHMARKING/examples/06_perf_workflow.sh
# step 3 runs `perf annotate sum_mod` and `perf annotate chase`:
#   sum_mod -> big % on `idiv`   (runtime modulo)
#   chase   -> big % on the load `mov (...,%rax,4),%e..`  (cache miss)

# precise:
perf record -e cycles:pp -g -- ./_perf/demo
perf annotate --stdio -l chase

# false sharing (write a 2-thread demo that shares a cache line):
perf c2c record -- ./app ; perf c2c report --stdio
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "annotate % = exact culprit" | skid — use `cycles:pp` (PEBS/IBS) |
| "more `-e` events = more info" | counter multiplexing → each less accurate |
| "high port counter = my bug" | memory / branches / algo first; ports last |
| "`perf mem`/`c2c` always work" | need precise-capable HW + kernel; VMs limited |
| "`stalls_l3_miss` cycles = time lost" | relative signal, overlapping misses |
| "LBR call graph = full stack" | shallow (~16–32 branches) — deep stacks need dwarf |

---

## Exercises

1. `perf annotate` (non-precise) shows 60% on `add %rdx, %rax` right after a
   `mov (%rsi), %rdx`. `add` is a 1-cycle instruction. What's really going
   on, and how to confirm?

   <details><summary>Answer</summary>

   **Skid.** The sample is attributed to the `add`, but the `add` can't cost
   60% — it's 1 cycle, fully pipelined. The instruction *before* it, `mov
   (%rsi), %rdx`, is a **load**, and if `%rsi` points to memory not in cache
   that load stalls ~200 cycles. The `cycles` interrupt fires while waiting,
   and by the time it's handled the PC has moved to the `add` (the next
   instruction that will retire) → the % lands on `add`. Confirm: re-record
   with `-e cycles:pp` — the % moves onto the `mov` load. Or `perf record -e
   mem_load_retired.l3_miss:pp` and annotate — the miss samples will be on
   the `mov`. Fix is at the load: prefetch, change layout so `%rsi`'s target
   is hot (folder 32).
   </details>

2. Multithreaded app: `perf stat` shows good IPC per thread in isolation,
   but scaling from 1→8 threads gives only 2.5× throughput. `perf stat`
   with 8 threads shows IPC dropped from 2.1 to 0.6. What tool next, and
   what are you looking for?

   <details><summary>Answer</summary>

   IPC collapsing under threading = **contention**. Next tool: **`perf c2c
   record` / `perf c2c report`**. Looking for: a **hot cache line with HITM**
   (hit-modified-in-another-core) — two or more threads writing to addresses
   on the same 64 B line, causing the line to bounce between cores' L1/L2
   caches every write (~100+ cycles each time). `perf c2c` names the cache
   line, the byte offsets, and the source locations. Common culprits: (a)
   **false sharing** — unrelated per-thread counters/flags packed into one
   struct/array without padding; (b) **true sharing** — a genuinely shared
   atomic counter / lock / queue head hammered by all threads. Fix: `alignas(64)`
   per-thread data (false sharing), or redesign to per-thread accumulation +
   periodic merge / a sharded / lock-free structure (true sharing — folder
   28). Also check `perf stat -e machine_clears.count` (memory ordering
   clears) and lock stats.
   </details>

3. You asked `perf stat` for 14 events. Output shows `[35.71%]` next to most
   of them. The `branch-misses` count looks 3× higher than a previous run
   with only 4 events. Bug, or expected?

   <details><summary>Answer</summary>

   **Expected — counter multiplexing artefact / noise, not a real
   regression.** The CPU has ~4 general PMU counters; you asked for 14
   events, so `perf` round-robins them — each event is only actually counted
   ~35% of the time, then **scaled up ×2.8** to estimate the full total.
   That scaling amplifies noise: if branch-misses happened to be bursty
   during the windows this event was scheduled, the extrapolation
   over/under-shoots. The 4-event run measured `branch-misses` ~100% of the
   time → accurate. Fix: measure the events you care about in **small groups
   that fit the counter budget** (≤4–6), one `perf stat` run per group, or
   use `perf stat --no-scale` and know you're seeing partial counts. Never
   compare a multiplexed count against a non-multiplexed one.
   </details>

---

## Interview questions

1. `perf annotate` — kya dekhte ho (per-instruction %), aur skid ka problem.
2. PEBS / IBS / `:pp` — precise events kyun, kab zaroori.
3. `cycle_activity.stalls_l3_miss` — kaise "memory-bound" ko quantify karta.
4. `perf c2c` — false sharing kaise detect, HITM kya hai, fix.
5. `perf mem` — per-access latency + "served from where", kis liye.
6. LBR call graphs vs DWARF unwind — trade-offs; AutoFDO se connection.
7. Counter multiplexing — kya hai, output mein kaise dikhta, kaise avoid.

---

## Next
→ [`12-flame-graphs.md`](12-flame-graphs.md)
