# 10 — `perf`: Linux profiler ka practical workflow

## Prerequisites
- `01-why-measure.md` (baseline → profile → hotspot)
- `31-CPU-ARCHITECTURE` (IPC, pipeline, cache), `34-ASSEMBLY` (asm padhna)
- `examples/06_perf_workflow.sh`
- **Linux** (perf = kernel tool; Windows pe VTune / ETW / `xperf`)

## Yeh topic abhi kyun
Micro-benchmarks (lesson 08–09) ek function ka number dete. Par bade
program mein **kaunsa function** slow hai — yeh `perf` batata. `perf` Linux
ka default profiler hai: hardware performance counters + sampling, minimal
overhead, no recompile. Ek HFT / systems engineer ka roz ka tool.

---

## Do modes: counting vs sampling

| | **Counting** (`perf stat`) | **Sampling** (`perf record`) |
|---|---|---|
| Kya | poore run ke exact counter totals | periodic snapshots of PC + stack |
| Overhead | ~0 (hardware counters) | low (~1–5%, sample rate pe) |
| Deta | "IPC 0.8, 5M cache-misses, 2% branch-miss" | "time kahan: 40% `parse`, 22% `hash`" |
| Kab | pehla look — "kis cheez se bound?" | "kaunsa function / line" |

Workflow: **pehle `perf stat`** (bound kya hai?), **phir `perf record`**
(kahan?), **phir `perf annotate`** (function ke andar kaunsi line/instr).

---

## `perf stat` — kis cheez se bound

```bash
perf stat ./app
perf stat -r 5 ./app                    # 5 runs + mean/stddev
perf stat -d ./app                      # + cache detail (-d -d -d = aur zyada)
perf stat -e cycles,instructions,cache-misses,branch-misses ./app
```

Typical output aur padhna:

```
     12,345.67 msec task-clock       #    1.00 CPUs utilized
   28,900,000,000  cycles            #    2.34 GHz              <- effective freq
   35,000,000,000  instructions      #    1.21  insn per cycle  <- IPC
    4,200,000,000  branches          #  340.2 M/sec
       80,000,000  branch-misses     #    1.90% of all branches <- predictor
    1,500,000,000  L1-dcache-loads
       95,000,000  L1-dcache-load-misses #  6.3% of L1 loads
       12,000,000  LLC-load-misses   #  <- DRAM trips: har ~200+ cyc
```

### Key numbers

- **IPC (insn per cycle)** — modern OoO cores 4-wide retire kar sakte.
  - `IPC > 2` → compute-bound aur healthy.
  - `IPC ~1` → some stalls (deps, L2 misses, mispredicts).
  - `IPC < 0.5` → **heavily stalled** — usually memory (LLC/DRAM misses) ya
    a long dependency chain (folder 31).
- **branch-misses %** — `> 2–3%` on a hot path → branchy code; `[[likely]]`
  / restructure / branchless / PGO (folder 33/08).
- **L1 / LLC miss rates** — LLC-load-misses ka absolute count × ~200 cyc ≈
  cycles lost to DRAM. Agar woh total cycles ka bada hissa → memory-bound
  (folder 32 → layout, blocking, prefetch).
- **GHz** (`cycles / task-clock`) — run-to-run stable? Nahi → frequency
  scaling (lesson 09 jaal 8).
- **CPUs utilized** — `~1.0` single-threaded; `> 1` threaded; `< 1` blocked
  on I/O (lesson 02).

### Top-down (Intel/AMD)

```bash
perf stat -M TopdownL1 ./app          # ya --topdown
```
Ek run mein 4 percentages — pipeline slots kahan gaye:

| Bucket | Matlab | Fix direction |
|---|---|---|
| **Retiring** | useful work | good; SIMD/algo se hi aur |
| **Frontend Bound** | instruction fetch/decode starve | I-cache/iTLB, code size, `[[unlikely]]` out-of-line (33/08) |
| **Backend Bound** | execution units / memory stall | usually memory (32) ya port pressure / deps (31) |
| **Bad Speculation** | mispredicted work thrown away | branch layout / branchless / PGO (33/08) |

Yeh sabse fast "kis dhang mein jaaun" signal hai.

---

## `perf record` + `perf report` — kahan time ja raha

```bash
perf record -F 999 -g -- ./app         # 999 Hz sample, -g = call graphs
perf report                            # interactive TUI
perf report --stdio --percent-limit 1  # text, >1% wale
```

- **`-F 999`** — samples/sec. 997/999 (prime) taaki periodic app behaviour
  se sync na ho. Higher = finer but more overhead + bigger `perf.data`.
- **`-g`** — call graphs. Frame-pointer based (needs
  `-fno-omit-frame-pointer` at compile, warna broken stacks). Bade C++:
  `--call-graph dwarf` (DWARF CFI unwind — no recompile flag needed, bigger
  data, slower) ya `--call-graph lbr` (hardware Last Branch Records, cheap,
  limited depth).
- **`-e <event>`** — kis event pe sample. Default `cycles`. Bhi:
  `perf record -e cache-misses ./app` → "kaun sa code cache-misses karta"
  (not just where time goes).
