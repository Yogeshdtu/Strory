# 17 — Exercises: profiling & benchmarking

## Prerequisites
- Poora folder 35 (`01`–`16`) + `examples/`

## Kaise use karein
- **Part A** — "yeh benchmark jhooth kyun bol raha" — find the bug, predict
  the number.
- **Part B** — statistics / percentiles reasoning.
- **Part C** — profiler output diagnosis (`perf stat` / `annotate` / flame /
  top-down).
- **Part D** — hands-on: chalao, measure, compare. **Real observations**.

> `-O2` unless noted. Numbers = is box (AMD Zen 2, ~2 GHz, MinGW/Windows;
> plain `-O2` = SSE2). **Ratios / shapes port; absolutes nahi.**

---

## Part A — Benchmark jhooth pakdo

### A1
```cpp
static void BM_hash(benchmark::State& state) {
    std::string key = "user:12345";
    for (auto _ : state) {
        size_t h = std::hash<std::string>{}(key);
        benchmark::DoNotOptimize(h);
    }
}
BENCHMARK(BM_hash);
```
Yeh ~1 ns/iter dikhata. Bug?

<details><summary>Answer</summary>

**Loop-invariant hoisting.** `key` never changes → `std::hash{}(key)` is
loop-invariant → the compiler computes it **once** outside the
`for (auto _ : state)` loop, and the body just re-sinks the same `h` every
iteration. You're measuring `DoNotOptimize` (0 instr) + loop overhead, not
the hash. `DoNotOptimize(h)` on the *output* doesn't help — the input is
still constant.
**Fix:** make the input vary per iteration — `key.back() = char('0' + (state.
iterations() & 7));` before hashing, or hash `keys[i % N]` from a vector of
distinct strings, or feed a carried value. Then you measure a real hash
(~tens of ns for a short string). Also consider: `std::hash<std::string>`
for a 10-char string is dominated by reading 10 bytes + the mix — realistic
only if the string length matches your real keys.
</details>

### A2
```cpp
double sum = 0;
auto t0 = steady_clock::now();
for (int i = 0; i < 100'000'000; ++i) sum += std::sin(i * 0.001);
auto t1 = steady_clock::now();
std::printf("%.2f ns/op\n", ns(t1 - t0) / 1e8);
// prints: 0.00 ns/op
```
`std::sin` is ~20+ ns. Kaise 0.00?

<details><summary>Answer</summary>

**Dead code elimination.** `sum` is computed but **never used** after the
loop (no print of `sum`, no sink). `-O2` sees `sum` is dead → deletes the
accumulation → deletes the `sin` calls → empty loop → possibly deletes the
loop entirely. 0.00 ns/op.
**Fix:** `benchmark::DoNotOptimize(sum);` (or `std::printf` the `sum`, or
`asm volatile("" : : "x"(sum))`) after `t1`. Then you'll measure real
`sin` — but note (a) `i * 0.001` is a predictable sequence the compiler
*might* still vectorize/approximate; use an opaque input array. (b) it's a
**latency** chain (`sum +=` is serial) — independent `sin` calls would
pipeline ~4× better. Decide whether you want latency or throughput of `sin`.
</details>

### A3
```cpp
// "reserve() se kitna farak?" — do benchmarks
static void BM_no_reserve(benchmark::State& s) {
    for (auto _ : s) {
        std::vector<int> v;
        for (int i = 0; i < 1000; ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
static void BM_reserve(benchmark::State& s) {
    for (auto _ : s) {
        std::vector<int> v;
        v.reserve(1000);
        for (int i = 0; i < 1000; ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
```
Dono ~same time dikhate. Expected `reserve` faster tha. Do possible reasons.

<details><summary>Answer</summary>

(1) **Allocator caching** — after the first few iterations, the same-sized
`malloc`/`free` pair is served from the allocator's thread cache (tcmalloc/
jemalloc/glibc fastbins) → the "extra" reallocs in `BM_no_reserve` are
cheap hits on a hot free-list, and the memory is cache-warm. The
theoretical `O(log n)` reallocs each copy `~n` ints, but for `n=1000`
that's ~10 KB of `memcpy` total — tens of ns, lost in the noise of the 1000
`push_back`s. (2) **The `push_back`s dominate** — 1000 branch-predicted
`size < capacity` checks + stores is the bulk of the work; the growth
reallocs are a small fraction. `reserve` removes the capacity checks'
*branch* only partially (still bounds logic). To *see* `reserve`'s benefit:
larger `n` (100k+ → the `memcpy` on each doubling becomes real), or measure
**allocations** (`operator new` count via a counting allocator / DHAT) not
time, or a type with an expensive move/copy ctor (then each realloc's
element moves cost real time). For trivially-copyable `int` at `n=1000`,
"no measurable difference" is the honest result (Rule 2).
</details>

