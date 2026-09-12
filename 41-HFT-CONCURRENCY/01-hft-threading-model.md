# 01 — HFT threading model: pipeline, thread-per-stage, shared-nothing

## Prerequisites
- `40-MATCHING-ENGINE` (poora, especially `11-single-threaded-design.md`)
- `26-CONCURRENCY/00-README.md`, `28-LOCK-FREE/00-README.md`

## Yeh topic abhi kyun

40/11 mein humne dekha: ek matching engine SINGLE-THREADED hona chahiye
(determinism ke liye), aur real parallelism **symbol-sharding** se aati
(har symbol apna alag engine instance, alag core). Yeh folder us idea ko
**ek symbol ke ANDAR** extend karta hai — agar matching engine khud ek
thread hai, baaki system (feed receive, risk checks, logging, market-
data publish) kis threading model mein chalta?

**Answer: pipeline — thread PER STAGE, koi stage doosre ka mutable state
share nahi karta.** Yeh general-purpose multithreading (jahan threads ek
shared data-structure ko locks se protect karte) se **fundamentally
alag** approach hai.

---

## General multithreading vs HFT pipeline

| | General multithreading (26-CONCURRENCY) | HFT pipeline (yeh folder) |
|---|---|---|
| Threads kya karte | SAME kaam parallel mein karte (worker pool) | ALAG kaam sequentially (ek pipeline ka ek-ek stage) |
| Data sharing | Shared state, mutex/lock se protect | Koi shared MUTABLE state nahi — message-passing (queues) |
| Coordination | Locks, condition variables | Lock-free queues, sequence numbers |
| Scaling | Zyaada threads = zyaada parallel work (same task) | Zyaada STAGES = zyaada pipeline depth, ya zyaada SYMBOLS = zyaada parallel pipelines |
| Failure mode | Contention badhne se sab SLOW ho jaate | Ek stage slow ho to sirf usi ke AAGE wale queue backlog karte (13) |

---

## Ek typical HFT pipeline

```
  [Feed receive]  ->  [Parse/decode]  ->  [Book update]  ->  [Strategy]  ->  [Risk check]  ->  [Order send]
       thread A          thread B           thread C          thread D        thread E          thread F
         |                  |                  |                 |               |                 |
         +---queue----------+---queue----------+---queue---------+---queue-------+---queue---------+
```

Har arrow ek **queue** hai (04's SPSC, ya 05's Disruptor agar ek stage ke
output ko MULTIPLE downstream stages independently consume karte hain —
jaisa market-data-out AUR risk-monitoring dono SAME feed dekhte). Har box
ek **alag thread** hai, jo APNA data OWN karta — koi doosre thread ka data
directly mutate nahi karta.

`08_pipeline_demo.cpp` mein exactly yeh pattern build kiya gaya hai (3
stages: feed -> match -> sink), 40-MATCHING-ENGINE ke REAL engine ke saath.

---

## Kyun yeh model (aur locks NAHI)

1. **Determinism** (40/09) — matching engine jaisi stages ko single-
   threaded rehna CHAHIYE; ek pipeline design isse naturally enforce
   karta (ek stage = ek thread = koi race condition possible hi nahi
   uss stage ke ANDAR).
2. **Predictable latency** — lock contention ka koi "kabhi fast, kabhi
   OS scheduler se block" jaisa unpredictability nahi (28/06's finding:
   mutex+cv p50 ~6us vs lock-free SPSC ~0.4us, is folder mein 05 mein
   phir se, CPU-cost ke saath).
3. **Fault isolation** — ek stage crash/slow ho jaaye, baaki stages
   (apna khud ka data lekar) chalti reh sakti (upstream queue bas
   backlog karta, poora system freeze nahi hota).
4. **Scalability via sharding, not locking** — 40/11 ka symbol-sharding
   principle isi model ka natural extension: har symbol apna poora
   pipeline instance, sab independent.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "thread-per-stage" ko "thread pool" samajh lena
Thread pool mein N threads SAME kaam karte (load balance), kisi bhi
order mein. Pipeline mein har thread ek DIFFERENT, FIXED responsibility
rakhta — order matter karta (stage 2 hamesha stage 1 ke BAAD chalta,
kabhi parallel nahi usi event ke liye).

### Trap 2 — queue ko "async function call" jaisa treat karna
Ek queue sirf DATA transfer karta — koi return value, koi exception
propagation nahi hota seedha. Errors ko explicitly EVENTS ke roop mein
pipeline mein hi carry karna padta (jaisa ek "reject" event).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Zyaada threads = zyaada speed (general rule) | Pipeline mein zyaada STAGES latency BADHATE (per-hop cost) — parallelism SYMBOLS/pipelines ke across aata, ek pipeline ke ANDAR nahi |
| Shared data + locks HFT ka standard hai | Shared-NOTHING + message-passing standard hai |
| Har stage independent full-speed chalti | Slowest stage (bottleneck) poore pipeline ki throughput decide karta |

---

## Exercises

1. Ek pipeline mein 5 stages hain, har ek ki processing cost 100ns hai.
   Ek single event ka end-to-end latency kitna hoga (roughly)?
   <details><summary>Answer</summary>
   Kam se kam 500ns (5 x 100ns), PLUS har queue-hop ki hand-off latency
   (04 se — kai sau ns tak, especially agar SPSC hai). Pipeline DEPTH
   directly latency badhata — isliye real design mein "kitne stages
   zaroori hain" ek trade-off hai (kam stages = kam latency, par kam
   separation-of-concerns).
   </details>

---

## Interview questions

1. General multithreading aur HFT pipeline model ka fundamental fark.
2. Pipeline mein parallelism kahan se aati (ek symbol ke andar nahi to)?
3. Fault isolation kaise milta is model se?

---

## Next
→ [`02-single-writer-principle.md`](02-single-writer-principle.md)
