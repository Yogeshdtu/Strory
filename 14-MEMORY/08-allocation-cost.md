# 08 — Allocation cost (measured) — HFT ka core problem

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

```
   latency distribution (allocation):

   count
    │        ▁▁███▁▁                              <- p50 yahan (fast path, tcache hit)
    │      ▁▁███████▁▁
    │    ▁▁███████████▁▁▁
    │ ▁▁▁████████████████▁▁▁▁▁▁▁▁▁ ...  ▁      ▁  <- lambi tail: lock, syscall, page fault
    └────┴──────────────┴─────────────────┴──────► latency
        40ns           200ns              20µs+
```

---

## Measured — is repo pe

### Stack vs heap ([`examples/02_stack_vs_heap.cpp`](examples/02_stack_vs_heap.cpp), `-O2`)

```
block = 256 bytes, 2,000,000 iterations
stack  per iter : ~0.85 ns      (sub rsp, 256 -- ek instruction)
heap   per iter : ~71   ns      (new[] + delete[], fast path, single-thread)
ratio           : ~83x
```

### Allocation latency distribution ([`examples/06_allocation_benchmark.cpp`](examples/06_allocation_benchmark.cpp), `rdtsc`, ns) — sample run

```
                     mean    p50    p90    p99   p99.9              max
A) new+delete (reuse)  ~50    ~45    ~55    ~60   ~120     ~45,000 - ~105,000    (run-variable)
B) new, retained       ~90    ~50    ~55   ~130   ~450    ~120,000 - ~1,200,000  (run-variable)
C) fixed buffer        ~10    ~10    ~10    ~40    ~40             (measuring noise)
```

Padhne ka tareeka:
- **p50 chhota** (~50 ns) — fast path (per-thread cache hit) kaam kar raha.
- **p99.9 / max bahut bada** — kabhi bin search, kabhi arena lock, kabhi
  `brk`/`mmap` syscall, kabhi first-touch page fault. Scenario B (blocks retain)
  mein allocator ko sach mein grow karna padta → tail milliseconds tak.
- **C flat** — pointer move, koi allocator nahi. (Lone max outlier = measuring
  thread ko OS ne preempt kiya, allocation nahi.)

### Pool ([`examples/07_simple_pool.cpp`](examples/07_simple_pool.cpp), `-O2`)

```
pool  alloc+free : ~0.43 ns/op
new   alloc+free : ~83   ns/op
speedup          : ~190x   (aur -- tail poori tarah flat)
```

> Numbers machine/allocator/load pe depend. **Magnitude** aur **shape**
> reproduce honge: stack ~0, pool ~0, `new` fast-path ~tens of ns, `new` tail
> µs–ms.

---

## Tail ke sources (kya `new` ko slow karta)

| Source | Kab | Cost |
|---|---|---|
| tcache/fast-bin **hit** | us size ka freed block ready | ~10-40 ns |
| bin **search** / unsorted bin | fast bin miss | ~50-200 ns |
| **arena lock** contention | multi-thread, ek arena pe race | µs (blocked) |
| **`brk`** heap extend | top chunk khatam | ~1-5 µs (syscall) |
| **`mmap`** (bada alloc / new region) | > mmap threshold, ya arena grow | ~5-50 µs (syscall) |
| **first-touch page fault** | naya page pehli baar likha | ~1-10 µs/page (allocation ke **baad**) |
| **coalescing** on `free` | adjacent free chunks merge | small, par variable |
| TLB miss / cache miss on metadata | cold allocator state | 10s-100s ns |

Aur bhi: `free` ki cost allocation ke number mein nahi dikhti par woh bhi
variable hai.

---

## Isiliye: hot path pe zero allocation

HFT hot path ka rule: **market data aane se order jaane tak, ek bhi `new` /
`malloc` nahi.** Kaise:

1. **Pre-allocate** — startup pe saari zaroori memory le lo aur **touch karo**
   (page-fault ab, hot path pe nahi).
   ```cpp
   orders_.reserve(MAX_ORDERS);          // ek allocation, startup pe
   for (auto& o : orders_) o = {};       // first-touch ab
   ```
2. **Object pools** (file 10, `07_simple_pool.cpp`) — fixed-size blocks, O(1)
   free-list, koi syscall. Objects "return to pool", free hote hi nahi.
3. **Arenas / bump allocators** — ek bade block se linear cut; reset = pointer
   wapas. Per-event scratch ke liye ideal.
4. **Ring buffers** — fixed-size, wrap-around; message queues, event logs.
5. **Stack / `std::array`** — jitna fit ho.
6. **`std::pmr`** (folder 22) — standard containers ko custom
   `memory_resource` (pool/monotonic) de do bina container type badle.

Non-hot paths (startup, config reload, logging thread) pe `new` bilkul theek —
wahan latency budget hai.

---

## Andar kya hota hai

- **`std::vector::push_back`** amortized O(1) — par jis push pe capacity full ho,
  woh ek `new` (2x size) + `memcpy(old)` + `delete` karta. Isliye hot path pe
  `reserve()`.
- **`std::string`** short (≤15 char libstdc++) → SSO, no alloc; lambi → heap
  (folder 10).
- **`std::make_shared`** ek allocation (control block + object saath) —
  `shared_ptr(new T)` do allocations. Par refcount atomics ki apni cost.
- **`new` elision** (`-O2`, C++14) short-lived allocations ko compiler hata
  sakta — benchmark mein barrier chahiye (folder 10, file 04).
- **Huge pages / `madvise`** bade pre-allocated arenas ke first-touch aur TLB
  cost ko kam karte (folder 34).

