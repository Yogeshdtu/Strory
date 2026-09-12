# 11 — Smart pointer performance (measured)

## Prerequisites
- [`04-unique-ptr.md`](04-unique-ptr.md), [`05-shared-ptr.md`](05-shared-ptr.md)
- Folder 14 file 08 (allocation cost), folder 16 file 12 (measuring dispatch)

## Yeh topic abhi kyun
"Smart pointers slow hote hain" ek common myth hai. Sach: **`unique_ptr` bilkul
free hai** (raw pointer jitna), aur **`shared_ptr` free nahi hai** (atomic
refcount, 2x size, 2 allocs with `new`). Yeh lesson `examples/07_smartptr_benchmark.cpp`
ke measured numbers ke saath — HFT ke liye kaunsa smart pointer kahan.

---

## `unique_ptr` — zero overhead (measured)

`examples/07_smartptr_benchmark.cpp` (`-O2`, GCC 15.1):

```
=== A) create + destroy one Obj ===
  raw new + delete           ~146 ns/op
  make_unique<Obj>           ~144 ns/op     -> unique_ptr / raw : 0.99x
```

```
=== B) copy / access an existing pointer in a hot loop ===
  raw pointer copy            ~0.4 ns/op
  unique_ptr .get()           ~0.7 ns/op     -> basically a mov
```

- **Create/destroy:** `make_unique` == `new`+`delete` (the allocation dominates;
  the `unique_ptr` wrapper adds nothing measurable).
- **Access:** `.get()` / `operator->` / `operator*` are inlined to a load —
  identical to using a raw pointer.
- **Size:** `sizeof(unique_ptr<T>) == 8` (default deleter → EBO).
- **Move:** 2 pointer moves, inlined.

`unique_ptr` is a **zero-cost abstraction** in the literal sense — you cannot
hand-write faster ownership management. Use it freely.

---

## `shared_ptr` — real costs (measured)

```
=== A) create + destroy ===
  make_shared<Obj>            ~140 ns/op     -> ~raw (+ control-block init)
  shared_ptr<Obj>(new Obj)    ~244 ns/op     -> ~1.74x make_shared  (2 allocations)

=== B) copy an existing shared_ptr in a hot loop ===
  raw pointer copy            ~0.4 ns/op
  shared_ptr copy + destroy   ~34 ns/op      -> ~90x a raw copy
```

Three distinct costs:

### 1. Size — 2x

`sizeof(shared_ptr<T>) == 16` (object ptr + control-block ptr). Matters for
cache-line density if you store many of them, and for function-argument
registers.

### 2. Allocation — 1 vs 2

`make_shared` → **1** allocation (object + control block together, better
locality). `shared_ptr<T>(new T)` → **2** (object, then a separate control block)
→ ~1.74x slower here. **Always `make_shared`** (unless you need a custom deleter
or the huge-object + long-weak-ptr case — file 05/06).

### 3. Refcount atomics — the big one

Every **copy** is an atomic increment; every **destroy** is an atomic decrement
(+ a compare-with-0). Measured: **~34 ns per copy+destroy pair vs ~0.4 ns for a
raw copy → ~90x.** And that's **uncontended** (single thread). Under contention
(multiple threads copying the same `shared_ptr`), the count's cache line
ping-pongs between cores → far worse (100s of ns, plus it stalls the other
cores).

**Move** a `shared_ptr` (instead of copy) → **no atomic** → cheap. Prefer
`std::move` when transferring.

---

## The rules that fall out

| Situation | Do |
|---|---|
| One owner, heap needed | `unique_ptr` (free) |
| Just read the object in a function | `const T&` / `T*` — **not** any smart pointer by value |
| Pass a `shared_ptr` you won't keep | `const shared_ptr<T>&`, or move it |
| Genuinely shared, unclear lifetime | `make_shared`, acquire **once** outside hot loops |
| Hot-path per-item objects | pool + raw handles (folder 14) — no smart pointer at all |
| Container of polymorphic objects | `vector<unique_ptr<Base>>` (not `shared_ptr` unless shared) |

### The classic mistake