### A4
```cpp
uint64_t x = 1;
for (auto _ : state) {
    x = x * 6364136223846793005ULL + 1442695040888963407ULL;   // LCG
    benchmark::DoNotOptimize(x);
}
// reports 1.0 ns/iter
```
Ek doosre machine pe same code 0.35 ns/iter. Same clock speed. Kya farak?

<details><summary>Answer</summary>

This is a **latency-bound** loop — each `x` depends on the previous (carried
dependency through the multiply). One 64-bit `imul` is ~3 cycles latency.
At ~2 GHz, 3 cyc ≈ 1.5 ns... the ~1.0 ns machine is retiring one iteration
per ~2 cycles (imul latency ~3 but the machine may have a faster multiplier,
or the +1 folds). The **0.35 ns machine** is doing something different:
either (a) the compiler **unrolled and the `DoNotOptimize` per-iteration was
weak enough** that it broke the chain / computed multiple LCG steps as a
combined multiply (LCG steps compose: `x' = a·x + c`, `x'' = a²·x + (ac+c)`
— a compiler *can* fuse these), turning a latency chain into throughput; or
(b) different `-march` giving a lower-latency multiply; or (c) the other run
wasn't actually dependent (a bug in how `x` was carried). Check the asm on
both (`build.ps1 asm`): count `imul`s per unrolled iteration and whether
the chain is intact. A true dependent LCG can't go below the multiply
latency — 0.35 ns at 2 GHz (<1 cycle) means the chain was broken.
</details>

### A5
```cpp
// benchmark of a branch: predict vs mispredict
std::vector<int> data(1<<20);
for (auto& v : data) v = rng() % 256;            // random 0..255
// std::sort(data.begin(), data.end());          // <-- commented out
uint64_t count = 0;
for (auto _ : state)
    for (int v : data) if (v >= 128) count += v;
benchmark::DoNotOptimize(count);
```
Sorted vs unsorted (uncomment the sort) — jo classic 3–6× dikhata hai —
yahan sirf **1.1×** farak aaya. Kyun ho sakta?

<details><summary>Answer</summary>

The classic sorted/unsorted branch-prediction demo relies on the compiler
emitting an **actual conditional branch** for `if (v >= 128) count += v;`.
At `-O2`, GCC/Clang often **if-convert** this to branchless code:
`count += (v >= 128) ? v : 0` becomes a `cmov` or a mask (`(-(v>=128)) & v`)
— **no branch, so no misprediction penalty**, and sorted vs unsorted is
nearly identical (~1.1×, just cache/prefetch differences). To see the 3–6×:
(a) make the body too complex to if-convert (a function call, a side effect
in the branch), or (b) compile with `-fno-if-conversion` / check the asm
(`build.ps1 asm` — look for `jge`/`jl` vs `cmovge`), or (c) use
`__builtin_expect` the wrong way, or a branch the compiler can't
speculate. This *is* the lesson (folder 33/08, 34/08): modern compilers
kill many "obvious" branch-prediction demos by if-converting them — always
check whether a real branch survived before attributing a result to
prediction.
</details>

---

## Part B — Statistics / percentiles

### B1
1000 latency samples: min 90 ns, p50 100 ns, p90 105 ns, p99 **900 ns**,
p99.9 **950 ns**, max 970 ns. Describe the distribution shape. Where's the
"1 in 100" event coming from, roughly how frequent, and is p99.9 meaningful
here?

<details><summary>Answer</summary>

**Bimodal.** 99% of samples are in a very tight band (90–105 ns) — a clean
fast path. Then a **hard cliff**: p99→p99.9→max all clustered ~900–970 ns,
~9× the median, also tight. So there's a **rare slow path** that, when
taken, costs a consistent ~900 ns (not a smear — a specific fixed-cost
event: a cache-cold branch, a periodic flush, a lock that's usually
uncontended). Frequency: it's between p99 (present) and... we only have
1000 samples, so ~10 samples are in the slow cluster → roughly **1%**,
maybe a bit under. **p99.9 is NOT statistically meaningful** here — p99.9 of
1000 samples = the 999th–1000th sample = 1 data point. It happens to read
950 because the slow cluster is tight, but you can't claim a real p99.9
without ~100k+ samples. Report: "p50 100 ns; ~1% of requests hit a ~900 ns
slow path (N=1000, so tail beyond p99 is indicative only)". Next: find the
slow path's trigger.
</details>

