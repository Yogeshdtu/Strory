# 10 — SIMD basics: SSE, AVX, AVX-512, vector registers

## Prerequisites
- `05-superscalar.md` (ILP), `09-instruction-latency-throughput.md`
- `02-registers.md` (xmm/ymm/zmm)

## Yeh topic abhi kyun
SIMD = **Single Instruction, Multiple Data**: ek `vaddps` 8 floats ek saath
jodta. Yeh "hardware ILP" hai — ek instruction, N independent lanes. HFT mein
market-data decode, price transforms, risk calcs, aur string/byte processing
sab SIMD se kai-guna tez hote (example `05`: SSE ~4×, AVX2 ~9.5× scalar se). Yeh
lesson concept + register model; file `11` actual intrinsics.

---

## The idea

Scalar: `c[i] = a[i] + b[i]` — one add per instruction, N instructions.
SIMD: load 8 `a`s into a register, 8 `b`s into another, **one `vaddps`** →
8 sums. N/8 instructions.

```
   xmm/ymm/zmm register = a "vector" of lanes:

   ymm0:  [ a0 | a1 | a2 | a3 | a4 | a5 | a6 | a7 ]   (8 x float32)
   ymm1:  [ b0 | b1 | b2 | b3 | b4 | b5 | b6 | b7 ]
   vaddps ymm2, ymm0, ymm1
   ymm2:  [a0+b0|a1+b1|a2+b2|a3+b3|a4+b4|a5+b5|a6+b6|a7+b7]   -- one instruction
```

Lanes are **independent** — no cross-lane dependency, so a `vaddps` has the same
~4-cycle latency as a scalar `addss` but does 8× the work. That's the win.

---

## The instruction-set ladder

| ISA | Reg width | f32 / f64 / i32 / i8 lanes | Year | Notes |
|---|---|---|---|---|
| **SSE / SSE2** | 128-bit `xmm` | 4 / 2 / 4 / 16 | 2001 | **baseline for x86-64** — always available |
| SSE3 / SSSE3 / SSE4.1 / SSE4.2 | 128-bit | + `pshufb`, `dpps`, `pcmpistri`, `crc32`, `popcnt` | 2004–08 | string/DSP helpers |
| **AVX** | 256-bit `ymm` | 8 / 4 / — / — | 2011 | float only for 256b; 3-operand (non-destructive) VEX encoding |
| **AVX2 + FMA** | 256-bit `ymm` | 8 / 4 / 8 / 32 | 2013 | integer 256b, `vfmadd`, gather; **the common HFT baseline (`x86-64-v3`)** |
| **AVX-512** (F/BW/VL/DQ/...) | 512-bit `zmm` | 16 / 8 / 16 / 64 | 2016 | `k` mask registers, per-lane predication, compress/expand; **fragmented**, can downclock (file `13`) |

**This box** (example `07`): AVX2 + FMA yes, **AVX-512 no** (AMD Zen 2). Intel
Ice Lake / Sapphire Rapids and AMD Zen 4+ have AVX-512.

---

## What SIMD is good at

| Pattern | SIMD fit |
|---|---|
| **Map**: `out[i] = f(in[i])` element-wise | excellent — pure lane parallelism |
| **Reduce**: `sum += in[i]` | good — N partial sums in the lanes, horizontal add at the end (example `05`, `02`) |
| **Filter / compare**: `mask = in[i] > thr` | good — `vcmpps` → mask, then `vblendvps` / AVX-512 compress |
| **Fixed-size struct decode** (parse N market-data records) | good — load, shuffle fields into lanes, `pshufb` byte-swap |
| **String search / memcmp / tolower** | good — `pcmpeqb`, `pmovmskb` |
| **Matrix / dot products / filters** | excellent — `vfmadd` |

## What SIMD is bad at

- **Data-dependent branching per element** — you can't branch per lane; you
  compute *both* sides and `blend` with a mask (predication). Wasteful if one
  side is expensive.
