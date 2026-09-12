# 09 — Fixed-point arithmetic: prices as integers

## Prerequisites
- `08-eliminating-false-sharing.md`
- `03-VARIABLES-DATA-TYPES/09-floating-point.md` (float representation)
- `37-HFT-FUNDAMENTALS/08-tick-size-and-lots.md`

## Yeh topic abhi kyun

Pipeline ka parse stage `std::stod` use karta tha — v0. v3 ne price ko
**int64, scale 100** kar diya. Yeh sabse important single change hai HFT
mein, aur speed se zyada **correctness** ka mamla hai.

---

## `double` prices — kyun landmine

`double` binary floating-point hai. `0.10` binary mein **exactly nahi**
banta (jaise `1/3` decimal mein nahi banta).

`05_fixed_point.cpp` measured:

```
double: 0.1 added 10x = 0.99999999999999988898   (== 1.0 ?  NO)
fixed : 0.10 added 10x = 1.00                     (== 1.00 ? yes)
```

Consequences:

| Problem | HFT impact |
|---|---|
| `price_a == price_b` silently false | "same price level" match nahi hua → order galat level pe |
| Accumulation drift (`sum += px` loop) | VWAP / notional / P&L slowly galat |
| Non-associative: `(a+b)+c != a+(b+c)` | Do code paths, same inputs, alag result → non-deterministic feel |
| Rounding at tick boundary | `100.005` → `100.00` ya `100.01`? Exchange reject |
| NaN / Inf propagation | Ek bad divide → poora book NaN, silently |
| Denormals slow | Bahut chhote numbers pe 10-100× slowdown (FTZ/DAZ off ho to) |
| Compare/convert cost | `ucomisd` + branch; int `cmp` sasta aur exact |

---

## Fixed-point — price = integer × scale

```cpp
using Px = std::int64_t;
constexpr std::int64_t kScale = 100;    // equity tick 0.01 -> scale 100

// "100.05" == 10005
```

Scale ka chunav:

| Instrument | Tick | Scale | Note |
|---|---|---|---|
| US equity | $0.01 | 100 | `int64` mein $92 trillion tak headroom |
| US equity sub-penny / options | $0.0001 | 10000 | |
| FX | 0.00001 (pip/10) | 100000 or 1e7 | pairs vary |
| Crypto | varies wildly | 1e8 (satoshi) common | |
| Futures | contract-specific | exchange spec se | kabhi non-decimal (32nds for bonds!) |

**`int64` headroom:** max ≈ 9.22 × 10¹⁸. Scale 1e8 pe bhi ≈ 9.2 × 10¹⁰
price units = 92 billion. Kaafi. `int32` (2.1e9) — scale 100 pe max
21M — equity ke liye theek par tight; **`int64` default rakho**.

---

## Parse — bina float ("PPPP.pp" → int64)

```cpp
static Px parse_px(const char* s, std::size_t len) {
    Px whole = 0; std::size_t i = 0;
    for (; i < len && s[i] != '.'; ++i) whole = whole * 10 + (s[i] - '0');
    Px frac = 0;
    if (i < len && s[i] == '.')
        frac = (s[i+1] - '0') * 10 + (s[i+2] - '0');   // exactly 2 digits
    return whole * kScale + frac;
}
```

Koi `std::stod`, koi locale, koi rounding mode. `pipeline.hpp` V3 ka parse
isi shape ka hai (`.` dhoondho, integer part, 2 frac digits).

Real feed: fields aksar **already integer** hote (ITCH price = uint32
scaled 1e4, `38/06`). Toh "parse" = ek `ntohl` / `memcpy` — float ka
sawal hi nahi. Text feeds (kuch FIX) pe upar wala parser.

---

## Format — int64 → "PPPP.pp"

```cpp
static int format_px(Px p, char* buf) {
    return std::snprintf(buf, 24, "%lld.%02lld",
                         (long long)(p / kScale), (long long)(p % kScale));
}
```

Round-trip verified (`05_fixed_point.cpp`): `"12345.67"` → `1234567` →
`"12345.67"` ✓.

---

## Arithmetic — scale discipline

| Operation | Result scale | Note |
|---|---|---|
| `px_a + px_b`, `px_a - px_b` | same (100) | direct |
| `px_a < px_b`, `== ` | — | **exact**, deterministic |
| `px * qty` (qty unitless int) | same (100) | notional in scale-100 → `/100` for dollars |
| `px_a * px_b` | **10000** | dob ara `/kScale` chahiye |
| `px / n` (n = int divisor) | same, but **rounding!** | truncates; banker's/half-up chahiye to explicit |
| mid = `(bid + ask) / 2` | same | `/2` = `>> 1` (lossy on odd sum — decide policy) |

`05_fixed_point.cpp`:
```
100.05 * 250 = 2501250 (scaled)  = "25012.50"       // scale stayed 100
```

**Har multiply ke baad scale track karo** — comment mein likho, ya ek
strong typedef (`struct Px { int64_t raw; };` with operators jo scale
enforce karein).

---

## Speed — bonus, not the point

`05_fixed_point.cpp` (is box):
```
int64  add+compare : ~0.85 ns/elem
double add+compare : ~1.05 ns/elem
```

