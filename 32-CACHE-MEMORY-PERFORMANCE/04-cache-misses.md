# 04 — Cache misses: compulsory, capacity, conflict (+ coherence)

## Prerequisites
- `03-cache-organization.md`
- `31-CPU-ARCHITECTURE/06-out-of-order-execution.md` (MLP, load stalls)

## Yeh topic abhi kyun
"Miss" ek shabd hai par teen alag bimariyan hain, aur **har ek ka ilaaj alag**.
Profiler bolega "L1 miss rate 20%" — par woh compulsory (data pehli baar
aa raha) hai, capacity (working set bada) hai, ya conflict (same-set thrash)
hai? Diagnosis galat → fix galat. Yeh lesson teenon ko pehchanna sikhata.

---

## The 3 C's

### 1. Compulsory (cold / first-reference) miss
Data pehli baar chhua ja raha — cache mein hona hi nahi tha. Ise poori tarah
avoid nahi kar sakte (kabhi to load karna hi hai), par **kam kar sakte**:

- **Prefetch** (HW ya SW, lesson 06) — miss abhi bhi hota par latency kaam
  ke peeche chhup jaati.
- **Bade lines / spatial locality** — ek miss ke saath 64 B aata, agle 15
  elements "free."
- **Kam data** — smaller types, compression → kam distinct lines → kam
  compulsory misses.

### 2. Capacity miss
Working set cache se bada hai. Aap sab kuch touch karte ho, phir dobara — par
tab tak woh evict ho chuka kyunki beech mein aur data aa gaya. "Infinite
associativity, same size" cache mein bhi yeh miss hota.

**Ilaaj: working set shrink karo ya usko cache-size ke tukdon mein process
karo:**
- **Blocking / tiling** (lesson 15, example `07`) — bade problem ko L1/L2-sized
  chunks mein todo, har chunk ko poora process phir agla.
- **Loop fusion** — do passes ko ek mein milao taaki data ek hi baar aaye.
- **Smaller types / SoA** — kam bytes = zyada elements cache mein.
- **Streaming with NT stores** (lesson 12) — write-only data cache pollute
  na kare.

### 3. Conflict miss
Working set cache mein fit ho **sakta** tha, par bad address mapping se kai
hot lines ek hi set ke liye ladti hain (lesson 03). Fully-associative cache
mein yeh miss **nahi** hota.

**Ilaaj:**
- **Padding** — array dimensions ko power-of-two se hataao (`N+1`, `N+8`).
- **Layout change** — hot arrays ke base addresses spread karo.
- **Access order** — same-set addresses ko time mein spread karo.

### (4th C) Coherence / communication miss
Multi-core. Line aapke L1 mein thi, par doosre core ne usme (ya usi line ke
kisi aur byte mein) likha → aapki copy invalidate → agla access miss. **True
sharing** (sach mein woh data chahiye) ya **false sharing** (bas same line,
lesson 07). Example `04`: ~6-40x slowdown.

---

## Kaunsa miss hai? — diagnosis table

| Symptom | Likely |
|---|---|
| Pehli pass slow, doosri pass fast | **compulsory** (ab warm) |
| Har pass equally slow, working set > cache size | **capacity** |
| Working set < cache size par phir bhi high miss rate | **conflict** (check strides / power-of-2 dims) |
| Miss rate scales with thread count, single-thread fine | **coherence** (false/true sharing) |
| `perf`: `l1d.replacement` high, `l2_lines_in` moderate | capacity/conflict |
| `perf`: `mem_load_retired.l3_miss` high | working set > L3 → DRAM bound |

`perf` counters (Linux, lesson 14):
```
L1-dcache-load-misses          # kitne L1 miss
l1d.replacement                # kitni lines L1 se nikali (capacity/conflict signal)
l2_rqsts.miss / LLC-load-misses
mem_load_retired.l3_miss       # DRAM tak jaane wale
dTLB-load-misses               # alag problem (lesson 11)
```

---

## Miss ki cost — aur OoO usse kitna chhupa sakta

