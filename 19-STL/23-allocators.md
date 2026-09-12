# 23 — Allocators: the concept, `std::allocator`, writing your own

## Prerequisites
- [`22-bit-utilities.md`](22-bit-utilities.md), folder 14 (heap, `new`/`delete`, placement new)
- Folder 17 (RAII), folder 18 (move), [`21-type-traits.md`](21-type-traits.md)

## Yeh topic abhi kyun
Har STL container by default `new`/`delete` se memory leta hai — aur `new` ek
**general-purpose heap allocator** hai: slow (µs-scale worst case), non-
deterministic (locks, OS calls), aur fragmentation-prone. HFT ko ye acceptable
nahi. Allocator = woh **customization point** jisse aap container ko batate ho
"memory yahan se lo" — ek arena, ek pool, stack buffer. Ye foundation hai file
24 (PMR) ka.

`examples/09_custom_allocator.cpp` mein `LoggingAllocator` aur `ArenaAllocator`
dono hain.

---

## What an allocator is

An allocator is an object a container uses for **raw memory** (not object
construction — that's separate). Every allocator-aware container takes it as the
last template parameter:

```cpp
std::vector<int>                                  // == std::vector<int, std::allocator<int>>
std::vector<int, MyAlloc<int>>
std::map<K, V, Cmp, MyAlloc<std::pair<const K, V>>>
std::basic_string<char, std::char_traits<char>, MyAlloc<char>>
```

Two operations that matter:
- `T* allocate(n)` — get storage for `n` objects of `T` (uninitialized).
- `void deallocate(T* p, n)` — give it back (`n` must match the `allocate`).

Construction/destruction of the elements in that storage goes through
`std::allocator_traits<A>::construct` / `destroy` (placement `new` / `~T()` by
default).

## `std::allocator<T>` — the default

```cpp
template <class T>
struct allocator {
    using value_type = T;
    T*   allocate(std::size_t n)          { return static_cast<T*>(::operator new(n * sizeof(T))); }
    void deallocate(T* p, std::size_t)    { ::operator delete(p); }
};
```

That's essentially it since C++17 (the old `construct`, `pointer`, `rebind`,
`max_size` members are now supplied by `std::allocator_traits`). It just forwards
to global `operator new` / `operator delete` → the general heap.

## Minimal custom allocator (C++17+)

```cpp
template <class T>
struct LoggingAllocator {
    using value_type = T;

    LoggingAllocator() = default;
    template <class U> LoggingAllocator(const LoggingAllocator<U>&) noexcept {}   // rebinding copy ctor -- REQUIRED

    T* allocate(std::size_t n) {
        void* p = ::operator new(n * sizeof(T));
        std::printf("  alloc  %zu * %zu = %zu B -> %p\n", n, sizeof(T), n * sizeof(T), p);
        return static_cast<T*>(p);
    }
    void deallocate(T* p, std::size_t n) noexcept {
        std::printf("  free   %zu * %zu -> %p\n", n, sizeof(T), (void*)p);
        ::operator delete(p);
    }
};
template <class T, class U>
bool operator==(const LoggingAllocator<T>&, const LoggingAllocator<U>&) { return true; }   // stateless -> all equal
template <class T, class U>
bool operator!=(const LoggingAllocator<T>& a, const LoggingAllocator<U>& b) { return !(a == b); }
```

The required surface:
1. `value_type`.
2. `allocate(n)` / `deallocate(p, n)`.
3. A **templated converting constructor** — a `std::list<int, A<int>>` internally
   needs `A<ListNode<int>>`, so the allocator must be convertible across `T`.
   (Old code did this via a `rebind` member struct; `allocator_traits` derives it
   from the template now, but the converting ctor must exist.)
4. `operator==` / `operator!=` — "can memory from `a` be freed by `b`?" For a
   stateless allocator, always `true`.

## Stateful allocator: a bump / arena allocator

