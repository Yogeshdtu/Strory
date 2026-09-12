# 07 — False sharing (deep): measured, padding se fix

## Prerequisites
- `02-cache-lines.md` (64-B line as coherence unit)
- `27-ATOMICS-MEMORY-MODEL/` ya `26-CONCURRENCY/` (MESI, threads)
- example `04_false_sharing.cpp`

## Yeh topic abhi kyun
Yeh multi-threaded performance ka sabse aam **chhupa** bug hai. Aapke do
threads bilkul alag variables likhte hain — koi data race nahi, code correct
hai — par woh dono variables **ek hi 64-byte cache line** pe hain. Coherence
protocol line ko unit maanta, isliye har write doosre core ki copy invalidate
karta, aur line cores ke beech ping-pong karti. Result: threads add karne se
throughput **badhne ke bajaye ghata** hai. Example `04` mein ~6x se ~40x tak
slowdown, aur — important — **run-to-run alag** (jitter).

---

## Mechanism — MESI ping-pong

Har cache line har core mein ek "state" mein hoti (MESI):
- **M**odified — sirf yahan, aur dirty
- **E**xclusive — sirf yahan, clean
- **S**hared — kai cores mein, read-only
- **I**nvalid — stale, use mat karo

Jab core 0 ek line pe **write** karta:
1. Use line **M** state mein chahiye → agar kisi aur core ke paas S/E/M copy
   hai, unhe **Invalidate** message bhejo.
2. Woh cores apni copy I kar dete (agar M thi to pehle write-back).
3. Core 0 likhta hai.

Ab core 1 usi line ke **doosre byte** ko likhna chahta:
1. Core 1 ke paas copy I hai (core 0 ne invalidate kiya) → miss.
2. Core 1 line ko M chahiye → core 0 ko invalidate bhejo → core 0 write-back
   → line core 1 ke paas.
3. Core 1 likhta hai.

Har alternating write = ek full **coherence transaction** (~40-100+ ns,
cores ki doori pe depend). Dono cores kabhi apni line L1 mein steady nahi
rakh paate — woh continuously chheeni jaati. Yeh **false** sharing hai kyunki
woh actually ek doosre ka data nahi chahte — bas wobhagya se same line.

```
line L (64 B): [ counter0 | counter1 | counter2 | counter3 | ... ]
   ^core0 writes counter0        ^core1 writes counter1
   -> har write doosre ko invalidate -> L har baar core0<->core1 bounce
```

---

## Measured — example `04`

4 threads, har ek apna `uint64_t` counter 80M baar increment. Is box:

| Layout | ns per increment | total |
|---|---|---|
| **A** packed (`struct{u64 v;}` array, sab ~1 line pe) | ~1.8 – 3.7+ (jittery) | 0.5 – 1.2 s |
| **B** padded (`alignas(64)`, har counter apni line) | ~0.2 – 0.3 (stable) | ~0.08 s |

**A / B = ~6x se ~44x, run-to-run alag.** B har run stable. A ka time OS ke
scheduling pe depend karta — 4 threads kaunse physical cores pe pade:
- Same CCX (L3 share) → line bounce "sasta" (L3 se)
- Alag CCX → line inter-CCX fabric se bounce → "mehnga"
- SMT siblings (is box pe SMT nahi, par general) → L1 share, sabse sasta

**Isliye false sharing sirf slow nahi — jittery.** p99 latency kills. Ek run
6x, agla 40x. Predictable low latency chahiye to ise poori tarah khatm karo.

---

## Diagnosis

1. **Scaling test.** 1 thread → 2 → 4 → 8. Agar throughput per-thread **girta**
   hai (ya total badhta nahi), aur data structures share nahi lagti → false
   sharing suspect.
2. **`perf c2c`** (cache-to-cache, Linux, lesson 14). Ye exactly batata:
   kaunsi cache line, kaunse offsets, kaunse threads, kitne HITM (modified
   line doosre core se aayi) events. False sharing ka signature: ek line,
   alag offsets, alag threads, high HITM.
3. **`perf stat -e`** `mem_load_l3_hit_retired.xsnp_hitm` (Intel) ya
   equivalent — cross-core modified-line hits.
4. **Layout audit.** Jo bhi struct/array multiple threads likhte hain — uske
   fields ke offsets nikaalo, dekho kaunse same 64-B window mein.

---

## Fixes

