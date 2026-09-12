# 15 — CPU differences: Intel vs AMD, generations, what matters

## Prerequisites
- Poora folder 31 tak (`01`–`14`)

## Yeh topic abhi kyun
ISA (x86-64) same hai, par **microarchitecture** har vendor/generation mein alag:
pipeline depth, port count, cache sizes, predictor, SIMD offset, chiplet layout.
Ek optimization jo ek box pe jeetti woh doosre pe neutral ya haar sakti (files
`09`, `10`, `13` mein examples aaye). Yeh lesson: kaunse differences HFT ke liye
matter karte, aur "kis box pe deploy kar rahe ho" ko benchmark/tuning ka pehla
sawaal kyun banao.

---

## Same ISA, different machine

Every x86-64 CPU runs the same instructions. But these differ per µarch:

| Property | Varies how | Impact |
|---|---|---|
| **Pipeline depth** | ~14–20 stages | mispredict penalty (file `04`, `07`) |
| **Decode width / µop cache** | 4–8 wide; µop cache 1.5–4K entries | front-end throughput, code-size sensitivity |
| **Execution ports** | count + which ops per port | the limiting-port analysis (file `05`) — different optimal instruction mix |
| **ROB / RS / PRF sizes** | ROB ~180–512 | how much latency OoO can hide (file `06`) |
| **Cache sizes + latencies** | L1 32–48 KB, L2 256 KB–4 MB, L3 8–100+ MB | working-set fit (folder 32); L2 size is a big one |
| **L2/L3 latency in cycles** | L2 ~12–18, L3 ~35–70 | |
| **Branch predictor** | TAGE variants, history length, BTB size | which branches predict well |
| **Prefetchers** | number, aggressiveness, stride/stream detection | how well sequential/strided access hides latency (folder 32) |
| **SIMD** | AVX2 vs AVX-512; the frequency offset (file `13`) | vectorization ROI |
| **`pdep`/`pext`, some others** | fast (Intel, Zen 3+) vs microcoded (Zen 1/2) | file `09` — µarch-sensitive instructions |
| **Chiplet / mesh layout** | monolithic vs CCD/CCX vs tiles; SNC | intra-socket NUMA (file `14`) |
| **Uncore behaviour** | how aggressively L3/mesh clocks down | file `13` |
| **Atomics / `lock` cost, fence cost** | varies | folder 27/28 |

---

## Intel vs AMD (broad strokes — check the specific SKU)

| Area | Intel (recent server: Ice Lake / Sapphire Rapids / Emerald Rapids) | AMD (recent server: EPYC Zen 3 / Zen 4 / Zen 5) |
|---|---|---|
| Die layout | monolithic or tiled (SPR); **SNC** splits into 2–4 nodes/socket | **chiplets** — several CCDs, each a CCX with its own L3; cross-CCD via Infinity Fabric |
| L3 | large shared per socket (or per SNC node) | **per-CCX** (e.g. 32 MB), *not* shared across CCXs → cross-CCX = a fabric hop |
| Cache-to-cache (same socket) | ~fast within a node | fast within a CCX, slower across CCXs |
| AVX-512 | yes (client sometimes fused-off); historically a frequency offset (Skylake-X/CLX), much reduced on ICX+ | Zen 4+ has AVX-512 (double-pumped on Zen 4, native-ish Zen 5), **no significant frequency offset** |
| `pdep`/`pext` | ~3 cyc since Haswell | microcoded ~18 cyc on **Zen 1/2**; ~3 cyc Zen 3+ |
| Core count / socket | moderate–high | very high (up to 96–128+) — more CCXs to worry about |
| PMU / `perf` events | rich, well-documented (`perf list`, Intel VTune, `toplev`) | improving; some events named differently; `amd_uprof` |
| Typical HFT presence | very common (colocation standard) | growing; used where core count / memory bandwidth / power favour it |

**This repo's box:** AMD Ryzen 7 4700U, **Zen 2** (family 0x17), AVX2 (no
AVX-512), `pdep`/`pext` microcoded, 8 cores no SMT, ~2 GHz throttled (mobile
part). Its numbers (examples `01`–`08`) illustrate *shapes*, not production
absolutes.

---

## ARM64 — the "what if" (Graviton, Ampere)

Some HFT firms trial **AWS Graviton** (Neoverse cores) or bare-metal ARM for
cost/power. Differences that bite:

