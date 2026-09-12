# 09 — Instruction latency & throughput, dependency chains, Agner Fog tables

## Prerequisites
- `04-pipelining.md` (latency vs throughput distinction)
- `05-superscalar.md` (ports)
- `01_instruction_timing.cpp`, `02_pipeline_stall.cpp`

## Yeh topic abhi kyun
Ab tak "latency vs throughput" aur "dependency chain" idea aaya. Yeh lesson use
**quantitative** banata: har instruction ke actual numbers kahan se milte
(Agner Fog / uops.info tables), unhe kaise padho, aur ek code snippet ka
**critical path** kaise nikaalo — kyunki wahi tumhare hot-path latency ka floor
hai.

---

## The two numbers, precisely

For each instruction, on each microarchitecture:

| Term | Definition | Units |
|---|---|---|
| **Latency** | cycles from when the inputs are ready to when the **result can be used** by a dependent instruction | cycles |
| **Throughput** (usually given as *reciprocal throughput*) | cycles between issuing two *independent* instances — i.e. `1 / (how many per cycle)` | cycles |
| **µops** | how many micro-ops it decodes to | count |
| **Ports** | which execution ports the µops can go to | e.g. p0, p1, p05 |

- Reciprocal throughput **0.25** = 4 per cycle (4 ports). **0.5** = 2 per cycle.
  **1** = 1 per cycle. **>1** = not fully pipelined (`div`, `sqrt`).
- Latency ≥ reciprocal throughput always. For `add`: latency 1, rt 0.25 — a
  dependent chain runs at 1/cyc, independent adds at 4/cyc. For `imul`: latency
  ~3, rt ~1 — chain at 1/3 the rate of independent muls.

---

## Representative numbers (modern x86, ballpark — check your µarch!)

| Instruction | Latency (cyc) | Recip. throughput (cyc) | Notes |
|---|---|---|---|
| `mov r,r` / `xor r,r` (zeroing) | 0–1 | 0.25 | zeroing idiom is often "free" (renamed) |
| `add`, `sub`, `and`, `or`, `lea` (simple) | 1 | 0.25–0.33 | 3–4 ALU ports |
| `lea` (3-component, `[b+i*s+d]`) | ~3 | 1 | complex form is slower |
| `shl`/`shr` by immediate | 1 | 0.5 | by `cl`: worse on some µarchs |
| `imul r64, r64` | ~3 | ~1 | one multiplier port |
| `mulx` / `imul` (128-bit result) | ~3–4 | ~1–2 | |
| `div r32` | ~15–25 | ~5–10 | not pipelined |
| `div r64` / `idiv r64` | ~20–45 | ~20–45 | **not pipelined**, data-dependent latency |
| `popcnt`, `lzcnt`, `tzcnt` | ~3 | ~1 | |
| `pdep`/`pext` (BMI2) | ~3 (Intel) / **~18** (older AMD) | varies | µarch-sensitive! |
| `load` (L1 hit) | ~4–5 | ~0.5 | 2 load ports; +misalign penalty |
| `store` (L1) | ~1 (to store buffer) | ~1 | actual write later |
| `addss`/`mulss` (scalar FP) | ~3–4 | ~0.5 | |
| `vaddps`/`vmulps` (256b) | ~3–4 | ~0.5 | AVX2 |
| `vfmadd...ps` (FMA) | ~4 | ~0.5 | mul+add in one, same latency as add |
| `sqrtss` | ~12–20 | ~5–6 | not pipelined |
| `rsqrtps` (approx) | ~4 | ~1 | ~11-bit precision; +1 Newton step ≈ full |
| `divps` (256b) | ~10–15 | ~5–8 | |
| `mfence` | ~20–40+ | serialising | folder 27 |
| `lock add [m], r` | ~15–25 | contended: much worse | folder 28 |
| `rdtsc` / `rdtscp` | ~20–30 / ~30–45 | — | example `08` measured ~20/45 cyc on this box |
| mispredicted branch | ~15–20 (penalty) | — | file `07` |
| denormal FP assist | ~100–200 | — | file `02`, `13` — flush-to-zero avoids |

