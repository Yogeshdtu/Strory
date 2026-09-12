# 06 — Order book kya hai (concept level)

## Prerequisites
- [`05-bid-ask-spread.md`](05-bid-ask-spread.md)
- `examples/01_orderbook_concept.cpp`

## Yeh topic abhi kyun
Order book HFT ka **central data structure** hai — har strategy isi ko
padhti, har matching engine isi ko maintain karti. Yahan sirf **concept**
samjhenge (kya hota hai, kaise sorted rehta); production-grade fast version
(O(1) cancel, cache-friendly layout) **folder 39-ORDER-BOOK** mein banega.

---

## Order book = do sorted lists

```
                BIDS (buy side)              ASKS (sell side)
           price       qty                price       qty
         --------    -----              --------    -----
           100.48       200    <- best    100.50       150   <- best
           100.47       500                100.51       400
           100.46       800                100.52       900
           100.45      1200                100.53       300

         (descending: highest      (ascending: lowest
          price = best bid)          price = best ask)
```

- **Bids sorted descending** — highest price sabse "aggressive" buyer,
  best bid hai.
- **Asks sorted ascending** — lowest price sabse "aggressive" seller, best
  ask hai.
- **Best bid < best ask hamesha** (jab tak match/cross na ho jaaye — tab
  woh order match ho jaata aur book se nikal jaata, gap bana rehta).

---

## L2 vs L3 (preview — 38-MARKET-DATA mein detail)

Iss lesson ka model **L2** hai: har price level pe sirf **total qty**
(individual orders ka pata nahi). **L3** har individual order track karta
(kiska order, kab aaya — time-priority ke liye zaroori). Real matching
engines L3 rakhte (fill karne ke liye order-by-order chahiye); market data
feeds L2 (aggregated) ya L3 (full) dono forms mein publish ho sakte.

---

## Measured (`01_orderbook_concept.cpp`)

```
=== Initial book (top 4 levels each side) ===
   bid qty       px  |         px   ask qty
----------------------------------------------
       200    10048  |      10050       150
       500    10047  |      10051       400
       800    10046  |      10052       900
      1200    10045  |      10053       300

best bid = 10048  best ask = 10050  spread = 2 ticks  mid = 10049
```

Ek naya bid `10049` aata hai (best bid se upar, best ask se abhi bhi neeche
— non-crossing, isliye rest karta):

```
=== Naya bid 10049 aane ke baad ===
   bid qty       px  |         px   ask qty
----------------------------------------------
       100    10049  |      10050       150
       200    10048  |      10051       400
naya best bid = 10049
```

**Top-of-book badal gaya** — ek naya event jo har strategy ko turant pata
chalna chahiye (yeh signal — book-change events — kaise fast propagate hote
hain, woh 38-MARKET-DATA aur 42-HFT-NETWORKING mein).

Ab yeh order **poora cancel** ho jaata:

```
=== 10049 poora cancel hone ke baad ===
best bid wapas = 10048
```

Best bid **automatically** apne aap agle level pe "fall back" karta —
kyunki data structure sorted hai, best hamesha `begin()` hai. Yeh price
movement ka ek chhota real source hai: ek level poora khaali ho jaana bhi
price ko "move" kara deta (visible book ke terms mein).

---

## Yeh book kaam kaise karti — 3 operations

| Operation | Kya karta |
|---|---|
| **Add** | Naya order — cross karta to fill(s), baaki resting; cross nahi karta to seedha rest |
| **Cancel** | Ek resting order (ya uska hissa) hataana — qty subtract, 0 pe level hata do |
| **Modify** | Zyaadatar exchanges mein **cancel + new** ke barabar treat hota (naya time-priority milta) — kuch venues "price-preserving modify" allow karte jo time-priority nahi todta agar sirf qty ghati ho |

`add`/`cancel`/`modify` — bas yeh 3 operations hain jo har market data
update (38-MARKET-DATA) represent karta.

---

## Depth aur book "shape"

```
     bid depth (cumulative qty upto N levels)
                                                    ask depth
   1200+800+500+200 = 2700    <- 4 levels ->    150+400+900+300 = 1750
```

Depth batata **kitni size** tum trade kar sakte ho bina price move kiye
zyada. Chhoti depth = thoda size hi kaafi hai price hilaane ke liye (03 ka
"liquidity" yehi hai, mathematically).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sochna ki order book "sirf best bid/ask" hai
Best bid/ask **L1** hai — sirf top. Poori book (L2/L3) depth deti hai, jo
zyaadatar HFT strategies ke liye zaroori hai (sirf L1 se limited signal
milta).

### Trap 2 — price ko `double` mein rakhna
`01_orderbook_concept.cpp` price ko **integer ticks** mein rakhta hai, na
ki `double`. Kyun: floating-point equality unreliable hai (03-VARIABLES/06
— `0.1 + 0.2 != 0.3`), aur book mein price equality baar-baar check hoti
(level lookup, matching). Integer ticks isse permanently avoid karte
(08-tick-size-and-lots mein detail).

### Trap 3 — `std::map` ko "production-ready" samajhna
`01_orderbook_concept.cpp` ka `std::map`-based book **concept ke liye**
theek hai (log-time insert/find/erase, readable code). Real HFT order book
isse bahut tez hota — O(1) cancel (per-order handle se seedha node tak),
cache-friendly flat array/intrusive-list layout. Yeh **measured, optimized**
version 39-ORDER-BOOK mein banega — is folder ka kaam sirf concept pakka
karna hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Order book = sirf best bid/ask | Woh L1 hai; poori book (levels) L2/L3 hai |
| Price double mein rakhna theek hai | Integer ticks — equality reliability ke liye |
| Modify = sirf qty change, time-priority same rehta | Zyaadatar venues pe modify = cancel+new (naya time-priority) |
| `std::map`-based book production-speed hai | Concept ke liye theek, real HFT book bahut tez hoti (39) |

---

## Exercises

1. Best bid poora cancel ho jaata hai. Book ka naya best bid kaise decide
   hota, code mein?
   <details><summary>Answer</summary>
   `std::map<Price, Qty, std::greater<Price>>` mein `.erase()` ke baad
   `.begin()` automatically agla-highest price return karta — data
   structure khud sorted hai, koi explicit "find new best" logic nahi
   likhni padi.
   </details>

2. Price ko `double` mein rakhne se order book mein specifically kaunsi do
   operations galat ho sakti?
   <details><summary>Answer</summary>
   (a) **Level lookup/matching** — do supposedly-same price `double` mein
   rounding se alag bit-pattern ho sakte, `map::find` fail ho jaata ya do
   alag levels ban jaate jo asal mein ek hi price the. (b) **Cross check**
   (`buy_price >= ask_price`) — boundary case pe rounding error se galat
   decision (cross hona chahiye tha, nahi hua ya ulta) ho sakta.
   </details>

---

## Interview questions

1. L1, L2, L3 market data mein fark batao.
2. Order book mein price ko integer ticks mein kyun rakhte, `double` mein
   kyun nahi?
3. Add/cancel/modify — teeno operations explain karo, aur "modify"
   zyaadatar venues pe kya actually karta.
4. `std::map`-based book aur production HFT order book mein performance
   fark ka main source kya hoga (guess, 39 se pehle)?

---

## Next
→ [`07-price-time-priority.md`](07-price-time-priority.md)
