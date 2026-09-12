# 13 — Recovery aur retransmission

## Prerequisites
- [`12-ab-feed-arbitration.md`](12-ab-feed-arbitration.md)
- [`03-snapshots-vs-incremental.md`](03-snapshots-vs-incremental.md)

## Yeh topic abhi kyun
12 ne dekha A/B arbitration zyaadatar gaps free mein fill karta — par
**"true gaps"** (dono feeds mein missing) abhi bhi bachte hain. Yeh lesson
un bache-hue gaps ka **last resort** hai.

---

## Recovery ke 3 layers (increasing cost order mein)

```
1. A/B arbitration (12)     -- FREE, already receiving both, koi extra
                                round-trip nahi
2. Retransmission request     -- ek extra round-trip (TCP request/response)
3. Snapshot resync (03)       -- POORI book state re-fetch, sabse mehnga
```

Har layer **pichle se mehnga** hai — isliye order **isi sequence** mein
try karna chahiye: pehle A/B se dekho fill hota hai ki nahi, tabhi
retransmission maango, aur snapshot sirf tab jab retransmission bhi
kaam na aaye (ya gap itna bada ho ki retransmission impractical ho).

---

## Layer 2: retransmission request

> **Consumer exchange ko bolta "mujhe seq X se Y tak ke messages FIR SE
> bhejo" — typically ek ALAG (TCP-based) channel pe, hot-path se off.**

```
Consumer:  "seq 4501-4503 missing, please retransmit"  --[TCP request]-->
Exchange:  [4501][4502][4503]                          <--[TCP response]--
```

**Kyun TCP, na ki hot-path UDP?** Retransmission **rare** hai (gap detect
hone pe hi trigger hota), aur **reliable delivery chahiye** (agar
retransmission response bhi kho jaaye, poora recovery fail ho jaata) —
TCP ki guarantee yahan sahi trade-off hai (30-NETWORKING: TCP reliable
par slower/more-overhead, UDP fast par unreliable — yahan reliability
priority hai, latency itni critical nahi kyunki yeh already ek "kuch galat
ho gaya" recovery path hai, not the hot path).

**Latency cost:** ek poora round-trip (request bhejo, response ka wait
karo) — jitna bada gap, utna zyada data wait karna padta. Is beech affected
symbols "uncertain" state mein rehte (04 se).

---

## Layer 3: snapshot resync

Agar retransmission bhi fail ho (gap bahut bada, ya retransmission
channel khud down), ya consumer **bahut** peeche ho gaya ho (jaise
connection drop ke baad reconnect), **poori book state snapshot se
re-fetch karna** aakhri option hai (03 ka snapshot channel).

```
1. Latest snapshot lo (poori book, ek sequence number tagged)
2. Us sequence number se AAGE ke incremental updates apply karo
3. Book ab known-correct state mein
```

**Mehnga kyun:** poori book (500+ price levels, hazaaron orders) transfer
karna, parse karna, apply karna — retransmission (kuch specific messages)
se bahut zyada data/time.

---

## Is beech kya karo — "go stale"

Jab tak recovery (koi bhi layer) complete na ho, affected symbol/book
**"stale" mark honi chahiye**:

```cpp
enum class BookState { LIVE, RECOVERING, STALE };
```

- **Trading rok do** us symbol pe jab tak state wapas `LIVE` na ho
  (37/13 ka fail-closed principle — galat book pe trade karna galat
  decisions deta).
- **Strategy ko explicitly batao** book stale hai — silent continue
  karna 03 ka wahi "silent drift" problem hai, bas manually triggered.
- **Recovery complete hone pe**, state `LIVE` wapas, trading resume.

---

## Design decision: "kab tak retry karo, kab give up"

```
Gap detect -> A/B try karo (12, ~instant) ->
    fill hua? -> LIVE, continue
    nahi hua? -> retransmission request bhejo (timeout ke saath) ->
        response mila? -> apply karo, LIVE
        timeout? -> snapshot resync (bada, mehnga) ->
            done? -> LIVE
            symbol/venue khud down? -> STALE, alert operator (13/37's
                                        kill-switch-jaisa escalation)
```

Har step ka **timeout** hona chahiye — infinite wait "stuck" state hai
(book kabhi LIVE nahi hoti, silently kabhi trading resume nahi hoti — ek
alag failure mode, par utna hi bura).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — recovery ko hot path pe (blocking) karna
Retransmission request/response ya snapshot fetch **blocking hona chahiye
ek alag thread/path pe**, market-data hot-path thread ko block nahi karna
chahiye (37/12 ka "market-data path aur order path do concurrent flows"
yahan bhi lagta — recovery ek teesra, background flow hai).

### Trap 2 — "book stale" state ko strategy tak propagate na karna
Agar feed handler khud recovery kar raha hai par strategy ko yeh state
pata nahi, strategy purani/incomplete book pe decisions leti rahegi.
Explicit signal (state machine) zaroori hai.

### Trap 3 — retry infinitely, koi timeout na hona
Ek down exchange/network segment ke against infinite retry system ko
permanently "recovering" state mein phansa deta. Timeouts + escalation
(operator alert, ya symbol ko poori tarah suspend karna) chahiye.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| A/B arbitration hi kaafi hai, retransmission ki zaroorat nahi | True gaps (dono feeds mein missing) abhi bhi hote — 13 zaroori |
| Recovery hot-path pe synchronously ho sakti hai | Alag path pe, non-blocking, taaki fresh data process hoti rahe |
| Gap ke dauraan trading continue kar sakte | Fail-closed — stale book pe trade karna risky (37/13) |
| Retry hamesha eventually kaam karega | Timeout + escalation chahiye, "stuck forever" avoid karne ke liye |

---

## Exercises

1. Ek gap detect hota hai. A/B arbitration se fill nahi hota. Retransmission
   request bhejte ho, par exchange 5 second tak respond nahi karta. Kya
   karoge?
   <details><summary>Answer</summary>
   Timeout hit hone pe, retransmission "fail" maano aur agle layer
   (snapshot resync) try karo. Is poore samay symbol "STALE"/"RECOVERING"
   state mein rehni chahiye (trading suspended us symbol pe), aur agar
   yeh baar-baar ho raha ho, operator ko alert karna chahiye (kuch
   systemically galat hai — network issue, exchange-side problem).
   </details>

2. Kyun retransmission request/response typically TCP pe hoti, jabki
   market data khud UDP pe hoti?
   <details><summary>Answer</summary>
   Retransmission ek RARE, RELIABILITY-CRITICAL operation hai — agar
   yeh response bhi kho jaaye, poora recovery hi fail ho jaata (infinite
   regress ka risk). TCP ki delivery guarantee yahan sahi trade-off hai;
   iski extra latency/overhead acceptable hai kyunki yeh already ek
   "kuch galat ho gaya, fix kar rahe hain" path hai, hot-path nahi (30-
   NETWORKING).
   </details>

---

## Interview questions

1. Recovery ke 3 layers batao (increasing cost order mein), aur kyun is
   order mein try karte.
2. "Go stale" design pattern kya hai, aur yeh 37/13 (fail-closed) se
   kaise connect karta?
3. Retransmission TCP pe kyun hoti, market data UDP pe kyun?
4. Recovery mein timeout/escalation kyun zaroori hai?

---

## Next
→ [`14-timestamping-and-clocks.md`](14-timestamping-and-clocks.md)
