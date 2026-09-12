# 09 — Aliasing: how it blocks optimization, `__restrict`, strict aliasing

## Prerequisites
- `12-POINTERS/`, `25-OBJECT-MODEL/06-strict-aliasing.md`
- `04-loop-optimizations.md`, `05-vectorization.md`
- `04-aliasing_restrict.cpp` example

## Yeh topic abhi kyun
**Aliasing** — do pointers/references same memory ko refer kar sakte hain —
compiler ki #1 enemy hai. Agar `a[i] = ...` ke baad compiler nahi jaan sakta
ki kya `b[j]` badla, to woh `b[j]` ko reload karta, hoist nahi karta,
vectorize nahi karta. `__restrict` aur type-based aliasing rules (TBAA) is
uncertainty ko kaam karte. Yeh HFT numeric kernels ke liye seedha 2-4x hai
(example `04`).

---

## The problem

```cpp
void f(int* a, int* b, int n) {
    for (int i = 0; i < n; ++i)
        a[i] = b[0] + i;       // b[0] loop-invariant? compiler CAN'T assume so
}
```
Agar caller `f(x, x + 5, 10)` kare, to `a[5] = ...` `b[0]` (= `x[5]`) ko
badal dega. So the compiler must **reload `b[0]` every iteration** — it can't
hoist it to a register. No LICM, and likely no vectorization (the value
changes mid-loop).

Real cost (example `04`, `out[i] = in[i]*(*scale) + (*offset)`):
```
  may-alias (plain ptr)  : 0.43 ns/elem
  __restrict             : 0.12 ns/elem      -> ~3.5x
```
Without `__restrict`: `*scale` and `*offset` reloaded every iteration + loop
stays scalar. With: both hoisted to registers once, loop vectorizes.

---

## Two kinds of aliasing analysis

### 1. Type-based alias analysis (TBAA / strict aliasing)
C++ rule: an lvalue access through a type that is **not compatible** with the
object's real type is UB (with exceptions: `char`/`unsigned char`/`std::byte`
can alias anything; signed/unsigned variants; a class and its first member;
etc.). `-fstrict-aliasing` (ON at `-O2`+) lets the compiler assume:
```cpp
void g(float* f, int* i) {
    *f = 1.0f;
    *i = 2;         // int and float are incompatible types ->
    return *f;      // compiler assumes *i did NOT change *f -> returns 1.0f (may skip reload)
}
```
Different scalar types → the compiler assumes no aliasing → optimizes freely.
**Same type** (`int*` and `int*`) → TBAA gives nothing; the compiler must be
conservative unless `__restrict` or provable non-overlap.

⚠️ Breaking strict aliasing = UB that bites at `-O2`:
```cpp
float x = 1.0f;
int bits = *(int*)&x;          // UB -- type-pun via pointer cast
// use std::bit_cast<int>(x) (C++20) or memcpy -- both are well-defined and free at -O2
```
Folder 25 lesson 06 has the full treatment. `-fno-strict-aliasing` disables
the assumption (Linux kernel builds with it) — safer but leaves optimization
on the table.

### 2. Pointer-based alias analysis (points-to)
The compiler tracks where pointers *could* point (allocation sites,
parameters, escapes). Within one function with everything inlined it's often
precise ("these two came from different `new`s"). Across a `noinline` /
cross-TU boundary it's pessimistic ("two `int*` params — assume they might
overlap"). This is where `__restrict` and `-flto`/inlining help.

---

## `__restrict` (`__restrict__` / MSVC `__restrict`)

```cpp
void axpy(float* __restrict y, const float* __restrict x, float a, int n) {
    for (int i = 0; i < n; ++i) y[i] += a * x[i];   // y, x provably distinct -> vectorize
}
```
Meaning: "for the lifetime of this pointer in this scope, the object it
points to is **only** accessed (for writing) through this pointer (or
pointers derived from it)." A **promise you make**; the compiler trusts it,
no check.

- C has `restrict` as a keyword (C99). C++ has **no standard** equivalent;
  `__restrict` / `__restrict__` is a GCC/Clang/MSVC extension (universally
  supported).
- Put it on the **parameters** of hot leaf functions (kernels).
- Wrong promise (the pointers *do* overlap) → **UB**, silent wrong results.
- Doesn't help a **real** loop-carried dependency (`x[i] = x[i-1] + ...`) —
  that's a true data dependency, not an aliasing doubt (example `04` blur3:
  `__restrict` present, still 2.3 ns/elem, can't parallelize).

