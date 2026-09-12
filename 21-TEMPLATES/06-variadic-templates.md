# 06 — Variadic templates and fold expressions

## Prerequisites
- [`02-function-templates.md`](02-function-templates.md), [`05-specialization.md`](05-specialization.md)
- Folder 18 file 12 (perfect forwarding)

## Yeh topic abhi kyun
Variadic template = "kitne bhi arguments, kisi bhi type ke". `std::tuple`,
`std::make_unique`, `std::variant`, `printf`-style APIs, `emplace_back` — sab
parameter packs pe khade hain. Pre-C++17 inhe **recursion** se unpack karte the;
C++17 ke **fold expressions** ne yeh clean kar diya.

`examples/03_variadic.cpp` dono tareeke dikhata hai.

---

## Parameter packs

```cpp
template <class... Ts>              // Ts is a TEMPLATE PARAMETER PACK (zero or more types)
void f(Ts... args) {               // args is a FUNCTION PARAMETER PACK
    // sizeof...(Ts)  == number of types  == sizeof...(args)
}

f();               // Ts = <>          (empty)
f(1);              // Ts = <int>
f(1, 'a', 2.5);    // Ts = <int, char, double>
```

- `sizeof...(Ts)` / `sizeof...(args)` — the pack size, a `constexpr size_t`.
- A pack must be **expanded** with `...` before you can use its contents:
  `args...` → `arg0, arg1, arg2`.
- **Pack expansion patterns**: `pattern...` expands `pattern` once per pack
  element, substituting that element into the pattern.
  ```cpp
  g(args...);                    // g(a0, a1, a2)
  g(std::forward<Ts>(args)...);  // g(fwd(a0), fwd(a1), fwd(a2))    -- perfect-forward each
  h(f(args)...);                 // h(f(a0), f(a1), f(a2))
  T arr[] = { transform(args)... };
  ```

---

## The old way: recursion + a base case

```cpp
// base case: no more arguments
void printAll() { std::printf("\n"); }

// recursive case: peel the first, recurse on the rest
template <class First, class... Rest>
void printAll(const First& first, const Rest&... rest) {
    std::printf("%s ", to_str(first).c_str());
    printAll(rest...);                          // one fewer argument each call
}
```

Each call instantiates a new function with a shorter pack until the base case.
Works, but: a base-case overload, `O(pack size)` instantiations, and awkward for
simple reductions.

---

## The modern way: fold expressions (C++17)

`(pack OP ...)` expands to `pack0 OP pack1 OP pack2 OP ...` at compile time — **no
recursion, no base case**.

```cpp
template <class... Ts> auto sum(Ts... xs)       { return (xs + ...); }          // unary RIGHT fold: x0 + (x1 + (x2))
template <class... Ts> auto sumL(Ts... xs)      { return (... + xs); }          // unary LEFT  fold: ((x0 + x1) + x2)
template <class... Ts> bool all(Ts... xs)       { return (xs && ...); }         // fold over &&
template <class... Ts> int  countTrue(Ts... xs) { return (0 + ... + (xs?1:0)); }// BINARY fold with an init value

template <class... Ts>
void print(const Ts&... xs) {
    ((std::cout << xs << ' '), ...);            // fold over the COMMA operator -> "do this for each"
    std::cout << '\n';
}

template <class F, class... Ts>
void for_each_arg(F f, Ts&&... xs) {
    (f(std::forward<Ts>(xs)), ...);             // call f on each argument, in order
}
```

The four forms:
| syntax | name | meaning | empty pack |
|---|---|---|---|
| `(pack op ...)` | unary right | `x0 op (x1 op (… op xn))` | error (except `&&`→true, `\|\|`→false, `,`→void) |
| `(... op pack)` | unary left | `(((x0 op x1) op …) op xn)` | same |
| `(pack op ... op init)` | binary right | `x0 op (… op (xn op init))` | `init` |
| `(init op ... op pack)` | binary left | `((init op x0) op …) op xn` | `init` |

Prefer the **binary** form with an explicit init so an empty pack is well-defined.

---

## Real uses

```cpp
// make_unique -- forward a pack into a constructor
template <class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// a tuple-like aggregate
template <class... Ts> struct Bundle { std::tuple<Ts...> data; };

// variadic class template with recursion (pre-C++17 tuple)
template <class... Ts> struct Tuple;
template <> struct Tuple<> {};
template <class Head, class... Tail>
struct Tuple<Head, Tail...> : Tuple<Tail...> { Head value; };

// apply a function to a tuple's elements (C++17)
std::apply([](auto&&... xs){ return (xs + ...); }, myTuple);

// index a pack via std::index_sequence (file 13)
template <class Tuple, std::size_t... Is>
void printTuple(const Tuple& t, std::index_sequence<Is...>) {
    ((std::cout << std::get<Is>(t) << ' '), ...);
}
```

---

## Andar kya hota hai

- Pack expansion is a **compile-time** operation: `f(args...)` is rewritten to
  `f(a0, a1, a2)` before codegen. There's no array, no loop, no `va_list` — each
  argument keeps its exact static type (unlike C varargs, which lose type info
  and only take trivially-copyable types).
- A fold expression compiles to the flat expression `x0 + x1 + x2` — at `-O2`
  that's constant-folded if the args are constants, or a few adds otherwise. No
  function-call overhead, fully inlinable.
