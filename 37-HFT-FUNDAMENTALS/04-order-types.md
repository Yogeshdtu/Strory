# 04 — Order types: market, limit, IOC, FOK, stop, iceberg, post-only

## Prerequisites
- [`03-market-microstructure.md`](03-market-microstructure.md)

## Yeh topic abhi kyun
Order type ek strategy ka **precise control lever** hai — kis price pe, kab
tak wait karna hai, kya bacha hua qty cancel ho ya rest ho. Yeh sab galat
choose karna real financial loss hota hai, is liye har type ka **exact**
behavior pata hona zaroori hai.

---

## Do fundamental type: market vs limit

| | Market order | Limit order |
|---|---|---|
| Price specify karte ho? | Nahi — "jo bhi best price mile" | Haan — "is price ya behtar" |
| Fill guarantee? | **Haan** (agar book mein kuch hai) | Nahi — cross nahi kiya to resting reh jaata |
| Price guarantee? | **Nahi** — jo bhi book de de (slippage risk) | Haan — apni price se behtar kabhi nahi |
| Maker ya taker? | Hamesha **taker** (liquidity leta) | **Taker** agar cross kare, **maker** agar rest kare |

```
Book:  bids: 100.00 x50, 99.95 x200 | asks: 100.05 x30, 100.10 x400

  Market BUY qty=50   -> fills 30 @ 100.05, 20 @ 100.10 (avg 100.07, slippage!)
  Limit  BUY 100.05 qty=50 -> fills 30 @ 100.05, 20 rests @ 100.05 (no slippage on filled part)
  Limit  BUY 99.90 qty=50  -> book cross nahi karta, poora 50 rests @ 99.90
```

**Market order ka danger:** thin book mein, price bahut door tak "walk" kar
sakta (slippage) — HFT mein zyaadatar **limit orders** use hote, price
control ke liye.

---

## Time-in-force variants — kitni der tak order zinda rahe

| Type | Matlab |
|---|---|
| **Day** | Trading session ke end tak resting rehta (agar fill nahi hua) |
| **GTC** (Good Till Cancelled) | Explicitly cancel na ho tab tak (kai sessions tak) rehta |
| **IOC** (Immediate Or Cancel) | Jitna **turant** fill ho sake ho jaaye, **baaki turant cancel** — kabhi resting nahi hota |
| **FOK** (Fill Or Kill) | **Poora** turant fill ho, warna **poora cancel** (partial fill accept nahi karta) |

```
Book asks: 100.00 x30, 100.05 x400
  Incoming BUY IOC 100.05 qty=100  -> fills 30@100.00, 70@100.05 (100 filled) ... agar poora available na hota, jitna milta utna fill, baaki cancel — kabhi rest nahi hota
  Incoming BUY FOK 100.05 qty=1000 -> book mein sirf 430 available -> POORA cancel, 0 filled
```

> **HFT connection:** IOC bahut common hai HFT mein — "agar abhi is price pe
> mil raha to lo, resting risk (adverse selection, 03) nahi chahiye." FOK
> tab jab partial fill ka koi matlab hi nahi (e.g. ek arbitrage jahan dono
> legs poori honi hi chahiye — 10).

---

## Stop orders — condition-triggered

**Stop order**: rest nahi karta jab tak ek **trigger price** cross na ho,
tab activate hoke market/limit order ban jaata.

```
Current price: 100.00
"Stop-loss SELL, stop=98.00" -> jab last trade/price 98.00 ya neeche
  jaaye, tab yeh ek SELL order (market ya limit) ban jaata
```

