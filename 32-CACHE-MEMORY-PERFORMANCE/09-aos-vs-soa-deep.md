# 09 — AoS vs SoA (deep): full analysis with SIMD

## Prerequisites
- `08-cache-friendly-structures.md`
- `31-CPU-ARCHITECTURE/10-simd-basics.md` (SoA is SIMD ka prerequisite)
- example `05_aos_vs_soa.cpp`

## Yeh topic abhi kyun
Ek collection of records ko memory mein do tarah rakh sakte ho — har record
saath (**Array of Structs**), ya har field ka apna array (**Struct of
Arrays**). Yeh choice cache bandwidth utilization aur vectorization dono
decide karta. Aur — jaise example `05` ne dikhaya — **"SoA hamesha better"
galat hai.** Access pattern decide karta.

---

## Do layouts

```cpp
// AoS
struct Particle { float x, y, z, vx, vy, vz, mass; uint32_t id; };  // 32 B
std::vector<Particle> ps;
// memory: [x0 y0 z0 vx0 vy0 vz0 m0 id0][x1 y1 z1 ...][x2 ...] ...

// SoA
struct Particles {
    std::vector<float> x, y, z, vx, vy, vz, mass;
    std::vector<uint32_t> id;
};
// memory: [x0 x1 x2 x3 ...] [y0 y1 y2 ...] [z0 ...] ...
```

---

## Kaun jeetta — access pattern pe depend

### Case A: few fields, many records, sequential → **SoA**
`for (i) bbox.update(x[i], y[i], z[i]);` — sirf 3 of 8 fields.
- **AoS**: har `Particle` load = poori 32-B struct line mein (2 particles /
  line), par sirf 12 B (x,y,z) use → **62% line bandwidth waste**. Aur
  vectorizer ko `x` values 32 B apart chahiye → gather/scatter → slow.
- **SoA**: `x[]` dense → 100% line use, 16 floats/line, aur `x[i], x[i+1],
  ...` contiguous → compiler 8-wide `_mm256` load → **vectorize**.

Example `05` Test 1: **SoA ~2.0x** (bandwidth + vectorization stacked).

### Case B: most fields, sequential → **still SoA** (surprise)
`for (i) { x[i] += vx[i]*dt; y[i] += vy[i]*dt; z[i] += vz[i]*dt; }` — 6 of 8.
- Intuition: AoS ka waste ab sirf 2/8 fields → gap shrink hona chahiye.
- **Measured (example `05` Test 2): SoA ~2.4x — gap SHRINK NAHI hua.**
- Kyun: `x[i] += vx[i]*dt` ek **perfect vector loop** (dense, aligned,
  independent) → compiler FMA-vectorizes it 8-wide. AoS ka 32-B stride
  vectorizer ko rok deta (strided gather of `x`, strided scatter back) →
  scalar. SoA ka SIMD advantage compensates for touching more arrays.

### Case C: all fields, random record order → **AoS**
`for (k : shuffled) process(ps[k]);` — har field chahiye, random `k`.
- **AoS**: `ps[k]` = ek 32-B struct, ek (ya do) cache line → **1 miss** per
  record.
- **SoA**: same record ke fields 8 alag arrays mein, har ek `arr[k]` random
  offset pe → **8 alag cache lines → 8 misses** per record.

Example `05` Test 3: **AoS ~3x faster.**

---

## Decision table

| Access pattern | Winner | Why |
|---|---|---|
| Scan a few fields, sequential | **SoA** | line utilization + vectorize |
| Scan most fields, sequential | **SoA** (usually) | SIMD on dense arrays wins |
| Touch all fields of one record, random order | **AoS** | 1 line vs N lines |
| Touch all fields, sequential | ~tie / **AoS** slight | AoS = 1 stream, SoA = N streams |
| Insert/remove records often | **AoS** | one `push_back`/swap-pop vs N |
| Passing single records around by value/ref | **AoS** | it's one object |

---

## AoSoA (Array of Structs of Arrays) — the hybrid

```cpp
constexpr int W = 8;                       // SIMD width
struct Chunk { float x[W], y[W], z[W], vx[W], ...; };   // one chunk = W records
std::vector<Chunk> chunks;                 // ceil(N/W) chunks
```

Har chunk ek cache-line-friendly SoA tile hai (W records). Benefits:
- **Vectorizable**: `chunk.x[0..7]` ek `_mm256` load — SoA jaisa.
- **Locality for whole-record access**: ek record ke saare fields ek chunk
  (few adjacent lines) mein — AoS jaisa, random-chunk access mein.
