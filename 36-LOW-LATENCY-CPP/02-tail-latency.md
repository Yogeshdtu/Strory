# 02 — Tail latency: p99 / p99.9 / p99.99

## Prerequisites
- `35-PROFILING-BENCHMARKING/05-percentiles.md` (nearest-rank, nines,
  coordinated omission, fan-out amplification)
- `01-latency-throughput-jitter.md`

## Yeh topic abhi kyun
Folder 35 mein percentiles ki mechanics seekhi. Yahan ek line: **is folder
ki har technique ka scorecard tail latency hai, p50 nahi.** Yeh lesson woh
mindset pakka karta.

---

## Kyun p50 se optimize karna galat hai

Ek hot path jo 99% time 200 ns leta aur 1% time 5 µs — uska:
- **p50 = 200 ns** (dikhne mein great)
- **mean ≈ 250 ns** (thoda upar, tail ne kheencha)
- **p99 = 200 ns** (abhi bhi fast!)
- **p99.9 = ~5 µs** (yahan cliff)

Agar tum p50 pe optimize karo — 200 → 180 ns — poore system pe farak ~0.5%.
Agar tum us **1% slow path ka source** dhoondh ke eliminate karo — p99.9
5 µs → 250 ns — woh 20x improvement us tail pe, aur **fan-out / burst rate**
ki wajah se woh tail hi user-facing / trade-facing latency ban jaati (35/05).

> **Rule:** hot path pe optimization = tail ka source dhoondo aur maaro.
> "Typical case thoda tez" HFT mein noise hai.

---

## "Kitne nines chahiye" — yeh ek business decision hai

| Context | Design target |
|---|---|
| Batch / analytics | mean / p50 (tail amortized away) |
| Web backend | p99 (fan-out → p99 becomes user median) |
| RPC mesh, 50-service fan-out | p99.9 (0.99^50 ≈ 0.6 → half of requests hit a p99 tail) |
| HFT tick-to-trade | **p99.9 aur p99.99** — har missed window ka $ cost |
| RT audio / control loop | **max** (ek missed deadline = a glitch / a fault) |

Jitna aage ka nine, utna zyada samples chahiye use claim karne ke liye
(p99.9 → 10k+, p99.99 → 100k+ — 35/04) aur utna hi bounded banana mushkil.

---

## Tail ke sources (is folder ka map)

| Source | Lesson | Fix |
|---|---|---|
| hot-path allocation | 04–09 | pre-allocate + pool / arena / ring |
| page faults | 18 | `mlockall` + pre-fault + warm-up |
| syscalls / blocking | 17 | busy-poll, batch, `io_uring` |
| context switch / preemption | 19 | pin + `isolcpus` + `nohz_full` |
| cache / TLB misses | 10, 20 | locality, hot/cold split, warming |
| false sharing | 11 | padding, per-thread data |
| branch mispredict (unpredictable) | 12 | branchless where genuinely random |
| virtual / indirect call mispredict | 13 | CRTP / variant / switch |
| `std::function` heap alloc | 14 | template / `function_ref` |
| data-dependent work | 01 ex 3 | bound the work / incremental / delta |
| I-cache pressure | 21 | hot/cold split, function ordering, PGO |
| GC | — | **C++ mein GC nahi hai** (lesson 03) — yeh Java/C#/Go ka problem |

---

## Ek concrete "tail budget"

```
  stage            p50      p99.9 budget
  feed decode      150 ns   300 ns
  book update      180 ns   350 ns
  strategy          200 ns   400 ns
  risk               90 ns   180 ns
  order encode      110 ns   220 ns
  -----------------------------------------
  hot path total    730 ns  ~1.45 us  (p99.9)
```

Har stage ka **p99.9** alag budget hai — us se upar gaya to us stage ke tail
source ko dhoondo. p50 total (730) achha dikhta par p99.9 (~1.45 µs) hi woh
number hai jispe SLA / risk model banega.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — p50 improvement ko "win" bolna
Poore system pe Amdahl-scaled asar tiny. Tail source maaro.

### Trap 2 — max ko SLA banana
Max ek single sample — infinitely noisy (35/05). SLA p99.9/p99.99 pe.
Max ko "worst observed, informational".

