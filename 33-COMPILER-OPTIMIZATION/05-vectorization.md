# 05 — Auto-vectorization: kya rokta hai, `-fopt-info-vec`, pragmas

## Prerequisites
- `31-CPU-ARCHITECTURE/10-simd-basics.md`, `11-simd-intrinsics.md`
- `04-loop-optimizations.md`
- `09-aos-vs-soa-deep.md` (folder 32 — SoA is the prerequisite)

## Yeh topic abhi kyun
SIMD se ek loop 4-16x tez ho sakta — par sirf agar compiler use vectorize
kar sake. Yeh lesson batata **exactly kaunsi conditions** chahiye, kaunsi
cheezein silently vectorization band kar deti hain, aur kaise `-fopt-info-vec`
se compiler se poochho "kyun nahi hua". Manual intrinsics (folder 31) aakhri
option hai — pehle auto-vec ko enable karna aata chahiye.

---

## Auto-vec ke liye loop kaisa ho

| Chahiye | Kyun |
|---|---|
| **countable trip count** (`for (i = 0; i < n; ++i)`, `n` known at loop entry) | vectorizer ko `n / width` + `n % width` split karna hota |
| **no loop-carried dependency** | lane `i+1` ko lane `i` ka result nahi chahiye (prefix-sum ❌) |
| **contiguous / affine access** (`a[i]`, `a[2*i]`, `a[i][j]`) | `movups` ek instruction; `a[perm[i]]` needs a slow gather |
| **no aliasing doubt** (`__restrict` or provable) | store ke baad reload → no SIMD (lesson 09) |
| **no function calls in body** (unless inlined or a vector-math intrinsic) | can't vectorize an opaque `call` |
| **no `break` / early exit / `goto` out** | (some "search loops" vectorize with masking, limited) |
| **simple body** — arithmetic, compares, selects | complex control flow → scalar |
| **known/aligned-ish data** | misaligned handled via peeling, but a hint helps |

Aur **float reductions** (`s += a[i]`) ko **`-ffast-math` / `-fassociative-math`**
chahiye — partial sums = reassociation = different rounding, jo compiler bina
permission nahi karta (example `03` CASE 2).

---

## Measured — example `03` (is box, plain `-O2` = SSE2, 4 floats/vector)

| Case | Body | Result |
|---|---|---|
| **MAP** | `out[i] = a[i]*a[i]*0.5 + a[i] + rep` | scalar 0.52 → **vectorized 0.15 ns/elem (~3.5×)** — no reassociation needed, plain `-O2` |
| **REDUCE** | `s += a[i]` | **0.72 ns/elem = scalar speed** at plain `-O2`; **~4× with `-ffast-math`** (verified) |
| **PREFIX** | `out[i] = out[i-1] + a[i]` | **0.75 ns/elem — never vectorizes** (loop-carried dep, any `-O`, any `-ffast-math`) |

Add `-march=native` → MAP goes 8-wide (AVX2) → bigger. These are the three
categories: **safe map** (compiler does it), **reduction** (needs a
math-relaxation flag or a `#pragma`), **true dependency** (algorithm change
only).

---

## `-fopt-info-vec` — ask the compiler

```bash
g++ -O2 -fopt-info-vec         file.cpp -o x   # what vectorized
#   file.cpp:34:31: optimized: loop vectorized using 16 byte vectors

g++ -O2 -fopt-info-vec-missed  file.cpp -o x   # why NOT
#   file.cpp:44:9: missed: couldn't vectorize loop
#   file.cpp:44:9: missed: not vectorized: relevant stmt not supported: ...
#   file.cpp:12:5: missed: not vectorized: possible aliasing between ...
#   file.cpp:20:5: missed: not vectorized: control flow in loop.

g++ -O2 -fopt-info-vec-all     file.cpp -o x   # everything (verbose)
```
Clang: `-Rpass=loop-vectorize`, `-Rpass-missed=loop-vectorize`,
`-Rpass-analysis=loop-vectorize`.

"16 byte vectors" = SSE (128-bit). "32 byte" = AVX2 (need `-mavx2` /
`-march=`). "64 byte" = AVX-512.

---

## Pragmas — nudging the vectorizer

