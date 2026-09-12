# 11 — Single-threaded kyun: determinism vs parallelism trade-off

## Prerequisites
- `10-event-sourcing.md`
- `26-CONCURRENCY/00-README.md` (races, locks ka basic vocabulary — is
  lesson mein use hota, deep-dive 41-HFT-CONCURRENCY mein)

## Yeh topic abhi kyun

Yeh ek COUNTER-INTUITIVE design decision hai: HFT "fast" chahta, "fast"
ka matlab zyaadatar log "parallel/multi-threaded" samajhte. Par is
folder ka `MatchingEngine` — aur zyaadatar REAL matching engines —
**single-threaded** hote hain unke core matching loop ke liye. Yeh
lesson SAMJHATA hai kyun, aur asli parallelism kahan chhupi hoti hai.

---

## Ek single symbol ka matching = single-threaded

**Ek hi symbol (jaisa RELIANCE) ke liye, saare orders EK strict sequence
mein process hone chahiye** — kyunki price-time priority (02) ki DEFINITION
hi "arrival order" pe depend karti. Agar do orders ek SAME symbol ke liye
DO alag threads pe simultaneously process ho rahe hon, "kaun pehle aaya"
ka concept hi **race condition** ban jaata — aur 09 ka determinism
guarantee **impossible** ho jaata (race condition literally non-determinism
ki definition hai).

```
Thread 1: Submit(order A) ----+
                                +---> DONO EK SAME price level ko
Thread 2: Submit(order B) ----+      simultaneously mutate karne ki
                                      koshish karte -- kaun PEHLE match
                                      karta? Undefined, run-to-run alag.
```

Isse avoid karne ke liye locks laga sakte ho (mutex around the whole
book), par tab bhi **"kaunsa thread lock PEHLE paata"** OS scheduler pe
depend karta — jo khud non-deterministic hai. Lock lagane se crash/data-
race toh ruk jaata (safety), par **determinism** (09's guarantee) NAHI
milta — bas "koi crash nahi" milta, "same input same output" nahi.

---

## Toh parallelism KAHAN hai?

**Sharding by symbol** — har symbol ka apna INDEPENDENT matching engine
instance, alag thread/core pe. RELIANCE ka matching engine TCS ke
matching engine se KABHI interact nahi karta — unke beech koi shared
state hi nahi hai.

```
   Core 1: RELIANCE matching engine  (single-threaded, deterministic)
   Core 2: TCS matching engine       (single-threaded, deterministic)
   Core 3: INFY matching engine      (single-threaded, deterministic)
   ...
```

Yeh "**embarrassingly parallel**" hai — jitne symbols utne independent
units, koi coordination overhead nahi (kyunki koi shared state nahi hai
JISKO coordinate karna pade). **Parallelism symbol-level pe hai, ek
symbol ke ANDAR nahi.**

> **HFT relevance:** yeh exact pattern real exchanges (NSE, NYSE, etc.)
> mein use hota — har product/symbol group apna independent matching
> engine chalata. Ek symbol ka matching engine crash ho jaaye toh baaki
> symbols UNAFFECTED rehte (fault isolation bhi milta, parallelism ke
> saath saath).

---

## Sequencer pattern -- ek core ke ANDAR bhi structure

Ek symbol ke ANDAR, real systems typically **ek single "sequencer" thread**
rakhte jo saare incoming orders ko EK deterministic sequence assign karta
(hamara `next_seq_` counter isi role ka simplest version hai), phir
matching logic ISI sequence ko follow karke chalti — chahe upstream (network
se orders receive karna) multi-threaded ho, matching-decision-making khud
strictly single-threaded/sequential rehti.

Yeh **LMAX Disruptor** jaisi architectures ka core idea hai (ek famous
low-latency Java trading system design) — ek single writer thread poori
"business logic" (matching) chalata, baaki sab (network I/O, logging,
market-data publishing) alag threads mein PARALLEL hota, par woh sirf
matching engine ke OUTPUT (Trade events) consume karte, kabhi uski state
mutate nahi karte. 41-HFT-CONCURRENCY isi pattern ko deep-dive karega.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "lock lagao, determinism aa jaayegi"
Locks **data races** rokte (memory corruption, crash) — woh **thread-
scheduling non-determinism** nahi rokte (kaunsa thread pehle lock leta,
OS ke haath mein hota, predictable/repeatable nahi). Yeh do ALAG
problems hain, ek solution (locks) sirf pehla solve karta.

### Trap 2 — "single-threaded = slow"
Ek single-threaded matching loop, agar cache-friendly data structures
(39's V3 jaisa, ya is folder ka simpler V1-style) use kare, MILLIONS of
ops/sec kar sakta (`08_engine_bench.cpp`'s ~5.63M ops/sec is machine pe)
— single core ki raw speed itni high hoti ki poore exchange ka ek symbol
ka volume aasani se handle ho jaata. "Single-threaded" ka matlab "slow"
NAHI hai jab tak per-operation cost genuinely low ho.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| HFT matlab sab kuch multi-threaded hona chahiye | Matching CORE single-threaded, parallelism symbol-SHARDING se aati |
| Mutex/lock lagane se determinism mil jaati | Lock races rokta, thread-scheduling-order non-determinism NAHI rokta |
| Single-threaded = automatically slow | Single core ki raw throughput HFT symbol-volume ke liye kaafi hoti (measured: ~5.6M ops/sec) |

---

## Exercises

1. Ek exchange 5000 symbols trade karta. Single-threaded-per-symbol
   design mein, yeh 5000 symbols ko kaise parallelize karega?
   <details><summary>Answer</summary>
   Har symbol ka apna independent matching-engine instance, alag thread/
   core pe (ya kai symbols ek thread pe agar cores kam hon, par har
   SYMBOL ke andar strictly sequential). "Sharding by symbol" -- koi
   shared state symbols ke beech nahi, isliye zero coordination overhead.
   </details>

---

## Interview questions

1. Kyun ek SINGLE symbol ka matching multi-threaded nahi kiya jaata?
2. Lock lagane se determinism kyun NAHI milti (sirf safety milti)?
3. "Sharding by symbol" pattern explain karo, real exchanges mein isse
   kyun use kiya jaata.

---

## Next
→ [`12-state-machine-design.md`](12-state-machine-design.md)
