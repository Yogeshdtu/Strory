# 05 — Disruptor pattern: ring buffer, sequences, batching

## Prerequisites
- `04-spsc-queue-for-pipeline.md`

## Yeh topic abhi kyun

04's SPSC queue ek fundamental limitation rakhta: **ek item, ek consumer**
— jaise hi ek consumer `try_pop()` karta, woh item GONE ho jaata, koi
DOOSRA consumer usse nahi dekh sakta. Real pipelines mein aksar ek EVENT
ko **MULTIPLE INDEPENDENT consumers** dekhna chahte (jaisa ek trade event
ko RISK-MONITOR bhi dekhna chahta, LOGGER bhi, STRATEGY bhi — teeno **HAR**
event dekhein, na ki teeno mein baant diya jaaye). Yeh **fan-out** hai, aur
LMAX Disruptor isi problem ko solve karta.

---

## SPSC vs Disruptor — fundamental difference

```
SPSC (04): work DISTRIBUTION
  Producer -> [item1, item2, item3] -> Consumer
  Consumer POPS -- item GONE after read. Ek item, ek consumer.

Disruptor (yeh lesson): work FAN-OUT
  Producer -> [item1, item2, item3] (ring, entries STAY)
                    |         |
              Consumer A   Consumer B   (dono SAB items dekhte, apni speed pe)
```

`03_disruptor.cpp`'s demo isi ko literally PROVE karta: 4 million events
publish hote, **DONO** consumers (A aur B) apna checksum/count 4 million
pe match karte — koi item "consumer A ko mila, B ko nahi" wala split
nahi hota.

---

## Core mechanism — gating sequences

```cpp
class DisruptorRing<T, Capacity> {
    T buf_[Capacity];
    std::atomic<std::int64_t> cursor_;              // producer: "yahan tak publish ho chuka"
    std::vector<std::atomic<std::int64_t>> consumer_seqs_;  // har consumer: "main yahan tak PADH chuka"
};
```

- **Producer** `claim()` karta agla sequence number, likhta `buf_[seq & mask]`,
  phir `publish(seq)` (cursor_ ko update karta, release).
- **Consumers** apna khud ka `wait_for(next_seq)` call karte — cursor_
  ko (acquire) padhte, jab tak >= next_seq na ho jaaye spin karte, phir
  **BATCH** process karte (`next_seq` se leke `cursor_` tak, sab EK
  saath) — batching NATURALLY hoti, explicit "batch API" ki zaroorat
  nahi (`03_disruptor.cpp` mein `batches_a`/`batches_b` counters isi ko
  measure karte — is run mein ~11% wait_for() calls ne >1 event batch
  mein diye).

**Producer safety (gating):** producer sirf tab ek OLD slot OVERWRITE
karta jab **SLOWEST consumer** ne us slot ko already consume kar liya ho
(`min_consumer_seq() >= seq - Capacity`). Yeh `spsc_queue.hpp`'s
cached-index trick ka EXACT SAME idea hai, bas ab "gate" EK nahi,
**MULTIPLE consumers ka minimum** hai:

```cpp
std::int64_t claim() {
    const std::int64_t s = next_to_claim_++;
    if (s >= Capacity) {
        const std::int64_t floor = s - Capacity;
        while (cached_min_consumed_ < floor) {           // maybe blocked?
            cached_min_consumed_ = min_consumer_seq();    // refresh (scan all consumers)
        }
    }
    return s;
}
```

Agar EK bhi consumer bahut peeche reh jaaye, producer USSI consumer ke
liye SPIN karega (backpressure poore ring ke liye, sabse SLOW consumer
ke hisaab se) — yeh design ka EXPLICIT trade-off hai.

---

## Simplifications vs real LMAX Disruptor (documented, not hidden)

Real Disruptor library kaafi zyaada sophisticated hai:

| Real Disruptor | Yeh course ka `DisruptorRing` |
|---|---|
| Pluggable `WaitStrategy` (busy-spin, yielding, blocking) | Hamesha busy-spin |
| Consumer DEPENDENCY graphs (`SequenceBarrier` chains — "B sirf A ke BAAD process kare") | Sab consumers INDEPENDENT (koi ordering constraint unke beech) |
| Multi-producer support (CAS-based claim) | Single producer only |
| Exception handling per-event | Koi built-in error handling |

Yeh simplifications DELIBERATE hain — core "gated ring + fan-out +
batching" idea seekhne ke liye, production-grade library banane ke liye
nahi.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — consumer ko `wait_for()` ke baad `consumed_up_to()` bhoolna
Agar consumer batch process karke apna `consumer_seqs_[idx]` update na
kare, producer SOCHEGA yeh consumer abhi bhi PEECHE hai — permanently
GATE lag jaayega us purani sequence pe, producer ring ko kabhi aage
nahi badha payega (deadlock-jaisa hang).

### Trap 2 — Disruptor ko "SPSC ka multi-consumer version" samajh lena
Behavior FUNDAMENTALLY alag hai — SPSC mein N consumers item SPLIT
karte (work distribution), Disruptor mein N consumers HAR item dekhte
(fan-out). Galat mental model se galat use-case pe galat structure
choose ho jaata.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Disruptor "SPSC se behtar SPSC" hai | Different PROBLEM solve karta (fan-out vs distribution) — dono apni jagah sahi |
| Batching ek explicit API call hai | Naturally hoti — `wait_for()` jitna available hai utna return karta, consumer khud decide karta process karna |
| Slow consumer sirf USSE affect karta | Producer poore ring ko us SLOWEST consumer ke hisaab se gate karta -- SABKO affect karta |

---

## Hands-on

```bash
./build.ps1 fast 41-HFT-CONCURRENCY/examples/03_disruptor.cpp
```

---

## Exercises

1. 2 consumers hain, ek bahut SLOW hai (busy doing heavy work). Fast
   consumer ka throughput is design mein kya hoga?
   <details><summary>Answer</summary>
   Fast consumer khud kabhi block nahi hota apne processing mein, PAR
   PRODUCER us slow consumer ke gate se ROOK sakta (agar ring bhar jaaye)
   -- jab producer stop ho jaata (backpressure), fast consumer ke paas
   naye events aana bhi ruk jaate. Isliye "slowest consumer poore
   pipeline ki throughput decide karta" (01's principle yahan bhi).
   </details>

---

## Interview questions

1. SPSC aur Disruptor ka fundamental difference (distribution vs fan-out).
2. Gating sequence kya hai, aur producer isse kaise use karta?
3. Batching Disruptor mein "automatically" kaise hoti, explicit API ke
   bina?
4. Is course ke `DisruptorRing` ke 4 major simplifications batao real
   LMAX Disruptor ke against.

---

## Next
→ [`06-seqlock-for-snapshots.md`](06-seqlock-for-snapshots.md)