- **Gather/scatter** (`out[i] = table[idx[i]]`) — `vgatherdps` exists but is
  many µops and often no faster than scalar. Restructure to contiguous (SoA).
- **Cross-lane dependencies** — a running scan/prefix, a recurrence. Possible
  (log-depth shuffles) but the compiler won't, and it's fiddly.
- **Short arrays** — the setup + horizontal-reduce overhead eats the win for
  N < ~16–32.
- **AoS layout** — fields you want in a lane are strided; you spend instructions
  shuffling. **SoA** (folder 32) makes the field contiguous → one load fills a
  lane vector.

---

## Alignment and loads

- **Aligned load/store** (`_mm256_load_ps`, address 32-byte aligned) vs
  **unaligned** (`_mm256_loadu_ps`, any address). On modern CPUs the *unaligned*
  form is ~free **when the data happens to be aligned**; a real misaligned access
  costs a bit, and one that **straddles a cache line** costs more. Prefer
  aligned data (`alignas(32)`, `std::aligned_alloc`), use the `u` intrinsics
  anyway (safe), let the hardware fast-path it.
- **AVX-512**: `zmm` loads want 64-byte alignment; a straddle is costlier.
- Tail handling: `N` not a multiple of the vector width → a scalar loop for the
  last `N % width` elements (example `05`/`06`), or a masked load (AVX-512).

---

## Auto-vectorization vs intrinsics (file `11`, `06`)

| | Auto-vectorization | Manual intrinsics |
|---|---|---|
| Effort | write plain loops well | write `_mm256_*` by hand |
| Portability | recompile with `-march=` | `#ifdef` per ISA, or function multiversioning |
| Ceiling | good for simple maps/reduces; the compiler is conservative | full control — shuffles, masks, FMA scheduling |
| When | default; `__restrict`, countable loops, `-O3 -march=x86-64-v3` | when the compiler won't, or you need a specific shuffle/pattern |

**Example `06` measured:** the *same* map loop, auto-vectorized vs
`no-tree-vectorize`, is **~2.5–3× faster** on this box (SSE2 baseline, 4 ints/
instr). With `-march=x86-64-v3` it would use AVX2 (8/instr) for more.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — AoS layout killing vectorization
`struct Tick { double px; double qty; int64 ts; };` array — to vectorize
`px * qty` over N ticks you must gather strided `px`s into a lane vector.
**SoA** (`double px[N]; double qty[N];`) → one contiguous load per lane vector
(folder 32).

### Trap 2 — per-element branches
`for (i) if (a[i] > 0) out[i] = f(a[i]); else out[i] = g(a[i]);` — SIMD computes
both `f` and `g` for all lanes and blends. If `f`/`g` are cheap, fine; if one is
a `div` or a `sqrt`, you're paying it for every lane regardless. Sometimes
scalar-with-good-prediction wins.

### Trap 3 — `gather` cargo-culting
`_mm256_i32gather_ps` looks like it should be fast; it's often ~8 separate loads
in µops with no win over a scalar loop, and it can't be reordered as freely.
Restructure the data so the access is contiguous.

### Trap 4 — SSE/AVX transition penalty
Mixing 128-bit legacy SSE instructions with 256-bit AVX without `vzeroupper` →
a ~tens-of-cycles state-save penalty on some µarchs. Use the VEX-encoded forms
throughout (`-mavx` does this), and `_mm256_zeroupper()` when handing off to
SSE-only code.

### Trap 5 — AVX-512 frequency downclocking
On some Intel Skylake-X / Cascade Lake parts, running "heavy" AVX-512 (or even
256-bit AVX2 FMA) drops the core frequency for a while (file `13`). A SIMD loop
that's 4× faster but at 0.8× frequency might not be worth it — measure. Newer
parts (Ice Lake+, Sapphire Rapids, Zen 4) largely fixed this.

