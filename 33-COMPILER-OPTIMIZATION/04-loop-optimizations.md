# 04 — Loop optimizations: unroll, fusion, interchange, hoisting, strength reduction

## Prerequisites
- `07-LOOPS/09-loop-performance.md`
- `32-CACHE-MEMORY-PERFORMANCE/05-locality.md` (interchange/fusion in cache terms)
- `31-CPU-ARCHITECTURE/04-pipelining.md` (why unroll helps ILP)

## Yeh topic abhi kyun
Programs apna zyada time loops mein bitate hain, isliye compiler ka sabse
bada optimization budget loops pe jaata. Yeh lesson batata **kaunsi loop
transforms compiler karta**, kab, aur kab woh aapki madad chahta (`__restrict`,
`constexpr` bounds, no early break). Aap in transforms ko pehchano to
assembly padhna aur "compiler ne yeh kyun nahi kiya" debug karna aata hai.

---

## Loop-invariant code motion (LICM / hoisting)

Jo computation loop ke andar hai par har iteration same result deta — use
loop ke bahar nikaal do.

```cpp
for (int i = 0; i < n; ++i) a[i] = b[i] * (x + y);   // (x+y) har iter?
// compiler: t = x + y;  for (i) a[i] = b[i] * t;
```
Compiler yeh routinely karta **agar** woh prove kar sake ki `x`, `y` loop mein
badalte nahi aur `a[i] = ` unhe alias nahi karta (lesson 09 — yahi jagah
`__restrict` bachata). Aliasing doubt → hoist nahi hota → har iter recompute.

---

## Strength reduction

Mehngi operation ko sasti se replace karo, aksar induction variables pe:

```cpp
for (int i = 0; i < n; ++i) p = base + i * stride;   // imul har iter
// compiler: p = base;  for (i) { ...; p += stride; }  // add har iter
```
`i * 4` → `i << 2`, `i * 10` → `(i<<3) + (i<<1)`, `x / 8` → `x >> 3` (unsigned),
`x % 16` → `x & 15`. Address arithmetic `a[i]` → `*(a + i)` with a running
pointer. `imul`/`idiv` ~3-40 cyc → `add`/`shl` ~1 cyc.

Division by a **constant** → reciprocal-multiply (`imul` + `shr`), not `idiv`
(folder 31 lesson 09). Runtime divisor → real `idiv` (or hoist `libdivide`).

---

## Loop unrolling

Body ko N baar copy karo, counter ko N se badhao:
```cpp
for (i = 0; i < n; ++i)      s += a[i];
// unroll x4:
for (i = 0; i + 3 < n; i += 4) { s += a[i]; s += a[i+1]; s += a[i+2]; s += a[i+3]; }
// + a scalar tail for n % 4
```
Faayde:
- **Loop overhead amortize** (`cmp`/`jne`/`add i` har 4 iters pe ek baar).
- **ILP** — 4 independent `s += ...` (agar `s` ko 4 partial accumulators mein
  split kiya, warna serial chain — folder 31 lesson 04). Compiler reduction
  unroll pe multiple accumulators khud banata (`-O2`+, integer; float needs
  `-ffast-math` — lesson 05/13).
- **Vectorization ka setup** — unroll aksar vectorize ke saath aata.

Nuksaan: **code size** → I-cache. `-O2` GCC pe **loops unroll nahi karta by
default** (sirf `-funroll-loops` ya `-O3`-cloned cases). `#pragma GCC unroll N`
ya `-funroll-loops` per-file. Aksar modern OoO CPU ko itna unroll nahi
chahiye jitna log sochte — measure.

---

## Loop fusion / fission

**Fusion** — do loops jo same range chalte, ek mein milao → data ek baar
cache mein (folder 32 lesson 05):
```cpp
for (i) b[i] = f(a[i]);
for (i) c[i] = g(a[i]);
// -> for (i) { b[i] = f(a[i]); c[i] = g(a[i]); }
```
Compiler `-floop-fuse` (Graphite, `-O3`) kabhi karta, par aksar aapko manually
karna padta. Caveat: 3+ streams overflow karein to fusion se koi fayda nahi.

**Fission** — ek bloated loop body ko todo (I-cache / multiple streams / to
enable vectorization of one part).

---

## Loop interchange

Nested loop ka order badlo taaki inner loop memory ko sequentially chale
(folder 32 lesson 05, example 03):
```cpp
for (j) for (i) sum += m[i*N + j];   // stride N -- cache hostile
// -> for (i) for (j) sum += m[i*N + j];   // stride 1
```
GCC `-ftree-loop-interchange` — **`-O3` pe ON**, `-O2` pe OFF. Folder 32
example `03` ne yeh measure kiya: `-O2` col-major 10× slower, `-O3
-march=native` interchange ne ratio ~1.0 kar diya. **Rely mat karo** — sirf
simple, provably-safe perfect nests pe hota.

