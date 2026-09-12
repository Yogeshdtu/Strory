# 02 — Function templates

## Prerequisites
- [`01-why-templates.md`](01-why-templates.md)
- Folder 08 (functions, overload resolution), folder 18 (value categories)

## Yeh topic abhi kyun
Function template = ek function jise compiler har type ke liye stamp karta hai.
Iska **argument deduction** samajhna zaroori — 90% baar aap `<...>` likhte hi
nahi, compiler khud nikaal leta. Aur deduction ke rules (conversions nahi hote,
reference stripping, `T` vs `T&` vs `T&&`) exactly kyun aise hain.

`examples/01_function_templates.cpp` sab dikhata hai.

---

## Declaring and calling

```cpp
template <class T>                    // `class` and `typename` are interchangeable here
T identity(T x) { return x; }

template <typename T, typename U>
auto add(T a, U b) { return a + b; }  // two type params, return deduced

identity(42);          // T = int      -> identity<int>
identity(3.14);        // T = double
add(2, 3.5);           // T=int, U=double, returns double

identity<double>(42);  // EXPLICIT T=double -> 42 converts to 42.0
```

`template <class T>` introduces a **template parameter**. The function is a
pattern; calling it with arguments triggers **instantiation** for the deduced (or
explicit) types.

---

## Template argument deduction

Given `template <class T> void f(ParamType p);` and a call `f(expr)`, the
compiler matches `ParamType` against the type of `expr` to find `T`.

### Case 1 — `ParamType` is `T` (by value)
```cpp
template <class T> void f(T p);
int i = 42; const int ci = i; int& ri = i;
f(i);    // T = int
f(ci);   // T = int    -- top-level const is DROPPED (you get a copy anyway)
f(ri);   // T = int    -- reference-ness is DROPPED
f("hi"); // T = const char*  -- array DECAYS to pointer
```
By-value params: top-level `const`/`volatile` and references are stripped; arrays
and functions decay to pointers. `T` is the *decayed* type.

### Case 2 — `ParamType` is `const T&`
```cpp
template <class T> void f(const T& p);
f(i);    // T = int          -> p is const int&
f(ci);   // T = int          -> p is const int&   (const is in ParamType already)
f(ri);   // T = int
f("hi"); // T = char[3]      -> p is const char(&)[3]   -- NO decay for references
```
Reference params **don't decay** — `T` keeps the array/reference structure. This
is why `template <class T> void f(const T& a)` can see `sizeof(a)` for an array
argument.

### Case 3 — `ParamType` is `T&&` (a **forwarding reference**, folder 18 file 05)
```cpp
template <class T> void f(T&& p);
f(i);    // i is an lvalue -> T = int&,  p is int&    (reference collapsing: int& && -> int&)
f(42);   // 42 is an rvalue -> T = int,   p is int&&
```
Only `T&&` where `T` is *this function's own template parameter* is a forwarding
reference. `std::vector<int>&&` is a plain rvalue reference (file 04).

### Deduction does **not** do implicit conversions
```cpp
template <class T> T myMax(T a, T b);
myMax(3, 2.5);   // ERROR: T can't be int AND double at once. myMax<double>(3, 2.5) or myMax(3.0, 2.5)
```
Both arguments must deduce `T` to the *same* type. Once `T` is fixed (explicitly),
normal conversions apply to the arguments.

---

## Return type: `auto`, `decltype`, trailing

```cpp
template <class T, class U>
auto add1(T a, U b) { return a + b; }                    // C++14: deduced from the return statement

template <class T, class U>
auto add2(T a, U b) -> decltype(a + b) { return a + b; } // trailing return type -- a and b are in scope

template <class T, class U>
std::common_type_t<T, U> add3(T a, U b) { return a + b; } // explicit, via a trait

template <class T>
decltype(auto) forwardElem(T&& c, std::size_t i) { return std::forward<T>(c)[i]; }
// decltype(auto) -> preserves reference-ness of c[i] (returns T& not T)
```