Use case: risk management ("agar price itna gir jaaye, mujhe auto-exit
karna hai"). HFT systems risk layer (13) mein similar logic khud implement
karte (kill-switch style), exchange-level stop order kam use hota kyunki
control chahiye hota exactly kab/kaise react karna.

---

## Iceberg orders — size chhupana

**Iceberg**: ek bade order ka sirf **chhota visible portion** book mein
dikhta; jab woh fill ho jaata, agla chhota chunk auto-reveal hota, jab tak
poora order khatam na ho.

```
Iceberg SELL qty=10000, display=100 @ 50.00
  Book pe dikhta: 100 @ 50.00
  Fill hote hi:   agla 100 @ 50.00 reveal hota (total 9900 baaki, hidden)
```

**Kyun:** ek bada visible order dekh ke doosre participants price move kar
sakte (information leak — "koi bahut bechna chahta"). Iceberg market impact
kam karta size chhupa ke. Trade-off: revealed chunk **naya arrival** maana
jaata time-priority mein — poori size ka original time-priority nahi milta.

---

## Post-only — sirf maker rehna

**Post-only**: order sirf tab accept hota jab woh **turant cross na kare**
(resting/maker ban sake). Agar woh cross kar jaata (turant fill ho jaata,
taker ban jaata), exchange use **reject** kar deta (fill nahi karta).

**Kyun:** maker-taker fee model mein (09) makers ko **rebate** milta, takers
**fee** dete. Post-only guarantee karta ki tum galti se taker ban ke fee na
de do — agar market itni jaldi move ho gayi ki tumhara order ab cross kar
jaata, better woh reject ho jaaye.

---

## Order type decision table

| Chahiye | Use |
|---|---|
| Guaranteed fill, price matter nahi | Market |
| Price control, resting theek hai | Limit (Day/GTC) |
| Abhi fill karo jitna ho sake, resting NAHI chahiye | IOC |
| Sab-ya-kuch-nahi (multi-leg arb ka ek leg) | FOK |
| Condition pe react karna hai | Stop |
| Bada order, size chhupana hai | Iceberg |
| Hamesha maker rehna hai (rebate ke liye) | Post-only |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — market order thin book mein use karna
Bade market order + thin book = bahut slippage. Real systems bhi bade
orders ko limit + iceberg/algo-slicing se chalate.

### Trap 2 — IOC aur FOK confuse karna
IOC = **jitna mile utna lo** (partial theek hai). FOK = **poora ya kuch
nahi**. Multi-leg arbitrage mein galat choice se ek leg fill ho jaaye,
doosri na ho — **unhedged position** (bade risk).

### Trap 3 — post-only ko "guaranteed maker" ki jagah "guaranteed fill" samajh lena
Post-only sirf yeh guarantee karta ki agar accept hua to maker rahega —
**fill hona guarantee nahi hai** (agar cross karta, reject hi hota, fill
nahi).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Market order = fast limit order | Market = price control nahi, slippage risk |
| IOC aur FOK same hain | IOC partial theek hai, FOK sab-ya-kuch-nahi |
| Iceberg poori size ka time-priority rakhta | Har revealed chunk naya arrival hai |
| Post-only fill guarantee karta | Sirf "maker rahenge" guarantee karta, fill nahi |

---

## Exercises

1. Ek arbitrage strategy do legs trade karti — agar sirf ek leg fill ho aur
   doosra na ho, position risky ho jaati (unhedged). Kaunsa order type
   dono legs ke liye use karna chahiye, aur kyun?
   <details><summary>Answer</summary>
   FOK (Fill Or Kill) har leg ke liye. Agar koi leg poora fill nahi ho
   sakta, poora us leg ka order cancel ho jaana chahiye — taaki firm
   partial/unhedged position mein na phaňse. (IOC yahan risky hai —
   partial fill ho sakta, jo unhedged exposure chhod deta.)
   </details>

2. Ek HFT market maker apna bid post-only bhejta. Market itni jaldi move ho
   jaati ki bhejne ke waqt hi woh price ab best-ask ko cross kar rahi hoti.
   Kya hota?
   <details><summary>Answer</summary>
   Order reject ho jaata (fill nahi hota, resting bhi nahi hota) — kyunki
   post-only guarantee karta ki order sirf maker (resting) ban sakta hai;
   agar accept karne se turant taker fill ho jaata, exchange use process
   hi nahi karta.
   </details>

---

## Interview questions

1. Market aur limit order ka fundamental fark, aur slippage kis se aata.
2. IOC vs FOK — exact behavior farq batao ek example ke saath.
3. Iceberg order kyun use hota, aur uska time-priority trade-off kya hai.
4. Post-only kis problem ko solve karta (maker-taker fee context mein)?

---

## Next
→ [`05-bid-ask-spread.md`](05-bid-ask-spread.md)
