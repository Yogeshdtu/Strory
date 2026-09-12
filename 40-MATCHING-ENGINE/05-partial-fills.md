# 05 — Partial execution, remaining quantity

## Prerequisites
- `04-market-orders.md`

## Yeh topic abhi kyun

Partial fill EK single mechanic hai jo poori engine mein baar-baar aata
hai — Limit ka leftover, Market ka partial sweep, IOC/FOK dono. Yeh
lesson usi mechanic ko isolate karta, dono taraf (incoming AUR resting)
se kya hota, exactly.

---

## Ek match ke andar, DONO orders ki qty ghatti hai

```cpp
// matching_engine.hpp -- match_against() ke andar
const Qty fill_qty = std::min(incoming.qty, resting.qty);
incoming.qty -= fill_qty;
resting.qty  -= fill_qty;
level.total_qty -= fill_qty;
```

`std::min` yahan critical hai — fill hamesha DONO orders ki available
qty se CHHOTA (ya barabar) hota, kabhi zyaada nahi. Teen scenarios:

| Incoming vs resting | Kya hota |
|---|---|
| `incoming.qty < resting.qty` | Incoming POORA fill, resting KUCH baaki (level mein reh jaata, front pe) |
| `incoming.qty == resting.qty` | DONO poore fill, resting level se hat jaata |
| `incoming.qty > resting.qty` | Resting POORA fill (hat jaata), incoming KUCH baaki (agle order/level try karta) |

---

## `orig_qty` -- kyun rakha jaata hai

```cpp
struct Order {
    ...
    Qty qty;        // REMAINING -- match hote hote ghatta hai
    Qty orig_qty;   // ORIGINAL -- kabhi nahi badalta
};
```

`qty` (remaining) matching ke liye use hota. `orig_qty` fill-RATIO nikalne
ke liye chahiye — "yeh order kitna % fill hua" — aur FOK ka invariant
check karne ke liye bhi (`filled == orig_qty` for a valid Filled FOK,
06_engine_tests.cpp mein).

---

## Resting order ki position -- partial fill KEEP karta, purani jagah pe

```cpp
if (resting.qty == 0) {
    ord_it = level.orders.erase(ord_it);   // POORA fill -- list se HATA do
    index_.erase(dead_id);
} else {
    ++ord_it;   // PARTIAL fill -- list mein RAHEGA, SAME position (front)
}
```

Yeh **price-time priority ka ek subtle par important guarantee** hai:
ek order jo partially fill ho chuka, apni ORIGINAL arrival-time priority
NAHI khota — woh abhi bhi level ke FRONT pe hai (jahan tha), naya order
NAHI ban jaata. Agar tum galti se erase+re-insert karte (jaisa "reduce
karo, phir wapas push_back karo"), yeh order **queue ke peeche chala
jaata** — sabse peeche, jaise abhi-abhi aaya ho — real market mein ek
BADI unfair cheez hoti (39/10's "partial-reduce preserves position" wala
principle yahan bhi hai).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `fill_qty` compute karne se pehle STP check na karna
Agar STP check (08) fill_qty calculation ke BAAD ho, ek self-trade ka
fill KAR diya jaayega phir "undo" karne ki koshish hogi — messy aur
error-prone. `match_against` mein STP check fill se PEHLE hota (order
matters).

### Trap 2 — `level.total_qty` update bhool jaana
`level.total_qty` best_bid_qty()/best_ask_qty() query ke liye maintain
hota — agar fill ke time isse update na karo, query WRONG (stale) qty
dikhayegi, chahe individual orders sahi se fill ho rahe hon. Ek "quiet"
bug — tests ise pakadte (06's `test_basic_match_and_fifo` implicitly is
consistency pe depend karta).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Partial fill = order "cancel + naya order" | SAME order, kam qty ke saath, SAME position |
| `orig_qty` matching logic mein use hota | Sirf `qty` (remaining) matching mein use hota; `orig_qty` sirf reporting/invariant-check ke liye |
| Partially-filled resting order queue ke peeche chala jaata | Apni ORIGINAL position (front) retain karta |

---

## Exercises

1. Ek resting order `qty=50` hai, do alag incoming orders isse partially
   fill karte: pehla 20 leta, dusra (baad mein aata) 15 leta. Is resting
   order ki final `qty` kya hai, aur woh abhi bhi level ke FRONT pe hai
   kya (agar koi naya order beech mein add hua ho)?
   <details><summary>Answer</summary>
   Final qty = 50-20-15 = 15. Haan, woh abhi bhi apni ORIGINAL FIFO
   position pe hai -- partial fills position nahi badalte, sirf REMOVE
   hone se (poora fill ya explicit cancel) position jaati hai.
   </details>

---

## Interview questions

1. `std::min(incoming.qty, resting.qty)` kyun -- kya hota agar galat order
   mein subtract kar do?
2. Partial fill resting order ki priority-position kyun preserve karta?
3. `qty` aur `orig_qty` ka fark, dono kyun rakhe jaate?

---

## Next
→ [`06-ioc-and-fok.md`](06-ioc-and-fok.md)
