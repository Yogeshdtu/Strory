# 09 — Custom allocators with STL containers

## Prerequisites
- `08-arena-allocators.md`
- `19-STL/23-24` (Allocator concept, PMR deep), `19-STL/examples/09_custom_allocator.cpp`, `10_pmr_demo.cpp`
- `examples/05_pmr_containers.cpp`

## Yeh topic abhi kyun
Tum STL containers use karna chahte ho (`vector`, `unordered_map`, `string`)
par unki heap allocation nahi. Do raste: **`std::pmr`** (runtime polymorphic,
easy) ya a **classic `Allocator<T>`** (compile-time, more control, more
boilerplate). Kab kaunsa, aur kaise galat na karein.

---

## Raasta 1: `std::pmr` (prefer this)

Ek `std::pmr::memory_resource*` container ko do; woh saari allocation usse
karta. Standard resources:

| Resource | Kya |
|---|---|
| `std::pmr::new_delete_resource()` | wraps global `new`/`delete` (the default) |
| `std::pmr::null_memory_resource()` | `allocate` always throws — a no-alloc tripwire |
| `std::pmr::monotonic_buffer_resource` | a bump arena over a buffer you give it (08) |
| `std::pmr::unsynchronized_pool_resource` | pool-of-pools (size classes), single-thread |
| `std::pmr::synchronized_pool_resource` | same, thread-safe (has a mutex — avoid on the hot path) |
| **your own** `: public std::pmr::memory_resource` | override `do_allocate` / `do_deallocate` / `do_is_equal` — wrap your `FixedPool` / `Arena` |

```cpp
class PoolResource : public std::pmr::memory_resource {
    FixedPool pool_;
    void* do_allocate(std::size_t b, std::size_t a) override {
        void* p = pool_.allocate();                 // ignores b/a — pool is one size
        if (!p) throw std::bad_alloc{};
        return p;
    }
    void do_deallocate(void* p, std::size_t, std::size_t) override { pool_.deallocate(p); }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
};
```

Hot-path recipe (`05_pmr_containers.cpp`, measured):
```cpp
alignas(64) std::array<std::byte, 16*1024> buf;
std::pmr::monotonic_buffer_resource mono(buf.data(), buf.size(), std::pmr::null_memory_resource());
for each message:
    std::pmr::vector<int> v(&mono);
    v.reserve(EXPECTED);                            // one bump from `buf`
    ... use ...
    mono.release();                                // rewind
// -> 0 global new, p50 ~40 ns vs std::vector ~270 ns
```

**Pros:** trivial to adopt; one resource works for all element types; swap
resources without touching the container type. **Cons:** `do_allocate` is a
virtual call per allocation (inlined + cheap vs the allocation, but present);
`std::pmr::vector<T>` is a distinct type from `std::vector<T>` (an API
boundary decision).

---

## Raasta 2: classic `Allocator<T>` (when you need it)

A stateless or stateful class matching the Allocator requirements. Minimal
C++17+ form:

```cpp
template <class T>
struct ArenaAlloc {
    using value_type = T;
    Arena* arena;                                   // stateful — points at your arena
    ArenaAlloc(Arena* a) noexcept : arena(a) {}
    template <class U> ArenaAlloc(const ArenaAlloc<U>& o) noexcept : arena(o.arena) {}

    T* allocate(std::size_t n) {
        void* p = arena->allocate(n * sizeof(T), alignof(T));
        if (!p) throw std::bad_alloc{};
        return static_cast<T*>(p);
    }
    void deallocate(T*, std::size_t) noexcept {}    // arena: no per-object free

    template <class U> bool operator==(const ArenaAlloc<U>& o) const noexcept { return arena == o.arena; }
    template <class U> bool operator!=(const ArenaAlloc<U>& o) const noexcept { return arena != o.arena; }
};
std::vector<int, ArenaAlloc<int>> v{ArenaAlloc<int>{&my_arena}};
```

**Pros:** no virtual — the allocator type is baked into the container type,
so `allocate` inlines fully; you control everything. **Cons:** the allocator
type is part of the container type (`std::vector<int, MyAlloc>` ≠
`std::vector<int>`) — viral through APIs; stateful allocators have subtle
rules (propagation on copy/move/swap — `propagate_on_container_*` traits);
more boilerplate. `std::pmr::polymorphic_allocator<T>` is literally "a
classic allocator that forwards to a `memory_resource*`" — the bridge
between the two worlds.

---

## Which to use

| You want | Use |
|---|---|
| STL containers with arena/pool speed, minimal fuss | **`std::pmr`** + `monotonic_buffer_resource` / your `memory_resource` |
| a no-allocation guarantee, checked | `std::pmr` with `null_memory_resource()` upstream |
| the absolute leanest allocate (no virtual), and you own the container type | classic `Allocator<T>` |
| nested containers all using the same arena | `std::pmr` (propagates automatically) or `std::scoped_allocator_adaptor` for classic |
| to swap allocation strategy at runtime | `std::pmr` (just pass a different resource) |
| a drop-in for existing `std::vector<T>` call sites | neither fully — both change the type; consider a small wrapper |

**Default: `std::pmr`.** Reach for a classic allocator only when profiling
shows the `do_allocate` virtual actually matters (rare — the allocation
dominates) or you're constrained to a non-pmr container type.

---

## Nested containers: `scoped_allocator_adaptor` / pmr propagation