- Recursive unpacking generates `N` function instantiations (`printAll<a,b,c>`,
  `printAll<b,c>`, `printAll<c>`, `printAll<>`) → more compile time and code than
  a fold. For simple reductions, folds are strictly better.
- `std::forward<Ts>(args)...` expands the *pattern* `std::forward<Ts>(args)` once
  per element, pairing each `Ts` with its `args` — this is how perfect forwarding
  scales to any arity (folder 18 file 12).

> **HFT relevance:** variadic templates power the **zero-overhead factory /
> emplace** pattern — `pool.emplace<Order>(id, px, qty)` forwards the pack
> straight into `Order`'s constructor with no temporary, no copy (folder 19 file
> 02). A variadic **structured logger** (`log(level, "px=", px, " qty=", qty)`)
> folds over the arguments to format into a preallocated buffer with no
> allocation and no `printf` parsing. `std::tuple<Ts...>` / `std::variant<Ts...>`
> carry fixed heterogeneous sets (a parsed message's fields, a set of message
> types) inline. Fold expressions replace recursive unpacking → fewer
> instantiations, faster builds. C varargs (`...` / `va_list`) are avoided —
> no type safety, and they force promotion.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/03_variadic.cpp
```

Write: `sum` and `all` as folds; a `min_of(Ts...)` fold; a `zip_print(a..., b...)`
style helper; a recursive `Tuple<Ts...>` and a `get<I>` for it. Compare the
recursion-based `printAll` instantiation count to the fold version.

---

## ⚠️ Traps

### Trap 1 — unary fold over an empty pack
```cpp
template <class... Ts> auto sum(Ts... xs) { return (xs + ...); }
sum();   // ❌ empty pack + unary fold over `+` -> ill-formed. Use (0 + ... + xs) so empty -> 0
```

### Trap 2 — forgetting to expand the pack
```cpp
template <class... Ts> void f(Ts... args) { g(args); }   // ❌ `args` is a pack, not a value. g(args...)
```

### Trap 3 — fold-over-comma without parentheses on the side effect
```cpp
(std::cout << xs << ' ' , ...);      // ⚠️ precedence: parenthesize the element expression -> ((std::cout << xs << ' '), ...)
```

### Trap 4 — expecting evaluation order in a plain expansion
```cpp
int arr[] = { f(args)... };   // ✅ order guaranteed (initializer list). But `g(f(args)...)` -- arg eval order unspecified pre-C++17
```

### Trap 5 — using C varargs for type-heterogeneous args
```cpp
void log(const char* fmt, ...);   // ⚠️ no type safety, promotes float->double, UB on mismatch. Use a variadic template
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`Ts... args` is like `printf`'s `...`" | Totally different — packs keep each argument's static type; C varargs erase it |
| "You need recursion to process a pack" | Fold expressions (C++17) handle reductions and per-element side effects |
| "`(xs + ...)` works for any pack" | Unary fold over an empty pack is ill-formed for most operators — use a binary fold with init |
| "Pack expansion is a runtime loop" | It's compile-time rewriting to a flat comma/expression list |
| "Fold and recursion cost the same" | Recursion = N instantiations; a fold = one flat expression |

---

## Exercises

1. **Fold forms:** for `xs = 10, 3, 2` and `op = -`, what does `(xs - ...)`
   (right) evaluate to vs `(... - xs)` (left)?

   <details><summary>Answer</summary>

   Right: `10 - (3 - 2) = 10 - 1 = 9`. Left: `(10 - 3) - 2 = 5`. Association
   direction matters for non-associative operators.
   </details>

2. **Safe empty pack:** write `product(Ts...)` that returns `1` for an empty
   pack.

   <details><summary>Answer</summary>

   `template <class... Ts> auto product(Ts... xs) { return (1 * ... * xs); }` —
   binary left fold with init `1`; empty pack → `1`.
   </details>

3. **Per-element call:** write `push_all(Container& c, Ts&&... xs)` that
   `c.push_back`s each argument, forwarding.

   <details><summary>Answer</summary>

   `template <class C, class... Ts> void push_all(C& c, Ts&&... xs) {
   (c.push_back(std::forward<Ts>(xs)), ...); }` — fold over comma, forwarding
   each.
   </details>

4. **Count matching:** write `count_if_type<Pred, Ts...>` — how many of `Ts`
   satisfy a trait `Pred`.

   <details><summary>Answer</summary>

   `template <template <class> class Pred, class... Ts> constexpr std::size_t
   count_types = (std::size_t{0} + ... + (Pred<Ts>::value ? 1 : 0));` — binary
   fold over `+` with init 0.
   </details>

5. **Recursion → fold:** rewrite a recursive `all_positive(First, Rest...)` (base
   case + recursive case) as a single fold.

   <details><summary>Answer</summary>

   `template <class... Ts> bool all_positive(Ts... xs) { return ((xs > 0) && ...
   && true); }` — binary fold over `&&` with init `true` (empty pack → true). One
   function, no base case.
   </details>

---

## Interview questions

1. Parameter pack — `sizeof...`, expansion `...`, expansion pattern kya?
2. Recursion se pack unpack vs fold expression — kya fark, kaunsa better?
3. Fold expression ke 4 forms — unary/binary, left/right?
4. Empty pack + unary fold ka problem, fix?
5. `std::forward<Ts>(args)...` pattern kaise expand hota (perfect forwarding)?
6. Variadic template vs C varargs (`va_list`) — type safety, promotion?

---

## Next
→ [`07-if-constexpr.md`](07-if-constexpr.md)
