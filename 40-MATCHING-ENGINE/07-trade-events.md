# 07 — Trade event generation, execution reports

## Prerequisites
- `06-ioc-and-fok.md`

## Yeh topic abhi kyun

Matching engine ka poora "output" — jo baaki system consume karta (market
data feed jaisa 38, participants ke execution reports, risk systems
37/13) — ek `Trade` struct hai. Iske fields aur unke conventions poore
downstream system ka contract hain.

---

## `Trade` struct

```cpp
struct Trade {
    TradeId       id;                      // is trade ki stable identity
    OrderId       aggressor_id;             // incoming (taker) order
    OrderId       resting_id;               // resting (maker) order
    ParticipantId aggressor_participant;
    ParticipantId resting_participant;
    bool          aggressor_is_buy;         // aggressor ki side (resting apne-aap ulti)
    Price         price;                    // EXECUTION price
    Qty           qty;                      // fill quantity
    std::uint64_t seq;                      // global event-timeline position
};
```

Ek `Trade` **do orders ka reference** rakhta (`aggressor_id`,
`resting_id`) — dono participants ko unka apna execution report bhejne ke
liye kaafi hai (har participant apna order-id filter kar sakta).

---

## "Maker sets the price" convention

```cpp
trades.push_back(Trade{
    ...,
    lvl_price,   // <- resting (maker) ki price, incoming ki NAHI
    fill_qty,
    ...
});
```

Ek incoming BUY apni limit se BEHTAR price offer kar sakta (jaisa 103,
jab resting ASK sirf 100 maang raha) — trade phir bhi **100** pe hota,
103 pe NAHI. Yeh "**price improvement**" hai: aggressor ko apni limit se
kabhi BADTAR price nahi milta (guarantee), aur agar market usse behtar de
sakta, woh improvement AGGRESSOR ko milta (exchange ko extra profit nahi
banta beech mein).

`03_trade_events.cpp`'s scenario #1 isi ko demonstrate karta.

> **HFT relevance:** market makers ki poori economics (37/09's maker-
> taker fee model) is convention pe depend karti — maker apni price
> "commits" karta, aur guaranteed woh price milegi (behtar nahi, badtar
> bhi nahi) jab match ho. Yeh predictability strategy design ke liye
> zaroori hai.

---

## Multi-level sweep -- multiple trades, alag prices

Ek incoming order jo kai levels sweep karta (02, 04), har level pe ALAG
`Trade` generate hota, apni HI level-price ke saath — `03_trade_events.cpp`'s
scenario #2: teen trades, prices 101/102/103.

---

## `seq` -- ek SHARED global event timeline

```cpp
std::uint64_t next_seq_ = 1;   // orders AUR trades DONO isi se increment hote
```

`incoming.seq = next_seq_++;` (jab order arrive hota) AUR `next_seq_++`
(har Trade ke liye) — **EK single monotonic counter**, dono "order
arrival" aur "trade execution" events isi timeline pe hain. Yeh
determinism (09) aur event-sourcing (10) ka bunyaad hai: agar tum poori
history ko `seq` se sort karo, tumhe EXACT order milta jismein cheezein
HUI (kaunsa order pehle aaya, kaunsa trade uske baad hua, agla order kab
aaya) — ek single, unambiguous timeline.

---

## Execution reports (real-world context)

Real exchanges Trade se **do alag reports** banate (ek har participant
ke liye) — is course ka engine sirf ek "shared" `Trade` return karta
(dono participants ka data usi mein hai); production system typically:

```
Trade{aggressor=A, resting=B, ...}
  -> Execution Report to A: "tumhara order X, Y qty @ Z price fill hua"
  -> Execution Report to B: "tumhara order M, Y qty @ Z price fill hua"
  -> Market Data feed: "trade hua @ Z price, Y qty" (anonymized -- A/B
     ka pata NAHI chalta baaki participants ko)
```

Yeh fan-out (ek Trade se 3 alag downstream messages) is folder ke scope
se bahar hai (41-HFT-CONCURRENCY/HFT-NETWORKING mein relevant), par
`Trade` struct ka design EXACTLY isi fan-out ko support karne ke liye hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — aggressor ki price use kar lena
Agar tum galti se `incoming.price` ko trade price maan lo (resting ki
jagah), price-improvement wale cases mein galat number report hoga —
ek subtle par regulatory-relevant bug (real markets mein trade reporting
accuracy legally mandated hoti, 37/16).

### Trap 2 — `seq` ko sirf trades ke liye rakhna, orders ke liye nahi
Agar order-arrival events `seq` timeline mein na hon, replay/audit se
"order X kab aaya trade Y ke relative" jaise questions answer nahi ho
sakte — sirf trades ka relative-order milta, poori event history ka nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Trade price = aggressor ki price | Trade price = RESTING (maker) ki price |
| Ek Trade ek hi participant ka data rakhta | Dono (aggressor + resting) ka reference rakhta |
| `seq` sirf trades count karta | Orders ki arrival bhi isi shared timeline pe hai |

---

## Exercises

1. Incoming SELL apni limit 95 pe hai, resting BID 98 hai (95 se behtar
   offer). Trade kis price pe hoga?
   <details><summary>Answer</summary>
   98 -- resting (maker) ki price. Incoming SELL ko apni limit (95) se
   BEHTAR (98) mila -- price improvement, jaisa BUY-side example mein tha,
   symmetric logic.
   </details>

---

## Interview questions

1. "Maker sets the price" convention explain karo, ek example ke saath.
2. `Trade` struct ke fields aur unka purpose batao.
3. Ek shared global `seq` counter (orders+trades dono) ka fayda kya hai?

---

## Next
→ [`08-self-trade-prevention.md`](08-self-trade-prevention.md)
