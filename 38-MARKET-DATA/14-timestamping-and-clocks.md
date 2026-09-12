# 14 — Timestamping aur clocks: exchange time, receive time, PTP, skew

## Prerequisites
- [`13-recovery-and-retransmission.md`](13-recovery-and-retransmission.md)
- `29-LINUX-SYSTEMS` (`CLOCK_MONOTONIC` vs `rdtsc`, light revision)
- `examples/07_market_data_simulator.cpp`

## Yeh topic abhi kyun
Har message hamare protocol mein `exch_ts_ns` carry karta (`wire_protocol.
hpp`). Yeh lesson batata hai woh field kis liye hai, aur ek timestamp ko
"trust" karna itna simple kyun nahi hai jitna lagta.

---

## Do alag timestamps — DO alag clocks

```
[exchange matching engine]  event generate hoti  -->  exch_ts_ns (EXCHANGE ki clock)
        |
        | (network delay)
        v
[tumhara feed handler]  bytes receive hote  -->  recv_ts_ns (TUMHARI clock)
```

| Timestamp | Kiski clock | Kya batata |
|---|---|---|
| **`exch_ts_ns`** (wire mein) | Exchange ki | "Yeh event WAHAN kab hui" |
| **`recv_ts_ns`** (tum khud measure karte) | Tumhari | "Yeh message YAHAN kab pahunchi" |

**Delta = `recv_ts_ns - exch_ts_ns` = wire-to-wire latency** (network +
serialization + propagation) — yeh 37/14 ke latency budget ka ek REAL,
measurable component hai, agar dono clocks **sync** hain (aage dekhenge
yeh kitna bada "agar" hai).

---

## Measured (`07_market_data_simulator.cpp`)

```
inter-message gap (exch_ts_ns delta): min=500.0 median=2740.0 mean=2747.6 max=4999.0 ns
=> is synthetic feed ki throughput ~ 363957 msgs/sec (exchange-clock-relative)
```

Yeh `exch_ts_ns` field se hi nikla — consecutive messages ke exchange-
timestamp ka **delta** batata "feed kitni frequently update ho rahi
(exchange ki apni clock se)." Yeh ek REAL diagnostic hai — agar tumhari
feed handler bahut slow hai, tum `recv_ts_ns` ke basis pe socho ki updates
kam frequent hain, jabki `exch_ts_ns` se pata chalta exchange to bahut
tez updates bhej rahi thi (tumhari processing/queueing hi bottleneck thi).

---

## Clock skew — jab dono clocks "sync" nahi hote

> **Clock skew = tumhari clock aur exchange ki clock same "abhi ka time"
> pe agree nahi karti** — chahe dono "correct rate" pe tick kar rahi hon.

```
Exchange clock:  10:00:00.000000
Tumhari clock:   10:00:00.000350   <- 350 microseconds AAGE (ya peeche)
```

Agar skew **known aur fixed** hota, tum bas subtract kar sakte the. Par
skew:
- **Drift karta** time ke saath (crystal oscillators perfectly accurate
  nahi hote).
- **Har machine pe alag** hota (tumhara server, exchange ka server, dono
  independent clocks).

Bina correction ke, `recv_ts_ns - exch_ts_ns` ek **galat** number de
sakta — kabhi negative bhi (agar tumhari clock exchange se peeche hai
aur delay chhota hai)!

---

## PTP (Precision Time Protocol) — clock sync ka solution

> **PTP = network par bahut precise (sub-microsecond) clock
> synchronization protocol** — sab participating machines apni clock ko
> ek common reference (grandmaster clock) ke against continuously adjust
> karte.

```
Grandmaster clock (GPS-disciplined, atomic-clock-grade accuracy)
        |
   [PTP messages -- sync, delay-request/response]
        |
        v
Tumhara server ki clock -- PTP daemon (jaise ptp4l) isse
                            continuously discipline karta
```

Colocated HFT infra (37/11) mein PTP **standard practice** hai — bina
isske, cross-machine timestamp comparison (jaise "exchange se mera server
tak kitni der lagi") meaningless ho jaati (skew ka size hi latency se
bada ho sakta!).

**Boundary/transparent clocks** — network switches bhi PTP-aware ho sakte
(timestamp-forwarding accuracy ke liye), taaki switch ke andar ka queueing
delay bhi PTP accuracy ko degrade na kare.

