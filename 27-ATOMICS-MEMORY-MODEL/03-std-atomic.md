# 03 — `std::atomic<T>`

## Prerequisites
- `02-atomicity.md`, `18-COPY-MOVE` (non-copyable types)
- [`examples/01_atomic_counter.cpp`](examples/01_atomic_counter.cpp)

## Yeh topic abhi kyun
`std::atomic<T>` woh type hai jise aap conflicting accesses ke liye use karte ho
without UB. Iska API, kaunse `T` allowed hain, `is_lock_free`, aur kuch subtle
points (no copy, `operator=` = `store`, `load` for reads) — yeh sab hardware
ordering (files 06+) se pehle clear hona chahiye.

---

## Declaring & initializing

```cpp
#include <atomic>

std::atomic<int>    counter{0};        // ✅ brace-init
std::atomic<bool>   ready{false};
std::atomic<long>   widx{0};
std::atomic<Node*>  head{nullptr};

std::atomic<int>    x;                 // ⚠️ C++17: NOT zero-initialized (indeterminate)
                                       //    C++20: value-initialized to 0
std::atomic<int>    y = 5;             // ❌ not allowed pre-C++17 (copy-init); use y{5}
```

- **Always brace-initialize.** `std::atomic<int> x;` alone is a trap pre-C++20.
- `ATOMIC_VAR_INIT(v)` — deprecated (C++20), was a workaround for static init.
- `std::atomic<T>` is **not copyable, not movable** — it wraps hardware state you
  can't just memcpy.
  ```cpp
  std::atomic<int> a{1};
  std::atomic<int> b = a;              // ❌ deleted copy ctor
  std::atomic<int> b{a.load()};        // ✅ copy the value out, then construct
  ```

---

## Reading & writing — use the methods

```cpp
std::atomic<int> a{0};

int v = a.load();            // atomic read     (default: seq_cst)
a.store(5);                  // atomic write    (default: seq_cst)
int old = a.exchange(9);     // atomic swap: set to 9, return the old value

// operator shorthands (all seq_cst, all atomic):
int v2 = a;                  // == a.load()
a = 7;                       // == a.store(7)   ⚠️ returns 7, NOT a reference
a += 3;                      // == a.fetch_add(3) + 3
++a; a++;                    // == a.fetch_add(1)
```

**`a = 7` returns the stored value `7`, not `std::atomic<int>&`** — so you can't
chain `x = a = 7` the way you'd expect for a normal type (and shouldn't).

**Prefer the explicit methods** (`load`, `store`, `fetch_add`) with an explicit
`std::memory_order` in real code — the shorthands hide the (expensive) `seq_cst`
default (file 09).

---

## Which `T` can go in `std::atomic<T>`

- **`T` must be trivially copyable** (`std::is_trivially_copyable_v<T>`) — no
  `std::string`, no `std::vector`, no `std::unique_ptr` (except the special
  `std::atomic<std::shared_ptr<T>>` / `<weak_ptr>` in C++20).
- Integral types, `bool`, pointers, `enum`s, `float`/`double`, and small
  trivially-copyable structs.
- `std::atomic<std::shared_ptr<T>>` (C++20) — special: atomic operations on a
  `shared_ptr` (usually **not** lock-free — an internal lock table).

```cpp
std::atomic<int>              // ✓
std::atomic<Order*>           // ✓
std::atomic<double>           // ✓
std::atomic<struct { int a, b; }>   // ✓ (trivially copyable, 8B → lock-free)
std::atomic<std::string>     // ❌ compile error (not trivially copyable)
```

---

## Integral / pointer / floating specializations

| `std::atomic<T>` | Extra members |
|---|---|
| **integral** (`int`, `long`, `uint64_t`, ...) | `fetch_add`, `fetch_sub`, `fetch_and`, `fetch_or`, `fetch_xor`, `++`, `--`, `+= -= &= |= ^=` |
| **pointer** (`T*`) | `fetch_add(n)`, `fetch_sub(n)` (in units of `sizeof(T)`), `++`, `--`, `+= -=` |
| **floating** (`float`, `double`, C++20) | `fetch_add`, `fetch_sub`, `+= -=` (may be a CAS loop internally) |
| **any trivially-copyable `T`** | `load`, `store`, `exchange`, `compare_exchange_weak/strong` |

`fetch_and`/`fetch_or`/`fetch_xor` are integral-only (no `fetch_mul`, `fetch_max` —
you CAS-loop those, file 05, `examples/02`).

---

