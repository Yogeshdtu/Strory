# 08 — Allocation cost (naapa hua) — HFT ka core problem

## Prerequisites
- [`03-heap-deep-dive.md`](03-heap-deep-dive.md)
- Folder 06 file 08 (branch prediction — tail/jitter idea), folder 07 (benchmark hygiene)

## Yeh topic abhi kyun
"Allocation slow hoti hai" har koi kehta hai. Kitni? Aur — zyada important —
**kitni consistent?** HFT mein average nahi, **tail (p99, p99.9, max)** matter
karta hai. Yeh lesson allocation ko naapta hai, tail dikhata hai, aur wahi wajah
hai ki hot path pe `new` nahi hota.

---

## Do numbers: average aur tail

**Average latency jhoot bolti hai.** Ek allocation jo 99% baar 40 ns leti hai
par 1% baar 20 µs — uska average ~240 ns dikhega, "theek" lagega. Par woh 1%
(p99) aapko har 100 events pe ek baar 20 µs pichhe kar deta — ek tick miss,
ek quote late.

Analogy: ek dukaan jahan 99 customer 1 minute mein nikalte hain, par har 100th customer ke waqt
dukaandaar godown jaata hai aur 30 minute lagte hain. "Average 1.3 minute" sunne mein theek hai — par
jo 100th customer hai, uska din kharab.

```
   latency distribution (allocation):

   count
    │        ▁▁███▁▁                              <- p50 yahan (fast path: allocator ka cache hit)
    │      ▁▁███████▁▁
    │    ▁▁███████████▁▁▁
    │ ▁▁▁████████████████▁▁▁▁▁▁▁▁▁ ...  ▁      ▁  <- lambi tail: lock, syscall, page fault
    └────┴──────────────┴─────────────────┴──────► latency
        30ns           100ns              100µs+
```

---

## Naapa hua — is repo pe

⚠️ **Yeh numbers kis allocator ke hain?** Is course ka toolchain MinGW-w64 **UCRT** hai. Binary ke imports
dekho (`objdump -x file.exe | grep "DLL Name"`): `malloc`/`free` `api-ms-win-crt-heap-l1-1-0.dll` se aate
hain — yaani **Windows UCRT ka heap**, glibc ka `malloc` nahi. libstdc++ ka `operator new` usi `malloc` ko
bulata hai. Neeche ke numbers usi allocator ke hain. Production HFT aam taur pe Linux + glibc (ya
jemalloc/mimalloc) pe chalta hai — wahan absolute numbers alag honge, **shape** (chhota median, lambi tail)
wahi rahega. File 03 ka tcache/bins walkthrough glibc ka hai.

GCC 16.2, `-O2`, Windows x64, AMD Zen 2:

### Stack vs heap ([`examples/02_stack_vs_heap.cpp`](examples/02_stack_vs_heap.cpp))

```
block = 256 bytes, 2,000,000 iterations (3 runs)
stack  per iter : 0.54 – 0.88 ns   (loop mein allocation ka koi instruction hi nahi)
heap   per iter : 36.1 – 37.2 ns   (new[] + delete[], fast path, single-thread)
ratio           : ~41 – 68x        (stack wala number itna chhota hai ki ratio jitter karta hai)
```

**Stack pe asal mein kya hua (assembly dekha):** `int buf[64]` ke liye loop ke andar `sub rsp` bhi **nahi**
hai — compiler ne poore function ka frame entry pe ek hi baar bana diya. Har iteration sirf do stores + do
loads. Stack "allocation" per-iteration cost zero hai; jo 0.5–0.9 ns dikh raha woh bas loop ka kaam hai.
Heap loop wahi kaam karta hai **plus** `call _Znay` / `call _ZdaPv`.

### Allocation latency distribution ([`examples/06_allocation_benchmark.cpp`](examples/06_allocation_benchmark.cpp), `rdtsc`, ns) — 2 runs

```
                       mean    p50    p90    p99   p99.9              max
A) new+delete (reuse)  ~34     30     40     90    110–120     7,500 – 25,000     (run-variable)
B) new, retained       63–67   30     40    100    410–430    67,000 – 109,000    (run-variable)
C) fixed buffer        ~9.5    10     10     10     20         20 ya 32,500 (lone outlier)
```

Padhne ka tareeka:
- **C** kuch allocate nahi karta — uska 10–20 ns bas do `rdtsc` ka apna cost hai (measurement floor).
  Isliye A/B ke p50 (30 ns) mein se ~10 ns measurement hai.
