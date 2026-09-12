# 24 — `<memory_resource>` (PMR): polymorphic allocators

## Prerequisites
- [`23-allocators.md`](23-allocators.md) (the classic allocator model — PMR is its runtime-polymorphic cousin)
- Folder 14 (heap), [`17-chrono.md`](17-chrono.md) (measuring)

## Yeh topic abhi kyun
Classic allocators (file 23) ka **do problem**: (1) allocator type ka hissa hai —
`std::vector<int, ArenaAlloc<int>>` aur `std::vector<int>` alag types, API friction.
(2) har custom allocator ek template. C++17 ka **PMR** dono theek karta: **ek**
container type (`std::pmr::vector<int>`), allocation strategy ek **runtime object**
(`memory_resource*`). Stack buffer pe zero-heap containers isi se milte.

`examples/10_pmr_demo.cpp` ye sab measure karta (global `operator new` count).

---

## The idea

```cpp
#include <memory_resource>
namespace pmr = std::pmr;
```

- **`std::pmr::memory_resource`** — an abstract base with virtual
  `do_allocate(bytes, align)` / `do_deallocate(...)` / `do_is_equal(...)`. Any
  concrete strategy derives from it.
- **`std::pmr::polymorphic_allocator<T>`** — a *concrete* allocator that holds a
  `memory_resource*` and forwards to it (through the virtual calls). This is the
  single allocator type all `pmr` containers use.
- **`std::pmr::vector<T>`** = `std::vector<T, std::pmr::polymorphic_allocator<T>>`.
  Same for `pmr::string`, `pmr::map`, `pmr::unordered_map`, `pmr::deque`, ...

So the container type is fixed; you pass a `memory_resource*` at **construction**
to choose where memory comes from. One `void process(std::pmr::vector<int>&)`
accepts a vector backed by the heap, an arena, or a stack buffer.

---

## The standard resources

```cpp
// 1. monotonic_buffer_resource -- bump allocator. deallocate() is a no-op;
//    everything is freed when the resource is destroyed or .release()d. FASTEST.
std::byte buffer[64 * 1024];
std::pmr::monotonic_buffer_resource pool{buffer, sizeof buffer};   // start from a STACK buffer
std::pmr::vector<int> v{&pool};                                     // all of v's allocations come from `buffer` -> ZERO heap
v.reserve(1000);                                                    // served from the stack buffer

// 2. unsynchronized_pool_resource / synchronized_pool_resource --
//    pool of fixed-size blocks (good for node containers). deallocate() actually recycles.
std::pmr::unsynchronized_pool_resource pool2;                       // single-thread; _synchronized_ adds a mutex
std::pmr::list<int> lst{&pool2};

// 3. new_delete_resource() -- the default: forwards to ::operator new/delete
// 4. null_memory_resource() -- throws std::bad_alloc on ANY allocation
```

### Chaining — the "upstream"

Every resource has an **upstream** resource it falls back to when its own storage
is exhausted:

```cpp
std::byte buf[4096];
// when buf is used up, DON'T go to the heap -- fail loudly instead:
std::pmr::monotonic_buffer_resource pool{buf, sizeof buf, std::pmr::null_memory_resource()};
std::pmr::vector<int> v{&pool};
// v grows within buf silently; the moment it needs more than 4096 B -> std::bad_alloc
```

`null_memory_resource()` as upstream is a **hard assertion of "no heap
allocation"** — great for a hot path you've sized for. Default upstream is
`new_delete_resource()` (spill to the heap).

### `.release()` — reuse a monotonic buffer

```cpp
std::pmr::monotonic_buffer_resource pool{buf, sizeof buf, std::pmr::null_memory_resource()};
for (int iter = 0; iter < 100; ++iter) {
    std::pmr::vector<int> scratch{&pool};
    // ... fill and use scratch ...
    pool.release();          // rewind the buffer to empty -- O(1). Next iteration reuses the same bytes
}
// 100 iterations, 0 heap allocations total
```

`examples/10` runs exactly this loop and shows the global `operator new` counter
staying at 0.

---

## Default resource & passing it around

```cpp
std::pmr::set_default_resource(&myPool);          // process-wide default for pmr containers created without an explicit resource
auto* r = std::pmr::get_default_resource();

// PMR containers propagate their resource to elements that are ALSO pmr-aware:
std::pmr::vector<std::pmr::string> names{&pool};
names.emplace_back("hello");     // the string's chars ALSO come from &pool  -- uses_allocator construction
```

