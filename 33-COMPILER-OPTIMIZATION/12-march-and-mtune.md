# 12 — `-march`, `-mtune`, ISA extensions, portability trade-off

## Prerequisites
- `31-CPU-ARCHITECTURE/03-instruction-set.md`, `10-simd-basics.md`, `15-cpu-differences.md`
- `05-vectorization.md`

## Yeh topic abhi kyun
Default `-O2` ek **baseline ISA** ke liye code banata (`x86-64` = SSE2 only,
2003-era). Aapke CPU mein AVX2, FMA, BMI2, `popcnt`, `lzcnt` sab hain jo
compiler use hi nahi kar raha jab tak aap `-march` na do. Yeh 2-4x
vectorized loops pe, aur portability ka ek asli trade-off — galat `-march`
se binary doosre CPU pe `SIGILL` (illegal instruction) crash karta.

---

## `-march` vs `-mtune` vs `-mcpu`

| Flag | Kya |
|---|---|
| **`-march=X`** | "yeh binary CPU X (ya better) pe chalega" — X ke saare instructions **use kar sakta hai** (AVX2, FMA, ...). Older CPU pe → `SIGILL`. |
| **`-mtune=X`** | "X ke liye **schedule / tune** karo" (instruction ordering, unroll factors, alignment) — par koi naya instruction use nahi. Binary har baseline CPU pe chalta. Safe. |
| **`-march=X`** implies `-mtune=X` | unless you also pass `-mtune=Y` |
| **`-mcpu=X`** (ARM) | ~= `-march` + `-mtune` combined |
| **`-m<feature>`** | individual: `-mavx2 -mfma -mbmi2 -mpopcnt` — fine-grained enable |

```bash
-march=native      # THIS machine's exact ISA + tuning (from CPUID). NOT portable.
-mtune=native      # tune for this machine, ISA stays baseline. Portable.
-march=x86-64      # the 2003 baseline (SSE2). The default.
```

---

## x86-64 microarchitecture levels (portable feature bundles)

GCC 11+ / Clang 12+, glibc 2.33+ `ld.so` can pick per-level libs:

| Level | Adds (roughly) | ~CPUs |
|---|---|---|
| `x86-64` (v1) | SSE2 | everything since 2003 |
| **`x86-64-v2`** | SSE3, SSSE3, SSE4.1/4.2, POPCNT, CMPXCHG16B | ~2009+ (Nehalem, Bulldozer) |
| **`x86-64-v3`** | + AVX, **AVX2**, **FMA**, BMI1/2, LZCNT, MOVBE | ~2013+ (Haswell, Zen 1) |
| **`x86-64-v4`** | + AVX-512 (F/BW/DQ/VL/CD) | Skylake-X, Ice Lake, Zen 4 |

`-march=x86-64-v3` is the pragmatic "modern server" target — AVX2 + FMA + BMI2
without the AVX-512 downclock/availability issues. Most HFT boxes are v3 or v4.

---

## Kya milta `-march` se

- **Wider SIMD** — SSE2 (128-bit) → AVX2 (256-bit) → AVX-512 (512-bit).
  Example `03` MAP: plain `-O2` gave SSE 3.5x; `-march=native` → AVX2 → wider.
- **FMA** — `a*b + c` in one instruction (1 uop, ~4 cyc) instead of `mul`
  then `add` — halves the op count and rounds once.
- **BMI2** — `pdep`/`pext` (bit gather/scatter — bitboard, packet parsing),
  `bzhi`, `mulx`, `shrx`/`shlx` (flag-free shifts). ⚠️ `pdep`/`pext`
  microcoded ~18 cyc on Zen 1/2 (folder 31 lesson 15).
- **`popcnt` / `lzcnt` / `tzcnt`** — hardware bit-count instead of a lookup
  table / loop.
- **`movbe`** — load-and-byteswap in one instruction (big-endian wire parse).
- **Better scheduling** for the target's port layout.
- **CRC32 / AES / SHA / VAES / GFNI** — hardware crypto & checksums.

---

## The portability trade-off

`-march=native` binary built on a Zen 3 dev box, deployed on an older Zen 1
prod box → the first AVX-512... wait Zen 1 has no AVX-512. Built on Ice Lake,
run on Haswell → `SIGILL` on the first AVX-512 instruction. **Crash, not a
warning.**

Options, safest → fastest:

| Strategy | Portability | Speed |
|---|---|---|
| default (`x86-64`) | runs anywhere | baseline SSE2 |
| `-mtune=native` (or `-mtune=<prod-cpu>`) | runs anywhere | baseline ISA, tuned |
| **`-march=<explicit prod target>`** (e.g. `x86-64-v3`, `haswell`, `znver2`) | runs on that CPU class + newer | full ISA for that class |
| `-march=native` | **only this exact CPU class** | max |
| **function multi-versioning** (`__attribute__((target_clones("avx2","default")))` / `target(...)` + CPUID dispatch) | runs anywhere, uses best available | max where available |

