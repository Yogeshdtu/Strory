# 07 — Project 6: Object pool with generation-checked handles

## Prerequisites
- `06-project-memory-pool.md`
- `25-OBJECT-MODEL` (lifetime, handles + generation counters)

## Yeh topic abhi kyun

`FixedPool` raw slots deta hai — double-free, use-after-free se khud nahi
bachata. `ObjectPool` typed objects + **handles** deta jinme ek generation
counter hota. Yeh HFT ka ek asli safety problem solve karta:

> Order slot 7 recycle ho gaya (order A retired, order B ne wahi slot
> liya). Ab exchange se order **A** ka ek late ack/fill aata (network
> delay). Bina generation check ke, woh order **B** pe apply ho jaata —
> silent wrong fill. Generation = cheap use-after-free guard.

## `ObjectPool<T>` (`mh_object_pool.hpp`)

```cpp
template <class T>
class ObjectPool {
    struct Slot { T obj{}; std::uint32_t gen = 0; bool live = false; };
    std::vector<Slot>          slots_;
    std::vector<std::uint32_t> free_;
public:
    struct Handle { std::uint32_t idx = 0, gen = 0;  bool valid() const { return gen != 0; } };

    Handle acquire(Args&&...);   // reuse a free slot or push a new one; gen never 0
    T*     get(Handle h);        // nullptr if h.gen != slots_[h.idx].gen  or  !live
    bool   release(Handle h);    // ++gen (invalidates every old handle); false on stale/double
};
```

- `acquire()` → `Handle{idx, gen}`. gen starts at 1, never 0 (0 = null
  handle).
- `release(h)` → `slots_[h.idx].gen++`, `live = false`, slot goes on
  `free_`. **Every handle with the old gen is now dead.**
- `get(stale_handle)` → `nullptr` (gen mismatch), even after the slot is
  re-`acquire`d by someone else with a new gen.
- `release` an already-released handle → `false` (double-release rejected).

## Measure (`06_object_pool.cpp`)

### Correctness
```
reuse, stale-handle rejection (even post-recycle), no double-release   PASS
```
The key assertion:
```cpp
auto h1 = pool.acquire();  auto stale = h1;
pool.release(h1);
assert(pool.get(stale) == nullptr);      // released -> dead
auto h2 = pool.acquire();                // reuses h1's slot
assert(h2.idx == stale.idx);             // same slot
assert(h2.gen != stale.gen);             // new generation
assert(pool.get(stale) == nullptr);      // OLD handle still dead, even though slot is live
assert(pool.get(h2) != nullptr);         // new handle works
```

### Latency: acquire + release cycle (this box)
```
p50 ~30   p99 ~40   p99.9 ~110   max ~9200  ns
```
Free-list index pop/push + one generation bump. Slot storage preallocated
(the `std::vector<Slot>` grows only when the pool hits a new high-water
mark).

## HFT relevance

- **OMS order records** (project 10): `OrderManager` stores each order in
  an `ObjectPool<OmsOrder>` and keeps `ClientOrderId -> Handle`. When an
  order retires, the slot recycles; a late exchange message for the old
  `ClientOrderId` finds nothing in the index (or a dead handle) and is
  safely dropped.
- **Any recycled resource with async callbacks**: connections, subscription
  slots, timers.
- The pattern generalizes: "handle = index + generation" is how game
  engines, ECS systems, and Vulkan-style APIs give you cheap, safe
  references into pooled storage.

## ⚠️ Traps

### Trap 1 — generation wraps
`std::uint32_t gen` wraps after 4.29 billion release cycles on one slot.
Then an ancient handle could alias. For most systems: never happens in a
session. If it might, use `uint64_t` gen, or detect wrap and retire the
slot permanently.

### Trap 2 — handing out gen 0
`Handle{idx, 0}` is the null handle. `acquire` must skip 0 (on wrap too).
The code does `if (s.gen == 0) s.gen = 1`.

### Trap 3 — storing a `T*` instead of a `Handle`
`get(h)` returns a `T*` that is only valid *right now*. Cache the `Handle`,
call `get` each time. A stored `T*` survives `release` and points at a
recycled object — exactly the bug the pool exists to prevent.

### Trap 4 — `T`'s destructor side effects
`release` does `s.obj = T{}` (assignment, not explicit dtor+ctor). If `T`
holds a resource, make sure `T{}`'s assignment releases it. Or use a
version that calls `obj.~T()` then placement-new on `acquire`.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Pool + raw pointers is fine | Recycled slot + stale pointer = silent wrong-object bug |
| Generation check is expensive | One `uint32` compare — cheaper than the bug it prevents |
| Handle == pointer | Handle = {idx, gen}; resolve via `get()` each use |
| A released handle is just "empty" | It's *poisoned* — and stays poisoned after the slot recycles |

## Exercises

1. `06_object_pool.cpp` mein `release` ke baad `get(stale)` ko `get(h2)`
   ke gen se call karo (spoof karke `stale.gen = h2.gen`). Kya hota?
   <details><summary>Answer</summary>
   Ab gen match karega → `get` non-null dega, pointing at h2's object.
   Yeh "forge a handle" attack hai — generation ek *accidental* misuse
   guard hai, security boundary nahi. Trusted code hi handles banaye.
   </details>

2. `Slot::gen` ko `std::uint8_t` kar do. Kab break hota?
   <details><summary>Answer</summary>
   256 release cycles per slot ke baad wrap → ancient handle alias kar
   sakta. Ek busy OMS mein ek slot minute mein 256 baar recycle ho sakta
   → seconds mein unsafe. `uint32` (billions) practically safe for a
   session; `uint8` nahi.
   </details>

3. OMS mein `ClientOrderId -> Handle` ki jagah `ClientOrderId -> OmsOrder*`
   store karo. Kaunsa bug wapas aata?
   <details><summary>Answer</summary>
   Order retire → pool slot recycle → agla `acquire` wahi memory dobara
   use karta → purana `OmsOrder*` ab kisi aur order ko point karta. Ek
   late fill for the old id → `find(id)` returns that pointer → fill lands
   on the WRONG order. Handle + `get()` (gen check) is exactly the fix.
   </details>

## Interview questions

1. "Handle = index + generation" pattern — yeh kaunsa bug class solve
   karta?
2. Ek recycled pool slot ka purana handle `get()` pe kya deta, aur kyun?
3. Generation counter wrap ka risk aur mitigation?
4. Handle vs pointer — kyun handle store karo, `T*` nahi?

## Next
→ [`08-project-spsc-queue.md`](08-project-spsc-queue.md)