```cpp
struct Arena {
    std::byte* buf; std::size_t cap; std::size_t off = 0;
    void* bump(std::size_t n, std::size_t align) {
        std::size_t p = (off + align - 1) & ~(align - 1);
        if (p + n > cap) throw std::bad_alloc{};
        off = p + n;
        return buf + p;
    }
    void reset() { off = 0; }                 // free EVERYTHING at once, O(1)
};

template <class T>
struct ArenaAllocator {
    using value_type = T;
    std::shared_ptr<Arena> arena;             // shared so rebound copies use the SAME arena

    explicit ArenaAllocator(std::shared_ptr<Arena> a) : arena(std::move(a)) {}
    template <class U> ArenaAllocator(const ArenaAllocator<U>& o) noexcept : arena(o.arena) {}

    T*   allocate(std::size_t n)          { return static_cast<T*>(arena->bump(n * sizeof(T), alignof(T))); }
    void deallocate(T*, std::size_t)      noexcept { /* no-op -- arena frees in bulk via reset() */ }
};
template <class T, class U>
bool operator==(const ArenaAllocator<T>& a, const ArenaAllocator<U>& b) { return a.arena == b.arena; }
```

- `allocate` = pointer bump + bounds check. **No syscall, no lock, no
  free-list** — a handful of instructions, fully deterministic.
- `deallocate` does **nothing**; memory is reclaimed all at once by
  `arena->reset()` between cycles.
- **Stateful** → `operator==` compares the arena identity; containers with
  unequal allocators can't steal each other's buffers on move.
- `std::allocator_traits` flags: `propagate_on_container_move_assignment`,
  `..._copy_assignment`, `..._swap`, and `select_on_container_copy_construction`
  control whether the allocator travels with the container on those operations —
  the defaults are usually right for a `shared_ptr`-held arena.

`examples/09` wires exactly this into `std::vector<int, ArenaAllocator<int>>` and
shows the vector's growth allocations all coming off the arena, then `reset()`
reusing the space.

---

## Andar kya hota hai

- A container never calls the allocator directly — it goes through
  `std::allocator_traits<A>`, which fills in anything `A` omits (`construct` =
  placement new, `max_size`, `rebind` via `A<U>`, pointer types). So a minimal
  `A` with 3 things works everywhere.
- `std::vector<T, A>` stores an `A` (empty-base-optimized if stateless → costs 0
  bytes; a `shared_ptr`-carrying arena alloc adds 16 bytes to the vector). On
  growth it calls `traits::allocate(a, newCap)`, moves/copies elements
  (`traits::construct`), `traits::destroy`s the old, `traits::deallocate`s.
- `operator==` matters on **move assignment / swap**: `v2 = std::move(v1)` can
  only adopt `v1`'s buffer if `a2 == a1` (or the allocator propagates); otherwise
  it must allocate with `a2` and move elements one by one.
- Node containers (`list`, `map`, `unordered_map`) rebind the allocator to their
  node type and call `allocate(1)` **per element** — this is where a pool
  allocator (fixed-size blocks, free-list) turns N heap calls into ~0.

> **HFT relevance:** the default `operator new` is a shared, locked,
> general-purpose allocator with tail-latency spikes (page faults, arena locks,
> `mmap`) — unacceptable in a path with a microsecond budget. The fix is a custom
> allocator: an **arena/bump** allocator for per-cycle scratch (`allocate` = one
> add, `deallocate` = nothing, `reset()` between events), or a **fixed-block
> pool** for node containers and message objects (free-list push/pop, O(1), no
> fragmentation). All buffers are reserved at **startup** so steady state never
> calls the OS. The template-parameter form here works but is rigid (the
> allocator is part of the type); file 24's `std::pmr` makes it a runtime choice
> with one container type.

---

## Hands-on

```bash
./build.ps1 19-STL/examples/09_custom_allocator.cpp
```

Watch `LoggingAllocator` print every `std::vector` growth allocation (sizes
1,2,4,8...), then `ArenaAllocator` serve all of them from one buffer with
`deallocate` a no-op, and `arena.reset()` rewind the offset so the next fill
reuses the same bytes — zero new allocations.

---

## ⚠️ Traps

### Trap 1 — missing the templated converting constructor
```cpp
template <class T> struct A { using value_type = T; /* ... no template<class U> A(const A<U>&) */ };
std::list<int, A<int>> l;   // ❌ list needs A<_List_node<int>> -> won't compile without the converting ctor
```

### Trap 2 — stateful allocator with a wrong / missing `operator==`
```cpp
// if operator== always returns true but allocators hold different arenas,
// a move/swap can free arena-B memory through arena-A -> corruption
```

### Trap 3 — arena `deallocate` no-op + long-lived container
```cpp
std::vector<T, ArenaAllocator<T>> v;   // grows repeatedly -> each growth bumps the arena, old buffers never reclaimed until reset() -> arena exhausts
```