**Measured on this box** (AMD Zen 2, ~2 GHz, examples `01`/`02`):
`div` latency ~6 ns (~12 cyc) — data-dependent, small divisors here; serial
`imul` chain ~1.2–1.7 ns/op (~2.5–3.5 cyc); 4 independent chains ~4× faster.

---

## Where to get the real numbers

| Source | What |
|---|---|
| **Agner Fog's instruction tables** (agner.org/optimize) | the canonical hand-measured tables, per µarch, PDF |
| **uops.info** | automated, per-instruction, per-µarch: latency, throughput, ports, µop breakdown |
| **`llvm-mca`** | `g++ -S ... \| llvm-mca -mcpu=native` — static pipeline analysis of *your* code, with a per-port bottleneck report |
| **uiCA** (uica.uops.info) | similar, cycle-accurate loop analysis |
| **`perf stat` / `perf annotate`** | *dynamic* — real cycles on real data, finds the actual hot instruction |
| **Intel SDM / AMD PPR** | vendor manuals — latencies for some, plus the semantics |

For a hot loop: `llvm-mca` for the static critical-path + port picture, then
`perf annotate` on the real workload to confirm where cycles actually go.

---

## Computing the critical path

Draw the data-flow graph of one loop iteration. Each node = an instruction,
labelled with its latency. Each edge = "this result feeds that input". The
**critical path** = the longest chain of latencies from a loop-carried input
back to itself.

```
Example: for (i) { acc = acc * a[i] + b[i]; }   (Horner-style)
  loop-carried: acc
  per iteration:  load a[i] (~5, NOT on carried path -- a[i] independent of acc)
                  imul acc, a[i]   (~3)  <- on carried path
                  add  acc, b[i]   (~1)  <- on carried path
  critical path per iter = 3 + 1 = 4 cycles  -> ~4 cyc/element floor
```

To go faster you must **shorten the carried chain** (here: split `acc` into 2–4
partial Horner accumulators over strided sub-sequences, combine at the end — but
Horner's recurrence makes this non-trivial; a plain sum/dot-product splits
cleanly, example `02`/`05`).

`llvm-mca -bottleneck-analysis` prints exactly this: "Cycles with backend
pressure increase" and which resource / the critical dependency.

---

## The division problem, concretely

`div`/`idiv` r64: ~20–45 cycles, **not pipelined**, latency depends on the
operand magnitudes. On a dependency chain, back-to-back divs are catastrophic.

| Situation | Fix |
|---|---|
| Divisor is a **compile-time constant** | compiler emits reciprocal-multiply (magic number) automatically — no `div` |
| Divisor is **loop-invariant** (runtime) | hoist: `double inv = 1.0 / d;` once, then `x * inv` in the loop |
| Integer, divisor known at struct-build time | precompute Lemire/libdivide "magic" (`libdivide` library) — ~4 cheap ops per divide |
| Power of two | `x >> k` / `x & (n-1)` |
| `sqrt` on the hot path | `rsqrtps` + one Newton-Raphson iteration (~5 cyc, ~23-bit) if full precision isn't needed |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — copying latency numbers from a different µarch
`pdep`/`pext` is ~3 cyc on Intel but was **~18 cyc** on Zen 1/2 (microcoded).
`lea` with 3 components, shift-by-`cl`, `popcnt` on old CPUs — all µarch-
sensitive. Check *your* target (uops.info, `-mcpu=native` in llvm-mca).

### Trap 2 — counting instructions instead of the critical path
A loop with 20 instructions but a 4-cycle carried chain runs at ~4 cyc/iter (if
the 20 fit the ports). A loop with 6 instructions all on an 18-cycle chain runs
at 18. Fewer instructions ≠ faster.

### Trap 3 — `div` in a struct/hot-path getter
`price / lot_size`, `qty / 100`, `ns / 1000` — if the divisor is fixed, make it
a constant or a precomputed reciprocal. A `div` in a per-tick function is a
~30-cycle tax.

### Trap 4 — `std::sqrt` / `std::pow` / `std::log` treated as cheap
Library call + tens–hundreds of cycles. Polynomial approx for your input range,
or a table, or restructure (compare in log-space, etc. — file `03`).

### Trap 5 — measuring a single instruction directly
You measure `rdtsc` + fences + the loop. Use a long dependency chain / an
independent stream and divide (example `01`); or use `llvm-mca` for a static
answer.