`auto` return strips references (`return x;` → returns by value). `decltype(auto)`
preserves exactly what the expression yields — use it when a generic function
must return a reference (a proxy, an element accessor).

---

## Overload resolution with templates

When a name has both non-template and template candidates:

1. Gather all viable functions (deduction succeeds, arguments convert).
2. **A non-template exact match beats a template** — `kind(42)` picks
   `kind(int)` over `kind<T>(const T&)` (`examples/01` section 4).
3. Among templates, the **more specialized** one wins (partial ordering).
4. Otherwise, normal ranking (exact > promotion > conversion).

```cpp
template <class T> void g(T);        // (1)
template <class T> void g(T*);       // (2) -- more specialized
void g(int);                         // (3) -- non-template

g(42);      // (3) -- non-template exact match
g(&x);      // (2) -- more specialized template
g(3.14);    // (1)
```

You generally **don't** specialize function templates (file 05) — add an overload
instead; it interacts better with overload resolution.

---

## Explicit instantiation and `extern template`

```cpp
// in a .cpp -- force the compiler to emit this instantiation HERE, once:
template int myMax<int>(int, int);

// in a header -- tell every other TU "don't instantiate this, it's defined elsewhere":
extern template int myMax<int>(int, int);
```

Used to cut compile time / binary size for a heavily-used instantiation (file
15) — compile it once in one TU instead of in every TU that uses it.

---

## Andar kya hota hai

- Deduction runs a pattern-match of `ParamType` against each argument's type,
  independently per parameter, then requires all deductions of the same `T` to
  agree. No conversions, no promotions — that's a deliberate design choice so
  that `T` is unambiguous and the generated code is predictable.
- The by-value decay rules exist because a by-value parameter *is* a copy — a
  `const`/reference on the source is irrelevant to the copy's type. The
  no-decay-for-references rule exists so generic code can inspect the real type
  (array bounds, etc.).
- Forwarding references + reference collapsing are the machinery behind
  `std::forward` and perfect forwarding (folder 18): `T` encodes the argument's
  value category (`int&` for lvalues, `int` for rvalues), and `std::forward<T>`
  casts back to it.
- Each instantiation is a normal function symbol with vague linkage; the linker
  dedups identical ones across TUs. At `-O2` a call to `myMax<int>` inlines to a
  `cmp`/`cmov` — no call at all.

> **HFT relevance:** function templates are how hot-path helpers stay both
> generic and fully inlined — a `template <class F> void for_each_tick(span<const
> Tick>, F&&)` takes the concrete handler type, so the callback inlines and the
> loop vectorizes (folder 19 file 16). Deduction's "no implicit conversion" rule
> occasionally forces an explicit `<T>` or a cast at a call site — a small price
> for the guarantee that the instantiation is exactly the type you expect (no
> silent `int`→`double` in a fixed-point pipeline). Forwarding references +
> `std::forward` give zero-copy argument passing into factories / `emplace`-style
> APIs. `extern template` trims build time for the handful of instantiations used
> everywhere.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/01_function_templates.cpp
```

Write: a `template <class T> void swapT(T& a, T& b)`; a `print_all` that takes a
container by `const&` and iterates it; a `min_of(std::initializer_list<T>)`.
Then try `myMax(1, 2u)` and read the deduction error, and fix it two ways
(explicit `<T>` and a cast).

---

## ⚠️ Traps

### Trap 1 — deduction can't unify two arguments
```cpp
myMax(1, 2u);   // ❌ int vs unsigned -> T ambiguous. myMax<unsigned>(1, 2u) or myMax(1u, 2u)
```

### Trap 2 — `auto` return dropping a reference you wanted
```cpp
template <class C> auto& front(C& c) { return c[0]; }        // ✅ auto& keeps the reference
template <class C> auto  front(C& c) { return c[0]; }        // ⚠️ returns a COPY of c[0]
```

### Trap 3 — array argument to a by-value template param
```cpp
template <class T> std::size_t len(T a) { return sizeof(a); }
int arr[10]; len(arr);   // ⚠️ T = int* -> returns 8 (pointer size), not 40. Take `const T& a`
```

### Trap 4 — specializing a function template to "override" an overload
```cpp
template <> void g<int*>(int*);   // ⚠️ interacts badly with overload resolution. Add `void g(int*)` overload instead
```

### Trap 5 — `template <class T> void f(T&& x)` and thinking it only takes rvalues
```cpp
// T&& on a deduced T is a FORWARDING reference -- it binds lvalues too. Use `const T&` / `T` for a real "rvalue only" ... (or a concept)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`myMax(1, 2.0)` picks `T = double`" | Deduction fails — `T` can't be both. Convert or specify `<T>` |
| "`template <class T> void f(T x)` keeps `const`/`&`" | By-value deduction strips top-level cv and references; arrays decay |
| "`auto` return type keeps references" | It strips them; use `auto&` or `decltype(auto)` |
| "Specialize a function template to customize it" | Add an overload — it plays nicely with resolution; specialization doesn't |
| "`T&&` means rvalue-only" | On a deduced `T` it's a forwarding reference (binds both) |