---

## Loop unswitching

Ek loop-invariant `if` ko loop ke bahar nikaal do, do loop versions bana ke:
```cpp
for (i) { if (flag) a[i] = x; else a[i] = y; }
// -> if (flag) { for (i) a[i] = x; } else { for (i) a[i] = y; }
```
`flag` ab har iteration branch nahi hota. GCC `-funswitch-loops` (`-O3`).

---

## Loop rotation / guarding

`while`/`for` ko `do-while` + entry guard mein badalta:
```cpp
for (i = 0; i < n; ++i) body;
// -> if (n > 0) { i = 0; do { body; ++i; } while (i < n); }
```
Loop body ke end mein ek hi backward branch (better prediction), aur `n <= 0`
ka case ek baar check hota. `-O1`+ pe standard.

---

## Induction variable elimination / simplification

Multiple counters jo linearly related hain — compiler unhe ek mein reduce
karta. `for (i) { x = i*2; y = i*4; use(x, y); }` → `y = x + x` ya ek running
pair. Address IVs (`&a[i]`, `&b[i]`) ko bhi.

---

## Vectorization (lesson 05 ka full treatment)

Sabse bada loop transform — ek scalar loop ko SIMD mein badalna. Iske liye
loop ko "well-behaved" hona chahiye (dense, no carried dep, runtime trip
count, no aliasing doubt, no calls). Aksar unroll + peeling ke saath aata.

---

## Kya compiler ko rok deta

| Blocker | Fix |
|---|---|
| pointer aliasing doubt | `__restrict` (lesson 09), or `-fno-strict-aliasing` off |
| loop-carried dependency (`out[i] = out[i-1] + ...`) | algorithm change (scan) |
| function call in body | inline it / mark `pure`/`const` (lesson 03) |
| early `break` / `goto` out | split the loop; hoist the condition |
| non-affine index (`a[perm[i]]`) | gather is slow; sort/partition, or accept it |
| trip count unknown at all (`while (*p++)`) | give it a count if you can |
| `volatile` access in body | can't reorder/eliminate — by design |
| exceptions that could throw mid-loop | `noexcept` the callees |

`-fopt-info-loop`, `-fopt-info-vec-missed`, `-fopt-info-ivopts` se compiler
batata kya hua / kya nahi.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — manually unrolling by hand at -O2
Aap `s += a[i]; s += a[i+1]; ...` likhte ho — par ek hi accumulator → serial
chain (folder 31 lesson 04), unroll ka ILP fayda nahi. Compiler ko karne do
(multiple accumulators khud banata), ya explicitly `s0..s3` alag rakho.

### Trap 2 — `#pragma unroll 16` blindly
Bade unroll factor → I-cache bloat + register pressure (spills) → slower.
4–8 aksar sweet spot; modern OoO ko itna nahi chahiye. Measure.

### Trap 3 — loop order maan lena "-O3 fix kar dega"
Interchange sirf simple perfect nests pe, `-O3` only. Write the right order
(folder 32 example 03).

### Trap 4 — aliasing se hoisting/vectorization block hona aur pata na chalna
`for (i) a[i] = b[i] / *scale;` — `*scale` har iter reload agar `a` `scale`
ko alias kar sakta. `__restrict` → hoisted. `-fopt-info-vec-missed` batata.

### Trap 5 — fusion jahan streams zyada
2 loops fuse karke 4 arrays ek body mein touch → L1 / prefetcher pressure →
kabhi 2 clean loops tez. Measure both.

### Trap 6 — `volatile` loop variable
`for (volatile int i = 0; ...)` — har iter memory load/store, koi
strength-reduction/unroll/vectorize nahi. Sirf jab genuinely MMIO.

---

## > **HFT relevance**

> - **Hot reduction loops → give the compiler what it needs**: `__restrict`
>   pointers (or inline so it sees the caller), `constexpr`/known trip counts,
>   no calls in the body, `noexcept`. Then check `-fopt-info-vec` says
>   "vectorized".
> - **Split serial reductions into N partial accumulators** (checksum, hash,
>   running sum) so the OoO engine / SIMD lanes have independent work
>   (folder 31 lesson 04).
> - **Write the cache-friendly loop order yourself** — book scans, matrix-ish
>   greeks — don't rely on `-O3 -ftree-loop-interchange`.
> - **Fuse the per-tick passes** — one traversal of the book/message batch,
>   not three (folder 32 lesson 15 recipe 1).
> - **Keep divisors compile-time-constant** (tick sizes, lot sizes) →
>   reciprocal-multiply, not `idiv` in the hot path.
> - **Don't over-`#pragma unroll`** — verify `.text` size + `perf` didn't
>   regress on frontend.

