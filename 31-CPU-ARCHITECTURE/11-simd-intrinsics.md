# 11 — SIMD intrinsics: writing them, auto-vec vs manual

## Prerequisites
- `10-simd-basics.md` (lanes, ISA ladder, SoA)
- `05_simd_basics.cpp`, `06_autovectorization.cpp`

## Yeh topic abhi kyun
File `10` ne concept diya. Ab **actual code**: intrinsics kaise likhte, naming
convention kaise padhte, common patterns (load/store/arith/compare/blend/
shuffle/horizontal-reduce), aur kab compiler pe chhod do vs kab haath se likho.
HFT mein manual SIMD wahan aata jahan compiler conservative hai — market-data
decode, custom shuffles, mask-heavy filters.

---

## The headers and types

```cpp
#include <immintrin.h>   // pulls in SSE..AVX-512 (guarded by -m flags / target attr)

// types:
__m128   // 4 x float      __m128d  // 2 x double     __m128i  // integers (any width)
__m256   // 8 x float      __m256d  // 4 x double     __m256i  // integers
__m512   // 16 x float     __m512d  // 8 x double     __m512i  // integers
__mmask8 / __mmask16 ...   // AVX-512 mask registers
```

Compile with the ISA enabled: `-msse4.2` / `-mavx2 -mfma` / `-march=x86-64-v3` /
`-march=native`. **Or** per-function `__attribute__((target("avx2")))` (example
`05`) so the rest of the file stays baseline and you CPUID-dispatch at runtime
(example `07`).

---

## Naming convention — how to read `_mm256_fmadd_ps`

```
   _mm  <width>  _  <operation>  _  <type>

   _mm         = SSE (128)     |  no number
   _mm256      = AVX (256)
   _mm512      = AVX-512

   operation   = add, sub, mul, div, fmadd, and, or, xor, cmp, min, max,
                 load, loadu, store, storeu, set1, setzero, blendv,
                 shuffle, permute, unpacklo, movemask, cvt..., hadd, ...

   type suffix:
     ps  = packed single (float32)      pd  = packed double (float64)
     ss  = scalar single (1 float)      sd  = scalar double
     epi8/16/32/64  = packed signed int    epu... = unsigned
     si128/si256    = whole integer register
```

So `_mm256_fmadd_ps(a, b, c)` = **256-bit**, **fused multiply-add**, **8×
float32**: returns `a*b + c` per lane, one instruction, ~4-cycle latency.

Reference: **Intel Intrinsics Guide** (searchable, per-intrinsic latency/
throughput/ISA) — bookmark it.

---

## The core patterns

### Load / store
```cpp
__m256 v  = _mm256_loadu_ps(p);        // unaligned load 8 floats (safe, fast if aligned)
__m256 va = _mm256_load_ps(p);         // aligned (p must be 32-byte aligned) -- UB if not
_mm256_storeu_ps(q, v);
__m256 b1 = _mm256_set1_ps(3.5f);      // broadcast: [3.5, 3.5, ... x8]
__m256 z  = _mm256_setzero_ps();
__m256i idx = _mm256_set_epi32(7,6,5,4,3,2,1,0);   // note: reversed (lane 0 last arg)
```

### Arithmetic
```cpp
__m256 s = _mm256_add_ps(a, b);
__m256 p = _mm256_mul_ps(a, b);
__m256 f = _mm256_fmadd_ps(a, b, c);   // a*b + c  (needs -mfma)
__m256 mn = _mm256_min_ps(a, b);       // per-lane min -- branchless
__m256 r  = _mm256_div_ps(a, b);       // slow-ish; prefer rcp+Newton if precision allows
__m256 rc = _mm256_rcp_ps(a);          // ~11-bit reciprocal approx, fast
```

### Compare → mask, then select (predication)
```cpp
// SSE/AVX2: compare produces an all-ones / all-zeros mask per lane
__m256 gt = _mm256_cmp_ps(a, thr, _CMP_GT_OQ);   // lane = 0xFFFFFFFF where a>thr, else 0
__m256 out = _mm256_blendv_ps(else_v, then_v, gt); // per-lane: gt ? then_v : else_v
int   bits = _mm256_movemask_ps(gt);              // 8-bit summary: bit i = lane i's sign

// AVX-512: real mask registers + masked ops
__mmask16 m = _mm512_cmp_ps_mask(a, thr, _CMP_GT_OQ);
__m512 out2 = _mm512_mask_add_ps(base, m, x, y);  // only lanes where m is set
```

