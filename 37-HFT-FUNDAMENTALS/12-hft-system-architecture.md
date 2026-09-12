# 12 — Poora HFT system architecture

## Prerequisites
- [`01`](01-what-is-hft.md)–[`11`](11-colocation.md) (poora folder abhi tak)

## Yeh topic abhi kyun
Yeh lesson sab kuch jodta hai ek diagram mein — aur seedha 38-44 ka roadmap
banata hai. Har box iss diagram ka ek pura folder banega aage.

---

## Poora pipeline

```
                    EXCHANGE (02)
                        |
            (public market data feed, multicast)
                        |
                        v
   +------------------------------------+
   |         FEED HANDLER                |  <- folder 38
   |  parse wire format, decode,          |
   |  sequence-check, gap-detect          |
   +------------------------------------+
                        |
              (decoded book-update events)
                        v
   +------------------------------------+
   |         ORDER BOOK                   |  <- folder 39
   |  per-symbol book state,              |
   |  best bid/ask, depth, microprice     |
   +------------------------------------+
                        |
              (book state / book-change events)
                        v
   +------------------------------------+
   |         STRATEGY                     |  <- 10, aur 44-HFT-PROJECTS
   |  fair-value/signal compute,          |
   |  decide: quote? trade? cancel?        |
   +------------------------------------+
                        |
                (candidate order intent)
                        v
   +------------------------------------+
   |         RISK / PRE-TRADE CHECKS      |  <- 13
   |  position limits, price bands (08),  |
   |  fat-finger, kill-switch state       |
   +------------------------------------+
                        |
                  (approved order)
                        v
   +------------------------------------+
   |    ORDER MANAGEMENT SYSTEM (OMS)     |  <- folder 40 (matching engine
   |  order state tracking, ID mgmt,      |     folder ke aas-paas)
   |  ack/fill/cancel reconciliation      |
   +------------------------------------+
                        |
              (wire-format order message)
                        v
   +------------------------------------+
   |         ORDER GATEWAY (OUT)          |  <- folder 42 (networking)
   |  encode, send to exchange gateway     |
   +------------------------------------+
                        |
                        v
                    EXCHANGE (02)
```

---

## Har component ka job, ek line mein

| Component | Kaam | Folder |
|---|---|---|
| Feed handler | Raw bytes -> decoded events, correctly aur fast | 38 |
| Order book | Book state maintain karna, queries answer karna | 39 |
| Strategy | Signal/decision logic | 10, 44 |
| Risk/pre-trade | Har order ko approve/reject karna bhejne se pehle | 13 |
| OMS | Order lifecycle track karna (sent -> acked -> filled/cancelled) | 40 ke aas-paas |
| Order gateway (out) | Encode + network send | 42 |

---

## Data flow ke DO alag paths

Dhyaan do: yeh ek simple straight-line pipeline **nahi** hai — do
independent-timed cheezein ho rahi hain:

1. **Market data path** (top se): continuously, high-frequency, event-driven
   — book update hote hi strategy re-evaluate hoti.
2. **Order path** (jab strategy decide karti): kam frequency, par jab hota
   hai, **poori chain se guzarna padta** (risk check zaroori hai — 13, kabhi
   skip nahi hota, chahe kitna bhi "urgent" lage).

Yeh do paths **concurrently** chal rahe hote — market data continuously aa
rahi hai jab order bhi in-flight ho sakta. 26-CONCURRENCY, 41-HFT-
CONCURRENCY mein iska thread/lock-free design detail milega.

---

## Latency budget ka mapping (14 se preview)

```
  feed handler    ~150 ns   <- 38 ka focus
  order book       ~180 ns   <- 39 ka focus
  strategy          ~200 ns   <- strategy-specific
  risk check         ~90 ns   <- 13 ka focus
  order gateway out ~150 ns   <- 42 ka focus
  ----------------------------------
  tick-to-trade    ~770 ns   (+ network, dono taraf — 11, 30)
```

(Illustrative numbers, jaisa 14 aur example 03 mein — real numbers
hardware/strategy-specific hain.)

