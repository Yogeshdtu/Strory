# 04 — Hot path mein allocation = disaster (measured)

## Prerequisites
- `14-MEMORY` (heap, `new`/`delete`, fragmentation, allocation cost)
- `35-PROFILING-BENCHMARKING/04-05` (latency distribution, percentiles)
- `examples/01_allocation_cost.cpp`

## Yeh topic abhi kyun
Yeh folder ki sabse important single rule hai: **hot path pe ek bhi
allocation nahi.** Yeh kyun — measured proof ke saath — aur kya "allocation"
count hota (bahut si cheezein jo dikhti nahi).

---

## `new` / `malloc` kya karta hai

Ek general-purpose allocator (glibc `malloc`, tcmalloc, jemalloc, MSVC):

1. **Size class** decide karo (small: bins/free-lists; large: direct `mmap`).
2. Small: thread-local cache ya arena ke free-list ka head lo → **fast path,
   ~tens of ns**. Cache empty → central free-list se refill (lock).
3. Large (~> 128 KB - 256 KB): `mmap` syscall → **~µs**, aur `free` pe
   `munmap` (ya not, allocator dependent).
4. `free`: block ko free-list pe wapas; adjacent free blocks ko **coalesce**;
   thresholds pe memory OS ko waapas (`madvise`/`munmap`).
5. Multi-thread: arena locks, cross-thread frees, size-class contention.

**Fast path fast hai. Par har baar fast path nahi hota** — aur woh jab nahi
hota, wahi tumhara p99.9 spike hai.

---

## Measured (`01_allocation_cost.cpp`, is box — AMD Zen 2 ~2 GHz, Windows/MinGW)

Per-op latency **distribution** (throughput nahi):

```
                        p50      p99      p99.9      max
  new[64] + delete      40 ns    90 ns    120 ns    ~22 us
  new[64 KB]            90 ns   110 ns    160 ns     ~6 us
  mixed 8..8192 B churn 80 ns   561 ns   2585 ns   ~176 us
```

- **p50 chhota** (~tens of ns) — free-list fast path, exactly as advertised.
- **p99.9 bahut bada** — free-list refill, coalesce, size-class hop, and
  (for the big sizes) `mmap`/`munmap`.
- **Mixed-size churn sabse jittery** — realistic pattern (alag sizes, kuch
  live rehte) → fragmentation + size-class thrash → p99.9 = **2.5 µs**,
  ~30x the p50.
- `max` values (~176 µs) is unpinned Windows box pe OS-interrupt noise
  hitting the per-op timer — p50/p99/p99.9 hi allocator ka signal (35/06).
  Par p99.9 alone (2.5 µs on a hot path with a ~1 µs budget) is already a
  fail.

**Nichod:** ek allocation ka *typical* cost bearable hai, uska *tail* nahi.
Aur hot path pe tail hi maayne rakhta (lesson 02).

---

## Kya sab "allocation" count hota (jo dikhta nahi)

| Code | Allocates? |
|---|---|
| `new T` / `new T[n]` / `make_unique` / `make_shared` | ✅ (make_shared: 1; `shared_ptr(new)`: 2) |
| `std::vector` first `push_back` / `resize` / grow | ✅ (grow copies too) |
| `std::string` > ~15 chars (past SSO) | ✅ |
| `std::map` / `std::set` / `std::list` per element | ✅ node alloc |
| `std::unordered_map` insert (+ rehash) | ✅ |
| `std::function` with capture > SBO (~16 B) | ✅ in the ctor (ex 08) |
| `throw` (exception object + unwind tables) | ✅ (23) |
| `std::stringstream` / `std::to_string` / `std::format` (to string) | ✅ usually |
| `std::any` (non-small), `std::regex` construction | ✅ |
| lambda captured into `std::function` / stored | ✅ maybe |
| first touch of any freshly-allocated page | ✅ **page fault** (ex 11, lesson 18) |
| growing a `std::deque` (block alloc), `std::vector<bool>` | ✅ |

**Rule:** agar tum type/size compile time pe nahi jaante, ya container grow
kar sakta, ya string SSO se bada — allocation ho rahi hai.

---

## Hot path pe kya karo instead (baaki folder)

| Chahiye | Use | Lesson |
|---|---|---|
| ek fixed-size object baar-baar | **fixed pool** / **object pool** | 06, 07 |
| bahut se temp objects, ek scope ke liye | **arena / bump allocator** | 08 |
| STL container par no heap | `reserve` at startup, **PMR** monotonic on a buffer | 05, 09 |
| producer→consumer messages | **ring buffer** (fixed capacity) | 15 |
| ek chhota variable string | fixed `char[N]` buffer + length, ya `string_view` | 22 |
| a small local collection | `std::array`, a `small_vector`, stack buffer | — |
| all your buffers | **allocate + touch + `mlock` at startup** | 05, 18 |

