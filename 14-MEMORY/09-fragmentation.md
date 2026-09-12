# 09 — Fragmentation

## Prerequisites
- [`03-heap-deep-dive.md`](03-heap-deep-dive.md), [`08-allocation-cost.md`](08-allocation-cost.md)

## Yeh topic abhi kyun
Long-running process (server, gateway) hafton chalti hai. Uski memory usage
dheere-dheere badhti hai **bina leak ke** — kyunki heap **fragment** ho jaati
hai: total free memory kaafi hai, par ek continuous bada block nahi milta. Yeh
allocation failures, RSS bloat, aur latency jitter laata hai.

---

## Do kism

### Internal fragmentation — block ke andar barbaad

Aapne `malloc(40)` maanga, allocator ne size-class rounding se **64** diya. 24
bytes us block ke andar barbad — aap use nahi kar sakte, allocator dusre ko de
nahi sakta.

```
  request 40 → block 64:  [ 40 used | 24 wasted ]  (+ ~16 header)
```

- Size classes (8, 16, 32, 48, 64, …), alignment, aur per-block header se aata.
- Chhoti allocations pe percentage bada (`malloc(1)` → ~16-32 bytes = 1600-3200%
  overhead).
- **Fix:** kam, bade allocations; size classes ke paas rakho; SoA / packed
  structs (folder 11).

### External fragmentation — blocks ke beech barbaad

Free memory bahut saare **chhote gaps** mein bikhri hai. Total free 500 MB, par
sabse bada continuous free block 2 MB — to `malloc(4 MB)` fail (ya `mmap` se naya
region → RSS badhta).

```
  heap:  [used][free 8KB][used][free 16KB][used][free 4KB][used]...
         total free = bahut,  largest contiguous = chhota
```

- Alag-alag size aur lifetime ki allocations interleave karne se banta.
- Classic: bahut saare medium objects allocate karo, phir har doosra free karo →
  "swiss cheese" heap.

---

## Ek concrete scenario

```cpp
std::vector<char*> blocks;
for (int i = 0; i < 100000; ++i)
    blocks.push_back(new char[i % 2 ? 64 : 4096]);   // mixed sizes interleaved

for (std::size_t i = 0; i < blocks.size(); i += 2)   // har doosra free
    delete[] blocks[i];
// ab heap: 4096-byte holes 64-byte allocations ke beech phanse hue.
// ek naya `new char[1<<20]` in holes mein fit nahi hoga -> allocator OS se aur maangega
```

RSS: aapne aadhi memory "free" ki, par process ka RSS shayad **kam nahi hua** —
allocator ne pages OS ko wapas nahi ki (holes ke beech used blocks hain, poora
page free nahi).

---

## Long-running process ka problem

- **RSS creep:** din 1 pe 500 MB, din 7 pe 900 MB — leak nahi, fragmentation.
  Peak usage pe pahunchne ke baad allocator woh memory rakhta hai (OS ko wapas
  dena mushkil — beech mein live blocks).
- **Latency jitter:** fragmented heap pe allocator ko zyada bins search karne
  padte, coalescing zyada kaam — p99 badhta (file 08).
- **Allocation failure** eventually — bada contiguous request fail, chhote OK.

---

## Kaise kam karein

| Technique | Kaise madad |
|---|---|
| **Pre-allocate + reuse** | Ek baar bade blocks lo, hot path pe reuse — heap churn hi nahi |
| **Object pools** (file 10) | Ek size class, fixed slots — external frag **zero** us pool mein |
| **Arena / region** | Related-lifetime objects ek block se; poora block ek saath free → no holes |
| **Size bucketing** | Same-size allocations ko alag pool/arena — internal + external dono kam |
| **`std::vector<T>` (flat)** | `T*` ke vector ki jagah — ek contiguous block, no per-element blocks |
| **Better allocator** | jemalloc/mimalloc — smarter size classes, decay/purge, kam frag |
| **`malloc_trim(0)`** (glibc) | Free top-of-heap pages OS ko wapas (band-aid, sab nahi) |
| **Restart** (last resort) | Kuch systems planned restart karte — accept karke design |

---

## Andar kya hota hai

- **Coalescing:** `free` pe glibc adjacent free chunks ko merge karta (boundary
  tags — har chunk ke aage/peeche size). Isliye "free everything then it's one
  big block" — theory mein. Practice mein live blocks beech mein → merge ruk
  jaata.
- **Size classes / bins:** allocator same-size frees ko ek bin mein rakhta →
  same-size reuse fast + no fragmentation *us size ke liye*. Mixed sizes hi
  problem.
- **`mmap`-backed chunks** (bade) `free` pe `munmap` → poori tarah OS ko wapas,
  no fragmentation — par next alloc phir syscall + faults (file 08 trade-off).
- **jemalloc/mimalloc**: "runs"/"slabs" of same-size objects, background
  **decay/purge** free pages ko `madvise(MADV_DONTNEED)` karke OS ko wapas →
  RSS control. glibc yeh aggressively nahi karta.
- **Compaction nahi ho sakti** C/C++ mein — objects ke raw pointers hain, unhe
  move karna = sab pointers update karna = impossible generally. (GC languages
  compact karti hain; C++ nahi.)

> **HFT relevance:** trading process poore din (ya hafton) chalti hai —
> fragmentation = predictable RSS growth + latency drift. Isliye same design jo
> allocation cost ke liye tha: hot path zero-alloc, pooled/arena memory jinke
> size classes fixed hain (order objects, event objects — sab ek size), aur
> startup pe bada contiguous pre-allocation. Frag ka sawaal hi nahi uthta jab
> aap steady-state mein `malloc`/`free` call hi nahi karte. Non-hot allocations
> ke liye jemalloc/mimalloc + soak-test RSS monitoring.

