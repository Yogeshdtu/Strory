# 08 — Arena / bump allocators, monotonic buffer, PMR

## Prerequisites
- `06-memory-pools.md`, `07-object-pools.md`
- `19-STL/23-24` (custom allocators, PMR), `19-STL/examples/10_pmr_demo.cpp`
- `examples/04_arena_allocator.cpp`, `examples/05_pmr_containers.cpp`

## Yeh topic abhi kyun
Pool (06/07) ek **fixed size** deta. Arena har size deta — aur usse bhi
tez — bas ek trade-off ke saath: **individual free nahi hai.** Perfect jab
bahut se temporary objects ek scope (ek message, ek tick, ek frame) ke liye
chahiye aur us scope ke end pe sab ek saath ja sakte.

---

## Design: bump a pointer

```
  buffer:  [==================== capacity ====================]
           ^begin          ^cur                             ^end
           allocated       next allocation here             limit

  allocate(n, align):
     cur = align_up(cur, align)
     if (cur + n > end) return nullptr        // overflow
     p = cur; cur += n; return p              // <-- that's it

  reset():
     cur = begin                              // <-- frees EVERYTHING
```

```cpp
class Arena {
public:
    Arena(std::byte* buf, std::size_t cap) noexcept
        : begin_(buf), cur_(buf), end_(buf + cap) {}

    void* allocate(std::size_t n, std::size_t align = alignof(std::max_align_t)) noexcept {
        std::uintptr_t p = reinterpret_cast<std::uintptr_t>(cur_);
        std::uintptr_t a = (p + (align - 1)) & ~(align - 1);
        auto* out = reinterpret_cast<std::byte*>(a);
        if (out + n > end_) return nullptr;              // OVERFLOW — caller's policy
        cur_ = out + n;
        return out;
    }
    template <class T, class... Args> T* make(Args&&... args) noexcept {
        void* m = allocate(sizeof(T), alignof(T));
        return m ? ::new (m) T(std::forward<Args>(args)...) : nullptr;
    }
    void reset() noexcept { cur_ = begin_; }
    std::size_t used() const noexcept { return static_cast<std::size_t>(cur_ - begin_); }
private:
    std::byte *begin_, *cur_, *end_;
};
```

`allocate` = an align + a compare + an add. `reset` = one store. That's the
cheapest allocation that exists.

---

## Measured (`04_arena_allocator.cpp`, 12 temp objects per "message", is box)

```
                          p50      p99      p99.9      max
  new/delete per msg      501 ns   561 ns   641 ns   ~23 us
  arena + reset per msg    30 ns    40 ns    80 ns   ~10 us
```

- `new/delete` per msg: 12 × (`new` + `delete`) + a `std::vector<Field*>`
  that itself allocates → ~500 ns and a real tail.
- **arena + reset**: 12 `make<Field>()` bumps + **one `reset()`** →
  **30 ns**, p99.9 **80 ns** — ~16x faster, flat.
- The entire per-message temp-object lifecycle is now outside the allocator.

---

## Per-scope arena pattern (HFT)

```cpp
// one arena per hot thread, sized for the worst-case single message
alignas(64) static std::byte g_msg_buf[64 * 1024];
Arena g_msg_arena(g_msg_buf, sizeof g_msg_buf);

void on_message(const std::byte* wire, std::size_t len) {
    ParseCtx ctx{g_msg_arena};
    parse(wire, len, ctx);          // all temp nodes / fields from g_msg_arena
    handle(ctx);
    g_msg_arena.reset();            // <-- one instruction frees the whole parse
}
```

- Every temporary the parser needs (field lists, string copies, intermediate
  nodes) comes from the arena.
- After the message is handled, `reset()` — no per-object frees, no dtors
  (if the temps are trivially destructible), no allocator involvement.
- Next message reuses the same bytes → cache-warm.

---

## PMR: the arena as a standard `memory_resource`

`std::pmr::monotonic_buffer_resource` **is** a bump arena, standardized:

```cpp
alignas(64) std::array<std::byte, 16 * 1024> buf;
std::pmr::monotonic_buffer_resource mono(buf.data(), buf.size(),
                                         std::pmr::null_memory_resource());
std::pmr::vector<int>            v(&mono);      // grows from `buf`, no heap
std::pmr::unordered_map<int,int> m(&mono);      // nodes from `buf`
// ... use ...
mono.release();                                // rewind to the start of buf
```

- `null_memory_resource()` as the upstream → if `buf` overflows, `allocate`
  throws `std::bad_alloc` → a **runtime tripwire** for an unexpected
  allocation (`05_pmr_containers.cpp` demonstrates this).
- `mono.release()` = `reset()`; `mono` also frees its (non-existent, here)
  upstream blocks.
- **Measured (`05`)**: `pmr::vector` on a stack buffer p50 **40 ns** vs
  `std::vector` (new/delete) p50 **270 ns**, and **0 global `new`** over
  200k messages.

Trade-off vs a hand-rolled arena: the `pmr` indirection is **one virtual
call per allocation** (`do_allocate`), usually inlined and dwarfed by the
allocation itself — but it exists. For the absolute hottest path a
hand-rolled `Arena` with no virtual is marginally leaner; for everything
else, PMR gives you STL containers with arena performance for free.

---

## Trade-offs (honest)

