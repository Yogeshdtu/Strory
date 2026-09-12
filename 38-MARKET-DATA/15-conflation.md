# 15 — Conflation: jab zyada updates aati hain jitni tum process kar sakte

## Prerequisites
- [`14-timestamping-and-clocks.md`](14-timestamping-and-clocks.md)
- [`03-snapshots-vs-incremental.md`](03-snapshots-vs-incremental.md)

## Yeh topic abhi kyun
Poora folder 38 abhi tak "har message correctly process karo" pe focused
tha. Yeh lesson ek honest exception discuss karta hai: **kabhi-kabhi tumhe
JAAN-BOOJH KAR updates DROP karne padte hain** — aur kaunse drop karna
"safe" hai, yeh ek design decision hai, accident nahi.

---

## Problem: burst > processing capacity

```
Normal:  1000 updates/sec aati, tum 100,000/sec process kar sakte  -- easy

News event / market open:  500,000 updates/sec aati, tum 100,000/sec
                            process kar sakte  -- QUEUE BADHTI JAATI
```

Agar tum **har** update queue karte raho bina drop kiye, queue **unbounded
grow** karti — memory badhti, aur jo bhi tum process kar rahe ho, woh
**purana ho chuka hota by the time tum wahan pahunchte** (36's batching
lesson ka "head-of-line latency" wahi problem hai, market-data context
mein).

---

## Conflation kya hai

> **Conflation = MULTIPLE consecutive updates (same price level/order ke
> liye) ko EK combined/latest-state update mein merge karna, taaki
> consumer ko sirf FINAL state mile, beech ke sab intermediate steps
> nahi.**

```
Real sequence:
  100.48 qty=200
  100.48 qty=180  (ek chhota fill)
  100.48 qty=150  (ek aur chhota fill)
  100.48 qty=140  (ek cancel)

Conflated (agar consumer overloaded hai):
  100.48 qty=140   <- sirf FINAL state, beech ke 3 intermediate steps GONE
```

Consumer ko **sahi final answer** milta, par **path** nahi — kitni baar
fill hua, kab hua, yeh information discard ho jaati.

---

## Kya conflate karna SAFE hai, kya NAHI

| Data type | Conflate karna safe? | Kyun |
|---|---|---|
| **L2 price-level updates** | ✅ Aksar safe | Sirf "level ka current total qty" matter karta zyaadatar use-cases mein — intermediate values discard-able |
| **Individual trade events** | ❌ NAHI | Har trade ek discrete economic event hai (P&L, volume-tracking, regulatory record) — ek "combine karo" nahi ho sakta |
| **Order-level (L3) events for YOUR OWN order tracking** | ❌ NAHI | Agar tumhara khud ka order kisi update mein tha (fill/cancel), woh miss/merge nahi ho sakta — tumhe apni exact position pata honi chahiye |
| **Top-of-book (L1) for a slow display** | ✅ Safe | Ek dashboard "current price" dikhata — sabse latest hi matter karta, history nahi |

**Nichod:** conflation **"latest state matters, history doesn't"** wale
use-cases mein safe hai (L2 book display, dashboards). **"Har event
individually matters"** wale use-cases mein (trades, tumhare khud ke
orders, strategy signals jo order-flow PATTERN pe depend karte) conflation
**data loss** hai, bug hai.

---

## Kahan conflation hoti hai (layers)

```
1. Exchange-side       -- kuch venues khud "conflated" feed offer karte
                          (full L3 ke alawa, ek slower/summarized feed)
2. Network/vendor-side  -- data vendors (market data resellers) aksar
                          conflate karte apne downstream clients ke liye
3. Tumhare consumer mein -- agar tumhara consumer overloaded ho jaaye,
                          TUM khud conflate karne ka decision le sakte
                          (ya drop, ya buffer-and-catch-up)
```

