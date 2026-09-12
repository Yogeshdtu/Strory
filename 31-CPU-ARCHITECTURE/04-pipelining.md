# 04 — Pipelining: stages, throughput vs latency, hazards

## Prerequisites
- `01-how-cpu-works.md` (fetch-decode-execute-retire)
- `03-instruction-set.md` (µops)

## Yeh topic abhi kyun
Pipelining = "car factory assembly line" for instructions. Ek instruction ke
poora hone ka wait kiye bina, agli instruction ko shuru kar do jab pehli ek
stage aage badh jaye. Yahi se aati hai **latency vs throughput** ki distinction
(examples `01`, `02`), aur **hazards** — woh cheezein jo pipeline ko rok deti
hain (dependency chains, branches, cache misses). Poora HFT low-latency mindset
inhe minimize karne ka hai.

---

## Assembly line analogy

Ek naive CPU: fetch(1) → decode(1) → execute(1) → retire(1), phir agli
instruction. Har instruction 4 cycles, throughput = 0.25 IPC (instructions per
cycle).

Pipelined: 4 alag hardware units, har cycle har unit ek alag instruction pe kaam
karta:

```
cycle:      1     2     3     4     5     6     7
inst A:   [IF]  [ID]  [EX]  [WB]
inst B:         [IF]  [ID]  [EX]  [WB]
inst C:               [IF]  [ID]  [EX]  [WB]
inst D:                     [IF]  [ID]  [EX]  [WB]
```

- **Latency of one instruction:** still 4 cycles (start to result).
- **Throughput:** after fill-up, **1 instruction retires per cycle** (IPC = 1).
- Real x86 pipelines: **15–20 stages** (fetch, predecode, decode, rename,
  allocate, schedule, ~several execute, memory, retire...), and **superscalar**
  (file `05`) so multiple per stage → IPC can be 3–4+.

---

## Latency vs throughput — the core distinction

For a single instruction type (from Agner Fog / uops.info tables, file `09`):

| Instruction | Latency (result-to-use) | Throughput (recip: how often you can start one) |
|---|---|---|
| `add`, `and`, `xor`, `lea` | 1 cycle | 0.25 cycle (4 ALU ports → 4/cycle) |
| `imul r64` | ~3 cycles | ~1 cycle (1 multiplier port) |
| `div r64` | ~20–45 cycles | ~20–45 (**not pipelined**) |
| `load` (L1 hit) | ~4–5 cycles | ~0.5 cycle (2 load ports) |
| FP `addss`/`mulss` | ~3–4 cycles | ~0.5 cycle |
| `vfmadd...ps` (AVX2 FMA) | ~4 cycles | ~0.5 cycle |

- **Dependency chain** (each op needs the previous result) → **latency-bound**.
  Cost ≈ sum of latencies. Example `01`/`02`: a serial `imul` chain ≈ 3 cyc/op.
- **Independent ops** (no dependency) → **throughput-bound**. Cost ≈ 1/throughput.
  Example `02`: 4 independent `imul` chains ≈ **4× faster** (~0.75 cyc/op),
  because the multiplier port starts a new `imul` every cycle while 4 chains'
  results are in flight.

**Measured on this box** (example `02`, ~2 GHz): serial chain **~1.2–1.7 ns/op**;
4 parallel chains **~0.3–0.4 ns/op (~4×)**; 8 parallel chains **no further gain**
(multiplier port saturated at 1 `imul`-start/cycle).

**Design rule:** on a latency-critical dependency chain, use short-latency ops
(`add`/`lea` not `imul`/`div`), and **interleave independent work** so the OoO
engine (file `06`) can extract instruction-level parallelism.

---

## Pipeline hazards — what stalls the line

### 1. Data hazard (RAW — read-after-write)
`b = a + 1; c = b * 2;` — `c` can't execute until `b`'s add finishes. On a
serial chain this is *the* bottleneck. Renaming (file `02`, `06`) removes the
*false* hazards (WAW, WAR); the *true* RAW chain remains.

### 2. Control hazard (branches)
The CPU doesn't know a branch's outcome at fetch time. It **predicts** (file
`07`) and fetches speculatively. Mispredict → flush all speculated work + refill
the pipeline = **~15–20 cycles** wasted. Example `03`: an unpredictable branch is
**~6–7× slower** than a predictable one.

### 3. Structural hazard
Not enough hardware units. E.g. two `div`s in flight — only one divider, the
second waits. Or 3 loads in one cycle — only 2 load ports, one waits.

