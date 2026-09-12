# 08 — NIC timestamps, PTP, measuring true wire time

## Prerequisites
- `07-busy-poll-sockets.md`
- `30-NETWORKING/09-timestamping.md` (RX-only `SO_TIMESTAMPING`, ALAG
  clock domain — is lesson ka starting point)

## Yeh topic abhi kyun

30/09 ne dikhaya tha `SO_TIMESTAMPING` se kernel RX timestamp milta,
PAR usse APP's `steady_clock` se compare karne pe sirf **relative
jitter** milta tha — ALAG clock domains (kernel `CLOCK_REALTIME`-based
vs app monotonic). Yeh lesson `04_hw_timestamps.linux.cpp` ke through
usi limitation ko **fix** karta.

---

## Fix: DONO taraf KERNEL timestamp lo

```
30/09:  RX kernel-ts  --vs--  APP steady_clock   -> ALAG domains, MEANINGLESS subtraction
04:     TX kernel-ts  --vs--  RX kernel-ts        -> SAME domain, MEANINGFUL subtraction
```

TX timestamp lena RX se ALAG mechanism maangta — **`MSG_ERRQUEUE`**:

```cpp
// Sender side, AFTER sendto():
recvmsg(fd, &msg, MSG_ERRQUEUE);   // TX completion async event, ERROR QUEUE se milta
```

Yeh async hai (TX completion turant available nahi hoti — driver ko
packet SEND karne ka time chahiye) — isliye **brief retry loop** chahiye
(`04`'s `get_tx_timestamp()`, 200 tries, 20µs gap).

---

## `scm_timestamping` struct — teen timestamps

```c
struct scm_timestamping { struct timespec ts[3]; };
// ts[0] = software timestamp (kernel, DMA-time ke baad, ISR mein)
// ts[1] = deprecated (legacy hardware transform, use mat karo)
// ts[2] = raw hardware timestamp (NIC PHY se, agar driver support kare)
```

**`ts[2]` (raw hardware) sabse ACCURATE hai** — NIC PHY chip khud
timestamp deta, PACKET actually wire pe jaane/aane ke waqt (driver
processing, interrupt latency, kuch bhi is number ko affect NAHI karta).
`ts[0]` (software) kernel ne jab packet PROCESS kiya tab ka time —
already kuch microseconds ka driver/kernel-queueing jitter isme mixed
hai.

`04`'s code `hw ? hw : sw` fallback karta — **loopback pe `ts[2]` HAMESHA
0 hota** (koi PHY hai hi nahi virtual interface mein), isliye demo mein
`ts[0]` (software) pe fall back hota, explicitly documented.

---

## PTP — cross-machine clock sync

`ts[2]` (hardware) sirf ek machine ke ANDAR accurate hai. Do ALAG
machines (jaisa exchange ka gateway aur tumhara receiver) ke clocks
ko compare karne ke liye, **dono ki clocks SYNC honi chahiye** — **PTP**
(Precision Time Protocol, IEEE 1588) isi ko karta:

```
ptp4l -i eth0 -m          # PTP daemon -- hardware clock (PHC) ko network
                          # ke doosre PTP-aware devices se sync karta
phc2sys -s eth0 -c CLOCK_REALTIME -O 0 -m   # PHC -> system clock sync
```

PTP-synced clocks **sub-microsecond** accuracy tak ja sakte (switches
BHI PTP-aware honi chahiye — "boundary clocks"/"transparent clocks,"
30/14's coverage). Isi ke saath, do ALAG machines ke hardware timestamps
MEANINGFULLY compare ho sakte — **exchange ka TX-timestamp vs tumhara
RX-timestamp = TRUE wire latency**, network jitter samet.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — software aur hardware timestamp ko same maan lena
`ts[0]` mein already kernel processing delay mixed hai — agar tum
"true wire time" measure karna chahte ho, `ts[2]` (hardware) hi sahi
hai. Loopback/virtual interfaces pe yeh NAHI milta (04 explicitly
handle karta).

### Trap 2 — cross-machine timestamps bina PTP ke compare karna
Bina sync ke, do machines ki clocks mein SECONDS tak ka drift ho sakta
— "latency" number GALAT (kabhi-kabhi NEGATIVE bhi, jo ek diagnostic
signal hai ki clocks sync nahi hain, 30's audit-mentioned "negative-
latency diagnostic").

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| TX timestamp seedha `sendto()` ke return se milta | Async hai, `MSG_ERRQUEUE` se poll/retry karna padta |
| Software aur hardware timestamp same accuracy dete | Hardware PHC-level accurate, software mein kernel-jitter mixed |
| Loopback pe hardware timestamp mil jaata | Nahi -- koi PHY hai hi nahi, sirf software timestamp milta |

---

## Hands-on

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread 04_hw_timestamps.linux.cpp -o hwts
```

---

## Exercises

1. Ek receiver `RX_ns < TX_ns` dikhata (negative latency!). Kya galat ho
   sakta?
   <details><summary>Answer</summary>
   Agar DONO taraf clocks alag machines pe hain aur PTP-sync nahi hai
   (ya sync drift ho gaya), ek negative number aa sakta -- yeh IMPOSSIBLE
   hai physically (effect apne cause se pehle nahi ho sakta), isliye
   yeh ek DIAGNOSTIC signal hai ki clock-sync check karo, na ki "network
   negative-latency hai."
   </details>

---

## Interview questions

1. Software aur hardware RX timestamp ka fark.
2. `MSG_ERRQUEUE` TX timestamp ke liye kyun zaroori hai (seedha
   `sendto()` se kyun nahi milta)?
3. PTP kya solve karta, aur "negative latency" kis problem ka symptom hai?

---

## Next
→ [`09-nic-tuning.md`](09-nic-tuning.md)