### Trap 6 — assuming 8× for AVX2 / 16× for AVX-512
Only if compute-bound and the data's in cache. Memory-bandwidth-bound loops
(streaming a big array once) hit the bandwidth ceiling and SIMD gives ~1×.
Example `05` fits in L2 → compute-bound → ~4×/~9.5×.

---

## > **HFT relevance**

> - **SoA layout for anything you'll vectorize** — parallel arrays per field so
>   one load fills a lane vector (folder 32). This is often the prerequisite,
>   not SIMD itself.
> - **Vectorize the maps and reduces:** price transforms, risk aggregations,
>   checksum/CRC (parallel streams, file `09` ex 4), fixed-record market-data
>   decode (`pshufb` for endian swap + field extract), string/symbol matching.
> - **Baseline `-march=x86-64-v3`** (AVX2 + FMA) so the auto-vectorizer uses
>   256-bit + FMA everywhere; manual intrinsics (file `11`) for the shuffles the
>   compiler won't do; CPUID-dispatch (example `07`) if you also target older
>   floors.
> - **Be wary of AVX-512** on Intel parts prone to downclocking (file `13`) —
>   measure end-to-end wall time, not just the loop.
> - **Measure the ceiling:** if the loop is memory-bound (folder 32), SIMD won't
>   help — fix the data movement first.

---

## Hands-on

