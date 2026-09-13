# 03 — Heap deep dive (allocator kya karta hai)

## Prerequisites
- [`02-stack-deep-dive.md`](02-stack-deep-dive.md)
- Folder 12 (pointers) — heap objects pointer se hi milte hain

## Yeh topic abhi kyun
`new` / `malloc` "memory de do" kehte hain — par **kaun deta hai, kaise?** Woh
kaam ek **allocator** karta hai (library code, aapke process mein). Uska
mechanism samajhna zaroori hai taaki file 08 (cost) aur file 09 (fragmentation)
samajh aayein, aur pata chale ki hot path pe `new` kyun avoid karte hain.

---

## Heap — ek bada pool jise allocator baantta hai

OS aapko memory **badi chunks** mein deta hai (pages, aksar 4 KB; ya bade
regions `mmap`/`VirtualAlloc` se). Aapka program hazaaron chhoti allocations
maangta hai (`new int`, `new std::string`, …). Beech mein **allocator** baithta
hai:

```
   your code:  new / malloc / free / delete
        │
        ▼
   allocator (glibc malloc / mimalloc / jemalloc / MSVCRT / …)
     - free blocks ka book-keeping
     - request ko ek free block se match karo (ya OS se aur maango)
     - alignment, headers, size classes, per-thread caches
        │
        ▼
   OS:  brk / sbrk  (heap ko thoda aage badhao)
        mmap / VirtualAlloc  (bade / independent regions)
```

`malloc` **syscall nahi hai** — zyada tar calls pure user-space book-keeping
hain. Syscall (`mmap`/`brk`) sirf tab jab allocator ke paas free memory khatam.

---

## Ek allocation ke andar (glibc malloc, simplified)

`malloc(40)` maangne pe:

1. **Size round up** — usable size + header/alignment. 40 → shayad 48 ya 64
   (size class).
2. **Chhota size?** → per-thread cache (**tcache**, glibc 2.26+) dekho: us size
   class ka ek block ready ho to turant do (lock-free, ~ns). Yeh **fast path**.
3. tcache empty → **bins** (free lists: fast/small/large/unsorted) mein us size
   ka block dhoondho. Lock lena pad sakta hai (arena mutex).
4. Kuch nahi mila → ek bade free chunk ko **split** karo.
5. Woh bhi nahi → **top chunk** se kaato; woh khatam → `brk` se heap badhao
   (syscall).
6. **Bada request** (glibc default > 128 KB, `M_MMAP_THRESHOLD`) → seedha
   `mmap` (syscall) — apna region, `free` pe `munmap`.

`free(p)`:
1. `p` ke pehle wale header se size nikaalo.
2. Chhota → tcache mein daalo (ready for next `malloc`).
3. tcache full / bada → bins mein; **adjacent free chunks ke saath coalesce**
   (merge) karo taaki bade blocks banein (fragmentation kam — file 09).

**Header:** har allocated block ke theek pehle allocator ka metadata (size,
flags) hota hai. Isiliye `malloc(1)` bhi ~16-32 bytes "kha jaata hai", aur
`p[-1]` likhna (OOB) allocator ko corrupt kar deta hai.

---

## `new` vs `malloc`

| | `malloc(n)` / `free(p)` | `new T` / `delete p` |
|---|---|---|
| Layer | C, raw bytes | C++ — allocation **+ constructor** / destructor **+ free** |
| Return | `void*` (cast karo) | `T*` (typed) |
| Failure | `nullptr` | `throw std::bad_alloc` (ya `new(nothrow)` → `nullptr`) |
| Size | aap dete ho | compiler `sizeof(T)` se |
| Array | `malloc(n * sizeof(T))` | `new T[n]` (element count yaad rakhta hai) |
| Override | `malloc` hook / `LD_PRELOAD` | global/class `operator new`/`delete` |

`new` andar `operator new(size)` call karta hai — jo **by default `malloc` jaisa
hi** hota hai (aksar literally `malloc` ko call karta). Phir constructor. To
`new` ki cost ≈ `malloc` ki cost + constructor ki cost.

**⚠️ Kabhi mix mat karo:** `malloc` + `delete`, `new` + `free`, `new[]` +
`delete` (bina `[]`) — sab **UB** (file 04).

---

## `operator new` — customizable hook

```cpp
void* operator new(std::size_t n);           // global -- new T isko call karta
void  operator delete(void* p) noexcept;
void* operator new[](std::size_t n);         // new T[] (kuch impl isko alag rakhte -- README note)
void  operator delete[](void* p) noexcept;
```

