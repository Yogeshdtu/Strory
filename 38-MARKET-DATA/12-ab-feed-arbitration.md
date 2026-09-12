# 12 — A/B feed arbitration (measured)

## Prerequisites
- [`04-sequence-numbers.md`](04-sequence-numbers.md)
- `examples/09_ab_arbitration.cpp`

## Yeh topic abhi kyun
04 mein gap detect karna seekha. Ab **ek trick** dekhte hain jo bahut se
gaps ko **bina kisi round-trip/retransmission ke** fill kar deta —
production HFT feed handlers mein yeh near-universal practice hai.

---

## Problem: UDP loses packets

Market data typically **UDP multicast** se aati (30-NETWORKING — UDP
kyun: no handshake, no retransmit-delay, ek-se-many broadcast efficient).
Trade-off: **UDP delivery guarantee nahi deta** — packets kabhi-kabhi
kho jaate (network congestion, buffer overflow, switch issues).

Ek single feed path pe, kuch % loss **normal** hai — chhota, par zero
nahi.

---

## Solution: EK data, DO INDEPENDENT paths

> **Exchange SAME market data ko DO alag multicast groups (feed "A" aur
> feed "B") pe simultaneously bhejta — alag network paths se.**

```
                    Exchange
                   /         \
            Feed A             Feed B
         (path 1, e.g.      (path 2, e.g.
          primary switch)    backup switch)
               |                   |
               v                   v
          Consumer receives BOTH, ARBITRATES
```

**Key insight:** agar A aur B **independent paths** hain (alag switches/
routes), ek packet ka A pe khona aur SAME packet ka B pe khona **largely
uncorrelated events** hain. Dono paths pe EK SAATH khona bahut kam
probable hai.

---

## Arbitration logic

```
Har sequence number ke liye:
  agar A mein mila -> use karo
  nahi to agar B mein mila -> use karo (A ka loss, B ne cover kar diya)
  dono mein nahi mila -> TRUE gap (13-recovery zaroori)
```

Simplest version: **"pehla jo aaye, use karo"** (real-time streaming
context mein — jo bhi feed pehle deliver kare us sequence number ka
message). Batch/offline context (jaisa example 09) mein: dono ko fully
receive karke compare karo.

---

## Measured (`09_ab_arbitration.cpp`)

```
=== A/B feed arbitration (20000 messages upstream) ===

Feed A alone:  19802 / 20000  (198 missing)
Feed B alone:  19807 / 20000  (193 missing)

in BOTH A and B:      19613
only in A (B dropped): 189
only in B (A dropped): 194
in NEITHER (both dropped -- true gap): 4

=== Arbitrated (A+B combined) result ===
recovered by combining (present in exactly one feed): 383
total present after arbitration: 19996 / 20000  (4 STILL missing)

single-feed loss: ~0.99%   arbitrated loss: ~0.02%  (49.5x better, ZERO round-trips)
```

**~1% single-feed loss → ~0.02% arbitrated loss — ~49.5x improvement,
ZERO retransmission requests, ZERO extra round-trips.** Yeh bilkul FREE
hai (assuming dono feeds already receive kiye jaa rahe hain — jo extra
bandwidth/CPU cost hai, par woh latency add nahi karta jaisa 13's
retransmission karega).

**Yeh independent-loss assumption pe depend karta:** dono paths alag hone
chahiye (alag switches/routes, ideally alag NICs bhi tumhare consumer end
pe). Agar A aur B **same physical path** share karte kahin, unka loss
correlated ho jaayega, aur arbitration ka faayda kam ho jaata.

---

## "Neither" case — 4 messages, dono feeds mein missing

Yeh **true gaps** hain — dono independent paths pe SAME message khona
(rare, par zero nahi). Inke liye **13-recovery-and-retransmission** chahiye
— A/B arbitration sab kuch cover nahi karta, par **zyaadatar** cover kar
deta (yahan 96% se 100% reduction single-feed-loss ke against).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — dono feeds ko "redundant" (waste) samajhna
Zyaadatar time (yahan 98%+), dono feeds mein message present hota —
"redundant" lagta. Par woh 2% jahan sirf ek mein hai, wahi is poore
mechanism ka poora point hai.

### Trap 2 — A aur B ko same path pe route hone dena (accidentally)
Agar network config galat hai aur dono feeds effectively ek hi switch se
guzarti hain, unka loss correlated ho jaata — arbitration ka statistical
faayda gayab ho jaata. Network topology verify karna zaroori hai.

### Trap 3 — "arbitrated loss 0.02% hai, ab safe hain" soch lena
0.02% > 0%. **True gaps abhi bhi ho sakte** (yahan 4 out of 20000) — 13
ka recovery mechanism **zaroori hai**, A/B usse replace nahi karta, sirf
uska zaroorat kam karta.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| A/B arbitration = zero loss guarantee | Dramatically kam loss, zero nahi (49.5x behtar, na ki infinite) |
| Dono feeds redundant/waste hain | Independent losses ko cover karte — poora point yehi hai |
| A/B arbitration retransmission ka replacement hai | Complement hai — zyaadatar gaps free mein fill karta, baaki 13 handle karta |
| Feed A aur B same path pe ho sakte hain, farq nahi padta | Independent paths zaroori — correlated loss faayda khatam kar deta |

---

## Exercises

1. Agar feed A aur B DONO same network switch se guzarti (galti se same
   path), kya A/B arbitration ka statistical faayda barkarar rahega?
   <details><summary>Answer</summary>
   Nahi, ya bahut kam ho jaayega. Arbitration ka poora faayda "independent
   loss" assumption pe depend karta — agar same switch pe congestion/issue
   ho, dono feeds SAME packets khoyenge (correlated loss), aur "doosre se
   fill karo" ka faayda gayab ho jaata. Network design verify karna
   (alag physical paths) is technique ke kaam karne ke liye zaroori hai.
   </details>

2. `09_ab_arbitration.cpp` mein "only in A" (189) aur "only in B" (194)
   roughly barabar hain (~1% each), jo consistent hai independent ~1%
   loss ke saath. Agar yeh numbers bahut ASYMMETRIC hote (jaise A: 5,
   B: 380), kya suspect karoge?
   <details><summary>Answer</summary>
   Ho sakta hai feed B ka path zyada lossy ho (worse network route,
   congestion, ya hardware issue us specific path pe) — asymmetric loss
   rates ek diagnostic signal hai ki DONO paths equally healthy nahi hain,
   aur B ke path ko investigate karna chahiye (network monitoring, 42-HFT-
   NETWORKING ka scope).
   </details>

---

## Interview questions

1. A/B feed arbitration kaise kaam karta, aur kyun yeh "independent
   paths" pe depend karta?
2. Measured example mein single-feed loss aur arbitrated loss ka fark
   batao — kitna improvement mila?
3. A/B arbitration ke baad bhi "true gaps" kyun possible hain?
4. A/B arbitration aur retransmission (13) — dono ka relationship
   (replace vs complement) batao.

---

## Next
→ [`13-recovery-and-retransmission.md`](13-recovery-and-retransmission.md)