---

## Hands-on

```bash
./build.ps1 asm 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp
#   map_vec loop: unrolled + `mulps`/`addps` ; map_scalar: one `mulss`/iter

g++ -O2 -fopt-info-loop-optimized ex.cpp -o x 2>&1 | head       # which loop transforms fired
g++ -O2 -fopt-info-vec-missed     ex.cpp -o x 2>&1 | head       # why not vectorized
g++ -O2 -fdump-tree-ivopts=/dev/stdout -c ex.cpp | head -60     # induction-var opts

# strength reduction on a constant divisor:
echo 'int f(int x){return x/7;}' | g++ -O2 -S -masm=intel -xc++ - -o - | grep -A6 'f(int)'
#   no `idiv` -- `imul` by a magic constant + shifts
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "manual unroll = faster" | one accumulator → serial; compiler splits, you often don't |
| "bigger `#pragma unroll` = better" | I-cache bloat + spills; 4-8, measure |
| "`-O3` fixes loop order" | interchange only for simple perfect nests |
| "`x % 16` costs a `div`" | `& 15`; and `x / const` → reciprocal-multiply |
| "compiler hoists everything invariant" | only if no aliasing doubt (→ `__restrict`) |
| "fusion always wins" | 3+ streams can overflow L1 / prefetcher |

---

## Exercises

1. `for (int i = 0; i < n; ++i) out[i] = base[i] + i * 12;` — name every loop
   optimization the compiler applies at `-O2`.

   <details><summary>Answer</summary>

   **Strength reduction** on `i * 12` → a running `t += 12` per iteration
   (`imul` gone). **Induction-variable simplification** — `&out[i]` and
   `&base[i]` become running pointers (`p += 4`), the loop compares pointers
   not `i`. **Loop rotation** — `if (n > 0) do { ... } while (p != end);`.
   **Vectorization** — if `out`/`base` don't alias (compiler versions or
   `__restrict`), 4/8-wide `paddd` with a vector of `[t, t+12, t+24, t+36,...]`.
   **Unrolling** — usually paired with the vectorization (plus a scalar tail
   for `n % width`). **LICM** — `n`, `base`, `out` (loop-invariant) held in
   registers. Confirm with `-fopt-info-loop-optimized` and `./build.ps1 asm`.
   </details>

2. `double avg(const double* a, int n) { double s = 0; for (int i=0;i<n;++i)
   s += a[i]; return s / n; }` — plain `-O2` pe vectorize hoga? Aur `s / n`?

   <details><summary>Answer</summary>

   **The sum loop does NOT vectorize at plain `-O2`** — `s += a[i]` is a
   float reduction; using partial sums changes the summation order → changes
   rounding → the compiler needs `-ffast-math` / `-fassociative-math` to do
   it (example `03` CASE 2, measured: ~4× with `-ffast-math`, nothing
   without). **`s / n`** — `n` is a runtime `int`, so this is one real
   `divsd` (fine, it's once, not in the loop) after converting `n` to double.
   If `n` were `constexpr`, `s / n` → `s * (1.0/n)` folded. To vectorize the
   sum safely, use `#pragma omp simd reduction(+:s)` (opts into reassociation
   for just that loop) or accept the plain scalar `addsd` chain (~3-4 cyc/elem).
   </details>

3. `for (i) { a[i] = x[i] * k; b[i] = x[i] + k; }` vs two separate loops.
   Which is faster and does it depend on sizes?

   <details><summary>Answer</summary>

   **Fused (one loop) is usually faster** when `x` doesn't fit cache: `x[i]`
   is loaded once and feeds both writes → half the reads of `x` vs two loops
   that each stream `x` from DRAM. Both `a` and `b` are written either way.
   It **depends on size**: if `x`, `a`, `b` all fit in L1/L2, the two-loop
   version costs nothing extra (data stays hot) and the fused body has 3
   live streams which is fine. If `x` is huge (>L3) the fused version's
   ~1.5× less memory traffic is a real win (folder 32 lesson 13). If you had
   5+ arrays in the fused body, the stream/prefetcher pressure could flip it.
   Measure at the real working-set size.
   </details>

---

## Interview questions

1. LICM — kya, aur kaunsi cheez use block karti (aliasing).
2. Strength reduction — 3 examples (mul→shift, IV, addr).
3. Loop unrolling — 2 benefits, 1 cost, aur "one accumulator" trap.
4. Loop interchange — kaunse `-O` pe, aur kyun rely nahi karna.
5. Loop fusion vs fission — kab kaunsa.
6. 4 cheezein jo loop vectorization ko rokti hain.
7. `x / 7` vs `x / d` (runtime) — codegen ka farak.

---

## Next
→ [`05-vectorization.md`](05-vectorization.md)