### B2
Load test tool sends 50k req/s (fixed rate), one outstanding request at a
time, waits for each reply. Server normally 15 µs. A 5 ms stall occurs once.
Reported p99 barely moves. Real production p99 under the same offered load
would be much worse. Explain, and give the fix.

<details><summary>Answer</summary>

**Coordinated omission.** At 50k req/s the tool should send one request
every 20 µs. During the 5 ms stall it's blocked waiting for the stuck
reply, so it sends **0** requests instead of ~250. Those ~250 requests that
*should* have been offered (and would have queued behind the stall, each
seeing 5 ms, 4.98 ms, ... down to 15 µs) are simply **never measured**.
Result: 1 bad sample (~5 ms) instead of ~250 → dwarfed in the percentiles →
p99 stays ~15 µs. In real production, offered load doesn't pause for your
server — those 250 requests arrive, pile up, and real p99 is milliseconds.
**Fixes:** (1) **constant-rate sender** — a separate thread emits on
schedule regardless of replies (async, many outstanding). (2) **interval
correction** — for a reply that's `T` late vs expected interval `I`, also
record synthetic samples `T-I, T-2I, ...` (HdrHistogram
`recordValueWithExpectedInterval(v, I)`). (3) use a tool that does this:
`wrk2`, `fortio`, HdrHistogram-based harnesses. (4) measure latency from
**request arrival/scheduled-time**, not from when the server dequeued it.
</details>

### B3
You benchmark two allocator configs. Config A: p50 80 ns, p99 200 ns,
p99.99 **40 µs**. Config B: p50 95 ns, p99 150 ns, p99.99 **900 ns**.
Which do you pick for (a) a batch data-processing job, (b) an HFT order
path? Justify with the numbers.

<details><summary>Answer</summary>

**(a) Batch job → Config A.** Batch cares about **throughput / total
time** ≈ driven by the common case. A's p50 (80) and even p99 (200) beat
B's (95 / 150 mixed). The 40 µs p99.99 event is rare (1 in 10,000) and in a
batch job it just adds a negligible amount to total runtime — amortized
away. Pick the faster typical case.
**(b) HFT order path → Config B.** HFT cares about the **tail** — a 40 µs
stall (Config A's p99.99) on the order path is a disaster: a missed quote, a
blown risk check window, an adverse fill, right when the market is moving.
B's worst realistic case is **900 ns** — bounded, predictable. Paying 15 ns
on the median (95 vs 80) to cut the p99.99 from 40 µs to 900 ns (44×) is an
easy trade. HFT: **bounded tail > fast median**. (Ideally: find why A
spikes to 40 µs — probably `mmap`/`munmap` or a global lock in the
allocator — and eliminate it, e.g. pre-allocated pools, folder 14.)
</details>

---

## Part C — Profiler output diagnosis

### C1
`perf stat ./app`: `task-clock 10,000 ms`, `CPUs utilized 0.25`,
`instructions 5e9`, `IPC 2.4`. App is supposed to be compute-heavy. What's
wrong, and what tool next?

<details><summary>Answer</summary>

`CPUs utilized 0.25` — the process was only actually running on a CPU **25%
of the wall-clock time**; 75% it was **off-CPU** (blocked). The `IPC 2.4` is
*fine* — when it runs, it runs well — but that's over the 25% it was
scheduled. The other 75% (7.5 s of a 10 s wall) it was **waiting**: I/O
(disk/network), a lock, a `sleep`, a condvar, waiting on another process.
"Compute-heavy" is wrong — it's **blocked-heavy**. `perf stat` (CPU-time
based) is misleading you here. Next: (1) `time ./app` → confirm `real >>
user+sys`. (2) **off-CPU analysis** — `perf sched record` +
`perf sched latency`, or an eBPF `offcputime` flame graph, to see *what*
it's blocked on and *where* in the code. (3) `strace -T -c ./app` — which
syscalls, cumulative time. Fix the blocking (async I/O, bigger buffers,
remove the lock, parallelize the wait), *then* worry about the compute.
</details>

### C2
`perf annotate` (recorded with `-e cycles`, no `:pp`) on a hot loop:
```
  0.4 :   mov    (%rdi,%rax,8), %rdx      ; load node
 41.0 :   mov    0x10(%rdx), %rsi         ; load node->next
  2.1 :   test   %rsi, %rsi
  3.0 :   ...
```
41% on `mov 0x10(%rdx), %rsi`. Diagnose, and what would you do to be sure?

<details><summary>Answer</summary>

