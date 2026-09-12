# 01 — Market data kya hai: feeds, ticks, updates

## Prerequisites
- `37-HFT-FUNDAMENTALS` (poora, especially 02-how-exchanges-work,
  06-order-book-concept)
- `30-NETWORKING` (sockets, UDP vs TCP, multicast — light revision kaafi)

## Yeh topic abhi kyun
37 mein tumne exchange architecture dekha: matching engine har book-changing
event ko ek public feed pe broadcast karta. Ab folder 38 ka poora kaam hai
us feed ko **correctly aur fast** parse karna — yehi tumhara system ka
**input** hai. Agar yahan bug hai ya slow hai, baaki poora pipeline (39-44)
galat/late data pe kaam kar raha hoga.

---

## Market data = ek continuous event stream

> **Market data feed = exchange se publish hone wala, sab participants ko
> milne wala, book-changing events ka stream — real time mein, jaise-jaise
> woh hote hain.**

37/02 ka diagram yaad karo: matching engine har add/cancel/modify/execute
pe ek event generate karta, aur woh event **market-data-out** path se sabko
(public, multicast — 30) broadcast hota. Yeh stream hi "market data" hai.

```
matching engine  --[event: AddOrder, Execute, Cancel, ...]-->  market data feed
                                                                       |
                                                    +------------------+------------------+
                                                    v                  v                  v
                                              tumhara firm      competitor firm      retail terminal
                                              (feed handler)    (feed handler)       (slow, delayed OK)
```

Sab isi EK stream ko sunte — koi "personalized" version nahi milta (37 ka
fair-access principle). Fark sirf yeh hai **kitni jaldi** aur **kitni
correctly** tum use process kar paate.

---

## "Tick" — historical naam, aaj bhi use hota

> **Ek tick = feed mein ek atomic update event** (naya order, cancel,
> execute, ya trade).

Naam purane ticker-tape machines se aaya — har price change ek "tick"
sound/print karti thi. Aaj bhi "tick data" ka matlab **event-by-event**
market data hai (jaisa "tick size" — 37/08 — price ka minimum increment
hai; **alag concept, same word** — context se pata chalta).

---

## Trade data vs quote data vs order data

| Kya | Kya batata | Example |
|---|---|---|
| **Order data** (L3) | Har individual order ka add/cancel/modify | "order 12345, buy 100 @ 50.05, ADD" |
| **Quote data** (L1/L2) | Sirf price levels (aggregated), individual orders nahi | "bid 50.05 total qty 800" |
| **Trade data** | Kab-kab trade hua, kis price/qty pe | "trade @ 50.05, qty 30" |

38 mein hum zyaadatar **order-level (L3)** data ke saath kaam karenge — yeh
sabse rich hai, aur usi se L1/L2 derive ho sakte (02 mein detail).

---

## Latency ka source — 14-latency-budget (37) ka pehla real stage

37/14 ka pipeline diagram yaad karo: `feed handler` pehla stage tha. Market
data ka poora journey:

```
exchange event ---[network: propagation + switches]---> tumhara NIC
    ---[kernel/driver]---> tumhara socket buffer
    ---[YEH FOLDER: parse]---> decoded event
    ---[39: order book update]---> tumhara book state
```

Har hop latency add karta. **Yeh folder** specifically "parse" step pe
focus karta — aur dikhayega ki naive vs optimized parsing ka fark
(examples 03-06 mein measured) **10-25x** ho sakta ek single stage mein.
14 ka poora budget agar 1000ns ka hai, aur feed-handler stage ka apna
budget ~150-200ns, to is stage ka p99.9 agar 25x zyada ho jaaye (allocation
tail ki wajah se), poora budget wahin fail ho jaata.

---

## Yeh folder mein kya banega

Poora folder ek EK protocol define karta (`examples/wire_protocol.hpp`) —
ek ITCH-style binary format jise hum shuru se end tak le jaayenge:

```
01-message_structs      -- wire layout, static_asserts
02-endian_handling       -- byte-order conversion, measured cost
03-simple_parser         -- STEP 1: correct, naive
04-parser_benchmark      -- STEP 2: measure it
05-zero_copy_parser      -- STEP 3: optimize
06-parser_comparison     -- STEP 4: before/after, real numbers
07-market_data_simulator -- synthetic feed generator (deterministic)
08-gap_detection         -- sequence numbers, missing messages
09-ab_arbitration        -- dual-feed recovery
10-feed_handler          -- SAB EK SAATH -- the capstone
```

Yeh CLAUDE.md ka HFT process hai: **build simple → measure → find
bottleneck → optimize → re-benchmark → explain kya badla** — pehli baar is
folder mein full end-to-end apply hoga (16 mein poori kahani).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "market data" aur "order entry" ko confuse karna
Market data = READ path (exchange se tumhare paas aata, one-way, public).
Order entry = WRITE path (tum exchange ko bhejte, private, 37/02). Alag
protocols, alag latency characteristics, alag design constraints.

### Trap 2 — sochna "sabko same speed se milta"
Sab EK hi stream sunte, par kaun kitni jaldi PROCESS kar paata — yeh
colocation (37/11) + feed-handler quality (yeh poora folder) pe depend
karta. "Same data" ≠ "same effective latency."

### Trap 3 — market data ko "sirf prices" samajhna
Order data (L3) mein orders ka poora lifecycle hota — add, modify,
cancel, execute. Sirf "price updates" sochna bahut information discard
kar deta (jaise order flow imbalance signals, 37/05).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Market data = sirf trade prices | Order-level events (add/cancel/execute), trades sirf ek type |
| Sabko same latency se milta | Same DATA, alag effective latency (parsing/network speed pe depend) |
| "Tick" = tick size | "Tick" = ek update event; tick size = price increment (37/08). Alag concept, same word |
| Feed handler ek chhota, unimportant piece hai | Poore tick-to-trade budget ka pehla stage — yahan slow = sab kuch late |

---

## Exercises

1. Ek retail trading app (delayed data) aur ek HFT firm (real-time,
   colocated) — dono EK hi exchange se data lete. Kya farq hai?
   <details><summary>Answer</summary>
   Same underlying stream/events — farq **kab** aur **kitni jaldi**
   process hote hain. Retail app typically delayed feed (regulatory/cost
   reasons se, kabhi 15-min delay bhi) use karti aur latency-insensitive
   hoti. HFT firm real-time feed, colocated (37/11), aur microsecond-level
   parsing (yeh folder) use karti. Data same, "effective speed" bahut alag.
   </details>

2. Order data (L3) se quote data (L2) derive ki jaa sakti hai. Ulta —
   quote data se order data derive ho sakta kya?
   <details><summary>Answer</summary>
   Nahi. L2 sirf aggregated per-price-level totals deta — individual
   orders ka pata nahi (kaunsa order kab aaya, kiska tha). Information
   LOSS hai aggregation mein — ek baar aggregate ho gaya, individual
   detail wapas nahi milta. Isiliye L3 "richest" hai (02 mein detail).
   </details>

---

## Interview questions

1. Market data feed kya hai, aur exchange architecture (37/02) mein yeh
   kahan se aata?
2. "Tick" ka do alag matlab batao (event vs price increment) — confusion
   kahan se aata.
3. Order data, quote data, trade data — teeno ka fark.
4. Feed handler ka latency budget (37/14) mein kya role hai?

---

## Next
→ [`02-l1-l2-l3-data.md`](02-l1-l2-l3-data.md)
