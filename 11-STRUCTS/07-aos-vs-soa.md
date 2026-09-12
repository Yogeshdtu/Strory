# 07 — Array-of-Structs vs Struct-of-Arrays

## Prerequisites
- [`05-padding-and-alignment.md`](05-padding-and-alignment.md), [`06-packed-structs.md`](06-packed-structs.md)
- `09-ARRAYS/10-array-performance.md` (`07_aos_vs_soa.cpp` — measured ~4x)
- `07-LOOPS/09-loop-performance.md` (cache locality)

## Yeh topic abhi kyun
Ek "entity" ke ki fields hain, aur aapke paas laakhon entities hain. Do tareeke
store karne ke — **AoS** (`vector<Entity>`) aur **SoA** (har field ka apna
array). Yeh **data-oriented design** ka core hai, aur HFT mein order books /
analytics ka default layout.

---

## Do layouts

```cpp
struct Quote { int64_t ts; int32_t bid, ask, bidSz, askSz; int16_t venue, flags; };

// AoS -- Array of Structs
std::vector<Quote> quotes;
// memory: [ts0 bid0 ask0 ...][ts1 bid1 ask1 ...]...   -- one struct's fields together

// SoA -- Struct of Arrays
struct QuotesSoA {
    std::vector<int64_t> ts;
    std::vector<int32_t> bid, ask, bidSz, askSz;
    std::vector<int16_t> venue, flags;
};
// memory: [ts0 ts1 ts2 ...] [bid0 bid1 bid2 ...] ...   -- one field's values together
```

---

## The performance question: which fields do you touch?

### Field-SUBSET access → SoA wins

"Compute max bid over all quotes" — only `bid` matters.

- **AoS**: `for (const Quote& q : quotes) m = max(m, q.bid);` — stride is
  `sizeof(Quote)` (32 B). Each 64-byte cache line holds 2 quotes → you load 32 B
  to use 4 B. **~8x wasted bandwidth.**
- **SoA**: `for (int32_t b : soa.bid) m = max(m, b);` — contiguous. Every byte in
  every cache line is a `bid`. **100% useful**, and it auto-vectorizes cleanly.

### Measured

`examples/04_aos_vs_soa.cpp` (this folder — `max(bid)`, GCC 15.1, `-O2`, N=4M):
```
  AoS (stride 32 B) : ~441 ms
  SoA (contiguous)  : ~283 ms      -> SoA ~1.6x faster
```

`09-ARRAYS/examples/07_aos_vs_soa.cpp` (single-field read-modify-write, no
reduction dependency):
```
  SoA ~4x faster
```

**The magnitude depends on the operation** (a reduction has a data dependency
that limits both; a plain RMW is pure bandwidth → the gap is bigger) — but SoA
is consistently faster for field-subset access. Measure your actual kernel.

### Whole-ENTITY access → AoS wins

