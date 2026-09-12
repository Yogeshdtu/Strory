# 05 — Locality: spatial aur temporal

## Prerequisites
- `04-cache-misses.md`
- `07-LOOPS/09-loop-performance.md` (folder 07 ka cache-locality intro)

## Yeh topic abhi kyun
Cache poori tarah ek **shart** par kaam karti hai: "jo abhi chhua, woh phir
chhua jaayega (temporal), aur uske aas-paas ka bhi chhua jaayega (spatial)."
Achha code is shart ko sach banata — access pattern aisa jo cache ki bet
jeetne de. Bura code ise todta — random jumps, columns, pointer graphs. Yeh
lesson locality ko **code transformations** mein badalna sikhata.

---

## Do tarah ki locality

### Spatial locality
"Agar address X chhua, to X+1, X+8, X+64 jaldi chahiye." Cache ise line
(64 B) se exploit karti — ek miss, agle 15 ints free. Arrays sequentially
padhna = maximum spatial locality.

### Temporal locality
"Agar address X abhi chhua, woh thodi der mein phir chahiye." Cache use rakh
leti (LRU-ish). Ek chhota lookup table jo baar-baar use hota → L1 mein baith
jaata. Ek loop variable, ek hot counter.

**Achha performance = dono ka fayda uthana.** Kabhi tension hoti hai (bade
line se spatial milta par capacity kam), par mostly dono ek saath aate hain
achhe layout se.

---

## Measured: sequential vs strided vs random

Example `02` (64 MiB int buffer, is box):

| Pattern | ns per access | kyun |
|---|---|---|
| sequential (stride 1) | ~0.3 | har line 16 hits + prefetcher aage laata |
| stride 16 (=64 B, har line 1 use) | ~3.3 | har access naya line, prefetcher madad |
| **random line order** (prefetcher defeated) | **~32 / line** | har line asli miss, sirf MLP overlap |

Ratio sequential : random ≈ **7x** (example `02` PART 2: 4.6 vs 32 ns/line).
Random access "same amount of data" chhuta hai — par ~7x dheema, kyunki na
spatial (har line 1 use), na prefetch (pattern unpredictable).

---

## Code transformations for locality

### 1. Loop interchange
Storage order se loop order match karo.

```cpp
// ❌ row-major array, column-major loop -> stride N, har element naya line
for (j) for (i) sum += m[i][j];          // example 03: ~2.5 ns/elem

// ✅ match
for (i) for (j) sum += m[i][j];          // example 03: ~0.25 ns/elem  (~10x)
```

### 2. Loop fusion
Do loops jo same array pe chalte → ek loop → data ek baar cache mein aata,
do baar nahi.

```cpp
// ❌ a[] do baar poora traverse
for (i) b[i] = f(a[i]);
for (i) c[i] = g(a[i]);

// ✅ ek traversal, a[i] ek baar load
for (i) { b[i] = f(a[i]); c[i] = g(a[i]); }
```
(Caveat: agar `b` aur `c` ke saath `a` teenon L1 mein na sama — 3 streams —
to fusion se koi fayda nahi ya thoda ulta. Measure.)

### 3. Loop fission (ulta)
Agar ek loop body bahut alag-alag data touch karta (I-cache / multiple
streams overflow), use todo — har part ka apna clean stream.

### 4. Tiling / blocking (lesson 15, example `07`)
Bade problem ko cache-sized tukdon mein — har tile ka data poore reuse ke baad
hi evict ho.

### 5. Array-of-struct → struct-of-array (lesson 09)
Agar aap ek struct ke sirf kuch fields scan karte, unhe alag contiguous arrays
mein rakho.

### 6. Hot/cold splitting (lesson 15)
Ek struct ke frequently-touched fields ek jagah, rarely-touched alag → hot
lines mein sirf hot data.

### 7. Packing / smaller types
`int32` instead of `int64`, `float` instead of `double`, indices instead of
8-byte pointers, bitfields for flags → zyada elements per line → better
spatial, smaller working set → better temporal.