Aap inhe **replace** kar sakte ho (poore program ke liye) — leak counters,
custom allocator, instrumentation. Class-specific `operator new` bhi (`struct S {
static void* operator new(std::size_t); };`) — us type ke liye pool.
[`examples/04_memory_leak.cpp`](examples/04_memory_leak.cpp) aur
[`05_use_after_free.cpp`](examples/05_use_after_free.cpp) yeh counting ke liye
karte hain.

---

## Alternative allocators

Default allocator general-purpose hai — safe, par latency-variable. Log,
servers, aur HFT aksar swap karte hain:

| Allocator | Kyun |
|---|---|
| **jemalloc** | kam fragmentation, achhi multithread scaling, profiling |
| **mimalloc** | bahut fast, chhota, achhi tail latency |
| **tcmalloc** | Google, thread caches, fast |
| custom **arena/pool** (file 10) | bounded, deterministic — hot path ke liye best |

`LD_PRELOAD=/usr/lib/libjemalloc.so ./app` — bina recompile swap (Linux).

---

## Andar kya hota hai — page faults + first touch

- `malloc` aapko ek **virtual** address deta hai. Physical RAM tab milti hai jab
  aap us page ko **pehli baar touch** karo → **page fault** → OS ek physical
  page map karta hai (aur zero karta) → har page pe sub-µs se kuch µs (is Windows machine pe naapa: 64 MB
  ka pehla `memset` doosre se ~4–7 ms zyada — 16,384 pages → ~0.3–0.4 µs/page). Yeh "first touch" cost
  allocation ke number mein nahi dikhta par uske turant baad aata hai (file 08, exercise 5).
- `mmap`-backed badi allocations `free` pe `munmap` → next same-size alloc phir
  se page-fault karega. Isliye bade buffers ko **reuse** karo, baar-baar
  alloc/free nahi.
- **`calloc`** OS se aayi fresh pages (already zero) ke liye memset skip kar
  sakta — bade zeroed buffers ke liye `malloc + memset` se tez.
- **THP / huge pages** (2 MB) — kam TLB misses bade working sets pe (folder 29 file 12, folder 32 file 11).

> **HFT relevance:** default `malloc` ka fast path (glibc pe tcache hit) tens of ns hai — is repo ke Windows
> UCRT heap pe p50 30 ns naapa (file 08) — par woh path miss ho sakta hai: heap/arena lock contention
> (multi-thread), bin search, `brk`/`mmap`/`VirtualAlloc` syscall, first-touch page faults. Yeh sab **tail
> latency** banate hain (file 08 naapa: p99.9 110–430 ns, max 7–109 µs). Isliye hot
> path pe allocation hoti hi nahi — sab kuch pehle se allocate, phir pool/arena
> se O(1) reuse. Allocator choice (jemalloc/mimalloc) startup aur non-hot paths
> ke liye matter karta hai.

---

## Hands-on

```bash
./build.ps1 fast 14-MEMORY/examples/02_stack_vs_heap.cpp        # heap ~37 ns/iter vs stack <1 ns
./build.ps1 fast 14-MEMORY/examples/06_allocation_benchmark.cpp # allocator ka tail dekho
```

Aur: [`examples/04_memory_leak.cpp`](examples/04_memory_leak.cpp) ka
`operator new` override padho — wahi hook hai jise real allocators replace karte
hain.

---

## ⚠️ Traps

### Trap 1 — `malloc(1)` ka matlab 1 byte
Header + alignment + size class → ~16-32 bytes actually consume. Chhoti
allocations ki count matter karti hai.

### Trap 2 — `malloc` ko syscall samajhna
Zyada tar user-space. Syscall sirf jab allocator ko OS se aur chahiye.

### Trap 3 — OOB write jo header corrupt kare
```cpp
int* p = (int*)malloc(4);  p[-1] = 0;  free(p);   // ⚠️ allocator metadata corrupt -> crash "kahin aur"
```

### Trap 4 — bade buffer ko baar-baar alloc/free
```cpp
for (...) { auto* b = new char[1<<20]; use(b); delete[] b; }   // har iter mmap+munmap+faults
```

