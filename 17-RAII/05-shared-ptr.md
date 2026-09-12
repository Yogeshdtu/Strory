# 05 — `std::shared_ptr`

## Prerequisites
- [`04-unique-ptr.md`](04-unique-ptr.md)
- Folder 14 file 08 (allocation cost), folder 27 preview (atomics — bas "atomic = thread-safe but costs" idea)

## Yeh topic abhi kyun
`std::shared_ptr<T>` = **shared ownership** via reference counting. Multiple
owners, object tabhi delete jab **last** owner gaya. Yeh convenient hai par
**not free**: har copy/destroy ek **atomic** refcount operation hai, `sizeof`
double hai, aur `shared_ptr(new T)` do allocations. Kab use karna, kab nahi.

---

## Basics

```cpp
auto a = std::make_shared<Widget>(1, 2);   // Widget + control block, a owns (count 1)
auto b = a;                                  // COPY -> count 2 (both own)
{
    auto c = b;                              // count 3
    c->method();
}                                            // ~c -> count 2
// ~b, ~a -> count 1, 0 -> Widget deleted
```

- Copyable (unlike `unique_ptr`). Each copy = one more owner.
- Object deleted when the **strong count** hits 0.
- `a.use_count()` — current strong count (debugging; racy in MT).

---

## The control block

```
   shared_ptr a:  [ ptr -> Widget ] [ ctrl -> control block ]
   shared_ptr b:  [ ptr -> Widget ] [ ctrl -> SAME control block ]

   control block:  { strong_count, weak_count, deleter, allocator, [the object if make_shared] }
```

- **`sizeof(shared_ptr<T>) == 2 * sizeof(void*)`** (16 on 64-bit) — the object
  pointer **and** the control-block pointer.