**HFT answer:** `-march=<the exact production CPU>` (you own the hardware —
`znver3`, `icelake-server`, `x86-64-v4`, whatever it is). Not `native` (dev
box ≠ prod box), not baseline (leaving 2-4x on the table). Re-set it on every
hardware refresh and re-benchmark.

---

## Function multi-versioning (portable + fast)

```cpp
// GCC: automatic -- compiler builds N versions + a CPUID resolver
__attribute__((target_clones("avx2,fma", "sse4.2", "default")))
float dot(const float* a, const float* b, int n) { ... }

// Manual -- one function per ISA, dispatch once at startup
__attribute__((target("avx2,fma"))) float dot_avx2(...);
__attribute__((target("default")))  float dot_sse (...);
static auto* dot = cpu_has_avx2() ? dot_avx2 : dot_sse;   // resolve once
```
The resolver runs once (or is an IFUNC, resolved by the loader). Steady state
= a direct call to the chosen version. This is how portable libraries ship
one binary that's fast on every CPU. Folder 31 example `05`/`07` show the
CPUID + `target("avx2")` pattern.

---

## `-march` and `-flto` / PGO

- Under LTO, `-march` must be **consistent across all TUs** (like all flags,
  lesson 10). A mismatch → the linker picks one.
- `-march=native` + `-O3` + a 512-bit loop → **AVX-512 downclock** (folder 31
  lesson 13) can make the *whole process* slower. Measure; sometimes
  `-mprefer-vector-width=256` (keep AVX-512 features, cap width) is the win.
- PGO doesn't change `-march`; it just uses the available ISA better.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `-march=native` in a shipped/distributed binary
Built on the CI machine → CI's CPU. Deployed elsewhere → `SIGILL` or, worse,
subtly-wrong scheduling for the real target. Use an explicit target.

### Trap 2 — `-march` vs `-mtune` confusion
`-march=X` = *may use X's instructions* (portability risk). `-mtune=X` =
*schedule for X, no new instructions* (safe). Want "tuned but portable" →
`-mtune`.

### Trap 3 — assuming AVX-512 is always a win
Downclock, limited availability, `vzeroupper` overhead. Benchmark 256 vs 512;
often 256 (`x86-64-v3` / `-mprefer-vector-width=256`) wins overall.

### Trap 4 — `-march` without re-benchmarking
A wider `-march` can *lose*: bad `-O3` vectorization at the new width, freq
offset, code bloat. Always A/B, watch `perf stat` (IPC, freq, frontend).

### Trap 5 — one binary for a heterogeneous fleet, compiled `-march=native` on the newest box
The oldest boxes crash. Compile for the **oldest** box in the fleet, or use
multi-versioning, or per-class builds.

### Trap 6 — forgetting `-mtune` when you pin `-march`
`-march=x86-64-v3` implies `-mtune=x86-64-v3` (generic). If prod is
specifically `znver2`, `-march=x86-64-v3 -mtune=znver2` schedules better.

---

## > **HFT relevance**

> - **`-march=<exact production CPU>`** — you own the box; use its full ISA
>   (`znver2`/`znver3`/`icelake-server`/`x86-64-v4`). Not `native` (build
>   machine ≠ prod), not baseline.
> - **Re-benchmark on every hardware refresh** and re-pin `-march`. Folder 31
>   lesson 15's "which box?" is the first question.
> - **AVX-512: measure before committing** — downclock can net-lose. Try
>   `-mprefer-vector-width=256` (keep VBMI/masks, cap width).
> - **FMA + BMI2 + `popcnt` + `movbe`** are the quiet wins — FMA for pricing
>   math, BMI2/`popcnt` for order-book bitsets, `movbe` for big-endian feeds.
> - **Multi-versioning** if you truly must ship one binary across CPU
>   generations — `target_clones` + a startup resolve, then monomorphic.
> - **Consistency under LTO** — `-march` on every TU + the link.

---

## Hands-on

