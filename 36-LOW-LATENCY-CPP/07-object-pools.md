# 07 — Object pools: reuse, free list, construction cost

## Prerequisites
- `06-memory-pools.md` (raw fixed pool)
- `25-OBJECT-MODEL` (object lifetime, placement new, `std::launder`, trivial types)
- `18-COPY-MOVE` (a valid-but-unspecified state, reset semantics)
- `examples/03_object_pool.cpp`

## Yeh topic abhi kyun
Raw memory pool (06) sirf **bytes** deta. Ek **object pool** bytes + **object
lifetime** manage karta. Aur yahan ek design choice hai jiska measurable
cost hai: **har acquire pe construct karo, ya pehle se constructed objects
ko recycle karo?**

---

## Do strategies

### A. Construct on acquire
```
  acquire()  = free-list se slot lo, placement-new T in it
  release(p) = p->~T(), slot free-list pe wapas
```
- Clean semantics: har acquire pe ek fresh, fully-constructed T.
- **Cost: har acquire pe constructor** — agar T ka ctor real work karta
  (zero an array, init members, allocate a sub-buffer... wait, no allocation),
  woh har acquire pe.

### B. Recycle (pre-constructed)
```
  at startup: har slot mein ek T construct karo (construct_all())
  acquire()  = free-list se slot lo, return the (already-constructed) T*
               caller does a cheap reset() to overwrite the fields it needs
  release(p) = slot free-list pe wapas (NO dtor)
  at shutdown: har slot ka dtor (destroy_all())
```
- **Cost: zero constructor per acquire.** Just the pointer swap + your reset.
- **Danger**: the object comes back with **stale field values** from its
  last use. You must overwrite every field you read, or call a `reset()`.

---

## Measured (`03_object_pool.cpp`, `Order` with a 16-int array ctor, is box)

```
                          p50      p99      p99.9      max
  new Order()             50 ns   130 ns   160 ns   ~28 us
  pool + construct        30 ns    30 ns   110 ns   ~63 us
  pool + recycle + reset  20 ns    30 ns   100 ns   ~5 us
```

- `new Order()` — free-list fast path + the ctor; tail = allocator internals.
- **pool + construct** — no allocator tail, but the ctor (16-int fill) is
  visible: p50 30 ns vs recycle's 20 ns.
- **pool + recycle + reset** — fastest and flattest. `acquire` is ~free;
  the timed `reset(id, px, qty)` is a cheap explicit overwrite you control.
- (max values = OS-interrupt noise on the per-op timer, unpinned box — the
  p50 / p99 / p99.9 are the allocator/ctor signal.)

**Nichod:** agar T ka ctor non-trivial hai aur tum acquire ke turant baad
sab relevant fields overwrite karte ho anyway → **recycle**. The ctor work
was going to be thrown away.

---

## Kaunsa kab

| Situation | Strategy |
|---|---|
| T trivially constructible (POD-ish), ya ctor sasta | doesn't matter much; construct is cleaner |
| T ka ctor real work (zero buffers, init sub-structures) **aur** har use pe woh fields overwrite hote | **recycle** — save the ctor |
| T ka lifetime semantics matter (RAII members, invariants at all times) | **construct** — an object between `release` and next `acquire` should not be "alive" |
| T holds a resource (fd, a sub-allocation) | **construct/destroy** — or the pool leaks resources |
| you want the object to be safe to inspect even when "free" | **recycle** with a defined idle state |

HFT: mostly **recycle** — `Order` / `MarketDataEvent` / `Message` have a
fixed schema, every field is set from the wire on each use, and the ctor
(memset-like) is pure waste. But be disciplined: a `reset()` method, or a
review rule that every field is written before read.

---

## A clean object pool (both strategies, zero heap after construction)

```cpp
template <class T, std::size_t Cap>
class ObjectPool {
    struct alignas(T) Slot { std::byte mem[sizeof(T)]; };
    std::array<Slot, Cap>  storage_{};
    std::array<Slot*, Cap> free_{};                  // free-list as an array stack
    std::size_t top_ = 0;
public:
    ObjectPool() { for (std::size_t i = 0; i < Cap; ++i) free_[top_++] = &storage_[i]; }

    template <class... A> T* acquire_construct(A&&... a) {
        if (!top_) return nullptr;
        return ::new (free_[--top_]->mem) T(std::forward<A>(a)...);
    }
    void release_destroy(T* p) noexcept { p->~T(); free_[top_++] = reinterpret_cast<Slot*>(p); }

    void construct_all() { for (auto& s : storage_) ::new (s.mem) T(); }
    T*   acquire_recycle() noexcept {
        return top_ ? std::launder(reinterpret_cast<T*>(free_[--top_]->mem)) : nullptr;
    }
    void release_recycle(T* p) noexcept { free_[top_++] = reinterpret_cast<Slot*>(p); }
    void destroy_all() noexcept { for (auto& s : storage_) std::launder(reinterpret_cast<T*>(s.mem))->~T(); }
};
```