### Integer / bytes (market-data decode)
```cpp
__m128i raw = _mm_loadu_si128((const __m128i*)bytes);
__m128i sw  = _mm_shuffle_epi8(raw, byteswap_pattern);   // pshufb -- endian swap + field extract
__m256i w   = _mm256_add_epi32(x, y);
int     mv  = _mm_movemask_epi8(_mm_cmpeq_epi8(a, b));   // memcmp-ish
```

### Horizontal reduction (sum the lanes — example `05`)
```cpp
// sum 8 floats in a __m256 -> one float
__m128 lo = _mm256_castps256_ps128(v);
__m128 hi = _mm256_extractf128_ps(v, 1);
__m128 s4 = _mm_add_ps(lo, hi);                 // 4 partial sums
__m128 s2 = _mm_add_ps(s4, _mm_movehl_ps(s4, s4));
__m128 s1 = _mm_add_ss(s2, _mm_shuffle_ps(s2, s2, 1));
float total = _mm_cvtss_f32(s1);
// (or just store to a float[8] and add scalar -- example 05 does this; simpler, ~same)
```

### Shuffle / permute (rearrange lanes)
```cpp
__m256 p = _mm256_permutevar8x32_ps(v, idx);   // gather-within-register by index vector
__m128 s = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3,2,1,0));  // pick lanes from a and b
```
Shuffles are the trickiest part — lane-numbering, the `_MM_SHUFFLE` macro
(reversed), 128-bit "lanes" inside 256-bit registers (AVX2 many ops are
"in-lane"). Draw it out; test with printouts.

---

## Tail handling

`N` not a multiple of width → the last `N % W` elements:
```cpp
std::size_t i = 0;
for (; i + 8 <= n; i += 8) { /* vector body */ }
for (; i < n; ++i)          { /* scalar body */ }   // examples 05, 06
```
AVX-512: a **masked load/store** for the tail (`_mm512_maskz_loadu_ps(tail_mask,
p+i)`) avoids the scalar loop.

---

## Auto-vec vs intrinsics — the decision

| Use auto-vectorization when | Use intrinsics when |
|---|---|
| simple map / reduce / filter over contiguous data | the compiler won't vectorize it (`-fopt-info-vec-missed` says why) |
| you can make the loop vectorizable (`__restrict`, countable, no early break, SoA) | you need a specific shuffle / byte permute (market-data decode, endian swap) |
| you want portability via `-march=` | mask-heavy logic where you control the blend |
| the win is "good enough" | you need the last 20–30% and know the µarch |
| — | horizontal patterns the compiler does poorly |

**Middle ground:** `#pragma omp simd` (portable vectorization hints),
`std::simd` (C++26, experimental in libstdc++ as `std::experimental::simd`), or
libraries — **Highway** (Google, portable, runtime-dispatched), **xsimd**,
**EVE**, **std::experimental::simd**. These give you SIMD without hand-writing
per-ISA intrinsics and without `#ifdef` soup.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `_mm256_load_ps` on unaligned data
Aligned-load intrinsic + a non-32-byte-aligned pointer = **crash** (`#GP`) or
silent corruption. Use `_mm256_loadu_ps` always unless you *own* the allocation
and `alignas(32)` it.

### Trap 2 — `_mm256_set_epi32(a,b,c,d,e,f,g,h)` lane order
The **first argument is the highest lane** (lane 7), the last is lane 0. Reversed
from how you'd write an array. Get this wrong and shuffles/gathers are silently
scrambled. Use `_mm256_setr_*` (the `r` = "reversed to natural order") if it
helps you.

### Trap 3 — `_MM_SHUFFLE(z,y,x,w)` is also "reversed"
`_MM_SHUFFLE(3,2,1,0)` = identity. The macro packs the indices MSB-first. Draw
the lane mapping.

### Trap 4 — forgetting `-mfma` for `fmadd`
`_mm256_fmadd_ps` without `-mfma` (or `target("fma")`) → compile error, or worse,
it's not the fused form. `-march=x86-64-v3` / `-mavx2 -mfma` includes it.