```bash
# what does native mean here?
g++ -march=native -### -E -x c++ /dev/null 2>&1 | grep -o "\-m[a-z0-9-]*" | sort -u   # (GCC 12+: -### shows it)
gcc -march=native -Q --help=target | grep -E "enabled|-mavx|-mfma|-mbmi" | head -30
echo | gcc -dM -E -march=native - | grep -E "__AVX2__|__FMA__|__BMI2__|SSE4"

# vectorization width jump:
g++ -O2                 -fopt-info-vec 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o x 2>&1 | grep 16-byte
g++ -O2 -march=native   -fopt-info-vec 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o x 2>&1 | grep 32-byte
./x   # compare CASE 1 ns/elem vs the plain -O2 run

# x86-64 levels:
for L in x86-64 x86-64-v2 x86-64-v3 x86-64-v4; do echo "== $L =="; \
  g++ -march=$L -Q --help=target 2>/dev/null | grep -c "\[enabled\]"; done
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`-march=native` for release" | build machine's CPU; use an explicit prod target |
| "`-march` and `-mtune` are the same" | `-march` may emit new instructions (crash risk); `-mtune` doesn't |
| "AVX-512 always faster" | downclock; often 256-bit wins overall — measure |
| "wider `-march` never regresses" | bad wide vectorization / freq / bloat can lose |
| "one `-march=native` binary for the fleet" | old boxes `SIGILL`; build for oldest or multi-version |
| "`-march` picks the fastest automatically" | it's a floor you promise; you must pick it |

---

## Exercises

1. Dev box Zen 4 (AVX-512), prod fleet mixed Zen 2 (v3) aur Zen 3 (v3). Ek
   binary ship karna hai. `-march` kya?

   <details><summary>Answer</summary>

   **`-march=x86-64-v3`** (or `-march=znver2` — the oldest prod
   microarchitecture), optionally `-mtune=znver3` if Zen 3 is the majority.
   Both Zen 2 and Zen 3 are `x86-64-v3` (AVX2 + FMA + BMI2, no AVX-512), so
   v3 gives you the full useful ISA for this fleet and runs on every box.
   **Not** `-march=native` (that's Zen 4 → AVX-512 instructions → `SIGILL`
   on every prod box). **Not** baseline (leaves AVX2/FMA on the table). If a
   subset of the fleet later gets Zen 4 and a specific kernel benefits from
   AVX-512, add function multi-versioning for that kernel rather than bumping
   the whole binary.
   </details>

2. `-march=native -O3` lagane ke baad ek hot pricing loop 2x faster, par
   overall process throughput 5% *kam* aur `perf` mein frequency ~200 MHz
   niche. Kya, fix?

   <details><summary>Answer</summary>

   The loop vectorized to **AVX-512** (or wide AVX2 with heavy 512-bit-ish
   use) → the core hit the **AVX frequency offset** (folder 31 lesson 13):
   running 512-bit SIMD forces a lower clock for that core (and, on some
   parts, briefly the package), so *every other* piece of code on that core
   — the parse, the book update, the network path — runs ~200 MHz slower.
   The pricing loop won 2x locally but the rest of the process lost more.
   Fix: `-mprefer-vector-width=256` (keep AVX-512 mask/VBMI features if
   useful, cap the vector width to 256 → no downclock), or `-march=x86-64-v3`
   for that TU, or isolate the wide-SIMD work onto a dedicated core that
   doesn't run latency-critical code. Re-benchmark end-to-end throughput and
   P99, not the loop in isolation.
   </details>

3. `__attribute__((target_clones("avx2,fma", "default")))` on a `dot()` —
   steady-state hot path pe iski cost kya, aur kab yeh worth NOT karna?

   <details><summary>Answer</summary>

   **Steady-state cost: ~zero.** The compiler builds two bodies + an IFUNC
   resolver; the dynamic linker runs the resolver **once** at load time (or
   first call) and patches the PLT/GOT so every subsequent call is a
   **direct call to the chosen version** — no per-call CPUID, no branch. The
   only overheads are (a) a slightly larger binary (two loop bodies), (b) the
   one-time resolve, (c) the function can't be inlined across the clone
   boundary (it's dispatched), which for a tiny hot `dot` in an inner loop
   could matter. **Not worth it when**: you control the exact hardware (just
   pin `-march` — simpler, and the function inlines), or the function is
   small and hot enough that losing inlining costs more than the ISA gain,
   or you only have one CPU class. Multi-versioning shines for **shipped
   libraries** that must run fast on unknown CPUs.
   </details>

---

## Interview questions

1. `-march` vs `-mtune` vs `-mcpu` — precise difference.
2. x86-64-v1/v2/v3/v4 — what each level adds.
3. Why `-march=native` is wrong for a shipped binary.
4. 4 concrete wins from `-march=x86-64-v3` over baseline (AVX2, FMA, BMI2, popcnt/movbe).
5. AVX-512 portability + downclock — why 256-bit sometimes wins.
6. Function multi-versioning — how dispatch works, steady-state cost.
7. `-march` under LTO — the consistency requirement.

---

## Next
→ [`13-fast-math-dangers.md`](13-fast-math-dangers.md)
