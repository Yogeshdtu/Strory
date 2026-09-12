# 04 — Value categories (lvalue / prvalue / xvalue / glvalue / rvalue)

## Prerequisites
- [`01-copy-constructor.md`](01-copy-constructor.md)
- Folder 13 (references — lvalue refs), folder 05 (expressions)

## Yeh topic abhi kyun
Move semantics samajhne ke liye pehle yeh: har **expression** ki ek **value
category** hoti hai. Yeh decide karta hai ki woh expression `T&` se bind hoti hai
ya `T&&` se, aur isliye — copy hoga ya move. Terms confusing lagte hain par
model simple hai: **do sawaal — identity hai? aur move ho sakti?**

---

## The two questions

Har expression ke liye:

1. **Has identity?** — kya woh ek specific object ko refer karti hai jiska
   address le sakte ho? (`i`, `arr[3]`, `*p` — yes; `42`, `a + b`, `foo()` —
   no)
2. **Can be moved from?** — kya iske resources safely steal kiye ja sakte hain
   (kyunki woh anyway khatam hone wali hai)?

| Has identity? | Movable? | Category | Examples |
|---|---|---|---|
| **yes** | **no** | **lvalue** | `i`, `arr[3]`, `*p`, `s.member`, `++i`, a function name, a string literal |
| **yes** | **yes** | **xvalue** ("eXpiring") | `std::move(x)`, `foo()` where `foo` returns `T&&`, `a[i]` where `a` is an rvalue array |
| **no** | **yes** | **prvalue** ("pure rvalue") | `42`, `true`, `nullptr`, `a + b`, `foo()` returning by value, `T{}`, `&x`, a lambda expression |

Two umbrella terms:
- **glvalue** ("generalized lvalue") = **lvalue or xvalue** — has identity.
- **rvalue** = **prvalue or xvalue** — movable (binds to `T&&`).

```
                       expression
              +------------+------------+
          glvalue                     rvalue
        +-----+-----+              +-----+-----+
     lvalue      xvalue  (xvalue in both)   prvalue
     identity,   identity,                   no identity,
     not movable movable                     movable
```

---

## Which reference binds

```cpp
void f(std::string&);          // #1 non-const lvalue ref
void f(const std::string&);    // #2 const lvalue ref  -- the universal fallback
void f(std::string&&);         // #3 rvalue ref

std::string s = "x";
const std::string cs = "y";

f(s);                // #1 -- s is an lvalue
f(cs);               // #2 -- cs is a const lvalue (can't bind #1, #3)
f(s + "!");          // #3 -- prvalue (result of operator+)
f("literal"s);       // #3 -- prvalue temporary
f(std::move(s));     // #3 -- xvalue (std::move casts to rvalue)
f(std::move(cs));    // #2 -- const xvalue: can't bind #3 (const), falls back to #2 -> COPIES
```

**Key:** `T&` binds only to non-const lvalues. `T&&` binds only to rvalues
(prvalue + xvalue). `const T&` binds to **everything** — which is why it's the
safe read-only parameter, and why `std::move` on a `const` object silently
copies (`examples/03_value_categories.cpp`, `examples/05_std_move_demo.cpp`).

---

## Common gotchas

### An rvalue reference *variable* is an lvalue

```cpp
void g(std::string&& s) {          // s is an rvalue REFERENCE
    // but inside g, the expression `s` is an LVALUE (it has a name, an address)
    h(s);                          // calls h(const std::string&) -- NOT h(string&&)!
    h(std::move(s));               // NOW it's an rvalue again
}
```

"Named rvalue reference" → the *name* is an lvalue. This is exactly why perfect
forwarding needs `std::forward` (file 12) and why move constructors do
`std::move(o.member)` internally (file 06).

### `decltype` — declared type vs value category

```cpp
int x = 0;
decltype(x)    a;   // int      -- declared type of the entity x
decltype((x))  b;   // int&     -- (x) is an expression; its category is lvalue -> T&
decltype(x+0)  c;   // int      -- x+0 is a prvalue -> T
decltype(std::move(x)) d;   // int&&  -- xvalue -> T&&
```

`decltype(expr)` on a bare name → the declared type. `decltype((expr))` (extra
parens, or any non-name expression) → adds a reference based on the value
category: lvalue → `T&`, xvalue → `T&&`, prvalue → `T`.

### Materialization — prvalue → temporary object

A prvalue doesn't *have* an object until it's "materialized" (bound to a
reference, has a member accessed, etc.), at which point it becomes an xvalue
referring to a temporary. C++17's "guaranteed copy elision" (file 11) works by
*delaying* materialization — `return T{};` constructs `T` directly in the
caller's storage, no temporary.

---

## Why this matters for move

```cpp
std::string a = "hello";

std::string b = a;              // a is an LVALUE -> COPY ctor (a must survive)
std::string c = a + "!";        // a+"!" is a PRVALUE -> MOVE ctor (the temporary is expiring)
std::string d = std::move(a);   // std::move(a) is an XVALUE -> MOVE ctor (you promised a is done)
```

The compiler picks copy vs move purely from the **value category of the
initializer**:
- lvalue → copy (the source is presumed to still be needed).
- rvalue (prvalue or xvalue) → move (the source is a temporary, or you've
  explicitly said "I'm done with it" via `std::move`).

---

## Andar kya hota hai

- Value category is a **compile-time** property of expressions — it affects
  overload resolution (which constructor / `operator=` / function overload),
  nothing at runtime directly.
- `std::move(x)` compiles to **nothing** — it's a `static_cast<T&&>` that changes
  the expression's category from lvalue to xvalue. Same bits, different overload.