~20% — chhota, aur `-O2` pe dono vectorize ho sakte to aur kam. **Asli
jeet: correctness + deterministic `==` + no NaN + no drift.** Agar koi
"fixed-point 5× faster" bole — woh division/transcendental-heavy code hoga,
ya un-vectorized scalar float. Plain add/compare pe farak modest.

Jahan fixed-point **sach mein** bahut tez: jab float hone se pura loop
**vectorize nahi** hota (float non-assoc → compiler `-O2` pe reduction
reorder nahi karta bina `-ffast-math`), par integer add associative hai →
auto-vectorizes.

---

## Kab float **theek** hai

- **Statistics** jo P&L / orders ko directly nahi chhuti: volatility
  estimate, correlation, model score. Precision-critical nahi, ranges wide.
- **Ratios / weights** jinke chhote errors OK.
- Kabhi bhi price, quantity, notional, position, fee — **NAHI**. Woh exact
  hone chahiye.

Rule: agar number kisi **order field** mein jata ya **money** hai → integer.
Warna float acceptable.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — scale mismatch add karna
`px (scale 100) + fee (scale 10000)` → garbage. Ek jagah normalize karo
(sab scale 1e4), ya types se enforce.

### Trap 2 — `px_a * px_b` ke baad `/kScale` bhoolna
Notional-of-notional, ya price×price kahin — scale 10000 ho gaya, aage
sab 100× off. Silent.

### Trap 3 — division se truncation, rounding policy undefined
`(bid + ask) / 2` odd sum pe niche truncates. VWAP `total_notional /
total_qty` bhi. Decide: truncate / round-half-up / round-half-even — aur
**consistent** rakho (exchange se match karo).

### Trap 4 — `int32` scale se overflow
`int32` scale 1e4, price $250000 → 2.5e9 > INT32_MAX (2.1e9) → wrap →
negative price. `int64` use karo. Crypto pe `int64` bhi tight ho sakta
(scale 1e8 × large supply) — `__int128` ya careful scaling.

### Trap 5 — float se parse karke phir int mein convert
`(int64)(std::stod(s) * 100)` — `std::stod` ne pehle hi rounding kar di,
`* 100` ne aur. `99.99` → `9998` ho sakta. **Text ko seedha integer parse
karo**, float ke through mat jao.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `double` prices fine hain agar 2-decimal round karta hoon | Round karne ke liye bhi float math — drift + `==` fail |
| Fixed-point = performance optimization | Primarily **correctness**; speed bonus modest |
| `(int)(price * 100)` = fixed-point conversion | Float rounding do baar; parse integer directly |
| Ek scale sab jagah kaam karega | Per-instrument tick size; multi-asset system mein explicit |

---

## Hands-on

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/05_fixed_point.cpp
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/04_before_after.cpp   # v0(double) vs v3(fixed) agreement
```

---

## Exercises

1. `05_fixed_point.cpp` mein `parse_px("100.005", 7)` call karo (3 frac
   digits). Kya milega, kyun?
   <details><summary>Answer</summary>
   Parser `s[i+1], s[i+2]` = `'0','0'` padhta → frac 0 → `10000`. Teesra
   digit `'5'` ignore. Yeh parser **exactly 2** frac digits assume karta
   (equity). 3-digit venue ke liye scale 1000 + 3-digit read. Input
   validation real handler mein zaroori.
   </details>

2. `bid = 10005` (100.05), `ask = 10006` (100.06). `mid = (bid+ask)/2` =
   ? Dollar mein? Problem?
   <details><summary>Answer</summary>
   `20011 / 2 = 10005` (truncated) = $100.05. True mid $100.055 —
   representable nahi scale-100 pe. Policy: (a) truncate (jo hua), (b)
   scale badhao (1000) mid ke liye, (c) `(bid+ask)` ko hi rakho (2×mid)
   aur comparisons double karo — `pipeline.hpp` V3 yahi karta (`sum2`).
   </details>

3. `04_before_after.cpp` mein v0 (double signal) aur v3 (integer signal)
   110/110 agree karte. Kya guarantee hai ki hamesha karenge?
   <details><summary>Answer</summary>
   Nahi — guarantee nahi. v3 ka integer formula v0 ka **algebraic
   equivalent** hai (`sum2*W*kDen > ring_sum*kNum`), par v0 float mein
   `sum`/`sma` compute karta → rounding. Threshold ke bilkul boundary pe
   (`mid ≈ sma*ratio` within ~1e-12) float idhar-udhar flip kar sakta.
   Is workload pe aisa tick nahi aaya. Aaye to report karo — woh khud
   float-ki-fragility ka demo hai.
   </details>

---

## Interview questions

1. `double` price ke 4 concrete failure modes (== , drift, assoc, NaN)?
2. Scale kaise chunte ho? `int32` vs `int64` — kab konsa?
3. `px_a * px_b` ka result scale? Aage kya karna?
4. Text `"99.99"` ko fixed-point mein — `std::stod` se kyun **nahi**?
5. Kab float acceptable hai trading system mein?

---

## Next
→ [`10-avoiding-division.md`](10-avoiding-division.md)