### Trap 6 — ignoring load latency on the critical path
A `load` (~5 cyc L1, ~12 L2, ~40 L3, ~200+ DRAM) *on* the carried chain is often
the dominant term. Prefetch it, or restructure so the load isn't dependent on
the previous iteration (no pointer chasing — file `06`).

---

## > **HFT relevance**

> - **Compute the critical path of the tick→order chain.** That sum of
>   dependent latencies is your hard floor — no amount of ILP beats it. Attack
>   it: replace `div`/`sqrt`/transcendentals, hoist invariants, split serial
>   reductions, and get loads off the carried chain (prefetch, SoA).
> - **No runtime `div` on the hot path.** Constant → compiler handles it;
>   loop-invariant → hoist a reciprocal; struct-time-known → `libdivide` magic.
> - **`llvm-mca` every hot loop** — it tells you the limiting port and the
>   critical dependency in seconds, before you profile.
> - **Verify latencies on your actual target µarch** (uops.info /
>   `-mcpu=znver3` etc.) — the numbers move, especially for AMD/Intel/gen
>   differences (file `15`).
> - **`perf annotate`** on the real workload to confirm the static analysis and
>   catch data-dependent surprises (a `div` whose latency scales with operand
>   size, a load that misses only under load).

---

## Hands-on

