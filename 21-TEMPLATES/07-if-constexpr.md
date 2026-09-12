# 07 — `if constexpr` — compile-time branching

## Prerequisites
- [`06-variadic-templates.md`](06-variadic-templates.md)
- Folder 19 file 21 (`<type_traits>`), folder 08 (`constexpr`)

## Yeh topic abhi kyun
`if constexpr` (C++17) generic code likhne ka **sabse simple tool** hai. Ek `if`
jiska **discarded branch instantiate hi nahi hota** — matlab us branch mein aisa
code likh sakte ho jo current `T` ke liye compile nahi hota. Isse tag dispatch
aur bahut saara SFINAE khatam ho jaata.

`examples/04_if_constexpr.cpp` sab dikhata hai.

---

## The rule

```cpp
template <class T>
void handle(const T& v) {
    if constexpr (std::is_pointer_v<T>) {
        use(*v);                        // dereference -- only compiled when T IS a pointer
    } else if constexpr (std::is_integral_v<T>) {
        use(v + 1);
    } else {
        use(v.method());                // .method() -- only compiled when the else branch is taken
    }
}
```

- The condition must be a **constant expression** convertible to `bool`.
- Inside a **template**, the branch **not taken is not instantiated** — its body
  is parsed for syntax but not type-checked against `T`. So `*v` is fine when `T`
  isn't a pointer, `v.method()` is fine when `T` has no such method — as long as
  that branch is the discarded one for that `T`.
- Outside a template (or in a non-dependent context), both branches are still
  compiled — `if constexpr` there only affects which one *runs*, not
  instantiation.

Contrast a plain `if`: **both** branches must compile for every `T`, so
`if (std::is_pointer_v<T>) use(*v);` fails to compile when `T` is `int`.

---

## What it replaces

### Tag dispatch (old)
```cpp
template <class T> void impl(T v, std::true_type)  { /* pointer path */ }
template <class T> void impl(T v, std::false_type) { /* value path */ }
template <class T> void f(T v) { impl(v, std::is_pointer<T>{}); }
```
→ one function with `if constexpr (std::is_pointer_v<T>)`.

### SFINAE overload pairs (old — file 09)
```cpp
template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>  void g(T);
template <class T, std::enable_if_t<!std::is_integral_v<T>, int> = 0> void g(T);
```
→ one `g` with `if constexpr`. (SFINAE / concepts are still needed to
*remove a function from overload resolution*; `if constexpr` only branches
*inside* a function that's already selected.)

### Recursion base cases
```cpp
template <std::size_t N>
constexpr std::size_t fact() {
    if constexpr (N <= 1) return 1;
    else                  return N * fact<N - 1>();   // recursion terminated by the compile-time branch
}
```

---

## Common patterns

```cpp
// fast path for trivially copyable types
template <class T>
void serialize(std::vector<std::byte>& out, const T& v) {
    if constexpr (std::is_trivially_copyable_v<T>) {
        auto n = out.size(); out.resize(n + sizeof(T));
        std::memcpy(out.data() + n, &v, sizeof(T));      // not compiled for T with a std::string member
    } else {
        v.serialize_into(out);
    }
}

// handle each alternative of a variadic visitor
std::visit([](auto&& x) {
    using U = std::decay_t<decltype(x)>;
    if constexpr (std::is_same_v<U, int>)         handleInt(x);
    else if constexpr (std::is_same_v<U, std::string>) handleStr(x);
    else                                          handleOther(x);
}, myVariant);

// enable a member conditionally-ish (member still exists, body is a no-op)
template <bool Metrics>
struct Engine {
    void onEvent() { work(); if constexpr (Metrics) ++count_; }
    std::conditional_t<Metrics, std::uint64_t, std::monostate> count_{};
};

// pick a return based on T
template <class T>
auto normalize(T x) {
    if constexpr (std::is_floating_point_v<T>) return x / T(1);
    else                                       return static_cast<double>(x);
}
```

---

## Gotchas

- **`else` is not optional for correctness** — if your branches don't cover all
  `T` and a function must return a value, a missing `else` means "falls off the
  end" for the uncovered `T` → UB / warning. Always have a final `else` (or a
  `static_assert(false-ish)` — but see below).
- **`static_assert(false)` in an `else`** to reject unhandled types used to be
  ill-formed (fires even if never instantiated); C++23 fixed the common case, but
  the portable trick is `static_assert(sizeof(T) == 0, "unhandled")` or
  `static_assert(always_false_v<T>)` where `always_false_v` is a
  dependent-on-`T` false.