### Trap 3 — GC se darna (C++ mein)
C++ mein tracing GC nahi hai. `shared_ptr` refcount = deterministic (par
atomic — 27), `delete` = deterministic (par allocator tail — 04). Tail
sources yahan alag hain (upar wali table).

### Trap 4 — coordinated omission (load testing)
Fixed-rate load mein reply ka wait karke agli request na bhejna → tail
under-count (35/05). Constant-rate sender + interval correction.

### Trap 5 — p99.9 claim, 3000 samples
p99.9 of 3000 = 3 samples. Noise. Us nine ke liye 10k+ (35/04).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "p50 tez karo" | tail ka source dhoondo aur eliminate karo |
| "C++ mein bhi GC pause" | C++ mein GC nahi; tail sources alag (alloc/fault/syscall/...) |
| "max = SLA" | p99.9 / p99.99; max informational |
| "ek nine kaafi (p99)" | fan-out / rate → aur nines chahiye |
| "tail rare hai, chhod do" | rare event × high rate = frequent; × fan-out = user median |

---

## Exercises

1. Ek service ke do versions. V1: p50 300 ns, p99 320 ns, p99.9 340 ns,
   max 900 ns. V2: p50 **180 ns**, p99 200 ns, p99.9 **12 µs**, max 15 µs.
   V2 ka p50 40% behtar hai. HFT hot path pe kaunsa lo, aur V2 ke tail ke
   baare mein kya sawaal poochoge?

   <details><summary>Answer</summary>

   **V1 lo.** V2 ka p50 (180) sundar hai par p99.9 = **12 µs** ek disaster —
   35x its own p50, aur V1 ke max (900 ns) se 13x bura. HFT mein "usually
   180 ns par kabhi 12 µs" >> "hamesha ~320 ns". V1 predictable, bounded.
   V2 ke bare mein: **12 µs kahan se?** V2 ne p50 kaise ghataya — kya usne
   ek cache add kiya (jo occasionally cold → 12 µs miss storm)? ek lazy
   allocation (jo first-use pe faults)? ek fast-path jo ek slow fallback pe
   girta rare inputs pe? V2 ka p50 win real hai — agar us 12 µs tail ka
   source fix ho jaaye (V1 ka bulk + V2 ka p50), to woh best hai. Tail
   source dhoondo, hataao, phir V2.
   </details>

2. Ek naya joining engineer kehta "hamare Java risk service ka p99.9 bahut
   achha hai, C++ mein rewrite ki kya zaroorat — dono mein GC pause to hoga
   hi." Iska jawab.

   <details><summary>Answer</summary>

   **C++ mein tracing garbage collector nahi hai.** Java/Go/C# mein GC ek
   background (ya stop-the-world) pass chalata jo unpredictable ~ms pauses
   deta — yahi unka p99.9 tail ka bada source hai (ya tuning ka bada sirdard).
   C++ mein memory management **deterministic** hai: `delete` / dtor jab tum
   bolo tab, RAII se scope end pe. Uska apna tail hai — **allocator internals**
   (free-list walk, `mmap`, coalesce, arena lock — example 01) — par woh
   (a) tumhare control mein hai aur (b) pool/arena/pre-allocation se poori
   tarah eliminate ho sakta (baaki folder). Java mein GC ko "eliminate"
   nahi kar sakte, sirf tune/delay (allocation-free coding + huge heaps +
   ZGC/Shenandoah). C++ hot path = **zero allocation, zero GC, deterministic
   teardown**. Wahi rewrite ka case hai. (Par: agar Java service already p99.9
   budget meet kar rahi hai aur rewrite ka $ cost > benefit, to rewrite mat
   karo — measure the actual gap first.)
   </details>

---

## Interview questions

1. p50 optimize karne ka poore-system pe asar kyun chhota; kya optimize karo instead.
2. "Kitne nines" ka decision kis pe based — fan-out / rate / cost-of-miss.
3. C++ mein GC pause kyun nahi; C++ ka equivalent tail source kya.
4. Tail latency ke 6+ sources aur unke fixes (is folder ka map).
5. p99.9 vs max — SLA kis pe, aur kyun.

---

## Next
→ [`03-sources-of-jitter.md`](03-sources-of-jitter.md)