```cpp
// OpenMP SIMD -- opts into reassociation for THIS loop only (safest way to
// vectorize a float reduction without -ffast-math globally):
#pragma omp simd reduction(+:s)
for (int i = 0; i < n; ++i) s += a[i];
// build with -fopenmp-simd  (just the simd pragmas, no runtime/threads)

// "trust me, no aliasing / no dependency in this loop":
#pragma GCC ivdep                 // GCC
#pragma clang loop vectorize(assume_safety)
for (...) a[idx[i]] = b[i];       // you promise idx[] has no duplicates

// force / tune:
#pragma GCC unroll 4
#pragma clang loop vectorize_width(8) interleave_count(2)

// disable (for A/B, or when it hurts):
#pragma GCC optimize("no-tree-vectorize")   // function attribute form used in ex 03
```
⚠️ `ivdep` / `assume_safety` are **unchecked promises** — wrong → UB / silent
corruption. Only when you've proven it.

---

## Structural enablers (do these first)

1. **SoA layout** (folder 32 lesson 09) — `x[]` dense, not `particle.x` at
   stride 32. Auto-vec needs contiguous same-type data.
2. **`__restrict` on pointer params** (lesson 09) — kills the aliasing doubt.
3. **Inline the body / no calls** — or use `<cmath>` functions the compiler
   knows how to vectorize (with `-ffast-math` or libmvec: `-lm` +
   `-fno-math-errno`).
4. **Split the reduction** or use `#pragma omp simd reduction`.
5. **Separate the un-vectorizable part** (loop fission) — vectorize the map,
   leave the scan scalar.
6. **`alignas(32)` / `alignas(64)`** hot arrays — removes the peel loop,
   enables aligned `movaps`.

---

## Why NOT just always write intrinsics?

- **Portability** — `_mm256_*` is x86-only; auto-vec + `-march` retargets to
  NEON/SVE/AVX-512 for free. (Or use Highway / `std::simd` / xsimd.)
- **Maintainability** — a scalar loop is readable and correct-by-inspection;
  intrinsics are write-only.
- **The compiler often wins** — modern auto-vec handles tail/peel/alignment/
  remainder correctly; hand code has bugs.
- **Intrinsics when**: the compiler provably can't (shuffles, cross-lane,
  gather with a known pattern, a bit-twiddle SWAR trick), and you've measured
  a real gap. Folder 31 lesson 11.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — expecting a float reduction to vectorize at `-O2`
It won't (reassociation). Example `03`. Use `#pragma omp simd reduction` or
`-ffast-math` (scoped) — and verify the numbers are still acceptable.

### Trap 2 — `int` overflow-UB defeating vectorization... or enabling it
Signed `int` loop counters: the compiler *assumes no overflow* (UB), which
actually *helps* it prove the trip count. `unsigned` counters wrap defined →
sometimes the compiler is more conservative. `size_t` is fine; just be
consistent.

### Trap 3 — a `std::vector::operator[]` bounds check in the body
`-D_GLIBCXX_ASSERTIONS` or `.at()` puts a branch per element → no vectorize.
Release builds: no assertions, use `[]` or `.data()`.

### Trap 4 — `ivdep` / `assume_safety` on a loop that DOES have a dependency
Silent wrong results. These are promises, not requests. Prove it first
(no duplicate indices, no overlap).

### Trap 5 — reading "vectorized using 16 byte vectors" and thinking it's AVX
16 bytes = SSE (4 floats). Add `-march=native` / `-mavx2` for 32-byte.

### Trap 6 — vectorized but slower (memory-bound)
If the loop is bandwidth-bound (folder 32 lesson 13), SIMD does nothing —
you were never ALU-limited. `-fopt-info-vec` says "vectorized" but the
benchmark doesn't move. Reduce bytes, not add SIMD.

---

## > **HFT relevance**

> - **Structure the hot numeric loops to auto-vectorize**: SoA, `__restrict`,
>   no calls, `noexcept`, countable bounds, no asserts in release. Then
>   `-fopt-info-vec` must say "vectorized".
> - **`-march=<target>`** (not `native` for portability across the fleet) so
>   you actually get AVX2/AVX-512 width — folder 33 lesson 12.
> - **Float sums / dot-products**: `#pragma omp simd reduction(+:acc)` +
>   `-fopenmp-simd`, and numerically validate against a reference. Don't turn
>   on `-ffast-math` fleet-wide.
> - **Watch AVX-512 downclock** (folder 31 lesson 13) — a 512-bit loop can
>   slow the whole process. Sometimes 256-bit is the sweet spot.
> - **Intrinsics only where measured** — a specific parse/mask/shuffle the
>   auto-vectorizer can't express — and wrap them `always_inline`.
> - **Re-check vectorization on toolchain bumps** — GCC 13→15 cost-model
>   changes can gain or lose a loop.

---

## Hands-on