`std::pmr::vector<std::string>` (note: **non**-pmr `std::string`) — the vector's
array comes from the pool, but each `std::string`'s heap buffer does **not**. Use
`std::pmr::string` for full propagation.

**Gotcha:** a `pmr` container's allocator does **not** propagate on
copy/move-assignment (`polymorphic_allocator` has all propagation traits =
`false`). A moved-from-into container keeps its **own** resource — a move across
resources copies element-by-element, not a pointer steal. Two `pmr::vector`s with
different resources → move assignment is O(n).

---

## Andar kya hota hai

- `polymorphic_allocator<T>::allocate(n)` → `resource->allocate(n*sizeof(T),
  alignof(T))` → a **virtual call** into the concrete resource. That's one
  indirect call per allocation — negligible next to what an allocation costs, and
  zero when the container doesn't allocate.
- `monotonic_buffer_resource`: holds `cur`, `end`, and the initial buffer +
  upstream. `do_allocate` = align `cur`, check against `end`, return and bump;
  on overflow, ask `upstream` for a bigger block (geometrically growing) and
  allocate from that. `do_deallocate` = **nothing**. `release()` = reset `cur` to
  the start and free any upstream-obtained blocks.
- `pool_resource`: maintains free-lists of several fixed block sizes; large
  requests go straight to upstream. `do_deallocate` pushes the block back on its
  size-class list. Amortizes many small same-size allocations (list/map nodes)
  into few upstream calls.
- The stack-buffer pattern works because `monotonic_buffer_resource` can be
  *seeded* with memory you already own (`{buffer, size}`) — the container then
  literally builds its dynamic array inside your `std::byte[]`.

> **HFT relevance:** PMR is the pragmatic middle ground — you keep normal-looking
> `std::pmr::vector` / `std::pmr::unordered_map` code, but back it with a
> `monotonic_buffer_resource` over a **stack (or statically reserved) buffer** and
> `null_memory_resource()` upstream, so the hot path provably never calls the
> allocator (it `bad_alloc`s in testing if you undersized it). `release()` at the
> top of each event loop reuses the buffer with an O(1) pointer rewind — no
> per-cycle allocation, no fragmentation, deterministic. `pool_resource` covers
> the node-container cases. The virtual-call indirection is the price; it's
> irrelevant because the whole point is that allocation barely happens. Downside
> vs a hand-rolled template allocator: the vtable indirection and slightly less
> the compiler can inline — usually a non-issue.

---

## Hands-on

```bash
./build.ps1 19-STL/examples/10_pmr_demo.cpp
```

It counts global `operator new` and shows:
- plain `std::vector<int>` filling to 1000 → ~11 heap allocations (growth).
- `std::pmr::vector<int>` + `std::pmr::vector<std::pmr::string>` on a 64 KB stack
  buffer → **0** global `new`.
- `null_memory_resource()` upstream → `std::bad_alloc` the moment the buffer
  overflows (the "no allocation" assertion).
- `pool.release()` across 100 iterations → still 0 heap allocations.

---

## ⚠️ Traps

### Trap 1 — the buffer resource outliving... no, the reverse: container outliving the buffer
```cpp
std::pmr::vector<int> makeVec() {
    std::byte buf[4096];
    std::pmr::monotonic_buffer_resource pool{buf, sizeof buf};
    return std::pmr::vector<int>{&pool};   // ⚠️ buf and pool die at return; the vector now points at dead memory
}
```

### Trap 2 — resource destroyed before the container
```cpp
std::pmr::vector<int>* v;
{ std::pmr::monotonic_buffer_resource pool{buf, n}; v = new std::pmr::vector<int>{&pool}; }
v->push_back(1);   // ⚠️ pool gone -> UB. The resource must outlive every container using it
```

### Trap 3 — expecting `pmr` allocator to propagate on move
```cpp
std::pmr::vector<int> a{&poolA}, b{&poolB};
a = std::move(b);   // ⚠️ a keeps poolA; this is an element-by-element move, not O(1). (polymorphic_allocator doesn't propagate)
```

### Trap 4 — `std::pmr::vector<std::string>` and thinking strings use the pool
```cpp
std::pmr::vector<std::string> v{&pool};
v.emplace_back(bigText);   // ⚠️ the vector's array is in the pool; each std::string's buffer is on the HEAP. Use std::pmr::string
```

