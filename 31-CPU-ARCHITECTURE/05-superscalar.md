# 05 — Superscalar: multiple execution units, ILP, ports

## Prerequisites
- `04-pipelining.md` (latency vs throughput, hazards)
- `02-registers.md` (renaming preview)

## Yeh topic abhi kyun
Pipelining ne throughput ko 1 IPC tak pahunchaya. **Superscalar** usse aage le
jaata: har cycle **kai instructions** issue + execute. Modern x86 core ~4–6
instructions/cycle decode/retire kar sakta, aur execution "**ports**" pe parallel
kaam hota. Yeh samajhna ki "kitna parallel kaam ek core kar sakta" — aur tumhara
code us capacity ko kitna use kar raha (IPC) — HFT tuning ka core hai.

---

## Superscalar = wide pipeline

Scalar pipeline: 1 instruction per stage per cycle.
Superscalar: **N-wide** — N instructions move through each stage per cycle.

```
                    N-wide front end (fetch/decode ~4-6 µops/cycle)
                                    |
                        +-----------+-----------+
                        |     Reorder Buffer    |   (in-flight window, ~200-500 µops)
                        |   + Scheduler / RS    |   (out-of-order — file 06)
                        +-----------+-----------+
                                    |
      dispatch up to ~6-10 µops/cycle to the EXECUTION PORTS:
      +------+------+------+------+------+------+------+------+
      | P0   | P1   | P2   | P3   | P4   | P5   | P6   | P7   |  <- example port map
      | ALU  | ALU  | LOAD | LOAD | STORE| ALU  | ALU  | STORE|
      | mul  | LEA  |      |      |      | vec  | branch| AGU |
      | vec  | vec  |      |      |      | shuf |      |      |
      +------+------+------+------+------+------+------+------+
```

- **Ports** = execution pipelines. Each cycle, the scheduler picks ready µops
  and sends at most one to each port.
- Different µops need different ports. `add` → any ALU port (P0/P1/P5/P6 on a
  typical Intel). `imul` → only P1. `load` → P2/P3. `store-data` → P4. `branch`
  → P6. Vector shuffle → P5. So **the port distribution of your instruction mix
  is a throughput limit**.
- **This box** (AMD Zen 2, Ryzen 7 4700U — example `07`): 4 ALU pipes, 3
  AGU/load-store pipes, 4× 128-bit FP pipes (2 FMA), ~180-entry ROB. Numbers
  differ per µarch (file `15`) but the model is the same.

---

## ILP — Instruction-Level Parallelism

ILP = how many of your instructions *can* execute simultaneously (no data
dependency between them). The CPU's job is to *find* ILP in your instruction
stream (via the reorder buffer + scheduler, file `06`); your job is to *have*
ILP to find.

- **Low ILP** (one long dependency chain): IPC ≈ 1 / (avg latency). A serial
  `imul` chain → IPC ~0.33 (3-cyc latency, one result every 3 cycles).
- **High ILP** (many independent chains): IPC → limited by decode width,
  retire width, or a hot port. Example `02`: 4 independent `imul` chains →
  ~4× the serial rate (the multiplier port at 1 `imul`/cycle becomes the limit).
- **The ceiling:** even with infinite ILP, you can't retire faster than the
  narrowest of {fetch width, decode width, rename width, retire width, the
  busiest port}.

**Example `02` measured (this box):** serial ~1.2–1.7 ns/op → 4 parallel ~0.3–0.4
ns/op (**~4×**) → 8 parallel **no further gain** (port-limited, not ILP-limited).

---

## Where ILP comes from — your side

| Technique | How it exposes ILP |
|---|---|
| **Multiple accumulators** | `s0 += a[i]; s1 += a[i+1]; ...` — N independent reduction chains (example `02`) |
| **Loop unrolling** | more independent iterations visible in the window at once (compiler does it at `-O2`/`-O3`) |
| **SIMD** | one instruction, N lanes = N independent operations (file `10`, example `05`) |
| **Software pipelining** | start iteration `i+1`'s loads while computing iteration `i` |
| **Reassociation** | `((a+b)+c)+d` (serial) → `(a+b)+(c+d)` (two independent adds) — compiler won't do this for FP without `-ffast-math` |
| **Independent data structures** | processing two order books / two symbols interleaved |

The CPU already does register renaming (removes false WAW/WAR deps) and OoO
scheduling (file `06`). It can't invent parallelism that isn't there — a serial
recurrence stays serial.

