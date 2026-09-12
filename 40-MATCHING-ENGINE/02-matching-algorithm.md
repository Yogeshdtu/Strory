# 02 — Price-time priority matching, crossing logic

## Prerequisites
- `01-what-is-matching.md`
- `37-HFT-FUNDAMENTALS/07-price-time-priority.md`

## Yeh topic abhi kyun

01 mein humne bola "matching engine decision leta." Yeh lesson EXACT
algorithm hai jo woh decision leta — step-by-step, code ke saath
(`matching_engine.hpp`'s `match_against()`).

---

## Crossing kya hai

Ek incoming BUY order aur ek resting ASK order **cross** karte hain agar
incoming ki price >= ask ki price hai (BUY "kam se kam itna dena" ready
hai, jitna ask maang raha). Symmetric: incoming SELL aur resting BID
cross karte agar incoming ki price <= bid ki price.

```
Resting ASK 100 x50
  Incoming BUY 100 x30  -> CROSSES (100 >= 100)
  Incoming BUY 99  x30  -> NAHI cross karta (99 < 100)
  Incoming BUY 105 x30  -> CROSSES (105 >= 100, aur BEHTAR bhi)
```

**Market orders** (04) ka koi price hi nahi hota -- woh HAMESHA cross
karte (jab tak opposite side khaali na ho).

---

## Algorithm -- step by step

```
function match_against(incoming, opposite_side):
    while incoming.qty > 0 AND opposite_side is not empty:
        level = opposite_side.best_level()        # best price pehle
        if incoming is NOT Market:
            if NOT crosses(incoming.price, level.price):
                break                               # ab koi aage cross nahi karega

        for order in level.orders (FIFO order):     # ARRIVAL order
            if incoming.qty == 0: break
            fill = min(incoming.qty, order.qty)
            incoming.qty -= fill
            order.qty    -= fill
            emit Trade(incoming, order, price=level.price, qty=fill)
            if order.qty == 0: remove order from level

        if level is empty: remove level from opposite_side
```

Do nested loops: **outer** price-levels ke through (best-to-worst,
jab tak cross kare), **inner** ek level ke andar FIFO orders ke through
(price-TIME priority — 37/07).

### Kyun "best level pehle, phir FIFO andar" — aur kuch nahi?

Yeh EXACT definition hai price-time priority ki: **price** pehle decide
karta kaun match hoga (behtar price walon ko pehle mauka), **time**
tabhi tie-break karta jab price SAME ho (jo pehle aaya, pehle fill).
Iske alawa koi order-preference nahi (size, participant, koi bhi cheez)
— yeh HFT ka sabse fundamental fairness-guarantee hai, aur isse violate
karna (jaise "size priority" — bada order pehle) bahut hi different market
banata (kuch exchanges pro-rata matching bhi use karte, par yeh course
sirf standard price-time cover karta).

---

## Multi-level sweep example

```
Book asks: 101 x20, 102 x20, 103 x20
Incoming BUY 103 x55:
  Level 101 (crosses: 103>=101): fill 20, incoming remaining 35
  Level 102 (crosses: 103>=102): fill 20, incoming remaining 15
  Level 103 (crosses: 103>=103): fill 15, incoming remaining 0 -> STOP
  Result: 3 trades, prices 101/102/103, total filled 55
```

Yeh `03_trade_events.cpp`'s test #2 hai (03-trade-events.md mein output).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf best level check karna, poori depth nahi
Agar tum sirf `best_level()` ek baar check karo aur loop na lagao,
multi-level sweep (upar wala example) kaam nahi karega — incoming order
sirf pehle level tak fill hoga, baaki galat tarah "rest" ho jaayega.

### Trap 2 — level KHATAM hone ke baad bhi usi price pe try karna
Har inner-loop iteration ke baad level empty check ZAROORI hai — nahi to
agla `opposite_side.best_level()` call SAME (ab-khaali) level return kar
sakta agar tumne use book se remove nahi kiya.

### Trap 3 — leftover ko GALAT side pe rest karna
Ek partially-filled Limit order ka leftover **apni HI side** pe rest hota
(BUY ka leftover BID banta, ASK NAHI) — `01_matching_engine.cpp` mein
yeh explicitly demonstrate kiya gaya (yeh khud humse ek comment-mein-galti
ke roop mein pehli baar saamne aaya, fix kiya gaya).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Matching sirf best price check karta | Best price se shuru, jab tak cross kare tab tak multi-level sweep karta |
| Size ya participant priority deta | SIRF price, phir time (arrival order) — kuch aur nahi |
| Partial leftover opposite side pe jaata | Leftover apni HI (incoming ki) side pe rest hota |

---

## Hands-on

```bash
./build.ps1 fast 40-MATCHING-ENGINE/examples/01_matching_engine.cpp
./build.ps1 fast 40-MATCHING-ENGINE/examples/03_trade_events.cpp
```

`03_trade_events.cpp` ka test #2 exactly upar wala 3-level-sweep
scenario chalata hai, actual Trade output ke saath.

---

## Exercises

1. Book: asks `100 x10, 100 x20 (dusra order, baad mein aaya), 105 x50`.
   Incoming BUY 100 x25. Kaunse orders match honge, kitna-kitna?
   <details><summary>Answer</summary>
   Dono 100-level orders SAME price hain, FIFO order se: pehla (x10) poora
   fill, phir dusra (x20) se 15 fill (25-10=15 baaki tha). 105 wala TOUCH
   hi nahi hota (100 < 105, cross nahi karta, aur incoming.qty already 0
   ho chuka).
   </details>

2. Ek incoming Market SELL order book ke bids ko POORI tarah khaali kar
   deta hai, phir bhi kuch qty baaki hai. Kya hota (04 se preview)?
   <details><summary>Answer</summary>
   Loop `opposite_side is not empty` condition pe khatam ho jaata (book
   khaali). Incoming ka remaining qty VOID ho jaata (Market order kabhi
   rest nahi karta) -- status Cancelled, jitna fill hua utne trades ke
   saath.
   </details>

---

## Interview questions

1. Price-time priority ka exact algorithm (do nested loops) explain karo.
2. Multi-level sweep kab hota, aur kab rukta?
3. Ek limit order ka partial leftover kis side pe rest hota, aur kyun?

---

## Next
→ [`03-limit-orders.md`](03-limit-orders.md)