Poora principle **lesson 05**: startup pe sab allocate, steady state mein
kuch nahi.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "chhoti allocation, fast path hai"
Fast path *aksar* hit hota — par p99.9 pe nahi. Ek hot-path allocation =
ek tail source, chahe woh 64 B ka ho.

### Trap 2 — allocation ko constructor mein chhupana
`std::vector<int> v(n)` in a hot function = ek `malloc` + zero-fill. Hoist
it out (reuse a member `v`, `v.clear()` keeps capacity).

### Trap 3 — `reserve` sirf ek baar bhoolna
`v.reserve(cap)` once + `v.clear()` per use = no realloc. `reserve` inside
the loop = still allocates on first iter.

### Trap 4 — multi-thread mein shared pool
Ek `new`/`delete` doosre thread se → allocator arena lock. Per-thread pool.

### Trap 5 — exceptions on the hot path
`throw` allocates the exception object + walks unwind tables (~µs) — 23.
Hot path pe `std::expected` / error codes (`-fno-exceptions` — 23/07).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "malloc ~30 ns, theek hai" | p99.9 ~2.5 µs (mixed churn); tail hi problem |
| "sirf `new` allocation hai" | vector grow / string SSO / map node / function capture / throw / page fault |
| "`vector v(n)` sasta hai loop mein" | ek malloc + zero-fill per call; hoist + reuse |
| "shared pool fine hai" | arena lock; per-thread |
| "exception rare hai, chalega" | throw = alloc + unwind ~µs; hot path pe nahi |

---

## Exercises

1. Yeh function har call pe kitni allocations karti (dikhne wali + chhupi)?
   ```cpp
   std::string format_order(const Order& o) {
       std::vector<std::string> parts;
       parts.push_back("id=" + std::to_string(o.id));
       parts.push_back("px=" + std::to_string(o.price));
       std::string out;
       for (auto& p : parts) out += p + ";";
       return out;
   }
   ```
   <details><summary>Answer</summary>

   Roughly (libstdc++, typical):
   - `std::vector<std::string> parts;` → 0 initially, but first `push_back`
     allocates the vector's buffer (**1**), and it grows on the 2nd push
     (**+1**, copies the first string).
   - `std::to_string(o.id)` → a `std::string`, likely SSO for a small number
     (**0**), heap if the number is long (**maybe 1**).
   - `"id=" + std::to_string(...)` → a temporary `std::string`; "id=" + up to
     ~20 digits ≈ under SSO, likely **0**, but not guaranteed.
   - `parts.push_back(...)` → moves the temp in (no copy if it fits, the
     vector buffer alloc already counted).
   - `out += p + ";"` → `p + ";"` a temp string; `out +=` grows `out` →
     as `out` exceeds SSO, **1+ reallocs**.
   - return → NRVO, no copy.
   Total: **~3-6 heap allocations** for one call, most invisible. On a hot
   path this is unacceptable. Fix: format into a fixed `char buf[128]` with
   `std::snprintf` / `std::to_chars` (no allocation, no exceptions), return
   a `std::string_view` into a caller-owned buffer, or push a fixed-size
   record into a log ring and format off the hot path.
   </details>

2. `01_allocation_cost.cpp` mein `new[64]` ka p50 = 40 ns par mixed-churn ka
   p99.9 = 2585 ns (~65x). Kyun mixed-size churn itna zyada jittery hai jab
   fixed-64 nahi?

   <details><summary>Answer</summary>

   Fixed 64 B: har allocation ek hi **size class** ki hai. Allocator ka
   thread-cache us class ke liye ek free-list rakhta; alloc = pop head,
   free = push head. Steady state mein yeh list warm rehti → ~har op fast
   path → tight distribution (p99.9 sirf 3x the p50).
   Mixed 8..8192 B: **~10+ different size classes**. Har class ka apna
   free-list, apna refill. Ek 200 B alloc, phir ek 4000 B, phir ek 30 B —
   allocator har baar alag class touch karta, jinme se kuch cache mein
   nahi (refill → central list → maybe lock). Aur **fragmentation**: freed
   blocks alag sizes ke → coalesce work, aur ek badي request ke liye ek
   contiguous run dhoondhna padta (best-fit / bin walk). Aur size-class
   boundaries pe requests round up → internal fragmentation → more `mmap`.
   Net: mixed churn har allocator internal (refill, coalesce, bin walk,
   mmap) ko regularly hit karta → fat tail. Fixed-size + a pool sidesteps
   all of it (example 02: p99.9 ~30 ns, flat).
   </details>

---

## Interview questions

1. `malloc` ka fast path vs slow path — slow path kab, aur woh kyun tumhara p99.9.
2. 6 "chhupi" allocations jo STL/language silently karti.
3. `01_allocation_cost` ke numbers — mixed-churn p99.9 kyun fixed-size se itna zyada.
4. Ek constructor mein chhupi allocation ka example aur uska fix.
5. Multi-thread mein `new`/`delete` ka extra cost.

---

## Next
→ [`05-preallocation.md`](05-preallocation.md)
