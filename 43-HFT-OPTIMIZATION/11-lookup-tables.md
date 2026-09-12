# 11 — Lookup tables: precompute vs recompute, and the cache trade-off

## Prerequisites
- `10-avoiding-division.md`
- `21-TEMPLATES/*` (`constexpr`), `32-CACHE-MEMORY-PERFORMANCE/*`

## Yeh topic abhi kyun

`10` mein ek option chhoda tha: division / mehnga computation ko **precompute
karke table mein rakho**. Yeh powerful hai par ek chhupi cost hai — table
khud cache mein jagah leti aur tumhare **data** ko evict kar sakti.

---

## Idea

Agar `f(x)` mehnga hai **aur** `x` ka domain chhota + finite hai:

```cpp
// recompute:
int level = (price - base) / tick_size;      // div, har call

// precompute:
static const int level_of[kMaxPriceUnits] = { /* ... */ };
int level = level_of[price - base];          // ek load
```

Ek load (~4 cyc L1 hit) vs ek `div` (~20 cyc). ~5× — jab table L1/L2 mein
rehti.

**Classic HFT table uses:**
- price → price-level index (agar tick math non-trivial)
- symbol id → per-symbol config struct pointer
- message type byte → handler index / size
- CRC / checksum table (ITCH/SBE frame validation) — `constexpr` generated
- `popcount` / `clz` fallback (agar hardware instr na ho)
- bit-reverse, gray code, small `exp`/`log` for score math
- ASCII digit-pair → 2-digit int (`"42"` → 42 in one load) for fast itoa/atoi

---

## `constexpr` table generation (C++20)

Runtime pe fill mat karo — compile time pe:

```cpp
#include <array>
consteval auto make_crc_table() {
    std::array<std::uint32_t, 256> t{};
    for (int i = 0; i < 256; ++i) {
        std::uint32_t c = static_cast<std::uint32_t>(i);
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        t[static_cast<std::size_t>(i)] = c;
    }
    return t;
}
inline constexpr auto kCrcTable = make_crc_table();   // in .rodata, zero runtime init
```

`consteval` / `constexpr` loops (C++14+ relaxed, C++20 fuller) se table
binary mein **baked** — koi startup cost, koi race, `.rodata` (shared,
read-only, no false sharing).

---

## The cache trade-off — table bhi memory hai

| Table size | Fits in | Access cost | Risk |
|---|---|---|---|
| ≤ ~2 KB | L1d (32 KB) comfortably | ~4 cyc | negligible |
| ~2–32 KB | L1d, but competes with data | ~4 cyc (if hot) | evicts your hot data |
| 32 KB – 1 MB | L2 (~512 KB–1 MB) | ~12–14 cyc | slower than some recompute |
| > L2 | L3 / RAM | ~40+ cyc | **slower than `div`!** |

Agar table 256 KB hai aur tumhara hot working set already 400 KB hai —
table add karne se dono ab L2 se overflow → sab slow. **Net negative.**

**Rule:** table ko L1 mein rakho (< few KB), ya jaan lo ki woh L2-resident
rahegi aur recompute se sach mein tez hai. Measure — `06`'s `classify_table`
(`36/06`) ne 5-entry table pe ~12× dikhaya (trivially L1), par ek 1-MB
table ulta hoti.

---

## Kab table, kab nahi

**Table jeetta:**
- `f` genuinely mehnga (div, transcendental, multi-step)
- domain chhota (`< few thousand entries`) → table chhota
- access pattern hot (har tick)
- table read-only, shared (`.rodata`)

**Recompute jeetta:**
- `f` sasta (add, shift, single mul)
- domain bada → table bada → cache pressure
- access sparse/random over huge table → har access ek miss
- `f` ko compiler khud constant-fold / vectorize kar deta

---

## Is folder ke pipeline pe

`pipeline.hpp` V3 ne table **nahi** use ki — kyunki:
- `price → level` = `px_ticks - kBase` (ek subtract) — table se sasta,
  koi division nahi bachani thi.
- Signal threshold integer-algebra se division-free (`10`).

Yeh sahi call hai: table tabhi jab pehle koi **mehnga** op ho jise
precompute karke bachaya ja sake. Yahan tha hi nahi. **Har technique ka
apna trigger hota — table ka trigger "expensive f + small hot domain".**

