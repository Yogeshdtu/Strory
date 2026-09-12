# 01 — Memory hierarchy: registers → L1 → L2 → L3 → DRAM → disk

## Prerequisites
- `31-CPU-ARCHITECTURE/01-how-cpu-works.md` (fetch-decode-execute-retire)
- `31-CPU-ARCHITECTURE/06-out-of-order-execution.md` (load stalls, MLP)
- `01-PROGRAMMING-BASICS/11-memory-basics.md` (folder 01 lesson 11 — woh "7x" wala)

## Yeh topic abhi kyun
Folder 01 lesson 11 mein aapne ek loop dekha tha jo sirf access-order badalne
se ~7x dheema ho gaya. Ab uska poora science. Har cheez isi ek fact pe khadi
hai: **CPU RAM se seedha kaam nahi karta.** Beech mein cache ki 3 layers hain,
aur unke latency numbers 100x tak alag hain. Jo code cache mein rehta woh tez;
jo baar-baar DRAM tak jaata woh dheema — chahe dono same instructions chalayen.

---

## Speed aur size ka trade-off

Tez memory (SRAM) mehngi aur choti hoti hai. Sasti memory (DRAM) badi aur
dheemi. Ek hi technology se 32 GB ka "sab kuch tez" banana ya to impossible hai
ya itna mehnga ki koi na khareede. Solution: **hierarchy** — thodi si bahut
tez memory CPU ke paas, phir badi-dheemi layers peeche, aur data ko upar-neeche
move karte raho taaki "abhi jo chahiye" woh sabse tez layer mein ho.

```
        size            latency (hai box, ~2 GHz Zen 2)     bandwidth
  ┌───────────────┐
  │  registers    │   ~180 (16 GP + 16 vector)   0 cycle (pipeline ka hissa)
  ├───────────────┤
  │  L1d cache    │   32 KiB / core     ~4-5 cyc   ~2-3 ns      ~2 loads/cyc
  ├───────────────┤
  │  L2 cache     │   512 KiB / core    ~12-14 cyc ~6-7 ns
  ├───────────────┤
  │  L3 cache     │   8 MiB / 4-core CCX ~40 cyc   ~20 ns       shared
  ├───────────────┤
  │  DRAM (DDR4)  │   16 GB            ~150-200 cyc ~80-100 ns   ~40 GB/s (2ch)
  ├───────────────┤
  │  NVMe SSD     │   512 GB           ~10-100 us  (10,000-100,000 ns)
  ├───────────────┤
  │  spinning HDD │   TB               ~5-10 ms   (5,000,000+ ns)
  └───────────────┘
```

> Numbers **is box ke** (example `08` ne L1 ~1.2 ns, DRAM ~95 ns napa; example
> `02` ne DRAM streaming ~14 GB/s, random ~2 GB/s napa). Production HFT box
> 3-5 GHz pe absolutes chhote honge — **ratios port karte hain, absolutes nahi**
> (`31-CPU-ARCHITECTURE/15`).

---

## Woh "1 second" wali analogy

Agar L1 hit = **1 second** maan lo, to baaki cheezein human scale pe:

| Layer | Real latency | Agar L1 = 1 sec |
|---|---|---|
| L1 | ~1 ns | 1 second |
| L2 | ~4 ns | 4 seconds |
| L3 | ~20 ns | 20 seconds |
| DRAM | ~100 ns | **1.5 minutes** |
| NVMe SSD | ~50 µs | **14 hours** |
| network (same DC) | ~100 µs | **1.2 days** |
| HDD seek | ~8 ms | **3 months** |

L1 se DRAM tak jaana = "kaam pe baithe the, ab 1.5 minute chai peene chale
gaye." Har DRAM miss aisa hi break hai — CPU ka kaam wahin ruk gaya (ya OoO
engine ne koi independent kaam pakda to woh chala, `31/06`).

---

## Yeh sab "transparent" hai — aur wahi problem hai

C++ mein aap `arr[i]` likhte ho. Aapko nahi pata woh L1 se aaya ya DRAM se —
syntax same, result same, sirf **time alag** (20x–100x). Compiler bhi mostly
nahi jaanta. CPU khud manage karta hai: har load pe check karta L1 mein hai?
nahi → L2? nahi → L3? nahi → DRAM. Jo line aati hai woh saari upar ki layers
mein daal di jaati (taaki agli baar paas ho).