### Trap 4 — allocator is part of the type
```cpp
std::vector<int> a;
std::vector<int, MyAlloc<int>> b;
a = b;   // ❌ different types. Can't pass a MyAlloc vector where a default vector is expected -> API friction (file 24 solves this)
```

### Trap 5 — forgetting alignment in a custom `allocate`
```cpp
return static_cast<T*>(::operator new(n * sizeof(T)));   // OK for global new (max-aligned). A raw buffer bump MUST align to alignof(T)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "The allocator constructs the objects" | It provides raw memory; construction is `allocator_traits::construct` (placement new) |
| "`std::allocator` does something clever" | Since C++17 it's a thin forward to `::operator new`/`delete` |
| "A custom allocator needs `rebind`, `pointer`, `max_size`..." | C++17+: just `value_type`, `allocate`, `deallocate`, converting ctor, `==`/`!=` — traits fill the rest |
| "Stateless and stateful allocators behave the same on move/swap" | `operator==` / propagation traits decide whether the buffer can be adopted |
| "Custom allocator = free performance" | Only if its strategy fits your pattern (bump for scratch, pool for nodes); wrong fit can be worse |

---

## Exercises

1. **Node allocation count:** `std::list<int, std::allocator<int>>` with 1000
   `push_back`s — how many `allocate` calls? With a pool allocator handing out
   fixed blocks from a preallocated slab?

   <details><summary>Answer</summary>

   Default: **1000** `allocate(1)` calls (one node each) → 1000 `operator new`s.
   Pool: 0 during steady state — nodes come off the slab's free-list (push/pop),
   the slab was allocated once up front.
   </details>

2. **Why the converting ctor:** explain concretely why `std::map<int,int,
   std::less<>, A<std::pair<const int,int>>>` needs `A` to be constructible from
   `A<U>`.

   <details><summary>Answer</summary>

   The map stores tree **nodes** (`_Rb_tree_node<pair<const int,int>>`), not bare
   pairs. It rebinds `A<pair<const int,int>>` to `A<_Rb_tree_node<...>>` and
   constructs that from the original via the templated ctor. No converting ctor →
   the rebind can't be constructed → compile error.
   </details>

3. **operator== semantics:** two `ArenaAllocator<int>` instances, one wrapping
   arena X, one wrapping arena Y. `std::vector<int,Alloc> a` (arena X) and `b`
   (arena Y). What must `a = std::move(b);` do, and why can't it just steal b's
   pointer?

   <details><summary>Answer</summary>

   Since `a`'s allocator (X) `!=` `b`'s (Y) and the allocator doesn't propagate
   on move-assignment, `a` cannot own memory allocated from Y. It must
   `allocate` from X, move each element over, and free its old buffer — an O(n)
   move, not an O(1) pointer swap.
   </details>

4. **Arena sizing:** your per-event scratch peaks at ~40 KB across several
   `std::pmr`-style vectors. How big do you make the arena, and when do you
   `reset()`?

   <details><summary>Answer</summary>

   Size it to the **worst-case peak** with margin (e.g. 64–128 KB), allocated
   once at startup. `reset()` at the top (or bottom) of each event-processing
   iteration, when nothing from the previous event is still referenced — O(1),
   reclaims all of it.
   </details>

5. **EBO check:** does `std::vector<int, LoggingAllocator<int>>` (stateless) have
   a larger `sizeof` than `std::vector<int>`? What about with `ArenaAllocator`?

   <details><summary>Answer</summary>

   Stateless `LoggingAllocator` — no, empty-base optimization means it adds 0
   bytes; still 24. `ArenaAllocator` holds a `std::shared_ptr<Arena>` (16 bytes)
   → the vector grows to ~40 bytes.
   </details>

---

## Interview questions

1. Allocator kya provide karta — raw memory ya constructed objects?
2. C++17+ mein minimal allocator ki surface kya (5 cheezein)?
3. Templated converting ctor kyun zaroori (node containers)?
4. `operator==` allocator ka kya decide karta (move/swap)?
5. Arena/bump allocator — `allocate`/`deallocate`/`reset` kya karte?
6. Node container ke saath pool allocator N `new` calls ko kya banata?

---

## Next
→ [`24-memory-resource.md`](24-memory-resource.md)
