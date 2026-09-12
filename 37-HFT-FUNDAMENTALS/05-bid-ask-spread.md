# 05 — Bid, ask, spread, mid, microprice, imbalance (measured)

## Prerequisites
- [`04-order-types.md`](04-order-types.md)
- `examples/02_spread_calculator.cpp`

## Yeh topic abhi kyun
Yeh woh **numbers** hain jo har strategy, har risk check, har display screen
constantly compute karti — L1 quote se nikalne wale core derived values. Book
concept (06) se pehle yeh vocabulary pakka honi chahiye.

---

## Basic terms

```
        BID                              ASK
   (buyers offer karte,           (sellers offer karte,
    yahan "buy" karna chahte)      yahan "sell" karna chahte)

   ...  99.95   100.00   |   100.05   100.10  ...
                  ^best bid   best ask^
                        <-- spread -->
```

| Term | Matlab |
|---|---|
| **Bid** | Highest price jispe koi ABHI khareedne ko taiyaar hai |
| **Ask / Offer** | Lowest price jispe koi ABHI bechne ko taiyaar hai |
| **Best bid / best ask** | Top-of-book — sabse achhi available price dono taraf |
| **Spread** | `ask - bid` — jitna chhota, utna "tight"/liquid market |
| **Mid price** | `(bid + ask) / 2` — ek reference price, khud tradeable nahi |
| **Depth** | Har price level pe kitni total qty resting hai |

Koi bhi trade **hamesha** best-bid-se-neeche ya best-ask-se-upar nahi ho
sakta bina ek naye price level create kiye — trade hamesha spread ke andar
ya usse cross karke hoti hai.

---

## Measured (`02_spread_calculator.cpp`)

```
BAL1    bid    100.00 x500.00   ask    100.10 x500.00  |  mid=   100.05  spread=  0.10 (  10.0 bps)  micro=  100.050  imb=  0.00
BUY-P   bid    100.00 x5000.00  ask    100.10 x500.00  |  mid=   100.05  spread=  0.10 (  10.0 bps)  micro=  100.091  imb=  0.82
SELL-P  bid    100.00 x500.00   ask    100.10 x5000.00 |  mid=   100.05  spread=  0.10 (  10.0 bps)  micro=  100.009  imb= -0.82
TIGHT   bid   2500.00 x800.00   ask   2500.05 x800.00  |  mid=  2500.03  spread=  0.05 (   0.2 bps)  micro= 2500.025 imb=  0.00
WIDE    bid     50.00 x300.00   ask     51.00 x300.00  |  mid=    50.50  spread=  1.00 ( 198.0 bps)  micro=   50.500 imb=  0.00
```

3 cheezein dhyaan se dekho:

1. **BAL1 / BUY-P / SELL-P ka `bid_px`, `ask_px`, `mid` bilkul IDENTICAL
   hai** — sirf resting **qty** ka ratio badla. Phir bhi microprice aur
   imbalance ne farq pakad liya. Mid price size-blind hai; microprice
   nahi.
2. **TIGHT vs WIDE**: absolute spread WIDE mein zyada (1.00 vs 0.05) hai,
   par **bps mein** WIDE bahut zyada wide hai (198 bps vs 0.2 bps) —
   kyunki WIDE ka price bhi chhota hai. Yeh bps hi hai jo do alag-price
   stocks ka spread fairly compare karta.

---

## Spread — bps mein kyun

Absolute spread compare karna galat hai jab prices bahut alag hon:

```
spread_bps = (ask - bid) / mid * 10000
```

Ek Rs 5 spread, Rs 100 ke stock pe = **500 bps** (bahut wide, illiquid).
Wahi Rs 5 spread, Rs 5000 ke stock pe = **10 bps** (tight, liquid). Absolute
number same, **relative** meaning bilkul alag.

---

## Microprice — size-weighted mid

```
micro = (bid_px * ask_qty + ask_px * bid_qty) / (bid_qty + ask_qty)
```