### 4. Memory hazard (the big one — folder 32)
A `load` that misses L1 (~12 cyc), L2 (~40 cyc), or L3 (~200–400 cyc to DRAM).
The dependent instructions stall for that whole time. OoO hides *some* of it by
running independent work, but a long dependency chain on missing data stalls.

---

## Bubbles, flushes, and refill

- **Bubble** = a cycle where a stage has no work (waiting on a hazard). The
  pipeline "has a hole".
- **Flush** = discard all instructions after a mispredicted branch / faulting
  instruction. Everything speculatively fetched/executed is thrown away.
- **Refill** = fetch + decode + rename the correct path from scratch. This is
  the ~15–20 cycle mispredict penalty — proportional to pipeline depth (deeper
  pipeline = higher clock, but costlier mispredict).

`perf stat` counters that measure this: `stalled-cycles-frontend`,
`stalled-cycles-backend`, `branch-misses`, `bubbles`.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — long serial dependency chains in the hot loop
A running hash, checksum, or accumulator that each iteration depends on the last
→ latency-bound. Split into N partial accumulators, combine at the end (example
`02`; folder 20/28 for reductions).

### Trap 2 — `div` / `sqrt` inside a dependency chain
Not pipelined; each waits for the previous. If you must, pull it out of the
critical path or replace it (reciprocal multiply, Newton-Raphson `rsqrt`).

### Trap 3 — assuming the OoO engine "fixes" a bad dependency structure
It only overlaps *independent* work. If your code is one long chain, there's
nothing to overlap. You have to expose the parallelism (multiple accumulators,
SIMD lanes, loop unrolling).

### Trap 4 — micro-benchmarking a single instruction
You mostly measure `rdtsc` + fences (example `08`). Time a loop of thousands and
divide; use a dependency chain to measure latency, independent ops to measure
throughput.

### Trap 5 — ignoring that a deeper pipeline = costlier mispredict
A 4 GHz CPU with a 19-stage pipeline pays ~19 cycles ≈ 4.75 ns per mispredict.
On a random-outcome hot branch at millions/sec, that's real. Branchless (file
`08`, example `04`) or predictable data.

### Trap 6 — `-O0` benchmarks
At `-O0` every variable round-trips to the stack every statement — you measure
loads/stores, not the pipeline. **Always `-O2`+** for CPU-behaviour benchmarks
(and `keep()`/`DoNotOptimize` barriers so `-O2` doesn't delete the loop —
example `01`).

---

## > **HFT relevance**

> - **The hot path is a dependency graph.** Draw it: which computation feeds
>   which. The **critical path** (longest chain of dependent latencies) is your
>   floor. Shorten it: fewer `imul`/`div`, hoist invariants, replace long-latency
>   ops, break serial reductions into parallel ones.
> - **Expose parallelism:** multiple accumulators, SIMD (file `10`), software
>   pipelining, unrolling — give the OoO engine independent work to overlap the
>   latencies.
> - **Kill unpredictable hot branches** (file `07`, `08`) — each mispredict is a
>   full pipeline refill.
> - **Keep the hot working set in L1/L2** (folder 32) — a memory hazard is the
>   biggest bubble of all.
> - **Measure with `perf stat`:** `instructions`, `cycles` (→ IPC),
>   `branch-misses`, `stalled-cycles-*`. IPC well below ~2 on a compute loop
>   means you're stalling on something — chase it.

---

## Hands-on

