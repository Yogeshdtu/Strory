# 12 — Perfect forwarding (`std::forward`, forwarding references)

## Prerequisites
- [`05-rvalue-references.md`](05-rvalue-references.md), [`08-std-move.md`](08-std-move.md)
- Folder 21 preview (templates — `template <class T>` deduction)

## Yeh topic abhi kyun
Ek generic wrapper (`std::make_unique`, `emplace_back`, `std::function`, a
factory) ko apne arguments ko **exactly waise** aage bhejna hota hai jaise woh
aaye — lvalue tha to lvalue, rvalue tha to rvalue — bina extra copy/move ke. Yeh
"**perfect forwarding**" hai, aur iske do parts: **forwarding references**
(`T&&` with deduced `T`) aur **`std::forward`**.

---

## The problem

```cpp
// A wrapper that should pass its arg straight through to `sink`
void sink(const Widget&);   // #1
void sink(Widget&&);        // #2

template <class T>
void relay(T arg) {          // ⚠️ BY VALUE -> always a copy of the argument, then...
    sink(arg);               // ...arg is an lvalue -> always #1
}

relay(std::move(w));         // copied on the way in, then passed as lvalue -> 1 copy, wrong overload
```

We want `relay(w)` → `sink(const Widget&)` and `relay(std::move(w))` →
`sink(Widget&&)`, with **zero** extra copies/moves. Neither `void relay(T)`, nor
`void relay(const T&)`, nor `void relay(T&)` does this.

---

## Part 1 — forwarding references

```cpp
template <class T>
void relay(T&& arg);         // `T&&` where T is DEDUCED -> a FORWARDING reference
```

When `T` is a **deduced template parameter** and the parameter is `T&&`, the
deduction + **reference collapsing** rules make it bind to **both** lvalues and
rvalues, remembering which:

| Call | `T` deduces to | `arg`'s type (`T&&`) |
|---|---|---|
| `relay(w)` — lvalue `Widget` | `Widget&` | `Widget& &&` → **`Widget&`** (lvalue ref) |
| `relay(std::move(w))` — xvalue | `Widget` | `Widget&&` (rvalue ref) |
| `relay(makeWidget())` — prvalue | `Widget` | `Widget&&` |
| `relay(cw)` — const lvalue | `const Widget&` | `const Widget&` |

**Reference collapsing:** `& + &` → `&`, `& + &&` → `&`, `&& + &` → `&`,
`&& + &&` → `&&`. "Any lvalue ref wins; only `&& + &&` gives `&&`." This is why
`T&&` with a deduced `T` can represent an lvalue.

**It's a forwarding reference ONLY when:**
- `T` is a template parameter of the **enclosing function template**, deduced
  from this parameter, and the parameter is exactly `T&&` (not `const T&&`, not
  `std::vector<T>&&`).
- Or the parameter is `auto&&`.

```cpp
template <class T> void a(T&& x);            // forwarding ref
template <class T> void b(const T&& x);      // NOT (const)
template <class T> void c(std::vector<T>&& x); // NOT (T deduced inside vector<T>, param is vector<T>&&)
void d(Widget&& x);                          // NOT (concrete type, not deduced)
auto&& e = expr;                             // forwarding ref
template <class T> struct S { void f(T&& x); };  // NOT -- T is the CLASS's param, not deduced by f
```

---

## Part 2 — `std::forward`

Inside `relay`, `arg` has a name → the expression `arg` is an **lvalue** (files
04/05). To pass it onward *with its original value category*, use `std::forward`:

```cpp
template <class T>
void relay(T&& arg) {
    sink(std::forward<T>(arg));   // T = Widget&  -> forward gives an lvalue
                                 // T = Widget   -> forward gives an rvalue (Widget&&)
}
```

`std::forward<T>(arg)`:
- If `T` deduced to `Widget&` (lvalue was passed) → returns an **lvalue** →
  `sink(const Widget&)`.
- If `T` deduced to `Widget` (rvalue was passed) → returns an **rvalue** →
  `sink(Widget&&)`.

Simplified implementation:

```cpp
template <class T>
constexpr T&& forward(std::remove_reference_t<T>& x) noexcept {
    return static_cast<T&&>(x);   // T&& collapses: T=U& -> U& ; T=U -> U&&
}
```

