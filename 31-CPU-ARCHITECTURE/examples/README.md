# Examples — Folder 31 (CPU architecture)

> Portable `.cpp` (Linux nahi chahiye — SIMD + `rdtsc` + CPUID sab x86-64 pe
> chalte, MinGW/Windows included). **`-O2` mandatory** — `-O0` pe har number
> bekaar (loop overhead sab chhupa deta). `./build.ps1 folder 31-CPU-ARCHITECTURE`
> → 8/8 OK. Benchmarks: `./build.ps1 fast <file>`.

## Compile / run

```bash
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/01_instruction_timing.cpp
# ... etc. `folder` compiles all 8 (debug flags, compile-check only).
```

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2, family 0x17), 8 cores / no SMT, running
**~2.0 GHz effective** (mobile part, power-save — example `08` calibrates this
from `rdtsc` vs `steady_clock`). AVX2 + FMA + BMI2 yes; **AVX-512 no**;
`pdep`/`pext` microcoded (Zen 2). Invariant-TSC yes.

**⇒ Quote the RATIOS, not the absolute ns.** A frequency-locked HFT box at
3–5 GHz gives different absolutes; the shapes (4× ILP, ~6–7× misprediction,
~4×/~9.5× SIMD, ~2.5–3× auto-vec) are microarchitectural and carry over. Numbers
also swing ±~30% run-to-run here as the laptop scales/throttles (files `13`,
`15`).

## Examples

| File | Lesson(s) | Kya dikhata / measured (this box, ~2 GHz) |
|---|---|---|
| `01_instruction_timing.cpp` | 04, 09 | latency (dependent chain) vs throughput (4 independent chains) of ADD/OR/MUL/DIV, with `keep()` inline-asm barriers so `-O2` doesn't DCE. **ADD lat ≈ thr (~0.15–0.4 ns); MUL lat ~2–3× thr; DIV lat ≈ thr, both ~4–6 ns (~12 cyc, least pipelined).** |
| `02_pipeline_stall.cpp` | 04, 05, 06 | one serial `imul`-chain vs 4 vs 8 independent chains. **serial ~1.2–1.7 ns/op → 4 parallel ~4× faster → 8 parallel no further gain** (multiplier port saturated at 1 `imul`-start/cycle). ILP demo. |
| `03_branch_prediction.cpp` | 07, 04 | `if (v[i] >= 128) s += v[i]` over random vs sorted data. **if-conversion disabled (pragma): RANDOM ~4–8 ns/elem vs SORTED ~0.5–1.4 ns/elem → ~6–7×.** ⚠️ **default `-O2` makes it a branchless `cmovge` → RANDOM == SORTED (no effect)** — the compiler already does example `04`'s job; that's the Rule 2 lesson. |
| `04_branch_free.cpp` | 07, 08 | branchy vs branchless (`x & -(cond)` mask), random vs sorted, `carry` threaded so the call can't be hoisted. **RANDOM: branchless ~6–7× faster; SORTED: branchy ~1.0–1.2× faster** (predictable branch is free; branchless pays the extra AND). |
| `05_simd_basics.cpp` | 10, 05 | float sum, scalar vs SSE (4-wide) vs AVX2 (8-wide, `__attribute__((target("avx2")))` + CPUID guard). **scalar 1.66 ns/elem → SSE 4.1× → AVX2 ~9.5×** (256 KB dataset = L2-resident = compute-bound; >L3 → converges to ~1× as it becomes bandwidth-bound). |
| `06_autovectorization.cpp` | 06, 10 | the SAME map function auto-vectorized vs `__attribute__((optimize("no-tree-vectorize")))`; plus a prefix-sum (loop-carried dependency) and a no-`__restrict` version. **auto-vec ~2.5–3× the scalar version** (SSE2 baseline, 4 ints/instr); prefix-sum & no-`__restrict` stay scalar. |
| `07_cpu_info.cpp` | 07(sec), 02, 03, 10 | CPUID: vendor, family/model/stepping, brand string, feature bits (SSE/AVX/AVX2/AVX-512/BMI/FMA/RDTSCP/**invariant-TSC**). On this box: `AuthenticAMD`, family 23, "AMD Ryzen 7 4700U", AVX2 yes / AVX-512 no. HFT startup-assert + dispatch pattern. |
| `08_rdtsc_timing.cpp` | 13, 09 | calibrate cycles/ns from `rdtsc` vs `steady_clock`; self-cost of `__rdtsc` / `lfence;rdtsc;lfence` / `rdtscp;lfence`; a barrier'd known loop. **calibrated ~2.0 cyc/ns (~2 GHz); `__rdtsc` ~20–25 cyc, serialized variants ~40–90 cyc; dependent-add loop ~1 cyc/iter.** |

## Notes / jaan-boojh kar cheezein

- **`keep()` / `asm volatile("" : "+r"(v))` barriers** in `01`, `02`, `08` —
  without them, `-O2` computed the loops in closed form / DCE'd them
  (`01` ADD & DIV latency showed `0.000` before the fix). This is the standard
  Google-Benchmark `DoNotOptimize` technique; it emits **zero instructions**.
- **`#pragma GCC optimize("no-if-conversion", ...)`** in `03`, `04` — default
  `-O2` if-converts `if (x >= thr) s += x` to a branchless `cmov`, which
  **eliminates the very effect the example measures**. The pragma keeps a real
  conditional jump. The "with default -O2 there's no difference" observation is
  itself taught (files `03`, `07`) — the compiler already does the branchless
  transform for simple predicates; you only intervene when the branch body is
  too complex to if-convert *and* the outcome is unpredictable.
- **`carry` threaded through `sum_ge`/`sum_branchy`** in `03`/`04` — a directly-
  named `noinline` pure function called in a `for r` loop with constant args
  gets **hoisted out** of the loop by CSE (`03`'s first version measured
  `0.000`). Threading a per-iteration `carry` (which doesn't affect the branch)
  defeats the hoist.
- **`05` AVX2 via `target("avx2")` + CPUID guard**, not `-mavx2` on the command
  line — so the file compiles at plain `-O2` and the AVX2 path is only *called*
  when `cpu_has_avx2()` is true (bina AVX2 wale CPU pe call = `#UD` crash).
- **No `broken_on_purpose` file.** Every example is a correct program measuring a
  microarchitectural property.
- **Numbers are this-box, ~2 GHz throttled AMD Zen 2.** They illustrate shapes.
  Real tick-to-trade budgets need a benchmark on the production box,
  frequency-locked + SMT-off + C-states-off (files `12`, `13`, `15`).
- All 8 compile clean under `-std=c++20 -Wall -Wextra -Wpedantic -Wshadow
  -Wconversion -Wsign-conversion -Wcast-align -Wunused -Wnull-dereference
  -Wdouble-promotion` (`./build.ps1 folder 31-CPU-ARCHITECTURE`).
