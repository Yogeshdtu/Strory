# 03 — Limit order handling: resting vs aggressing

## Prerequisites
- `02-matching-algorithm.md`
- `37-HFT-FUNDAMENTALS/04-order-types.md`

## Yeh topic abhi kyun

Limit order sabse COMMON order type hai — aur uska behavior do modes mein
split hota hai: jab woh **aggress** karta (kisi resting order se turant
match karta) vs jab woh **rest** karta (book mein baith jaata, future
match ka wait karta). Yeh do modes SAME order ke andar EK submit() call
mein bhi mix ho sakte (partial fill + partial rest).

---

## Maker vs taker (37/09 se recall)

| | Aggressor ("taker") | Resting ("maker") |
|---|---|---|
| Kab | Jab order turant match karta | Jab order book mein baith kar wait karta |
| Kaun decide karta price | Resting order ki price "wins" (07 mein detail) | Apni price se commit karta |
| Fee model (real venues) | Zyaadatar fee LETA (liquidity leta) | Zyaadatar rebate PAATA (liquidity deta) |

Ek SINGLE limit order dono ban sakta ek hi call mein: jitna cross karta
(aggressor), baaki jo REST hota (ab maker) — 01_matching_engine.cpp ka
order id=4 exactly yeh demonstrate karta.

---

## `submit()` mein Limit order ka poora flow

```cpp
// matching_engine.hpp -- submit()
if (incoming.qty == 0) {
    return {OrderStatus::Filled, ...};            // poora aggress ho gaya
}
if (incoming.type == OrderType::Limit && !stp_aborted) {
    add_resting(incoming);                         // leftover REST karo
    return {trades.empty() ? OrderStatus::New       // ZERO cross, seedha resting
                            : OrderStatus::PartiallyFilled,  // KUCH cross, baaki resting
            ...};
}
```

Teen possible outcomes ek Limit order ke liye:
1. **Poora fill** (`Filled`) — sab cross ho gaya, kuch resting nahi.
2. **Zero fill** (`New`) — kuch cross NAHI karta, poora resting.
3. **Partial** (`PartiallyFilled`) — kuch cross hua, baaki resting.

---

## ⚠️ Recurring trap — `bids_`/`asks_` alag map types

`matching_engine.hpp` mein `bids_` (`std::map<Price, Level, std::greater<Price>>`)
aur `asks_` (`std::map<Price, Level>`) **alag C++ types** hain (comparator
type-signature ka hissa hota `std::map` ke liye). Isliye:

```cpp
// ❌ COMPILE ERROR -- ternary donon branches ka common type nahi nikal sakta
auto& side = is_buy ? bids_ : asks_;
```

Yeh EXACT SAME trap hai jo 37/01 aur 39/03 mein pehle bhi mila — fix bhi
same: templated private helper (`add_resting_impl<Map>`, `cancel_impl<Map>`)
+ explicit `if/else`/ternary-of-CALLS (function selection, not value
selection) dispatch:

```cpp
// ✅ SAHI
void add_resting(const Order& o) {
    if (o.is_buy) add_resting_impl(bids_, o); else add_resting_impl(asks_, o);
}
```

Ternary function-CALL choose karna (`is_buy ? f(bids_) : f(asks_)`) bhi
theek hai (dono calls SAME return type return karte) — sirf ternary se
REFERENCE nikalna (`is_buy ? bids_ : asks_`) galat hai.

---

## Real-world use: kyun Limit sabse common hai

37/04 mein bataya gaya tha — HFT mein price control ka mahatva (slippage
avoid karna). Isi wajah se market makers hamesha Limit orders use karte
(apni price commit karte, guaranteed fill nahi chahte). Zyaadatar retail
aur institutional order flow bhi Limit hi hota.

> **HFT relevance:** ek market-making strategy continuously Limit orders
> post/cancel/replace karti dono taraf (bid + ask) — spread capture karne
> ke liye. Iska matlab hai matching engine ki **resting-order path**
> (add/cancel/replace) utni hi latency-critical hai jitni matching-path
> (37/14's latency budget mein "order entry" stage).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Limit order ya to poora aggress karta ya poora rest karta | Dono ek saath ho sakte -- partial fill + partial rest, EK hi submit() call mein |
| `is_buy ? bids_ : asks_` compile ho jaayega | Alag map types -- templated helper + dispatch chahiye |
| Limit order "safe" hai, kabhi reject nahi hota | Duplicate id pe REJECT ho sakta (09-order-id-lookup jaisa 39) |

---

## Exercises

1. Ek Limit BUY order book ke best ask se BEHTAR price offer karta (jaisa
   `03_trade_events.cpp`'s test #1). Trade kis price pe hoga -- incoming
   ki price pe, ya resting ki?
   <details><summary>Answer</summary>
   Resting (maker) ki price pe -- "price improvement" aggressor ko milta
   (usse apni limit se BEHTAR price pe fill milta), exchange ki taraf se
   koi extra profit nahi banta is trade se. Poori detail 07-trade-events.md.
   </details>

2. Kyun `add_resting_impl` ek TEMPLATE function hai, do alag (non-template)
   overloads (ek `bids_` ke liye, ek `asks_` ke liye) kyun nahi?
   <details><summary>Answer</summary>
   Template ek hi code likhne deta jo dono `Map` types (`bids_`'s aur
   `asks_`'s, jo comparator ke wajah se alag types hain) ke saath kaam
   kare -- duplicate logic (DRY) avoid karta. Do overloads bhi kaam karte,
   par identical body do baar likhna padta (maintenance burden).
   </details>

---

## Interview questions

1. Maker aur taker ka fark, ek hi order dono ban sakta hai kya?
2. Limit order ke 3 possible outcome statuses batao.
3. `bids_`/`asks_` alag map types wala trap kyun hota, kaise fix hota?

---

## Next
→ [`04-market-orders.md`](04-market-orders.md)