---

## Reading it: IPC and port pressure

```bash
perf stat -e cycles,instructions ./prog          # IPC = instructions / cycles
perf stat -e cycles,uops_retired.all ./prog      # µops/cycle
```
- **IPC ~3–4** on a compute loop → you're well-utilised.
- **IPC ~1** → latency-bound (dependency chain) or a serialising op.
- **IPC < 1** → stalling on memory (folder 32) or mispredicts (file `07`).

`llvm-mca` and `uica.uops.info` give a static per-port breakdown of a code
snippet — "this loop is bottlenecked on P1 (the multiplier)" or "on the load
ports" — which tells you exactly what to change.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — one accumulator in a reduction
`for (x : v) sum += x;` — a single serial FP-add chain, ~3–4 cyc/element,
IPC-poor. Multiple partial sums (or SIMD, or `-ffast-math` letting the compiler
do it) → several× faster (examples `02`, `05`).

### Trap 2 — "more instructions = slower" (always)
Not if they're independent. A branchless version with *more* instructions but
*no* mispredicts can be much faster (example `04`). Count the critical-path
latency and port pressure, not the instruction count.

### Trap 3 — over-unrolling
Past a point, unrolling bloats I-cache, increases register pressure (spills,
file `02`), and doesn't help (you've saturated the limiting port). Let `-O2`/
`-O3` choose; measure if you override with `#pragma GCC unroll N`.

### Trap 4 — ignoring the limiting port
You added 4 accumulators but the loop is now bottlenecked on the *load* port
(2 loads/cycle) because you also load 4 values per iteration. Adding more
accumulators won't help; you need fewer loads (wider loads / SIMD) or a different
data layout.

### Trap 5 — assuming SMT gives you 2× the ports (file `12`)
Hyperthreading shares the *same* execution ports between two threads. Two threads
on one core don't get 2× throughput — they *share* ~1×, with better latency
hiding. HFT usually disables SMT (file `12`).

### Trap 6 — mixing very-different-latency ops in a tight chain
A chain of `add; div; add; div` runs at the `div` rate. Isolate the `div` (or
kill it) so the `add`s can flow.

---

## > **HFT relevance**

> - **Target IPC ~3+ on compute-bound hot loops.** Measure it (`perf stat`).
>   Below ~1.5 → find the stall (dependency chain, hot port, memory, mispredict).
> - **Expose ILP deliberately:** partial accumulators for reductions, SIMD for
>   maps, unrolling for independent iterations. Don't rely on the compiler for
>   FP reassociation — either `-ffast-math` (measure the accuracy hit) or do it
>   by hand.
> - **Know your limiting port.** `llvm-mca` / `uica` on the hot loop. If it's
>   the multiplier or the divider, restructure the math. If it's the load ports,
>   fix the data layout (folder 32) or widen the loads (SIMD).
> - **SMT off** (file `12`) so your hot thread owns the full port set of its
>   core.
> - **Don't over-optimise instruction count** — a longer branchless sequence
>   with no mispredicts and good port spread beats a short branchy one on
>   unpredictable data.

---

## Hands-on

