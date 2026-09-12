# 06 — Fixed-size memory pool: build it, benchmark it

## Prerequisites
- `04-allocation-avoidance.md`, `05-preallocation.md`
- `14-MEMORY/examples/07_simple_pool.cpp` (the first build)
- `12-POINTERS` (pointer arithmetic, `void*`), `25-OBJECT-MODEL` (placement new, storage vs lifetime)
- `examples/02_memory_pool.cpp`

## Yeh topic abhi kyun
Sabse simple zero-tail allocator. Ek fixed block size, ek fixed capacity,
`O(1)` allocate/free, no syscall, no lock (per-thread), no branch. Yeh HFT
mein order / event / message objects ka default pattern hai.

---

## Design: intrusive free list

Ek baar mein ek **arena** (bada contiguous block) allocate karo. Use `N`
equal **slots** mein baanto. Free slots ki ek **free list** rakho — aur woh
list slots ke **andar** store karo: har free slot ke pehle `sizeof(void*)`
bytes mein agle free slot ka address likh do. Zero extra memory.

```
  arena:  [ slot0 ][ slot1 ][ slot2 ][ slot3 ] ... [ slotN-1 ]
                                                      free list head ─┐
  free_  ──> slot5 ──> slot2 ──> slot9 ──> ... ──> nullptr           │
             (the "next" pointer lives in the first bytes of each free slot)
```

```cpp
class FixedPool {
public:
    FixedPool(std::size_t block, std::size_t count)
        : block_(std::max(block, sizeof(void*))), count_(count),
          arena_(static_cast<std::byte*>(::operator new(block_ * count_,
                    std::align_val_t{alignof(std::max_align_t)}))) {
        free_ = nullptr;
        for (std::size_t i = count_; i-- > 0; ) {
            void* slot = arena_ + i * block_;
            std::memcpy(slot, &free_, sizeof free_);   // *(void**)slot = free_
            free_ = slot;
        }
    }
    ~FixedPool() { ::operator delete(arena_, std::align_val_t{alignof(std::max_align_t)}); }

    void* allocate() noexcept {                        // O(1), no branch except "full"
        if (!free_) return nullptr;                    // FULL — caller must handle
        void* p = free_;
        std::memcpy(&free_, p, sizeof free_);          // free_ = *(void**)p
        return p;
    }
    void deallocate(void* p) noexcept {                // O(1)
        if (!p) return;
        std::memcpy(p, &free_, sizeof free_);          // *(void**)p = free_
        free_ = p;
    }
private:
    std::size_t block_, count_;
    std::byte* arena_;
    void* free_;
};
```

- `std::memcpy` for the pointer read/write — avoids a strict-aliasing / type-
  punning question (`*reinterpret_cast<void**>(slot)` also works if the
  arena is suitably aligned; `memcpy` is the clean form, and the compiler
  turns it into a single `mov` at `-O2`).
- `allocate` / `deallocate` are `noexcept`, branch-light, touch **one**
  cache line (the slot + the `free_` head).
- Full → `nullptr`. The caller **must** have a policy (lesson 24): drop the
  message, reject the order, or a proof that capacity is never exceeded.

---

## Measured (`02_memory_pool.cpp`, is box — steady-state churn, 64 B blocks)

```
                       p50      p99      p99.9      max
  new[64] (churn)      70 ns   160 ns   180 ns   ~60 us
  FixedPool::allocate  20 ns    20 ns    30 ns   ~21 us
```

- **p50**: pool 20 ns vs `new` 70 ns — pool bas ek pointer swap; `new` has
  size-class + thread-cache logic even on its fast path.
- **p99 / p99.9**: pool **20 / 30 ns — flat**. `new` **160 / 180 ns** —
  the free-list refill / coalesce tail. This flatness is the whole point.
- `max` (~21 µs pool, ~60 µs new) is OS-interrupt noise on this unpinned
  box hitting the per-op timer, not the allocator (35/06) — the p99.9
  comparison is the real signal.

**Nichod:** pool ka *typical* thoda tez, uska *tail* naatakiya flat — koi
`mmap`, coalesce, size-class, ya arena lock nahi.

---

## Trade-offs (honest)

| | |
|---|---|
| ❌ **ek hi block size** | alag sizes → alag pools, ya ek arena (08). |
| ❌ **fixed capacity** | full → `nullptr` → tumhari policy. Size for worst case (05). |
| ❌ **not thread-safe** | per-thread pool. A shared pool needs a lock or lock-free free-list (defeats the point on the hot path). |
| ❌ **memory OS ko wapas nahi** | design hai — pool process lifetime tak rehta. |
| ⚠️ **random free order → cache scatter** | freed slots get reused in LIFO free-list order; a burst of frees then allocs can hand back slots that are cold. Usually fine; if it matters, a per-slot generation / a segregated "hot" sub-pool. |
| ✅ **O(1), flat tail, no syscall, one cache line** | the reason it exists. |

---

## HFT usage pattern