```cpp
// ❌ shared_ptr by value, called per market-data tick
void onTick(std::shared_ptr<Book> book) {   // atomic ++ on entry, atomic -- on exit, EVERY tick
    update(*book);
}

// ✅
void onTick(Book& book) { update(book); }    // zero overhead; the shared_ptr lives one level up
```

---

## `unique_ptr` isn't *always* free in one sense

`unique_ptr` as a **function parameter or return** by value = a move (2 pointer
ops) — cheaper than a copy but not literally zero. And a `unique_ptr<T>` member
makes the class move-only + gives it a non-trivial move ctor (member-wise) — fine,
but it's not `is_trivially_copyable` anymore, so it can't be `memcpy`'d. For
hot-path POD you want **no** owning pointer members — the object *is* the data,
lives in a pool. `unique_ptr` is for the control plane and for heap objects with
identity.

---

## Andar kya hota hai

- **`unique_ptr`** codegen: dtor = `if (p) delete p;` inlined; `get()`/`->`/`*` =
  a `mov`/deref; move = 2 `mov`s. With EBO the object is literally `struct { T*
  p; }`. The optimizer treats it like a raw pointer.
- **`shared_ptr` refcount:** `strong_count` is `std::atomic`. Copy →
  `lock xadd` (x86) on the count (~15-25 cycles, serializing — it's a full
  barrier). Destroy → `lock xadd` (decrement) + a branch; if it hit 0, an
  indirect call to the type-erased deleter + a second atomic on the weak count.
- **`make_shared` layout:** one `operator new` for `sizeof(_Sp_counted_ptr_inplace<T>)`
  which contains the atomic counts and the `T` (aligned). Deleter for this layout
  just runs `T::~T()`.
- **Contention:** the count's cache line is `MESI`-bounced between cores on every
  cross-core copy/destroy → each such op can cost 100s of cycles and steals
  bandwidth from everything else.

> **HFT relevance:** the smart-pointer performance story is: **`unique_ptr`
> everywhere it fits (free), `shared_ptr` almost nowhere on the hot path
> (atomics + size + allocs), no smart pointer at all in the innermost loops
> (pools + raw)**. A profiler flag for a hot-path `shared_ptr` copy is a
> refactor, not a tune — hoist the `shared_ptr` out of the loop and pass
> `const T&`/`T*` in. Where shared lifetime is real (a snapshot read by several
> strategy threads), the pattern is: publisher holds one `shared_ptr`, hands out
> `const T*` (valid for the snapshot's epoch), swaps in a new snapshot
> atomically. `make_shared` for the 1 allocation + locality whenever a
> `shared_ptr` is warranted. `unique_ptr` members on hot POD types are avoided
> (breaks trivial-copyability / pool relocation).

---

## Hands-on

```bash
./build.ps1 fast 17-RAII/examples/07_smartptr_benchmark.cpp
```

A) create+destroy: `unique_ptr` ≈ raw, `make_shared` ≈ raw, `shared_ptr(new)` ~1.7x.
B) copy: raw ≈ 0.4 ns, `unique_ptr.get()` ≈ 0.7 ns, `shared_ptr` copy ≈ 34 ns
(~90x). Try adding threads that copy the same `shared_ptr` and watch the number
climb (contention).

---

## ⚠️ Traps

### Trap 1 — "smart pointers are slow" (blanket)
`unique_ptr` is free (measured 0.99x). Only `shared_ptr` has costs.

### Trap 2 — `shared_ptr` by value for a read
```cpp
void f(std::shared_ptr<T> p);   // ⚠️ atomic ++/-- per call. const T& or const shared_ptr&
```

### Trap 3 — `shared_ptr(new T)` instead of `make_shared`
```cpp
std::shared_ptr<T> p(new T);   // ⚠️ 2 allocations, worse locality. std::make_shared<T>()
```

### Trap 4 — copying `shared_ptr` in a loop instead of moving / borrowing
```cpp
for (auto sp : vec_of_shared) use(sp);        // ⚠️ atomic per iter. `for (const auto& sp : ...)` + use(*sp)
```