(The `03` example threads the free-list through a separate `free_` array for
readability + so both strategies work; a production single-strategy pool
threads it through the slot memory like `06` for zero extra storage.)

`std::launder` after `reinterpret_cast` from the byte buffer: tells the
compiler "a `T` really does live here now" so it doesn't assume otherwise
from the storage's declared type (25/02).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — recycle bina reset
Object comes back with last use's fields. Read a stale `id` / `price` /
flag → a wrong order, a phantom position. **Every field written before
read**, or a `reset()` you always call.

### Trap 2 — recycle a resource-owning object
If `T` owns an fd / a sub-buffer / a `unique_ptr`, recycling without
destroy → the resource from the previous use is still held (leak) or
double-released. Recycle only value-like objects.

### Trap 3 — forgetting `destroy_all()` at shutdown (recycle mode)
Constructed once, never destroyed → dtors don't run. Fine for trivially-
destructible `T`; a real leak / missed side effect otherwise.

### Trap 4 — `std::launder` omission
`reinterpret_cast<T*>(buf)` without `launder` after a placement-new into a
differently-typed buffer is technically UB the compiler may exploit (25/02).
Use `launder`, or store `T` directly with a `union`/`std::optional`-like
wrapper.

### Trap 5 — acquire returns `nullptr`, unchecked
Same as the raw pool — a full pool + an unchecked deref = crash. `[[unlikely]]`
branch + policy at every call site.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "pool + construct is always right" | if the ctor's work is overwritten anyway → recycle |
| "recycle = just skip the ctor" | + you MUST reset / overwrite every field read |
| "recycle any object" | only value-like; resource-owners need construct/destroy |
| "no need for `launder`" | placement-new into a byte buffer → `launder` the cast |
| "the pool is full but that's a crash" | policy: drop / reject / prove it can't fill |

---

## Exercises

1. `Order` ka ctor: `id{0}, price{0}, qty{0}, side{Side::Buy}, filled{0},
   fills_[64]{}` (zero a 64-element array). Hot path har acquire ke baad
   sets `id, price, qty, side` from the wire, but **not** `filled` or
   `fills_` (those get filled later as fills arrive). Recycle strategy use
   karein? Kya reset chahiye?

   <details><summary>Answer</summary>

   Yes, recycle — but with a **partial reset**. The ctor zeros `fills_[64]`
   (256 bytes) every acquire, which under construct-on-acquire is pure waste
   *if* it were always overwritten — but here `filled` and `fills_` are
   **not** set on acquire, they accumulate later. So:
   - If you recycle without resetting `filled`/`fills_`, an order reused
     from a previous life comes back with **stale fill data** → you think
     it's partially filled, or you append fills after garbage.
   - So `reset()` must zero `filled` and `fills_` (or at least `filled`, and
     then only read `fills_[0..filled)`). That's back to ~the ctor's cost
     for that part.
   Better design: **don't store fills inline**. Keep `Order` small (just the
   wire fields), and put fills in a separate arena/pool keyed by order id,
   allocated only when the first fill arrives. Then `Order` recycle is a
   cheap 4-field reset, and the 256-byte array isn't touched on the hot path
   at all. (Data-oriented: separate the hot fields from the cold/rare ones —
   lesson 10, folder 32.)
   </details>

2. Ek object pool ka `acquire_recycle()` return karta hai `std::launder(
   reinterpret_cast<T*>(slot->mem))`. Ek reviewer poochta "`launder` kyun —
   `reinterpret_cast` kaafi nahi?" Jawab.

   <details><summary>Answer</summary>

   `slot->mem` is declared as `std::byte[sizeof(T)]`. After
   `::new (slot->mem) T(...)` a `T` object *lives* there, but the compiler,
   looking at the source, still sees the storage's declared type as
   `std::byte[]`. A plain `reinterpret_cast<T*>(slot->mem)` gives you a
   pointer of the right type, but the compiler is allowed to assume the
   *object* it points to is still the `std::byte` array (or nothing) — it
   may cache/reorder/elide loads based on that assumption → UB, wrong codegen
   in aggressive builds. `std::launder(p)` is a compiler barrier that says
   "the object at this address may have changed type / been replaced —
   reload through this pointer honestly." So: placement-new **creates** the
   object; `reinterpret_cast` **types** the pointer; `std::launder`
   **tells the optimizer** to trust it. All three needed when you manage
   object lifetime inside raw storage of a different type. (Alternatives that
   avoid the dance: store `union { T value; }` or an aligned `std::optional`-
   like wrapper so the storage's type *is* `T`.)
   </details>

---

## Interview questions

1. Construct-on-acquire vs recycle — the trade-off, and when the ctor work is "waste".
2. Recycle mode ke dangers — stale fields, resource-owning objects, shutdown dtors.
3. `std::launder` — placement new ke saath kyun zaroori.
4. Object pool + non-trivial dtor — release pe kya karna.
5. Hot / cold field separation — recycle ko sasta banane ke liye.

---

## Next
→ [`08-arena-allocators.md`](08-arena-allocators.md)