```bash
# example 02: serial vs 4 vs 8 parallel chains -> where ILP stops helping
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/02_pipeline_stall.cpp

# example 05: SIMD as hardware ILP (lanes = independent ops)
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/05_simd_basics.cpp

# static port analysis of a loop
g++ -std=c++20 -O2 -S your.cpp -o - | llvm-mca -mcpu=native -bottleneck-analysis

# IPC of a real program (Linux)
perf stat -e cycles,instructions,uops_retired.all ./trader
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "superscalar = higher clock" | wider issue/execute per cycle, same clock |
| "more instructions = slower" | independent ones overlap; count critical-path latency + ports |
| "one accumulator is fine" | serial chain; N partials → ~N× |
| "unroll more = faster" | past the limiting port it bloats I-cache + spills |
| "SMT doubles throughput" | shares the ports; ~1× split two ways |
| "compiler reassociates FP for me" | not without `-ffast-math`; do it by hand or opt in |

---

## Exercises

1. Ek loop `for i: sum += a[i] * b[i]` (dot product), `sum` carried, FP.
   `addss` latency 4, `mulss` latency 4, throughput 0.5 each. IPC/rate estimate
   with 1 accumulator vs 4?

   <details><summary>Answer</summary>

   1 accumulator: the `sum += ...` add is a serial chain — one 4-cycle-latency
   FP add per element → **~4 cycles/element** (the mul overlaps, it's not on the
   carried chain). 4 accumulators (`s0..s3`, each summing every 4th element):
   4 independent add chains → the FP-add *port* (throughput 0.5, i.e. 2 adds/
   cycle) becomes the limit → **~0.5 cycle/element** for the adds, plus the muls
   (also 2/cycle) → ~1 cycle/element region. ~4-8× faster. Then combine `s0+s1+
   s2+s3` once at the end. SIMD (example `05`) does this with lanes: one `vfmadd`
   does 8 mul-adds.
   </details>

2. Tumne 8 partial accumulators daale par speedup 4→8 accumulators pe 0 hai
   (example `02` jaisa). Kyun?

   <details><summary>Answer</summary>

   You've hit a **structural limit**, not an ILP limit. The `imul` (or FP-mul)
   port issues at most 1 per cycle. With 4 independent chains you already feed it
   one `imul` every cycle — it's 100% busy. Adding 4 more chains gives the
   scheduler more choices but no more port throughput, so the rate is unchanged
   (and register pressure rises, risking spills). The lesson: expose *enough*
   ILP to saturate the limiting resource, not more.
   </details>

3. `perf stat` shows IPC 3.4 on your hot loop but the loop is still "too slow"
   for the latency budget. Ab kya?

   <details><summary>Answer</summary>

   IPC 3.4 means the CPU is well-utilised — it's *not* stalling. The loop is slow
   because it's simply doing **too much work** per element (too many
   instructions), or it's called too many times. Now you optimise algorithmically:
   fewer operations per element (better math, precomputation, SoA to load less),
   process fewer elements (early-out, coarser granularity), SIMD to do 8×/
   instruction (file `10`), or move work off the hot path entirely. IPC tuning
   is done; instruction-count / algorithm tuning begins.
   </details>

4. "Execution port" ka concept ek RISC (ARM64) core pe bhi apply hota hai?

   <details><summary>Answer</summary>

   Yes. Ports (a.k.a. issue/execution pipes) are a microarchitecture concept, not
   an ISA one. A modern ARM64 core (Apple Firestorm, ARM Neoverse V2, AWS
   Graviton3) has a wide decode (6-8), a large reorder window, and a set of
   execution pipes — several integer ALUs, 2-4 load/store, 2-4 SIMD/FP — just
   like x86. The port *layout* and counts differ, so `llvm-mca -mcpu=` and the
   optimal instruction mix differ, but "which pipe is the bottleneck" is the
   same question. HFT firms experimenting with Graviton do this analysis per
   core.
   </details>

5. `-ffast-math` FP reduction ko reassociate karne deta (`(a+b)+(c+d)` style).
   HFT context mein kab OK, kab nahi?

   <details><summary>Answer</summary>

   `-ffast-math` (or the narrower `-fassociative-math -fno-signed-zeros
   -fno-trapping-math`) lets the compiler reassociate/vectorize FP reductions —
   often a 2-8× speedup on sums/dot-products. The cost: results are **not
   bit-identical** to the strict left-to-right order, and it also assumes no
   NaN/Inf and flushes signed zeros. OK when: the computation is a statistic /
   signal where a few ULPs don't matter, and you've tested that NaN/Inf can't
   occur (or you guard inputs). **Not OK** when: you need reproducible results
   across builds/machines (regulatory, backtest-vs-live parity), or the code
   relies on NaN propagation / exact FP comparison. Safer: apply it per-function
   (`__attribute__((optimize("fast-math")))` on the specific hot reduction) or do
   the reassociation by hand with explicit partial sums so you control the order.
   </details>

---

## Interview questions

1. Superscalar vs pipelined — what "wide" adds.
2. Execution ports — what they are, why the port mix of your code is a limit.
3. ILP — where the CPU finds it, where *you* have to provide it.
4. IPC — what values mean latency-bound vs throughput-bound vs memory-bound.
5. Multiple accumulators for a reduction — why, and why 8 isn't 2× better than 4.
6. Does SMT (hyperthreading) give a single-threaded loop more port throughput?
7. `-ffast-math` FP reassociation — the speedup and the two big risks.

---

## Next
→ [`06-out-of-order-execution.md`](06-out-of-order-execution.md)