---

## Hardware timestamping — driver/OS ko bhi bypass karna

Sabse precise measurement ke liye, **NIC khud** packet ke arrival pe
hardware timestamp lagata (kernel/driver processing se pehle) — isse
kernel scheduling jitter (37/03 ka jitter-source) timestamp accuracy ko
affect nahi karta. `SO_TIMESTAMPING` (Linux) is capability ko expose
karta — deep detail **42-HFT-NETWORKING** mein.

---

## Timestamps ka practical use — is folder ke context mein

| Use | Kaise |
|---|---|
| **Throughput measure karna** | `exch_ts_ns` deltas (jaisa 07 mein) — exchange-side rate |
| **Wire-to-wire latency measure karna** | `recv_ts_ns - exch_ts_ns` — **PTP-synced clocks chahiye** |
| **Event ordering** (agar `seq_num` na ho) | Timestamp se bhi order establish ho sakta, par seq_num zyada reliable hai (04 — timestamps clock-precision-limited hote, do events same ns pe ho sakte) |
| **Staleness detect karna** | "last update `exch_ts_ns` se X ms ho gaye, book stale ho sakti" |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — bina PTP ke cross-machine latency claim karna
`recv_ts_ns - exch_ts_ns` **sirf tab meaningful hai** jab dono clocks
sync hain. Bina PTP (ya kam se kam pata clock-skew-bound) ke, yeh number
skew ka artifact ho sakta, real latency nahi.

### Trap 2 — `seq_num` ki jagah `timestamp` se ordering establish karna
Timestamps clock resolution-limited hote (do events **exactly same** ns
pe timestamp ho sakte, ya out-of-order arrival apni-apni timestamp ke
saath aa sakti). `seq_num` (04) ek **explicit, unambiguous** ordering
guarantee deta jo timestamp nahi de sakta.

### Trap 3 — application-level timestamp (kernel ke baad) ko "arrival
time" maan lena
Agar tum apna `recv_ts_ns` socket-`recv()` ke BAAD lete ho, usmein kernel
scheduling delay bhi shaamil hai — asli "wire pe kab aaya" nahi. Hardware
timestamping (upar) is gap ko eliminate karta.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Dono machines ki clock automatically sync hoti hain | Nahi — PTP jaisa explicit mechanism chahiye |
| Timestamp delta hamesha "real latency" hai | Sirf agar clocks sync hain (PTP-disciplined) |
| Timestamp se hi ordering kaafi hai | seq_num zyada reliable hai (resolution-independent) |
| Application-level timestamp = "true" arrival time | Kernel/scheduling delay isme shaamil ho sakta; HW timestamping zyada precise |

---

## Exercises

1. Tumhara measured wire-to-wire latency kabhi-kabhi **negative** aata hai
   (`recv_ts_ns < exch_ts_ns`). Kya ho raha hai?
   <details><summary>Answer</summary>
   Physically impossible (message tumhare paas apne bhejne se PEHLE nahi
   aa sakti) — yeh **clock skew** ka signature hai: tumhari clock
   exchange ki clock se AAGE hai, aur is skew ka magnitude actual network
   delay se bada hai. Fix: PTP se dono clocks ko ek common reference ke
   against sync karo, taaki delta meaningful ho.
   </details>

2. Kyun `seq_num` (04) timestamp se zyada reliable hai ordering ke liye?
   <details><summary>Answer</summary>
   Timestamp resolution-limited hai (agar clock granularity 1µs hai aur
   do events 200ns apart hue, dono same timestamp dikha sakte — ordering
   ambiguous). `seq_num` explicit, discrete, aur exchange-guaranteed
   monotonic hai — koi resolution-limit issue nahi, aur reliably gap/
   duplicate bhi detect karta (04) jo raw timestamp comparison nahi kar
   sakta.
   </details>

---

## Interview questions

1. Exchange timestamp aur receive timestamp mein fark batao, aur unka
   delta kya represent karta.
2. Clock skew kya hai, aur PTP isse kaise solve karta?
3. Kyun ordering ke liye `seq_num` timestamp se better hai?
4. Hardware timestamping application-level timestamp se zyada accurate
   kyun hoti?

---

## Next
→ [`15-conflation.md`](15-conflation.md)