- **`-c <count>`** — har N events pe ek sample (instead of `-F` rate).

`perf report` reading:
- **Self** — us function mein khud kitna time (uske callees chhod ke).
- **Children** — us function + uske saare callees.
- Sort by **Self** to find the actual hot leaf; by **Children** to find hot
  subtrees.
- `+` to expand call graph, `a` to annotate, `/` to filter.

`06_perf_workflow.sh` ka demo: `sum_mod` (runtime `%` — divider unit) aur
`chase` (random-stride walk — cache misses) dono top pe aate — ek compute-
bound hotspot, ek memory-bound. `perf report --stdio` unhe naam se dikhata.

---

## `perf list` — kya-kya measure ho sakta

```bash
perf list                              # sab events
perf list cache                        # cache-related
perf list | grep -i tlb
```

Categories:
- **Hardware** — `cycles`, `instructions`, `cache-misses`, `branch-misses`,
  `bus-cycles`.
- **Hardware cache** — `L1-dcache-load-misses`, `LLC-load-misses`,
  `dTLB-load-misses`, `iTLB-load-misses`.
- **Software** — `task-clock`, `context-switches`, `page-faults`,
  `cpu-migrations` (kernel se, always available, even in VMs).
- **PMU raw / named** — `cycle_activity.stalls_l3_miss`,
  `frontend_retired.latency_ge_16`, etc. (CPU-specific, `perf list` se exact
  naam).
- **Tracepoints** — `sched:sched_switch`, `syscalls:sys_enter_*`,
  `block:block_rq_issue`, `irq:*`.

---

## Setup gotchas

```bash
# perf allowed? (paranoia level)
cat /proc/sys/kernel/perf_event_paranoid
#  2 (default many distros) = no kernel/tracepoint, user-space OK-ish
#  1 = user + kernel profiling
#  -1 = everything
sudo sysctl -w kernel.perf_event_paranoid=1

# symbols missing ("[unknown]" in report):
#  - compile with -g (aur -fno-omit-frame-pointer for -g stacks)
#  - don't strip the binary (ya keep a separate .debug)
#  - system libs: install -dbg / -debuginfo packages, ya debuginfod
#  - JIT (JVM etc.): perf map files

# containers / VMs:
#  - many PMU events unavailable -> perf falls back / shows <not supported>
#  - --call-graph dwarf works even without frame pointers
```

---

## Ek poora session (recipe)

```bash
# 1. build with symbols, optimized
g++ -std=c++20 -O2 -g -fno-omit-frame-pointer app.cpp -o app

# 2. what's it bound on?
perf stat -r 3 -d ./app
perf stat -M TopdownL1 ./app

# 3. where is the time?
perf record -F 999 -g -- ./app
perf report --stdio --percent-limit 1 | head -30

# 4. inside the hot function
perf annotate -i perf.data --stdio -l hot_function | head -50   # lesson 11

# 5. targeted: who causes the cache misses?
perf record -e LLC-load-misses -g -- ./app
perf report --stdio

# 6. after a fix — compare
perf stat -r 5 ./app_v1 ; perf stat -r 5 ./app_v2
perf diff perf.data.v1 perf.data.v2
```

---

## > **HFT relevance**

> - `perf stat -M TopdownL1` release build pe → "hum retiring-bound hain ya
>   frontend/backend/bad-spec" — pehli baat jo poore hot path ke baare mein.
> - `perf record -e cycles:pp` (precise) hot path pe, **isolated core pe**,
>   ek replayed market data session ke saath — real workload, real numbers.
> - `perf annotate` (lesson 11) — us `div` / us `mov [mem]` pe % → exact
>   instruction to fix, folder 34 ke saath cross-check.
> - `perf stat -e dTLB-load-misses,iTLB-load-misses` — huge pages worth it?
>   (folder 32/08.)
> - Steady-state pe `perf stat -p <pid> -- sleep 30` — live process ko
>   observe kiye bina restart.
> - `perf.data` ko commit / archive karo har release ke saath — regressions
>   `perf diff` se turant.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `perf record` bina `-g` / bina frame pointers
Call graph tuta ("[unknown]" parents). `-fno-omit-frame-pointer` ya
`--call-graph dwarf`.

### Trap 2 — `-O0` binary profile karna
Har line "hot" dikhti (no inlining, stack traffic). Release flags pe profile.

### Trap 3 — `perf stat` ka IPC blindly trust
Blocked/sleeping time IPC mein count nahi hota (task-clock CPU-time hai).
Ek I/O-bound app ka IPC achha dikh sakta jabki wall-time bura. `perf stat`
+ `time` dono.

### Trap 4 — sample skid
`cycles` event ka sample **kuch instructions late** attribute ho sakta
(pipeline skid) — annotate mein "wrong" line pe % dikhe. Use `cycles:pp`
(precise, PEBS/IBS) for accurate instruction attribution (lesson 11).

### Trap 5 — `-F` bahut high
`-F 50000` → huge overhead + massive `perf.data` + perf itself distorts the
profile. 999–4000 usually enough.

