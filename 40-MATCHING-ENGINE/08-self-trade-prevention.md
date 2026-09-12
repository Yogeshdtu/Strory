# 08 — STP rules: cancel-newest/oldest/both

## Prerequisites
- `07-trade-events.md`

## Yeh topic abhi kyun

Ek hi participant (firm/trader) ke DO orders agar apas mein match ho jaaye
— **self-trade** — real markets mein aksar **allowed nahi** hota (kai
jagah regulatory bhi issue hai — 37/16, aur ek firm ke apne accounting
ko confuse karta, "maine khud se khareeda-becha," koi real economic
activity nahi hui). **Self-Trade Prevention (STP)** ek OPT-IN mechanism
hai jo isse rokta.

---

## Kab fire hota hai

```cpp
if (incoming.stp != StpMode::None && incoming.participant == resting.participant) {
    // STP logic yahan
}
```

Do conditions: (1) incoming order ka `stp` field `None` NAHI hai (yeh
per-order opt-in hai, sab orders STP maangte nahi), (2) `participant`
match karta (SAME firm/trader dono taraf).

STP off hone pe (`StpMode::None`, default) — self-trade **normally allow**
hota, jaisa koi aur trade. `04_self_trade_prevention.cpp`'s scenario #1
isi baseline ko dikhata.

---

## Teen modes

### CancelNewest -- incoming (hamesha "newest") cancel, resting UNTOUCHED

```
Book ASK 100 {id1=OTHER x20, id2=SELF x30}
Incoming BUY 100 x100, participant=SELF, stp=CancelNewest:
  id1(OTHER) match -> fill 20
  id2(SELF) TOUCH -> STP fires -> incoming CANCEL (poora remaining VOID)
  id2 khud UNTOUCHED reh jaata (resting order jaisa tha)
Result: filled=20 (sirf id1 se), incoming ka baaki (80) VOID
```

Incoming hamesha "newest" hai kyunki woh abhi-abhi arrive hua — CancelNewest
ka matlab **incoming ka poora matching attempt turant ABORT** ho jaata
jaise hi koi self-order touch ho.

### CancelOldest -- resting (older) hatao, incoming AAGE try karta rehta

```
Book ASK 100 {id1=SELF x20, id2=OTHER x30}
Incoming BUY 100 x100, participant=SELF, stp=CancelOldest:
  id1(SELF) TOUCH -> STP fires -> id1 REMOVE (match NAHI hota, order gone)
  id2(OTHER) match -> fill 30 (incoming CONTINUE karta agle order pe)
Result: filled=30, id1 poori tarah book se GONE (self-trade avoid, par
        incoming apna matching jaari rakh saka)
```

### CancelBoth -- dono cancel, incoming ka matching bhi ABORT

```
Book ASK 100 {id1=OTHER x20, id2=SELF x30}
Incoming BUY 100 x100, participant=SELF, stp=CancelBoth:
  id1(OTHER) match -> fill 20
  id2(SELF) TOUCH -> STP fires -> id2 REMOVE, incoming ka baaki (80) bhi VOID
Result: filled=20, id2 gone, incoming baaki VOID (jaise CancelNewest, PAR
        resting order bhi is baar REMOVE hua)
```

| Mode | Resting order ka kya hota | Incoming ka matching |
|---|---|---|
| `CancelNewest` | Untouched, book mein rehta | Turant ABORT (poora remainder void) |
| `CancelOldest` | REMOVE (gone) | CONTINUE (agle order/level pe try karta) |
| `CancelBoth` | REMOVE (gone) | ABORT (baaki VOID) |

---

## Implementation (`match_against` ke andar)

```cpp
if (incoming.stp != StpMode::None && incoming.participant == resting.participant) {
    if (incoming.stp == StpMode::CancelOldest) {
        // resting hatao, list-erase() agla iterator deta -- CONTINUE
        ord_it = level.orders.erase(ord_it);
        level.total_qty -= dead_qty;
        index_.erase(dead_id);
        continue;
    }
    if (incoming.stp == StpMode::CancelBoth) {
        level.orders.erase(ord_it);
        ... index_.erase(dead_id);
        if (level.orders.empty()) opp_side.erase(lvl_it);
        stp_aborted = true;
        return;                  // poore function se BAHAR -- match rukta
    }
    stp_aborted = true;          // CancelNewest -- resting KO CHHUA TAK NAHI
    return;
}
```

`stp_aborted` ek out-param hai jo `submit()` ko batata "yeh incoming
Limit order ka remainder REST nahi karega, chahe qty>0 ho" (03 mein Limit
ka normal path yahi hai; STP-abort ek EXCEPTION hai us path ka).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `resting.qty` erase() ke BAAD read karna (dangling reference)
```cpp
// ❌ GALAT
level.orders.erase(ord_it);
level.total_qty -= resting.qty;   // resting AB DANGLING hai! (list node freed)

// ✅ SAHI -- erase se PEHLE capture karo
const Qty dead_qty = resting.qty;
level.orders.erase(ord_it);
level.total_qty -= dead_qty;
```
Yeh EXACT bug is folder banate waqt draft mein pehli baar likha gaya tha
(design section mein socha), pakda gaya aur fix kiya gaya — `resting` ek
`Order&` hai jo `*ord_it` ko reference karta; `erase()` ke baad woh
memory freed ho chuki, use-after-free UB hota.

### Trap 2 — CancelOldest mein `continue` na karna
Agar `continue` ki jagah `++ord_it` likh do CancelOldest ke baad, tum
DO baar aage badh jaate (`erase()` khud agla iterator deta hai, phir
manual `++` ek order SKIP kar deta — ek non-STP order match hone se
reh sakta).

### Trap 3 — FOK ke precheck ko STP-unaware rakhna
06 mein poori tarah cover kiya — yahan sirf reminder: STP ki teeno modes
ki exact semantics precheck mein bhi replicate honi chahiye.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| STP hamesha ON hota | Per-order opt-in (`stp` field), default `None` |
| "Newest"/"Oldest" ka matlab price se hai | Arrival TIME se -- incoming hamesha "newest" (abhi aaya) |
| Teeno modes same result dete | Different -- resting-fate aur incoming-continuation dono alag |

---

## Hands-on

```bash
./build.ps1 fast 40-MATCHING-ENGINE/examples/04_self_trade_prevention.cpp
```

---

## Exercises

1. `CancelOldest` mode mein, agar resting side pe LAGATAAR 3 self-orders
   ho (SAME participant), sab teeno alag price/qty ke, incoming inhe
   touch karta jaaye -- kya hota?
   <details><summary>Answer</summary>
   Har ek teeno self-order TOUCH hote hi REMOVE hote (`continue` loop),
   koi match NAHI hota inse. Incoming apna matching jaari rakhta jab tak
   koi NON-self order na mile ya book/qty khatam ho jaaye.
   </details>

2. `CancelNewest` aur `CancelBoth` dono "incoming ka matching ABORT karte"
   -- fark sirf kya hai?
   <details><summary>Answer</summary>
   `CancelNewest`: resting order UNTOUCHED rehta (book mein wahi jagah).
   `CancelBoth`: resting order bhi REMOVE ho jaata (dono cancel). Dono
   incoming ka remaining matching rokte, par resting-side ka fate alag.
   </details>

---

## Interview questions

1. Self-trade prevention kyun zaroori hai (regulatory + business dono
   angle se)?
2. Teeno STP modes ka exact behavior batao, ek example ke saath.
3. Erase-then-read dangling-reference bug yahan kaise/kyun hota hai?

---

## Next
→ [`09-determinism.md`](09-determinism.md)
