# 01 — Order book requirements: kya chahiye, kya NAHI chahiye

## Prerequisites
- `38-MARKET-DATA` (poora — Add/Execute/Cancel/Delete/Replace event model)
- `37-HFT-FUNDAMENTALS/06-order-book-concept.md`, `07-price-time-priority.md`

## Yeh topic abhi kyun
**Yeh HFT ka sabse classic interview question aur sabse important data
structure hai.** Isse teen baar banayenge (naive → better → optimized),
har baar measure karenge. Par pehle: exactly kya banana hai, aur — utna
hi zaroori — kya **nahi** banana.

---

## Scope: yeh book kya hai (aur kya NAHI hai)

> **Yeh order book market-data CONSUME karta hai** — 38-MARKET-DATA jaisi
> feed se aane wale Add/Execute/Cancel/Delete/Replace events apply karke
> apni state maintain karta, taaki har waqt "book abhi kaisi dikhti hai"
> query ki jaa sake.

**Yeh book khud MATCHING/CROSSING NAHI karta** — koi check nahi ki ek naya
bid kisi existing ask se cross karta hai ki nahi, koi automatic fill nahi.
Woh **40-MATCHING-ENGINE** ka kaam hai (jo aggressive orders ko resting
orders se match karta, 37/07 ka price-time-priority algorithm implement
karta).

```
38-MARKET-DATA          39-ORDER-BOOK              40-MATCHING-ENGINE
(feed parse karta,   -> (feed ke events se book  -> (naye AGGRESSIVE
 events decode karta)    STATE maintain karta)       orders ko RESTING
                                                       orders se match
                                                       karta, khud events
                                                       GENERATE karta)
```

Yeh do alag roles hain: **39 ek "mirror" hai** (jo ho chuka, uska
accurate reflection rakhta), **40 ek "decision maker" hai** (jo hona
chahiye, decide karta). Real trading system ke andar tum **dono** rakhte
ho — apna khud ka book (39-style, market data se) taaki tumhari strategy
dekh sake "market kaisi dikhti," aur agar tum khud ek exchange/simulator
bana rahe ho, ek matching-engine-style book (40) bhi.

---

## Functional requirements

| Operation | Kya karta | Konsa 38-event isse trigger karta |
|---|---|---|
| **Add** | Naya order book mein daalo (side, price, qty) | `AddOrder` |
| **Execute** | Order ki qty reduce karo (fill) | `Execute` |
| **Cancel** | Order ki qty reduce karo (partial cancel) | `Cancel` |
| **Delete** | Order poora hatao | `Delete` |
| **Replace** | Purana order retire, naya order same-ID-chain mein | `Replace` |
| **Query: best bid/ask** | Top-of-book price + qty | (koi event nahi — pull query) |
| **Query: FIFO order at a level** | Price-time priority order (37/07) | (koi event nahi — pull query) |

**Execute aur Cancel mechanically SAME hain** book ke perspective se — dono
"order ki qty ghatao" (37/38 mein already noted). Isliye is folder ke
`Book*` classes mein ek hi method (`reduce(id, qty)`) dono ko handle karta.

---

## Non-functional requirements (HFT context)

| Requirement | Kyun |
|---|---|
| **Har operation FAST** (nanoseconds) | 37/14's latency budget ka ek stage — book update slow hui to poora tick-to-trade slow |
| **Tail (p99.9) bhi tight** | 35/05 — average nahi, worst-realistic-case matter karta |
| **Best bid/ask O(1)** | Har strategy decision isi query se shuru hoti — 11 mein detail |
| **Cancel/Execute by order_id, FAST** | Real feeds mein cancel/execute id se aate (price nahi pata hota caller ko) — 09 mein detail |
| **Correct price-time priority** | 37/07 — kaun pehle fill hoga, yeh order preserve karna zaroori |
| **Bounded/predictable memory** | Allocation-free hot path (36-LOW-LATENCY-CPP ka poora principle) |

---