- SIMD libraries (Highway, xsimd, ISPC) aur physics engines yahi use karte.

Cost: indexing thoda complex (`chunk = i / W`, `lane = i % W`), aur partial
last chunk handle karna.

---

## Padding waste in AoS

```cpp
struct Bad { double t; int32_t a; double v; };   // 8 + 4 + (4 pad) + 8 = 24
struct Good { double t; double v; int32_t a; };   // 8 + 8 + 4 + (4 pad) = 24 (still)
struct Packed { double t; double v; int32_t a; };  // reorder large->small helps
```

Fields ko **large → small** order karo → internal padding minimize. `int32`
vs `int64`, `float` vs `double`, `enum : uint8_t` — har byte × N. SoA mein
yeh problem nahi (har array apne type ka dense).

---

## Vectorization needs SoA (or AoSoA)

`31-CPU-ARCHITECTURE/10` se: SIMD ek instruction se 8 floats process karta —
par woh 8 floats **contiguous** chahiye (`_mm256_load_ps`). AoS mein `x` values
`sizeof(Particle)` apart → compiler ko `_mm256_i32gather_ps` (slow, ~lane-
count cycles) chahiye ya scalar fallback. SoA/AoSoA → dense → real SIMD.

Isliye: **agar aap ek field pe heavy math karte ho over many records → SoA,
aur -O2/-O3 usse auto-vectorize karega** (example `05` Test 1/2).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "SoA always faster"
Example `05` Test 3: random whole-record access → AoS 3x faster. SoA ka har
field ek alag miss.

### Trap 2 — SoA with `std::vector` per field, no reserve
8 vectors, har `push_back` alag realloc timeline → data fragmented across
heap. `reserve(N)` sab pe, ya ek arena.

### Trap 3 — SoA banake bhi scalar loop likhna
`for (i) x[i] = f(x[i]);` jahan `f` non-vectorizable (branch, call) → SoA ka
aadha fayda (locality) milta hai, SIMD nahi. `f` ko branchless / inline karo.

### Trap 4 — AoS struct that straddles lines
`struct { double a,b,c; }` = 24 B → `vector` mein har ~2.67 records ek line
boundary cross → ek record kabhi 2 lines mein. `alignas(32)` ya SoA.

### Trap 5 — "id" / cold fields in the hot SoA arrays
SoA mein bhi: `id[]`, `flags[]` jo hot loop mein use nahi hote — unhe hot
arrays (`x,y,z`) se alag rakho taaki prefetcher unke liye bandwidth waste
na kare. (Hot/cold, lesson 15.)

### Trap 6 — converting AoS↔SoA every frame
Agar aapko dono chahiye (SoA for the physics pass, AoS for the render/query),
transpose per frame = ek extra full pass. AoSoA se dono ek layout mein.

---

## > **HFT relevance**

> - **Market data scan = SoA.** Ek feed handler jo N instruments ke `last_px`
>   pe filter chalata → `px[]` dense array → vectorized compare. `struct
>   Instrument { ... }` array nahi.
> - **Order book level = it depends.** Agar aap har tick top-K levels ke
>   `(price, qty)` scan karte → SoA `price[]`, `qty[]`. Agar aap ek specific
>   level ko random-access karke uske saare fields chahiye → AoS `Level`.
> - **Signals / features = SoA.** Ek row per instrument, ek column per
>   feature → column-major (SoA) → har feature computation vectorized over
>   all instruments.
> - **AoSoA for the risk/greeks batch.** W instruments per chunk, SIMD the
>   pricing, whole-instrument locality for the per-name adjustments.
> - **`static_assert` on struct sizes.** AoS structs 64 ka factor, no
>   straddle. SoA arrays `reserve`d, hot/cold separated.

---

## Hands-on

