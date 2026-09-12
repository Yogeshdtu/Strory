# 09 — SIMD assembly: reading SSE / AVX instructions

## Prerequisites
- `31-CPU-ARCHITECTURE/10-simd-basics.md`, `11-simd-intrinsics.md`
- `33-COMPILER-OPTIMIZATION/05-vectorization.md`
- `08-recognizing-patterns.md`

## Yeh topic abhi kyun
"Mera loop vectorize hua?" ka jawab asm mein ek nazar mein milta hai:
`xmm`/`ymm`/`zmm` registers + **packed** ops (`ps`/`pd`/`d`/`q` suffix) =
haan; `xmm` + **scalar** ops (`ss`/`sd`) = nahi. Yeh lesson SIMD instructions
padhna sikhata — likhne ke liye folder 31 lesson 11.

---

## Registers

| | width | holds |
|---|---|---|
| `xmm0-15` | 128-bit | 4×`float` / 2×`double` / 16×`int8` / 4×`int32` ... ; also 1 scalar float/double in the low lane |
| `ymm0-15` | 256-bit | 8×`float` / 4×`double` / ... ; `xmm` is the low half |
| `zmm0-31` | 512-bit | 16×`float` / 8×`double` ; + `k0-k7` mask registers |

AVX instructions have a **`v` prefix** and are 3-operand (non-destructive):
`vaddps ymm0, ymm1, ymm2` = `ymm0 = ymm1 + ymm2` (ymm1 not clobbered). SSE is
2-operand: `addps xmm0, xmm1` = `xmm0 += xmm1`.

---

## The suffix tells you the element type & count

```
    ss  = Scalar Single   (1 float)        <- NOT vectorized
    sd  = Scalar Double   (1 double)       <- NOT vectorized
    ps  = Packed Single   (4 float in xmm, 8 in ymm)
    pd  = Packed Double   (2 in xmm, 4 in ymm)
    b/w/d/q (integer)     = packed  8/16/32/64-bit  (paddd = packed add dword)
    dqu/dqa              = double-quadword unaligned/aligned (a 16-byte int/byte load)
```
`addss` → 1 float. `addps` (xmm) → 4 floats. `vaddps ymm` → 8 floats.
`vpaddd ymm` → 8 int32. **This one glance answers "did it vectorize".**

---

## Common SIMD instructions

### Load / store
```asm
movss / movsd            xmm0, [mem]      ; 1 scalar float/double
movups / movupd          xmm0, [mem]      ; 4 floats / 2 doubles, UNALIGNED
movaps / movapd          xmm0, [mem]      ; ... ALIGNED (faults if not 16-aligned)
movdqu / movdqa          xmm0, [mem]      ; 16 bytes int, unaligned/aligned
vmovups / vmovdqu        ymm0, [mem]      ; 32 bytes
vmovntps / movntdq       [mem], ymm0      ; NON-TEMPORAL store (bypass cache -- folder 32/12)
vbroadcastss             ymm0, [mem]      ; load 1 float, splat to all 8 lanes
```
`movups` vs `movaps` — the compiler emits `movaps` only when it proved
16-byte alignment (or after a "peel" loop aligned the pointer). `vbroadcast`
= a scalar constant being splatted for a `y = a*x + b` style loop.

### Arithmetic
```asm
addps / mulps / subps / divps / sqrtps / maxps / minps
vfmadd132ps / vfmadd213ps / vfmadd231ps   ; a*b + c fused (needs FMA / -mfma)
vpaddd / vpmulld / vpsubd / vpand / vpor / vpxor / vpsrld / vpslld  ; integer packed
```
`vfmadd...ps` in the loop = FMA-vectorized (`-mfma` / `-march=x86-64-v3`).
The `132/213/231` variants just permute which operand is the accumulator.

### Compare → mask, blend, movemask
```asm
cmpps xmm0, xmm1, 1       ; per-lane compare -> lanes become all-1s or all-0s (a mask)
vcmpgtps ymm0, ymm1, ymm2 ; ymm0 = per-lane (ymm1 > ymm2) ? -1 : 0
blendvps xmm0, xmm1, xmm2 ; per-lane select using the sign bit of xmm2 (SIMD `cmov`)
pmovmskb / movmskps eax, xmm0  ; pack the lane sign bits into an integer bitmask
```
This trio = a **branchless per-lane `if`**: compare → mask → blend, or
compare → `movemask` → scalar `popcnt`/`tzcnt` on the result (find matching
lanes). Common in filtering / search loops.

### Shuffle / permute / horizontal reduce
```asm
shufps / pshufd / vpermd / vperm2f128 / vpermilps   ; rearrange lanes
vextractf128 xmm1, ymm0, 1     ; get the high 128 of a ymm
haddps / vhaddps               ; horizontal add adjacent pairs
; a reduction's TAIL looks like: vextractf128 + vaddps + vshufps + vaddps + ... + vmovss
```
A cluster of `vextractf128` / `vshufps` / `vaddps` right after a `ymm` loop =
the **horizontal reduction** of the 8 partial sums into 1 scalar.