### Trap 5 — `unique_ptr` member on a hot POD type
```cpp
struct Msg { std::unique_ptr<Payload> p; };   // ⚠️ not trivially copyable -> can't memcpy into a pool. Value/pool the payload
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "All smart pointers add overhead" | `unique_ptr` == raw (measured); `shared_ptr` has real costs |
| "`shared_ptr` copy is cheap (just a pointer + int)" | Atomic inc/dec — ~90x a raw copy, worse contended |
| "`sizeof(shared_ptr)` == 8" | 16 (object ptr + control-block ptr) |
| "`make_shared` and `shared_ptr(new T)` perform the same" | 1 alloc vs 2; ~1.74x here |
| "Moving a `shared_ptr` also touches the refcount" | No — move is a pointer steal, no atomic |

---

## Exercises

1. **Run + read:** `07_smartptr_benchmark.cpp` — note (a) `unique_ptr / raw`
   ratio for create+destroy, (b) `shared_ptr copy / raw copy` ratio. Which
   confirms "unique_ptr is free"?

   <details><summary>Answer</summary>

   (a) ~1.0x → `unique_ptr` create/destroy == raw (the allocation dominates,
   wrapper adds nothing). (b) ~90x → `shared_ptr` copy is expensive (atomic
   refcount). Together: `unique_ptr` free, `shared_ptr` not.
   </details>

2. **Fix the hot path:** `void processOrders(std::vector<std::shared_ptr<Order>>
   book) { for (auto o : book) match(o); }` — two things wrong for performance.
   Fix both.

   <details><summary>Answer</summary>

   (1) `book` by value → copies the whole vector (and bumps every element's
   refcount). Take `const std::vector<std::shared_ptr<Order>>&`. (2) `for (auto
   o : book)` → copies each `shared_ptr` per iteration (atomic ++/--). Use `for
   (const auto& o : book) match(*o);`.
   </details>

3. **Alloc count:** counted `operator new` — `for (i in 1e6) { auto p =
   std::make_shared<T>(); }` vs `{ std::shared_ptr<T> p(new T); }`. Total `new`
   calls each?

   <details><summary>Answer</summary>

   `make_shared` → 1e6 `new` calls (one per iteration). `shared_ptr(new T)` →
   2e6 (object + control block per iteration). Plus `make_shared` has better
   cache locality (co-located).
   </details>

4. **Contention:** why does `shared_ptr` copy get *much* slower when 4 threads
   each copy the *same* `shared_ptr` in a loop, but stays fast if each thread
   has its *own* `shared_ptr` to *different* objects?

   <details><summary>Answer</summary>

   Same `shared_ptr` → same control block → same cache line holding the atomic
   count. Every thread's `fetch_add`/`fetch_sub` forces that line to bounce
   between cores (MESI invalidations) → 100s of cycles each. Different objects →
   different control blocks → different cache lines → no contention.
   </details>

5. **Snapshot pattern:** design "publisher owns one `shared_ptr<Snapshot>`,
   readers see a `const Snapshot*` for an epoch". Why does this keep `shared_ptr`
   atomics off the reader hot path?

   <details><summary>Answer</summary>

   Readers get a raw `const Snapshot*` (or `const Snapshot&`) valid for the
   current epoch — no `shared_ptr` copy, no refcount atomic in their loop. The
   publisher swaps in a new `shared_ptr<Snapshot>` atomically and keeps the old
   one alive until readers have advanced past its epoch (via sequence numbers /
   RCU-style — folder 28). Ownership cost is paid once by the publisher, not per
   read.
   </details>

---

## Interview questions

1. `unique_ptr` ka runtime + size overhead — measured?
2. `shared_ptr` ke 3 costs (size, alloc, atomics) — numbers?
3. `shared_ptr` copy vs move — kaunsa refcount touch karta?
4. `make_shared` vs `shared_ptr(new T)` — perf difference + why?
5. Hot loop mein `shared_ptr` dikha profiler mein — kya karo (tune ya redesign)?
6. Multi-thread `shared_ptr` copy contention — kyun itna mehnga?

---

## Next
→ [`12-exercises.md`](12-exercises.md)