## `is_lock_free()` / `is_always_lock_free`

```cpp
std::atomic<int>::is_always_lock_free    // static constexpr bool — compile-time
std::atomic<long>::is_always_lock_free   // true
std::atomic<Big24>::is_always_lock_free  // false — hidden mutex

std::atomic<long> a;
a.is_lock_free();                        // runtime query (rarely differs from the static one)
```

- **`is_always_lock_free`** (C++17) — `constexpr`. Use in `static_assert`.
- **`is_lock_free()`** — member function, runtime. (Historically could differ by
  alignment; in practice ≈ the static answer.)
- Not lock-free → `std::atomic<T>` uses an internal lock (a table of spinlocks
  keyed by address). Still correct, just not usable in lock-free algorithms and
  slower. `examples/01`: `atomic<24-byte struct>::is_always_lock_free == 0`.

```cpp
static_assert(std::atomic<Head>::is_always_lock_free,
              "Head must be lock-free for the SPSC queue");   // folder 28
```

---

## `ATOMIC_*_LOCK_FREE` macros

```cpp
ATOMIC_INT_LOCK_FREE      // 0 = never, 1 = sometimes, 2 = always
ATOMIC_POINTER_LOCK_FREE
ATOMIC_LLONG_LOCK_FREE
```
Old-style compile-time hints per fundamental type. `is_always_lock_free` is the
modern, per-`std::atomic<T>` form.

---

## `std::atomic_flag` — the one guaranteed-lock-free type

```cpp
std::atomic_flag lock = ATOMIC_FLAG_INIT;   // (C++20: just std::atomic_flag lock;)

// spinlock:
while (lock.test_and_set(std::memory_order_acquire)) { /* spin */ }
// ... critical section ...
lock.clear(std::memory_order_release);

// C++20:
lock.test();                  // read without setting
lock.wait(false);             // block until != false
lock.notify_one();
```

`std::atomic_flag` is **guaranteed lock-free on every platform** — it's the
lowest-common-denominator primitive (basically one bit). Only `test_and_set` /
`clear` (+ C++20 `test`/`wait`/`notify`). Use it for a minimal spinlock; for
anything richer, `std::atomic<bool>`.

---

## C++20 additions

- **`std::atomic<T>::wait(old)` / `notify_one()` / `notify_all()`** — futex-style
  blocking: `wait(old)` blocks while the value `== old`. Lets you build efficient
  waiting without a separate CV.
- **`std::atomic_ref<T>`** — apply atomic ops to an *existing* non-atomic object
  (file 13).
- **`std::atomic<std::shared_ptr<T>>` / `<std::weak_ptr<T>>`** — replaces the
  deprecated free `std::atomic_load(shared_ptr*)` functions.
- **Floating-point `fetch_add`/`fetch_sub`**.

---