| | |
|---|---|
| ❌ **no individual free** | an object's lifetime = the arena's scope. Can't free one temp early. |
| ❌ **non-trivial dtors not run** by `reset()` | if a temp owns something, `reset()` leaks it. Track destructors separately, or only put trivially-destructible things in the arena. |
| ❌ **overflow policy needed** | a huge message → arena full → `nullptr` / `bad_alloc`. Size for the worst case (05) + a fallback (a bigger secondary arena, or reject). |
| ❌ **one long-lived object poisons the arena** | if one allocation must outlive the scope, the whole arena can't reset. Keep long-lived things elsewhere. |
| ✅ **fastest allocation, flat tail, cache-warm reuse, any size** | why it's the default for per-message scratch. |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — a temp with a non-trivial dtor in the arena
`Arena::make<std::vector<int>>()` then `reset()` → the vector's heap buffer
leaks (its dtor never ran). Arena temps should be trivially destructible,
or you maintain a list of "things to destroy before reset".

### Trap 2 — a pointer into the arena outliving `reset()`
After `reset()`, arena memory is reused. A pointer/reference you kept is now
dangling → UAF. All arena pointers die with the scope.

### Trap 3 — under-sizing the arena
A rare large message overflows → `nullptr` unchecked → crash. Size from the
measured worst case + margin, and handle overflow (secondary arena / reject).

### Trap 4 — sharing an arena across threads
`allocate` bumps `cur_` non-atomically. One arena per thread.

### Trap 5 — `monotonic_buffer_resource` forgetting `release()`
Without `release()`, `mono` keeps bumping and (once `buf` is full) falls
back to its upstream → heap allocations (or `bad_alloc` with
`null_memory_resource`). `release()` per scope to rewind.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "arena = pool" | any size, no individual free; scope-lifetime |
| "reset frees the objects" | it rewinds a pointer; dtors don't run |
| "PMR is heavy" | one (inlined) virtual per allocation; near-hand-rolled speed |
| "keep this arena pointer for later" | dies at `reset()` — UAF |
| "16 KB arena is plenty" | size from the measured worst-case message + margin |

---

## Exercises

1. Ek parser arena se `std::pmr::string`s banata hai (field values). Ek
   field value ko message ke baad bhi rakhna hai (ek `last_seen_price`
   string). Direct `last_price_ = ctx.field("price")` (a pmr::string into
   the arena) kyun galat hai, aur fix?

   <details><summary>Answer</summary>

   `ctx.field("price")` is a `std::pmr::string` whose storage is in the
   message arena. When `on_message` calls `arena.reset()` (or
   `mono.release()`), that storage is reclaimed and reused by the next
   message. `last_price_` now points at (soon-to-be) garbage → a **dangling
   read**, and the next message's parse overwrites it → `last_price_` shows
   the wrong symbol's price.
   Fixes:
   (1) **Copy out of the arena**: `last_price_.assign(ctx.field("price"))`
   where `last_price_` is a normal `std::string` (its own heap) or a
   fixed `char[32]` buffer + length. The copy is one small memcpy, once per
   message — cheap, and it's on a value you actually keep.
   (2) Better for HFT: **don't keep it as a string at all** — parse the
   price to an integer (fixed-point ticks) at parse time and store the
   `int64_t`. No string, no allocation, no lifetime issue, and comparisons/
   arithmetic are free.
   Rule: anything that must outlive the arena scope is **copied out** (or,
   ideally, was never string-shaped).
   </details>

2. Tumhare `Arena::allocate` mein overflow pe `nullptr` return hota hai.
   Ek engineer ise "fix" karta by making it fall back to `::operator new`
   when full. Yeh HFT hot path pe kyun ek bura fix hai?

   <details><summary>Answer</summary>

   The point of the arena is a **flat, deterministic** allocation cost.
   A silent fallback to `::operator new` on overflow means: *most* messages
   are fast (30 ns), but the occasional large message that overflows the
   arena suddenly does a real `malloc` (tens to hundreds of ns, with the
   allocator tail — lesson 04) **plus** now you have a heap allocation that
   `reset()` won't reclaim → you must track it and `delete` it separately,
   or leak. So you've reintroduced exactly the jitter and the tail you built
   the arena to remove, on precisely the inputs (large messages) that are
   already the most expensive to process — worst possible correlation.
   Better: (a) **size the arena for the true worst case** so it never
   overflows in practice (measure from historical data), and treat an
   overflow as a **bug / hard error** (assert, alert), not a silent path;
   or (b) if large messages are legitimate and rare, have a **separate,
   pre-allocated large arena** for them (still no `malloc`), chosen up front
   from the message header size, so the cost is known and bounded; or (c)
   **reject / drop** the oversized message with a logged metric if your
   protocol allows. Never `malloc` on the hot path as a fallback.
   </details>

---

## Interview questions

1. Bump allocator — `allocate` aur `reset` ki exact cost.
2. Arena vs pool — kab kaunsa; arena ka core trade-off.
3. `monotonic_buffer_resource` — kya hai, `null_memory_resource` upstream ka faayda.
4. Non-trivial dtors + arena — kya problem, kaise handle.
5. Arena pointer outliving `reset()` — kyun UAF; kya copy out karna.
6. Overflow pe `malloc` fallback kyun bura on the hot path.

---

## Next
→ [`09-custom-allocators.md`](09-custom-allocators.md)