```bash
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/05_aos_vs_soa.cpp
#   T1 scan-few seq     -> SoA ~2.0x
#   T2 scan-most seq    -> SoA ~2.4x  (gap NAHI shrink -- SoA vectorizes)
#   T3 all-fields random-> AoS ~3x

# vectorization confirm karo:
./build.ps1 asm 32-CACHE-MEMORY-PERFORMANCE/examples/05_aos_vs_soa.cpp
#   SoA loop me ymm registers + vfmadd ; AoS me scalar / gather
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "SoA hamesha tez" | random whole-record → AoS 3x (example 05 T3) |
| "SoA sirf memory saving" | bada win = enables vectorization |
| "most-fields touched → AoS jeetta" | SoA still ~2.4x — SIMD compensates (T2) |
| "AoSoA overkill" | SIMD libs / physics engines ka default — dono benefits |
| "field order koi farak nahi" | large→small = less padding, ×N bytes |
| "SoA banao, kaam ho gaya" | scalar loop = no SIMD; branchless + inline chahiye |

---

## Exercises

1. `struct Quote { char sym[8]; double bid, ask; uint32_t bidsz, asksz;
   uint64_t ts; };` — 100k quotes. Aap har tick sabka `mid = (bid+ask)/2`
   compute karke ek threshold se compare karte ho. AoS ya SoA? Layout likho.

   <details><summary>Answer</summary>

   **SoA** — aap sirf `bid` aur `ask` (2 of 6 fields) scan karte ho over
   100k records. AoS: `sizeof(Quote)` = 8+8+8+4+4+8 = 40 B → har quote ek
   line ka 62% se zyada, aur sirf 16 B (bid+ask) use → waste. SoA:
   `vector<double> bid, ask;` dense → 8 mids per AVX2 op, contiguous.
   ```cpp
   struct Quotes {
       std::vector<double> bid, ask;      // hot
       std::vector<uint32_t> bidsz, asksz;// cold-ish
       std::vector<uint64_t> ts;          // cold
       std::vector<std::array<char,8>> sym; // cold
   };
   // hot loop: for (i) if (0.5*(bid[i]+ask[i]) > thr[i]) mark(i);  -> vectorizes
   ```
   </details>

2. Example `05` Test 2 mein 6 of 8 fields touch hue par SoA ka lead 2.0x se
   2.4x ho gaya, ghata nahi. Do reasons.

   <details><summary>Answer</summary>

   (1) **SoA vectorizes, AoS doesn't.** `x[i] += vx[i]*dt` over dense arrays
   → compiler emits 8-wide FMA (`vfmadd...ps` on ymm). AoS `ps[i].x += ps[i].
   vx*dt` has 32-B stride → vectorizer bails → scalar FMA, 1 element/instr.
   The 8× SIMD throughput on SoA more than pays for reading 6 arrays instead
   of 1 struct stream. (2) **AoS read-modify-write of a partially-used line**:
   AoS loads the whole 32-B struct, modifies 12 B (x,y,z), writes the line
   back — the vx,vy,vz reads share the line (ok) but the RFO + writeback is
   for the full line. SoA writes only `x[],y[],z[]` — dense, and `vx,vy,vz`
   are read-only dense. Net: SoA moves less and computes 8× faster.
   </details>

3. Aapke pas ek 512-instrument risk batch hai. Har instrument ke 20 fields.
   Pricing pass sabhi 20 fields ko SIMD-heavy math mein use karta (over all
   512). Per-name adjustment pass ek instrument ke sab 20 fields random-access
   karta hai. Layout?

   <details><summary>Answer</summary>

   **AoSoA** with W = 8 (AVX2) or 16 (AVX-512): `struct Chunk { double
   f0[W], f1[W], ..., f19[W]; }; vector<Chunk> chunks(512/W);`. Pricing pass:
   iterate chunks, `chunk.f0[0..7]` is a dense `_mm256d` load → full SIMD,
   and all 20 fields of a chunk are in ~20*W*8/64 ≈ 20 adjacent lines (for
   W=8: 160 lines... hmm, actually a chunk is 20*8*8 = 1280 B = 20 lines).
   Per-name pass: instrument i is in `chunks[i/W]`, lane `i%W` — its 20 fields
   are within that one chunk's 20 lines → ~20 misses worst case but they're
   **contiguous / prefetchable**, vs pure SoA's 20 scattered misses across
   20 separate big arrays. AoSoA gives you SIMD for the batch pass AND
   bounded, local access for the per-name pass.
   </details>

---

## Interview questions

1. AoS vs SoA — memory layout, ek diagram.
2. "Scan few fields over many records" — kaun jeetta aur kyun (2 effects).
3. Example `05` Test 2 — 6/8 fields touch hote hue bhi SoA kyun aage.
4. "Random whole-record access" — AoS kyun (miss count).
5. AoSoA — kya, aur woh kaunse do benefits combine karta.
6. Struct field ordering (large→small) — kya bachata.
7. SoA + scalar loop — kya milta hai aur kya nahi.

---

## Next
→ [`10-data-oriented-design.md`](10-data-oriented-design.md)