---

## Exercises

1. **Deduce it:** for `template <class T> void f(const T& p);` and calls `f(5)`,
   `f(str)` (a `std::string`), `f("lit")`, what is `T` and what is `p`'s type
   each time?

   <details><summary>Answer</summary>

   `f(5)`: `T = int`, `p` is `const int&`. `f(str)`: `T = std::string`, `p` is
   `const std::string&`. `f("lit")`: `T = char[4]`, `p` is `const char(&)[4]` —
   references don't decay.
   </details>

2. **Fix the call:** `template <class T> T avg(T a, T b) { return (a + b) / 2; }`
   — `avg(3, 4)` gives 3, `avg(3, 4.0)` won't compile. Explain both and fix.

   <details><summary>Answer</summary>

   `avg(3, 4)`: `T = int`, `(3 + 4)/2 = 3` (integer division). `avg(3, 4.0)`:
   deduction conflict (`int` vs `double`). Fixes: `avg(3.0, 4.0)`, or
   `avg<double>(3, 4)`, or make it `template <class A, class B> auto avg(A a, B b)
   { return (a + b) / 2.0; }`.
   </details>

3. **decltype(auto):** you write a generic `at(Container& c, size_t i)` that must
   return a reference so the caller can assign through it. Which return type?

   <details><summary>Answer</summary>

   `decltype(auto)` (or `auto&`): `decltype(auto) at(Container& c, size_t i) {
   return c[i]; }` — preserves `c[i]`'s reference-ness so `at(v, 0) = 5;` works.
   Plain `auto` would return a copy.
   </details>

4. **Overload vs template:** given `template <class T> void h(T);` and `void
   h(double);`, what does `h(3.14f)` call, and `h(3.14)`?

   <details><summary>Answer</summary>

   `h(3.14)` — `double` exact match → the non-template `h(double)`. `h(3.14f)` —
   `float`: the template `h<float>` is an exact match, the non-template needs a
   `float→double` promotion → the **template** wins (exact beats conversion).
   </details>

5. **extern template:** your build compiles `std::vector<Order>` methods in 60
   translation units. How do you make it compile once?

   <details><summary>Answer</summary>

   In a shared header: `extern template class std::vector<Order>;` (suppresses
   instantiation in every including TU). In exactly one `.cpp`: `template class
   std::vector<Order>;` (emits it once). Link picks up the single copy.
   </details>

---

## Interview questions

1. Template argument deduction — by-value vs `const T&` vs `T&&` mein kya fark?
2. Deduction conversions kyun nahi karti — `myMax(1, 2.0)` ka kya hota?
3. `auto` vs `decltype(auto)` return type — kab kaunsa?
4. Non-template overload vs template — resolution kaise (exact match rule)?
5. Function template specialize karne ke bajaye overload kyun?
6. `extern template` / explicit instantiation — kya problem solve karta?

---

## Next
→ [`03-class-templates.md`](03-class-templates.md)
