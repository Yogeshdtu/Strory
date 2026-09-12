# 13 — `-ffast-math`: kya todta hai, kab safe hai

## Prerequisites
- `03-VARIABLES-DATA-TYPES/09-float-deep-dive.md` (IEEE 754, rounding, NaN)
- `05-vectorization.md` (float reduction needs reassociation)

## Yeh topic abhi kyun
`-ffast-math` (aur `-Ofast` = `-O3 -ffast-math`) float loops ko 2-4x tez
kar sakta — vectorized reductions, FMA contraction, fewer instructions. Par
woh **IEEE 754 guarantees tod deta**: `NaN`/`Inf` handling, associativity,
signed zero, denormals. Result: kabhi silently galat answer, kabhi ek edge
case pe crash ya `NaN` propagation. Yeh lesson batata exactly kya toota,
aur kaunse individual flags safely use ho sakte.

---

## `-ffast-math` = 8 sub-flags ka bundle (GCC)

| Sub-flag | Kya assume/allow karta | Risk |
|---|---|---|
| **`-fno-math-errno`** | math functions (`sqrt`, `log`) `errno` set nahi karte | low — almost nobody checks `errno` after `sqrt`; **often safe alone** |
| **`-funsafe-math-optimizations`** | umbrella for the reassociation ones ↓ | — |
| **`-fassociative-math`** | `(a+b)+c == a+(b+c)` — reorder float sums | **changes results** (rounding); enables SIMD reductions |
| **`-freciprocal-math`** | `x/y` → `x * (1/y)` | rounding differs; `1/y` may overflow where `x/y` wouldn't |
| **`-fno-signed-zeros`** | `+0.0 == -0.0`, ignore the sign of zero | breaks `1.0 / x` sign at zero, `copysign`, some branch-cut math |
| **`-fno-trapping-math`** | FP ops don't raise traps/exceptions | can't use FP exception flags / signal handlers |
| **`-ffinite-math-only`** | **assumes no `NaN`, no `Inf` ever** | `x != x` (NaN check) → folded to `false`; `isnan`/`isinf` broken; a stray NaN silently corrupts everything downstream |
| **`-fcx-limited-range`** | complex arithmetic range shortcuts | complex only |
| (also) **`-fno-rounding-math`** (default) | assume round-to-nearest | fine unless you use `fesetround` |

`-Ofast` adds `-fallow-store-data-races` and a couple more on top.

---

## The two that actually matter

### `-ffinite-math-only` — the dangerous one
Compiler assumes **no value is ever `NaN` or `Inf`**. So:
```cpp
if (x != x) handle_nan();     // x != x is TRUE only for NaN -> folded to `if (false)` -> DELETED
if (std::isnan(y)) ...;       // isnan folded to false
if (v > 1e308) ...;           // "can't happen" -> maybe deleted
```
If a `NaN` *does* appear (bad input, `0.0/0.0`, `sqrt(-1)`, an uninitialized
read), your guards are gone → it propagates silently through every
computation (`NaN + anything = NaN`) and you ship garbage prices / a `NaN`
order size. This has caused real production incidents.

### `-fassociative-math` — the useful one
Lets the compiler reassociate float `+`/`*`:
```cpp
for (i) s += a[i];   // -> 4/8 partial sums + a horizontal add at the end
```
This **changes the result** (different rounding order → last-few-ULP
differences, and catastrophic-cancellation cases can differ more). But it's
what makes a float reduction vectorize (example `03` CASE 2: **~4x** with it,
nothing without). If your algorithm is not sensitive to summation order (or
you use a compensated sum), this is a legitimate, scoped win.

---

## Measured — example `03` CASE 2

```
  plain -O2      : 0.72 ns/elem   (strict left-fold, scalar addss)
  -ffast-math    : 0.18 ns/elem   (~4x -- 4 partial sums, vectorized)
```
The speedup is real. The cost is that the sum is now computed in a different
order → a different (usually negligible, occasionally not) result.

---

## FMA contraction (separate knob)

`-ffp-contract=fast` (ON at `-O2`/`-O3` in GCC by default!) lets `a*b + c`
become a single **FMA** — which is *more* accurate (one rounding instead of
two) but **different** from separate `mul`+`add`. `-ffp-contract=off` for
bit-exact reproducibility with a reference that doesn't use FMA.
`-std=c++20` strict + `#pragma STDC FP_CONTRACT OFF` for a region.

---

## When is `-ffast-math` (or parts) safe

| Situation | Verdict |
|---|---|
| Graphics / audio / games / ML training | usually fine — perceptual, not exact |
| A dot-product / sum where order doesn't matter, inputs bounded | `-fassociative-math` + `-fno-signed-zeros` on that TU, **validate numerically** |
| `-fno-math-errno` alone | almost always safe (who checks `errno` after `sqrt`?) |
| Financial pricing, risk, anything reported / audited | **no `-ffast-math`** — need reproducibility + correct NaN/Inf |
| Cross-machine reproducibility required | no — even `-ffp-contract` breaks it |
| Any code where a `NaN`/`Inf` could appear from bad data | **never `-ffinite-math-only`** |
| A library you ship | never globally — a user's `NaN` guard would break |