**Isliye performance "invisible" hoti hai** jab tak aap measure na karo. Do
functions same big-O, same instruction count — ek 10x dheema kyunki uska
access pattern cache ke against hai. Yeh poora folder us pattern ko dekhna,
naapna aur theek karna sikhata hai.

---

## Inclusive vs exclusive (chhota sa detail)

- **Inclusive** (kai Intel): L3 mein har woh line hoti hai jo L2/L1 mein hai.
  L3 se evict → L1/L2 se bhi invalidate. Snooping asaan (bas L3 dekho).
- **Exclusive** (kai AMD): ek line ek hi jagah. Effective capacity = L2 + L3.
- **NINE** (non-inclusive non-exclusive): beech ka.

Zyada roz-marra fark nahi padta, par "L3 8 MiB" ka matlab AMD pe thoda zyada
usable data hota. `31-CPU-ARCHITECTURE/15` mein per-CCX L3 ki baat hai —
2 CCX wale chip pe ek core ko sirf apne CCX ka 8 MiB dikhta, doosre CCX ka L3
"remote" (dheema) hota.

---

## Ek line ka safar (load `arr[i]`)

```
1. CPU: virtual address VA = &arr[i]  (base + i*size)
2. TLB lookup: VA -> PA (physical address)   [lesson 11]
      hit  -> aage
      miss -> page walk (~4 dependent memory accesses)
3. L1d: PA ke index/tag se set dekho  [lesson 03]
      hit  -> ~4 cyc mein data register mein. KHATAM.
      miss -> L2 request
4. L2: ~12 cyc. hit -> line L1 mein bhi daalo, data do.
       miss -> L3 request
5. L3: ~40 cyc. hit -> line L2+L1 mein daalo, data do.
       miss -> DRAM request (memory controller)
6. DRAM: row activate + column read, ~100+ ns. Poori 64-B line aati,
         L3+L2+L1 mein daali jaati, data register mein.
```

Har step pe **64 bytes** (ek cache line, lesson 02) move hota — chahe aapne
1 byte maanga ho. Isliye "kitna data chhua" nahi, "**kitni alag lines chhui**"
maayne rakhta.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — big-O se performance predict karna
`O(n)` linked-list traversal aur `O(n)` vector traversal dono "linear" hain —
par list har node pe ek cache miss (~100 ns), vector ~1 miss / 16 elements.
10-50x fark, same big-O. (Lesson 08, example — folder 20 bhi.)

### Trap 2 — "RAM random access hai, sab addresses barabar"
Naam "Random Access Memory" hai par **cost random nahi**. Same-row access saste
(row buffer hit), row change mehnga. Aur cache ki wajah se recently/nearby
touched addresses 100x saste. Access pattern sab kuch hai.

### Trap 3 — registers ko bhool jaana
Sabse tez "memory" registers hain (~180 total). Ek hot variable jo register
mein reh sakta tha par aapne use struct field/pointer-deref bana diya → har
use pe ek load. `31-CPU-ARCHITECTURE/02` (spilling).

### Trap 4 — disk ko "bas thodi dheemi RAM" samajhna
SSD RAM se ~1000x, HDD ~50,000x dheema. mmap'd file ka page fault = disk read
= aapka thread ~50 µs–10 ms ke liye soya. HFT mein hot path kabhi disk touch
nahi karta (lesson `29/17` mlockall).

### Trap 5 — latency aur bandwidth ko ek samajhna
DRAM latency ~100 ns (ek line aane mein), par bandwidth ~40 GB/s (agar aap
kai lines ek saath maango). Ek dependent chain latency-bound (~100 ns/line);
ek streaming loop bandwidth-bound (~1.6 ns/line). Lesson 13.

---

## > **HFT relevance**

> - **Hot working set ko L1/L2 mein rakho.** Order book ka woh hissa jo har
>   tick pe chhua jaata (top N levels, recent orders) — use chhota aur
>   contiguous rakho taaki 32 KiB L1 / 512 KiB L2 mein fit ho. Ek DRAM miss
>   = ~100 ns = poore tick-to-trade budget ka bada hissa.
> - **Latency budget ko hierarchy ke terms mein socho.** "Tick-to-trade 1 µs"
>   ka matlab: ~10 DRAM misses ka room, ya ~500 L1 hits. Har accidental miss
>   budget kha jaata.
> - **p99 jitter aksar cache/TLB miss hota.** Median fast, par kabhi woh line
>   evict ho gayi (koi aur thread, ya interrupt) → us ek event pe +100 ns.
>   Isliye pinning, huge pages, prefault (lesson 11, `29/16-17`).
> - **Disk/network ko hot path se poori tarah hatao.** Logging async,
>   config pre-loaded, `mlockall` se swap band.

