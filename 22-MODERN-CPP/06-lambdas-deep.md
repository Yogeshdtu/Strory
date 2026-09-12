# 06 — Lambdas in depth

## Prerequisites
- [`01-cpp11-features.md`](01-cpp11-features.md), folder 21 file 06 (generic lambdas / packs)
- Folder 19 file 16 (`std::function` cost), folder 18 (captures = members)

## Yeh topic abhi kyun
Lambda modern C++ ka sabse zyada use hone wala feature hai — algorithm
predicates, callbacks, scoped helpers, `std::visit` visitors. "Ek lambda ek
compiler-generated class hai jiska ek `operator()` aur captured state hai" — yeh
mental model saaf hona chahiye, kyunki captures, `mutable`, aur lifetime sab ise
follow karte.

[`examples/01_lambdas_all.cpp`](examples/01_lambdas_all.cpp) mein har feature hai.

---

## The desugaring

```cpp
int k = 10;
auto f = [k](int x) mutable { return x + (++k); };

// is roughly:
struct __lambda {
    int k;                                          // one member per captured-by-value name
    int operator()(int x) /* not const, because `mutable` */ { return x + (++k); }
};
__lambda f{k};                                       // k copied in at the point of definition
```

- **No capture** → the closure is empty (`sizeof == 1`), `operator()` is `const`,
  and it **converts to a function pointer**.
- **By-value capture** → a member holding a *copy*, taken when the lambda is
  created. `operator()` is `const` (so the copy is read-only) **unless**
  `mutable`.
- **By-reference capture** → a member holding a reference (really a pointer). The
  referent must outlive the closure.
- The type is **unique and unnamable** — `decltype(f)`. Two textually identical
  lambdas are different types.

---

## Capture forms

```cpp
[]              // capture nothing
[x, &y]         // x by value, y by reference
[=]             // everything used, by value        (captures `this` by pointer -- deprecated to rely on this in C++20; write [=, this])
[&]             // everything used, by reference
[=, &big]       // by value, except big by reference
[&, total]      // by reference, except total by value
[this]          // the enclosing object's `this` pointer -> members accessed directly (dangling if the object dies)
[*this]         // (C++17) a COPY of the whole enclosing object
[k = expr]      // (C++14) INIT capture -- a new member `k` initialized to `expr` (any expression, incl. std::move)
[p = std::move(uptr)]   // move a move-only object into the closure
[...args = std::move(pack)]   // (C++20) capture a parameter pack by move
```

`[=]` / `[&]` capture **only the names actually used** in the body — not every
local in scope.

---

## `mutable`

By default `operator()` is `const` → you can't modify by-value captures. `mutable`
drops the `const`:

```cpp
auto counter = [n = 0]() mutable { return ++n; };   // n is the closure's own state, persists across calls
counter(); counter(); counter();                     // 1, 2, 3
```

`mutable` does **not** affect by-reference captures (you can already modify
through a non-const reference).

---

## Generic and template lambdas

```cpp
auto add   = [](auto a, auto b) { return a + b; };            // C++14: operator() is a template
auto front = []<class T>(const std::vector<T>& v) { return v.front(); };  // C++20: explicit template param
auto sum   = [](auto... xs) { return (xs + ...); };           // C++20: variadic + fold
```

Explicit template params (C++20) let you name and constrain the type
(`[]<std::integral T>(T x){...}`), and access it (`T{}`, `sizeof(T)`).

---

## Return type

```cpp
[](int x) { return x * 2; }                  // deduced -> int
[](int x) -> double { return x * 2; }         // explicit trailing return type
[](bool b) { if (b) return 1; return 2.0; }  // ❌ inconsistent deduction (int vs double)
```

---

## `constexpr` and `consteval` lambdas

```cpp
constexpr auto sq = [](int x) { return x * x; };   // implicitly constexpr if the body qualifies
static_assert(sq(5) == 25);
```
Since C++17 a lambda is `constexpr` automatically when its body is a valid
constant expression; `consteval` (C++20) forces compile-time.

---

## Where the cost is

| Use | Cost |
|---|---|
| Passed as a **template parameter** (`std::sort`, `std::for_each`, your `template <class F>`) | **inlined** — zero overhead, same as hand-written |
| Stored in a **`std::function`** | type-erased indirect call (~5 ns) + heap allocation if the capture exceeds the small buffer (~16 B) — folder 19 file 16 |
| Stored in a **`auto` variable** and called directly | inlined at the call site |
| No-capture lambda → **function pointer** | an indirect call, not inlined |

[`examples/01`](examples/01_lambdas_all.cpp): `sizeof([x1(int), x2(double)]{})` =
16; `sizeof([]{})` = 1; `sizeof(std::function<int()>)` = 32.

**Hot path rule:** template the callable or use a small tag/function pointer;
reserve `std::function` for cold callbacks (folder 19 file 16, folder 21 file 16).

---

## Andar kya hota hai

- The closure is a real class with automatic storage — a stack object holding
  the captures. Its `operator()` is an ordinary member function the compiler can
  inline.
- A no-capture lambda has a compiler-generated `operator T(*)(Args...)`
  conversion → it can decay to a function pointer for C APIs (`qsort`, callbacks).
- `[this]` stores the `this` **pointer** — accessing `member` in the body is
  `this->member`. If the enclosing object is destroyed while the closure lives
  (stored in a `std::function`, passed to an async op), that's a dangling access.
  `[*this]` copies the object into the closure to avoid it.
- Init-capture `[p = std::move(uptr)]` gives the closure a `std::unique_ptr`
  member move-constructed from `uptr` — this is how you get a move-only object
  into a lambda (a lambda with a move-only capture is itself move-only).