- **p50 chhota** (30 ns) — allocator ka fast path (freed block ready mila) kaam kar raha.
- **p99.9 / max bahut bada** — kabhi free-list miss, kabhi heap lock, kabhi OS se nayi memory
  (`VirtualAlloc` Windows pe; `brk`/`mmap` Linux pe), kabhi first-touch page fault. Scenario B (blocks
  retain) mein allocator ko sach mein grow karna padta → max 100 µs tak.
- **A ka max / p50 ≈ 250–800x.** Wahi tail HFT ko maarti hai.
- C ka kabhi-kabhi aane wala 32 µs max — allocation nahi, measuring thread ko OS ne preempt kiya.
  Measurement ki bhi apni tail hoti hai.

### Pool ([`examples/07_simple_pool.cpp`](examples/07_simple_pool.cpp)) — 64 orders ka burst

```
pool  alloc+free : 1.70 – 1.85 ns/pair   (placement new + read samet)
new   alloc+free : 38.1 – 39.0 ns/pair
speedup          : ~21 – 22x             (aur -- tail flat, file 10)
```

⚠️ **Is example ka purana number "~190x" galat tha.** Pehle benchmark har iteration mein *wahi* slot
allocate karke turant free karta tha. `bench` pool ek local tha jiska address kahin nahi gaya, isliye GCC ne
`free_` ko register mein rakh ke poora allocate+deallocate **ek `mov`** bana diya (assembly mein dekha) —
"0.24 ns/op" ek khaali loop tha, `new`/`delete` ke asli calls ke saamne. Ab 64 ka burst allocate → construct
→ padho → free: free list sach mein chalti hai. Sabak: speedup "bahut accha" lage to assembly dekho.

> Numbers machine/allocator/load pe depend. **Magnitude** aur **shape** reproduce honge: stack ~0,
> pool ~1–2 ns, `new` fast path ~tens of ns, `new` tail µs se upar.

---

## Tail ke sources (kya `new` ko slow karta)

| Source | Kab | Cost (typical) |
|---|---|---|
| Allocator cache **hit** (glibc: tcache/fast-bin; Windows: LFH bucket) | us size ka freed block ready | ~10–40 ns |
| Free-list / bin **search** | cache miss | ~50–200 ns |
| **Heap lock** contention | multi-thread, ek heap/arena pe race | µs (blocked) |
| Heap extend syscall (`brk` / `VirtualAlloc` commit) | allocator ki memory khatam | ~µs |
| Bada region (`mmap` / `VirtualAlloc` reserve+commit) | bada alloc, ya naya region | µs–10s µs |
| **first-touch page fault** | naya page pehli baar likha | allocation ke **baad**, per page |
| **coalescing** on `free` | adjacent free blocks merge | chhota, par variable |
| TLB miss / cache miss on metadata | thanda allocator state | 10s–100s ns |

Aur bhi: `free` ki cost allocation ke number mein nahi dikhti par woh bhi variable hai.

---

## Isiliye: hot path pe zero allocation

HFT hot path ka niyam: **market data aane se order jaane tak, ek bhi `new` / `malloc` nahi.** Kaise:

1. **Pre-allocate** — startup pe saari zaroori memory le lo aur **touch karo** (page-fault abhi, hot path pe
   nahi).
   ```cpp
   orders_.reserve(MAX_ORDERS);          // ek allocation, startup pe
   for (auto& o : orders_) o = {};       // first-touch abhi
   ```
2. **Object pools** (file 10, `07_simple_pool.cpp`) — fixed-size blocks, O(1) free-list, koi syscall nahi.
   Objects "pool mein wapas" jaate hain, free hote hi nahi.
3. **Arenas / bump allocators** — ek bade block se seedha kaatte jao; reset = pointer wapas. Per-event
   scratch ke liye ideal.
4. **Ring buffers** — fixed-size, wrap-around; message queues, event logs.
5. **Stack / `std::array`** — jitna fit ho.
6. **`std::pmr`** (folder 22) — standard containers ko custom `memory_resource` (pool/monotonic) de do bina
   container type badle.

Non-hot paths (startup, config reload, logging thread) pe `new` bilkul theek — wahan latency budget hai.

---

## Andar kya hota hai