- prvalues in C++17 are "unmaterialized" until needed → `return f();` where `f`
  returns by value can construct straight into the caller's slot (no temporary,
  no move) — guaranteed elision (file 11).
- The "named rvalue ref is an lvalue" rule is why `T&&` parameters need
  `std::move`/`std::forward` to actually trigger a move downstream.

> **HFT relevance:** value categories are the mechanism behind zero-copy: a
> function that returns a big `std::vector`/`Book`/message by value costs nothing
> extra because the return is a prvalue → constructed in place or moved, never
> copied. Passing `f(makeThing())` similarly. The trap is turning an rvalue back
> into an lvalue by naming it (`auto t = makeThing(); f(t);` copies `t` unless
> you `f(std::move(t))`). Reviewers check hot-path signatures for accidental
> lvalue passing where a move/forward was intended, and for `const` sources that
> silently downgrade a move to a copy.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/03_value_categories.cpp
```

The `probe(T&)` / `probe(const T&)` / `probe(T&&)` overload set reports which
reference each expression binds to; `decltype(x)` vs `decltype((x))`; `std::move`
on a const → falls back to `const&`.

---

## ⚠️ Traps

### Trap 1 — naming an rvalue ref makes it an lvalue
```cpp
void g(T&& x) { consume(x); }   // ⚠️ x is an lvalue here -> consume COPIES. consume(std::move(x))
```

### Trap 2 — `auto t = makeThing(); use(t);`
```cpp
use(t);   // ⚠️ t is an lvalue -> copy. use(std::move(t)) if t is dead after
```

### Trap 3 — `std::move` on a `const`
```cpp
const std::string cs = ...;  std::string s = std::move(cs);   // ⚠️ silently COPIES (const rvalue -> const&)
```

### Trap 4 — `decltype(x)` vs `decltype((x))`
```cpp
decltype((x)) r = ...;   // ⚠️ int& not int (extra parens -> value category)
```

### Trap 5 — thinking a string literal is a prvalue
```cpp
"hello"   // it's an LVALUE (a `const char[6]` with static storage, addressable)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "rvalue = temporary" | rvalue = prvalue OR xvalue; xvalues have identity |
| "`std::move(x)` moves x" | It's a cast to xvalue; a move ctor/assign does the work |
| "A named `T&&` parameter is an rvalue" | The *name* is an lvalue — use `std::move`/`std::forward` |
| "`f(makeThing())` copies the result" | prvalue → moved/elided, never copied |
| "String literals are prvalues" | lvalues (static array with an address) |

---

## Exercises

1. **Categorize:** for each expression, lvalue / prvalue / xvalue?
   `x` (a variable), `42`, `x + 1`, `++x`, `x++`, `std::move(x)`, `arr[i]`,
   `getVectorByValue()`, `getVectorRef()` (returns `T&`), `T{}`.

   <details><summary>Answer</summary>

   `x` lvalue. `42` prvalue. `x + 1` prvalue. `++x` lvalue. `x++` prvalue.
   `std::move(x)` xvalue. `arr[i]` lvalue. `getVectorByValue()` prvalue.
   `getVectorRef()` lvalue. `T{}` prvalue.
   </details>

2. **Which overload:** `void f(int&); void f(const int&); void f(int&&);` — for
   `int i; const int ci = 0;` — `f(i)`, `f(ci)`, `f(5)`, `f(i + 0)`,
   `f(std::move(i))`, `f(std::move(ci))`.

   <details><summary>Answer</summary>

   `f(i)` → `int&`. `f(ci)` → `const int&`. `f(5)` → `int&&`. `f(i + 0)` →
   `int&&` (prvalue). `f(std::move(i))` → `int&&` (xvalue). `f(std::move(ci))` →
   `const int&` (const xvalue can't bind `int&&`).
   </details>

3. **decltype:** `double d = 1.0; const double& r = d;` — what are
   `decltype(d)`, `decltype((d))`, `decltype(r)`, `decltype((r))`, `decltype(d +
   d)`?

   <details><summary>Answer</summary>

   `decltype(d)` → `double`. `decltype((d))` → `double&` (lvalue). `decltype(r)`
   → `const double&` (declared type). `decltype((r))` → `const double&` (lvalue,
   already a reference). `decltype(d + d)` → `double` (prvalue).
   </details>

4. **Fix the copy:** `std::string build(); void store(std::string);  auto s =
   build(); store(s);` — one needless copy. Where, and how to fix?

   <details><summary>Answer</summary>

   `store(s)` copies `s` (an lvalue). If `s` is not used afterward: `store(std::move(s));`.
   Or skip the local entirely: `store(build());` — the prvalue is moved/elided
   straight into the parameter.
   </details>

5. **Named rvalue ref:** `void sink(Buffer&& b) { registry_.push_back(b); }` —
   why does this *copy* `b` into the vector? Fix.

   <details><summary>Answer</summary>

   Inside `sink`, the expression `b` is an lvalue (it has a name), so
   `push_back(b)` calls the **copy** ctor. Fix: `push_back(std::move(b));` — turn
   it back into an rvalue so the move ctor runs.
   </details>

---

## Interview questions

1. Value categories ke 3 primary (lvalue/prvalue/xvalue) — 2-question model?
2. glvalue vs rvalue — kya cover karte?
3. `T&` / `const T&` / `T&&` — kaunsa kis category se bind hota?
4. "Named rvalue reference is an lvalue" — kyun, iska practical impact?
5. `decltype(x)` vs `decltype((x))` — fark?
6. Move vs copy — compiler kaise decide karta (initializer ki category se)?

---

## Next
→ [`05-rvalue-references.md`](05-rvalue-references.md)