## > **HFT relevance**
> - **`static_assert(std::atomic<T>::is_always_lock_free)`** on every atomic used
>   in a hot / lock-free structure (SPSC/MPSC head & tail, a published pointer, a
>   seqlock counter). A hidden-mutex atomic on the hot path is a latency
>   land-mine.
> - **Explicit `std::memory_order` everywhere** — never the `operator=` / `++`
>   shorthands on the hot path (they're `seq_cst` = a store fence on x86, file 09).
> - **`std::atomic_flag` for a bare spinlock** where you're sure it's lock-free by
>   spec; `std::atomic<bool>` when you need `load`/`store`/`exchange` semantics.
> - **`std::atomic<T>::wait/notify` (C++20)** for a low-overhead "park until the
>   producer publishes" on a helper thread — no `std::condition_variable` + mutex.
> - **Fit shared state into ≤ 8 bytes** (counter, index, tagged pointer, small
>   enum). Bigger → seqlock / snapshot-pointer-swap, not `std::atomic<Big>`.

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/01_atomic_counter.cpp
```

`is_always_lock_free` for `int`/`long`/`void*`/24-byte-struct; non-atomic vs
atomic counter. Then:
- `std::atomic<int> x; std::printf("%d\n", x.load());` at `-std=c++17` vs
  `-std=c++20` — indeterminate vs 0.
- A minimal spinlock with `std::atomic_flag`; count 4 threads × 100k `++shared`.
- `std::atomic<int> a{0}; auto r = (a = 5);` — `r` is `int` (5), not a reference.

---

## ⚠️ Traps

### Trap 1 — `std::atomic<int> x;` and reading it (pre-C++20)
Indeterminate value. **Always `std::atomic<int> x{0};`.**

### Trap 2 — copying a `std::atomic`
```cpp
std::atomic<int> b = a;          // ❌ deleted
std::atomic<int> b{a.load()};    // ✅
```

### Trap 3 — `a = 7` returns `7`, not `a`
Can't chain like a normal type; and `a = a + 1` is a non-atomic RMW (`load` then
`store`) — use `a.fetch_add(1)` or `++a`.

### Trap 4 — assuming `std::atomic<BigStruct>` is lock-free
Check `is_always_lock_free`. Big → internal spinlock.

### Trap 5 — `std::string` / `std::vector` in `std::atomic`
Compile error (not trivially copyable). Publish a pointer/index instead.

### Trap 6 — the `operator` shorthands on the hot path
`++a`, `a += n`, `a = v` are all `seq_cst`. Use `a.fetch_add(n, relaxed)` /
`a.store(v, release)` explicitly.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::atomic<int> x;` is zero-initialized" | Only in C++20; indeterminate in C++17 — brace-init always |
| "`std::atomic` is copyable" | Non-copyable, non-movable — copy the value out |
| "`a = a + 1` on an atomic is atomic" | It's `load` then `store` — a non-atomic RMW; use `fetch_add`/`++` |
| "any type works in `std::atomic`" | Trivially-copyable only (+ the shared_ptr/weak_ptr specializations) |
| "`std::atomic<T>` is always lock-free" | Small trivially-copyable `T` only; check `is_always_lock_free` |
| "the `++`/`=` shorthands are cheap" | They're `seq_cst` — use explicit `memory_order` |

---

## Exercises

1. **Which compile:** `std::atomic<int> a{1};` `std::atomic<int> b = a;`
   `std::atomic<int> c{a.load()};` `std::atomic<std::string> s;`
   `std::atomic<int*> p{nullptr};`

   <details><summary>Answer</summary>

   `a`, `c`, `p` compile. `b = a` — error (deleted copy ctor). `s` — error
   (`std::string` isn't trivially copyable).
   </details>

2. **RMW correctness:** two threads run `a = a * 2;` on `std::atomic<int> a`. Is
   the result `a * 4`? Fix?

   <details><summary>Answer</summary>

   No — `a = a * 2` is `int t = a.load(); a.store(t * 2);` — the two threads can
   interleave and one doubling is lost. There's no `fetch_mul`, so CAS-loop it:
   `int cur = a.load(); while (!a.compare_exchange_weak(cur, cur * 2)) {}`
   (`examples/02`).
   </details>

3. **`is_always_lock_free`:** write a `static_assert` guaranteeing the head of an
   SPSC ring is lock-free.

   <details><summary>Answer</summary>

   `static_assert(std::atomic<std::size_t>::is_always_lock_free, "SPSC head must
   be lock-free");` (and similarly for whatever index/pointer type you use).
   </details>

4. **`atomic_flag` spinlock:** write `lock()` and `unlock()` for a
   `std::atomic_flag`.

   <details><summary>Answer</summary>

   ```cpp
   std::atomic_flag f = ATOMIC_FLAG_INIT;   // or just `std::atomic_flag f;` in C++20
   void lock()   { while (f.test_and_set(std::memory_order_acquire)) { /* pause */ } }
   void unlock() { f.clear(std::memory_order_release); }
   ```
   </details>

5. **Return value:** `std::atomic<int> a{0}; int r = (a = 10);` — what is `r`, and
   why can't you write `b = (a = 10)` for `std::atomic<int> b`?

   <details><summary>Answer</summary>

   `r == 10` — `operator=` on `std::atomic` returns the *value* stored (an `int`),
   not `std::atomic<int>&`. So `b = (a = 10)` would try `b = 10` which is
   `b.store(10)` — that actually works, but it's two independent atomic stores,
   not a "chained assignment," and reads oddly. Use explicit `a.store(10); b.store(10);`.
   </details>

---

## Interview questions

1. `std::atomic<int> x;` — C++17 vs C++20 init behaviour.
2. `std::atomic` copyable? Kaise copy karein value?
3. `a = a + 1` on `std::atomic` — atomic hai? Kyun nahi?
4. Kaunse `T` `std::atomic<T>` mein ja sakte (trivially copyable)?
5. `is_always_lock_free` vs `is_lock_free()` — farq, kab use.
6. `std::atomic_flag` special kyun (guaranteed lock-free)?
7. `operator++` / `operator=` shorthands ka default memory order?

---

## Next
→ [`04-atomic-operations.md`](04-atomic-operations.md)