```bash
# examples 01 + 02: latency vs throughput, serial vs parallel chains
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/01_instruction_timing.cpp
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/02_pipeline_stall.cpp

# on Linux: IPC + stall breakdown of a loop
perf stat -e cycles,instructions,branches,branch-misses,stalled-cycles-frontend,stalled-cycles-backend ./your_prog

# llvm-mca: static pipeline analysis of a code snippet (great for critical-path)
g++ -std=c++20 -O2 -S your.cpp -o - | llvm-mca -mcpu=native
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "pipelining makes each instruction faster" | latency unchanged; **throughput** improves |
| "OoO fixes bad code" | only overlaps *independent* work; a serial chain has none |
| "one accumulator is fine" | latency-bound; N partial accumulators → ~N× |
| "mispredict costs a few cycles" | ~15–20 (pipeline depth); ~5 ns at 4 GHz |
| "`div` will pipeline like `mul`" | not pipelined; each waits for the last |
| "-O0 benchmark shows CPU behaviour" | shows stack traffic; use -O2 + barriers |

---

## Exercises

1. `imul` latency ~3 cyc, throughput ~1/cyc. Ek loop `for i: a = a*b*c*d`
   (`a` carried) kitne cyc/iter? Aur `for i: a=a*b; e=e*c; f=f*d;` (3 chains)?

   <details><summary>Answer</summary>

   `a = a*b*c*d`: three dependent `imul`s per iteration, and the next iteration's
   first `imul` needs this iteration's `a` → the chain is `... → a*b → *c → *d →
   (next) a*b → ...`. That's 3 × 3-cyc latency = **~9 cycles/iter** (roughly;
   `b,c,d` are loop-invariant so maybe 3 muls of 3 cyc each in a chain).
   Three independent chains `a*=b; e*=c; f*=d;`: each is a 3-cyc-latency chain,
   but they're independent, so the multiplier port (1 `imul`-start/cycle) is the
   limit → **~3 cycles/iter** for all three (one `imul` issued per cycle,
   pipelined). ~3× faster. This is example `02`'s lesson.
   </details>

2. Ek 20-stage pipeline, 4 GHz. Ek hot branch jiska outcome ~50% random hai,
   1 million times/sec chalti. Mispredict se kitna time waste (per second)?

   <details><summary>Answer</summary>

   ~50% of 1M = 500,000 mispredicts/sec. Each ≈ 20 cycles / 4 GHz = 5 ns.
   500,000 × 5 ns = **2.5 ms/sec** = 0.25% of wall time on *that one branch's*
   mispredictions. Sounds small, but it's pure jitter — those 5 ns spikes land
   on individual latency-critical events (a quote update, an order decision),
   inflating p99. Branchless or predictable-by-construction removes it.
   </details>

3. Tumhara profiler kehta hot loop ki IPC 0.7 hai (compute-heavy loop,
   expected ~2–3). Kya diagnose karoge?

   <details><summary>Answer</summary>

   IPC 0.7 = the CPU is retiring less than one instruction per cycle on a loop
   that should be ~2-3 → it's **stalling**. Check: (1) `stalled-cycles-backend`
   high → a long dependency chain (latency-bound) or a memory stall (cache
   misses — `perf stat -e cache-misses,LLC-load-misses`). (2)
   `stalled-cycles-frontend` high + `branch-misses` high → branch mispredictions
   / I-cache misses / bad code layout. (3) `perf annotate` to find the exact
   instruction eating cycles — often a `div`, a missing load, or a mispredicted
   `jne`. Fix the specific stall, don't guess.
   </details>

4. "Dependency chain ko todo" — ek running CRC/checksum ke liye yeh kaise karte?

   <details><summary>Answer</summary>

   Compute N partial checksums over interleaved slices of the input
   (`crc0` over bytes 0,N,2N,...; `crc1` over 1,N+1,...), each an independent
   chain, then combine the N partials at the end with the checksum's
   "combine" operation (for CRC there's a known GF(2) polynomial-multiply
   combine; for a simple additive/xor hash it's trivial). The hardware `crc32`
   instruction has ~3-cycle latency, ~1/cycle throughput — exactly the `imul`
   situation, so 3-4 parallel streams ≈ 3-4× throughput. This is how fast CRC
   libraries (and `zlib`'s slice-by-8) work.
   </details>

5. Kyun ek deeper pipeline (zyada stages) ek trade-off hai, na ki pure win?

   <details><summary>Answer</summary>

   Deeper pipeline → each stage does less work → shorter critical path per stage
   → **higher achievable clock frequency** (more GHz). But: (a) a branch
   mispredict flushes and refills *all* stages → penalty grows with depth
   (~15-20+ cycles); (b) more stages = more latches, more power; (c) hazards
   create more/longer bubbles. Intel's Pentium 4 (NetBurst, ~31 stages) chased
   clock speed and lost on real workloads to shorter-pipeline designs.
   Modern CPUs sit around 14-20 stages as the sweet spot, and lean hard on good
   branch prediction (file `07`) to keep the deep pipeline fed.
   </details>

---

## Interview questions

1. Pipelining — what improves (throughput) and what doesn't (single-instruction latency).
2. Latency vs throughput of an instruction — define both, give `imul` numbers.
3. Latency-bound vs throughput-bound loop — how to tell, how to fix.
4. The four hazard types (data, control, structural, memory) with an example each.
5. Pipeline flush + refill — the mispredict penalty and why it scales with depth.
6. Why does the OoO engine not "just fix" a long serial dependency chain?
7. `perf stat` IPC well below 2 on a compute loop — your diagnosis steps.

---

## Next
→ [`05-superscalar.md`](05-superscalar.md)