### 1. Padding / alignment
```cpp
#include <new>
#ifdef __cpp_lib_hardware_interference_size
  constexpr size_t kLine = std::hardware_destructive_interference_size;
#else
  constexpr size_t kLine = 64;
#endif

struct alignas(kLine) PerThread {
    std::uint64_t counter;
    char pad[kLine - sizeof(std::uint64_t)];   // rest of the line reserved
};
std::vector<PerThread> stats(num_threads);      // har element apni line
```

### 2. Per-thread local, combine at end (best)
```cpp
// har thread apna LOCAL accumulator (stack / thread_local) — line kabhi share nahi
void worker(int tid) {
    std::uint64_t local = 0;
    for (...) local += ...;
    global_stats[tid].store(local, std::memory_order_relaxed);   // ek baar, end me
}
```
Yeh best hai kyunki hot loop mein koi shared line touch hi nahi hoti — sirf
final ek write. Padding tab bhi rakho `global_stats` pe.

### 3. Structure splitting
Producer-mostly aur consumer-mostly fields ko alag lines pe (SPSC ring:
`alignas(64) head;` … `alignas(64) tail;` — folder 28).

### 4. `alignas` on the hot atomic
```cpp
struct Queue {
    alignas(64) std::atomic<size_t> head;
    alignas(64) std::atomic<size_t> tail;
    alignas(64) T buffer[N];
};
```

---

## Constructive sharing (ulta case)

Kabhi aap **chahte** ho ki do cheezein ek line pe hon:
`std::hardware_constructive_interference_size` — agar do fields hamesha ek hi
thread saath padhta/likhta hai, unhe ek line mein rakho → ek miss, do nahi.
E.g. ek lock aur woh data jise woh guard karta — same line pe → lock lene ke
baad data already aa gaya.

Tension: constructive (ek thread ke liye pack) vs destructive (threads ke
beech separate). Layout decide karta kaun jeetta — profile.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `std::atomic` ko "automatically safe from false sharing" maan lena
`atomic` correctness deta, layout nahi. `std::atomic<int> a, b;` adjacent →
same line → false sharing. `alignas` khud lagana padta.

### Trap 2 — `std::vector<std::atomic<T>>` of small T
Har element 4-8 B → ~8-16 per line → agar alag threads alag indices likhen →
massive false sharing. Element ko line-sized banao.

### Trap 3 — array of mutexes
`std::mutex locks[64];` — `sizeof(std::mutex)` ~40 (libstdc++) → ~1.5 per
line → adjacent locks contend on the line even without contending on the
lock. `struct alignas(64) PaddedMutex { std::mutex m; };`.

### Trap 4 — padding daala par `alignas` nahi
`struct { u64 v; char pad[56]; };` — `sizeof` 64, par agar array ka base
64-aligned nahi to elements phir bhi lines straddle. `alignas(64)` **aur**
padding dono.

### Trap 5 — false sharing sirf counters mein sochna
Koi bhi frequently-written field: flags, `last_seen_ts`, ref-counts, ring
indices, per-thread RNG state. Read-only shared data mein false sharing
**nahi** hota (sab S state, koi invalidate nahi).

### Trap 6 — over-padding, cache waste
Har chhoti cheez `alignas(64)` = 60 B padding × N. Sirf **contended, hot,
written-by-multiple-threads** fields pe. Ek 1000-element array ko line-pad
karna = 64 KB → L1 se bahar.

---

## > **HFT relevance**

> - **Har shared, written data structure ko audit karo.** SPSC/MPSC queues,
>   sequence counters, per-thread stats, event flags — offsets nikaalo,
>   `alignas(64)` jahan do threads alag fields likhte hain.
> - **`head` aur `tail` alag lines** — folder 28 ka core lesson. Producer ke
>   `tail` write ko consumer ke `head` line se door rakho.
> - **Per-core sharding.** Har core apna order book shard / apni stats —
>   line-aligned, koi cross-core write nahi. Aggregation alag (cold) path pe.
> - **Jitter ka source.** Agar p99 latency spiky hai aur code single-thread
>   clean chalta hai — `perf c2c` chalao. False sharing p50 ko bhi nahi
>   badalta par p99 ko double kar deta.
> - **Benchmark on the real topology.** 2-socket / multi-CCX box pe false
>   sharing ki cost 1-socket se kai guna — production hardware pe test.

---

## Hands-on