---

## Hands-on

```cpp
// frag.cpp -- interleaved alloc/free, RSS dekho
#include <cstdlib>
#include <vector>
#include <cstdio>
int main() {
    std::vector<char*> v;
    for (int i = 0; i < 200000; ++i) v.push_back((char*)malloc(i & 1 ? 32 : 8192));
    for (size_t i = 0; i < v.size(); i += 2) free(v[i]);          // half freed
    // Linux: yahan `cat /proc/self/status | grep VmRSS` -- kam hua? aksar nahi
    char* big = (char*)malloc(64 << 20);
    printf("big alloc %s\n", big ? "ok (naya mmap region)" : "FAIL");
    return 0;
}
```

```bash
g++ -std=c++20 -O2 frag.cpp -o frag && ./frag
# Linux: /usr/bin/time -v ./frag  -> "Maximum resident set size" dekho
# glibc alternative allocator: LD_PRELOAD=libjemalloc.so ./frag  -> RSS behaviour compare
```

---

## ⚠️ Traps

### Trap 1 — "free kiya to RSS kam hoga"
Allocator pages OS ko rakh sakta hai (reuse ke liye). RSS aksar peak pe stuck.

### Trap 2 — leak aur fragmentation confuse karna
Leak: `new` > `delete`. Frag: counts match, par memory bikhri/held. Tools alag
(LSan vs `malloc_stats` / heap profiler).

### Trap 3 — `vector<T*>` "sirf ek allocation" samajhna
`vector<T*>` = 1 (pointer array) + N (har `*T`) allocations, scattered. `vector<T>`
= 1 contiguous.

### Trap 4 — mixed-size allocations ko ek pool mein daalna
Fixed-size pool sirf ek size ke liye. Mixed → alag pools / size buckets.

### Trap 5 — `malloc_trim` / restart pe rely karna as design
Band-aids. Asli fix: steady-state pe alloc/free mat karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "RSS badh raha = leak" | Frag bhi ho sakta — counts match par memory held/scattered |
| "Sab free karo to heap ek bada block ban jaata" | Live blocks beech mein → coalesce ruk jaata |
| "C++ heap compact ho sakti hai" | Nahi — raw pointers, objects move nahi kar sakte |
| "Internal frag chhoti si baat" | Chhoti allocations pe 100s% overhead; SoA/packing matter |
| "Allocator choice frag pe asar nahi" | jemalloc/mimalloc decay/purge se RSS bahut behtar |

---

## Exercises

1. **Internal frag calc:** `malloc(1)`, `malloc(33)`, `malloc(65)` — glibc size
   classes (16-byte aligned, min ~32) maan ke: har request pe kitna usable vs
   allocated vs header? Percentage waste?

   <details><summary>Answer</summary>

   `malloc(1)` → ~32 bytes chunk (16 min + header) → ~31 waste (>3000%).
   `malloc(33)` → ~48 chunk → ~15 waste (~45%). `malloc(65)` → ~80 chunk → ~15
   waste (~23%). Chhote requests pe % waste bahut zyada.
   </details>

2. **External frag demo:** hands-on `frag.cpp` chalao. Half-free ke baad
   `malloc(64 MB)` succeed karta? RSS (`/usr/bin/time -v`) — half-free se pehle
   vs baad?

   <details><summary>Answer</summary>

   64 MB alloc succeed (naya `mmap` region — heap ke holes mein fit nahi).
   Peak RSS half-free ke baad bhi ~same (allocator ne interior pages rakhe;
   sirf outer 64-MB region alag). Holes reuse ke liye available par bada
   contiguous nahi.
   </details>

3. **`vector<T*>` vs `vector<T>`:** 100000 `Point{double x,y}` — dono tareeke.
   `operator new` count aur total RSS. Frag angle?

   <details><summary>Answer</summary>

   `vector<Point*>`: 1 + 100000 allocations, har `Point` (16 B + header ~16 B)
   scattered → ~3 MB, poor locality, frag-prone. `vector<Point>`: 1 allocation,
   1.6 MB contiguous, cache-friendly, no frag.
   </details>

4. **Allocator swap:** `frag.cpp` ko `LD_PRELOAD=libjemalloc.so` ke saath vs
   without chalao (Linux). Peak RSS difference?

   <details><summary>Answer</summary>

   jemalloc aksar kam peak RSS + background purge se free pages OS ko wapas →
   steady-state RSS glibc se noticeably kam mixed-workload pe. (Exact machine/
   version pe depend.)
   </details>

5. **Pool = no external frag:** ek fixed-size pool (`07_simple_pool.cpp`) mein
   1000 alloc, random 500 free, phir 500 alloc — koi failure? Kyun external
   frag nahi?

   <details><summary>Answer</summary>

   Koi failure nahi (capacity ke andar). Har slot exactly ek size ka → koi bhi
   free slot kisi bhi request ko fit karta → external frag possible hi nahi.
   Internal frag fixed (slot size vs object size) par predictable.
   </details>

---

## Interview questions

1. Internal vs external fragmentation — definition + example har ek?
2. "Sab free kiya par RSS kam nahi hua" — kyun?
3. C++ heap compact kyun nahi ho sakti (GC languages kyun kar sakti)?
4. Fragmentation kam karne ke 5 techniques?
5. Fixed-size pool mein external fragmentation kyun impossible?
6. Leak vs fragmentation — symptoms kaise distinguish karo?

---

## Next
→ [`10-custom-allocation-intro.md`](10-custom-allocation-intro.md)
