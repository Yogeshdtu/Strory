# 16 — BUILD: low-latency multicast receiver

## Prerequisites
- Poora folder `42` (`01`–`15`) is point tak

## Yeh topic abhi kyun

01-15 ne piece-by-piece cover kiya — abstraction (03/08), receive path
(01/02), busy-poll (07), timestamping (08), TCP gateway (11), wire-to-
wire (14). Yeh lesson unhe **EK production-grade design** mein jodta —
"agar main REAL low-latency multicast receiver banata, kaisa dikhta."

---

## Design — layer by layer

```
Layer 1: BACKEND (03, 08)
  INetworkReceiver interface -- kernel-socket abhi, bypass library
  (Onload/ef_vi/DPDK) config-time swap ke liye ready.

Layer 2: RECEIVE PATH (01, 02, 07)
  Non-blocking socket + SO_RCVBUF tuned + (agar real NIC) SO_BUSY_POLL.
  Sequence-gap detection HAR message pe (38/09's mechanism).

Layer 3: TIMESTAMPING (04, 08, 14)
  RX kernel timestamp (hardware agar available) HAR message pe stamp
  hoti -- downstream stages (41's pipeline) isi timestamp ko carry
  karte end-to-end latency ke liye.

Layer 4: THREADING (41's poora folder)
  Receive-thread ISOLATED core pe pinned (41/08), SPSC queue (41/04)
  se downstream stage (matching engine, 40) ko events forward karta.

Layer 5: SYSTEM TUNING (09, 10)
  NIC ring buffers tuned, coalescing off, GRO/LRO off, IRQs housekeeping
  cores pe pinned (hot-path core se ALAG).

Layer 6: OBSERVABILITY (09's drop-counters, 15's packet-capture)
  Har drop/gap COUNTER track hota (verify-by-effect, 29's principle).
  SPAN-port capture setup hai bypass-safe debugging ke liye.
```

---

## Poora flow, ek diagram mein

```
Exchange multicast feed
        |
        v
[NIC -- tuned: rings, coalescing off, GRO/LRO off]   (09)
        |
[IRQ -- pinned to housekeeping core, NOT hot-path core]  (10)
        |
        v
[INetworkReceiver -- kernel-socket ya bypass backend]     (03, 08)
        |  (try_receive, non-blocking, isolated pinned core)  (01, 41/08)
        v
[RX timestamp stamped]                                     (04, 08)
        |
[Sequence-gap check]                                        (02)
        |
        v
[SPSC queue -- shared-nothing hand-off]                     (41/04)
        |
        v
[Matching engine / strategy thread, SEPARATE core]           (40, 41)
```

Yeh EXACTLY `08_pipeline_demo.cpp` (41) ka Stage-1-se-pehle-ka hissa hai
— market data yahan se ANDAR aati, phir 41's pipeline lekar jaata.

---

## Design decisions — explicitly justified

| Decision | Kyun | Kahan cover hua |
|---|---|---|
| Abstraction layer (backend-agnostic) | Backend swap = config, na ki rewrite | 03, 08 |
| Non-blocking + spin (isolated core pe) | Lowest latency jab dedicated core ho | 07, 41/07 |
| Sequence-gap detection | UDP/multicast unreliable — transport-loss ALAG hai market-data-loss se | 02, 38/09 |
| RX kernel timestamp (na ki app steady_clock) | Downstream latency-measurement ke liye SAME clock domain chahiye | 04, 08, 14 |
| SPSC hand-off (na ki lock/mutex) | Single-writer principle, shared-nothing | 41/02-04 |
| NIC/IRQ tuning | Kernel-stack cost minimize, hot-path core ko IRQ-free rakhna | 09, 10 |

---

## ⚠️ Yeh design kab OVER-ENGINEERED hai

Agar tumhara latency requirement **microseconds** mein hai (nanoseconds
NAHI), poori is design ki JAROORAT nahi — plain kernel sockets + basic
tuning (09) shayad kaafi ho. Bypass backend (03-06), FPGA-adjacent
thinking (13) — yeh sab **incremental cost/complexity** ke against
incremental latency-gain hain. **Measure PEHLE apna current latency,
phir decide karo kitni depth chahiye** (35/05's principle, is poore
folder mein bhi laagu).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Har HFT system ko poori is design ki zaroorat hai | Requirement-dependent -- microsecond-scale ke liye simpler design kaafi |
| Layers ko ek saath, EK BAAR mein banana chahiye | Incremental -- pehle simple (kernel socket + basic tuning), measure, phir upgrade jahan zaroorat ho (35/36's process) |
| Yeh design "final" hai | Requirement badalne pe (jaisa naya symbol, naya exchange) reconsider karna padta |

---

## Exercises

1. Ek naya team, budget limited, latency requirement ~10 microseconds
   (nanoseconds nahi). Is poori design mein se KAUNSE layers zaroori
   hain, KAUNSE skip kar sakte?
   <details><summary>Answer</summary>
   Zaroori: receive path (01/02) + basic NIC tuning (09) + IRQ affinity
   (10) + threading/pinning (41). SKIP kar sakte: kernel bypass (03-06,
   overkill 10µs requirement ke liye), FPGA (13, definitely overkill).
   Yeh EXACTLY "measure current, phir incrementally add" principle hai.
   </details>

---

## Interview questions

1. Poora receiver-design, layer-by-layer, explain karo.
2. Kaunsi design choices "always do this" hain, kaunsi
   "requirement-dependent"?
3. Kab yeh poora design OVER-ENGINEERED hoga?

---

## Next
→ [`17-exercises.md`](17-exercises.md)
