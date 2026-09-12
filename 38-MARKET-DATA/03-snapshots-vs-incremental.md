# 03 — Snapshots vs incremental updates

## Prerequisites
- [`02-l1-l2-l3-data.md`](02-l1-l2-l3-data.md)

## Yeh topic abhi kyun
Yeh design decision poore feed handler ki **correctness** ka foundation
hai — aur yeh samjhna 04 (sequence numbers) aur 13 (recovery) dono ke
"kyun zaroori hain" ka jawab deta hai.

---

## Do tareeke market data publish karne ke

| | Snapshot | Incremental |
|---|---|---|
| Kya bhejta | **Poori book ki current state**, ek baar | Sirf **delta** — jo abhi CHANGE hua |
| Size | Bada (poori book) | Chhota (ek event) |
| Self-sufficient? | **Haan** — apne aap complete hai | **Nahi** — pichli state ke bina meaningless |
| Frequency | Kam (periodic, ya on-demand) | Zyada (har change pe) |
| Bandwidth (steady state) | Zyada agar baar-baar bhejo | **Kam** — sirf jo badla |

---

## Incremental kyun default hai (bandwidth)

Ek active book mein per-second sainkdo-hazaaron updates ho sakte, par har
update **poori book ke comparison mein chhota** hota (ek order add/cancel/
execute — chhoti si change). Har baar POORI book bhejna (snapshot-only)
bandwidth mein bahut wasteful hota.

```
Book: 500 price levels, 5000 resting orders

Snapshot: poori 5000-order state ~kai KB
Incremental: "order 41823, cancel" ~kuch bytes

Agar 10,000 changes/sec ho rahi:
  Snapshot-only: 10,000 x kai KB  = HUGE bandwidth
  Incremental:   10,000 x kuch bytes = tiny
```

Isiliye real feeds **incremental-primary** hote — poori book baar-baar
resend nahi hoti, sirf changes.

---

## Par incremental ka ek fundamental problem hai

> **Incremental updates SELF-SUFFICIENT nahi hote.** Agar tum feed BEECH
> mein join karo (late), ya EK update miss kar do, tumhari local book
> state **permanently galat** ho jaati — aur agla har incremental update
> bhi galat base pe apply hoga.

```
Exchange book (real):  100.00 x500

Tum feed join karte ho DER se, ya ek "Cancel 200 @ 100.00" update MISS
karte ho:
  Tumhari local book:   100.00 x500   (purani, galat)
  Real book (exchange): 100.00 x300   (cancel ho chuka)

Agla update aata: "Execute 50 @ 100.00"
  Tumhari book:  100.00 x450   (500 - 50)   <- GALAT
  Real book:     100.00 x250   (300 - 50)   <- SAHI
```

**Ek miss ho gaya update = tumhari book permanently drift kar gayi**, jab
tak kuch use fix na kare. Yeh galat state silently reh sakti — koi crash
nahi hota, bas numbers galat hote (worst kind of bug).

---

## Solution: snapshot se SHURU karo, phir incremental follow karo

```
       [SNAPSHOT: poori book, EK sequence number ke saath tagged]
                              |
                              v
       [INCREMENTAL updates, seq N+1, N+2, N+3, ...]
                              |
                              v
              tumhari local book = snapshot + har incremental
```

Real feeds isliye **do channels** publish karte:
1. **Incremental channel** — continuous, har change.
2. **Snapshot channel** — periodic (jaise har few seconds, ya on-demand
   request pe) — poori book state ek known sequence number ke saath.

Ek naya joining consumer:
1. Latest snapshot lo (isse tumhari book ek **known-correct** state pe
   shuru hoti, ek specific sequence number ke saath tagged).
2. Us sequence number se AAGE ke incremental updates apply karna shuru
   karo.
3. Ab tumhari book snapshot + incrementals = exchange ki real state
   (jab tak koi incremental miss na ho — 04, 13).

---

## Yeh sequence numbers (04) ko zaroori kyun banata hai

Snapshot + incremental ka poora model **sequence numbers pe depend karta**:
- Snapshot batata "main sequence number X tak ki state hoon."
- Tumhe pata hona chahiye agla incremental **X+1** hona chahiye.
- Agar X+2 aa jaaye X+1 ki jagah, tumhe **turant** pata chalna chahiye ki
  ek update miss hua — warna silent drift (upar wala example).

Yeh 04-sequence-numbers.md ka poora motivation hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sochna "incremental hamesha sahi order mein pahunchti"
UDP (jo market data ke liye common hai, 30-NETWORKING) **order guarantee
nahi karta**, aur packets **lost** ho sakte. Bina sequence-number-checking
ke, tumhe pata hi nahi chalega ki kuch miss/reorder hua.

### Trap 2 — snapshot ko "ek-baar ki cheez" samajhna
Snapshots **periodically** publish hoti (ya on gap-detect on-demand
request se — 13) — sirf startup pe nahi. Long-running system agar kabhi
resync chahiye (galat state detect hui), snapshot channel hamesha
available hona chahiye.

### Trap 3 — snapshot lagne tak incremental buffer NA karna
Agar tum snapshot ka wait kar rahe ho, is beech aa rahe incrementals ko
**discard mat karo** — buffer karo, snapshot aane ke baad unme se sirf
snapshot-ke-sequence-number-se-AAGE wale apply karo (warna snapshot aur
tumhari book ke beech ka gap phir se miss ho jaayega).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Incremental hamesha kaafi hai | Self-sufficient nahi — ek base (snapshot) chahiye |
| Snapshot sirf startup pe chahiye | Periodic/on-demand bhi — resync ke liye |
| UDP order/delivery guarantee karta | Nahi — isliye sequence numbers zaroori (04) |
| Miss hua update = crash hota | Nahi — silent, permanently galat state (worse) |

---

## Exercises

1. Tum feed ko 10:00:05 baje join karte ho. Book mein pehle se hazaaron
   orders hain (10:00:00 se chal rahi). Sirf incremental sunne se kya
   hoga?
   <details><summary>Answer</summary>
   Tumhari local book **khaali** shuru hogi, par incremental updates
   pehle-se-existing orders ke against (cancel/execute) aayenge jo
   tumhari (khaali) book mein hain hi nahi. Book turant inconsistent ho
   jaayegi. Fix: pehle latest snapshot lo (poori state, ek sequence
   number tagged), phir us point se aage incrementals apply karo.
   </details>

2. Ek incremental update MISS ho jaata hai, par tumhara system use detect
   nahi karta (sequence check nahi hai). Kya symptom dikhega?
   <details><summary>Answer</summary>
   Koi crash/error nahi — bas book **silently galat** ho jaati (upar ka
   worked example). Yeh sabse dangerous class ka bug hai kyunki koi alert
   nahi milta; strategy galat book pe decisions leti rehti, jab tak kisi
   external check (jaise price sanity check, ya periodic snapshot
   comparison) se pakda na jaaye.
   </details>

---

## Interview questions

1. Snapshot aur incremental updates ka trade-off (bandwidth vs
   self-sufficiency) batao.
2. Ek naya consumer feed mein kaise safely join karta (snapshot + seq
   number se)?
3. Ek miss hui incremental update ka symptom kya hota, aur kyun yeh
   dangerous hai?
4. UDP (order/delivery guarantee na dena) is design ko kaise affect karta?

---

## Next
→ [`04-sequence-numbers.md`](04-sequence-numbers.md)
