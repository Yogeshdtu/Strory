# 01 — Matching engine kya hai, exchange mein uski jagah

## Prerequisites
- `39-ORDER-BOOK` (poora — especially `01-order-book-requirements.md` ka
  scope-fark)
- `37-HFT-FUNDAMENTALS/02-how-exchanges-work.md`, `06-order-book-concept.md`,
  `07-price-time-priority.md`, `04-order-types.md`

## Yeh topic abhi kyun

39 mein humne ek order book banaya jo **state maintain karta** tha — jo
market-data feed se aane wale events (Add/Execute/Cancel/Delete/Replace)
apply karke "book abhi kaisi dikhti hai" batata. Us book ne khud kabhi
KOI DECISION nahi liya — usse jo event mila, wahi apply kar diya.

**Matching engine woh component hai jo DECISION leta hai.** Jab ek naya
order aata "main 100 pe 50 shares buy karna chahta hoon," matching engine
hi decide karta: kya yeh kisi resting sell order se cross karta hai? Agar
haan, kitna fill hoga, kis price pe, kis resting order se pehle? Yeh
decisions khud Execute events **generate** karte — jo phir baaki world ko
(38-MARKET-DATA jaisi feed ke through) broadcast hote.

---

## Exchange ke andar jagah

```
   Trader A                                          Trader B
  "BUY 100 x50"                                  "SELL 99 x30" (resting,
        |                                          pehle se book mein)
        v
  +---------------------------------------------------------+
  |                    MATCHING ENGINE                      |
  |  1. Incoming order ACCEPT karta (validate: duplicate id?)|
  |  2. Opposite side ke resting orders se COMPARE karta      |
  |     (price cross karti hai kya, price-time priority se)  |
  |  3. Match(es) EXECUTE karta -> Trade event(s) generate    |
  |  4. Remainder ka decision: REST karo, VOID karo, ya       |
  |     REJECT karo (order type ke hisaab se -- 03-06)        |
  +---------------------------------------------------------+
        |                                          |
        v                                          v
  Execution report                          Execution report
  (Trader A ko)                             (Trader B ko)
        |
        v
  Market data feed (38-style) -- baaki SAARE participants ko
  Trade event broadcast hota (anonymized -- kaun trade kar raha pata nahi
  chalta, sirf price/qty/time)
```

**Yeh EK single component hai jiske through HAR trade guzarta.** Agar
yeh galat ho jaaye (crash, wrong match, race condition) — poora exchange
down ya galat ho jaata. Isliye is folder ka poora focus **correctness aur
determinism** pe hai, sirf speed pe nahi (09, 11, 14-16 mein detail).

---

## 39 vs 40 — ek baar phir, crisply

| | 39-ORDER-BOOK | 40-MATCHING-ENGINE |
|---|---|---|
| Input | Already-decided events (feed se) | RAW incoming orders (side, price, qty, type) |
| Kaam | State maintain — "book abhi kya dikhta" | DECISION lena — "yeh order match karta ya nahi" |
| Output | Query results (best bid/ask, etc.) | Trade events (khud generate karta) |
| Analogy | Ek mirror | Ek judge/referee |

Real system mein tumhare paas **dono** ho sakte: agar tum khud exchange/
simulator ho, tumhe 40-style engine chahiye (real matching). Agar tum ek
trading firm ho jo kisi EXISTING exchange se connect karti, tumhe apna
39-style book chahiye (unki feed se apni state maintain karne ke liye) —
tumhara khud ka matching engine tab hi chahiye jab tum "exchange" ho, ya
apna internal crossing-network (dark pool) chala rahe ho.

---

## Kya is folder mein banayenge

Ek `MatchingEngine` class jo:
1. `submit(Order)` — naya order accept karta, match karta, Trade events
   return karta, order ka final status batata (02, 12).
2. `cancel(id)` — resting order hatata.
3. `replace(old_id, new_order)` — cancel-replace, same-id-chain.
4. 4 order types support karta: **Limit, Market, IOC, FOK** (03-06).
5. **Self-trade prevention** (STP) — 3 modes (08).
6. Har cheez **deterministic** — same input sequence = same output,
   hamesha (09, 10, 11).

Poore folder mein rigorously **test aur fuzz** kiya jaayega (14, 15) —
ek independent, jaan-boojh kar simple O(n) reference implementation ke
against, jaisa 39 mein hua tha.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "matching engine bas ek if-check hai"
"Agar price cross kare to fill kar do" utna simple lagta hai jab tak tum
partial fills, multiple price levels, order types (Market/IOC/FOK), aur
self-trade prevention nahi sochte. Yeh EK single naive-looking function
call (`submit()`) ke peeche kaafi state machine hai (12).

### Trap 2 — determinism ko "nice to have" samajhna
Ek matching engine jo do baar SAME input pe DIFFERENT output de — replay/
audit/disaster-recovery sab tootg jaate (09, 10, 11 mein poora reasoning).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| 39 aur 40 same cheez hain | 39 STATE maintain karta, 40 DECISION leta aur khud events generate karta |
| Matching sirf ek price-compare hai | Price-time priority + order types + STP + determinism — poora system |
| Matching engine sirf exchanges ke paas hoti | Dark pools, internal crossing networks bhi apni matching engine rakhte |

---

## Exercises

1. Ek trading firm sirf NSE se connect karti (khud exchange nahi hai). Kya
   unhe 40-style matching engine chahiye?
   <details><summary>Answer</summary>
   Zaroori nahi — unka apna 39-style order book chahiye (NSE ki feed se
   apni local state maintain karne ke liye, taaki strategy fast query kar
   sake). Matching engine (40-style) tab chahiye jab woh khud exchange
   ban rahe ho, ya apna internal crossing network/dark pool chala rahe ho.
   </details>

2. Kyun matching engine mein correctness/determinism ko speed se BHI zyaada
   priority di jaati?
   <details><summary>Answer</summary>
   Kyunki yeh EK component hai jiske through poore exchange ka har trade
   guzarta — ek bug (wrong match, race condition, non-determinism) direct
   financial loss, regulatory violation, ya poore market ka trust todta.
   Speed important hai, par "fast par kabhi-kabhi galat" ek exchange ke
   liye acceptable nahi hai.
   </details>

---

## Interview questions

1. Order book aur matching engine ka responsibility-fark batao.
2. Matching engine ka input aur output kya hote hain?
3. Determinism matching engine ke liye kyun non-negotiable hai?

---

## Next
→ [`02-matching-algorithm.md`](02-matching-algorithm.md)