### `vzeroupper`
```asm
        vzeroupper
        ret
```
Emitted after AVX code, before returning to possibly-SSE code — clears the
upper halves of `ymm` to avoid an SSE/AVX transition penalty. Ignore it;
it's correct hygiene.

---

## A full vectorized loop, annotated

```asm
sum(float const*, int):
        test   esi, esi
        jle    .L4                      ; n <= 0 -> return 0
        lea    eax, [rsi-1]
        cmp    esi, 7
        jbe    .L9                       ; n < 8 -> scalar path
        mov    edx, esi
        vpxor  xmm0, xmm0, xmm0          ; ymm0 = 0 (accumulator)
        shr    edx, 3
        ...
.L3:
        vaddps ymm0, ymm0, [rdi + rax]   ; ymm0 += 8 floats from memory
        add    rax, 32                   ; advance 32 bytes = 8 floats
        cmp    rcx, rax
        jne    .L3                        ; loop
        vextractf128 xmm1, ymm0, 0x1     ; ---- horizontal reduce ----
        vaddps xmm0, xmm0, xmm1          ;  add high 128 to low 128
        vmovhlps xmm1, xmm0, xmm0        ;  ...
        vaddps xmm0, xmm0, xmm1
        vshufps xmm1, xmm0, xmm0, 0x55
        vaddss xmm0, xmm0, xmm1          ; xmm0 low lane = sum of all 8
        vzeroupper
.L9:    ; scalar tail: add the remaining n % 8 elements one at a time
        ...
```
Read: guard → small-n bailout → `vaddps ymm` main loop (8/iter) → horizontal
reduce → `vzeroupper` → scalar tail. **8-wide** because `ymm` + `add rax, 32`.

---

## Not-vectorized (for contrast)

```asm
.L3:
        vaddss xmm0, xmm0, [rdi + rax*4]   ; SCALAR add, one float
        add    rax, 1                       ; advance 1 element
        cmp    esi, eax
        jg     .L3
```
`ss` + `add rax, 1` = **scalar**. If you expected vectorization, run
`-fopt-info-vec-missed` (folder 33 lesson 05) — aliasing, a float reduction
without `-ffast-math`, a call in the body, or a loop-carried dependency.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `xmm` present ⇒ "vectorized"
`xmm` also holds a single scalar `float`/`double`. Check the **op suffix**:
`ss`/`sd` = scalar; `ps`/`pd`/`d`/`q` = packed.

### Trap 2 — "16-byte vectors" is AVX
`xmm` / `movups` / `addps` = **SSE** (128-bit, 4 floats). AVX = `ymm` /
`vaddps ymm` (256-bit, 8 floats). Need `-march`/`-mavx2` for `ymm`
(folder 33 lesson 12).

### Trap 3 — mistaking the horizontal reduce for real work
The `vextractf128`/`vshufps`/`vaddps` cluster after the loop is fixed
overhead (once), not per-element. For a tiny `n` it can dominate — hence the
"small n → scalar" bailout.

### Trap 4 — `vzeroupper` looks like a bug
It's mandatory hygiene between AVX and SSE code. Ignore it.

### Trap 5 — `movaps` vs `movups`
`movaps` (aligned) faulting = the data wasn't 16-aligned. The compiler adds a
peel loop or uses `movups` when it can't prove alignment. `alignas(32)` on
hot arrays lets it use aligned loads (folder 32 lesson 02).

### Trap 6 — gather/scatter looks vectorized but is slow
`vgatherdps` / `vpgatherdd` = SIMD gather (`a[idx[i]]`). It *is* a vector
instruction but internally does N separate loads → ~lane-count cycles. Not
the win a contiguous `vmovups` is (folder 33 lesson 05, SoA).

---

## > **HFT relevance**

> - **One-glance "did it vectorize"**: `ymm` + `vfmadd...ps`/`vaddps` + `add
>   ptr, 32` = yes, 8-wide. `xmm` + `mulss`/`addss` = no → go find out why.
> - **Confirm the width matches `-march`** — `xmm` where you compiled for
>   `x86-64-v3` means the vectorizer bailed or the cost model chose SSE; a
>   `zmm` where you didn't want AVX-512 means downclock risk (folder 31/13,
>   33/12).
> - **`vmovntps` (NT store)** — confirm your write-only bulk output actually
>   got non-temporal stores (folder 32 lesson 12).
> - **compare→mask→blend** cluster = a branchless per-lane filter — good for
>   an unpredictable per-element predicate (folder 31 lesson 08).
> - **`vgather`** in a hot loop = a gather; if the indices have structure,
>   restructure to a contiguous SoA scan instead (folder 32/33).

---

## Hands-on

