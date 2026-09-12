# 04 — Market orders: sweeping levels, partial fills

## Prerequisites
- `03-limit-orders.md`

## Yeh topic abhi kyun

Market order Limit se ek fundamental tareeke se alag hai: **koi price
limit hi nahi**. Yeh simplicity DEADLY bhi ho sakti (slippage, 37/04) —
aur is folder ke engine mein ek DESIGN DECISION maangti hai jo har venue
different tareeke se leta.

---

## Match_against mein Market ka special-case

```cpp
// matching_engine.hpp
if (incoming.type != OrderType::Market) {
    const bool crosses = ...;
    if (!crosses) break;
}
// Market -- yeh IF poora SKIP ho jaata, matlab hamesha "crosses" maana
// jaata, jab tak opposite_side khaali na ho
```

Ek Market order **price check hi nahi karta** — sirf `incoming.qty > 0`
aur `opposite_side` khaali nahi, tab tak sweep karte jaata, best-price-
se-worst-price levels ke through.

---

## DESIGN DECISION: unfilled remainder ka kya hota

Real exchanges is par ALAG policies rakhte:
- Kuch: jitna mile fill karo, baaki **cancel** (yeh option, jaisa IOC)
- Kuch: agar POORA fill na ho sake, POORA reject karo (all-or-none)
- Kuch: baaki **book mein daal do** (par tab woh "market order resting"
  hai — confusing concept, kam hi use hota)

**Hamara engine ka decision:** jitna mile fill karo, baaki **VOID** (kabhi
rest nahi hota) — effectively Market = "IOC without a price limit."

```cpp
// submit() -- Market ka remainder yahan girta:
return {OrderStatus::Cancelled, std::move(trades)};   // Market != Limit
```

Yeh choice document karna ZAROORI hai (jaisa yahan explicit likha) —
"market order kaisi behave karta jab liquidity kam ho" ek genuine
ambiguity hai jahan tumhe apna design explicitly state karna chahiye,
kyunki different venues genuinely different answers dete hain.

---

## Sweep example -- thin book, slippage

```
Book asks: 100 x30, 105 x400
Market BUY x1000:
  100 x30 -> fill 30 (price 100)
  105 x400 -> fill 400 (price 105)
  Book ab khaali (asks side) -- loop rukta (opposite_side empty)
  incoming.qty ab bhi 570 baaki -- VOID (Cancelled status)
  TOTAL filled = 430, avg price ~104.79 (weighted)
```

Yeh `02_order_types.cpp`'s scenario #2 hai — book mein sirf 430 tha,
1000 maanga, 430 mila, baaki void.

> **HFT relevance:** 37/04 ka slippage-warning yahan CODE mein concrete
> hota. Ek thin book pe bada market order chalana = tumhare hi trade ne
> price ko "walk" kara diya (worse aur worse levels consume kiye) — real
> systems isi liye market orders ko chhoti size mein use karte ya
> algo-slicing (bade order ko chhote pieces mein todna, time ke saath) —
> woh separate strategy-layer ka kaam hai, matching engine ka nahi.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — Market order ko rest karne ki koshish
Agar tum galti se Market order ko `add_resting()` bhej do (jaisa Limit
ke liye hota), ek "price-less resting order" book mein baith jaata — jab
best_bid()/best_ask() query hoti, uski price 0 (ya kuch bhi garbage) ho
sakti, poori book corrupt ho jaati. `submit()` mein explicit
`incoming.type == OrderType::Limit` check ISI ko rokta hai.

### Trap 2 — Market order ki price field use karna
`make_market()` factory `price=0` set karta — is field ko KABHI read
nahi karna chahiye Market order ke liye (crossing check hi skip ho jaata
`match_against` mein). Agar koi code accidentally `incoming.price` use
kar le Market order pe, silent bug (0 ek "valid-looking" price hai,
crash nahi karega, bas galat result dega).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Market order guaranteed POORA fill hota | Sirf jitna book de sake, baaki void (thin book mein partial ya zero fill) |
| Market order price-less hone ka matlab "free" hai | Slippage risk sabse zyaada isi type mein (worst realistic price impact) |
| Unfilled remainder book mein "kahin" reflect hota | Kahin nahi -- poori tarah VOID, jaise woh maanga hi nahi gaya tha |

---

## Hands-on

```bash
./build.ps1 fast 40-MATCHING-ENGINE/examples/02_order_types.cpp
```

---

## Exercises

1. Book bilkul khaali hai (koi resting order nahi). Ek Market BUY x100
   submit hota. Kya hota?
   <details><summary>Answer</summary>
   `match_against` ka `while` loop turant `opposite_side.empty()` pe
   khatam ho jaata (0 iterations). `incoming.qty` abhi bhi 100 hai --
   status Cancelled, ZERO trades. Poora order silently void ho jaata.
   </details>

2. Kyun Market order ka design-decision (partial-fill-then-void) explicitly
   likhna zaroori tha, "obviously right" kyun nahi maan sakte?
   <details><summary>Answer</summary>
   Kyunki real venues yahan genuinely alag behave karte -- kuch all-or-
   none reject karte, kuch partial allow karte. "Obviously right" jaisa
   kuch nahi hai yahan, sirf ek reasonable, DOCUMENTED choice hai. CLAUDE.md
   ka Rule 4 -- har claim verifiable/explicit ho, assumption chhupi na ho.
   </details>

---

## Interview questions

1. Market order price check kyun skip karta match_against mein?
2. Market order ka unfilled remainder ka kya hota, aur yeh venue-specific
   decision kyun hai?
3. Thin book pe bada market order chalane ka risk (slippage) explain karo.

---

## Next
→ [`05-partial-fills.md`](05-partial-fills.md)