Ek L1 miss ki nominal cost L2 latency (~12 cyc). Par **dependent** load jo
DRAM tak jaata (~150-200 cyc) — us par jo instructions depend karti hain woh
sab ruk jaati.

OoO engine (folder 31) is stall ko **partially** hide karta:
- **MLP (memory-level parallelism)**: CPU ke paas ~10-12 "line fill buffers"
  (LFB / MSHR) — ek saath itne outstanding misses ho sakte. Agar aapke misses
  **independent** hain (`sum += a[idx[i]]`, idx sequential-read), to CPU 10
  misses ek saath launch karta → effective latency ~real/10. Example `02`
  PART 2: random line access ~32 ns (true ~80 ns) — MLP ne 2.5x overlap diya.
  Example `06` S1: light gather ~9 ns — MLP ne aur zyada.
- **Kya hide NAHI hota**: **dependent chain of misses** — pointer chasing
  (`node = node->next`), jahan agla address current load ke result se aata.
  Zero MLP → har hop poori latency → example `08` curve B (~95 ns/hop at
  8 MiB+). List traversal, tree walk, hash chaining — sab yehi.

**Design rule:** misses ko independent banao (index arrays, batch lookups,
SoA) taaki MLP kaam kare. Pointer-chasing hot path se hataao.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — miss type guess karke fix lagana
"Miss rate high hai, prefetch add karta hoon" — agar woh capacity miss tha to
prefetch bandwidth aur waste karega (example `06` S2: 3x SLOWER). Pehle
identify (pass-1-vs-2, working-set-vs-cache, thread scaling), phir fix.

### Trap 2 — "cache warm karke" benchmark
Agar aap loop ko 100 baar chalake time lete ho, aap **compulsory** misses ko
average out kar dete ho — real workload (jo har object ek baar dekhta) mein
woh dominant the. Kabhi cold-start bhi measure karo.

### Trap 3 — capacity miss ko "bas RAM daal do" se solve karna
Working set > L3 hai — aur RAM to already hai. Problem cache-fit ka hai. Fix
algorithm/layout se aata (tiling, smaller types), hardware se nahi.

### Trap 4 — MLP maan lena jahan dependency hai
`for (n = list.head; n; n = n->next) sum += n->val;` — aapko lagta 10 misses
overlap honge, par har `n->next` pichle load pe depend → serial → 100 ns/node.
Iska fix data structure change (flat array, lesson 08), prefetch nahi.

### Trap 5 — streaming write se cache flush
Ek 100 MB buffer ko `memset`/fill karna poore L3 ko us data se bhar dega jo
aap turant dobara nahi padhoge → aapka asli hot data evict. NT stores (lesson
12) use karo, ya woh fill hot loop se pehle.

---

## > **HFT relevance**

> - **Har miss type ka apna knob:**
>   compulsory → prefetch / smaller data;
>   capacity → shrink working set / tile / hot-cold split;
>   conflict → padding / non-power-of-2 dims;
>   coherence → `alignas(64)` / per-thread local + combine.
> - **Cold-start matters.** Market open pe pehla tick har cache line cold —
>   woh ek event slow. Warm-up loop / cache priming se hot path ki lines
>   pehle se laa lo (dummy pass over the book, the handlers).
> - **Steady-state mein capacity ka dhyaan.** Hot working set (book +
>   handlers + recent orders) < L2 (512 KiB) rakhne ki koshish — tab har
>   tick L2 se, DRAM kabhi nahi.
> - **MLP-friendly design.** Batch market-data messages, index-array se
>   lookups — taaki misses parallel chalein, serial nahi.
> - **`perf stat` har build pe.** `mem_load_retired.l3_miss` per tick track
>   karo — regression turant dikh jaayega.

---

## Hands-on