- The control block holds:
  - **strong count** — how many `shared_ptr` own it. 0 → destroy the object.
  - **weak count** — how many `weak_ptr` (+ 1 while strong > 0). 0 → free the
    control block itself.
  - the **deleter** and **allocator** (type-erased — so `shared_ptr<T>` size
    doesn't change with a custom deleter, unlike `unique_ptr`).

---

## `make_shared` vs `shared_ptr(new T)` — one alloc vs two

```cpp
auto a = std::make_shared<Widget>(args);     // ✅ ONE allocation: object + control block together
std::shared_ptr<Widget> b(new Widget(args)); // ⚠️ TWO: `new Widget` + separate control block
```

`examples/03_shared_ptr.cpp` measures this with a counted `operator new`:
`make_shared` → 1 `new`, `shared_ptr(new T)` → 2.

**`make_shared` advantages:**
- 1 allocation instead of 2 (faster, `examples/07`: ~1.74x).
- Object + refcounts in **one cache line** neighbourhood → better locality.
- Exception-safe (no leak if control-block alloc would throw).

**`make_shared` caveat:** object + control block share **one** allocation → the
memory is freed only when **both** strong AND weak counts hit 0. So a lingering
`weak_ptr` keeps the whole (object-sized) block alive even after the object is
destroyed. For huge objects with long-lived `weak_ptr`s, `shared_ptr(new T)`
(separate blocks) can be better. Rare.

---

## Passing `shared_ptr` around — the cost

```cpp
void observe(std::shared_ptr<Widget> w);        // ⚠️ BY VALUE -> a copy -> atomic ++ then atomic -- per call
void observe(const std::shared_ptr<Widget>& w); // ✅ by const ref -> no refcount touch
void observe(const Widget& w);                  // ✅✅ if the callee doesn't need ownership at all
void observe(Widget* w);                        // ✅✅ raw non-owning view
```

Every `shared_ptr` **copy** is an **atomic increment**; every **destroy** an
**atomic decrement** (+ a check for 0). Atomics are:
- ~10-30x a plain integer op even uncontended (`examples/07`: shared copy ~33 ns
  vs raw copy ~0.4 ns → ~90x).
- Much worse **contended** (multiple threads copying the same `shared_ptr` →
  cache-line ping-pong on the count).

**Rule:** pass `shared_ptr` **by value only when the callee genuinely takes/
shares ownership** (stores it somewhere). To just *use* the object, take
`const T&` / `T*` / `const shared_ptr&`.

---

## `enable_shared_from_this` — getting a `shared_ptr` to `this`

```cpp
struct Session : std::enable_shared_from_this<Session> {
    void startAsync() {
        auto self = shared_from_this();          // a shared_ptr<Session> to *this
        io.post([self] { self->handle(); });     // keeps *this alive until the callback runs
    }
};
auto s = std::make_shared<Session>();
s->startAsync();
```

If an object needs to hand out `shared_ptr`s to itself (async callbacks that must
keep it alive), derive from `enable_shared_from_this<T>` and call
`shared_from_this()` — **only valid if the object is already owned by a
`shared_ptr`** (else UB / `std::bad_weak_ptr`). Don't `shared_ptr<T>(this)` — that
creates a **second** control block → double-free.

---

## When to use `shared_ptr` (rarely)

✅ **Genuine shared ownership with unclear lifetime:**
- A cache entry used by multiple subsystems, deleted when the last drops it.
- Nodes in a graph with no single owner.
- Objects captured by multiple async callbacks (`enable_shared_from_this`).

❌ **Not for:**
- A single owner → `unique_ptr`.
- "I don't know who owns it" → figure out ownership; that's a design smell, not
  a `shared_ptr` use case.
- Passing to functions that just read → `const T&`.
- Hot-path per-message objects → pool + raw handles (folder 14).
- Cycles → you'll leak; need `weak_ptr` (file 06).

**Herb Sutter:** "`shared_ptr` is for genuinely shared ownership — which is less
common than people think."

---

## Andar kya hota hai

- **Atomic refcount:** `strong_count` is a `std::atomic<long>` (or similar).
  Copy → `count.fetch_add(1, relaxed)`. Destroy → `count.fetch_sub(1,
  acq_rel)`; if it was 1, run the deleter, then decrement the weak count.
  `fetch_add`/`fetch_sub` = a `lock xadd` on x86 (~15-25 cycles uncontended,
  serializing).
- **`make_shared`:** allocates `sizeof(control block + T)` in one go, placement-
  news the `T` inside, control block points at it. Deleter for this layout just
  runs `T::~T()` (no `operator delete` for the object — the whole block is freed
  when weak count also hits 0).
- **Type-erased deleter:** the control block stores the deleter as part of a
  polymorphic (or function-pointer) structure allocated with it — so
  `shared_ptr<T>`'s own size never changes, but `shared_ptr(new T, myDeleter)`
  may allocate more for the deleter.
- Move (`shared_ptr b = std::move(a)`) — **no atomic op**, just pointer steals +
  null out. Cheap. Prefer moving `shared_ptr` where possible.

> **HFT relevance:** `shared_ptr` is largely **kept out of the hot path** — the
> atomic refcount is a serializing operation, and under multi-thread sharing the
> count's cache line ping-pongs between cores (false-sharing-like contention).
> A hot loop that copies a `shared_ptr` per iteration is a real, measurable
> slowdown (~90x a raw copy, before contention). Where shared lifetime is
> genuinely needed (a market-data snapshot handed to several strategy threads),
> the `shared_ptr` is acquired **once** outside the loop and the loop uses a raw
> `const T*` / `const T&` into it. `make_shared` (1 alloc, better locality) is
> the default when a `shared_ptr` is warranted at all. Often the better answer
> is a different design: immutable snapshots + sequence numbers, or a
> single-writer object others read without owning.

---

## Hands-on

```bash
./build.ps1 17-RAII/examples/03_shared_ptr.cpp
./build.ps1 fast 17-RAII/examples/07_smartptr_benchmark.cpp   # shared copy ~90x raw copy
```

`03` — refcount lifecycle, `use_count`, `make_shared` (1 alloc) vs `shared_ptr(new)`
(2 allocs), `sizeof` 16.

---

## ⚠️ Traps

### Trap 1 — `shared_ptr` by value in a hot loop / hot function
```cpp
for (...) helper(sharedThing);   // ⚠️ atomic ++/-- per call. const shared_ptr& or const T&
```

### Trap 2 — `shared_ptr<T>(this)`
```cpp
struct S { auto self() { return std::shared_ptr<S>(this); } };   // ⚠️ 2nd control block -> double-free.
                                                                 //    enable_shared_from_this + shared_from_this()
```

### Trap 3 — `shared_ptr` where `unique_ptr` fits
```cpp
std::shared_ptr<Config> cfg = load();   // ⚠️ one owner -> unique_ptr. shared = needless atomics + 2x size
```

### Trap 4 — cycles
```cpp
a->next = b;  b->prev = a;   // ⚠️ both shared_ptr -> refcount never 0 -> leak (file 06, weak_ptr)
```

### Trap 5 — `use_count()` for logic
```cpp
if (p.use_count() == 1) { /* I'm the only owner */ }   // ⚠️ racy in MT; another thread may copy right after
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`shared_ptr` is the default smart pointer" | `unique_ptr` is; `shared_ptr` only for genuine sharing |
| "Copying a `shared_ptr` is cheap" | Atomic inc + (on destroy) atomic dec — ~90x a raw copy |
| "`make_shared` and `shared_ptr(new T)` are equivalent" | 1 alloc vs 2; locality; exception-safety |
| "`sizeof(shared_ptr)` == 8" | 16 — object ptr + control-block ptr |
| "`shared_ptr<T>(this)` gives a shared_ptr to me" | Second control block → double-free; use `enable_shared_from_this` |

---

## Exercises

1. **use_count trace:** `auto a = std::make_shared<int>(1); { auto b = a; auto c
   = b; f(a); }` where `void f(std::shared_ptr<int> x)` — print `use_count()`
   at each step (before/inside/after `f`, after the block).

   <details><summary>Answer</summary>

   After make: 1. `b = a`: 2. `c = b`: 3. Inside `f` (by value → copy): 4. After
   `f` returns: 3. After the block (`c`, `b` gone): 1.
   </details>

2. **alloc count:** counted `operator new` — `auto a = std::make_shared<Big>();`
   vs `std::shared_ptr<Big> b(new Big);` — how many `new` calls each? Why?

   <details><summary>Answer</summary>

   `make_shared` → 1 (object + control block in one allocation). `shared_ptr(new
   Big)` → 2 (`new Big`, then a separate control-block allocation).
   </details>

3. **Fix the signature:** `double totalValue(std::shared_ptr<Portfolio> p)` is
   called 1M times in a loop and only reads `p`. Rewrite the signature; what
   does it save?

   <details><summary>Answer</summary>

   `double totalValue(const Portfolio& p)` (or `const Portfolio*`). Saves 1M
   atomic increments + 1M atomic decrements (~30 ns each here) → tens of ms, plus
   avoids cache-line contention if multi-threaded.
   </details>

4. **shared_from_this:** why is `std::shared_ptr<S>(this)` inside a member
   function a bug, and how does `enable_shared_from_this` fix it?

   <details><summary>Answer</summary>

   `shared_ptr<S>(this)` makes a **new** control block unaware of the existing
   one → when both hit 0, the object is deleted twice. `enable_shared_from_this`
   stores a `weak_ptr` to the *original* control block (set when the first
   `shared_ptr` is made) and `shared_from_this()` returns a `shared_ptr` sharing
   that block.
   </details>

5. **move vs copy:** `std::shared_ptr<T> b = std::move(a);` vs `= a;` — which
   touches the atomic refcount? What's `a` after each?

   <details><summary>Answer</summary>

   `= a` (copy) → atomic increment; `a` still valid (count higher). `=
   std::move(a)` → **no atomic op**, just pointer steal; `a` becomes `nullptr`.
   Prefer move when you don't need `a` afterwards.
   </details>

---

## Interview questions

1. `shared_ptr` ownership model, `sizeof`, control block ke contents?
2. `make_shared` vs `shared_ptr(new T)` — 3 differences?
3. `shared_ptr` copy ki cost — kya aur kyun (atomic)?
4. `shared_ptr` by value kab pass karo, kab nahi?
5. `enable_shared_from_this` — kya problem solve karta?
6. `shared_ptr` ko HFT hot path se kyun door rakha jaata?

---

## Next
→ [`06-weak-ptr.md`](06-weak-ptr.md)