### Trap 5 — `movemask` on the wrong element size
`_mm256_movemask_ps` gives 8 bits (per float lane); `_mm256_movemask_epi8` gives
32 bits (per byte). Using the float one on integer-compare results loses
resolution.

### Trap 6 — hand-writing SIMD the compiler already does well
A plain `for (i) c[i] = a[i] + b[i];` with `__restrict` at `-O3 -march=v3` is
already optimal AVX2. Writing intrinsics for it adds bugs and maintenance for
zero gain. Profile first; intrinsics for what the compiler *won't* do.

### Trap 7 — no `vzeroupper` in hand-rolled AVX called from SSE code (file `10`)
Transition penalty. `-mavx` inserts it at function boundaries; hand-asm doesn't.

---

## > **HFT relevance**

> - **Auto-vectorize first** — SoA layout + `__restrict` + `-O3 -march=<your
>   floor>` gets most maps/reduces. Check `-fopt-info-vec-missed` for the ones
>   that don't.
> - **Intrinsics for market-data decode** — `pshufb` (`_mm_shuffle_epi8`) does
>   endian-swap + field-gather from a packed record in one instruction; SIMD
>   compare + `movemask` for delimiter/field scanning.
> - **Runtime dispatch** — `target("avx2")` / `target_clones` + a startup CPUID
>   assert (example `07`) so one binary runs on the deployment floor and the
>   newer boxes.
> - **Consider Highway / xsimd** for portable, dispatched SIMD without per-ISA
>   `#ifdef` — especially if you also target ARM64 (NEON/SVE).
> - **Keep SIMD bursts short on downclock-prone Intel parts** (file `13`),
>   measure end-to-end, and check the loop isn't memory-bound first (folder 32).

---

## Hands-on

```bash
# example 05: hand-written SSE + AVX2 (target attribute + CPUID guard)
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/05_simd_basics.cpp
./build.ps1 asm  31-CPU-ARCHITECTURE/examples/05_simd_basics.cpp | grep -E 'vaddps|addps|vmovups|movups'

# what auto-vec produces / misses
g++ -std=c++20 -O3 -march=x86-64-v3 -fopt-info-vec-optimized -fopt-info-vec-missed your.cpp -c -o nul

# Intel Intrinsics Guide: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`_mm_set_epi32(a,b,c,d)` = lanes 0,1,2,3" | reversed — `a` is the top lane; use `setr` for natural order |
| "aligned-load intrinsic is just faster" | crashes on unaligned; use `loadu` unless you `alignas` |
| "SIMD `if` per lane" | compute both + `blendv`/mask; no per-lane branch |
| "hand intrinsics always beat the compiler" | for simple maps the compiler is already optimal |
| "one `#include <immintrin.h>` and I have AVX-512" | need `-m` flags / target attr; and the CPU must support it |
| "`fmadd` works without `-mfma`" | compile error / not fused |

---

## Exercises

1. `_mm256_cmp_ps(a, b, _CMP_LT_OQ)` ka result kya hai (per lane), aur usse
   `a` ya `0.0` select kaise karte (branchless)?

   <details><summary>Answer</summary>

   Per lane: `0xFFFFFFFF` (all ones, which as a float is a NaN — treat it as a
   mask, not a value) where `a < b`, and `0x00000000` where not (`_OQ` =
   "ordered, quiet" — false if either is NaN). To select `a` where `a<b` else
   `0.0`: `_mm256_and_ps(mask, a)` — AND with all-ones keeps `a`, AND with zero
   gives `+0.0`. Or `_mm256_blendv_ps(_mm256_setzero_ps(), a, mask)` which reads
   cleaner. AVX-512: `__mmask8 m = _mm512_cmp_ps_mask(a, b, _CMP_LT_OQ);` then
   `_mm512_maskz_mov_ps(m, a)`.
   </details>