- **Weaker memory model** (folder 27) — `acquire`/`release` are *not* free like
  on x86-TSO; missing barriers that "worked" on x86 corrupt on ARM. Model-check
  + test on ARM CI.
- **SIMD:** NEON (128-bit) + **SVE/SVE2** (scalable, vector-length-agnostic) —
  different intrinsics; portable-SIMD libs (Highway) help (file `11`).
- **No `rdtsc`** — use `cntvct_el0` (the ARM virtual counter) for cycle-ish
  timing.
- **Different perf tooling**, different cache/prefetcher behaviour, generally
  wider front-ends and big ROBs.
- Often **excellent perf/watt and perf/$**, sometimes lower single-thread peak.

If ARM is on the table, treat it as a full second target: ARM build, ARM CI,
ARM benchmarks, re-audit memory ordering.

---

## What actually matters for HFT (the short list)

1. **L2 size** — does your hot working set (order book, hot lookup tables) fit?
   A 512 KB L2 vs a 2 MB L2 changes which structures stay fast (folder 32).
2. **Memory latency + prefetcher quality** — the dominant hot-path cost is
   usually a load; how well the box hides it.
3. **Chiplet / SNC layout** — where is "one NUMA node" really (file `14`)?
4. **AVX-512 + its frequency behaviour** — vectorization ROI (files `10`, `13`).
5. **Branch predictor strength** — how predictable your hot branches need to be.
6. **Frequency stability under sustained load** — turbo/thermal (file `13`).
7. **µarch-sensitive instructions** you rely on (`pdep`, gather, some string ops).
8. **`perf` event availability** — can you actually profile it in production?

---

## The rule: benchmark and tune *on the deployment target*

- **Build for the floor** (`-march=x86-64-v3` or a specific `-march=znver3` /
  `-march=sapphirerapids`), with a runtime CPUID assert (example `07`) — one
  binary that runs everywhere it must, using the features it's allowed.
- **Benchmark on the production box** (or an identical one), **frequency locked,
  SMT off, C-states off, mitigations per policy** (files `12`, `13`, `08`) —
  that's the only measurement that feeds a latency budget.