```bash
# example 05: scalar vs SSE (4-wide) vs AVX2 (8-wide) float sum -- measured 1x/4x/9.5x
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/05_simd_basics.cpp

# example 06: same code auto-vectorized vs not -- ~2.5-3x
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/06_autovectorization.cpp

# this box's SIMD support
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/07_cpu_info.cpp

# what -march gives the vectorizer
g++ -O3 -march=x86-64-v3 -fopt-info-vec-optimized your.cpp -c -o nul
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "SIMD gives exactly 8×/16×" | only compute-bound + in-cache; memory-bound → ~1× |
| "AoS array is fine, SIMD will figure it out" | strided fields → shuffle overhead; use SoA |
| "gather is fast, it's one instruction" | ~8 µop loads; often no faster than scalar |
| "per-element `if` is fine in SIMD" | compute both sides + blend; costly if a side is expensive |
| "AVX-512 always wins over AVX2" | fragmentation + downclock risk (file 13); measure |
| "unaligned loads are slow, always align" | `loadu` is fast on aligned data; only straddles cost |

---

## Exercises

1. `struct Q { double bid, ask; }; Q book[10000];` — tumhe `spread[i] = ask - bid`
   vectorize karna hai. AoS layout ke saath kya problem, SoA se kya badalta?

   <details><summary>Answer</summary>

   AoS: `bid` and `ask` alternate in memory (`bid0 ask0 bid1 ask1 ...`). To fill
   a lane vector with 4 `bid`s you must gather elements at stride 16 bytes — the
   compiler emits shuffles / a strided load / a gather, eating the benefit. SoA
   (`double bid[N]; double ask[N];`): `bid` is contiguous, so one
   `_mm256_loadu_pd(&bid[i])` fills 4 lanes, one for `ask[i]`, one `vsubpd`,
   one store → clean 4×. The transform is: store market data column-wise (per
   field) not row-wise (per record) for the fields you compute on.
   </details>

2. Example `05` pe AVX2 speedup 9.5× hai, SSE 4×. AVX2 exactly 2× SSE kyun (aur
   scalar se ~8× nahi, ~9.5×)?

   <details><summary>Answer</summary>

   AVX2 lane width is 2× SSE (8 f32 vs 4 f32), so ~2× the SSE rate — matches. The
   >8× vs scalar comes from two effects stacking: (1) **width** — 8 elements per
   instruction; (2) **ILP within the reduction** — the scalar `s += p[i]` is a
   single ~4-cycle-latency FP-add dependency chain (~4 cyc/element), while the
   SIMD version's accumulator is a *vector* of 8 partial sums, so consecutive
   `vaddps`es to it are less latency-bound and the FP-add port (2/cycle) fills
   up. Width × better-ILP ≈ a bit more than the raw lane count. (The horizontal
   add at the end is one-time.)
   </details>

3. Ek loop `for (i) out[i] = (in[i] > 0) ? sqrt(in[i]) : 0.0;` — SIMD karo to
   kya hota, aur kab yeh worth nahi?

   <details><summary>Answer</summary>

   SIMD can't branch per lane, so it computes `sqrt` for **all** lanes (including
   the `in[i] <= 0` ones, where `sqrt` of a negative gives NaN — you'd compute
   `sqrt(max(in[i], 0))` or `sqrt(abs)` and mask), builds a mask `in[i] > 0`,
   and `blend`s `sqrt` vs `0.0`. `sqrtps` is ~12-20 cyc, not fully pipelined. If
   most elements are `<= 0`, you're paying the expensive `sqrt` for lanes you
   throw away — a scalar loop with a well-predicted branch (skipping `sqrt` for
   the common `<= 0` case) can win. Worth SIMD-ing when: most elements take the
   expensive path anyway, or the "expensive" side is cheap (a few FMAs), or you
   can use `vrsqrtps` + Newton (~cheap, pipelined) instead of true `sqrtps`.
   </details>

4. `vzeroupper` kya karta aur ise kab emit karna chahiye?

   <details><summary>Answer</summary>

   `vzeroupper` zeroes the upper 128 bits of all `ymm`/`zmm` registers. Purpose:
   after your AVX (256/512-bit) code, if you call into code that uses **legacy
   128-bit SSE encodings** (a lot of old libraries, or `-mno-avx` object files),
   the CPU sees "dirty upper state" and inserts a costly state-save/merge
   (~tens of cycles) on every SSE instruction until it's cleared. `vzeroupper`
   clears it cheaply. When to emit: at the end of an AVX routine before returning
   to potentially-SSE callers, and before calling external functions. Compilers
   with `-mavx`/`-mavx2` insert it automatically at function boundaries; you only
   hand-emit it (`_mm256_zeroupper()`) in hand-written asm-ish code or tight
   interop.
   </details>

5. Tumhare backtester ka ek hot loop `-march=x86-64-v3` pe AVX2 se 4× tez hua,
   par ek Intel Skylake-X production box pe end-to-end sirf 1.5× improvement
   mila. Sabse likely wajah?

   <details><summary>Answer</summary>

   **AVX frequency downclocking.** Skylake-X (and Cascade Lake) drop the core
   clock when running "heavy" 256-bit AVX2/FMA or any AVX-512 — sometimes by
   15-40%, and for a few hundred microseconds *after* the SIMD code stops, so
   *neighbouring* scalar code also runs slower. A loop that's 4× faster in
   isolation, run at 0.7× frequency and dragging the surrounding code down,
   nets far less end-to-end. Mitigations: use narrower SIMD (128-bit AVX) which
   downclocks less, keep SIMD bursts short, or measure whether the loop is even
   worth vectorizing on that µarch. Newer Intel (Ice Lake+, Sapphire Rapids) and
   AMD Zen 4 largely removed the penalty (file `13`, `15`) — but you deploy to
   the box you have.
   </details>

---

## Interview questions

1. SIMD — the idea, why lanes being independent means same latency, more work.
2. The ISA ladder: SSE2 (baseline) → AVX → AVX2+FMA (`x86-64-v3`) → AVX-512, widths and years.
3. What SIMD is good at (map/reduce/filter) and bad at (per-element branch, gather, cross-lane, short arrays).
4. AoS vs SoA — why layout is the prerequisite for vectorization.
5. Auto-vectorization vs intrinsics — when each; what the compiler needs (`__restrict`, countable loop).
6. Alignment — aligned vs `loadu`, the cache-line-straddle cost, tail handling.
7. AVX-512 downclocking — what it is, why it can make a 4× loop a net loss.

---

## Next
→ [`11-simd-intrinsics.md`](11-simd-intrinsics.md)