- Passed to a template, the concrete closure type is known → `operator()`
  inlines → `std::sort` with a lambda comparator is the same code as with `<`.

> **HFT relevance:** lambdas are free on the hot path **when templated** — a
> no-capture comparator/predicate inlines to nothing, and a small stateful
> closure passed to a `template <class F>` function inlines with its captures in
> registers (folder 19 file 16, folder 21 file 16). The two things that cost:
> (1) storing a lambda in **`std::function`** — indirect call + a possible
> `operator new` when a bigger capture is assigned; avoid on the tick path.
> (2) **`[this]` / `[&]` captures that outlive their referents** — a classic
> use-after-free when a closure is stashed in a callback registry or an async
> task; capture by value / `[*this]` / a `shared_ptr` when the closure escapes.
> Init-capture (`[buf = std::move(buffer)]`) moves ownership into a task with no
> copy.

---

## Hands-on

```bash
./build.ps1 22-MODERN-CPP/examples/01_lambdas_all.cpp
./build.ps1 fast 19-STL/examples/08_std_function_cost.cpp    # templated ~1.5 ns vs std::function ~5 ns
```

Write: a stateful `mutable` accumulator lambda; a generic lambda used with 3
types; a lambda that move-captures a `std::unique_ptr`; a no-capture lambda
passed to a C API expecting a function pointer.

---

## ⚠️ Traps

### Trap 1 — dangling reference capture
```cpp
std::function<int()> f;
{ int local = 5; f = [&local]{ return local; }; }
f();   // ⚠️ UB. Capture by value, or init-capture
```

### Trap 2 — `[this]` in a closure that outlives the object
```cpp
registry.on_event([this]{ handle(); });   // ⚠️ if `this` is destroyed before the event fires -> UB. [*this] or a shared_ptr
```

### Trap 3 — forgetting `mutable` for a stateful by-value capture
```cpp
auto c = [n = 0] { return ++n; };   // ❌ operator() is const -> can't ++n. add `mutable`
```

### Trap 4 — `std::function` on a hot callback
```cpp
std::function<void(const Tick&)> cb = [state](const Tick& t){ ... };
for (auto& t : million) cb(t);   // ⚠️ indirect + no inline (+ alloc if `state` is big). Template the loop
```

### Trap 5 — capturing by value expecting to see later changes
```cpp
int x = 1;
auto f = [x]{ return x; };
x = 100;
f();   // 1 -- the capture snapshotted x at creation. Use [&x] for a live view
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "A lambda is a `std::function`" | A lambda is a unique compiler-generated class; `std::function` can hold one (with cost) |
| "`[=]` captures every local in scope" | Only the names the body actually uses |
| "By-value capture tracks later changes" | It's a snapshot at creation time; use `[&]` for a live reference |
| "`mutable` makes a reference capture writable" | Reference captures are already writable; `mutable` is for **by-value** captures |
| "Lambdas cost something vs a function object" | None — passed to a template they inline identically |

---

## Exercises

1. **Snapshot vs live:** `int a = 1; auto byV = [a]{ return a; }; auto byR = [&a]{
   return a; }; a = 9;` — what does each return?

   <details><summary>Answer</summary>

   `byV()` → 1 (snapshot at creation). `byR()` → 9 (reads the live `a`).
   </details>

2. **Move-capture:** write a lambda that takes ownership of a
   `std::unique_ptr<Socket>` and uses it. Is the lambda copyable?

   <details><summary>Answer</summary>

   `auto task = [sock = std::move(theSocket)]{ sock->send(...); };`. The lambda
   has a `unique_ptr` member → it's **move-only** (not copyable), so it can go
   into a `std::move_only_function` / a thread, but not a plain `std::function`
   pre-C++23.
   </details>

3. **Overloaded visitor:** why does `std::visit` need `Overloaded{ lambdas... }`
   (inherit + `using operator()`) rather than just a list of lambdas?

   <details><summary>Answer</summary>

   `std::visit` calls one callable that must handle every alternative. A single
   struct that inherits from each lambda and brings in each `operator()` via
   `using` becomes that one overloaded callable; the compiler picks the right
   overload per alternative (and errors if one's unhandled — exhaustiveness).
   </details>

4. **Function pointer decay:** `qsort` needs `int(*)(const void*, const void*)`.
   Can you pass a lambda? Which lambda?

   <details><summary>Answer</summary>

   Only a **no-capture** lambda — it has an implicit conversion to a function
   pointer. `qsort(a, n, sizeof(int), [](const void* x, const void* y){ return
   *(const int*)x - *(const int*)y; });`. A capturing lambda has state → no
   function-pointer conversion.
   </details>

5. **Hot vs cold:** you have a per-tick handler and a once-at-startup config
   callback. Storage for each?

   <details><summary>Answer</summary>

   Per-tick: a **template parameter** (`template <class H> void run(H&&)`) so it
   inlines. Startup config: `std::function` is fine — called once, the indirect
   call and any small allocation are irrelevant, and you get a uniform stored
   type.
   </details>

---

## Interview questions

1. Lambda ka desugaring — kya class banti, captures kya bante?
2. By-value vs by-reference capture — snapshot vs live, lifetime?
3. `mutable` kya karta, kis capture pe asar?
4. No-capture lambda function pointer mein kyun convert ho sakta, capturing nahi?
5. `[this]` vs `[*this]` — dangling risk kaise?
6. Lambda ki cost — template param vs `std::function` vs direct call?

---

## Next
→ [`07-constexpr-family.md`](07-constexpr-family.md)
