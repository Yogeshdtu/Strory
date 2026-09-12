# 07 — Struct layout tuning: field order, hot/cold fields, packing

## Prerequisites
- `06-instruction-cache-layout.md`
- `25-OBJECT-MODEL/*` (alignment, padding), `32-CACHE-MEMORY-PERFORMANCE/*`
- `36-LOW-LATENCY-CPP/10-cache-locality-tuning.md`

## Yeh topic abhi kyun

06 tak **code** layout. Ab **data** layout — struct ke andar fields kaise
arrange hote, aur ye kaise decide karta ki tumhara hot scan 1 cache line
chhuta ya 10.

`07_struct_tuning.cpp` teen alag cheezein measure karta.

---

## 1. Padding — field order galat → struct bada

C++ har field ko uske type ke alignment pe rakhta. Beech mein gap = padding.

```cpp
struct PadBad  { char side; double price; char flag; };   // sizeof = 24
//   [side][pad×7][ price (8) ][flag][pad×7]
struct PadGood { double price; char side; char flag; };   // sizeof = 16
//   [ price (8) ][side][flag][pad×6]
```

Measured:
```
sizeof(PadBad)  = 24 bytes
sizeof(PadGood) = 16 bytes    -> 34% smaller, ZERO behaviour change
```

**Rule of thumb:** fields ko **descending alignment** mein rakho (8-byte
pehle, phir 4, phir 2, phir 1). Ek array-of-struct mein 33% chhota =
33% zyada elements per cache line = 33% kam memory traffic ek scan mein.

Detect: `g++ -Wpadded` — har padding byte pe warning. `offsetof()` /
`static_assert(sizeof(T) == N)` se lock karo.

---

## 2 & 3. Hot fields vs cold fields, AoS vs SoA

Ek realistic order record:

```cpp
struct FatOrder {           // 64 bytes = 1 cache line
    std::int32_t  px;       // <-- hot (har book scan chhuta)
    std::int32_t  qty;      // <-- hot
    std::uint64_t id;
    std::uint64_t ts_recv;  //   cold (sirf logging / audit)
    std::uint64_t ts_book;  //   cold
    std::uint32_t participant;
    std::uint32_t flags;
    std::uint64_t prev, next;  // intrusive list links
    std::uint64_t reserved;
};
```

Ek "aggregate px+qty over all resting orders" scan sirf **8 bytes** (px,
qty) chhuta — par har element **64 bytes** memory se aata (poori line).
**8× waste.**

Do fixes:

**Slim struct** — us pass ke liye sirf jo chahiye:
```cpp
struct SlimOrder { std::int32_t px; std::int32_t qty; };   // 8 bytes
```

**SoA (Structure of Arrays)** — har field apna array:
```cpp
std::vector<std::int32_t> px_arr, qty_arr;
```

Measured (`07_struct_tuning.cpp`, 4M orders, is box):

```
AoS fat  (64 B/elem) :  ~3.5  ns/elem
AoS slim ( 8 B/elem) :  ~0.52 ns/elem   (~6.7x vs fat)
SoA      ( 8 B/elem) :  ~0.42 ns/elem   (~8.3x vs fat)
```

**~8× faster** — bilkul jitna memory-traffic ratio predict karta (64/8).
Yeh scan **memory-bandwidth-bound** hai; kaam kam nahi hua, sirf bytes kam
huye.

> NOTE: `07`'s scan ka predicate jaan-boojh kar hataya gaya (pure streaming
> reduction) taaki teenon versions vectorize hon aur **sirf layout**
> compare ho. Ek unpredictable `if` daal do to branch-mispredict (`36/06`)
> signal ko dhak leta — alag lesson.

---

## Kab AoS, kab SoA

| Access pattern | Behtar | Kyun |
|---|---|---|
| Ek field, saare elements pe scan (book aggregate, filter) | **SoA / slim** | sirf us field ke bytes; vectorize-friendly |
| Saare fields, ek element (ek order fully process) | **AoS** | ek cache line, ek miss — sab paas |
| Kuch hot fields har baar + kuch cold fields kabhi | **Split**: hot struct + cold struct, `id` se link | hot line dense, cold alag |

Production order book (39 V3, `pipeline.hpp` V3) yahi karti: level array
mein **sirf** `total_qty` (aur shayad `order_count`) — 8-16 bytes/level.
Per-order metadata (ts, participant, flags) alag slab mein, `id` se
indexed, sirf cancel/audit pe touch.

---

## Alignment — hot struct ko cache line pe