```bash
# examples 01 + 02: latency vs throughput, and the critical-path effect
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/01_instruction_timing.cpp
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/02_pipeline_stall.cpp

# static analysis of a hot loop (per-port + critical path)
g++ -std=c++20 -O2 -S your.cpp -o - | llvm-mca -mcpu=native -bottleneck-analysis -timeline

# div vs reciprocal-multiply in the asm
cat > /tmp/d.cpp <<'EOF'
double dv(double x, double d){ return x / d; }         // -> divsd (runtime divisor)
double dr(double x){ static const double inv = 1.0/7.0; return x * inv; } // -> mulsd
int    ic(int x){ return x / 7; }                      // -> reciprocal multiply (constant)
EOF
g++ -std=c++20 -O2 -S -masm=intel /tmp/d.cpp -o - | grep -E 'dv\(|dr\(|ic\(|div|mul'
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "latency and throughput are the same number" | latency ≥ recip-throughput; `imul` 3 vs 1, `div` 30 vs 30 |
| "fewer instructions = faster" | the **critical path** (dependent latency sum) sets the floor |
| "`div` is like `mul`, ~3 cycles" | ~20–45, not pipelined, data-dependent |
| "`pdep`/`popcnt` cost is fixed across CPUs" | µarch-sensitive (pdep ~3 Intel vs ~18 old AMD) |
| "I can time one instruction in a loop" | you time `rdtsc`+fences+loop; use a chain / llvm-mca |
| "load latency doesn't matter, OoO hides it" | not when it's *on* the carried chain (pointer chasing) |

---

## Exercises

1. `for (i) sum += a[i];` — 1 accumulator, FP. `addss` latency 4, rt 0.5.
   Critical path per element? 4 accumulators?

   <details><summary>Answer</summary>

   1 accumulator: the carried chain is `sum → sum+a[i] → sum+a[i+1] → ...`, one
   4-cycle-latency FP add per element → **~4 cycles/element** (the `load a[i]`
   is independent of `sum`, so it overlaps). 4 accumulators (`s0..s3` over
   strided sub-sequences): 4 independent 4-cycle chains → limited by the FP-add
   port throughput (rt 0.5 = 2/cycle) → **~0.5 cycle/element** for the adds
   (each accumulator gets an add every 2 cycles, 4 accumulators = 2/cycle,
   port-saturated). ~8× faster. Combine `s0+s1+s2+s3` once at the end. SIMD
   (`vaddps`, example `05`) does this with 8 lanes per instruction — the
   measured ~4× (SSE) / ~9.5× (AVX2).
   </details>

2. `x / 3` jahan `x` ek `int` variable hai aur `3` literal — `div` emit hoga?
   `x / n` jahan `n` bhi `int` variable hai?

   <details><summary>Answer</summary>

   `x / 3`: **no `idiv`** — 3 is a compile-time constant, the compiler emits a
   signed reciprocal-multiply: `(x * 0x55555556) >> 32` plus a sign-correction
   add/shift — ~4-5 cheap instructions. `x / n` (n a runtime variable): the
   compiler **must** emit `idiv` (~20-30 cycles), because it can't precompute
   the magic number. If `n` is loop-invariant, hoist a `libdivide::divider<int>`
   built once outside the loop (it precomputes the magic and does ~4 ops per
   divide inside), or if it's floating-point-acceptable, `double inv = 1.0/n`
   and multiply.
   </details>

3. `llvm-mca` ek hot loop ke liye kehta "Bottleneck: SQRT/DIV unit; Critical
   sequence: vdivps -> vaddps -> vdivps". Iska matlab, aur teen possible fixes?

   <details><summary>Answer</summary>

   The `vdivps` (packed FP divide) is on the loop-carried critical path *and*
   the divide unit (not pipelined, ~5-8 cyc rt) is the throughput bottleneck —
   so the loop runs at roughly the divide rate. Fixes: (1) **Hoist the reciprocal**
   if the divisor is loop-invariant: `vrcpps` once (or `1.0/d`), then `vmulps`
   in the loop — turns a ~10-cyc divide into a ~4-cyc multiply, and it's
   pipelined. (2) **`vrcpps` + one Newton-Raphson step** if the divisor varies
   but ~22-bit precision is enough (~2 muls + 1 sub, all pipelined, ~9 cyc but
   throughput-friendly). (3) **Restructure the math** so the division isn't on
   the carried chain — e.g. accumulate numerators and denominators separately
   and divide once at the end, or compare cross-multiplied instead of dividing
   (`a/b > c/d` → `a*d > c*b` when signs are known).
   </details>

4. Tumne `pdep`/`pext` (BMI2) use karke ek bit-packing routine likhi jo Intel
   dev laptop pe fast thi. Production AMD Zen 2 box pe woh 5× slow. Kyun?

   <details><summary>Answer</summary>

   `pdep`/`pext` are ~3-cycle, single-µop on Intel (Haswell+). On AMD **Zen 1
   and Zen 2** they are **microcoded** — ~18 cycles and many µops (Zen 3 fixed
   this to ~3 cyc). So a routine built around them is ~6× slower on Zen 2. Lesson:
   BMI2 scatter/gather-bit instructions are µarch-sensitive — check uops.info for
   your *deployment* target, not your dev machine. Alternative: a portable
   shift-and-mask sequence, or a small lookup table, or SIMD `pshufb`-based
   packing, benchmarked on the real box (file `15`).
   </details>

5. Ek "constant-time" comparison likhna hai (crypto ke liye nahi, par ek
   deterministic-latency order-matching check). `if (a == b)` ki jagah branchless
   kaise, aur `div`/variable-latency ops se kyun bachna?

   <details><summary>Answer</summary>

   Branchless equality: `int neq = (a != b);` gives 0/1 with no branch (compiler
   emits `cmp` + `setne` or `sub` + normalize). For "return x if equal else y":
   `int m = -(a == b); result = (x & m) | (y & ~m);` (mask select), or let the
   compiler `cmov`. Avoid `div`/`sqrt`/denormals in such a path because their
   latency is **data-dependent** (div latency scales with operand magnitude;
   denormals trigger a ~100-cycle assist) — so the "constant-time" property
   breaks silently on certain inputs, giving you a latency spike exactly when
   some particular price/quantity comes through. Use fixed-latency integer/FP
   ops, flush-to-zero mode (file `02`), and mask-select instead of branches.
   </details>

---

## Interview questions

1. Latency vs reciprocal throughput — precise definitions, `imul` and `div` numbers.
2. Where do you get real per-instruction numbers (Agner Fog, uops.info, llvm-mca, perf)?
3. Critical path of a loop — how to compute it, why it's the latency floor.
4. `div` on the hot path — the four replacement strategies (constant, invariant, magic, power-of-two).
5. Why "fewer instructions" doesn't imply "faster".
6. A µarch-sensitive instruction (`pdep`, `lea`-3, shift-by-cl) — the trap.
7. Load latency on the carried chain (pointer chasing) — why OoO can't hide it.

---

## Next
→ [`10-simd-basics.md`](10-simd-basics.md)