- **`std::vector::push_back`** amortized O(1) — par jis push pe capacity full ho, woh ek `new` (2x size) +
  purane elements ki copy/move + `delete` karta. 1e6 `push_back` bina `reserve` = **21 allocations** (GCC 16.2
  pe gine). Isliye hot path pe `reserve()`.
- **`std::string`** chhoti (≤15 char, libstdc++) → SSO, allocation nahi; lambi → heap (folder 10).
- **`std::make_shared`** ek allocation (control block + object saath) — `shared_ptr(new T)` do allocations.
  Par refcount atomics ki apni cost.
- **`new` elision** (`-O2`, C++14) — agar allocation ka koi asar bahar nahi dikhta, compiler `new`+`delete`
  poora hata sakta hai (folder 10, file 04). Is lesson ka exercise 5 pehli koshish mein isi mein phansa:
  64 MB `new` + `memset` + `delete` → "0 µs, 0 ms" — sab gayab.
- **Huge pages / `madvise`** bade pre-allocated arenas ke first-touch aur TLB cost ko kam karte (folder 29, 32).

> **HFT relevance:** yeh poore folder ka "kyun". `new` ki median cost bhi (~30 ns yahan) ek order ke total
> budget (aksar sub-µs wire-to-wire) ka achha-khaasa hissa hai — aur median matter nahi karta, p99.9 aur max
> (100s ns se 100 µs tak, upar dekha) karta hai. Isliye allocation ko **design se** hot path se hataya jaata
> hai, "optimize" karke nahi. Jab profiling hot path pe `operator new` dikhaye → woh ek bug hai, tune karne ki
> cheez nahi.

---

## Hands-on — performance engineering loop (CLAUDE.md §12)

```bash
# 1. simple
./build.ps1 fast 14-MEMORY/examples/02_stack_vs_heap.cpp        # heap ~37 ns vs stack ~0
# 2. distribution naapo
./build.ps1 fast 14-MEMORY/examples/06_allocation_benchmark.cpp  # p50 vs p99.9 vs max
# 3. optimize: pool
./build.ps1 fast 14-MEMORY/examples/07_simple_pool.cpp           # ~21x + flat tail
# 4. explain: kya badla? -> allocator hataya, O(1) free-list pop/push
# 5. verify: speedup ka assembly dekho -- khaali loop to nahi naap rahe?
g++ -std=c++20 -O2 -S -masm=intel 14-MEMORY/examples/07_simple_pool.cpp -o pool.s
```

Experiment: `06` ke scenario B ka `REPS` badhao (500k, 1M) — max badhta hai kyunki allocator zyada baar OS se
maangta. Scenario C ka max variance — CPU affinity (`taskset` / `SetThreadAffinityMask`) se kam ho sakta.

---

## ⚠️ Traps

### Trap 1 — average latency pe optimize karna
p50 30 ns "fine" — par p99.9 100s ns aur max 100 µs har kuch hazaar events pe ek tick tod deta.

### Trap 2 — benchmark jo sirf fast path hit kare
`new`+`delete` ek hi size, single thread, tight loop → allocator ka cache hamesha hit → tail nahi dikhta. Asli
load (multi-thread, mixed sizes, retention) alag.

### Trap 3 — `-O0` pe allocation benchmark
`-O0` inlining/elision off, sab dheema → numbers bekaar. `07_simple_pool.cpp` `-O0` pe pool vs new sirf ~5x
dikhata hai, `-O2` pe ~21x. Hamesha `-O2`.

### Trap 4 — `reserve` bhool ke hot loop mein `push_back`
```cpp
for (auto& e : events) out.push_back(f(e));   // ⚠️ kai reallocs. out.reserve(events.size())
```

### Trap 5 — pool ko "sirf throughput" ke liye samajhna
Asli jeet **tail latency** (deterministic, syscall-free, lock-free) hai — ~21x throughput bonus hai.

### Trap 6 — benchmark jise compiler ne khaali kar diya
Same slot alloc/free (pool) → ek `mov`. Unused `new`+`memset`+`delete` → poora elide. "Bahut tez" number aaye
to **assembly dekho**, ya kaam ko `[[gnu::noipa]]` function ke peeche rakho.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Allocation ~constant ~50 ns" | Fast path haan (~30 ns yahan); tail µs tak (lock/syscall/fault) |
| "Average latency batata hai kitna slow" | HFT mein p99/p99.9/max matter — average chhupata hai |
| "Hot path pe `new` optimize kar lenge" | Design se hatao — tune nahi hota |
| "`vector::push_back` O(1) hai to loop safe" | Realloc wale push pe `new`+copy — `reserve` karo |
| "Pool bas fast hai" | + deterministic + no syscall + no lock — tail flat |
| "Stack allocation = har iteration `sub rsp`" | Frame function entry pe ek baar; loop mein koi instruction nahi |
| "MinGW pe bhi glibc ka tcache chal raha hai" | Yahan UCRT/Windows heap hai — tcache glibc ka hai |

