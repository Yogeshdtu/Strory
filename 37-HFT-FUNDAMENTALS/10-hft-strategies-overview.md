# 10 — HFT strategies overview (concepts only, no alpha)

## Prerequisites
- [`09-market-makers-and-takers.md`](09-market-makers-and-takers.md)

## Yeh topic abhi kyun
**Important disclaimer pehle:** yeh lesson strategy **categories** aur
unke **engineering requirements** sikhaata hai — yeh "alpha" (actual
profitable trading signal/edge) nahi sikhaata. Real edge research,
statistical validation, aur bahut saari competitive/proprietary detail
maangta hai jo iss course ke scope se bahar hai. Yahan maqsad hai: samjho
**kaunsi category ke liye kaunsa engineering priority** hai — taaki 38-44
mein jo system banega, uska "kis liye" clear ho.

---

## Category 1 — Market making

**Kya:** dono taraf (bid aur ask) continuously quote lagana, spread capture
karna (03), inventory ko balanced rakhna.

| Engineering priority | Kyun |
|---|---|
| **Quote update speed** | Stale quote = adverse selection (03) |
| **Risk/inventory tracking** | Position limits real-time chahiye (13) |
| **Multi-symbol scale** | Zyaadatar makers bahut symbols cover karte simultaneously |

**Core loop:** market data aata -> fair-value estimate update hota (05 ka
microprice jaisa signal) -> quote reprice/re-issue hota -> repeat.

---

## Category 2 — Statistical / latency arbitrage

**Kya:** do (ya zyada) related instruments/venues ke beech temporary price
mismatch dhoondhna aur capture karna.

Do sub-flavors (concept level):

**a) Cross-venue arbitrage:** ek hi (ya equivalent) instrument do
exchanges pe alag price pe trade ho raha (temporarily) — dono jagah
simultaneously trade karke difference capture karna.

**b) Latency arbitrage:** ek venue pe price-moving info pehle aati (ya
pehle process ho jaati) doosri venue ke comparison mein — jitni jaldi
dusri venue mein react karo, utna edge (yeh directly 07 ke FIFO-priority
discussion se connect karta).

| Engineering priority | Kyun |
|---|---|
| **Raw latency (tick-to-trade)** | Poori strategy ka core edge yehi hai |
| **Multi-venue connectivity** | Do jagah simultaneously monitor + act karna hota |
| **FOK/atomic execution (04)** | Ek leg fill, doosra na ho = unhedged risk |

---

## Category 3 — Event-driven

**Kya:** ek discrete, identifiable event (news release, index rebalance,
economic data print, corporate action) ke turant baad react karna, jab
market pehle uss info ko poori tarah absorb kar leti.

| Engineering priority | Kyun |
|---|---|
| **Event detection latency** | Event se react tak ka time hi window hai |
| **Parsing speed** (agar text/structured feed) | Info jitni jaldi decode ho, utna jaldi act ho sakta |
| **Pre-computed decision logic** | Event aane ke baad "sochne" ka time nahi hota — decision tree pehle se ready honi chahiye (23-compile-time-dispatch jaisi techniques yahan literally apply hoti) |

---

## Common thread — sab categories mein

| Cheez | Har category mein zaroori |
|---|---|
| Fast, correct market data processing (38) | Input hi galat/slow to sab galat |
| Order book (39) | State jispe decision based hota |
| Risk system (13) | Har strategy ko bound karna zaroori, chahe kitni bhi "sure" ho |
| Latency budget thinking (14) | Kaunsi stage critical hai, category-specific |

**Sabse important insight:** har category ka **latency budget alag jagah
tight hai.** Market making mein quote-update path critical hai; latency
arb mein poora tick-to-trade path critical hai; event-driven mein event-
detection-to-decision critical hai. **"Sab kuch fast karo" ek engineering
goal nahi hai — "kaunsa specific path critical hai" pehle pata karna hai**
(exactly 24-tradeoffs-and-when-not-to ka Rule 0: measure first).

---

## Yeh course mein isse hum kya karenge (aage)

44-HFT-PROJECTS tak, hum ek **simplified market-making-style** system
banayenge (feed handler -> book -> simple quoting logic -> risk -> order
gateway) — **koi real alpha/signal research nahi**, sirf yeh dikhana ki
poora pipeline kaise design/measure/optimize hota engineering perspective
se. Yeh production-ready trading strategy nahi hai, ek **learning
system** hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sochna ki "strategy" sirf ek if-statement hai
Real strategies statistical models, risk-adjusted sizing, continuous
calibration involve karti — course ka focus **infrastructure** hai jo
kisi bhi strategy ko run kar sake, strategy khud research/domain-expertise
ka separate, deep field hai.

### Trap 2 — latency arb ko "sabse achhi" strategy samajhna
Har category ka apna risk/reward/competitive-dynamics profile hai. Latency
arb mein competition **extreme** hai (sabse fast firm hi jeetti,
"almost fast" kuch nahi jeetta) — yeh sabke liye best fit nahi.

### Trap 3 — "no alpha" ko "yeh sab useless hai" samajh lena
Ulta — infrastructure (jo course sikhaata) hi **necessary condition** hai
kisi bhi alpha ko monetize karne ke liye. Best signal bhi slow/buggy
infra pe fail hoga.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Sab HFT strategies same hain | Alag category, alag latency-critical path |
| Yeh course "trading signal" sikhaayega | Infrastructure/engineering sikhaata, alpha research nahi |
| Latency arb sabse acha hai | Har category ka apna trade-off, extreme competition |
| Fast hona hi kaafi hai kisi bhi strategy ke liye | Sahi signal + correct risk control equally zaroori |

---

## Exercises

1. Ek strategy do exchanges ke beech same stock ka price-mismatch dhoondti
   hai — kaunsi category, aur kaunsa engineering priority sabse critical?
   <details><summary>Answer</summary>
   Cross-venue (statistical/latency) arbitrage. Sabse critical: raw
   tick-to-trade latency (dono venues tak) + multi-venue connectivity +
   atomic/FOK-style execution (ek leg fill, doosra na ho to unhedged risk).
   </details>

2. Ek market-making strategy 200 symbols simultaneously quote karti. Kyun
   iske liye "raw single-symbol latency" utna critical factor nahi ho
   sakta jitna "multi-symbol scale/throughput"?
   <details><summary>Answer</summary>
   Agar strategy ek symbol ko extreme-fast (par baaki 199 ko slow/late)
   process karti hai, total system value limited hai — poore portfolio ka
   quote-freshness matter karta, na ki ek symbol ka absolute-fastest
   number. Throughput/scale (16-batching jaisi techniques) yahan zyada
   relevant ho sakta pure single-op latency se.
   </details>

---

## Interview questions

1. Market making, statistical/latency arbitrage, event-driven — teeno ka
   core loop aur latency-critical path batao.
2. Latency arbitrage mein competition "extreme" kyun hoti (07 se connect
   karo)?
3. Kyun infrastructure "alpha" ke bina bhi zaroori hai, aur alpha
   infrastructure ke bina kya hota?

---

## Next
→ [`11-colocation.md`](11-colocation.md)