2. Ek fixed 8-byte market-data field big-endian aata hai, tumhe little-endian
   `uint64` chahiye, aur ek batch mein 2 aise fields (16 bytes). SIMD se kaise?

   <details><summary>Answer</summary>

   Load the 16 bytes: `__m128i raw = _mm_loadu_si128((const __m128i*)p);`. Build
   a byte-permute pattern that reverses each 8-byte group:
   `__m128i pat = _mm_setr_epi8(7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8);`. Then
   `__m128i le = _mm_shuffle_epi8(raw, pat);` — one `pshufb` reverses both
   fields' byte order simultaneously. Now `le` holds the two little-endian
   `uint64`s; extract with `_mm_extract_epi64(le, 0)` / `(le, 1)` or store and
   read. For a whole message of many such fields, load 16/32 bytes at a time and
   `pshufb` with the right pattern — this is how fast ITCH/OUCH decoders work.
   </details>

3. Auto-vectorization `-fopt-info-vec-missed` kehta: "not vectorized: control
   flow in loop" ek loop pe jisme `if (x[i] != 0) y[i] = 1.0/x[i]; else y[i] =
   0.0;`. Kaise vectorizable banao?

   <details><summary>Answer</summary>

   Rewrite the per-element `if` as branchless so the loop body is straight-line:
   `double safe = x[i] + (x[i] == 0.0);  // avoid div-by-zero: use 1 where x==0`
   `y[i] = (x[i] != 0.0) * (1.0 / safe);`
   Now every iteration does the same operations (a compare→0/1, an add, a
   reciprocal, a multiply) with no control flow → the vectorizer can do it 4/8
   lanes at a time, computing `1.0/safe` for all lanes and zeroing the ones
   where `x==0` via the multiply-by-mask. (`1.0/x` for the masked-off lanes is
   wasted work but the divide unit is the same cost either way, and there's no
   NaN because `safe` is never 0.) Alternatively `#pragma omp simd` with an
   `if`-free body, or intrinsics with `_mm256_blendv_ps`.
   </details>

4. `_mm256_i32gather_ps(base, idx, 4)` — kab yeh scalar loop se worth hai, kab
   nahi?

   <details><summary>Answer</summary>

   `vgatherdps` fetches 8 floats from `base + idx[lane]*4`. It decodes to many
   µops (one load per lane internally on most µarchs) and has high latency /
   low throughput — often **no faster than 8 scalar loads**, and it can't be
   reordered as freely. Worth it when: the indices are unavoidable (a genuine
   permutation / sparse lookup) *and* the surrounding computation is heavy enough
   that the gather isn't the bottleneck, *and* the target µarch has a decent
   gather (newer Intel/AMD). **Not** worth it when: you can restructure the data
   so the access is contiguous (SoA, sorting the work by index, an SoA "hot"
   copy) — that's almost always the better fix. Rule: treat gather as a code
   smell that says "your data layout is wrong for this access pattern".
   </details>

5. Ek team `#ifdef __AVX2__ ... #elif __SSE4_2__ ... #else ...` se teen code
   paths maintain kar rahi. Highway / xsimd jaisi library se kya badalta?

   <details><summary>Answer</summary>

   You write the kernel **once** against the library's portable vector type
   (`hn::Vec<D>`, `xs::batch<float>`), and the library: (a) instantiates it for
   each target ISA (SSE4, AVX2, AVX-512, NEON, SVE) from that single source, (b)
   does **runtime CPU dispatch** to pick the best one, (c) handles alignment,
   tails (masked), and cross-platform lane semantics. You lose a little
   control over exotic shuffles (you can drop to raw intrinsics for those), but
   you delete the `#ifdef` maze, get ARM64 support for free (relevant if you
   trial Graviton — file `15`), and the dispatch/CPUID logic is battle-tested.
   The trade-off: an extra dependency and slightly larger binaries (multiple
   ISA versions of the kernels).
   </details>

---

## Interview questions

1. Read `_mm256_fmadd_ps` — width, operation, lane type, what it computes.
2. The core intrinsic patterns: load/store, set1/setzero, arith, cmp→mask, blend, movemask, shuffle, horizontal reduce.
3. `_mm_set_epi32` / `_MM_SHUFFLE` lane ordering — the "reversed" gotcha.
4. Predication — how SIMD does a per-element conditional without branching.
5. Tail handling — the scalar epilogue vs AVX-512 masked load.
6. Auto-vec vs intrinsics vs a portable-SIMD library (Highway/xsimd) — when each.
7. `pshufb` / `_mm_shuffle_epi8` for market-data decode — what it does in one instruction.

---

## Next
→ [`12-hyperthreading.md`](12-hyperthreading.md)