### `restrict` on `this` / members
```cpp
struct Buf {
    float* __restrict data;    // GCC: restrict-qualified member
    void scale(float k) { for (...) data[i] *= k; }
};
```
Or `void scale(float k) __restrict;` (member function `this` is restrict).
Niche; usually restrict the params.

---

## Alternatives to `__restrict`

| Approach | Effect | Cost |
|---|---|---|
| **inline the kernel** (header / `-flto`) | compiler sees the call site — `f(vec_a.data(), vec_b.data())` — proves distinct allocations | recompile cost, `-flto` link time |
| **loop versioning** (`-O2` does this automatically for some loops) | compiler emits a runtime overlap check + fast/slow versions | the check + code size; not always applied |
| **`#pragma GCC ivdep` / `#pragma clang loop vectorize(assume_safety)`** | "no dependencies in this loop" — unchecked | UB if wrong; per-loop |
| **`-fno-strict-aliasing`** | disables TBAA (safer for type-punning code) | *removes* an optimization — opposite direction |
| **local copies** | `float s = *scale;` before the loop, use `s` | manual LICM; always works, always safe |

The "local copy" is the safe manual fix: pull the possibly-aliased scalar
into a local before the loop. The compiler then knows the local doesn't
alias the array.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — type-punning via pointer cast
`*(int*)&myfloat` — UB under strict aliasing, breaks at `-O2` (folder 25).
`std::bit_cast` / `memcpy` — free, correct.

### Trap 2 — `__restrict` promise that's false
`f(buf, buf, n)` where `f` marked both params `__restrict` → UB. Only when
you *know* they're distinct (and document the precondition).

### Trap 3 — expecting `__restrict` to fix a real dependency
`out[i] = out[i-1] * k` — `__restrict` can't help; each iteration genuinely
needs the last. Algorithm change (scan) or accept scalar.