---

## Working-set thinking

Har hot loop ke liye poochho: **"is loop ka ek iteration kitni distinct cache
lines chhuta hai? Aur poora loop?"**

- Poora loop ka working set **L1 (32 KiB = 512 lines)** mein → har line
  reuse hogi, ~L1 speed.
- L1 se bada, **L2 (512 KiB)** mein → L2 speed (~4x dheema than L1).
- L2 se bada, **L3 (8 MiB)** mein → L3 speed.
- L3 se bada → **DRAM** har iteration → sabse dheema (aur bandwidth-bound,
  lesson 13).

Blocking ka poora idea: agar working set L3 se bada hai, use L1/L2-sized
tiles mein todo taaki har tile ka working set upar aa jaaye.

---

## Prefetcher se dosti (lesson 06 detail)

HW prefetcher **monotonic strides ek page ke andar** detect karta. Isliye:
- ✅ `for (i) sum += a[i];` — perfect, prefetcher aage bhaagta
- ✅ `for (i) sum += a[i * 4];` — constant stride, mostly detect
- ⚠️ `for (i) sum += a[perm[i]];` — indirect, prefetcher andha
- ❌ `n = n->next` — pointer chase, prefetcher andha
- ⚠️ page boundary pe stride prefetcher reset hota (per-page state)

"Linear banao" ka matlab sirf spatial nahi — prefetcher ko bhi khush karna.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — nested loop order storage se ulta
Sabse common cache bug. `m[i][j]` row-major hai to `i` bahar. `std::vector<
std::vector<T>>` mein to har row alag allocation — aur bura.

### Trap 2 — "sab kuch ek loop mein" (over-fusion)
5 arrays ek loop body mein touch karna = 5 streams = prefetcher/L1 pressure.
Kabhi 2 clean loops 1 bloated loop se tez.

### Trap 3 — temporal locality maan lena bina reuse ke
Ek lookup table jise aap **ek baar** use karte per element — usme temporal
locality nahi, woh bas ek aur stream hai jo cache kha raha.

### Trap 4 — random shuffle "fairness" ke liye
Kai algos data ko shuffle karte (ML batching, Monte Carlo). Shuffled access =
locality khatm. Agar possible ho to block-shuffle (chunks shuffle karo,
chunk ke andar sequential).

### Trap 5 — `std::map` / `std::set` ko "sorted array" samajhna
Woh red-black tree — har lookup ~log N pointer hops, har hop possibly a miss.
Sorted `std::vector` + `std::lower_bound` = contiguous, binary search, kahin
behtar locality. (Lesson 08.)

### Trap 6 — matrix transpose "free" maan lena
`b[j][i] = a[i][j]` — ek side sequential, doosra strided. Naive transpose
cache-hostile. Blocked transpose (32×32 tiles) chahiye.

---

## > **HFT relevance**

> - **Hot loop ka working set L1/L2 mein.** Book ke top levels, active orders,
>   handler state — sab ko contiguous + chhota rakho. Ek `perf` number:
>   `mem_load_retired.l3_miss` per tick ~0 hona chahiye.
> - **Sequential > everything.** Market data ring buffer sequentially consume
>   karo. Order book updates ko sorted array mein rakho (index by price
>   level), tree mein nahi.
> - **Batch, don't chase.** Ek tick pe N updates aaye → unhe ek array mein
>   collect karke ek sequential pass mein apply karo — na ki har update pe
>   tree walk.
> - **Prefault + prime.** Startup pe hot data structures ko touch karo (page
>   fault + cache warm) taaki pehla real tick cold miss na khaye.
> - **Shuffle mat karo hot path mein.** Agar randomization chahiye (A/B feed
>   pick), woh branch se karo, data reorder se nahi.

---

## Hands-on