```bash
./build.ps1 fast 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp
g++ -std=c++20 -O2 -fopt-info-vec        33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o x
g++ -std=c++20 -O2 -fopt-info-vec-missed 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o x
g++ -std=c++20 -O2 -ffast-math -fopt-info-vec 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o xfm && ./xfm
#   -> CASE 2 (reduce) now vectorizes; measured ~4x faster

# see the vector body:
./build.ps1 asm 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp | grep -iE "xmm|ymm|mulps|addps"
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`-O2` vectorizes any float loop" | reductions need `-ffast-math`/`#pragma omp simd` |
| "prefix-sum will vectorize at -O3" | loop-carried dep — never; algorithm change |
| "`ivdep` = please try harder" | unchecked promise; wrong → UB |
| "16-byte vectors = AVX" | SSE (4 floats); need `-march` for 32-byte |
| "vectorized report = faster" | not if memory-bound; check the benchmark |
| "always write intrinsics" | auto-vec is portable + often correct; intrinsics = last resort |

---

## Exercises

1. `float dot(const float* a, const float* b, int n) { float s=0; for(i) s +=
   a[i]*b[i]; return s; }` — plain `-O2` pe vectorize? 3 tareeke isse SIMD
   banane ke, trade-offs ke saath.

   <details><summary>Answer</summary>

   Plain `-O2`: **no** (float reduction → reassociation). Ways: (1)
   **`#pragma omp simd reduction(+:s)`** + `-fopenmp-simd` — scoped, no
   global math change, compiler uses N partial sums; verify the result is
   within tolerance. (2) **`-ffast-math`** (or `-fassociative-math
   -fno-signed-zeros -fno-trapping-math`) on just this TU — broader, riskier
   (also changes NaN/Inf/denormal behaviour). (3) **Manual**: 4/8 partial
   accumulators in the source (`s0..s7`), combine at the end — you've done
   the reassociation explicitly and visibly, plain `-O2` then vectorizes the
   independent `si += a[i+k]*b[i+k]`. (4) **Intrinsics** (`_mm256_fmadd_ps`
   + horizontal add) — most control, least portable. For HFT: (1) or (3),
   numerically validated.
   </details>

2. `-fopt-info-vec-missed` bolta hai: `not vectorized: possible aliasing
   between 'out' and 'in'`. `out` aur `in` alag `std::vector`s hain, function
   3 pointer params leta hai. 2 fixes.

   <details><summary>Answer</summary>

   The function only sees pointers; it can't know the vectors are distinct.
   Fixes: (1) **`__restrict` on the pointer parameters** — `void f(float*
   __restrict out, const float* __restrict in, ...)` — a promise that they
   don't overlap; the aliasing check disappears, loop vectorizes. (2)
   **Inline the function** (move to a header / `-flto`) so the compiler sees
   the call site where `out.data()` and `in.data()` are provably different
   allocations — then it drops the check itself (example `04`: the `noinline`
   attribute is what *forced* the pessimism). (3) lesser: `#pragma GCC ivdep`
   before the loop (unchecked — only if you're sure). For HFT: `__restrict`
   on the API + `-flto`.
   </details>

3. Tumne ek loop ko `#pragma omp simd` se vectorize karaya, `-fopt-info-vec`
   confirm karta hai, par benchmark 0% tez. Kya conclude?

   <details><summary>Answer</summary>

   The loop was **not ALU-bound** — vectorizing the arithmetic doesn't help
   because the bottleneck is elsewhere: most likely **memory bandwidth**
   (working set >> LLC, folder 32 lesson 13 — SIMD reads/writes the same
   bytes, just faster per instruction, but the channels were already full),
   or a **latency-bound dependency** the vectorizer didn't remove, or
   **frontend/branch** limited. Check `perf stat --topdown`: if "Backend →
   Memory Bound" dominates, SIMD was never going to help — reduce bytes
   (smaller types, fewer passes) instead. If it's "Core Bound" with a
   dependency chain, you need more independent work (partial accumulators),
   not wider ones.
   </details>

---

## Interview questions

1. 5 conditions a loop needs for auto-vectorization.
2. Why a float reduction doesn't vectorize at `-O2`, and 2 ways to make it.
3. `-fopt-info-vec` vs `-fopt-info-vec-missed` — output examples.
4. `#pragma omp simd reduction` vs `-ffast-math` — scope + risk.
5. `#pragma GCC ivdep` / `assume_safety` — what it promises, failure mode.
6. "vectorized using 16 byte vectors" — what width, how to get more.
7. When to drop to intrinsics instead of auto-vec.

---

## Next
→ [`06-constant-folding.md`](06-constant-folding.md)