### Trap 5 — `new`/`malloc` mix
```cpp
int* p = (int*)malloc(sizeof(int));  delete p;    // ⚠️ UB
int* q = new int;                    free(q);     // ⚠️ UB
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`malloc` OS se memory maangta har baar" | Zyada tar user-space book-keeping; syscall kabhi-kabhi |
| "`new` bas `malloc` ka doosra naam" | `operator new` (≈malloc) **+ constructor**; `delete` = dtor + free |
| "`malloc(n)` exactly n bytes leta" | + header + alignment + size-class rounding |
| "Allocation cost constant hai" | Fast path ~ns; slow path (lock/syscall/fault) us se 100-1000x |
| "Allocator choice matter nahi karta" | Fragmentation + tail latency + MT scaling pe bada asar |

---

## Exercises

1. **Fast vs slow path:** [`examples/06_allocation_benchmark.cpp`](examples/06_allocation_benchmark.cpp)
   run karo. Scenario A (reuse) ka p50 vs max? Woh ratio kya batata hai
   allocator ke baare mein?

   <details><summary>Answer</summary>

   Is machine pe (GCC 16.2 `-O2`, Windows UCRT heap): p50 **30 ns** (fast path — us mein ~10 ns rdtsc ka apna
   cost), max **7.5–25 µs** (slow path: free-list miss / lock / OS se memory / fault). ~250–800x spread;
   scenario B (retain) mein max 67–109 µs. Allocation "constant cost" nahi hai.
   </details>

2. **Header overhead:** 1,000,000 baar `malloc(1)` (free mat karo) — process RSS
   kitna badha (Linux `/proc/self/status` `VmRSS`)? 1 MB se bahut zyada kyun?

   <details><summary>Answer</summary>

   Linux/glibc: ~32 MB (minimum chunk 32 bytes — glibc ke niyam, is box pe chalaya nahi). Windows pe
   (`GetProcessMemoryInfo` → `PrivateUsage`, GCC 16.2, UCRT heap) naapa: **+17.2 MB → ~18 bytes per
   `malloc(1)`**. Dono jagah 1 byte maango, 16–32 bytes jaate hain — header + minimum block + alignment.
   </details>

3. **mmap threshold:** `malloc(100000)` vs `malloc(200000)` (glibc,
   `M_MMAP_THRESHOLD` default 128 KB) — `strace` mein kaunsa `mmap` syscall
   trigger karta, kaunsa nahi?

   <details><summary>Answer</summary>

   100 KB < 128 KB → heap se (koi naya `mmap` nahi, aksar). 200 KB > 128 KB →
   apna `mmap` region. `free` pe doosra `munmap` karta hai.
   </details>

4. **calloc vs malloc+memset:** ek 64 MB buffer — `malloc + memset(0)` vs
   `calloc`. Time compare (`-O2`). `calloc` tez kyun ho sakta?

   <details><summary>Answer</summary>

   Fresh pages OS se already-zero aati hain (`mmap` Linux pe, `VirtualAlloc` Windows pe); `calloc` jaanta hai
   to `memset` skip karta. `malloc+memset` explicitly poori 64 MB likhta (aur har page fault karta). `calloc`
   first-touch pe hi fault karega, par redundant memset bachta.

   Windows UCRT pe naapa (3 runs × 2, har page ek baar padh ke): `malloc`+`memset`+touch **7.0–11.1 ms**,
   `calloc`+touch **4.9–6.5 ms**, aur `calloc` akela **13–37 µs** — yaani calloc ne 64 MB zero nahi kiye,
   sirf maange. ⚠️ `malloc`/`memset` ko `[[gnu::noipa]]` helpers mein rakho, warna GCC unused buffer elide kar
   dega.
   </details>

5. **Override hook:** ek chhota program jisme global `operator new`/`delete`
   override karke har alloc/free print kare. `std::string s = "a very long
   string..."; std::vector<int> v(100);` — kitne `operator new` calls?

   <details><summary>Answer</summary>

   `std::string` (SSO se lambi) → 1. `std::vector<int> v(100)` → 1 (400 bytes).
   Chhoti string (SSO) → 0. Exact count libstdc++ pe depend, par pattern yahi.
   ⚠️ MinGW pe: `std::string`/`std::vector` template code exe mein banta hai, isliye gine jaate hain; par
   `libstdc++-6.dll` ke andar hone wali allocations (jaise default `operator new[]`) exe ka override nahi dekhta —
   `-static` se link karke gino (file 05).
   </details>

---

## Interview questions

1. `malloc` ke andar kya hota hai (fast path vs slow path)?
2. `malloc` syscall hai? Kab syscall banta hai?
3. `new T` `malloc` se kaise alag — steps?
4. Allocation header/metadata kya hai, OOB write se kya bigadta?
5. `mmap` threshold — chhoti vs badi allocation ka alag treatment kyun?
6. First-touch page fault — allocation cost ke number mein kyun nahi dikhta, par matter kyun karta?

---

## Next
→ [`04-new-and-delete.md`](04-new-and-delete.md)