**Intuition:** micro = mid + ek "lean" jo **opposite-side qty** ke weight se
aata:
- `bid_qty` bahut bada (bahut buyers) -> micro **ask ki taraf** khinchta
  (BUY-P: micro=100.091, ask ke paas). Intuition: itni buying pressure hai
  ki ask level jaldi khaaya jaa sakta — price upar jaane ka zyada chance.
- `ask_qty` bahut bada (bahut sellers) -> micro **bid ki taraf** khinchta
  (SELL-P: micro=100.009, bid ke paas).

Yeh ek **common heuristic hai** (exact formula/weighting scheme paper aur
venue ke hisaab se thoda differ karta) — teaching purpose ke liye best hai
kyunki simple, interpretable, aur fast compute hoti (ek hot-path signal ban
sakti).

> **HFT connection:** microprice ek chhota "next price direction" signal
> hai — market making strategies isse apna quote skew karti (agar micro
> mid se upar hai, thoda upar-biased quote lagao) taaki adverse selection
> (03) kam ho.

---

## Order imbalance

```
imbalance = (bid_qty - ask_qty) / (bid_qty + ask_qty)     range: [-1, +1]
```

+1 ke paas = sab buying pressure, -1 ke paas = sab selling pressure, 0 =
balanced. Microprice jaisa hi signal, differently normalized — dono
literature mein widely use hote (10-hft-strategies-overview mein aage
aayega).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — mid price ko "tradeable" price samajhna
Mid koi resting order ki price nahi hai — koi bhi order mid pe fill nahi ho
sakta (bid se neeche ya ask se upar hi milega, mid dono ke beech ka
**reference** hai, real execution price nahi).

### Trap 2 — absolute spread compare karna alag stocks ke beech
Hamesha **bps** mein compare karo. Absolute Rs number sirf same-price
instruments ke beech meaningful hai.

### Trap 3 — spread ko sirf "cost" samajhna
Taker ke liye spread ek cost hai (aggressive order best price se worse
door tak "walk" kar sakta agar depth kam ho). Maker ke liye spread ek
**revenue source** hai (03).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Mid pe trade ho sakta | Mid sirf reference hai, tradeable price nahi |
| Absolute spread hamesha compare karo | bps mein compare karo (relative) |
| Mid hi sabse acha "fair price" estimate hai | Microprice size ko account karta, aksar behtar short-term estimate |
| Spread sirf cost hai | Maker ke liye revenue, taker ke liye cost |

---

## Exercises

1. Stock X: bid=99.90 x1000, ask=100.10 x1000. Stock Y: bid=999.00 x1000,
   ask=1001.00 x1000. Absolute spread dono ka 0.20 aur 2.00 — kaunsa
   "tighter" market hai (bps mein)?
   <details><summary>Answer</summary>
   X: mid=100.00, spread_bps = 0.20/100.00*10000 = 20 bps.
   Y: mid=1000.00, spread_bps = 2.00/1000.00*10000 = 20 bps.
   **Dono bilkul same tightness hain (20 bps)** — chahe absolute spread
   10x alag dikhe. Yeh exactly wahi trap hai jo bps normalize karta.
   </details>

2. `02_spread_calculator.cpp` mein BUY-P ka imbalance +0.82 hai. Yeh kis
   direction ka signal hai, aur kyun?
   <details><summary>Answer</summary>
   Bullish/upward lean signal — bid side pe bahut zyada resting qty hai
   (5000) ask side (500) se. Zyada buying interest ka matlab ask side
   jaldi khaa liya jaa sakta, price upar move karne ka chance zyada.
   Microprice (100.091, mid 100.05 se upar) yehi signal alag formula se
   confirm karta.
   </details>

---

## Interview questions

1. Spread ko bps mein kyun measure karte, sirf absolute number kyun nahi?
2. Microprice formula likho aur intuition explain karo.
3. Mid price aur microprice mein kab farq aata (specific scenario)?
4. Order imbalance kya signal deta, aur uska range kya hai?

---

## Next
→ [`06-order-book-concept.md`](06-order-book-concept.md)