```bash
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/04_false_sharing.cpp
# kai baar chalao -- A/B ratio 6x se 40x tak jhoolega (scheduling)

# Linux -- exact culprit line + offsets + threads:
perf c2c record -- ./your_prog
perf c2c report --stdio        # "HITM" wali lines dekho, offsets note karo
```

Experiment: example `04` mein `kThreads` ko 2, 4, 8 karke A ka time dekho.
Aur `Padded` mein `alignas(kLine)` hata ke dekho (padding rakh ke) — agar
`vector` base aligned nahi to phir bhi thoda false sharing.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "alag variables = no sharing problem" | same 64-B line = coherence ping-pong |
| "`std::atomic` false sharing se bachata" | correctness haan, layout nahi — `alignas` khud |
| "false sharing = thoda dheema" | ~6-40x (example 04), aur **jittery** |
| "sirf counters affected" | koi bhi frequently-written adjacent field |
| "padding kaafi hai" | padding **+** `alignas(64)` dono chahiye |
| "read-only shared data mein bhi hota" | nahi — sab Shared state, koi invalidate nahi |

---

## Exercises

1. `struct Stats { std::atomic<uint64_t> msgs; std::atomic<uint64_t> bytes;
   std::atomic<uint64_t> errors; };` — ek `Stats` global, 3 threads har ek
   ek field increment karta. Kya problem, aur 2 alag fixes.

   <details><summary>Answer</summary>

   Teeno atomics ek hi 64-B line pe (offsets 0, 8, 16) → har thread ka
   increment doosre 2 threads ki line-copy invalidate karta → line 3 cores
   ke beech continuously bounce → throughput ~1/3 ya worse, jittery. **Fix 1**:
   `struct Stats { alignas(64) std::atomic<uint64_t> msgs; alignas(64) ...
   bytes; alignas(64) ... errors; };` → har field apni line. **Fix 2** (better):
   har thread `uint64_t local_msgs` (stack) mein count kare, aur periodically
   (ya end pe) `relaxed` add kare global mein → hot loop mein zero shared
   writes. Global ko phir bhi line-pad karo.
   </details>

2. Aapne ek `std::vector<std::atomic<int>> flags(1000)` banaya, 8 threads
   har ek ~125 flags ko set karta hai (contiguous ranges: thread 0 → 0..124,
   thread 1 → 125..249, ...). False sharing kahan?

   <details><summary>Answer</summary>

   Boundary lines pe. Thread 0 ka flag 124 aur thread 1 ka flag 125 — `int`
   4 B, 16 per line → flags 112..127 ek line pe → thread 0 (flag 112-124)
   aur thread 1 (flag 125-127) us **ek line** ke liye ladte hain. Har range-
   boundary pe aisa ek line. 8 threads → 7 contended lines. Fix: har thread
   ka range ek 64-B (16-flag) boundary pe align karo (thread t → `[t*128,
   t*128+128)` with 128 divisible by 16), ya flags ko `struct alignas(64)
   { int v; }` banao (16x memory, sirf agar zaroori), ya per-thread local
   bitset + merge.
   </details>

3. `perf c2c report` dikhata hai: ek cache line, offset 0 pe thread A writes,
   offset 32 pe thread B writes, HITM count 2M. Kya yeh false ya true sharing?
   Fix kaise?

   <details><summary>Answer</summary>

   **False sharing** — A aur B alag offsets (0 vs 32) likhte hain, matlab woh
   alag data hai jo bas ek line share kar raha. (True sharing hota to dono
   *same* offset touch karte.) Fix: un do fields ko alag cache lines pe daalo
   — struct ko reorganize karo taaki A ka field aur B ka field ≥ 64 B door
   hon (`alignas(64)` on the second, ya ek padding array beech mein). HITM
   2M = 2M cross-core coherence transactions = shayad tens of ms wasted +
   jitter.
   </details>

---

## Interview questions

1. False sharing — define, aur "false" kyun.
2. MESI ping-pong — do cores alternating writes pe line ka safar.
3. Example `04` ka A/B run-to-run jitter kyun (scheduling / topology).
4. 3 fixes — padding, per-thread-local, structure splitting — trade-offs.
5. `std::atomic` false sharing se kyun nahi bachata.
6. `perf c2c` output mein false vs true sharing kaise distinguish karte.
7. `hardware_constructive_interference_size` — kab do cheezein ek line pe chahiye.

---

## Next
→ [`08-cache-friendly-structures.md`](08-cache-friendly-structures.md)
