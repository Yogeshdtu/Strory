# 16 — Exercises: CPU architecture

## Prerequisites
- Poora folder 31 (`01`–`15`)

## Kaise use karein
- Part A: predict the behaviour / number, phir example chala ke verify.
- Part B: find-the-bug — har snippet mein ek CPU-architecture-level performance
  bug hai.
- Part C: reasoning — µarch se latency reason karo.
- Part D: challenge — build + measure. **Real numbers likho** (CLAUDE.md Rule 2),
  aur agar compiler ne benchmark defeat kiya (jaisa examples `01`/`03` mein hua)
  to woh bhi likho — woh khud ek finding hai.

> Yeh folder ke examples portable `.cpp` hain (Linux nahi chahiye). `-O2`
> mandatory; `-O0` pe har number bekaar. Is repo ka box AMD Zen 2 ~2 GHz
> (throttled) hai — **ratios port karo, absolutes nahi** (file `13`, `15`).

---

## Part A — Predict

### A1
```cpp
uint64_t a = 1;
for (uint64_t i = 0; i < N; ++i) a = a * C + i;      // C a constant, a carried
```
vs
```cpp
uint64_t a=1,b=2,c=3,d=4;
for (uint64_t i = 0; i < N/4; ++i) { a=a*C+i; b=b*C+i; c=c*C+i; d=d*C+i; }
```
Second version kitna tez, aur kyun? (Example `02` ka setup.)

<details><summary>Answer</summary>

~**4×** faster. The first loop is one serial `imul`-chain — each iteration's `a`
needs the previous `a`, so it runs at the `imul` latency (~3 cyc/iter), leaving
the multiplier port idle 2 of every 3 cycles. The second has **4 independent
chains**; the OoO engine interleaves them, feeding the (single) multiplier port
a new `imul` every cycle → ~1 `imul`/cycle → ~4× the throughput. Adding a 5th–8th
chain gives ~nothing more (port saturated). Measured on this box: serial
~1.2–1.7 ns/op → 4-parallel ~0.3–0.4 ns/op.
</details>

### A2
`if (v[i] >= 128) s += v[i];` over 32768 ints, 20000 reps. Array random vs
sorted. Default `-O2` pe kya farak dikhega? `#pragma GCC optimize("no-if-
conversion")` ke saath?

<details><summary>Answer</summary>