"For entity `i`, do something with all its fields" (e.g. "apply a fill to order
`i`", "render particle `i`").

- **AoS**: entity `i`'s fields are together — one or two cache lines, one load
  brings them all.
- **SoA**: entity `i` needs one element from each of 7 arrays — **7 separate cache
  lines**, 7 potential misses.

---

## Decision

| Access pattern | Layout |
|---|---|
| Scan many entities, touch **a few fields** (analytics, filtering, SIMD math) | **SoA** |
| Process **one entity at a time, all its fields** | **AoS** |
| Mixed / unsure | AoS (simpler); profile; consider **AoSoA** (hybrid: array of small SoA blocks) |
| Small data (fits in L1/L2) | Doesn't matter much |

Also: struct **size** (padding, file 05) amplifies both — a bloated AoS struct
wastes more per line; reorder members first.

---

## SoA ergonomics

SoA loses the "one object" convenience. Helpers:

```cpp
struct QuotesSoA {
    std::vector<int64_t> ts;
    std::vector<int32_t> bid, ask;
    std::size_t size() const { return ts.size(); }
    void push(int64_t t, int32_t b, int32_t a) { ts.push_back(t); bid.push_back(b); ask.push_back(a); }
    // a "reference proxy" for entity i:
    struct Ref { int64_t& ts; int32_t& bid; int32_t& ask; };
    Ref operator[](std::size_t i) { return { ts[i], bid[i], ask[i] }; }
};
```

C++ has no built-in SoA container. Libraries: `boost::pfr` + custom, EnTT
(game ECS), or hand-rolled. `std::experimental::simd` for the math.

---

## AoSoA — the hybrid

```cpp
struct QuoteBlock {                 // 8 quotes, SoA within the block
    std::array<int64_t, 8> ts;
    std::array<int32_t, 8> bid, ask, bidSz, askSz;
};
std::vector<QuoteBlock> blocks;     // array of SoA blocks
```

Each block fits in a few cache lines and vectorizes (8-wide), while a whole
entity is still local-ish (within one block). Used in high-end SIMD code and some
HFT feed handlers.

---

## Andar kya hota hai

- **AoS single-field scan**: strided loads, `hits_per_line = 64 / sizeof(struct)`.
  Effective bandwidth = actual bandwidth × `field_size / stride`. Vectorizer needs
  gather (slow, `-O3`/`-march=native` only).
- **SoA scan**: sequential loads at full bandwidth, prefetcher streams, vectorizer
  emits packed loads + a SIMD reduction.
- **SoA whole-entity**: one load from each array — the addresses are far apart →
  no spatial locality, TLB pressure if many fields.
- Struct padding in AoS inflates the stride → worse. (File 05.)

> **HFT relevance:** Order books and market-data analytics are **SoA / column
> layout**: `std::array<int64_t, Depth> prices; std::array<int64_t, Depth> qtys;
> ...` — scanning "all prices" or "all qtys" touches contiguous memory and
> vectorizes. Per-order processing (match, fill) uses AoS-style order records.
> Feed handlers sometimes go AoSoA for SIMD decode. The layout choice is made
> per hot kernel, measured, and reviewed. Folders 32, 36, 39, 43.

---

## Hands-on

```bash
./build.ps1 fast 11-STRUCTS/examples/04_aos_vs_soa.cpp                    # max(bid): ~1.6x
./build.ps1 fast 09-ARRAYS/examples/07_aos_vs_soa.cpp                     # RMW: ~4x
# both at -O3 -march=native too -- gap changes
```

---

## ⚠️ Traps

### Trap 1 — AoS for a field-subset hot scan
```cpp
for (auto& q : quotes) sum += q.bid;   // ⚠️ 7/8 of each line wasted. SoA
```

### Trap 2 — SoA for whole-entity processing
```cpp
for (i) match(soa.px[i], soa.qty[i], soa.id[i], soa.side[i], ...);   // ⚠️ N cache misses per entity
```

### Trap 3 — SoA arrays getting out of sync
```cpp
bid.push_back(b);   // forgot ask.push_back(a) -> arrays different lengths -> bug
```
(Wrap in a `push()` helper that updates all arrays.)

### Trap 4 — micro-optimizing layout for tiny data
Fits in L1 → AoS vs SoA barely matters. Profile first.

### Trap 5 — ignoring struct padding in AoS
A 40-byte struct that should be 24 → 1.67x worse AoS scan. Reorder (file 05) before
reaching for SoA.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "SoA is always faster" | Only for field-subset access; AoS wins whole-entity |
| "AoS vs SoA is a 10x thing always" | 1.5x–8x depending on the kernel — measure |
| "SoA is just a code style" | Different memory layout → different cache behaviour |
| "Struct size doesn't matter for AoS" | Padding inflates the stride (file 05) |
| "C++ has an SoA container" | No — hand-rolled or a library |

---

## Exercises

1. **Both benchmarks:** run this folder's `04_aos_vs_soa.cpp` (max reduction) and
   folder 09's `07_aos_vs_soa.cpp` (RMW). Why is the RMW gap bigger?

2. **Whole-entity task:** add a task to `04_aos_vs_soa.cpp` — "for each quote,
   `mid = (bid + ask) / 2`, accumulate `sum(mid)`". Which layout wins now?

3. **Padding × AoS:** make `Quote` bloated (bad member order, +20 bytes padding).
   Re-run the AoS scan. Slower? By how much (vs the 32-byte version)?

4. **SoA helper:** implement `QuotesSoA` with `push()`, `size()`, and a `Ref`
   proxy `operator[]`. Use it in a whole-entity loop.

5. **AoSoA:** implement `QuoteBlock` (8-wide) + `std::vector<QuoteBlock>`. Scan
   `bid` across all blocks. Time vs pure AoS and pure SoA.

6. **`-O3 -march=native`:** run all layouts at `-O2` and `-O3 -march=native`. Does
   the compiler's AoS gather close the gap?

---

## Interview questions

1. AoS vs SoA — memory layout ka fark?
2. Field-subset access pe SoA kyun jeet-ta hai (cache line math)?
3. Whole-entity access pe AoS kyun better?
4. AoS/SoA ka speedup itna vary kyun karta hai (reduction vs RMW)?
5. Struct padding AoS performance ko kaise affect karta hai?
6. AoSoA kya hai, kab use hota hai?

---

## Next
→ [`08-unions.md`](08-unions.md)
