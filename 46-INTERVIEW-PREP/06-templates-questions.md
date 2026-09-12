# 06 — Layer 5: templates, SFINAE, concepts, CRTP

## Prerequisites
Folder `21-TEMPLATES`, `22-MODERN-CPP`. HFT loves compile-time
polymorphism (zero runtime dispatch), so this Layer matters.

---

## A — Template fundamentals

### A1. Template instantiation kab hoti — compile ya link time?
<details><summary>Answer</summary>
**Compile time**, per translation unit, on first use with concrete args.
Isliye template definitions **headers** mein hone chahiye (linker
duplicate instantiations ko merge karta — vague linkage / COMDAT).
`extern template` se ek TU mein instantiate, baaki mein suppress (build
time). (`21`, `24`.)
</details>

### A2. `typename` keyword `T::value_type` se pehle kyun?
<details><summary>Answer</summary>
Compiler ko nahi pata `T::value_type` ek **type** hai ya ek **static
member** (dependent name). `typename T::value_type x;` batata "yeh type
hai". Similarly `template` disambiguator: `t.template get<int>()`. C++20
ne kuch contexts mein `typename` optional kiya. (`21/`.)
</details>

### A3. Function template vs class template — overload/specialization?
<details><summary>Answer</summary>
Function templates **overload** ho sakte (aur non-template overloads ke
saath resolve hote — non-template preferred on tie), par **partial
specialize nahi** ho sakte (full specialization only, and it's often a
trap — prefer overloading). Class templates **partial specialize** ho
sakte (`template<class T> struct S<T*>`). (`21/`.)
</details>

### A4. `template<class T> void f(T x)` vs `template<class T> void f(T& x)`
vs `f(const T& x)` vs `f(T&& x)` — deduction?
<details><summary>Answer</summary>
`f(T x)` — `T` = decayed type (const/ref/array→ptr dropped). `f(T&)` —
lvalue only, `T` keeps const. `f(const T&)` — anything, `T` = bare type.
`f(T&&)` — **forwarding reference**: lvalue → `T` = `U&` (collapses to
`U&`), rvalue → `T` = `U`. Yeh forwarding-reference case perfect
forwarding ka basis hai. (`07`, `21`.)
</details>

### A5. `constexpr` function — guarantee kya?
<details><summary>Answer</summary>
**Compile-time evaluate ho sakta** jab args constant expressions hon aur
result compile-time chahiye (array size, template arg, `constexpr`
variable). Warna runtime pe normal function jaise chalta. `consteval`
(C++20) = **must** be compile-time. `if constexpr` = compile-time branch,
false branch discarded (not even instantiated). (`22-MODERN-CPP`.)
</details>

---

## B — SFINAE & concepts

### B1. SFINAE kya hai — plain English.
<details><summary>Answer</summary>
**Substitution Failure Is Not An Error.** Overload resolution ke dauraan
jab ek template ke args substitute karne pe (signature mein) ill-formed
expression bane, woh candidate chupchap **discard** hota — hard error
nahi. Isse hum overloads ko "enable" kar sakte types ki properties pe
(`std::enable_if`, `void_t`, expression SFINAE). C++20 concepts iska
readable replacement hai. (`21/`.)
</details>

### B2. `std::enable_if` ka ek use, aur concepts se kaise better?
<details><summary>Answer</summary>
`template<class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void f(T)` — sirf integral types ke liye enable. Cryptic, error messages
bad. Concepts: `template<std::integral T> void f(T)` ya
`void f(std::integral auto)` — readable, better diagnostics, subsumption
(ordering by constraint strength). (`21`, `22`.)
</details>

### B3. Concept aur type trait mein fark?
<details><summary>Answer</summary>
Type trait = compile-time bool/value (`std::is_integral_v<T>`) — ek
predicate. Concept = named set of requirements usable directly in
template parameter lists / `requires` clauses, participates in overload
resolution + subsumption. Concepts often built **on** traits. (`22`.)
</details>

### B4. `requires` clause aur `requires` expression — do alag cheezein?
<details><summary>Answer</summary>
`requires`-**clause**: `template<class T> requires Sortable<T> void f(T)` —
constraint check. `requires`-**expression**: `requires(T a, T b) { a < b;
{ a + b } -> std::convertible_to<T>; }` — ek compile-time bool jo check
karta ki yeh operations valid hain. Clause often uses an expression.
(`21`, `22`.)
</details>

---

## C — CRTP & compile-time polymorphism