```bash
# capacity: working set sweep -- ek pointer-chase jiska size badhta jaaye
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/08_tlb_hugepages.cpp
#   64 KiB (L1/L2) ~3 ns -> 8 MiB+ (DRAM) ~95 ns : capacity cliff

# coherence: example 04 -- single-thread fine, multi-thread false sharing
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/04_false_sharing.cpp

# Linux -- miss breakdown:
perf stat -e L1-dcache-loads,L1-dcache-load-misses,l1d.replacement,\
LLC-loads,LLC-load-misses,mem_load_retired.l3_miss ./prog
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "miss ek cheez hai" | 3-4 types, alag ilaaj |
| "prefetch har miss theek karta" | sirf compulsory/latency-bound; capacity pe ulta |
| "OoO saari miss latency chhupa leta" | sirf independent misses (MLP); dependent chain nahi |
| "warm benchmark = real" | compulsory misses hide ho jaate; cold bhi dekho |
| "capacity miss = kam RAM" | working set > cache; RAM se koi lena-dena nahi |
| "high miss rate = bad code" | pehle type identify — conflict ho to ek-line padding fix |

---

## Exercises

1. Ek loop pehli pass 40 ms, doosri (same data) 8 ms, teesri 8 ms. Kaunsa
   miss dominate kar raha tha pass 1 mein? Steady-state (pass 2+) mein?

   <details><summary>Answer</summary>

   Pass 1: 40 ms mein se ~32 ms extra = **compulsory misses** (data pehli
   baar DRAM se). Pass 2+: 8 ms steady → working set cache mein fit ho gaya,
   ab sirf koi baaki misses (agar working set > cache hota to pass 2 bhi
   40 ms hoti = capacity). Yahan working set ≤ cache. Agar real workload har
   element ek hi baar dekhta hai, to woh 40 ms wala case hai — prefetch /
   smaller data se attack karo, warm-loop se nahi.
   </details>

2. `for (Node* n = head; n; n = n->next) sum += n->value;` — 1M nodes, har
   node alag malloc. Aapko lagta 10 outstanding misses (MLP) → ~10 ns/node.
   Actual ~90 ns/node. Kyun, aur fix?

   <details><summary>Answer</summary>

   `n = n->next` **dependent** hai — agle node ka address abhi wale load ke
   poore hone tak pata nahi. Zero MLP, har hop full DRAM latency serially.
   1M × 90 ns = 90 ms. Fix: nodes ko ek `std::vector<Node>` mein contiguous
   rakho (arena / pool allocator) → traversal sequential → prefetcher +
   spatial locality → ~0.5 ns/node. Ya agla-N nodes ke pointers ek array mein
   rakho aur prefetch karo (lesson 06) — par flat layout behtar. (Lesson 08.)
   </details>

3. Aapka image filter 4K frame (33 MB) pe 3 passes karta: blur-x, blur-y,
   sharpen. Har pass poora frame padhta-likhta. L3 8 MB. Miss type? Ek fix?

   <details><summary>Answer</summary>

   **Capacity** — 33 MB frame har pass mein poora L3 se bahar, har pass har
   pixel DRAM se re-load. Fix: **tiling/fusion** — frame ko L3-sized strips
   (ya L2-sized tiles) mein todo, ek tile pe teenon passes chalao (jitna
   possible — blur-y needs neighbor rows, to overlap/halo), phir agla tile.
   Ab har pixel DRAM se ek baar aata, teen passes L2/L3 mein. Ya kam-se-kam
   blur-x + blur-y ko ek pass mein fuse karo. (Lesson 15.)
   </details>

---

## Interview questions

1. The 3 C's — define each, aur har ek ka ek fix.
2. Coherence miss (4th C) — kab hota, false vs true sharing.
3. Compulsory miss ko "kam" kaise karte (avoid nahi kar sakte)?
4. Capacity miss ka structural fix (tiling) — kyun kaam karta.
5. MLP — kya, kitne outstanding misses, aur kaunse misses overlap ho sakte.
6. Pointer-chasing DRAM latency ko kyun hide nahi kar paati.
7. Profiler "20% L1 miss" bola — aapke diagnosis steps.

---

## Next
→ [`05-locality.md`](05-locality.md)
