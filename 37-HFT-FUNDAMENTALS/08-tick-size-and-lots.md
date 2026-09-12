# 08 — Tick size, lot size, price bands

## Prerequisites
- [`07-price-time-priority.md`](07-price-time-priority.md)

## Yeh topic abhi kyun
Yeh woh **hard constraints** hain jo har order ko follow karni hoti — aur
yeh bhi confirm karte hain ki price ko integer ticks mein rakhna (06 ka
Trap 2) sirf ek engineering choice nahi, **exchange ka fundamental rule**
hai.

---

## Tick size — price ki minimum increment

> **Tick size = sabse chhota price difference jo do valid prices ke beech
> allowed hai.**

```
Agar tick size = 0.05:
  100.00  -> valid
  100.05  -> valid
  100.03  -> INVALID (0.05 ka multiple nahi)
```

Tick size **instrument ke hisaab se alag** hota — high-price stocks ka
tick bada ho sakta (relative terms mein chhota), low-price stocks ka
chhota, aur kuch venues price-band ke hisaab se tick size **badalte** bhi
hain (jaise price jitni zyada, tick utna bada — taaki bps mein roughly
consistent rahe). **Exact values apne venue ke current spec se lo** — yeh
rules bhi time ke saath revise hote.

**Yehi wajah hai price integer ticks mein rakha jaata (06):** agar price
sirf tick-multiples le sakta hai, to price ko **tick-count** (ek integer)
ke roop mein store karna natural hai — `price_in_ticks = 2010` matlab
`100.50` agar tick=0.05. Yeh floating-point rounding ka poora sawaal hi
khatam kar deta.

---

## Lot size — minimum tradeable quantity

> **Lot size = minimum quantity jiske multiple mein order place ho sakta.**

```
Agar lot size = 1 (equity, zyaadatar cash market):
  qty = 1, 2, 3, ... sab valid

Agar lot size = 25 (kuch derivatives contracts):
  qty = 25, 50, 75 valid
  qty = 10, 30 INVALID
```

Cash equity markets mein zyaadatar lot size = 1 (single share tradeable)
hota; derivatives (futures/options) mein contract-level lot sizes common
hain (venue/instrument-specific, verify karo).

---

## Price bands — daily move ka circuit limit

> **Price band = ek range (typically previous close ke ± kuch %) jiske
> bahar order accept hi nahi hota.**

```
Previous close: 100.00
Band: ±10% (illustrative)
  Valid order price range: 90.00 - 110.00
  Order at 111.00 -> REJECTED by exchange (band se bahar)
```

Kai venues **circuit breakers** bhi rakhte — agar price band tak pahunch
jaaye, trading temporarily halt ho sakti (poore market ya us instrument ke
liye), taaki extreme volatility mein orderly trading bani rahe. Exact
percentages, dynamic vs static bands, aur halt rules **venue aur
instrument-category specific** hain — apne broker/exchange ki current spec
dekho, yeh regularly revise hoti hai.

---

## Minimum quantity — order ka floor

Kuch order types (04 ka "minimum-quantity" variant, ya kuch venues ka
default) ek minimum fill quantity specify karne dete — chhota partial fill
avoid karne ke liye (chhota fill kabhi-kabhi transaction cost/processing
overhead ke hisaab se worth nahi hota).

---

## Yeh sab HFT system ke liye kya matlab rakhte hain

| Constraint | System pe impact |
|---|---|
| Tick size | Price internally **integer ticks** mein represent karo, `double` nahi (06) |
| Lot size | Qty validation **pehle apne system mein** karo (exchange reject se pehle hi pakdo — 13-risk-systems) |
| Price band | Order price ko band ke against pre-trade check karo (13) — band-violating order bhejna sirf latency waste hai |
| Min qty | Order-slicing logic (agar bade order chhote pieces mein todte ho) ko min-qty se compatible rakho |

Yeh sab ek badi theme ka part hain: **"exchange reject hone se pehle,
tumhara khud ka system invalid order ko pehchan le"** — kyunki reject bhi
ek round-trip hai (latency waste), aur galat order bhejna risk (13) hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — hardcode tick size ek "universal" number
Tick size instrument-ke-hisaab-se, kabhi price-band-ke-hisaab-se badalta.
Ek fixed constant poore system mein use karna galat orders generate
karega jaise hi ek naya instrument add ho.

### Trap 2 — price ko round karna bina tick-check ke
`round(price, 2)` (2 decimal places) tick=0.05 ke against valid nahi hai —
tumhe **tick ke multiple** tak round/snap karna hai, decimal places tak
nahi.

### Trap 3 — price band ko "kabhi hit nahi hoga" maan lena
Volatile events (news, circuit-triggering moves) mein bands hit hoti hain
real trading mein. System ko yeh case gracefully handle karna chahiye
(order reject, retry-with-adjusted-price, ya bas skip logic) — crash nahi
hona chahiye.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Tick size sab instruments mein same hai | Instrument/price-band ke hisaab se badalta, venue spec check karo |
| Price ko decimal places tak round karna kaafi hai | Tick ke multiple tak snap karna chahiye |
| Price band rarely hit hoti, ignore kar sakte | Volatile events mein real hoti hai, handle karo |
| Lot size sirf ek "suggestion" hai | Hard exchange-enforced constraint hai |

---

## Exercises

1. Tick size = 0.05, ek strategy price 100.07 compute karti (ek internal
   calculation se). Kya problem hai, aur fix kya hoga?
   <details><summary>Answer</summary>
   100.07, 0.05 ka multiple nahi (0.05, 0.10, ..., 100.05, 100.10 — 100.07
   invalid) — exchange reject karega. Fix: price ko nearest valid tick tak
   snap karo (round to nearest multiple of tick_size, side ke hisaab se —
   buy order ke liye typically round DOWN taaki tum better/equal price se
   zyada na chuko, sell ke liye round UP — exact convention venue/strategy
   pe depend karta).
   </details>

2. Kyun price ko `price_in_ticks` (integer) ke roop mein store karna,
   `double` mein rupees store karne se, tick-validity check karna EASIER
   banata?
   <details><summary>Answer</summary>
   Agar price already tick-count hai (integer), to "valid hai kya" check
   trivial hai — koi bhi integer valid hai (tick granularity already
   baked in structure mein, invalid state represent hi nahi ho sakta).
   `double` rupees mein, har baar `price / tick_size` ka remainder-check
   (floating-point mod, jo khud rounding-error-prone hai) karna padta.
   </details>

---

## Interview questions

1. Tick size, lot size, price band — teeno ka fark batao.
2. Price ko integer ticks mein represent karna kis specific bug class ko
   avoid karta hai?
3. Pre-trade risk check mein tick/lot/band validation kyun **apne system**
   mein karni chahiye, sirf exchange pe depend kyun nahi karna chahiye?

---

## Next
→ [`09-market-makers-and-takers.md`](09-market-makers-and-takers.md)
