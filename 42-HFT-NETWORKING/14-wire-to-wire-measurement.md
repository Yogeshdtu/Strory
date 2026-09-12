# 14 — True latency measurement: taps, timestamps, correlation

## Prerequisites
- `13-fpga-offload-intro.md`
- `08-hardware-timestamping.md`

## Yeh topic abhi kyun

08 ne EK hop (TX→RX, same clock domain) measure karna sikhaya. Yeh
lesson `06_wire_to_wire.linux.cpp` ke through **poori chain** (multiple
hops + internal processing) ko ek breakdown mein todta — 37/14's
latency-budget concept ka poora IMPLEMENTATION.

---

## Design: TIMESTAMP HAR HOP PE, EK CLOCK SE

```
[MD sender] --TX-ts--> [wire] --RX-ts--> [Gateway] --TX-ts--> [wire] --RX-ts--> [Exchange sim]
             |<--- hop 1 --->|          |<-processing->|      |<--- hop 2 --->|
```

`06`'s poora design isi principle pe hai: **HAR** send/receive point pe
kernel timestamp lo (08's TX/RX technique), phir **subtraction se
breakdown nikalo**:

```cpp
hop1 = md_rx_ns - md_tx_ns;        // network transit (MD leg)
proc = ord_tx_ns - md_rx_ns;       // OUR OWN processing (gateway internal)
hop2 = ord_rx_ns - ord_tx_ns;      // network transit (order leg)
total = ord_rx_ns - md_tx_ns;      // end-to-end (wire-to-wire)
```

**Kyun yeh important hai**: agar sirf `total` measure karte (ek single
number), pata NAHI chalta "kahan time gaya" — kya NETWORK slow tha, ya
HAMARI apni processing? Breakdown se yeh EXPLICITLY separable hai.

---

## Measured (this design, expected shape — 06's file mein detail)

```
Hop 1 (MD send -> gateway RX)  : ~3000 ns  (loopback, software TS)
Processing (gateway internal)  : ~200 ns   (trivial demo logic)
Hop 2 (order send -> exch RX)  : ~3000 ns
TOTAL                          : ~6500 ns
```

**Yeh demo ke liye "processing" trivial hai** — REAL strategy mein yeh
ratio **INVERT** ho jaata: real NIC + kernel-bypass se network hops
~100-300ns tak SHRINK ho jaate, PAR real strategy logic (matching, risk
checks, decision-making) us SAME jagah pe HUNDREDS ya THOUSANDS of
nanoseconds le sakti — **processing SABSE BADA, aur sabse CONTROLLABLE,
hissa ban jaata.**

> **HFT relevance:** yeh EXACTLY 37/14's latency-budget table ka
> practical, MEASURED version hai — "network," "parsing," "matching,"
> "risk," "order-send" har ek APNA number, na ki ek opaque total.

---

## "Taps" — production mein kaise karte

Production HFT systems mein **network taps** (physical devices jo cable
ko "sunte" bina traffic ko affect kiye) use hote — exchange se aane
wale HAR packet ka EXACT wire-arrival-time capture karte, **hardware
timestamp** ke saath (08's PTP-synced clocks). Isse tumhara "true wire
time" milta bina APP ke andar kisi bhi measurement-overhead ke —
measurement khud latency add NAHI karta (jo `06`'s in-process design
thoda karta, chhota sa, since measurement code khud kuch cycles leta).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf END-TO-END number report karna
"Tick-to-trade 10 microseconds hai" — USEFUL nahi jab tak breakdown na
ho. Har HOP ka number separately track karo.

### Trap 2 — measurement-overhead ko IGNORE karna
`06`'s design khud kuch cost add karta (timestamps lene ke liye extra
syscalls/cmsg parsing) — production taps yeh overhead avoid karte
(external hardware, application code untouched). In-process measurement
"real" number se thoda upar dikha sakta.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Ek single "total latency" number kaafi hai | Breakdown (per-hop) zaroori — batata KAHAN optimize karna hai |
| Demo mein processing sabse bada hissa hai (matlab wahi optimize karo) | Yeh demo-artifact hai (trivial logic) — real strategy mein ratio INVERT hota |
| In-process measurement = production-accurate | External taps overhead-free hote, in-process measurement khud thoda cost add karta |

---

## Hands-on

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread 06_wire_to_wire.linux.cpp -o w2w
```

---

## Exercises

1. Ek real system mein hop1=200ns, processing=1500ns, hop2=200ns
   (kernel-bypass + FPGA-adjacent hardware). Optimization effort kahan
   focus karni chahiye?
   <details><summary>Answer</summary>
   Processing (1500ns, sabse bada AUR sabse controllable hissa) --
   network hops already bahut optimized (bypass/hardware), unmein aur
   marginal improvement mushkil aur expensive hoga. Processing
   (matching/risk/decision logic) mein optimization ka SABSE ZYAADA
   "bang for the buck" milega.
   </details>

---

## Interview questions

1. "Timestamp har hop pe, ek clock se" pattern explain karo.
2. Kyun sirf end-to-end number kaafi nahi hota?
3. Production "network taps" in-process measurement se kaise behtar hain?

---

## Next
→ [`15-packet-capture-analysis.md`](15-packet-capture-analysis.md)
