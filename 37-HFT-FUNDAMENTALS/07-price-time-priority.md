# 07 — Matching rules: price-time priority vs pro-rata (measured)

## Prerequisites
- [`06-order-book-concept.md`](06-order-book-concept.md)
- `examples/04_matching_rules.cpp`

## Yeh topic abhi kyun
Matching rule hi decide karta hai **speed ka actual edge kya hai**. Yeh
poori course ka "speed kyun important hai" ka mathematically precise
answer hai — alag rule mein speed ka faayda alag hota.

---

## Price priority (dono rules mein common)

Har rule ka pehla criterion same hai: **better price hamesha pehle match
hoti.** Best bid pehle sell orders se match hota, best ask pehle buy orders
se — price ke andar tie-break ke liye hi rules alag hote hain.

---

## Rule 1 — Price-time priority (FIFO)

**Sabse common** (NSE, Nasdaq, aur zyaadatar equity/most instrument
exchanges ka default). Same price pe: **jo pehle aaya, use pehle fill.**
Ek order **poora** fill hota agle ko kuch milne se pehle.

```cpp
// match_fifo() se — 04_matching_rules.cpp
for (auto& o : book) {              // arrival order mein
    if (incoming_qty <= 0) break;
    const long long fill = std::min(o.qty, incoming_qty);
    fills.push_back({o.id, fill});
    incoming_qty -= fill;
}
```

---

## Rule 2 — Pro-rata

Kuch venues (typically kuch derivatives/futures/options books) is model ka
use karte: fill **size ke proportion** mein baantta, arrival time se
(zyaadatar) farq nahi padta.

```
fill_i = floor(incoming * qty_i / total_resting_qty)
```

Rounding se total thoda kam ho sakta — leftover ek convention se allocate
hota (`04_matching_rules.cpp` mein: sabse pehle-aaye order ko).

---

## Measured: same book, same incoming order, do alag results

```
Resting bids (arrival order):
  order 101  qty=100  arrived #1
  order 102  qty=50   arrived #2
  order 103  qty=200  arrived #3
  order 104  qty=75   arrived #4
  total resting qty = 425

Incoming SELL, qty=180 (crosses, partially eats the level)

--- FIFO ---                        --- PRO-RATA ---
  order 101  filled 100                order 101  filled 44
  order 102  filled 50                 order 102  filled 21
  order 103  filled 30                 order 103  filled 84
  (order 104: 0)                       order 104  filled 31
  total = 180                          total = 180
```

**FIFO**: 101 aur 102 **poora** fill hote (pehle aaye), 103 sirf partial,
**104 ko kuch nahi milta** — chahe uski size (75) 102 (50) se bhi zyaada
ho, kyunki woh **baad mein** aaya.

**Pro-rata**: sab 4 orders ko unki **size ke proportion** mein hissa milta
— 104 (sabse baad aaya) ko bhi 31 milta, kyunki uski size thi. Order 103
(sabse badi resting size, 200) ko sabse zyada fill milta (84) — arrival
time se farq nahi padta.

---

## Iska matlab: speed ka edge kahan hai, yeh RULE decide karta hai

| Rule | Speed ka edge | Size ka edge |
|---|---|---|
| **FIFO** | **Bahut bada** — 1 microsecond pehle order bhejna = poori tarah agli line mein | Chhota — bade order bhi FIFO order mein hi lagte |
| **Pro-rata** | **Kam** — thoda pehle/baad aane se allocation nahi badalta | **Bada** — badi size bhejna directly zyada fill deta |

Isliye:
- **FIFO markets** mein latency arbitrage (10) ka edge bahut bada hai —
  microseconds matter karte kyunki "sabse pehle line mein aana" hi jeet
  hai.
- **Pro-rata markets** mein size/capital allocate karna zyada matter karta,
  raw speed utna nahi (chahe fir bhi thodi zaroorat rehti — quote update
  karne, adverse selection avoid karne ke liye — sirf allocation ka edge
  kam hota).

> **HFT connection:** yeh lesson bataata hai "kitna fast hona kaafi hai"
> venue-dependent hai. Same firm, alag venues pe alag latency budgets
> justify kar sakti — pro-rata book pe extreme speed spend karna kam ROI
> de sakta FIFO book ke comparison mein.

---

## Hybrid: pro-rata + top priority

Kuch venues dono ko mix karte — jaise "**time priority niche threshold
tak**, uske upar pro-rata" ya "top order (sabse pehla) ko ek chhota
guaranteed minimum fill, baaki pro-rata." Exact rule **venue-specific hai**
— live trading se pehle uske exchange rulebook/spec padhna zaroori hai;
yahan sirf concept samjhaya hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sab exchanges FIFO maan lena
Galat assumption se strategy design galat hoga — pro-rata book pe pure
speed-focused strategy underperform kar sakti agar size allocation ka game
alag hai.

### Trap 2 — pro-rata mein rounding ignore karna
Integer division se total thoda kam allocate hota — remainder handling
(kisko extra milta) khud ek rule hai jo venue define karti, guess nahi
karni.

### Trap 3 — "modify" ko FIFO mein free samajhna
FIFO mein price change (ya kabhi qty-increase) typically **naya time-
priority** deta (cancel+new). Isliye ek existing order ko "upgrade" karne
ka apna cost hai — 06 mein already noted.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Sab exchanges FIFO use karte | Kuch venues pro-rata ya hybrid use karte |
| Pro-rata mein speed bekaar hai | Kam important hai, bilkul zero nahi (quoting/adverse-selection ke liye phir bhi chahiye) |
| FIFO mein size matter nahi karti | Matter karti hai, par arrival time pehle decide karta |
| Rounding remainder "auto-fair" allocate hota | Venue-specific convention hai, verify karo |

---

## Exercises

1. Ek venue FIFO use karta. Tumhari firm ke paas best signal hai par
   network thoda slow hai; competitor ka signal thoda weak hai par network
   sabse fast hai. Kaun jeetega zyaadatar fills?
   <details><summary>Answer</summary>
   FIFO mein "pehle order bhejna" hi jeet hai — agar competitor consistently
   pehle pahunch raha (fast network), woh zyaadatar fills lega chahe unka
   signal weaker ho. Yeh exactly FIFO ki property hai jo latency arbitrage
   (10) ko itna valuable banaati.
   </details>

2. Same do firms, par venue ab **pro-rata** use karta. Ab kaun advantage
   mein hai?
   <details><summary>Answer</summary>
   Ab size/capital allocation zyaada matter karta arrival-order-speed se.
   Strong signal wali firm (agar woh bigger size confidently quote kar
   sakti) zyaada fill le sakti chahe network thoda slow ho — speed ka edge
   compress ho jaata pro-rata mein.
   </details>

---

## Interview questions

1. FIFO aur pro-rata matching ka exact fark, code-level.
2. Kaunse type ke venue mein latency ka edge sabse zyada hota, aur kyun?
3. Pro-rata mein rounding leftover kaise handle hota (concept)?
4. "Modify" order FIFO mein time-priority pe kya asar daalta hai?

---

## Next
→ [`08-tick-size-and-lots.md`](08-tick-size-and-lots.md)