- **The discarded branch is still parsed** — syntax errors (unbalanced braces,
  unknown identifiers that aren't dependent) still fail. It's *type-checking
  against `T`* that's skipped.
- Non-dependent conditions in a template: `if constexpr (sizeof(void*) == 8)` —
  fine, still a compile-time branch, both-not-instantiated rule applies inside a
  template.

---

## Andar kya hota hai

- During instantiation, the compiler evaluates the condition; the false branch's
  statements are put in a "discarded statement" state — names that depend on
  template parameters are **not** looked up / checked there. So `*v` in the
  pointer branch, when `T = int`, is never diagnosed.
- Codegen only sees the taken branch → **zero** runtime cost, no dead code in the
  binary, no branch instruction (unlike a runtime `if` the optimizer *might*
  eliminate).
- This is strictly more powerful than `if` + `[[likely]]` for type-dependent
  logic: the optimizer can't drop `if (std::is_pointer_v<T>) *v;` for non-pointer
  `T` because `*v` still has to *compile*.
- `if constexpr` composes with fold expressions, `std::visit` generic lambdas,
  and `constexpr` functions to do compile-time control flow that reads like
  ordinary code.

> **HFT relevance:** `if constexpr` is the everyday tool for **type-dependent
> fast paths with zero runtime branch** — `memcpy` a POD vs field-by-field
> serialization, a SIMD path when `sizeof(T)` and alignment allow, a
> lock-free-safe store when `is_trivially_copyable_v<T>`. Feature flags and
> metrics compile **completely out** with `template <bool>` + `if constexpr`
> (nothing in the binary, no predictor pressure). It replaces the tag-dispatch
> and `enable_if` pair-of-overloads noise with one readable function — fewer
> instantiations, clearer errors. The one thing it can't do: remove a function
> from overload resolution — that still needs a concept (file 10) or SFINAE
> (file 09).

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/04_if_constexpr.cpp
```

`describe()` (branch per type category), `serialize()` (trivial → memcpy),
compile-time `factorial`. Write a `to_bytes(const T&)` that handles arithmetic
types, `std::string`, and `std::array<byte, N>` with three `if constexpr`
branches, and a `static_assert(always_false_v<T>)` else.

---

## ⚠️ Traps

### Trap 1 — plain `if` where you need `if constexpr`
```cpp
template <class T> auto f(T v) { if (std::is_pointer_v<T>) return *v; else return v; }
// ❌ `*v` must compile for T = int too -> error. `if constexpr`.
```

### Trap 2 — `static_assert(false)` in the else
```cpp
else { static_assert(false, "unhandled"); }   // ⚠️ fires unconditionally (pre-C++23). Use static_assert(always_false_v<T>)
```

### Trap 3 — missing `else`, function falls off the end
```cpp
template <class T> int f(T) { if constexpr (cond<T>) return 1; }   // ⚠️ no else -> non-void fn with no return for !cond<T>
```

### Trap 4 — expecting `if constexpr` to prune outside a template
```cpp
void g() { if constexpr (DEBUG) heavy(); else light(); }   // both compile; only affects which runs (like a normal branch the optimizer drops)
```

### Trap 5 — syntax error in the discarded branch
```cpp
if constexpr (cond) { ... } else { totally invalid c++ ( }   // ❌ still parsed -> compile error. Only type-checking-vs-T is skipped
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`if constexpr` and `if` are interchangeable" | Only `if constexpr` doesn't instantiate the dead branch — needed for type-dependent code |
| "`if constexpr` removes an overload" | No — it branches *inside* a chosen function; use a concept/SFINAE to remove one |
| "`static_assert(false)` in the else is fine" | Fires always (pre-C++23) — use a `T`-dependent false |
| "The discarded branch is ignored entirely" | It's parsed; syntax errors still count, only type-checking against `T` is skipped |
| "It has a runtime branch the optimizer removes" | Codegen only emits the taken branch — there's nothing to remove |

---

## Exercises

1. **Why `if constexpr`:** `template <class T> auto first(T& c) { if
   (has_front_v<T>) return c.front(); else return c[0]; }` — why won't this
   compile for a type with `front()` but no `operator[]`, and the fix?

   <details><summary>Answer</summary>

   A plain `if` compiles **both** branches → `c[0]` must be valid even though
   `front()` exists → error. `if constexpr (has_front_v<T>)` discards the `c[0]`
   branch for such `T`.
   </details>

2. **always_false:** write `always_false_v<T>` so `static_assert(always_false_v<T>,
   "unhandled")` in an `else` fires only when that `else` is actually
   instantiated.

   <details><summary>Answer</summary>

   `template <class...> inline constexpr bool always_false_v = false;` — it
   *depends* on the template parameters, so the compiler can't evaluate the
   `static_assert` until instantiation. `static_assert(always_false_v<T>, "...")`.
   </details>

3. **Feature flag:** `template <bool Trace> void step()` — how do you make
   `step<false>()` emit **zero** trace code?

   <details><summary>Answer</summary>

   `void step() { doWork(); if constexpr (Trace) { logTrace(); } }` — for
   `Trace == false` the `logTrace()` call isn't instantiated, so it's not in the
   binary. Also gate any trace-only member with `if constexpr`.
   </details>

4. **Visitor:** in a `std::visit` generic lambda over `variant<int, double,
   std::string>`, dispatch with `if constexpr`. What's the else for?

   <details><summary>Answer</summary>

   `using U = std::decay_t<decltype(x)>; if constexpr (is_same_v<U,int>) ...; else
   if constexpr (is_same_v<U,double>) ...; else { /* U == std::string */ ...; }` —
   the final `else` handles the last alternative (and, with `always_false_v`,
   catches a newly-added alternative at compile time).
   </details>

5. **Runtime vs compile-time:** `if constexpr (sizeof(T) <= 8)` vs `if (sizeof(T)
   <= 8)` inside a template — any observable difference at `-O2`?

   <details><summary>Answer</summary>

   Both produce the same runtime code at `-O2` if both branches happen to compile
   for `T` (the optimizer folds the constant). The difference: `if constexpr`
   also **doesn't instantiate** the dead branch, so it's required when the dead
   branch wouldn't compile for this `T` (e.g. a SIMD path needing `T` to be a
   POD).
   </details>

---

## Interview questions

1. `if constexpr` normal `if` se kaise alag (dead branch instantiation)?
2. Template ke bahar `if constexpr` ka kya matlab?
3. Kya `if constexpr` ek overload ko resolution se hata sakta? (nahi — kyun)
4. Discarded branch mein kya check hota, kya nahi (parse vs type-check)?
5. `static_assert(false)` else mein kyun galat, `always_false_v` kaise?
6. Tag dispatch / `enable_if` pair `if constexpr` se kaise replace hota?

---

## Next
→ [`08-type-traits-deep.md`](08-type-traits-deep.md)