`mov 0x10(%rdx), %rsi` = load `node->next` (offset 16). 41% of the
function's samples on a single **load** = that load is **stalling on a
cache miss** — this is a **pointer chase** (linked list / tree traversal),
each `node->next` is at an unpredictable address, so ~every iteration is an
L2/L3/DRAM miss (~15–200+ cycles) with nothing independent to hide it
(folder 32). The previous load (`(%rdi,%rax,8)`) is only 0.4% — that array
access is prefetchable/sequential. **To be sure:** (1) re-record with
`-e cycles:pp` (PEBS/IBS) — the % should stay on this `mov` (precise), and
non-precise skid isn't misplacing it. (2) `perf record -e
mem_load_retired.l3_miss:pp -g` and annotate — miss samples land on this
exact load. (3) `perf stat -e cycle_activity.stalls_l3_miss` — big fraction
of cycles. **Fixes:** software prefetch `node->next->next` ahead (folder
32/06); convert the linked structure to an array / indices for sequential
access; pool-allocate nodes so `next` is usually nearby (folder 14);
restructure to have several independent chases in flight (more MLP).
</details>

### C3
`perf stat -M TopdownL1`: `Retiring 22%`, `Frontend Bound 51%`, `Backend
Bound 19%`, `Bad Speculation 8%`. Binary is a large C++ service with lots of
virtual calls and a big hot path. What does "Frontend Bound 51%" mean here,
and what are the fixes in priority order?

<details><summary>Answer</summary>

**Frontend Bound 51%** = over half the pipeline issue slots go empty because
the **frontend can't deliver instructions fast enough** — instruction fetch
/ decode is starving the backend. For a large C++ service this almost always
means **instruction-cache (L1i) and iTLB misses** and **BTB pressure**: the
hot path spans more code than fits in L1i (~32 KB) / the µop cache, so
execution constantly waits on code fetches, and the many virtual/indirect
calls thrash the branch target buffer. Fixes, priority order: (1) **PGO**
(folder 33/11) — profile-guided layout puts hot blocks together, splits cold
code (error handlers) out of line → dramatically better I-cache density;
biggest single win for frontend-bound C++. (2) **LTO + function reordering**
(`-ffunction-sections` + linker `--symbol-ordering-file` from a profile, or
BOLT post-link optimizer) — pack hot functions contiguously. (3)
**`[[unlikely]]` / `__builtin_expect`** on cold branches so the compiler
out-lines them (folder 33/08). (4) **Devirtualization** where possible
(`final`, CRTP, known types — folder 33/07) to cut indirect-call BTB
pressure and enable inlining. (5) **Reduce code size on the hot path** —
fewer template instantiations, `-Os` for cold TUs, avoid giant inlined
call chains. (6) **Huge pages for the text segment** (iTLB). Re-measure
top-down: Frontend Bound should fall and Retiring rise.
</details>

### C4
A differential flame graph (after − before) is mostly grey (unchanged), but
there's one bright **red** tower: `operator new` → `malloc` → `mmap`, ~12%
wide, that wasn't there before. The change was "use `std::vector` instead of
a raw array for the scratch buffer". Wall-clock time went up 10%. Diagnosis
and fix?

<details><summary>Answer</summary>

The switch to `std::vector` for a **per-call scratch buffer** now does a
heap allocation (`operator new` → `malloc`, and 12% of it reaching `mmap`
means the allocations are large enough or frequent enough that the allocator
is hitting the OS, not just its free-list) on **every call**, where the raw
array was a stack allocation (free) or a one-time allocation. The red tower
= the new allocation cost; the 10% wall-clock regression matches. **Fixes:**
(1) **hoist the `vector` out of the hot function** — make it a member / a
thread-local / a pool, `.clear()` (keeps capacity) and reuse across calls →
one allocation ever. (2) if the size is bounded and known, use a
**stack buffer** (`std::array` or a small local array) or a `small_vector`
(inline storage, heap only if it overflows). (3) if it must be dynamic and
per-call, at least `reserve()` once on a reused instance. The point: RAII
containers are great, but a `vector` **constructed in a hot loop** trades a
free stack allocation for a `malloc`/`free` pair (and here, `mmap` traffic)
— reuse the storage.
</details>

---

## Part D — Hands-on

### D1 — Timer characterization
`./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/01_timing_methods.cpp`.
Record: (a) each clock's resolution, (b) `steady_clock::now()` vs plain
`__rdtsc()` vs fenced-rdtsc self-cost, (c) does the 94 µs workload agree
between `steady_clock` and calibrated rdtsc? Explain why they agree at 94 µs
but wouldn't at 40 ns.

### D2 — The mean lies
`./build.ps1 fast .../02_percentiles.cpp`. Run it **5 times**. Which numbers
(min, p50, p90, p99, p99.9, p99.99, max) are stable run-to-run and which
aren't? Why does that map onto "report min or p50 for microbenchmarks, but
you need 100k+ samples to claim a p99.99"?

### D3 — Jitter: clean vs noisy
`./build.ps1 fast .../03_jitter_measure.cpp`. Compare CLEAN vs NOISY:
`min` (should be identical — why?), spike count, p99.9, max. The single
`max` sometimes comes out *higher* for CLEAN — explain why max is a bad
jitter metric and spike-count / p99.9 are better.

### D4 — Every benchmark bug, live
`./build.ps1 fast .../04_benchmark_mistakes.cpp`. For each of the 6 cases,
note the BUG number and the FIX number. Then: `./build.ps1 asm
.../04_benchmark_mistakes.cpp` and find one of the "BUG" loops in the asm —
confirm it's gone / hoisted / folded.

### D5 — Google-Benchmark-style
`cd 35-PROFILING-BENCHMARKING/examples/05_google_benchmark && ./build.ps1`.
Read the table: (a) `BM_reduce_NO_barrier` vs `_WITH_barrier` — the ratio.
(b) `BM_memcpy` vs `BM_manual_copy` at 4096 — the ratio, and why (`build.ps1
asm` the manual copy... it's `.cxx`, so `g++ -O2 -S -masm=intel bench.cxx`).
(c) `BM_StringCopy/8` vs `/64` — the jump, and what SSO threshold it implies.

### D6 — Production recorder
`./build.ps1 fast .../08_latency_recorder.cpp`. Record: `record()` ns/event
vs `vector.push_back()` ns/event; the histogram-vs-exact relative error at
each percentile; and describe the shape shown by the log-spaced display
(how many humps, roughly where). Why does the histogram cost stay flat while
`push_back` would eventually spike?

### D7 — (Linux) perf workflow
`bash .../06_perf_workflow.sh` on a Linux box. From `perf stat`: IPC — is
the demo compute- or memory-bound? From `perf annotate sum_mod` and `perf
annotate chase`: which instruction dominates each, and which folder-31/32/34
technique fixes it?

### D8 — (Linux) flame graph
`git clone .../FlameGraph ~/FlameGraph && bash .../07_flamegraph.sh`. Open
`_flame/cpu.svg`. Which function is the widest top plateau? It appears in
two places — why aren't they merged? Ctrl-F `heavy` — what total % is under
it?

---

## Interview questions (folder-wide)

1. Measure-don't-guess; Amdahl; premature optimization ka asli matlab.
2. `chrono` ke 3 clocks; `steady_clock` kyun; resolution vs self-cost.
3. `rdtsc`: fencing, calibration (ticks→ns), core hopping, kab vs `chrono`.
4. Mean kyun jhooth (latency); median/min/max/σ kab; bimodal.
5. Percentiles: nearest-rank, "nines", fan-out amplification, coordinated
   omission, kyun average nahi kar sakte.
6. Jitter: sources (SW + HW/firmware), `isolcpus`/`nohz_full`/`rcu_nocbs`,
   measuring, "quiet core".
7. Histograms: linear vs log-linear, HdrHistogram (mergeable, O(1),
   CO-correction), CDF plot.
8. Google Benchmark: `State` loop, `DoNotOptimize`/`ClobberMemory`, args,
   fixtures, pitfalls the library can't fix.
9. Benchmark pitfalls: DCE / const-fold / hoist / cold-start / timer-overhead
   / one-run / alignment / frequency / denormals / bench≠reality.
10. `perf`: stat (IPC, top-down) vs record/report vs annotate; skid & `:pp`;
    `perf mem` / `perf c2c`.
11. Flame graphs: axes, top plateau, on- vs off-CPU, differential.
12. Valgrind: cachegrind `Ir` (deterministic, CI), callgrind+KCachegrind,
    massif, DHAT — vs `perf`.
13. VTune / top-down: Retiring/Frontend/Backend/Bad-Spec; bandwidth- vs
    latency-bound; `toplev`.
14. Sanitizers: ASan/UBSan/TSan/MSan — what each catches; why not for perf
    numbers; `volatile` vs `atomic`.
15. Production measurement: inline rdtsc + per-thread histogram, SPSC ring,
    sampling, CO in prod, white vs black box.

---

## Next
→ [`../36-LOW-LATENCY-CPP/00-README.md`](../36-LOW-LATENCY-CPP/00-README.md)