## ⚠️ Ek precondition jo crash de sakta hai (real bug is folder banate waqt mila)

`best_bid()` / `best_ask()` ka ek **precondition** hai: **book mein kam se
kam ek bid/ask hona chahiye.** Empty book pe `best_bid()` call karna UB
hai (out-of-bounds read ya invalid-iterator dereference — exact failure
mode implementation ke hisaab se alag, par sab teeno versions mein UB).

```cpp
// ❌ GALAT -- has_bid() check kiye bina
Price p = book.best_bid();       // book khaali ho to UB

// ✅ SAHI
if (book.has_bid()) {
    Price p = book.best_bid();
}
```

**Yeh isi folder ki testing (16, `08_orderbook_tests.cpp`) banate waqt
pakda gaya** — ek test ne V3 pe `best_bid()` call kiya bina `has_bid()`
check kiye, aur array-bounds assertion crash hui. Fix: test ko `has_bid()`
guard se pehle likha. Yeh exactly woh cheez hai jo **testing (16)** pakadti
hai jo casual manual testing miss kar sakta — ek precondition jo "zyaadatar
case mein" (jab book khaali nahi hoti) chup rehta hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — matching logic book mein daal dena
Agar tum "add order, aur agar cross kare to match kar do" jaisa likhte ho,
tum 39 aur 40 ko mix kar rahe ho. Alag concerns, alag classes/files —
39-ORDER-BOOK sirf STATE maintain karta.

### Trap 2 — price ko `double` mein rakhna
37/08 ka principle yahan bhi — integer ticks (`Price = std::int64_t`),
kabhi `double` nahi.

### Trap 3 — Execute aur Cancel ko alag mechanism samajhna
Dono book ke perspective se **identical** hain (qty reduce) — sirf
*semantic meaning* alag hai (fill vs cancel), jo higher-level logging/
accounting mein matter karta, book ki internal mechanics mein nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Order book matching karta hai | Order book STATE maintain karta; matching alag component (40) |
| Execute aur Cancel alag book-operations hain | Dono "reduce qty" hain, mechanically identical |
| `best_bid()` hamesha safe hai call karna | Precondition: `has_bid()` true hona chahiye |
| Price `double` mein theek hai | Integer ticks — 37/08 |

---

## Exercises

1. Ek naya order aata hai jiski price existing best ask se cross karti
   hai. Is folder ka order book iske saath kya karega?
   <details><summary>Answer</summary>
   Kuch special nahi — order book sirf event ko apply karega (Add karega
   us price/side pe), koi crossing-check ya auto-fill nahi. Real system
   mein aisa event kabhi book tak pahunchega hi nahi (matching engine —
   40 — usse turant match kar degi, aur book ko ek DIFFERENT event
   (Execute) milega, "Add" nahi) — par ISI book ke tests mein agar
   deliberately aisi price di jaaye, book bina complain kiye apply kar
   degi (uska kaam matching-validity check karna nahi hai).
   </details>

2. `best_bid()` ko `has_bid()` check ke bina call karna kis specific bug
   class mein aata hai (23-ERROR-HANDLING se yaad karo)?
   <details><summary>Answer</summary>
   Undefined behavior (precondition violation) — jaisa "empty container
   pe `.front()`/`.begin()->...` call karna." Yeh koi exception nahi
   phenkta (jaisa `.at()` karta), seedha UB hai — sanitizers (35/15)
   ya assertions (jaisa yahan V3's `std::array::operator[]`) ise pakad
   sakte hain, par bina explicit check ke silently galat memory bhi
   padh sakta production build mein.
   </details>

---

## Interview questions

1. Order book aur matching engine ka scope-fark batao.
2. Execute aur Cancel book ke perspective se same kyun hain?
3. `best_bid()` ka precondition kya hai, aur violate karne pe kya hota?
4. Har operation ke liye latency requirement kyun matter karta (37/14 se
   connect karo)?

---

## Next
→ [`02-design-space.md`](02-design-space.md)