```cpp
struct alignas(64) HotState { std::int64_t best_bid, best_ask, mid, ...; };
```

- Ek `alignas(64)` struct kabhi 2 cache lines mein split nahi hoga (ek load
  = ek line).
- Multi-threaded: `alignas(64)` + padding = **false sharing** se bachao
  (`08`, `36/11`, `41`).
- Cost: memory waste (har instance ≥ 64 B), array mein gaps. Sirf sach
  mein hot / shared structs pe.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — reorder se ABI toot gaya
Agar struct kisi wire format / shared-memory / mmap'd file / dusri library
se match karta — field reorder = **breaking change**. Wire structs ko
`#pragma pack` + explicit layout, unhe reorder mat karo. Internal structs
free hain.

### Trap 2 — `alignas(64)` sab pe
Har chhoti struct 64 B → arrays mein massive waste → **zyada** cache lines
touch → ulta slow. Sirf hot/shared.

### Trap 3 — SoA sab jagah
SoA mein ek element ke saare fields chahiye to N alag cache lines (ek per
array) → N misses. Random single-element access pe AoS jeetta. **Access
pattern se decide karo.**

### Trap 4 — 4K aliasing (SoA ka chhupa footgun)
Do bade arrays jo exactly `2^k` bytes apart alloc huye → unke same indices
same cache set pe map → ek doosre ko evict. `07` mein arrays ko `+17` int
offset diya isse bachne ko (is box pe farak nahi pada, par safe habit).
`perf c2c` / `perf stat` cache-miss se pakdo.

### Trap 5 — `-Wpadded` ko blindly follow
`-Wpadded` **har** padding pe cheekhta, chhote structs pe bhi jahan farak
nahi. Isko sirf hot array-of-struct types pe apply karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Field order = readability ka mamla | Hot array-of-struct mein = performance (padding, lines/scan) |
| `sizeof` chhota = hamesha behtar | Sirf jab woh struct arrays mein scan hota; single-use pe farak nahi |
| SoA hamesha tez (vectorization) | Sirf single-field scans pe; multi-field element access pe AoS |
| `alignas(64)` = free speed | Memory waste; sirf hot/shared; warna zyada lines |

---

## Hands-on

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/07_struct_tuning.cpp
g++ -std=c++20 -O2 -Wpadded 43-HFT-OPTIMIZATION/examples/07_struct_tuning.cpp -o /dev/null
```

---

## Exercises

1. `PadBad`/`PadGood` mein ek aur `char` field add karo. Dono ke `sizeof`
   ab? Rule confirm hua?
   <details><summary>Answer</summary>
   PadGood: `double(8) + char + char + char + pad×5` = 16 (abhi bhi — pad
   me aa gaya). PadBad: `char + pad7 + double(8) + char + char + pad6` = 24.
   PadGood ke andar 3 chars "free" fit huye kyunki descending-align layout
   mein pehle se pad tha. Rule confirm.
   </details>

2. `07` ka SoA scan `slim` se thoda tez kyun (0.42 vs 0.52)?
   <details><summary>Answer</summary>
   `slim` = ek 32 MB contiguous stream (interleaved px,qty). SoA = do 16 MB
   streams, dono pure-sequential, HW prefetcher dono ko independently
   chalata, aur vectorizer ke liye SoA "textbook" hai (koi
   struct-of-2 deinterleave nahi). Farak chhota — dono memory-BW bound.
   </details>

3. `pipeline.hpp` ka V3 book `std::array<std::int64_t, 4096>` per side
   rakhta (sirf qty). Agar per-order `{id, ts, participant}` bhi usi array
   mein daal dein to kya hoga signal ke top-of-book scan pe?
   <details><summary>Answer</summary>
   Level struct 8 B se ~32 B ho jaata → `rewalk_bid_/ask_` (jo empty levels
   pe walk karta) 4× zyada memory chhuta → dheema, aur cache mein kam
   levels. Per-order data ko alag slab mein rakho (jo V3 karti: `id_loc_`
   vector), level array ko patla.
   </details>

---

## Interview questions

1. Struct fields ko kis order mein rakhna (alignment terms)? Kyun?
2. AoS vs SoA — ek-ek example jahan har ek jeeta.
3. Hot/cold **field** split kya hai? Order book mein example?
4. `alignas(64)` kab lagana, kab **nahi** (dono side ke reasons)?
5. `-Wpadded` kya batata? Iske output ko kaise filter karoge?

---

## Next
→ [`08-eliminating-false-sharing.md`](08-eliminating-false-sharing.md)