### C1. CRTP kya hai, ek example.
<details><summary>Answer</summary>
**Curiously Recurring Template Pattern** — `struct Derived : Base<Derived>`.
`Base` `static_cast<Derived*>(this)` se derived ke methods call kar sakta
— **compile-time** "virtual" dispatch, no vtable, inlinable.
```cpp
template<class D> struct Shape { double area() const {
    return static_cast<const D&>(*this).area_impl(); } };
struct Circle : Shape<Circle> { double area_impl() const { return 3.14*r*r; } };
```
Measured: virtual ~23 ns vs CRTP ~2.2 ns (`16`). Trade-off: no runtime
heterogeneous containers (`vector<Shape*>` nahi). (`21`, `36`.)
</details>

### C2. CRTP vs `std::variant` + `std::visit` — kab kaunsa?
<details><summary>Answer</summary>
**CRTP:** ek known type per call site, static, fully inlined, no storage
overhead. Use jab type ek template parameter ho (venue, strategy). **`variant`:**
ek **closed set** of types stored in one object, runtime-chosen but no
heap / no vtable indirection (jump table); use jab tumhe ek collection
of "one of N known types" chahiye. Measured similar (~1.1 ns both, `21`).
Virtual only jab set open/extensible. (`21`, `36`, `43/12`.)
</details>

### C3. Template ka downside HFT builds mein?
<details><summary>Answer</summary>
**Code bloat** — har instantiation apna code (instruction-cache pressure,
`43/06`). **Compile time** — heavy metaprogramming slow builds. **Error
messages** — pre-concepts brutal. Mitigate: `extern template`, type-erase
the cold path, concepts for diagnostics, keep the hot template small.
(`21`, `24`, `43`.)
</details>

### C4. `if constexpr` vs runtime `if` — hot path.
<details><summary>Answer</summary>
`if constexpr (cond)` — `cond` compile-time; false branch **not
instantiated** (can contain code invalid for that T). Zero runtime cost,
no branch in the binary. Runtime `if` — branch predictor, both branches
compiled. HFT: config known at compile time (venue type, feature flags)
→ `if constexpr` / template param → the dead code literally isn't there
(`43/12`).
</details>

### C5. Perfect forwarding — `std::forward` kyun, `std::move` se fark?
<details><summary>Answer</summary>
`std::move(x)` — **always** casts to rvalue. `std::forward<T>(x)` —
casts to rvalue **only if** `T` deduced as a non-reference (i.e. the
caller passed an rvalue); otherwise stays lvalue. Use `forward` in a
function template with a forwarding reference (`T&&`) to pass args on
**preserving their value category**. `move` when you definitely want to
steal. (`07`, `18`, `21`.)
</details>

---

## D — Output prediction / traps

### D1. `template<class T> void f(T&& x)` called with `int lvalue` — `T`
aur `x` ka type?
<details><summary>Answer</summary>
`T` = `int&`, `x`'s type = `int& && ` → collapses to `int&`. (Rvalue
`int` pass karo → `T` = `int`, `x` = `int&&`.) Reference collapsing:
`& &` → `&`, `& &&` → `&`, `&& &` → `&`, `&& &&` → `&&`. (`07`, `21`.)
</details>

### D2. `std::vector<int> v; auto it = v.begin();` — `it` ka type
`int*`?
<details><summary>Answer</summary>
Not guaranteed — `std::vector<int>::iterator` is *contiguous* and often a
raw `int*` (libstdc++ release) but can be a wrapper class (debug mode,
libc++). Code should use `auto` / `iterator`, not assume `int*`.
`std::to_address(it)` / `&*it` for the pointer. (`19`, `05`.)
</details>

### D3. `max(a, b)` as a macro vs template — why template wins.
<details><summary>Answer</summary>
Macro: double-evaluation (`max(i++, j)`), no type safety, no scope,
precedence traps. `template<class T> const T& max(const T& a, const T&
b)` — single evaluation, type-checked, debuggable, ADL-friendly. (Real
`std::max` takes `const T&` and has an `initializer_list` overload.)
(`22`, `45/12` C3.)
</details>

### D4. Two-phase name lookup — kya hai?
<details><summary>Answer</summary>
Template compile hone pe: **phase 1** (definition) — non-dependent names
resolve hote. **Phase 2** (instantiation) — dependent names (jo template
params pe depend karte) resolve hote, including ADL. Isliye base class
members ko `this->member` ya `Base<T>::member` se qualify karna padta
dependent context mein. (`21/`.)
</details>

---

## Interview tips for Layer 5

- Templates ki value-prop HFT mein: **"dispatch ka decision compile-time
  pe le lo → indirect call gaya, inlining khul gaya, dead code gaya
  gaya."**
- SFINAE ke bare mein poochein to "yeh purana tareeka; C++20 concepts se
  readable + better errors" bolo.
- CRTP: ek 4-line example likh sako. Trade-off (no heterogeneous
  container) zaroor mention karo.
- `std::move` vs `std::forward`: "move always rvalue-casts; forward is
  conditional, for forwarding references, preserves category."

## Next
→ [`07-move-semantics-questions.md`](07-move-semantics-questions.md)