### Trap 5 — monotonic resource never reclaiming within its lifetime
```cpp
std::pmr::monotonic_buffer_resource pool;      // heap upstream, no fixed buffer
std::pmr::vector<int> v{&pool};
for (...) { v.clear(); v.resize(bigN); }       // ⚠️ each resize's allocation is never freed until `pool` dies -> unbounded growth. Use release(), or a pool_resource
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::pmr::vector<int>` is a different template from `std::vector`" | It's `std::vector<int, polymorphic_allocator<int>>` — a fixed alias; strategy is a runtime `memory_resource*` |
| "`monotonic_buffer_resource::deallocate` frees memory" | No-op — memory is reclaimed only on destruction or `release()` |
| "PMR containers share a resource on move" | `polymorphic_allocator` never propagates — cross-resource move is O(n) |
| "`std::pmr::vector<std::string>` allocates strings from the pool" | Only the vector's array; use `std::pmr::string` for the elements too |
| "PMR is slower because of virtual calls" | One indirect call *per allocation* — negligible; the win is that allocation stops happening |

---

## Exercises

1. **Zero-heap scratch:** write a function that builds a `std::pmr::vector<int>`
   of up to 500 ints on a stack buffer, asserting (via the type system / a
   resource) that it never touches the heap.

   <details><summary>Answer</summary>

   `std::byte buf[500 * sizeof(int) + 64]; std::pmr::monotonic_buffer_resource
   pool{buf, sizeof buf, std::pmr::null_memory_resource()};
   std::pmr::vector<int> v{&pool}; v.reserve(500); /* fill */` — any allocation
   beyond `buf` throws `std::bad_alloc` because upstream is `null_memory_resource`.
   </details>

2. **Lifetime order:** list the correct declaration order for `buffer`, the
   `monotonic_buffer_resource`, and the `std::pmr::vector` using it, and why.

   <details><summary>Answer</summary>

   `buffer` first, then the `monotonic_buffer_resource` (it references `buffer`),
   then the `std::pmr::vector` (it references the resource). Destruction is
   reverse: vector → resource → buffer. Any other order → a container or resource
   outlives its backing storage → UB.
   </details>

3. **release() loop:** rewrite this to do 0 heap allocations after the first
   iteration: `for (auto& ev : events) { std::vector<Node> tmp; build(tmp, ev);
   use(tmp); }`.

   <details><summary>Answer</summary>

   `std::byte buf[BIG]; std::pmr::monotonic_buffer_resource pool{buf, sizeof
   buf}; for (auto& ev : events) { std::pmr::vector<Node> tmp{&pool}; build(tmp,
   ev); use(tmp); pool.release(); }` — `release()` rewinds the buffer each
   iteration; nothing hits the heap once `buf` is sized for the peak.
   </details>

4. **Which resource:** (a) per-event scratch vectors, sized-boundable. (b) a
   `std::pmr::unordered_map` with lots of insert/erase churn. (c) a long-lived
   container where you want normal heap behaviour but PMR-typed for API
   uniformity.

   <details><summary>Answer</summary>

   (a) `monotonic_buffer_resource` over a fixed buffer + `release()` per event.
   (b) `unsynchronized_pool_resource` (recycles fixed-size node blocks). (c)
   `std::pmr::get_default_resource()` / `new_delete_resource()` — heap-backed.
   </details>

5. **Propagation surprise:** `std::pmr::string a{&poolA}; std::pmr::string b =
   std::move(a);` — where does `b` get its memory, and is `a`'s buffer stolen?

   <details><summary>Answer</summary>

   `b` is constructed with the **default** resource (move *construction* doesn't
   copy the source's allocator for `polymorphic_allocator`), so `b`'s chars come
   from `get_default_resource()`. Since `b`'s resource ≠ `a`'s, the buffer is
   **not** stolen — `b` allocates and copies the characters. (Move *assignment*
   between differing resources is likewise a copy.)
   </details>

---

## Interview questions

1. PMR classic allocators ke kaunse 2 problems solve karta?
2. `memory_resource` vs `polymorphic_allocator` — kaun abstract, kaun concrete?
3. `monotonic_buffer_resource` — `deallocate` kya karta, memory kab free hoti?
4. `null_memory_resource()` upstream ka kya matlab, kab use?
5. `polymorphic_allocator` move pe propagate karta? Iska move-assign pe asar?
6. `std::pmr::vector<std::string>` vs `std::pmr::vector<std::pmr::string>` — fark?

---

## Next
→ [`25-container-performance.md`](25-container-performance.md)