- **`perf` on production** to confirm the static analysis (`llvm-mca -mcpu=`) and
  catch µarch surprises (a downclock, a microcoded instruction, a prefetcher
  that doesn't like your stride).
- **Re-benchmark on every hardware refresh.** A new generation can flip a
  previous win (AVX-512 now free, `pdep` now fast, bigger L2, different chiplet
  layout).
- **Keep a config-vs-machine matrix** — which sysctls/BIOS/`isolcpus`/`-march`
  for which box model (folder 29 file 18).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — optimizing on the dev laptop, deploying to a server
Different µarch, different frequency behaviour, different cache sizes, SMT, NUMA.
The laptop tells you *ratios and shapes*; the server tells you *numbers*.

### Trap 2 — `-march=native` in the shipped binary
Bakes in the build machine's ISA. Runs (or #UD-crashes) only on ≥ that µarch.
Build for the deployment floor; CPUID-dispatch the rest.

### Trap 3 — assuming an AMD box behaves like an Intel one (or vice versa)
Per-CCX L3 (not shared across CCXs), microcoded `pdep`/`pext` on Zen 1/2,
different `perf` events, different chiplet NUMA. Re-audit.

### Trap 4 — copying Agner Fog / uops.info numbers for the wrong µarch
File `09`: latencies and port maps are per-µarch. Use `-mcpu=<target>` in
`llvm-mca`; look up the actual SKU on uops.info.

### Trap 5 — ignoring memory-ordering differences when trialing ARM
x86-TSO forgave a lot (folder 27). ARM won't. A "working" lock-free structure
can corrupt on Graviton. Full re-audit + ARM model-checking + ARM CI.

### Trap 6 — one-time tuning, never re-checked after a hardware refresh
A new generation flips wins: AVX-512 offset gone, `pdep` fast, L2 doubled,
mesh instead of ring. Re-benchmark the hot path on every refresh.

### Trap 7 — no CPUID assert at startup
The binary silently runs on a box missing a feature it needs (or takes a slow
fallback path). Assert the required feature set at startup and hard-fail (example
`07`).

---

## > **HFT relevance**

> - **"Which box?" is the first question** for any benchmark or tuning decision.
>   Intel vs AMD, generation, SKU, L2 size, AVX-512 behaviour, chiplet layout.
> - **Build for the deployment floor** (`-march=<floor>`), CPUID-assert +
>   dispatch (example `07`); never `-march=native` in the shipped binary.
> - **Benchmark on the production box, tuned** (frequency-locked, SMT-off,
>   C-states-off) — that's the number that feeds tick-to-trade budgets.
> - **`llvm-mca -mcpu=<target>`** for static analysis on the *right* µarch;
>   **`perf` on production** to confirm and catch surprises (downclock,
>   microcoded op, prefetcher stride issue).
> - **Re-benchmark the hot path on every hardware refresh** — new generations
>   flip old trade-offs.
> - **Keep a machine ↔ config matrix** (folder 29 file 18): `-march`, sysctls,
>   BIOS, `isolcpus`, C-state/turbo policy per box model.
> - **If trialing ARM (Graviton):** treat it as a full second target —
>   re-audit memory ordering (folder 27), ARM CI, ARM benchmarks, portable-SIMD.

---

## Hands-on

```bash
# identify the exact CPU + what it can do
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/07_cpu_info.cpp
lscpu | grep -E 'Model name|Flags' | head
cat /sys/devices/system/cpu/cpu0/cache/index*/size          # L1d/L1i/L2/L3

# what -march the compiler picks for "native" (and what that enables)
g++ -march=native -Q --help=target | grep -E 'march=|mtune=' | head
g++ -march=native -### -E - </dev/null 2>&1 | grep -o "'-march=[^']*'" | head -1

# static analysis for a SPECIFIC target
g++ -std=c++20 -O2 -S hot.cpp -o - | llvm-mca -mcpu=znver3        # or sapphirerapids, icelake-server, ...

# per-instruction data for your µarch: https://uops.info  /  agner.org/optimize
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "x86-64 = x86-64, one behaviour" | ISA same; µarch (ports, caches, predictor, chiplets) differs a lot |
| "AMD L3 is one big shared cache like Intel" | per-CCX on Zen; cross-CCX = a fabric hop |
| "`pdep` is ~3 cycles" | ~18 cyc microcoded on Zen 1/2; ~3 on Intel / Zen 3+ |
| "AVX-512 always downclocks" | Skylake-X/CLX yes; ICX+/Zen 4+ largely no |
| "benchmark on the laptop, ship it" | laptop = ratios/shapes; production box = numbers |
| "`-march=native` for the release build" | crashes on older CPUs; build for the floor |
| "tuned once, done forever" | re-benchmark on every hardware generation |

---

## Exercises

1. Ek hot lookup structure ~1.5 MB ka hai. Dev box ka L2 512 KB, production box
   ka L2 2 MB. Kya farak, aur benchmark kahan valid?

   <details><summary>Answer</summary>

   On the dev box the 1.5 MB structure **doesn't fit L2** → hot accesses miss to
   L3 (~40 cyc) or DRAM (~300 cyc), and the measurement is dominated by that. On
   the production box it **fits L2** (2 MB) → hot accesses are ~12–15 cyc — a
   completely different performance regime. Any latency number from the dev box
   is meaningless for capacity planning; only the production-box measurement
   (frequency-locked) feeds the budget. And a design choice like "shrink the
   structure to 480 KB to fit dev's L2" would be optimizing for the wrong
   machine. Benchmark working-set-sensitive code on the target's actual cache
   hierarchy (folder 32).
   </details>

2. Tumhare bit-packing code Intel Xeon pe fast tha (`pdep`/`pext` heavy),
   naya AMD EPYC Zen 2 fleet aaya aur woh 5× slow. Kya, aur do fixes?

   <details><summary>Answer</summary>

   `pdep`/`pext` are microcoded on Zen 1/2 (~18 cyc, many µops) vs ~3 cyc on
   Intel — a routine built around them collapses. Fixes: (1) **Detect at
   startup** (CPUID: check `bit_BMI2` *and* the µarch/family, or maintain a
   small allowlist of "fast pdep" models) and dispatch to a **shift-and-mask /
   lookup-table** implementation on Zen 2. (2) **Rewrite the packing** to not
   need arbitrary bit-scatter — a table-driven or SIMD `pshufb`-based approach
   that's portable and fast on both. (3) If the fleet is moving to Zen 3+
   (fast `pdep`), it may resolve itself — but don't assume; benchmark the actual
   deployed SKU. Lesson: file `09` — µarch-sensitive instructions must be
   validated on the deployment target, not the dev machine.
   </details>

3. "Build for the floor + CPUID assert + dispatch" — ek concrete example do
   (ek HFT firm ke paas Skylake, Ice Lake, aur Sapphire Rapids boxes hain).

   <details><summary>Answer</summary>

   Floor = the oldest box's feature set. Skylake-SP supports AVX-512 but with a
   frequency offset; Ice Lake and SPR support it cleanly. So: build the main
   binary with **`-march=skylake-avx512`** (or `-march=x86-64-v3` if you want to
   avoid AVX-512 entirely as the baseline and dispatch into it). At **startup**,
   `cpuid` to assert the required set (AVX2 + FMA + BMI2 + invariant-TSC at
   minimum) and **hard-fail** if missing (example `07`). For the hot SIMD
   kernels, provide multiple versions via `__attribute__((target_clones("avx2",
   "avx512f")))` or explicit `target("...")` functions + a runtime pick, so
   Ice Lake / SPR use the AVX-512 path and Skylake uses AVX2 (avoiding its
   downclock). Log the chosen path + CPU model at startup so a perf anomaly can
   be tied to a specific box class. Keep the machine↔config matrix (folder 29
   file 18) for the differing BIOS/sysctl/isolcpus per model.
   </details>

4. Ek team Graviton3 (ARM64) pe apna trading engine port kar rahi hai. Teen
   cheezein jo x86 se alag hain aur re-audit chahiye.

   <details><summary>Answer</summary>

   (1) **Memory ordering** — ARM has a weak memory model; `std::memory_order_
   acquire`/`release`/`relaxed` compile to actual barriers (`ldar`/`stlr`/`dmb`),
   and a lock-free structure that "worked" on x86-TSO (which only reorders
   store→load) can have missing synchronization that corrupts on ARM. Re-audit
   every atomic, run a model checker (herd7/GenMC) with the ARM model, and set up
   ARM CI with stress tests (folder 27/28). (2) **SIMD** — no SSE/AVX; NEON
   (128-bit) + SVE/SVE2 (scalable). Rewrite intrinsic kernels (or use a
   portable-SIMD library — Highway — file `11`), re-tune widths. (3) **Timing** —
   no `rdtsc`; use `cntvct_el0` / `clock_gettime(CLOCK_MONOTONIC)` (still vDSO on
   ARM Linux). Plus: different `perf` events, different cache/prefetcher
   behaviour, different `-march`/`-mcpu` (`neoverse-v1`), and re-run *all*
   latency benchmarks — the whole tuning is target-specific.
   </details>

5. Purane fleet pe AVX-512 turbo-offset ki wajah se tumne AVX2 (256-bit) hi use
   kiya. Naya Sapphire Rapids fleet aaya. Kya karoge?

   <details><summary>Answer</summary>

   Re-benchmark. Sapphire Rapids (and Ice Lake before it) largely removed the
   AVX-512 frequency offset, so the AVX-512 path that was a net loss on Skylake-X
   may now be a clear win (16 f32 lanes, better mask ops, `vcompress`/`vexpand`
   for filtering). Steps: (1) build an AVX-512 version of the hot kernels
   (`target_clones` / a `target("avx512f,avx512bw,avx512vl")` function). (2)
   Benchmark **end-to-end tick-to-trade** on an SPR box, frequency-locked, not
   just the kernel — confirm no residual downclock or thermal effect. (3) If it
   wins, enable the AVX-512 path via runtime dispatch (CPUID) for SPR/ICX boxes
   while the old Skylake fleet keeps the AVX2 path. (4) Update the machine↔config
   matrix. This is exactly why file `13`/`15` say "re-benchmark on every refresh".
   </details>

---

## Interview questions

1. Same ISA, different µarch — name 5 things that vary and their impact.
2. Intel vs AMD server CPUs — L3 layout (shared vs per-CCX), AVX-512, `pdep` cost.
3. `-march=native` vs building for a floor + CPUID dispatch — why the latter.
4. `llvm-mca -mcpu=` — why the target matters for the static analysis.
5. Trialing ARM64 (Graviton) — the three biggest things to re-audit (memory model, SIMD, timing).
6. The AVX-512 frequency offset across Intel generations — how it changes the vectorization decision.
7. Why re-benchmark the hot path on every hardware refresh — give an example of a flipped trade-off.

---

## Next
→ [`16-exercises.md`](16-exercises.md)