```cpp
// per-thread, sized to max in-flight (measured from historical peak × 3)
thread_local FixedPool g_order_pool(sizeof(Order), 8192);

Order* make_order() {
    void* m = g_order_pool.allocate();
    if (!m) [[unlikely]] { ++stats_.pool_exhausted; return nullptr; }  // policy
    return ::new (m) Order{};                     // placement new
}
void retire_order(Order* o) noexcept {
    o->~Order();
    g_order_pool.deallocate(o);
}
```

- `placement new` builds the object in pool memory (25/09).
- `Order`'s dtor runs on retire; if `Order` is trivially destructible you can
  skip it (and then it's basically the "recycle" strategy — lesson 07).
- Wrap it in a small RAII handle (`PoolPtr<Order>`) so you can't forget the
  retire — Rule of Zero for the call sites (17).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `block < sizeof(void*)`
The free-list "next" pointer lives in the slot. A block smaller than a
pointer can't hold it → `std::max(block, sizeof(void*))`.

### Trap 2 — alignment
Slots must be aligned for the object you'll place there. `::operator new`
with `align_val_t` for the arena; ensure `block_` is a multiple of the
alignment (round up).

### Trap 3 — forgetting the dtor on retire
`deallocate` just reclaims memory. If `Order` has a non-trivial dtor
(owns something), you must call `o->~Order()` first — or you leak / UB.

### Trap 4 — using a pool across threads
`allocate`/`deallocate` on `free_` from two threads = a race. Per-thread
pools; if an object is freed by a different thread than allocated it,
route it back via an SPSC ring to the owning thread.

### Trap 5 — no full-policy
`allocate()` returns `nullptr` and the caller derefs it → crash. Every
call site needs `if (!p) [[unlikely]] { ...policy... }`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "pool = fancy malloc" | one size, fixed cap, O(1), flat tail, per-thread |
| "free-list needs extra storage" | it lives *in* the free slots — zero overhead |
| "deallocate frees the object" | it reclaims memory; call the dtor yourself |
| "one pool for all threads" | per-thread; cross-thread free → ring back to owner |
| "full will never happen" | size from data + a policy for when it does |

---

## Exercises

1. Yeh `deallocate` mein ek bug hai jo double-free ko silently corrupt
   karta hai. Kaunsa, aur ek cheap debug check?
   ```cpp
   void deallocate(void* p) noexcept {
       std::memcpy(p, &free_, sizeof free_);
       free_ = p;
   }
   ```
   <details><summary>Answer</summary>

   Agar tum ek slot `p` ko **do baar** `deallocate` karo (double free), to:
   pehli baar `p` free-list ka head ban jaata. Doosri baar `*(void**)p =
   free_` (= `p` itself, since `p` is now head) → `p->next = p` → a **cycle**
   in the free list. Ab do `allocate()` calls **same slot** return karengi →
   two live objects at one address → corruption, undiagnosable.
   Cheap debug check (compile out in release):
   ```cpp
   #ifndef NDEBUG
       assert(p >= arena_ && p < arena_ + block_ * count_);          // in range
       assert((static_cast<std::byte*>(p) - arena_) % block_ == 0);  // slot-aligned
       for (void* f = free_; f; std::memcpy(&f, f, sizeof f))        // not already free
           assert(f != p);
   #endif
   ```
   The free-list walk is O(free count) — fine in a debug build. Production:
   an occupancy bitmap (1 bit/slot) makes double-free / OOB an O(1) assert.
   </details>

2. Tumhara `Order` object ek thread pe pool se allocate hota hai (the feed
   handler) par ek **doosre** thread pe retire hota hai (the order manager,
   after the exchange ack). Direct `g_order_pool.deallocate(o)` from the
   second thread kyun galat hai, aur sahi pattern?

   <details><summary>Answer</summary>

   `g_order_pool` is `thread_local` (or per-thread) and its `free_` list is
   single-writer. The order manager thread calling `deallocate` races with
   the feed handler's `allocate` on the same `free_` head → torn free list →
   corruption. Also, `thread_local` means the order manager would touch *its
   own* pool, not the feed handler's — the slot never gets returned to the
   right pool → the feed handler's pool leaks until it's exhausted.
   Correct pattern: **the object goes home to be freed.** The order manager,
   when done, pushes `o`'s pointer into an **SPSC "retire" ring** owned by
   the feed handler thread. The feed handler drains that ring in its loop
   and calls `o->~Order(); pool.deallocate(o);` on its own thread. One extra
   ring hop, zero contention, correct ownership. (This is the standard
   "each thread frees what it allocated" discipline for per-thread pools.)
   Alternative: a genuinely concurrent pool with a lock-free free-list
   (Treiber-style with ABA protection — 28/04) — but that's slower per op
   and only worth it if the retire-ring hop is unacceptable.
   </details>

---

## Interview questions

1. Intrusive free list — "next" pointer kahan store hota, kyun zero overhead.
2. `allocate`/`deallocate` ki cost — kitni instructions, kitni cache lines.
3. Pool ka p99.9 `new` se itna flat kyun (kya nahi hota).
4. Placement new + explicit dtor — pool ke saath object lifecycle.
5. Cross-thread free — kyun problem, aur "goes home to be freed" pattern.
6. Double-free ko pool mein kaise detect karein (bitmap / free-list walk).

---

## Next
→ [`07-object-pools.md`](07-object-pools.md)