It's a **conditional cast** driven by the deduced `T`. `std::move` is
*unconditional* (always → rvalue); `std::forward` is *conditional* (→ whatever
the caller had). **`std::forward<T>` inside a forwarding-reference function;
`std::move` for a plain rvalue reference / a value you're definitely done with.**

`examples/07_perfect_forwarding.cpp`: `relayBad` (no forward → always lvalue) vs
`relayGood` (with `std::forward` → preserves category).

---

## The canonical forwarding factory

```cpp
template <class T, class... Args>
std::unique_ptr<T> make(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));   // forward EACH arg
}

// exactly what std::make_unique does. Args forwarded -> T's ctor sees the real value categories
// -> a moved-in temporary is moved, an lvalue is copied (or bound), zero extra copies.
```

`std::forward<Args>(args)...` — a pack expansion that forwards each argument with
its own deduced category.

---

## `auto&&` — forwarding reference in ranged-for and generic lambdas

```cpp
for (auto&& elem : container) {          // binds to lvalue or rvalue elements without copying
    use(std::forward<decltype(elem)>(elem));   // forward if you pass it on
}

auto fwd = [](auto&& x) { g(std::forward<decltype(x)>(x)); };   // generic forwarding lambda
```

For `auto&&`, use `std::forward<decltype(x)>(x)` (since there's no named template
`T`).

---

## Andar kya hota hai

- `T&&` forwarding reference: template argument deduction sees the argument's
  value category. For an lvalue of type `U`, `T` deduces to `U&` (special rule
  for `T&&` parameters); the parameter type `T&&` = `U& &&` collapses to `U&`.
  For an rvalue, `T = U`, parameter `U&&`.
- `std::forward<T>(x)` = `static_cast<T&&>(x)`. With `T = U&`: `static_cast<U& &&>`
  → `static_cast<U&>` → an lvalue. With `T = U`: `static_cast<U&&>` → an rvalue.
- **Zero runtime cost** — like `std::move`, it's a compile-time cast that steers
  overload resolution. `-O2` erases it.
- `std::forward` **must** be given the explicit template argument (`std::forward<T>(x)`)
  — calling `std::forward(x)` (deduced) would defeat the purpose (it can't tell
  what the caller had).

> **HFT relevance:** perfect forwarding is a library-writer's tool — it's what
> makes `emplace_back`, `make_unique`, `std::function`, `std::apply`, and your
> own generic pools/queues/factories pass constructor arguments through **without
> a spurious copy or move**. In hot code that uses these (`q.emplace(seq, px,
> qty)` constructing a message in place), forwarding is why the args land in the
> object's storage directly. Writing your own: `template <class... A> void
> emplace(A&&... a) { new (slot()) T(std::forward<A>(a)...); }`. The bugs:
> forgetting `std::forward` (everything arrives as an lvalue → copies), using
> `std::move` instead (an lvalue arg gets moved-from — surprising the caller),
> or `const T&&` / `vector<T>&&` params that aren't actually forwarding
> references.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/07_perfect_forwarding.cpp
```

`relayBad` vs `relayGood`; reference collapsing table; a generic `make<T>(Args&&...)`
factory forwarding a `std::move(src)` string (arrives as rvalue → moved into
`dst`, `src` emptied).

---

## ⚠️ Traps

### Trap 1 — forgetting `std::forward` in a forwarding-ref function
```cpp
template <class T> void relay(T&& x) { sink(x); }   // ⚠️ x is an lvalue here -> always sink(const&). std::forward<T>(x)
```

### Trap 2 — `std::move` instead of `std::forward`
```cpp
template <class T> void relay(T&& x) { sink(std::move(x)); }
// ⚠️ if caller passed an lvalue, you just moved-from THEIR object. Use std::forward<T>(x)
```

### Trap 3 — thinking a concrete `T&&` is a forwarding reference
```cpp
void take(std::string&& s);   // rvalue ref -- only rvalues. NOT forwarding
```

### Trap 4 — `std::forward` without the template argument
```cpp
sink(std::forward(x));   // ❌ / defeats the purpose. std::forward<T>(x)
```