---

## Exercises

1. **Distribution padho:** [`examples/06_allocation_benchmark.cpp`](examples/06_allocation_benchmark.cpp) chalao.
   A ka p50 aur max note karo — ratio? Woh ratio HFT ke liye kyun problem hai (ek thos scenario)?

   <details><summary>Answer (is machine pe)</summary>

   p50 30 ns vs max 7,500–25,000 ns → ~250–800x. Scenario B mein max 67–109 µs. Har kuch hazaar allocations pe
   ek bada spike — agar woh ek order ke path pe pade, order µs late → market move miss, ya stale quote hit.
   </details>

2. **reserve ka asar:** `std::vector<int> v;` mein 1e6 `push_back` — bina `reserve` vs `v.reserve(1e6)` ke saath.
   `-O2` time + `operator new` count (override karke). Bina reserve kitne reallocs?

   <details><summary>Answer (GCC 16.2, gin ke)</summary>

   Bina reserve: **21 allocations** (capacity 1, 2, 4, ... 1,048,576) + har grow pe saare elements ki copy.
   Reserve ke saath: **1** allocation, koi copy nahi.
   </details>

3. **Multi-thread tail:** `06` scenario A ko 4 threads se ek saath chalao (har thread apna `new`/`delete`
   loop). Single-thread p99.9 se milao — badha? Kyun?

   <details><summary>Answer</summary>

   Aam taur pe badhta hai — shared heap/arena ka lock contention. glibc mein arenas threads se kam ho sakte hain;
   Windows process heap mein bhi ek heap sab threads share karte hain. jemalloc/mimalloc (per-thread caches) pe
   kam. Yeh multi-thread allocator scaling ka core issue hai. Apni machine pe naap ke number likho.
   </details>

4. **Pool vs new tail:** `07_simple_pool.cpp` ko `06` jaisa distribution naapne ke liye badlo (har pool alloc/free
   pe rdtsc). p99.9 / max — flat?

   <details><summary>Answer</summary>

   Pool ka p50 ≈ p99.9 ≈ measurement floor (scenario C jaisa, 10–20 ns — rdtsc ka apna cost; pool ka asli kaam
   ~1–2 ns). Slow path hai hi nahi (koi branch to syscall/lock nahi). Max sirf measuring noise (preemption). Yeh
   determinism hi asli value hai.
   </details>

5. **first-touch:** `char* p = new char[64<<20];` (64 MB) — allocation ka time vs uske baad `memset(p, 1, 64<<20)`
   ka time vs doosre `memset` ka time. Kaunsa bada, kyun? ⚠️ `new` aur `memset` ko `[[gnu::noipa]]` helpers mein
   rakho, warna `-O2` sab elide kar dega.

   <details><summary>Answer (Windows, GCC 16.2, 2 runs × 3)</summary>

   `new`: **9–42 µs**. Pehla `memset`: **6.5–11.8 ms**. Doosra `memset` (wahi memory): **2.3–4.7 ms**. Allocation
   sirf virtual memory reserve/commit karta hai; physical pages pehli baar likhne pe milte hain (16,384 × 4 KB
   page faults). Pehle aur doosre memset ka ~4–7 ms farq = first-touch ki cost — jo "allocation" ke number mein
   dikhti hi nahi. Isliye pre-allocate ke saath **pre-touch**.
   </details>

---

## Interview questions

1. Allocation ki fast path vs slow path — cost ka order of magnitude?
2. HFT mein average latency kyun bekaar, p99.9 kyun matter?
3. Allocation tail ke 4 sources batao.
4. Hot path pe zero-allocation kaise karte hain (4 techniques)?
5. `std::vector::push_back` "O(1)" — hot path pe kya dhyan?
6. first-touch page fault — allocation number mein kyun nahi, phir bhi matter kyun karta?
7. Benchmark mein "bahut bada speedup" aaye to pehle kya check karoge?

---

## Next
→ [`09-fragmentation.md`](09-fragmentation.md)
