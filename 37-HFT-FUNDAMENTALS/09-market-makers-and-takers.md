# 09 — Maker vs taker, fees, rebates

## Prerequisites
- [`08-tick-size-and-lots.md`](08-tick-size-and-lots.md)

## Yeh topic abhi kyun
Maker/taker distinction poore business model ka economic foundation hai —
04 ka post-only order type isi ke liye tha. Ab economics samjho.

---

## Maker vs taker — ek line mein

> **Maker = jo liquidity book mein DAALTA hai (resting order, jo turant
> fill nahi hota). Taker = jo liquidity book se NIKAALTA hai (order jo
> existing resting order ko cross/fill karta).**

```
Order A rests in the book (limit order, price cross nahi karti)  -> MAKER
Order B aata hai aur Order A ko fill karta (cross karti hai)     -> TAKER
```

Ek hi order **kabhi maker, kabhi taker** ban sakta uske behavior pe
depend karta — ek limit order jo turant fill ho jaata (cross kar gaya)
**taker** hai; wahi price ka order agar market move na hone se turant fill
na ho, **maker** ban jaata (rest karta hai).

---

## Fee structure: maker-taker model

Kai exchanges (equity aur derivatives dono, region ke hisaab se) ek
**maker-taker fee model** use karte:

| | Typical direction |
|---|---|
| **Maker** | Rebate milta (paisa milta liquidity provide karne ke) |
| **Taker** | Fee deta (paisa deta liquidity consume karne ke) |

**Intuition:** exchange chahta hai zyada liquidity (resting orders) uski
book mein rahe — depth zyada = market attractive zyada = zyada volume. Isi
liye makers ko incentivize karta (rebate), takers se fee leke woh
subsidize hota.

**Exact rates, tiers, aur direction venue/instrument/membership-tier ke
hisaab se bahut alag hote** aur regularly change hote — yeh guide sirf
**model ka concept** samjhati, kisi specific current published rate ko
teach nahi karti (woh apne broker/exchange ke fee schedule se lo, always
current spec check karo).

Kuch venues **inverted model** bhi use karte (taker ko rebate, maker fee
deta) — kam common, par exist karta kuch instruments/venues mein.

---

## Iska strategy design pe impact

```
Ek strategy jo spread capture karti (buy at bid, sell at ask, dono
resting orders se):

  Gross P&L per round-trip = spread_captured
  Net P&L = spread_captured + maker_rebate*2 (agar dono legs maker the)

Agar wahi strategy accidentally taker ban jaati (kyunki apni hi resting
order ko cross kar gayi, ya market itni tez move hui):

  Net P&L = spread_captured - taker_fee*2
```

Ek chhoti si spread-capture strategy ke liye, **maker vs taker ka fee farq
poore trade ka profit/loss decide kar sakta.** Isiliye:
- **Post-only orders (04)** — guaranteed maker rehne ke liye.
- Fee-aware order routing (kis venue pe kaunsa fee tier) — bigger firms ke
  liye significant factor.

---

## Designated market makers — ek formal role

Kuch exchanges **formal market-maker agreements** karte specific firms ke
saath: badle mein obligations (minimum quote presence, maximum spread
width, ek % of trading time quote maintain karna) firms ko **behtar
rebate tier ya fee waiver** milta.

```
Obligation:  X% trading time dono taraf quote maintain karo,
             max spread Y bps se zyada nahi
Benefit:     behtar fee tier, kabhi exchange se direct rebate/payment
```

Yeh ek trade-off hai: **guaranteed liquidity** provide karne ka commitment,
badle mein **better economics**. Failure to meet obligations typically
tier downgrade ya penalty leti (venue-specific).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "market maker" ko sirf ek casual term samajhna
"Market maker" kabhi **role** (koi bhi jo liquidity provide karta, casual
sense) hota, kabhi **formal designated role** (exchange agreement ke saath,
obligations + benefits). Context se pata chalta konsa matlab hai.

### Trap 2 — fee structure ignore karke strategy P&L calculate karna
Chhote-margin strategies mein fee/rebate ek **significant fraction** ho
sakta gross P&L ka. Backtesting/simulation mein fees na include karna
overly optimistic results dega.

### Trap 3 — maker/taker ko "buy/sell" se confuse karna
Maker/taker ka buy/sell direction se koi lena-dena nahi — dono taraf
(buy ya sell) ka order maker ya taker ho sakta, uska "resting vs
crossing" behavior decide karta, na ki "kharida ya becha."

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Maker = buyer, taker = seller | Dono direction ka maker/taker ho sakta; resting vs crossing matter karta |
| Fee/rebate chhoti cheez hai, ignore kar sakte | Chhote-margin strategy mein poore P&L ka significant part ho sakta |
| Ek order hamesha maker ya hamesha taker rehta | Same order ka behavior situation-dependent hota (cross ki ya nahi) |
| Sab exchanges same fee model use karte | Model, direction, aur rates venue/instrument-specific hain |

---

## Exercises

1. Ek order limit price pe bheja jaata jo abhi best-ask se better hai
   (crosses immediately). Yeh maker ya taker?
   <details><summary>Answer</summary>
   Taker — order turant existing resting liquidity (ask side) ko cross/
   fill karta hai, khud resting nahi hota. Chahe order type "limit" ho,
   behavior (turant fill) hi maker/taker decide karta, order type nahi.
   </details>

2. Ek strategy ka gross spread-capture per trade bahut chhota hai (kuch
   ticks). Kyun is strategy ke liye maker-vs-taker status "kaafi bada
   deal" ho sakta?
   <details><summary>Answer</summary>
   Agar gross profit khud chhota hai (kuch ticks worth), to fee/rebate
   (jo bhi ek chhota fraction hota) us chhote profit ke against **relative
   mein bada** ho sakta — ek maker trade profitable ho sakta jo taker
   status mein loss mein badal jaaye, sirf fee/rebate difference se.
   </details>

---

## Interview questions

1. Maker aur taker define karo, buy/sell se independent examples ke saath.
2. Maker-taker fee model ka exchange ke liye kya incentive-design purpose
   hai?
3. Designated market maker ka trade-off kya hai (obligation vs benefit)?
4. Kyun spread-capture strategies fee/rebate ko P&L calculation mein zaroor
   include karti hain?

---

## Next
→ [`10-hft-strategies-overview.md`](10-hft-strategies-overview.md)