> **HFT relevance:** yeh poore folder ka "kyun". `new` ki median cost bhi (~50
> ns) ek order ke total budget (aksar sub-µs wire-to-wire) ka bada hissa hai —
> aur median matter nahi karta, p99.9 (µs–ms) karta hai. Isliye allocation ko
> **design se** hot path se hataya jaata hai, na ki "optimize" karke. Jab
> profiling hot path pe `operator new` dikhaye → woh ek bug hai, tune karne ki
> cheez nahi.

---

## Hands-on — performance engineering loop (CLAUDE.md §12)

```bash
# 1. simple
./build.ps1 fast 14-MEMORY/examples/02_stack_vs_heap.cpp        # heap vs stack ~83x
# 2. measure distribution
./build.ps1 fast 14-MEMORY/examples/06_allocation_benchmark.cpp  # p50 vs p99.9 vs max
# 3. optimize: pool
./build.ps1 fast 14-MEMORY/examples/07_simple_pool.cpp           # ~190x + flat tail
# 4. explain: kya badla? -> allocator hataya, O(1) pointer swap
```

Experiment: `06` ka scenario B ka `REPS` badhao (500k, 1M) — max badhta hai
kyunki allocator zyada baar OS se maangta. Scenario C ka max variance —
CPU affinity (`taskset` / `SetThreadAffinityMask`) se kam ho sakta.

---

## ⚠️ Traps

### Trap 1 — average latency pe optimize karna
p50 40 ns "fine" — par p99.9 5 µs har 1000 events pe ek tick tod deta.

### Trap 2 — benchmark jo sirf fast path hit kare
`new`+`delete` ek hi size, single thread, tight loop → tcache hamesha hit →
tail nahi dikhta. Real load (multi-thread, mixed sizes, retention) alag.

### Trap 3 — `-O0` pe allocation benchmark
`-O0` inlining/elision off, sab dheema → numbers meaningless. Hamesha `-O2`.

### Trap 4 — `reserve` bhool ke hot loop mein `push_back`
```cpp
for (auto& e : events) out.push_back(f(e));   // ⚠️ kai reallocs. out.reserve(events.size())
```

### Trap 5 — pool ko "sirf throughput" ke liye samajhna
Asli jeet **tail latency** (deterministic, syscall-free, lock-free) hai —
throughput ~190x bonus.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Allocation ~constant ~50 ns" | Fast path haan; tail µs–ms (lock/syscall/fault) |
| "Average latency batata hai kitna slow" | HFT mein p99/p99.9/max matter — average chhupata hai |
| "Hot path pe `new` optimize kar lenge" | Design se hatao — tune nahi hota |
| "`vector::push_back` O(1) hai to loop safe" | Realloc push pe `new`+copy — `reserve` karo |
| "Pool bas fast hai" | + deterministic + no syscall + no lock — tail flat |

---

## Exercises

1. **Read the distribution:** [`examples/06_allocation_benchmark.cpp`](examples/06_allocation_benchmark.cpp)
   run karo. A ka p50 aur max note karo — ratio? Woh ratio HFT ke liye kyun
   problem hai (ek concrete scenario)?

   <details><summary>Answer</summary>

   ~50 ns vs ~100,000 ns → ~2000x. Har ~1000-10000 allocations pe ek 100 µs
   spike — agar woh spike ek order ke path pe pade, order 100 µs late → market
   move miss, ya stale quote hit ho jaaye.
   </details>

2. **reserve impact:** `std::vector<int> v;` mein 1e6 `push_back` — bina
   `reserve` vs `v.reserve(1e6)` ke saath. `-O2` time + `operator new` count
   (override). Kitne reallocs bina reserve?

   <details><summary>Answer</summary>

   Bina reserve: ~20 reallocs (2x growth: 1,2,4,...,~1M) → ~20 `operator new` +
   ~20 `delete` + total ~2M int copies. Reserve: 1 `operator new`, 0 copies.
   Measurable time farq.
   </details>

3. **Multi-thread tail:** `06` scenario A ko 4 threads se ek saath chalao (har
   thread apni `new`/`delete` loop). Single-thread p99.9 se compare — badha?
   Kyun?

   <details><summary>Answer</summary>

   Badhta hai — arena lock contention (glibc default arenas < threads, ya same
   arena share). jemalloc/mimalloc (per-thread arenas) pe kam. Yeh MT allocator
   scaling ka core issue.
   </details>

4. **Pool vs new tail:** `07_simple_pool.cpp` ko `06` jaisa distribution-measure
   karne ke liye extend karo (rdtsc per pool alloc/free). p99.9 / max — flat?

   <details><summary>Answer</summary>

   Pool p50 ≈ p99.9 ≈ ~0-5 ns (ek pointer swap, no branch to slow path, no
   syscall). Max sirf measuring noise. Yeh determinism hi asli value hai.
   </details>

5. **first-touch:** `char* p = new char[64<<20];` (64 MB) — allocation ka time
   vs uske baad `memset(p, 1, 64<<20)` ka time. Kaunsa bada, kyun?

   <details><summary>Answer</summary>

   Allocation chhota (`mmap` — sirf virtual reservation). `memset` bada — 16384
   page faults (4 KB pages), har ek OS mein jaake physical page map karta.
   "Allocation cost" ka bada hissa allocation ke **baad** first-touch pe aata.
   </details>

---

## Interview questions

1. Allocation ki fast path vs slow path — cost order of magnitude?
2. HFT mein average latency kyun useless, p99.9 kyun matter?
3. Allocation tail ke 4 sources bata.
4. Hot path pe zero-allocation kaise achieve karte (4 techniques)?
5. `std::vector::push_back` "O(1)" — hot path pe kya dhyan?
6. first-touch page fault — allocation number mein kyun nahi, matter kyun karta?

---

## Next
→ [`09-fragmentation.md`](09-fragmentation.md)