**Default `-O2`: no difference** — GCC if-converts the `if` to a branchless
`cmovge` (or masked add), so there's no branch to mispredict and data order stops
mattering. **With if-conversion disabled:** a real conditional jump is emitted;
random order → ~50% mispredict (~15–20 cyc each) → measured **~4–8 ns/element**;
sorted order → ~perfectly predicted → **~0.5–1.4 ns/element** → **~6–7×**
slowdown from the unpredictable branch. The default-`-O2` "no effect" is itself
the lesson: for a simple predicate the compiler already does the branchless
transform (example `04`'s job). (Example `03`.)
</details>

### A3
`double s = 0; for (i) s += a[i];` (1M floats, in L2). Scalar vs `_mm256_add_ps`
(8-wide) speedup — 8×? Zyada? Kam?

<details><summary>Answer</summary>

Measured on this box: SSE (4-wide) **~4×**, AVX2 (8-wide) **~9.5×** — slightly
*more* than the raw lane count. Two effects stack: (1) width — 8 elements per
instruction; (2) ILP — the scalar version is a single ~4-cycle-latency FP-add
dependency chain (~4 cyc/element), while the SIMD accumulator is a *vector* of 8
partial sums, so consecutive `vaddps`es are less latency-bound and saturate the
FP-add port (~2/cycle). If the array were bigger than L3 (memory-bandwidth-
bound), SIMD would give ~1× — the win requires compute-bound + in-cache.
(Example `05`.)
</details>

### A4
`x / d` jahan `d` ek runtime `int` variable hai, hot loop mein. `x / 7` (literal).
Dono ke asm mein kya, aur latency ka farak?

<details><summary>Answer</summary>

`x / d` (runtime divisor) → `idiv` — ~20–30 cycles, **not pipelined**, latency
data-dependent. `x / 7` (compile-time constant) → the compiler emits a
**reciprocal multiply** (`(x * magic) >> shift` + sign correction) — ~4–5 cheap,
pipelined instructions, **no `idiv`**. So a constant divide is ~5× cheaper and
predictable. If `d` is loop-invariant, hoist a `libdivide` divider (precomputes
the magic once, ~4 ops per divide inside) or, if FP is acceptable, `double inv =
1.0/d;` then multiply. (File `09`.)
</details>

### A5
`__rdtsc()` self-cost is measured as ~20 cycles, but `lfence;rdtsc;lfence` and
`rdtscp;lfence` as ~40–90 cycles (example `08`). Kaunsa use karoge HFT latency
timestamp ke liye, aur kyun?

<details><summary>Answer</summary>

For a **coarse** measurement (a loop of thousands, divide by count) — plain
`__rdtsc()` (~20 cyc) is fine; its slight out-of-order slip averages out. For a
**precise, single-region** measurement (~100-cycle region, e.g. one order
decision) — you need `rdtscp;lfence` (or `lfence;rdtsc;lfence`) to pin the read
to a fixed point, accepting the ~40–90 cyc self-cost. For an HFT **event
timestamp** on the hot path (recording when a decision happened), `rdtscp`
(~30–45 cyc) is the usual choice, or `clock_gettime(CLOCK_MONOTONIC)` via vDSO
(~20 ns, folder 29 file 16). Pin the thread (file `12`, folder 29 file 11) so
cross-core TSC skew is a non-issue.
</details>

### A6
Ek event-driven strategy thread quiet period ke baad pehla event handle karne
mein extra ~80 µs leta hai. `governor=performance` set hai. Kya ho raha?

<details><summary>Answer</summary>

Deep **C-state** wake + frequency ramp. During the quiet period the core had no
work → dropped to C6 (powered off, L1/L2 flushed), and even with `performance`
governor, if turbo is on the core must ramp from base to turbo when work
resumes. The first event pays: C6 exit (~tens of µs), frequency ramp (~tens of
µs to ms), and cold-cache misses re-loading the working set. Fixes: busy-poll so
the core stays in C0 at full frequency; and/or `processor.max_cstate=1` +
`no_turbo` (fixed base frequency, no ramp) on the latency core. (File `13`.)
</details>

---

## Part B — Find the bug

### B1
```cpp
// "fast" running hash over a large buffer
uint64_t h = 1469598103934665603ull;
for (size_t i = 0; i < n; ++i) { h ^= buf[i]; h *= 1099511628211ull; }
return h;
```

<details><summary>Answer</summary>

Not a correctness bug — a **throughput** bug. It's a single serial dependency
chain: `h` each iteration needs the previous `h` (xor then multiply, ~3-cyc
`imul` latency) → ~3–4 cycles/byte, latency-bound, multiplier port idle most
cycles. Fix: **N independent hash streams** over interleaved slices (`h0` over
bytes 0,N,2N,...; `h1` over 1,N+1,...), each an independent FNV chain, then
combine the N partials at the end (mix them together). ~3–4× throughput. Or use
a hash designed for ILP/SIMD (xxHash, which uses 4 lanes internally). (File `04`
ex 4, file `09`.)
</details>

### B2
```cpp
struct Tick { double px; double qty; int64_t ts; char sym[8]; };
std::vector<Tick> ticks;
// hot loop: sum of px*qty over all ticks
double notional = 0;
for (const auto& t : ticks) notional += t.px * t.qty;
```

<details><summary>Answer</summary>

**AoS layout** defeats vectorization and wastes cache. Each `Tick` is 32 bytes;
the loop only needs `px` and `qty` (16 bytes) but every cache line pulls in `ts`
and `sym` too → ~half the L1/L2 bandwidth wasted, and `px`/`qty` are strided
(stride 32) so the vectorizer must gather/shuffle. Fix: **SoA** — `std::vector<
double> px, qty;` (parallel arrays). Now `px` is contiguous: one
`_mm256_loadu_pd(&px[i])` fills 4 lanes, same for `qty`, one `vmulpd`, accumulate
→ clean AVX2, and only the bytes you need are fetched. (File `10`, folder 32.)
Also: single `notional` accumulator → add partial accumulators / SIMD lanes.
</details>

### B3
```cpp
// dispatch on message type in the hot feed loop
for (auto& msg : batch) {
    handler_for(msg.type)->process(msg);   // returns a base* ; virtual process()
}
```

<details><summary>Answer</summary>

**Polymorphic indirect call in the hottest loop.** `handler_for(...)->process(...)`
is a `call [vtable+off]` — the target varies with `msg.type`, so if the type mix
is unpredictable the **indirect branch predictor mispredicts** per message
(~15–20 cyc, file `07`), *and* the call boundary blocks inlining of `process`.
Fix: devirtualize. If the set of message types is small and known (it always is
for an exchange protocol): a `switch (msg.type)` over concrete handlers (jump
table, and `msg.type` is often the same for runs of messages → well predicted),
or `std::variant<Add, Cancel, Trade, ...>` + `std::visit`, or templated
dispatch. Measured elsewhere in this repo: virtual ~2.4 ns vs CRTP ~0.6 ns at a
real call boundary. (Folder 16/21/36, file `07`.)
</details>

### B4
```cpp
// pre-fault + measure a scale kernel
for (size_t i = 0; i < n; ++i) out[i] = in[i] * 3 + 1;     // out, in are int*
// benchmark shows this is ~same speed as a version with a data dependency
```

<details><summary>Answer</summary>

`out` and `in` are plain `int*` — the compiler **cannot prove they don't
alias** (a caller could pass `scale(x, x+1, n)`), so it can't safely vectorize
(a store to `out[i]` might change `in[i+1]`) — it emits a scalar loop, or a
runtime alias-check + two versions. Fix: `int* __restrict out, const int*
__restrict in` (or use separate/SoA buffers so aliasing is impossible). Measured
(example `06`): the `__restrict` version auto-vectorizes to ~2.5–3× the
no-`__restrict` version on this box. (File `06`, `10`.)
</details>

### B5
```cpp
// "constant time" price comparison for deterministic latency
double ratio_a = bid_a / ask_a;
double ratio_b = bid_b / ask_b;
return ratio_a > ratio_b;
```

<details><summary>Answer</summary>

Two `divsd` on the critical path (~10–20 cyc each, and FP-divide latency is
**data-dependent** — it varies with the operand values), plus the risk of a
**denormal** input triggering a ~100+ cycle microcode assist (file `02`, `13`).
So it's neither fast nor constant-time. Fix: **cross-multiply** — `bid_a / ask_a
> bid_b / ask_b` ⇔ `bid_a * ask_b > bid_b * ask_a` (valid when `ask_a, ask_b >
0`, which they are for real quotes). Two `mulsd` (~4 cyc, fixed latency) + one
compare, no divide, no denormal path. Also set flush-to-zero / denormals-are-
zero mode at thread startup (file `02`) as a general guard.
</details>

### B6
```cpp
// SIMD sum with AVX2, called from a large scalar function
float sum8(const float* p, size_t n) {
    __m256 acc = _mm256_setzero_ps();
    for (size_t i = 0; i + 8 <= n; i += 8) acc = _mm256_add_ps(acc, _mm256_loadu_ps(p+i));
    float t[8]; _mm256_storeu_ps(t, acc);
    return t[0]+t[1]+t[2]+t[3]+t[4]+t[5]+t[6]+t[7];
}
```

<details><summary>Answer</summary>

Two issues. (1) **Tail not handled** — if `n` isn't a multiple of 8, the last
`n % 8` elements are silently dropped (correctness bug). Add a scalar epilogue:
`for (; i < n; ++i) scalar_sum += p[i];`. (2) **No `vzeroupper` / potential
SSE↔AVX transition penalty** — this AVX (256-bit) function, called from and
returning to a large scalar function that (or whose other callees) may use
legacy 128-bit SSE, leaves "dirty upper" state → ~tens-of-cycles penalty on
subsequent SSE instructions on some µarchs. With `-mavx2` the compiler inserts
`vzeroupper` at the function boundary; if this were hand-asm or `-mno-vzeroupper`
you'd need `_mm256_zeroupper()` before return. (Also: a single `acc` vector is
fine here since it's already 8 partial sums, but for very long `n` two `acc`
vectors would hide the `vaddps` latency better.) (File `10`, `11`.)
</details>

---

## Part C — Reasoning

### C1
Tumhare hot path ka critical path (dependent-latency chain) ~40 cycles hai, aur
tumhare paas plenty ILP hai (IPC 3.2). Box 4 GHz locked. Kya tick-compute ka
floor hai, aur usse neeche jaane ke liye kya karoge?

<details><summary>Answer</summary>

Floor ≈ **40 cycles / 4 GHz = 10 ns** — no amount of ILP or wider issue beats
the sum of dependent latencies on the critical path. IPC 3.2 means you're
*already* overlapping all the independent work; the 40-cycle chain is the
limiter. To go below 10 ns you must **shorten the chain**: replace long-latency
ops on it (`div`/`sqrt`/dependent load → reciprocal-multiply / `rsqrt`+Newton /
prefetch so the load isn't dependent), hoist loop-invariant sub-computations off
the chain, split a serial reduction into parallel partials, or restructure the
math (cross-multiply instead of divide, compare in a cheaper space). `llvm-mca
-mcpu=<target>` prints the critical sequence — attack exactly those instructions.
(File `04`, `09`.)
</details>

### C2
`perf stat` on hot loop: IPC 0.6, `branch-misses` low, `LLC-load-misses` high,
`cycle_activity.stalls_l3_miss` high. Diagnosis + two fixes.

<details><summary>Answer</summary>

**Memory-bound** — the loop is stalling on last-level-cache misses to DRAM
(~200–400 cycles each), not on branches or compute (branch-misses low, IPC
crushed to 0.6). The working set doesn't fit cache, or the access pattern is
random/pointer-chasing (file `06`, folder 32). Fixes: (1) **Shrink / restructure
the data** so the hot working set fits L2/L3 — smaller records (drop unused
fields), SoA so only needed bytes are fetched, quantize/compress. (2) **Fix the
access pattern** — flat arrays + sequential/strided access so the hardware
prefetcher hides the latency; replace node-based containers (`std::map`,
`std::list`, chained hash) with contiguous ones (sorted vector + binary search,
open-addressing hash, implicit tree); `__builtin_prefetch` the next record while
processing the current. SIMD won't help here — it's not compute-bound. (Folder
32 covers this fully.)
</details>

### C3
Ek AVX2 loop isolated benchmark mein 4× tez hai. Do alag production boxes:
box A (Sapphire Rapids) pe end-to-end 3.8× improve, box B (Cascade Lake) pe sirf
1.2×. Explain.

<details><summary>Answer</summary>

**AVX frequency offset.** Cascade Lake (Skylake-SP family) drops the core to a
lower "licence" frequency for heavy 256-bit AVX2/FMA, and stays low for
~hundreds of µs *after* the SIMD burst — so the surrounding scalar code also
slows. A 4× loop run at ~0.7× frequency, dragging its neighbours down, nets
~1.2× end-to-end on box B. Sapphire Rapids largely removed the offset, so the
4× carries through to ~3.8× on box A. Action: on box B, either use **128-bit
AVX** (little/no offset), keep the burst very short and isolated, or skip
vectorization for that loop and spend the budget elsewhere; on box A, use the
full AVX2 (or AVX-512) path via runtime dispatch. This is why you benchmark
end-to-end on each box class and keep a per-machine config (files `10`, `13`,
`15`).
</details>

### C4
Do hot threads: feed decoder aur book builder. Tumhe unhe pin karna hai. Box:
8 physical cores, SMT on (16 logical), 1 socket, 2 CCX (nodes 0/1), NIC on
node 0. Layout?

<details><summary>Answer</summary>

Both hot threads on **node 0** (the CCX with the NIC), one per **physical**
core, with their **SMT siblings isolated and idle** (files `12`, `14`; folder 29
file 11). E.g. feed decoder → physical core 2 (logical CPUs 2 & 10 — pin to 2,
leave 10 unused); book builder → physical core 3 (CPUs 3 & 11 — pin to 3, 11
unused). Their memory (`mbind` to node 0), the NIC RX rings, and the SPSC ring
between them all on node 0 — every hand-off stays within one CCX's L3 and one
memory domain, no Infinity Fabric hop. Boot: `isolcpus=2,3,10,11 nohz_full=2,3
rcu_nocbs=2,3 irqaffinity=0,1`, or disable SMT entirely and use cores 2–3.
Housekeeping (OS, NIC IRQ softirq, logging) on cores 0–1; non-latency work on
node 1. Startup self-check asserts core is on node 0, sibling is idle, memory is
node-0-local (folder 29 file 11/14).
</details>

### C5
`__rdtsc()` ka use karke ek thread pe latency timestamps le rahe ho. Ek din
kuch measurements *negative* aate hain (end < start). Do possible wajah, fix.

<details><summary>Answer</summary>

(1) **Thread migrated across cores** and the two cores' TSCs aren't perfectly
synchronized (older hardware, or the BIOS didn't sync them, or you're across
sockets — file `14`). The `end` `rdtsc` ran on a core whose TSC is slightly
behind the `start` core's. Fix: **pin the thread** (files `12`, folder 29 file
11) so both reads are on the same core; use `rdtscp` (returns the core id in
`aux` — you can detect a migration and discard/retag that sample); verify
`constant_tsc` + `nonstop_tsc` + `tsc_reliable` in `/proc/cpuinfo` (example
`07`). (2) **Out-of-order slip** — plain `__rdtsc()` can execute earlier/later
than its position in the code, so for a very short region the `end` read can
retire "before" the `start` read's effect. Fix: `lfence;rdtsc;lfence` or
`rdtscp;lfence` to serialize (example `08`), accepting the higher self-cost.
</details>

---

## Part D — Challenge (build + measure)

> `-O2` mandatory. Real numbers. If the compiler defeats a benchmark, report
> that (it's a finding — examples `01`/`03` had exactly this).

### D1 — latency vs throughput on your box
`examples/01` + `02` chalao (kai baar — is box pe frequency scaling se numbers
±30% swing karte). Likho: (a) ADD/OR/MUL/DIV latency vs throughput, (b) serial
chain vs 4 vs 8 parallel chains ka ratio. Kaunsi op ka lat/thr ratio sabse bada
(→ least pipelined)?

<details><summary>Expected shape</summary>

`01`: ADD lat≈thr (both tiny, ~0.15–0.4 ns), MUL lat ~2–3× its thr, **DIV
lat≈thr and both large (~4–6 ns)** — DIV is the least pipelined. `02`: serial
~1.2–1.7 ns/op → 4 parallel ~4× → 8 parallel ~same as 4 (multiplier port
saturated). If ADD shows lat 0.000: the barrier didn't hold / it got DCE'd —
report it and check the `keep()` asm barrier is present.
</details>

### D2 — branch misprediction cost
`examples/03` chalao. RANDOM vs SORTED ka ratio likho. Phir `./build.ps1 asm
31-CPU-ARCHITECTURE/examples/03_branch_prediction.cpp | grep -E 'cmov|jl|jge'` —
pragma ke saath kya dikhta? Phir pragma line comment out karke rebuild + rerun —
ab ratio kya?

<details><summary>Expected shape</summary>

With the pragma: `jl`/`jge` in the asm, RANDOM/SORTED ratio **~6–7×** (RANDOM
~4–8 ns/elem, SORTED ~0.5–1.4 ns/elem). Without the pragma (default `-O2`):
`cmovge` in the asm, ratio **~1.0×** (no branch → no misprediction → data order
irrelevant). The "no effect" case is the lesson — the compiler already did the
branchless transform.
</details>

### D3 — SIMD speedup, and where it stops
`examples/05` chalao. scalar / SSE / AVX2 ka ns/elem aur speedup likho. Phir
`N` ko `1u << 22` (16 MB, > L3) kar do, rebuild, rerun — ab SIMD speedup kya?
Kyun badla?

<details><summary>Expected shape</summary>

L2-resident (default `1u<<16` = 256 KB): SSE ~4×, AVX2 ~9.5× (compute-bound).
`1u<<22` = 16 MB, exceeds L3 → the loop becomes **memory-bandwidth-bound** →
scalar, SSE, and AVX2 all converge to ~the same ns/elem (SIMD speedup → ~1–1.5×).
The lane parallelism is useless when you're waiting on DRAM. Lesson: SIMD helps
compute-bound + in-cache; fix data movement first (folder 32).
</details>

### D4 — auto-vectorization: what the compiler will and won't do
`examples/06` chalao. Likho: auto-vec vs `no-tree-vectorize` ka ratio; prefix-sum
aur no-`__restrict` versions kitne slow. Phir `-march=x86-64-v3` add karke
(`STD` / a manual `g++` line) rebuild — auto-vec version aur tez hua? (`-fopt-
info-vec-optimized` se confirm AVX2 use hua.)

<details><summary>Expected shape</summary>

Default (SSE2 baseline): auto-vec ~2.5–3× the scalar version. prefix-sum stays
scalar (loop-carried dependency); no-`__restrict` stays scalar (possible
aliasing). With `-march=x86-64-v3`: the vectorizer uses 256-bit AVX2 (8 ints/
instr vs 4) → auto-vec version faster still, and `-fopt-info-vec-optimized`
reports the AVX2 vectorization. Lesson: `__restrict` + countable loop + no
dependency + a good `-march` = the compiler does it for you.
</details>

### D5 — RDTSC self-cost and calibration
`examples/08` chalao. Likho: calibrated cycles/ns (→ effective GHz), aur
`__rdtsc` vs `lfence;rdtsc;lfence` vs `rdtscp;lfence` ki self-cost. Phir ek chhoti
known region (e.g. 100 dependent `add`s) ko in teenon se time karo — kaunsa
sabse consistent number deta, aur kyun?

<details><summary>Expected shape</summary>

Calibration: ~2.0 cycles/ns on this throttled laptop (~2 GHz). Self-cost:
`__rdtsc` ~20–25 cyc, serialized variants ~40–90 cyc. Timing a ~100-cycle region:
plain `__rdtsc` gives a noisy number (its own OoO slip is a large fraction of
100); `rdtscp;lfence` gives a consistent number (the region is pinned) at the
cost of the ~45-cyc overhead you subtract. For sub-1000-cycle regions,
serialize; for loops of millions, plain `__rdtsc` and divide.
</details>

---

## Next
→ [`../32-CACHE-MEMORY-PERFORMANCE/00-README.md`](../32-CACHE-MEMORY-PERFORMANCE/00-README.md)