`std::vector<std::pmr::string>` — the outer vector uses its resource, but the
inner `pmr::string`s need to use it too. `std::pmr` containers **propagate
the resource to elements automatically** when the element is also a `pmr`
type. For classic allocators, `std::scoped_allocator_adaptor<OuterAlloc,
InnerAlloc...>` does the same plumbing. Without it, `vector<pmr::string>`'s
strings would use the **default** resource (global `new`) — a silent leak of
your no-alloc guarantee.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `std::pmr::vector<T>` ≠ `std::vector<T>`
Different type. A function taking `const std::vector<int>&` won't accept a
`std::pmr::vector<int>`. Decide your API boundary; often you pass
`std::span<const int>` and sidestep it.

### Trap 2 — nested containers using the default resource
`std::vector<std::string>` inside a pmr context → the `std::string`s still
`new`. Use `std::pmr::string` (propagates) or `scoped_allocator_adaptor`.

### Trap 3 — `synchronized_pool_resource` on the hot path
It has a mutex. Use `unsynchronized_pool_resource` (single-thread) or a
per-thread `monotonic_buffer_resource`.

### Trap 4 — stateful classic allocator + container move/swap
`propagate_on_container_move_assignment` / `_swap` / `_copy_assignment` and
`is_always_equal` traits decide whether the allocator travels with the
container. Get them wrong → a moved container points at the wrong arena, or
deallocates against the wrong one. `std::pmr::polymorphic_allocator` gets
this right for you.

### Trap 5 — allocator `deallocate` that does real work on the hot path
If your `memory_resource::do_deallocate` walks a free-list / coalesces,
you've reintroduced the allocator tail. An arena's `do_deallocate` should be
a no-op; a pool's should be O(1).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "custom allocator = write an `Allocator<T>`" | usually just a `memory_resource` + `std::pmr` |
| "`pmr::vector` is a `vector`" | distinct type; API boundary decision |
| "outer container's resource covers elements" | only for pmr element types / `scoped_allocator_adaptor` |
| "`synchronized_pool_resource` is fine" | mutex — not on the hot path |
| "pmr virtual call is expensive" | inlined, dwarfed by the allocation; measure before switching to classic |

---

## Exercises

1. `std::pmr::vector<std::pmr::vector<int>> grid(&mono);` — tum expect karte
   ho poori grid `mono` (a stack buffer) se aaye. `grid.push_back({})` ke
   baad `grid[0].push_back(5)` global `new` hit karta hai. Kyun, aur fix?

   <details><summary>Answer</summary>

   Actually `std::pmr` **does** propagate — when you `grid.push_back({})`,
   the inner `std::pmr::vector<int>` is constructed by the outer vector using
   the outer's resource (`&mono`), so `grid[0]`'s resource *should* be
   `&mono`. **But** if the inner vector was constructed elsewhere and moved/
   copied in — e.g. `std::pmr::vector<int> tmp; grid.push_back(std::move(tmp));`
   — `tmp` was default-constructed with `std::pmr::get_default_resource()`
   (global new), and `std::pmr` containers **do not** rebind the resource on
   move (that would require reallocation). So `grid[0]` keeps `tmp`'s
   resource → `push_back(5)` hits global `new`.
   Fixes: (1) construct the inner in place: `grid.push_back(std::pmr::vector<int>{&mono})`,
   or `grid.emplace_back()` (uses-allocator construction picks up the
   outer's resource). (2) If you build `tmp` first, give it the resource:
   `std::pmr::vector<int> tmp{&mono};`. (3) Verify with `null_memory_resource`
   upstream — the global `new` becomes a `bad_alloc` you catch in a test.
   The general rule: with `std::pmr`, **construct nested containers so they
   pick up the outer resource** (in-place / emplace / explicit resource
   arg), never move in a default-constructed one.
   </details>

2. Tum `FixedPool` ko ek `std::pmr::memory_resource` mein wrap karte ho aur
   `std::pmr::unordered_map<int, Order>` use karte ho. Yeh compile hota hai
   par crash karta hai. Kya galat hai?

   <details><summary>Answer</summary>

   `FixedPool` allocates **one fixed block size** (`sizeof(Order)` presumably).
   But `std::pmr::unordered_map` calls `do_allocate` with **several different
   sizes**: the node type (`std::pair<const int, Order>` + hash bookkeeping —
   bigger than `Order`), the **bucket array** (`n * sizeof(void*)`, and it
   *grows* on rehash), and possibly control structures. Your `FixedPool`
   ignores the requested size `b` and hands back a `sizeof(Order)` block for
   *all* of these → the map writes a bucket array / a bigger node into a slot
   that's too small → heap corruption → crash.
   Fixes: (1) Use `std::pmr::unsynchronized_pool_resource` (a pool-of-pools:
   it maintains separate free-lists per size class, backed by an upstream) —
   built for exactly this. (2) Or `std::pmr::monotonic_buffer_resource` over
   a big enough buffer (arena — handles any size, no per-node free, fine if
   the map is rebuilt per scope). (3) Or don't use a hash map on the hot
   path at all — `std::vector<Order>` indexed by a dense `symbol_id`
   computed at startup (no allocation, no hashing, O(1), cache-friendly).
   A single-size `FixedPool` is for **arrays of one object type**, not for a
   node-based container with mixed internal allocations.
   </details>

---

## Interview questions

1. `std::pmr` vs classic `Allocator<T>` — trade-offs, when each.
2. `memory_resource` — the 3 virtuals; wrapping a pool/arena.
3. `null_memory_resource()` as an upstream — what it buys you.
4. Nested containers — resource propagation, `scoped_allocator_adaptor`.
5. Why a single-size `FixedPool` can't back a `pmr::unordered_map`.
6. `std::pmr::vector<T>` vs `std::vector<T>` as an API boundary.

---

## Next
→ [`10-cache-locality-tuning.md`](10-cache-locality-tuning.md)