**Rule:** never `-ffast-math` / `-Ofast` project-wide. If a specific kernel
needs reassociation, enable the *minimal* sub-flags on *that TU only*, and
diff the output against a strict-IEEE reference on representative data.

---

## Scoped alternatives (safer)

```cpp
// vectorize THIS reduction without global math changes:
#pragma omp simd reduction(+:s)         // + build with -fopenmp-simd
for (int i = 0; i < n; ++i) s += a[i];

// per-function:
__attribute__((optimize("fast-math")))  // GCC -- just this function
float sum_fast(const float* a, int n) { ... }

// FP contract for a region:
#pragma STDC FP_CONTRACT OFF
// ... reference-exact code ...
#pragma STDC FP_CONTRACT ON

// explicit, visible reassociation (no flag): 4 named accumulators in source
float s0=0,s1=0,s2=0,s3=0;
for (int i=0;i+3<n;i+=4){s0+=a[i];s1+=a[i+1];s2+=a[i+2];s3+=a[i+3];}
float s = (s0+s1)+(s2+s3);   // YOU chose this order; plain -O2 vectorizes it
```
The last one is best for HFT: the reassociation is **in the source, visible,
reviewable**, and no compiler flag can surprise you.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `-Ofast` "because it's the fastest -O"
`-Ofast` = `-O3 -ffast-math`. You just silently turned off NaN handling and
changed every float result. Use `-O3` and add math flags deliberately.

### Trap 2 — `-ffinite-math-only` with untrusted input
A malformed feed value, a `0/0`, a `log(-1)` → `NaN` → your `isnan` guard is
compiled to `false` → silent corruption. Never on input-facing float code.

### Trap 3 — global `-ffast-math`, then a subtle wrong answer weeks later
It's not a crash — it's a report that's off by 0.3%, or a risk number that's
wrong only when a position is near zero (`-fno-signed-zeros`). Very hard to
debug. Keep it scoped.

### Trap 4 — assuming `-ffast-math` results are "close enough"
For a well-conditioned sum, yes (~ULPs). For catastrophic cancellation,
near-singular matrices, iterative solvers → the error can blow up. Depends
on the algorithm; test.