---

## Yeh course mein har box kahan banega

```
38-MARKET-DATA        -> feed handler (parse, decode, gap-detect)
39-ORDER-BOOK         -> order book (fast, O(1)-cancel data structure)
40-MATCHING-ENGINE    -> khud ka simplified exchange banao (dono taraf
                          se samajhne ke liye — tumhara book jo consume
                          karta, woh kaise generate hota hai)
41-HFT-CONCURRENCY    -> multi-thread design: feed thread, strategy
                          thread, risk thread — kaise safely communicate
                          karte (ring buffers, 15 se)
42-HFT-NETWORKING     -> kernel bypass, multicast receive, order send
43-HFT-OPTIMIZATION   -> case studies: poora pipeline profile karke
                          bottleneck-by-bottleneck optimize karna
44-HFT-PROJECTS       -> sab jodo, ek end-to-end simplified system
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — risk check ko "optional fast-path" banana
"Yeh trade itna obviously safe hai, risk check skip kar dete" — **kabhi
mat karo.** Risk check ka poora point hi yeh hai ki koi bhi bug (strategy
mein, market data mein) catastrophic order na bhej sake. Skip karna is
poore safety-net ko defeat karta (13 mein detail).

### Trap 2 — market-data path aur order path ko same thread/queue treat
karna
Market data high-frequency hai (continuously aati), order path
lower-frequency par higher-stakes. Inhe blindly ek hi serialized queue
mein daalna dono ki latency ko unnecessarily couple karta.

### Trap 3 — poore system ko "ek monolith" bana dena
Har box alag concern hai. Unhe mix karna (jaise strategy logic order-book
internals ke saath tightly coupled likhna) testing aur optimization dono
mushkil bana deta — 39 mein order book ek clean, standalone, testable unit
banayenge.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Poora system ek linear pipeline hai | Market-data path aur order path do alag-timed concurrent flows hain |
| Risk check kabhi-kabhi skip kar sakte | Kabhi nahi — safety-net ka poora point hi hamesha-on hona hai |
| Feed handler aur order book ek hi cheez hain | Feed handler PARSE karta, order book STATE maintain karta — alag concerns |
| Order gateway sirf "network send" hai | Encode + reliable send + acknowledgment tracking (OMS ke saath) |

---

## Exercises

1. Diagram mein "strategy" box ke baad seedha "order gateway" kyun nahi
   jaata — beech mein "risk" kyun zaroori hai?
   <details><summary>Answer</summary>
   Strategy logic mein bug ho sakta (galat calculation, stale state se
   decision), ya market data mein anomaly (bad tick) galat signal de
   sakta. Risk layer ek independent, simple, hamesha-verified safety net
   hai jo koi bhi single point of failure se catastrophic order (galat
   size, galat price, position limit cross) exchange tak jaane se rokta —
   chahe strategy "trust" ho.
   </details>

2. Market data path aur order path ko "concurrently chal rahe" kyun kaha
   gaya — inhe sequentially ek hi thread mein kyun nahi rakh sakte?
   <details><summary>Answer</summary>
   Market data continuously, high-frequency aati rehti (book update honi
   chahiye turant, chahe koi order in-flight ho ya nahi). Agar sab ek hi
   sequential thread mein hota, ek order-send/wait operation market-data
   processing ko block kar deta — book stale ho jaati exactly jab
   sabse zyada fresh hone ki zaroorat hai. Isliye alag concerns/threads
   (41-HFT-CONCURRENCY).
   </details>

---

## Interview questions

1. Poora HFT pipeline diagram banao (bina dekhe) — har box ka kaam batao.
2. Market-data path aur order path alag kyun rakhe jaate?
3. Risk layer poore pipeline mein kahan baithta, aur "kabhi skip nahi"
   kyun important principle hai?
4. Kaunsi cheez feed handler karta jo order book NAHI karta (aur vice
   versa)?

---

## Next
→ [`13-risk-systems.md`](13-risk-systems.md)
