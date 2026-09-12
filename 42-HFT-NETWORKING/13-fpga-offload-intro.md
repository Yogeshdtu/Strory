# 13 — FPGA ka role: overview only, yeh alag domain hai

## Prerequisites
- `12-switch-and-network-latency.md`

## Yeh topic abhi kyun

06 ne DPDK dikhaya — packet processing ko user-space CPU pe le aana.
FPGA is idea ka **agla step** hai: packet processing ko **CPU se BHI
HATA dena**, hardware (reconfigurable logic) mein karna. Yeh course
FPGA/Verilog/HLS **sikhata NAHI** (alag career/domain hai — 00-START-
HERE ki "SPECIALIZED/DOMAIN-SPECIFIC" list mein explicitly note kiya
gaya) — yeh lesson sirf **"yeh kya hai, kab use hota hai"** deta.

---

## Kya hai

**FPGA** (Field-Programmable Gate Array) — ek chip jiski LOGIC
(circuits) **reconfigure** ho sakti (software ki tarah "program" hoti,
Verilog/VHDL jaisi hardware description languages se), PAR ek baar
configure hone ke baad woh **fixed hardware circuit** ki tarah chalti —
koi instruction-fetch/decode/execute cycle nahi (jo CPU karta), sirf
DIRECT signal-processing, **clock-cycle-deterministic**.

```
CPU (even bypass ke saath):  packet -> memory -> instructions parse
                              karte hue process karta (sequential-ish)
FPGA:                        packet ke bits DIRECTLY circuit ke through
                              guzarte, parallel logic gates process
                              karte -- NO instruction overhead
```

---

## Kahan use hota HFT mein

| Use case | Kyun FPGA |
|---|---|
| **Market-data parsing + decision** (tick-to-trade) | Feed parse karna + simple threshold check (jaisa "price X se upar? BUY order bhej do") — ek FIXED, SIMPLE decision loop, FPGA pe **nanoseconds** mein |
| **Risk checks** (pre-trade) | Simple bounds-check (qty limit, price band) — hardware mein parallel, deterministic |
| **Order encoding/framing** | Exchange protocol format mein order encode karna — fixed logic |

**FPGA "smart" NAHI hai** — complex, DYNAMIC strategy logic (jo baar-baar
badalti, machine learning models, etc.) FPGA ke liye impractical hai
(reconfigure karna SLOW hai, minutes lagte, aur logic simple/fixed
honi chahiye). FPGA **SIMPLE, FIXED, ULTRA-LOW-LATENCY decisions** ke
liye hai, na ki general-purpose computing ke liye.

---

## Trade-offs

| Fayda | Cost |
|---|---|
| Nanosecond-scale, deterministic latency | Verilog/VHDL (poori ALAG skill, C++ se BAHUT alag) |
| CPU se BHI zyaada fast (koi instruction overhead nahi) | Reconfigure/redeploy SLOW (minutes), "hot-fix" jaisa concept mushkil |
| Parallel hardware logic | Complex/dynamic logic FPGA pe impractical |
| Predictable, jitter-free (koi OS scheduler bhi involved nahi) | Specialized hardware + expensive engineering talent |

---

## Yeh course ka scope

Is course mein FPGA **implement NAHI** hota — Verilog/VHDL/HLS (High-
Level Synthesis) poori ALAG language/toolchain/career hai. Jo
important hai samajhna: **kab** FPGA reach for karna sensible hai
(ultra-simple, ultra-fixed, ultra-latency-critical decision — jaisa
market-making ka fastest tier ka pre-trade risk check), aur **kab
nahi** (dynamic strategies, kuch bhi jo baar-baar CHANGE hota).

> **HFT relevance:** yeh 03-06 ke poore bypass-ladder ka NATURAL
> extension hai — kernel bypass CPU pe reh ke overhead hataata, FPGA
> **CPU KHUD** hata deta specific, fixed decisions ke liye.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — FPGA ko "har jagah use karna chahiye agar budget ho" samajhna
FPGA sirf SIMPLE, FIXED logic ke liye value deta — complex strategy
logic FPGA pe implement karna zyaadatar counter-productive hai
(reconfigure-slowness, development-complexity).

### Trap 2 — FPGA aur DPDK/ef_vi ko "same category" samajhna
DPDK/ef_vi (03-06) SOFTWARE hai jo CPU pe chalta (bas kernel ko bypass
karta). FPGA HARDWARE hai — koi CPU involved hi nahi us specific logic
ke liye.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| FPGA "sabse fast hai to hamesha use karo" | Sirf simple/fixed logic ke liye sensible; dynamic strategies ke liye impractical |
| FPGA CPU bypass jaisa hi hai, bas fast | Fundamentally alag paradigm -- hardware circuit, na ki software |
| FPGA seekhna is course mein zaroori hai | Explicitly out-of-scope, alag domain/career (00-START-HERE mein noted) |

---

## Exercises

1. Ek strategy team har hafte apna pricing model update karti (ML-based).
   Kya yeh FPGA pe implement karna sensible hai?
   <details><summary>Answer</summary>
   Nahi -- FPGA reconfigure karna SLOW hai (development + synthesis +
   deployment cycle, minutes-to-hours), aur ML models complex/dynamic
   hote. FPGA sirf FIXED, SIMPLE logic (jaisa ek basic risk-check ya
   market-data-parse-and-threshold) ke liye sensible hai.
   </details>

---

## Interview questions

1. FPGA CPU se fundamentally kaise alag hai?
2. FPGA kis TYPE ke use-case ke liye appropriate hai, kis type ke liye nahi?
3. FPGA ka SABSE BADA trade-off (dev velocity ke context mein) kya hai?

---

## Next
→ [`14-wire-to-wire-measurement.md`](14-wire-to-wire-measurement.md)