### Trap 5 — `-ffp-contract=fast` surprising a cross-check
FMA changes `a*b+c` even without `-ffast-math` (it's on by default at `-O2`).
Your bit-exact comparison with a non-FMA reference fails. `-ffp-contract=off`
for that comparison.

### Trap 6 — mixing `-ffast-math` and non-`-ffast-math` TUs under LTO
The math semantics can leak / one setting wins (lesson 10). Be consistent;
prefer per-function attributes over per-TU flags if you must mix.

---

## > **HFT relevance**

> - **No `-ffast-math` / `-Ofast` on anything priced, reported, or audited.**
>   Reproducibility and correct `NaN`/`Inf` are non-negotiable for financial
>   numbers.
> - **If a specific hot kernel needs SIMD reduction** — a signal sum, a
>   dot-product — do the reassociation **in the source** (N named
>   accumulators) so `-O2` vectorizes it and the numerics are reviewable. Or
>   `#pragma omp simd reduction` scoped to that loop. Then diff against a
>   strict reference on a captured day.
> - **`-fno-math-errno` project-wide is usually fine** — it's the one benign
>   piece — but confirm nothing checks `errno` after libm calls.
> - **Decide `-ffp-contract`** deliberately — `fast` (default) for speed +
>   slightly better accuracy, `off` for bit-exact cross-checks.
> - **Guard against `NaN` explicitly** at input boundaries (`if (!std::isfinite(x))
>   reject;`) and **never** compile that guard with `-ffinite-math-only`.

---

## Hands-on

```bash
# the reduction speedup + result change:
g++ -O2              33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o r_strict && ./r_strict | grep "CASE 2" -A1
g++ -O2 -ffast-math  33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o r_fast   && ./r_fast   | grep "CASE 2" -A1

# what -ffast-math turns on:
gcc -ffast-math -Q --help=optimizers | grep -E "math|associat|signed-zero|finite|errno|trap|reciproc"

# the NaN-guard deletion:
echo 'bool bad(double x){return x != x;}' | g++ -O2 -ffinite-math-only -S -masm=intel -xc++ - -o - | grep -A3 'bad('
#   -> `xor eax, eax ; ret`   (always returns false -- your NaN check is gone)

# FMA contraction:
echo 'double f(double a,double b,double c){return a*b+c;}' | g++ -O2 -S -masm=intel -xc++ - -o - | grep -E 'fmadd|mulsd'
echo 'double f(double a,double b,double c){return a*b+c;}' | g++ -O2 -ffp-contract=off -S -masm=intel -xc++ - -o - | grep -E 'fmadd|mulsd'
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`-Ofast` = fastest safe `-O`" | `-O3 -ffast-math`; changes every float result, breaks NaN handling |
| "`-ffast-math` just loses a few ULPs" | true for well-conditioned; can blow up for cancellation / iterative |
| "`isnan` still works with `-ffinite-math-only`" | folded to `false` — your guard is deleted |
| "FMA only with `-ffast-math`" | `-ffp-contract=fast` is ON by default at `-O2` |
| "enable it globally, it's fine" | scoped only; a wrong result weeks later is un-debuggable |
| "reassociation needs a flag" | or write N accumulators in the source — visible + no flag risk |

---

## Exercises

1. Team ne `-Ofast` project-wide lagaya, sab tests pass. 3 hafte baad ek
   client report karta ki ek exotic option ka price kabhi-kabhi `NaN` aata
   hai jabki input valid hai. Kya connection ho sakta?

   <details><summary>Answer</summary>

   `-Ofast` includes `-ffinite-math-only` → the code's `if (std::isnan(intermediate))
   fallback();` guards are compiled to `if (false)` → deleted. Somewhere in
   the pricing (a `sqrt` of a slightly-negative discriminant due to rounding,
   a `log` near zero, a `0.0/0.0` in a degenerate case) an intermediate
   `NaN` is produced that the guard *used* to catch and replace with a
   fallback. Now it propagates (`NaN` contaminates every subsequent `+`/`*`)
   straight to the output price. The inputs are valid; the intermediate
   isn't, and the safety net was optimized away. Fix: drop `-Ofast`, use
   `-O3` (+ scoped math flags only where proven safe), keep the `NaN` guards,
   and compile the pricing code *without* `-ffinite-math-only`.
   </details>

2. Ek hot `dot(a, b, n)` ko SIMD banana hai. `-ffast-math` (whole TU) vs
   `#pragma omp simd reduction(+:s)` vs 8 manual accumulators — HFT ke liye
   ranking + reasoning.

   <details><summary>Answer</summary>

   **Best: 8 manual accumulators in the source.** `float s0..s7 = 0; for (i
   += 8) sk += a[i+k]*b[i+k]; float s = ((s0+s1)+(s2+s3))+((s4+s5)+(s6+s7));`
   — the reassociation is explicit, visible in code review, deterministic,
   and plain `-O2` vectorizes the independent `sk +=` loop. No compiler flag
   can surprise you, and you can pick a numerically-nicer combine tree if
   needed. **Second: `#pragma omp simd reduction(+:s)` + `-fopenmp-simd`** —
   scoped to this one loop, no global math change, but the exact partial-sum
   arrangement is the compiler's choice (less controlled than writing it).
   **Worst for HFT: `-ffast-math` on the TU** — also enables
   `-ffinite-math-only`, `-fno-signed-zeros`, `-freciprocal-math` on
   *everything* in that file, not just the dot product. Whichever you pick,
   diff the result against a strict-IEEE reference sum on a captured trading
   day and confirm the difference is within tolerance.
   </details>

3. `-ffp-contract=fast` (default) ke saath tumhara C++ result Python/numpy
   reference se ~1e-14 relative alag hai ek matmul mein. Kya, aur bit-exact
   kaise?

   <details><summary>Answer</summary>

   GCC contracted your `sum += a[i][k] * b[k][j]` into **FMA** instructions
   (`vfmadd...`) — one rounding per multiply-add instead of two. That's
   actually *more* accurate than numpy's separate `mul` then `add`, but
   **different**, hence the ~1e-14 (a few ULPs accumulated over the
   reduction). It's not a bug. For bit-exact agreement with a non-FMA
   reference: compile with **`-ffp-contract=off`** (globally, or `#pragma
   STDC FP_CONTRACT OFF` around the kernel) so `a*b+c` stays two rounded
   operations. You'll lose a little speed and a little accuracy, but match
   the reference. (Alternatively, make numpy use FMA — but you usually don't
   control the reference.) Decide which you need: fastest+slightly-more-
   accurate (FMA on) or reference-matching (FMA off).
   </details>

---

## Interview questions

1. `-ffast-math` — name 3 of its sub-flags and what each breaks.
2. `-ffinite-math-only` — why it's the dangerous one (NaN guard deletion).
3. `-fassociative-math` — what it enables (SIMD reduction) and what it costs.
4. `-Ofast` — what it actually is, why not to use it blindly.
5. `-ffp-contract` — FMA, default state, when to turn it off.
6. Safe scoped ways to vectorize a float reduction without global `-ffast-math`.
7. Why HFT pricing code must not use `-ffast-math`.

---

## Next
→ [`14-preventing-optimization.md`](14-preventing-optimization.md)