### Trap 5 — forwarding reference "eats" everything, breaking overloads
```cpp
struct S { template <class T> S(T&& x); };   // ⚠️ this greedy ctor can hijack the copy ctor for non-const S args.
                                             //    constrain it (concepts / enable_if -- folder 21)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`T&&` always means rvalue reference" | Deduced `T&&` / `auto&&` = forwarding reference (binds lvalues too) |
| "`std::forward` and `std::move` are interchangeable" | `move` = unconditional → rvalue; `forward` = conditional, preserves category |
| "Inside `relay(T&& x)`, `x` is an rvalue" | The name `x` is an lvalue — need `std::forward<T>(x)` |
| "`std::forward` can deduce its argument" | You must write `std::forward<T>(x)` explicitly |
| "Forwarding references have runtime cost" | Zero — compile-time deduction + cast |

---

## Exercises

1. **Deduce `T`:** for `template <class T> void f(T&& x);` — what is `T` and
   `decltype(x)` for `int i; const int ci = 0;` — `f(i)`, `f(ci)`, `f(42)`,
   `f(std::move(i))`?

   <details><summary>Answer</summary>

   `f(i)` → `T = int&`, `x : int&`. `f(ci)` → `T = const int&`, `x : const int&`.
   `f(42)` → `T = int`, `x : int&&`. `f(std::move(i))` → `T = int`, `x : int&&`.
   </details>

2. **Fix the forward:** `template <class T> void store(T&& x) { data_.push_back(x);
   }` — for `store(std::move(bigString))`, does the string get moved into the
   vector? Fix.

   <details><summary>Answer</summary>

   No — `x` is a named lvalue inside `store`, so `push_back(x)` copies. Fix:
   `data_.push_back(std::forward<T>(x));` — then an rvalue argument is moved, an
   lvalue is copied.
   </details>

3. **move vs forward:** `template <class T> void a(T&& x) { g(std::move(x)); }`
   vs `void b(T&& x) { g(std::forward<T>(x)); }` — call each with an lvalue `w`.
   What happens to the caller's `w`?

   <details><summary>Answer</summary>

   `a(w)` → `std::move(x)` unconditionally → `g` gets an rvalue → if `g` moves,
   the caller's `w` is now moved-from (a surprise bug). `b(w)` → `std::forward<T>(x)`
   with `T = W&` → `g` gets an **lvalue** → `w` untouched. `forward` is correct
   for pass-through.
   </details>

4. **Not a forwarding ref:** which of these parameters is a forwarding
   reference? `template<class T> void p(T&&)`, `template<class T> void
   q(const T&&)`, `void r(int&&)`, `template<class T> void s(std::pair<T,int>&&)`,
   `auto&& u = expr`.

   <details><summary>Answer</summary>

   Forwarding: `p` (`T&&`, deduced), `u` (`auto&&`). Not: `q` (`const T&&`), `r`
   (concrete), `s` (`T` deduced inside `pair<T,int>`, top-level param is
   `pair<T,int>&&`).
   </details>

5. **Emplace:** write `template <class... A> void Pool::emplace(A&&... a)` that
   placement-news a `T` in a slot, forwarding the args. Show a call with a
   temporary and a call with an lvalue.

   <details><summary>Answer</summary>

   `template <class... A> void emplace(A&&... a) { new (nextSlot())
   T(std::forward<A>(a)...); }`. `pool.emplace(std::string("hi"), 42)` — the
   temporary string is forwarded as an rvalue → moved into `T`. `std::string s;
   pool.emplace(s, 42)` — `s` forwarded as an lvalue → copied into `T`.
   </details>

---

## Interview questions

1. Forwarding reference kya hai — `T&&` kab forwarding ref, kab nahi?
2. Reference collapsing rules?
3. `std::forward` kya karta, `std::move` se kaise alag?
4. Forwarding-ref function ke andar `x` lvalue kyun, iska fix?
5. `std::forward<T>(x)` mein `<T>` explicitly kyun dena padta?
6. Perfect forwarding kis library feature (emplace/make_unique) ke peeche hai?

---

## Next
→ [`13-noexcept-move.md`](13-noexcept-move.md)