```bash
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/02_stride_access.cpp   # seq vs random
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/03_matrix_traversal.cpp # loop interchange
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/05_aos_vs_soa.cpp       # layout

# Linux: locality ka signal
perf stat -e L1-dcache-load-misses,LLC-load-misses,cycles,instructions ./prog
#   IPC low + LLC-load-misses high on a "simple" loop -> locality problem
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "random access thoda dheema" | ~7x (example 02) — locality + prefetch dono gaye |
| "loop order style choice" | storage order se match — 10x tak farak |
| "fusion hamesha achhi" | 3+ streams overflow — kabhi ulta |
| "`std::map` = sorted array" | RB-tree, pointer hops, har hop possible miss |
| "temporal locality = data reuse hoga" | sirf agar sach mein reuse ho — warna bas ek stream |
| "transpose free hai" | naive transpose cache-hostile; blocked chahiye |

---

## Exercises

1. `for (k) for (i) for (j) C[i][j] += A[i][k] * B[k][j];` — teeno arrays
   row-major. Har array ka inner-loop (`j`) access pattern? Kaunsa sabse
   achha, kaunsa bura?

   <details><summary>Answer</summary>

   Inner loop `j` chalta: `C[i][j]` → stride 1 (sequential, ✅). `B[k][j]` →
   stride 1 (sequential, ✅). `A[i][k]` → `j` pe depend nahi, **loop-invariant**
   (ek hi value, register mein baith jaati, ✅✅). Yeh `ikj` order hai —
   sabse cache-friendly (teenon sequential/invariant). Example `07` mein yehi
   ~4x tez hai naive `ijk` se (jismein `B[k][j]` stride-N column access hota).
   </details>

2. Aap ek particle sim mein har frame: (pass 1) `for p: p.vel += gravity*dt;`
   (pass 2) `for p: p.pos += p.vel*dt;`. Fusion karein? Kya fayda/nuksaan?

   <details><summary>Answer</summary>

   Haan, fuse karo: `for p: { p.vel += g*dt; p.pos += p.vel*dt; }`. Pass-1 +
   pass-2 alag mein `p` (ya SoA mein `vel[]` aur `pos[]`) do baar poora
   traverse hota — 2× DRAM reads agar particles > cache. Fused: har particle
   ek baar load, dono updates, ek baar store. ~2× less memory traffic.
   Nuksaan: sirf tab jab fused body itne alag arrays touch kare ki streams
   overflow karein — yahan sirf vel + pos (ya SoA me 2 arrays), theek hai.
   </details>

3. `std::unordered_map<int,int>` mein 1M entries, aap 1M random keys lookup
   karte ho. Approx kitne cache misses per lookup, aur `absl::flat_hash_map`
   se kya badlega?

   <details><summary>Answer</summary>

   `std::unordered_map` = bucket array + **linked list per bucket** (nodes
   alag alloc). Random key → bucket array index (1 miss) → first node (1 miss,
   alag alloc) → possibly more nodes on collision (more misses). ~2-3 misses/
   lookup, mostly dependent (serial). `absl::flat_hash_map` = open addressing,
   ek contiguous slot array + SIMD control-byte probing → bucket group (1
   miss, sometimes 0 if hot) → data in same/adjacent slot. ~1 miss/lookup,
   better locality, no per-node alloc. Typically 2-4x faster lookups.
   (Lesson 08.)
   </details>

---

## Interview questions

1. Spatial vs temporal locality — definition aur cache har ek ko kaise exploit karti.
2. Loop interchange — kya, kab, kitna farak (example do).
3. Loop fusion vs fission — dono kab.
4. "Working set" — kaise estimate karte, aur woh L1/L2/L3 se compare karke kya batata.
5. HW prefetcher kaunse patterns pasand karta — 3 achhe, 2 bure.
6. `std::map` vs sorted `std::vector` — locality ke terms mein.
7. Random shuffle performance ko kyun maar deta, aur block-shuffle kaise madad karta.

---

## Next
→ [`06-prefetching.md`](06-prefetching.md)