```bash
./build.ps1 asm 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp | grep -iE "xmm|ymm|mulps|mulss|addps|addss|vfmadd|vzero"
#   map_vec loop: mulps/addps (SSE) ; map_scalar: mulss/addss

# force AVX2 -> ymm:
g++ -std=c++20 -O2 -march=native -S -masm=intel 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o - \
 | grep -iE "ymm|vfmadd|add rax, 32"

# what vectorized, at what width:
g++ -O2 -march=native -fopt-info-vec 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o /dev/null
#   "loop vectorized using 32 byte vectors"
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`xmm` = vectorized" | check the suffix: `ss`/`sd` scalar, `ps`/`pd`/`d` packed |
| "`movups`/`addps` = AVX" | that's SSE (128-bit, 4 floats); AVX = `ymm` |
| "the reduce cluster is per-element work" | fixed once-per-loop overhead |
| "`vzeroupper` is a mistake" | required SSE/AVX transition hygiene |
| "`movaps` fault = compiler bug" | data wasn't 16-aligned; use `alignas` or `movups` |
| "`vgather` is a fast vectorized load" | internally N loads, ~lane-count cycles |

---

## Exercises

1. Loop body: `vmulps ymm1, ymm2, [rdi + rax]` / `vaddps ymm0, ymm0, ymm1` /
   `add rax, 32`. Element type, width, aur ye kya compute kar raha?

   <details><summary>Answer</summary>

   `ps` = packed single (`float`), `ymm` = 256-bit → **8 floats per
   iteration**. `add rax, 32` = 32 bytes = 8×4 confirms it. `ymm2` is a
   loop-invariant (a splatted scalar — probably from `vbroadcastss`). So per
   iteration: load 8 floats from `[rdi + rax]`, multiply by the constant in
   `ymm2`, add into the accumulator `ymm0`. Source: `for (i) acc += k *
   a[i];` — a scaled sum / part of a `y = k*x + ...` — **vectorized 8-wide
   with AVX** (so compiled with `-march=x86-64-v3`+ or `-mavx`). No `vfmadd`
   → FMA not enabled (or the cost model split it); `-mfma` would fuse the
   `vmulps`+`vaddps` into one `vfmadd231ps`.
   </details>

2. Asm mein ek `ymm` loop hai, phir turant: `vcmpgtps ymm1, ymm0, ymm2` /
   `vmovmskps eax, ymm1` / `tzcnt eax, eax`. Kya ho raha?

   <details><summary>Answer</summary>

   A **SIMD search / find-first**. `vcmpgtps ymm1, ymm0, ymm2` — per-lane
   compare: each of the 8 lanes of `ymm1` becomes all-1s if `ymm0[lane] >
   ymm2[lane]` (e.g. `value > threshold`), else all-0s. `vmovmskps eax,
   ymm1` — pack the 8 lane sign bits into an 8-bit mask in `eax`. `tzcnt
   eax, eax` — count trailing zeros = the **index of the first set bit** =
   the first lane that matched. So this loop is `for (i) if (a[i] >
   threshold) return i;` vectorized: test 8 elements at once, and if any
   matched (`eax != 0`), `tzcnt` gives the offset of the first, add the
   block base → the answer. Branchless per-block; one scalar branch per
   8 elements to check "any match".
   </details>

3. Tumne `-march=native` diya, `-fopt-info-vec` "loop vectorized using 64
   byte vectors" bolta hai, aur asm mein `zmm` registers hain. Concern?

   <details><summary>Answer</summary>

   "64 byte vectors" = **AVX-512** (`zmm`, 16 floats). Concerns (folder 31
   lesson 13, folder 33 lesson 12): (1) **Frequency downclock** — running
   512-bit SIMD forces the core (and sometimes package) to a lower clock,
   slowing *everything else* on that core; a 2× win in this loop can be a
   net loss for the process. (2) **Availability** — if any deployment box
   isn't AVX-512 (many Zen 2/3, some Intel), the binary `SIGILL`s. (3)
   `vzeroupper`/transition and mask-register overhead. Actions: benchmark
   **end-to-end throughput + P99**, not the loop; try
   `-mprefer-vector-width=256` (keep AVX-512 features like masks/VBMI, cap
   the width → no downclock); or pin `-march=x86-64-v3` for that TU. Only
   keep `zmm` if the whole-system measurement is better.
   </details>

---

## Interview questions

1. `xmm` vs `ymm` vs `zmm`, and `ss`/`sd` vs `ps`/`pd`.
2. How to tell vectorization width from the asm (register + `add ptr, N`).
3. `vfmadd...ps` — what it is, what flag enables it.
4. The compare→mask→blend / movemask pattern — what construct.
5. The horizontal-reduce cluster after a loop — what it does, why once.
6. `vzeroupper` — why it's there.
7. `movaps` vs `movups`, and `vgather` — the gotchas.

---

## Next
→ [`10-inline-assembly.md`](10-inline-assembly.md)
