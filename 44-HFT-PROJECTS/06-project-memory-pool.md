# 06 — Project 5: Fixed-size memory pool + allocation benchmark

## Prerequisites
- `05-project-matching-engine.md`
- `14-MEMORY/07-memory-pools.md`, `36-LOW-LATENCY-CPP/04-allocation-avoidance.md`,
  `05-preallocation.md`, `06-memory-pools.md`

## Yeh topic abhi kyun

Hot path pe `new`/`malloc` **mana hai** (36/04): syscall (page fault),
lock (allocator), size-class lookup, unpredictable latency (p99.9 spike
jab OS se page maangna pade). Pool = ek baar preallocate, phir O(1)
pop/push. Yeh capstone ka OMS order records ke liye use hota.

## `FixedPool<T, N>` (`mh_mem_pool.hpp`)

```cpp
template <class T, std::size_t N>
class FixedPool {
    struct Slot {
        alignas(T) std::byte storage[sizeof(T)];
        std::uint32_t next;                    // free-list link (valid only while free)
    };
    std::array<Slot, N> buf_{};
    std::uint32_t head_;                       // free-list head index
public:
    T*   alloc();                              // pop head, or nullptr if empty
    void free(T* p);                           // push back onto head
};
```

- **Intrusive free list**: free slot ke andar hi next free slot ka index.
  Koi side table. `alloc` = `i = head_; head_ = slot(i).next; return
  &storage`. `free` = `slot(i).next = head_; head_ = i`.
- **Preallocated**: `std::array<Slot, N>` — poori slab ctor pe. Zero
  runtime allocation after that.
- **O(1), no syscall, no lock, no branch churn.**

Trade-offs (36/06):
- Ek block size (`T`), capacity FIXED — full → `nullptr`, caller handle
  kare.
- Thread-safe **nahi** — per-thread pool (single-writer principle).
- Freed memory OS ko wapas **nahi** — design hai, bug nahi.
- Random free order → slots cache mein bikhar sakte (allocation *pattern*
  matters).

## Measure (`05_memory_pool.cpp`)

### Correctness
```
distinct ptrs, exhaustion -> nullptr, free -> reuse   PASS
```
1024 allocs → 1024 distinct pointers, `in_use()==1024`, 1025th → `nullptr`.
Free all → `in_use()==0`, next alloc reuses a freed slot.

### Latency: alloc + free cycle (32-byte record, this box)
```
FixedPool<Rec,N>   p50  ~30   p99  ~30   p99.9  ~40    max  ~6900   ns
new/delete         p50  ~60   p99  ~80   p99.9  ~200   max ~32000   ns

p50 speedup: ~2.0x   p99.9 speedup: ~5.0x
```

**The tail is the point.** p50 2× is nice; p99.9 5× is the reason. `new`'s
tail spikes = allocator taking a lock under contention, or asking the OS
for a page. Pool's tail = a rare cache miss on a cold slot. HFT cares about
p99.9, not the mean (36/02).

> NOTE (this box): even the pool shows a ~6900 ns `max` — an OS
> deschedule, not the pool. On a pinned/isolated core it would be
> flat. 41/13, 43/16.

## HFT relevance

- Order records, market-data event objects, per-message scratch — all
  pool-allocated with a known upper bound.
- The whole hot path is designed to do **zero** allocation in steady
  state: everything preallocated at startup, pre-faulted (`mlockall` +
  touch, 29), then never grown.
- `std::pmr` (19) is the STL-friendly version: give a container a
  monotonic/pool `memory_resource` and it stops calling global `new`.

## ⚠️ Traps

### Trap 1 — pool full, not handled
`alloc()` returns `nullptr` and the caller derefs it → crash on the hot
path. Size `N` to the real worst case + margin, and handle `nullptr`
(drop, log-cold, or backpressure).

### Trap 2 — `free`ing a pointer the pool doesn't own
`index_of` computes a garbage index → corrupts the free list. `owns(p)`
check in debug builds.

### Trap 3 — double free
`free(p)` twice → the slot appears in the free list twice → two `alloc`s
return the same pointer → aliasing bug. A generation/owned-set check
(debug) or discipline. `ObjectPool` (project 6) solves this with handles.

### Trap 4 — pool shared across threads
No lock → data race → UB. Per-thread pool. If you must share, that's an
MPMC allocator (different, slower structure).

### Trap 5 — measuring at `-O0`
`new`/`delete` at `-O0` are extra-slow (no inlining of the fast path) →
inflated speedup. `-O2` always (35/03).

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Pool = just faster malloc | Also: no syscall, no lock, no size classes → predictable p99.9 |
| Pool never gives back memory = leak | It's fixed-capacity by design; return-to-OS is not the goal |
| One pool for everything | One block size per pool; per-thread |
| Mean speedup is the win | Tail speedup (p99.9) is the win in HFT |

## Exercises

1. `05_memory_pool.cpp` mein pool capacity 4096 se 64 kar do, kIters
   200000. Kya hota?
   <details><summary>Answer</summary>
   Loop `alloc()` then immediately `free()` — steady state mein sirf 1
   slot in use at a time, to capacity 64 kaafi. Numbers unchanged. Ab agar
   loop 100 allocs hold karta phir free karta → capacity 64 pe 65th alloc
   `nullptr` → test the exhaustion path.
   </details>

2. `Rec` ko 32 bytes se 256 bytes kar do. p50 latency pe asar?
   <details><summary>Answer</summary>
   `alloc`/`free` ka pointer math same (O(1)) — par ab har slot 256+ bytes,
   `std::array<Slot,4096>` ~1 MB → cold slots L2/L3 se aayenge → p99.9
   thoda upar. `new/delete` bhi bada block → similar shift. Ratio ~same.
   Bigger objects = more cache pressure regardless of allocator.
   </details>

3. Pool ko `std::pmr::monotonic_buffer_resource` se replace karo ek
   `std::vector` ke liye. Kab yeh better, kab FixedPool?
   <details><summary>Answer</summary>
   `pmr` monotonic: great for "allocate a bunch, free all at once" (per-tick
   scratch, then reset). No individual `free`. FixedPool: individual
   `alloc`/`free` with reuse (order records that come and go). Different
   lifetimes → different tool.
   </details>

## Interview questions

1. Hot path pe `new` ke 4 concrete costs?
2. Intrusive free list kaise kaam karta — free slot mein kya store hota?
3. Pool ka p99.9 `new`/`delete` se itna behtar kyun (mean se zyada)?
4. Pool thread-safe kyun nahi, aur multi-thread mein kya karte?

## Next
→ [`07-project-object-pool.md`](07-project-object-pool.md)