---

## Hands-on

```bash
# is box ki hierarchy khud dekho (CPUID leaf 4):
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/07_cpu_info.cpp

# latency ladder measure karo (pointer-chase, working set sweep):
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/08_tlb_hugepages.cpp
# curve B: 64 KiB pe ~3 ns (L1/L2), 8 MiB+ pe ~95 ns (DRAM) -- yahi ladder hai

# streaming vs random bandwidth:
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/02_stride_access.cpp

# Linux pe: lscpu, aur
#   perf stat -e L1-dcache-load-misses,LLC-load-misses,cycles,instructions ./prog
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "CPU RAM se compute karta hai" | CPU registers se; RAM 3 cache layers peeche |
| "same big-O = same speed" | access pattern se 10-100x fark, same big-O |
| "L1/L2/L3 me farak thoda hai" | ~1 / ~4 / ~20 / ~100 ns — 100x range |
| "cache manually manage karna padta" | HW auto; aap sirf **access pattern** control karte |
| "bada L3 matlab sab fit" | 8 MiB — ek 4096² int matrix (64 MiB) 8x bahar |
| "SSD ~ dheemi RAM" | ~1000x dheema; page fault = thread stall |

---

## Exercises

1. L1 = 1 ns, DRAM = 100 ns. Ek loop 1M iterations, har iter 1 memory access.
   (a) sab L1 hits: kitna time? (b) sab DRAM misses: kitna? (c) 5% miss rate:
   kitna, aur miss ka contribution %?

   <details><summary>Answer</summary>

   (a) 1M × 1 ns = **1 ms**.
   (b) 1M × 100 ns = **100 ms**.
   (c) 950k × 1 ns + 50k × 100 ns = 0.95 ms + 5 ms = **5.95 ms**. Misses
   5 ms / 5.95 ms = **~84%** of the time, sirf 5% accesses hone ke bawajood.
   Yahi cache optimization ka pura point hai — miss rate 5% → 1% karo aur
   5.95 ms → ~2 ms.
   </details>

2. Ek `std::vector<int>` (4 MiB) aur ek `std::list<int>` (same 1M ints) —
   dono ko sum karo. Kaunsa tez aur kitna, aur **kyun** (cache terms mein)?

   <details><summary>Answer</summary>

   Vector kaafi tez (is box pe ~10-30x). Vector: contiguous, har 64-B line
   16 ints, ~1 miss / 16, + prefetcher aage ki lines laata → effective
   ~0.2-0.3 ns/int. List: har node alag heap allocation, `next` pointer
   follow karna = **dependent** load, agla address tab tak pata nahi jab
   tak yeh load poora na ho → prefetcher andha → har node ~L3/DRAM miss
   (~20-100 ns/node). Plus har node mein `int` (4B) + `next` (8B) + malloc
   overhead (~16B) = ~28B useful-to-junk. (Lesson 08.)
   </details>

3. Aapke paas 3 GHz CPU hai, tick-to-trade budget 800 ns. DRAM miss ~90 ns.
   Hot path mein kitne "afford-able" DRAM misses hain? Aur agar aapka code
   abhi 15 misses/tick karta hai to?

   <details><summary>Answer</summary>

   800 ns / 90 ns ≈ **8-9 DRAM misses** ka poora budget (agar aur kuch na
   karein — jo galat hai, compute bhi chahiye). Realistically 2-4 misses ka
   room. 15 misses/tick = ~1350 ns sirf memory stalls mein = budget se pehle
   hi bahar. Fix: working set shrink (smaller types, hot/cold split), SoA,
   flat structures, prefault + huge pages, aur profile karke woh 15 → 3 laana.
   </details>

---

## Interview questions

1. Memory hierarchy ki layers, unke approximate latencies (cycles aur ns).
2. Cache "transparent" kaise hai aur yeh performance analysis kyun mushkil banata.
3. Ek `load` instruction ka poora path (TLB → L1 → L2 → L3 → DRAM).
4. Latency vs bandwidth — DRAM ke liye dono numbers, aur kab kaunsa matter karta.
5. Inclusive vs exclusive cache — fark aur ek practical implication.
6. "Same big-O, 10x speed difference" — ek concrete example cache terms mein.
7. HFT hot path ko design karte waqt hierarchy se kya 3 rules nikalte ho.

---

## Next
→ [`02-cache-lines.md`](02-cache-lines.md)