### Trap 4 — aliasing between an array and a struct member / a global
`for (i) a[i] = g_scale * b[i];` — `g_scale` is a global; if `a` is `double*`
and `g_scale` is `double`, TBAA doesn't disambiguate (same type). Local copy,
or `__restrict` on `a` (doesn't cover the global — local copy is safer).

### Trap 5 — `char*` aliasing everything
A `char*`/`std::byte*` write can alias any object → a loop that writes
through `char*` and reads `int*` won't optimize. Do byte manipulation in a
separate pass, or `memcpy` in/out of a typed local.

### Trap 6 — `-fno-strict-aliasing` "to be safe" everywhere
It disables a real optimization class. Fix the type-punning (use `bit_cast`);
keep strict aliasing on.

---

## > **HFT relevance**

> - **`__restrict` on every hot numeric kernel's pointer params** — book
>   aggregation, greeks, signal computation, parse buffers. It's the single
>   cheapest 2-4x on a vectorizable loop (example `04`).
> - **Or inline + `-flto`** so the compiler proves non-aliasing from the call
>   site — do both.
> - **`std::bit_cast` for wire parsing** — reading a `uint32_t` out of a byte
>   buffer, punning a `double` to bits — never `*(T*)ptr`. It's UB-free and
>   compiles to the same `mov` (folder 25, folder 38).
> - **Local-copy the loop-invariant scalars** — `const double tick = *p_tick;`
>   before the hot loop — safe manual LICM that survives any aliasing.
> - **Keep strict aliasing on** (default `-O2`), fix punning properly.
> - **Verify** — `-fopt-info-vec-missed` will literally say "possible
>   aliasing between 'x' and 'y'" when it's the blocker.

---

## Hands-on

```bash
./build.ps1 fast 33-COMPILER-OPTIMIZATION/examples/04_aliasing_restrict.cpp
#   may-alias ~0.43 vs __restrict ~0.12 ns/elem  (~3.5x)

g++ -std=c++20 -O2 -fopt-info-vec-missed 33-COMPILER-OPTIMIZATION/examples/04_aliasing_restrict.cpp -o x
#   affine_alias: "not vectorized: possible aliasing ..."   affine_restrict: "vectorized"

./build.ps1 asm 33-COMPILER-OPTIMIZATION/examples/04_aliasing_restrict.cpp | grep -iE "mulss|mulps|movss"
#   alias path: mulss (scalar, reloads);  restrict path: mulps (packed)

# strict-aliasing UB demo (folder 25):
./build.ps1 fast 25-OBJECT-MODEL/examples/06_strict_aliasing.cpp   # delta real at -O0, 0 at -O2
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "aliasing is a rare edge case" | any 2+ same-type pointer params → the compiler assumes overlap |
| "`__restrict` is a hint" | it's an unchecked promise; wrong → UB |
| "`__restrict` fixes any slow loop" | only aliasing doubt, not real dependencies |
| "`*(int*)&f` to read float bits" | UB under strict aliasing; use `bit_cast`/`memcpy` |
| "different types can still alias, be safe" | TBAA: incompatible types → assumed no alias (that's the point) |
| "`-fno-strict-aliasing` everywhere" | disables a real optimization; fix punning instead |

---

## Exercises

1. `void norm(double* v, const double* len, int n) { for (int i=0;i<n;++i)
   v[i] /= *len; }` — plain `-O2`, kya hota? 3 fixes.

   <details><summary>Answer</summary>

   `*len` is `double`, `v` is `double*` — same type → TBAA can't rule out
   `v` aliasing `len` → `*len` is **reloaded every iteration** (a divide by a
   value that "might change"), and the loop stays scalar (`divsd` per elem,
   ~13-20 cyc). Fixes: (1) **local copy** — `const double inv = 1.0 / *len;
   for (i) v[i] *= inv;` — safe, always works, and turns divide into multiply
   (folder 31 lesson 09). (2) **`__restrict`** — `double* __restrict v, const
   double* __restrict len` — promises `v` doesn't alias `len` → `*len`
   hoisted, loop vectorizes. (3) **inline + `-flto`** so the caller's
   `v.data()` vs `&length` non-aliasing is visible. Best: (1) — it also kills
   the divide.
   </details>

2. `struct Vec3 { float x, y, z; }; void addv(Vec3* a, const Vec3* b, int n)
   { for (int i=0;i<n;++i) { a[i].x += b[i].x; a[i].y += b[i].y; a[i].z +=
   b[i].z; } }` — vectorize hoga? `__restrict` se?

   <details><summary>Answer</summary>

   Without `__restrict`: `a` and `b` are both `Vec3*` (same type) → assumed
   possibly overlapping → conservative. Even with `__restrict`, this is AoS —
   `a[i].x`, `a[i+1].x` are 12 bytes apart (stride `sizeof(Vec3)`) → the
   vectorizer would need a strided/gather load, which it usually declines →
   likely stays scalar or does narrow SLP (3-wide within one Vec3). The real
   fix is **SoA** (folder 32 lesson 09): `float* ax, *ay, *az; ...` with
   `__restrict` → each `ax[i] += bx[i]` is a dense contiguous loop that
   vectorizes cleanly. `__restrict` on the AoS version removes the aliasing
   doubt but not the layout problem.
   </details>

3. Ek parser: `uint32_t read_u32(const uint8_t* p) { return *(const uint32_t*)p; }`
   — kya galat, kya risk `-O2` pe, sahi kaise?

   <details><summary>Answer</summary>

   Two problems: (1) **Strict-aliasing UB** — accessing bytes as a
   `uint32_t` lvalue when the underlying object isn't a `uint32_t`. At `-O2`
   the compiler may reorder/elide surrounding accesses assuming this
   `uint32_t` read doesn't alias the `uint8_t` writes, producing wrong
   results in a larger function. (2) **Alignment UB** — `p` may not be
   4-aligned; `*(uint32_t*)p` on some targets faults, on x86 is slow/split.
   Correct: `uint32_t v; std::memcpy(&v, p, 4); return v;` — well-defined,
   handles misalignment, and `-O2` compiles it to a single `mov` (or `movbe`
   if you also byteswap). Or `std::bit_cast` if you have a properly-typed
   4-byte source. This is the standard HFT wire-parse idiom (folder 38).
   </details>

---

## Interview questions

1. Aliasing — why it's the compiler's main obstacle (LICM, CSE, vectorize).
2. TBAA / strict aliasing — the rule, `char*` exception, `-O2` default.
3. `__restrict` — meaning, that it's an unchecked promise, where to put it.
4. `__restrict` vs a real loop-carried dependency.
5. 3 alternatives to `__restrict` (inline+LTO, versioning, local copy).
6. Type-punning: why `*(int*)&f` is UB, what to use instead.
7. `-fopt-info-vec-missed` "possible aliasing" — what it means, how to fix.

---

## Next
→ [`10-lto.md`](10-lto.md)
