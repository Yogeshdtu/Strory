# 02 — Exchange andar se kaise kaam karta hai

## Prerequisites
- [`01-what-is-hft.md`](01-what-is-hft.md)

## Yeh topic abhi kyun
Har HFT system ka counterpart ek **exchange** hai — usko black-box maan ke
nahi chalega, tumhe pata hona chahiye woh andar kya karta hai, taaki tum
samajh sako tumhara system uske saath kaise interact karta hai (order bhejna,
data lena) aur latency kahan-kahan add hoti hai.

---

## Exchange kya hai — ek line mein

> **Ek exchange ek matching service hai**: buyers aur sellers ke orders
> collect karta, unhe rules (price-time priority — 07) se match karta, aur
> **sabko** result (trades + book updates) ek public feed pe broadcast karta.

Exchange khud trade nahi karta (zyaadatar — kuch have designated market
makers, alag topic). Yeh ek **neutral matching + broadcasting infrastructure**
hai.

---

## Exchange ke andar 4 core components

```
   Members (brokers, prop firms, HFT firms)
        |
        |  orders (naya/cancel/modify)
        v
   +------------------+
   |  ORDER GATEWAY    |  <- validate, auth, rate-limit, sequence
   +------------------+
        |
        v
   +------------------+
   |  MATCHING ENGINE  |  <- order book per symbol, price-time priority
   +------------------+
        |
        +---------------------------+
        v                           v
   +------------------+     +------------------+
   |  MARKET DATA OUT  |     |  TRADE/EXEC OUT   |
   |  (public feed)     |     |  (private, to     |
   |                    |     |   the two parties)|
   +------------------+     +------------------+
```

### 1. Order gateway
Tumhara order yahan pehle pahunchta. Yeh:
- Session/auth check karta (kaun bhej raha).
- Basic validation (symbol valid hai? qty > 0? price band ke andar? — 08).
- Ek **sequence number** assign karta (order ka arrival order fix ho jaata
  yahan — 07 ka time-priority isi pe based hai).
- Matching engine ko forward karta.

Yeh layer khud latency add karti — isliye colocation (11) is layer ke
jitna paas ho sake utna zaroori hai.

### 2. Matching engine
Har symbol ke liye ek order book (39-ORDER-BOOK), price-time priority (ya
pro-rata, 07) se match karta. Ek incoming order:
1. Existing book ke against check hota — cross karta? (buy price >= best
   ask, ya sell price <= best bid)
2. Cross karta to fill hota (poora ya partial), trade generate hota.
3. Baaki (agar kuch bacha aur order type limit/resting hai) book mein rest
   karta as a new resting order.
4. Cross nahi karta to poora resting ho jaata (limit order case).

Yeh **single-threaded per symbol** hona common hai (deterministic ordering
zaroori hai — do threads ek book pe simultaneously match karein to
price-time guarantee toot jaata). High-throughput exchanges symbols ko
alag matching engine instances/shards mein split karte.

### 3. Market data out (public)
Har book-changing event (naya order aaya, cancel hua, trade hua) turant
**sabko** ek feed pe broadcast hota — is se koi bhi participant apna local
book copy maintain kar sakta (38-MARKET-DATA). Yeh multicast hota hai
(ek-se-many, efficient) — 30-NETWORKING/42-HFT-NETWORKING mein detail.

### 4. Trade/execution confirmation (private)
Jinke orders match hue, unhe seedha ek confirmation milta ("tumhara order
X, qty Y, price Z pe fill hua") — yeh private channel hai, sirf involved
parties ko.

**Important:** market data feed (public) aur execution confirm (private)
**alag paths** hain, alag latency ho sakti — kabhi-kabhi tumhe apna hi trade
public feed pe pehle dikh jaata confirmation se pehle. Yeh race condition
handle karna feed-handler design ka part hai (38).

---

## Latency kahan add hoti hai (high level — 14 mein detail)

```
tumhara server -> [network] -> exchange gateway -> matching engine
      -> [network] -> market data / confirm -> tumhara server
```

Har hop latency add karta: tumhara NIC, network switch(es), exchange gateway
processing, matching engine processing, wapas same rasta. **Colocation
(11)** is poore path ka network hissa minimize karta — tumhara server
literally exchange ke datacenter mein hota.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sochna ki exchange khud "instant" hai
Matching engine bhi ek program hai, uski apni processing latency hai
(microseconds range, exchange/load ke hisaab se). Tumhara poora
tick-to-trade budget (14) isse bhi account karta.

### Trap 2 — market data aur trade confirm ko same channel samajhna
Alag paths, alag guarantees. Apna khud ka fill kabhi bhi assume mat karo
jab tak confirmation na aaye — sirf public feed pe apne jaisa trade dikhna
proof nahi (ambiguous — kisi aur ka bhi ho sakta usi price pe).

### Trap 3 — sab exchanges same architecture samajhna
Yeh generic model hai. Real exchanges implementation details mein alag
(sharding strategy, gateway count, protocol) — vendor docs padhna zaroori
hai jab kisi specific venue pe connect karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Exchange khud trade karta hai | Zyaadatar sirf neutral matching service hai |
| Order seedha matching engine ko jaata | Pehle gateway (auth, validate, sequence) |
| Market data aur trade confirm same cheez | Alag paths, alag latency, alag guarantees |
| Matching engine "instant" hai | Uski bhi apni processing latency hai (µs range) |

---

## Exercises

1. Tumhara order gateway ko pahunchta hai, validate hota, matching engine
   tak jaata — is beech kaunsa number assign hota jo baad mein price-time
   priority decide karega?
   <details><summary>Answer</summary>
   Sequence number, gateway pe assign hota (order ka arrival order yahin
   lock ho jaata). Isi wajah se colocation/network speed matter karta —
   jitni jaldi tumhara order gateway tak pahunche, utna behtar sequence
   number milne ka chance.
   </details>

2. Kyun matching engine typically single-threaded per symbol hota hai?
   <details><summary>Answer</summary>
   Deterministic price-time ordering guarantee karne ke liye. Agar do
   threads simultaneously ek hi book pe match karein, race condition se
   ordering guarantee toot sakti (kaunsa order "pehle" tha, ambiguous ho
   jaata). Single-threaded per symbol = ek natural serialization point.
   Alag symbols independent hote, to unhe alag threads/shards pe daal
   sakte (parallelism symbol-level pe milta, ek symbol ke andar nahi).
   </details>

---

## Interview questions

1. Order gateway aur matching engine ka kaam alag-alag batao.
2. Market data feed aur trade confirmation — alag kyun hain, iska practical
   impact kya hai apne system design pe?
3. Matching engine single-threaded per symbol kyun hota hai (common design)?

---

## Next
→ [`03-market-microstructure.md`](03-market-microstructure.md)