> **HFT connection:** HFT ka hot-path **kabhi conflate nahi karta** —
> poora point hi hai ki tumhe HAR update, jaldi se jaldi, chahiye (37/03
> ka adverse-selection avoidance isi speed pe depend karta). Conflation
> typically **non-latency-critical consumers** (dashboards, compliance
> logging, retail feeds) ke liye hoti — 37/15 (conflation kuch venues mein
> retail-tier feeds ka standard part bhi hota hai).

---

## Alternative to conflation: drop entirely (aur mark stale)

Agar conflation bhi possible nahi (jaise tumhara consumer itna slow hai ki
even "latest state" track karna mushkil hai), better option **explicitly
drop karo aur "stale" mark karo** (13 ka concept) — silently purani data
process karte rehna (jaise kuch nahi hua) sabse bura option hai (03 ka
silent-drift problem, phir se).

```
Option A: conflate (latest state track karo, intermediate discard)  <- OK for L2 display
Option B: drop + mark stale (13)                                     <- OK agar even latest state track nahi kar sakte
Option C: process EVERYTHING, bina drop/conflate kiye                <- HOT PATH, HFT ka default
Option D: silently fall behind, purani data process karte raho       <- ❌ KABHI NAHI (worst)
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — hot-path pe accidentally conflation ho jaana
Agar tumhara queue bounded hai aur overflow pe purani entries silently
drop hoti hain (bina explicit design decision ke), yeh **accidental,
undocumented conflation** hai — bug hai jab tak jaan-boojh kar decide na
kiya ho ki yeh acceptable hai.

### Trap 2 — trade events ko L2-jaisa treat karna
Trade events **kabhi conflate mat karo** — "10 trades ka summary" ek
"1 bada trade" jaisa nahi dikhta P&L/volume tracking ke liye.

### Trap 3 — apne khud ke orders ko conflation mein khona
Agar tumhara khud ka order 2 baar partial-fill hota beech mein, aur
conflation sirf "final qty" dikhata, tum apne 2 fills ka individual detail
(jo risk system ko chahiye ho sakta) kho dete.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Conflation hamesha bad practice hai | Kuch use-cases (L2 display) mein legitimate, designed choice |
| Trade events conflate ho sakte | Nahi — har trade discrete event hai |
| HFT hot path pe conflation normal hai | Nahi — hot path har update chahta, poori speed se |
| Silent fall-behind aur conflation same cheez hain | Conflation explicit design hai; silent fall-behind ek bug hai |

---

## Exercises

1. Ek dashboard sirf "current best bid/ask" dikhata, update per second
   refresh hoti. Kya is dashboard ke consumer ko L3 conflate karna safe
   hai?
   <details><summary>Answer</summary>
   Haan — dashboard sirf LATEST state dikhata, intermediate history
   irrelevant hai use-case ke liye. Conflate karke sirf per-second "latest
   known state" bhejna bandwidth/processing bachata bina kisi functional
   loss ke (dashboard ka purpose hi "abhi price kya hai" hai, "kaise wahan
   pahunchi" nahi).
   </details>

2. Ek risk system tumhare khud ke orders ke fills track karta hai. Kya
   is stream ko kabhi conflate karna chahiye?
   <details><summary>Answer</summary>
   Kabhi nahi. Risk system ko HAR fill individually pata hona chahiye —
   position tracking, P&L calculation, aur exposure limits (37/13) sab
   HAR discrete fill event pe depend karte. Conflation yahan "final
   position" bata sakta, par kaise/kab wahan pahunche (jo audit trail —
   37/16 — ke liye zaroori hai) kho jaata.
   </details>

---

## Interview questions

1. Conflation kya hai, aur yeh kis use-case mein safe hai?
2. Kaunsa data type kabhi conflate nahi karna chahiye, aur kyun?
3. HFT ka hot path conflation kyun use nahi karta?
4. Conflation aur "silent fall-behind" mein fark batao.

---

## Next
→ [`16-building-feed-handler.md`](16-building-feed-handler.md)
