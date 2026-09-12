# 02 — L1, L2, L3 market data

## Prerequisites
- [`01-what-is-market-data.md`](01-what-is-market-data.md)
- `37-HFT-FUNDAMENTALS/06-order-book-concept.md`

## Yeh topic abhi kyun
"L1/L2/L3" industry mein har jagah use hone wali terminology hai —
data-vendor pricing, API docs, job descriptions, sab isi mein baat karte.
Aur yeh sirf naming nahi — har level ka **information content**, **size**,
aur **use case** alag hai.

---

## Teen levels — ek nazar mein

```
L1: sirf TOP           bid: 100.48 x200   |   ask: 100.50 x150

L2: HAR price level,     bid: 100.48 x200   |   ask: 100.50 x150
    aggregated qty        100.47 x500   |         100.51 x400
                           100.46 x800   |         100.52 x900

L3: HAR individual       bid: 100.48                ask: 100.50
    order                  - order#41, qty 120       - order#88, qty 150
                           - order#42, qty 80
                        (100.48 ka total 200 = 120+80, do orders se)
```

| Level | Kya milta | Size (relative) | Kisko chahiye |
|---|---|---|---|
| **L1** | Best bid/ask price + total qty | Sabse chhota | Retail terminals, dashboards, sirf "current price" |
| **L2** | Har price level, aggregated qty | Medium | Zyaadatar trading systems, depth-aware strategies |
| **L3** | Har individual order (id, qty, arrival order) | Sabse bada | Order-book reconstruction, precise time-priority (07), HFT |

---

## L1 — top of book

Sirf best bid aur best ask (37/05 ka "spread/mid/microprice" seedha yahin
se compute hota). **Sabse compact**, sabse kam bandwidth, sabse "shallow"
information.

```cpp
struct L1Quote { Price bid_px, ask_px; Qty bid_qty, ask_qty; };
```

Kaafi hai jab tumhe sirf "abhi price kya hai" jaanna ho — display screens,
simple alerts. **Kaafi NAHI** jab tumhe depth/liquidity dekhni ho (37/06 ka
"depth" concept) ya order-flow signal chahiye ho.

---

## L2 — market by price (aggregated depth)

Har price level ka total resting qty — 37/06 ka `SimpleBook` model exactly
yehi tha (`std::map<Price, Qty>`). Individual orders **nahi** dikhte, sirf
level totals.

```cpp
struct L2Level { Price px; Qty total_qty; };
std::vector<L2Level> bids, asks;   // sorted
```

L2 **derived** hoti hai L3 se (agar exchange L3 publish kare) — har level
ka total = us level ke sab orders ki qty ka sum. **Kai exchanges DIRECTLY
L2 publish karte** (individual order details kabhi kisi ko nahi dete —
privacy/fairness reasons) — tab tumhe L2 UPDATES milte (`"level 100.48 ab
qty 320 hai"`), individual add/cancel events nahi.

---

## L3 — market by order (full depth)

Har individual order — kab aaya (time priority ke liye — 37/07), kitni qty,
kaunsa order_id. Yeh **hamara `wire_protocol.hpp` ka model hai**
(`AddOrderMsg`, `CancelMsg`, etc. — har message ek specific `order_id` ke
against hota).

```cpp
// examples/wire_protocol.hpp se
struct AddOrderMsg {
    MsgHeader hdr; std::uint64_t order_id; std::uint32_t symbol_id;
    std::uint32_t qty; std::int64_t price_ticks; std::uint8_t side;
};
```

**L3 se L1 aur L2 dono derive ho sakte** (order-by-order book maintain
karo, phir top/aggregate compute karo) — ulta nahi ho sakta (01 ka
exercise). Isiliye **L3 ek strict superset hai** information ke terms mein.

---

## Trade-off: information vs bandwidth vs parsing cost

```
L1:  chhota, fast, KAM information
L2:  medium
L3:  bada, SABSE ZYADA information, SABSE ZYADA bandwidth/parsing cost
```

Ek HFT firm jo precise price-time priority (07) simulate karna chahti
(apna khud ka book maintain karke, taaki pata ho "agar main ab order bheju,
mera fill kahan hoga"), use **L3** chahiye — L2 sirf aggregate deta,
individual order queue-position pata nahi chalta.

Ek dashboard jo sirf "current price" dikhata, use **L1** kaafi hai — L3
process karna waste hai (zyada bandwidth, zyada CPU, koi extra faayda
nahi us use-case ke liye).

> **HFT connection:** yeh course L3 pe focus karta (`wire_protocol.hpp` ke
> AddOrder/Cancel/Execute/Delete/Replace messages) kyunki **39-ORDER-BOOK**
> mein tum khud ka precise book banaoge — L3 ke bina woh possible nahi.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — L2 se order-level time-priority nikalne ki koshish
L2 mein "100.48 @ qty 200" hai — kaun sa order pehle tha, andar kitne
orders hain, kuch pata nahi. Agar tumhe FIFO simulation (07) chahiye, L2
kaafi NAHI hai.

### Trap 2 — L3 hamesha "behtar" samajhna
Zyada information = zyada bandwidth + zyada parsing cost + zyada storage.
Agar use-case ko sirf top-of-book chahiye, L3 process karna sirf overhead
hai (Rule: apne latency budget — 37/14 — ke against decide karo, "zyada
data hamesha better" nahi).

### Trap 3 — sochna sab exchanges teeno level publish karte
Kai venues sirf L1/L2 publish karte (order-level privacy ya bandwidth
reasons se) — L3 sabhi jagah available nahi hota. Apne target venue ka
spec check karna zaroori (37/15 — venue-specific).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| L2 se time-priority pata chal sakti | Nahi — L2 aggregated hai, order-level detail L3 mein |
| L3 hamesha better choice hai | Bandwidth/parsing cost zyada; use-case-dependent |
| Sab exchanges L3 dete | Kai sirf L1/L2 publish karte |
| L1/L2/L3 alag data sources hain | L1, L2 dono L3 se DERIVE ho sakte (ek hi source) |

---

## Exercises

1. Ek L3 feed se, ek specific price level ki L2 "total qty" kaise compute
   karoge?
   <details><summary>Answer</summary>
   Us price level ke sab resting (abhi tak add hue, cancel/execute na hue)
   orders ki qty sum karo. Real-time mein: ek running total per price
   level maintain karo — AddOrder pe += qty, Cancel/Delete/Execute pe -=
   qty (jitna reduce hua) us order ke price level ka. Yehi 39-ORDER-BOOK
   mein implement hoga.
   </details>

2. Ek exchange sirf L1 publish karta. Kya tum uske data se ek accurate L2
   book reconstruct kar sakte ho?
   <details><summary>Answer</summary>
   Nahi. L1 sirf best bid/ask deta — doosre levels (100.47, 100.46, ...)
   ka koi data hi nahi milta feed mein. Missing information wapas nahi
   nikaal sakte — jo publish nahi hua, woh reconstruct nahi ho sakta.
   </details>

---

## Interview questions

1. L1, L2, L3 define karo — har ek mein exactly kya information hai.
2. Kyun L3 se L1/L2 derive ho sakte, par ulta nahi?
3. Ek use-case do jahan L1 kaafi hai, aur ek jahan L3 zaroori hai.
4. Sab exchanges L3 publish nahi karte — iska tumhare system design pe
   kya impact hota?

---

## Next
→ [`03-snapshots-vs-incremental.md`](03-snapshots-vs-incremental.md)