### Trap 6 — single short run
App 200 ms chalta → few hundred samples → noisy profile. Loop the workload,
ya `--benchmark`-style repeat, ya profile a longer representative run.

---

## Hands-on (Linux)

```bash
bash 35-PROFILING-BENCHMARKING/examples/06_perf_workflow.sh
```
Yeh khud ek demo (`sum_mod` = runtime `%`, `chase` = random walk) banata aur
`perf stat` (IPC, cache), `perf stat -d`, top-down, `perf record`/`report`,
`perf annotate` sab chala ke dikhata. Windows pe: script `.sh` hai,
`folder`/`checkall` skip karte — Linux/WSL pe chalao.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "perf = ek command" | `stat` (count) → `record`/`report` (sample) → `annotate` |
| "IPC high = fast" | IPC CPU-time based; blocked time invisible — `time` bhi |
| "profile ke liye recompile chahiye" | nahi — `-g`/`-fno-omit-fp` helps but perf works on any binary |
| "`-F 99999` = better profile" | overhead + distortion; 999–4000 kaafi |
| "annotate ka top line = the bug" | skid — `cycles:pp` for precise attribution |
| "ek 200ms run kaafi" | few samples; loop the workload for a solid profile |

---

## Exercises

1. `perf stat ./app` deta: `IPC 0.35`, `LLC-load-misses 8.0e9`, total
   `cycles 4.0e10`, `branch-misses 0.4%`. App CPU-bound hai (CPUs utilized
   0.99). Kya bottleneck, aur agla step?

   <details><summary>Answer</summary>

   `IPC 0.35` — core ~65% cycles idle waiting. `branch-misses 0.4%` — fine,
   not the issue. `LLC-load-misses 8.0e9` × ~200 cyc/miss ≈ **1.6e12 cycles
   worth of DRAM latency exposure** — but total cycles only 4.0e10, so
   misses **massively overlap** (MLP) yet still dominate: this is
   **memory-latency-bound** (folder 32). Even 4.0e10 / 8.0e9 = 5 cycles per
   miss "budget" — the OoO window + prefetchers hide most, but there's not
   enough independent work to hide all. Next: `perf record -e LLC-load-misses
   -g` → which code/data structure. Then folder 32 toolkit — improve
   locality (AoS→SoA, blocking), add prefetch, shrink the working set,
   huge pages for TLB. Also `perf stat -M TopdownL1` should show high
   "Backend Bound / Memory Bound".
   </details>

2. Do runs of `perf stat` on the same binary/input: run 1 `2.10 GHz`, run 2
   `2.85 GHz`; `instructions` identical, `cycles` 30% different, wall-time
   30% different. Kya ho raha, aur "faster" number kaunsa?

   <details><summary>Answer</summary>

   **Frequency scaling.** Instructions identical → same work. Cycles ratio ≈
   frequency ratio (2.85/2.10 ≈ 1.36) → the CPU just ran at different clock
   speeds. Run 2 got turbo / a cooler start; run 1 was throttled or on a
   `powersave` governor. **Neither number is "the" number** — report
   **cycles per operation** (frequency-invariant): both runs have the same
   cycles/op, so the true answer is "X cycles/op", and ns/op = that ÷
   whatever frequency you'll actually run at in production. Fix the
   measurement: `cpupower frequency-set -g performance -f 2.5GHz`, turbo off,
   re-run — `GHz` should be stable across runs.
   </details>

3. `perf report` shows `memcpy` at 35% self time — highest in the profile.
   Is that the thing to optimize? What do you check?

   <details><summary>Answer</summary>

   `memcpy` being hot is a **symptom**, not usually the target — `memcpy`
   itself is already optimal (SIMD / `rep movsb`). The real questions: (1)
   **Who calls it, and why so much?** Expand the call graph (`-g`, look at
   callers) — maybe you're copying a large buffer that could be passed by
   reference / moved / used in place. (2) **What size?** Many tiny copies →
   per-call overhead; a few huge copies → memory bandwidth. `perf annotate
   memcpy` won't help (it's optimal asm); the fix is at the **call site** —
   eliminate the copy (views, `std::span`, move semantics, arena reuse), not
   speed up `memcpy`. Same logic for `malloc`, `std::_Rb_tree` ops,
   `std::string` ctor showing up hot — the leaf is fine, the caller's
   algorithm/data-structure choice is the bug.
   </details>

---

## Interview questions

1. `perf stat` vs `perf record` — counting vs sampling, kab kaunsa.
2. IPC — kya batata, `<0.5` / `~1` / `>2` ka matlab.
3. Top-down (Retiring / Frontend / Backend / Bad-Spec) — har bucket ka fix direction.
4. `-g` call graphs — frame-pointer vs dwarf vs lbr, trade-offs.
5. `perf_event_paranoid`; "[unknown]" symbols — kaise fix.
6. Sample skid — kya hai, `cycles:pp` kyun.
7. Ek hot `memcpy` / `malloc` ko kaise approach karo (leaf vs call site).

---

## Next
→ [`11-perf-advanced.md`](11-perf-advanced.md)
