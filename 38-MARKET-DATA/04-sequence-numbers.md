# 04 — Sequence numbers: gap detection, duplicates, out-of-order (measured)

## Prerequisites
- [`03-snapshots-vs-incremental.md`](03-snapshots-vs-incremental.md)
- `examples/08_gap_detection.cpp`

## Yeh topic abhi kyun
03 ne dikhaya: ek miss hui update = silent, permanently galat book. Yeh
lesson uska **detection mechanism** hai — sequence numbers, aur unse gap/
duplicate/out-of-order pakadna.

---

## Sequence number kya hai

37/02 mein dekha: exchange gateway har order ko ek sequence number deta,
arrival order lock karne ke liye. **Market data feed** bhi wahi principle
use karti — har published message ka ek **monotonically increasing**
number hota, taaki consumer verify kar sake "maine sab kuch, sahi order
mein, receive kiya."

```cpp
// wire_protocol.hpp
struct MsgHeader {
    ...
    std::uint32_t seq_num;   // feed-wide monotonic counter
    ...
};
```

---

## Teen cheezein jo sequence number pakadta hai

```
expected = 5

seq=5 aaya  -> NORMAL, expected ab 6
seq=7 aaya  -> GAP! (5 mila tha, 6 nahi mila, ab 7 aaya) -- [6,6] missing
seq=4 aaya  -> DUPLICATE ya OUT-OF-ORDER (already process kar chuke the)
```

| Case | Matlab | Action |
|---|---|---|
| `seq == expected` | Normal | Process karo, `expected++` |
| `seq > expected` | **Gap** — `[expected, seq-1]` missing | Log/count gap, recovery trigger karo (13), `expected = seq+1` |
| `seq < expected` | **Duplicate/out-of-order** | Discard (already process ho chuka), `expected` mat badlo |

---

## Measured (`08_gap_detection.cpp`)

Ek feed ko simulate kiya **~0.5% packet loss** ke saath (jaisa UDP mein
realistically hota — 30-NETWORKING):

```
sent: 20000 messages, 717796 bytes
received (post-loss): 714318 bytes (0.484539% bytes lost)

gaps detected: 96
total missing sequence numbers: 96
duplicates/out-of-order seen: 0

first gaps:
  seq 417  (1 message missing)
  seq 462  (1 message missing)
  seq 1130  (1 message missing)
  ...

sanity: sent=20000 received=19904 (sent - received)=96
        total_missing_from_gaps=96  match? haan
```

**96 messages "lose" hue, aur gap-detection logic ne EXACTLY 96 detect
kiye** — sanity check confirm karta ki har missing message ek gap ke roop
mein pakda gaya, koi silently discard nahi hua. Yeh exactly 03 ka
"silent drift" problem solve karta — ab tumhe **pata** hai kya missing hai.

---

## Gap detect hone ke baad kya karo (preview — 12, 13)

Gap detect karna sirf pehla step hai. Uske baad:
1. **Book us range ke liye "uncertain" mark karo** — jab tak fill na ho,
   affected price levels pe decisions mat lo (risk, 37/13 se connect).
2. **Recovery try karo** — dusra feed (A/B, 12) se fill, ya
   retransmission request (13), ya snapshot resync (03).
3. **Fill na ho paaye to** — book "stale" mark karo us symbol ke liye,
   trading rok do us symbol pe jab tak resync na ho (fail-closed, 37/13
   ka principle yahan bhi lagta).

---

## Duplicates aur out-of-order kab aate hain

Pure packet-LOSS scenario mein (jaisa example 08) duplicates nahi aate
(kuch bytes gaye, baaki sahi order mein aaye). Par real networks mein:
- **Retransmission** (13) ke baad, agar original bhi late pahunch jaaye
  → duplicate.
- **Multiple network paths** (12 ka A/B) → same message do baar mil sakta.
- **UDP reordering** (rare, par possible on some networks/multi-path
  setups) → out-of-order arrival.

Isliye production code **dono** cases handle karta (`seq < expected` →
duplicate/out-of-order, discard), sirf gap (`seq > expected`) nahi.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf gap check karna, duplicate check nahi
Agar `seq < expected` ko bhi silently "normal" process kar do, ek
duplicate message do baar apply ho sakta (jaise ek Cancel do baar apply
= qty double-subtract = galat book).

### Trap 2 — `expected_seq` ko galat jagah se initialize karna
Pehla message jo mile, uska seq number `expected` ka starting point hai
(ya snapshot ka seq number, 03) — hardcoded `0` ya `1` se start karna,
agar feed kahin beech se shuru ho rahi, galat initial state dega.

### Trap 3 — gap detect karke bhi kuch na karna
Gap sirf **log** karna kaafi nahi — us gap ke range ke orders/price-levels
ab "unknown state" mein hain. Bina recovery/staleness-marking ke, tum
galat book pe trade kar sakte ho (13, 37/13 se connect).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Sequence gap = crash hoga | Silent — explicit check zaroori |
| Sirf gap check kaafi hai | Duplicate/out-of-order (`seq < expected`) bhi handle karo |
| Gap detect karna hi kaafi hai | Recovery/staleness-marking bhi chahiye (12, 13) |
| Duplicates rare hain, ignore kar sakte | Retransmission/A-B/reordering se aate hain, real hain |

---

## Exercises

1. `expected_seq = 100`. Messages is order mein aate: 100, 101, 103, 102,
   104. Kya-kya detect hoga?
   <details><summary>Answer</summary>
   100 -> normal, expected=101. 101 -> normal, expected=102. 103 -> GAP
   (102 missing), expected=104. 102 -> out-of-order/duplicate (< expected
   104), discard. 104 -> normal, expected=105. Net: ek gap detect hota
   (102 "missing" report hota) chahe woh baad mein (out-of-order) aa
   jaaye — real system ko is reordering case ko bhi gracefully handle
   karna chahiye (ideally ek chhoti reorder-buffer/window se, agar
   network reordering common ho).
   </details>

2. `08_gap_detection.cpp` ka sanity check kya PROVE karta hai jo sirf
   "gaps detected: 96" print karne se nahi hota?
   <details><summary>Answer</summary>
   Yeh confirm karta ki gap-detection logic **exactly** utne hi messages
   ko "missing" mark kar rahi jitne actually drop hue the (na kam, na
   zyada) — na koi false-positive gap, na koi missed (undetected) drop.
   Bina is cross-check ke, "96 gaps" ek unverified number hai; sanity
   check se woh ek PROVEN-correct number ban jaata.
   </details>

---

## Interview questions

1. Sequence number se gap, duplicate, out-of-order — teeno kaise
   distinguish karte?
2. Gap detect hone ke baad system ko kya karna chahiye (immediate se
   lekar recovery tak)?
3. Duplicates/out-of-order real networks mein kahan se aate?
4. `expected_seq` ko initialize karne mein kya trap hai?

---

## Next
→ [`05-binary-protocols.md`](05-binary-protocols.md)