Agar V3 ko non-uniform tick sizes handle karni hoti (`price < $1` → tick
$0.0001, `$1–$10` → $0.001, ...) — tab `price → level` non-trivial, aur
ek chhoti table (ya branch-tree) worth hoti.

> **HFT relevance:** ITCH/SBE feed handlers CRC/framing tables use karte
> (256 entries, 1 KB, permanently L1-hot). Options pricers small `exp`/`erf`
> tables + interpolation. Symbol dispatch: `symbol_id → &Config` direct
> array (dense ids) — ek load vs a hash lookup.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — table itni badi ki data evict ho gaya
Micro-benchmark mein table akela → L1 → 10× win. Production mein table +
book + order pool → L1 overflow → table access ab L2/L3 → slower than the
`div` you replaced. **Poore working set ke saath measure.**

### Trap 2 — runtime table init (static ctor)
`static std::array<...> t = compute();` — startup pe fill, aur agar
multiple TUs / threads → init-order / race issues. `constexpr`/`consteval`
→ compile-time, `.rodata`.

### Trap 3 — table index out of range
`level_of[price - base]` jahan `price < base` ya `price` huge → OOB read
(silent garbage ya crash). Clamp / validate index — especially feed se
aane wale values pe.

### Trap 4 — 2D table jab 1D + math kaafi
`table[symbol][level]` = `nsym * nlevel` entries, aksar huge + sparse.
`table[symbol]` (config) + `level = math(...)` chhota rehta.

### Trap 5 — table jise likha bhi jata (not read-only)
Agar table mutable hai aur multiple threads use karte → cache-line bounce
(`08`), aur woh ab `.data` mein (not shared `.rodata`). Read-only rakho.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Table hamesha tez ("ek load bas") | Sirf jab table cache-resident; badi table > `div` cost |
| Micro-bench mein 10× → production mein bhi | Poore working set ke saath re-measure (table + data compete) |
| Runtime `static` fill theek hai | `constexpr`/`consteval` → `.rodata`, zero startup, no race |
| Bada domain? Bada table bana lo | Bada table = cache killer; recompute ya smaller keying |

---

## Hands-on

```bash
# 36/06 ka classify: 5-entry table vs switch
./build.ps1 fast 36-LOW-LATENCY-CPP/examples/06_branchless.cpp
```

---

## Exercises

1. `f(x) = (x * 2654435761u) >> 20` (ek shift-mul hash) ko 65536-entry
   table se replace karna — faayda ya nuksaan?
   <details><summary>Answer</summary>
   Nuksaan. `f` = 1 mul + 1 shift (~4 cyc). Table = 256 KB (65536 × 4 B) →
   L2 resident best case (~13 cyc), random access → often L3/RAM (~40+).
   Aur 256 KB tumhare data ko evict karta. Recompute jeetta — `f` sasta hai.
   </details>

2. CRC-32 frame check har incoming ITCH message pe. Byte-at-a-time bit
   loop vs 256-entry table. Table cache mein kahan rahegi?
   <details><summary>Answer</summary>
   256 × 4 B = 1 KB. Har message use hoti → permanently L1-hot. Bit-loop =
   8 iterations/byte of shift+xor+branch. Table = 1 load + xor per byte,
   ~8× faster, aur 1 KB L1 se kuch meaningful evict nahi hota. Clear win —
   `constexpr` generate karo.
   </details>

3. Non-uniform tick sizes: 5 price bands, har band ka apna tick. `price →
   level`. Table, branches, ya kuch aur?
   <details><summary>Answer</summary>
   5 bands = 4 comparisons (branch tree, sorted, predictable — same band
   usually) → tick + reciprocal for that band (`10` D). Table sirf tab jab
   bands bahut zyada ho ya boundaries irregular. 5 bands: branches +
   per-band cached reciprocal sabse simple aur tez.
   </details>

---

## Interview questions

1. Lookup table ka faayda kab, aur uski hidden cost kya?
2. `constexpr`/`consteval` table generation — runtime `static` init se
   kyun behtar?
3. Table cache se overflow ho jaye to `div` se **slower** kaise ho sakti?
4. 2D table `[sym][level]` kyun aksar galat design?
5. Ek 1 KB CRC table permanently L1 mein — dusre data ka kya (evict)?

---

## Next
→ [`12-compile-time-strategy-dispatch.md`](12-compile-time-strategy-dispatch.md)
