# 14 — Two-phase lookup, `typename` / `template` disambiguators

## Prerequisites
- [`02-function-templates.md`](02-function-templates.md), [`03-class-templates.md`](03-class-templates.md)
- Folder 24 preview (name lookup, ODR)

## Yeh topic abhi kyun
"Yeh code GCC pe chalta, MSVC pe nahi" ya "`typename` kahan lagau?" — dono ke
peeche **two-phase lookup** hai. Compiler template ko **do baar** dekhta: ek baar
definition pe (parameter-independent cheezein), phir instantiation pe
(parameter-dependent). Isse `typename` / `template` disambiguators aur
`this->` kyun chahiye — sab samajh aata.

---

## The two phases

### Phase 1 — at the template **definition**
The compiler checks everything that **doesn't depend** on the template
parameters:
- syntax
- non-dependent names (resolved right here, using the context at the definition)
- non-dependent type checks
- `static_assert` with non-dependent conditions

### Phase 2 — at each **instantiation** (per set of template arguments)
The compiler re-checks the parts that **do depend** on the parameters:
- dependent names (looked up now, with the concrete types known)
- dependent type checks
- `static_assert` with dependent conditions

```cpp
template <class T>
void f() {
    undeclared_function();          // Phase 1 ERROR -- non-dependent, not found now
    T::static_method();             // Phase 2 -- dependent on T, checked at instantiation
    typename T::type x;             // Phase 2 -- `typename` tells phase 1 "this is a type"
}
```

Because non-dependent names are bound in **phase 1**, they use the declarations
visible **at the point of definition**, not at the call site. This is why adding
a function *after* a template that "should" call it doesn't work (ADL and
dependent lookup are the exception — see below).

---

## Why `typename` is required

Inside a template, `T::something` is a **dependent name**. The compiler in phase 1
doesn't know whether `T::something` is a **type** or a **value** (static member).
By default it assumes **value**:

```cpp
template <class T>
void f() {
    T::iterator * p;      // parsed as: MULTIPLICATION  T::iterator * p   (assumes T::iterator is a value)
}
```

To say "it's a type", prefix `typename`:

```cpp
template <class T>
void f() {
    typename T::iterator p;                   // declaration of a variable `p` of type T::iterator
    typename T::value_type x = getVal();
    using It = typename std::vector<T>::iterator;
}
```

Rule: **`typename` before a dependent qualified name that names a type.** Not
needed for non-dependent names (`std::vector<int>::iterator` — fully known) or in
contexts where only a type is allowed (base class list, since C++20 in more
places).

---

## Why `template` is required (the `.template` / `::template` disambiguator)

If a dependent name is a **member template**, and you use it with explicit
template arguments (`<...>`), the compiler needs to know `<` starts a template
argument list, not a less-than:

```cpp
template <class T>
void g(T obj) {
    auto x = obj.get<int>();            // ⚠️ parsed as (obj.get) < int > (...)  -- comparison!
    auto y = obj.template get<int>();   // ✅ `template` says: get is a template, `<` opens its args

    typename T::template rebind<double> r;   // member template that is a type
}
```

Needed only for **dependent** member templates called with explicit args. Common
with allocators (`Alloc::rebind<U>::other`) and some iterator/tuple code.

---

## Why `this->` is sometimes required (dependent base classes)

```cpp
template <class T>
struct Base { void helper() {} int data = 0; };

template <class T>
struct Derived : Base<T> {
    void f() {
        helper();          // ⚠️ Phase 1 ERROR -- `Base<T>` is a DEPENDENT base, not searched now
        this->helper();    // ✅ makes the name dependent -> looked up in phase 2, finds Base<T>::helper
        Base<T>::helper(); // ✅ also works -- explicitly qualified
        data = 1;          // ⚠️ same problem
        this->data = 1;    // ✅
    }
};
```

Names from a **dependent base class** are **not** visible unqualified in phase 1
(the base could be specialized to not have them). Force phase-2 lookup with
`this->`, `Base<T>::`, or a `using Base<T>::helper;` declaration.

---

## ADL and dependent calls — the one thing that *is* deferred

Function calls with **dependent arguments** do their **argument-dependent lookup
(ADL)** at instantiation, so a `swap` / `begin` / `operator<<` defined in the
argument's namespace *is* found even if declared after the template:

```cpp
template <class T>
void sortIt(T& c) {
    using std::swap;         // bring std::swap into scope for the fallback
    swap(c[0], c[1]);        // ADL also finds a `swap` in T's namespace, at instantiation
}
```

This is the "`using std::swap; swap(a, b);`" idiom (folder 18) — it works because
the unqualified `swap` call with dependent args is resolved in phase 2 with ADL.

---

## Compiler differences (historical)

MSVC for years did **not** implement proper two-phase lookup — it deferred
*everything* to instantiation, so code missing `typename` / `this->` compiled on
MSVC but failed on GCC/Clang. Modern MSVC (`/permissive-`, default in new
projects) is conformant. If you see "works on one compiler only" template bugs,
missing disambiguators are the usual cause.

---

## Andar kya hota hai

- The compiler classifies each name in a template as **dependent** (its meaning
  depends on a template parameter) or **non-dependent**. Non-dependent names are
  resolved and bound during parsing (phase 1) using ordinary lookup at that
  point. Dependent names are left as placeholders and resolved during
  substitution (phase 2).
- `T::x` is dependent, and the grammar is ambiguous between "type" and "value"
  without more info → the standard mandates "assume value" → `typename` is the
  override. Same logic for `template` (assume `<` is less-than) and dependent
  bases (don't search them in phase 1).
- ADL for dependent calls is explicitly specified to happen at the point of
  instantiation, using the namespaces of the actual argument types — that's what
  makes customization points (`swap`, `begin`, `operator<<`) work.
- Getting these right is purely a *compile-time* correctness matter — no runtime
  effect. The cost of getting them wrong is a build failure (on a conformant
  compiler) or, worse, a portability bug.

> **HFT relevance:** no runtime angle here — this is about **template code that
> compiles on every toolchain**. Trading codebases build with GCC and Clang (and
> sometimes MSVC for tooling); missing `typename` / `this->` / `.template` is the
> classic "green on my machine, red in CI" bug. Knowing the rules also lets you
> read allocator/iterator/`std::pmr` internals (`Alloc::template rebind<U>`,
> `typename traits::pointer`) and write correct generic infrastructure. The
> `using std::swap; swap(a,b);` idiom (a two-phase/ADL consequence) is standard
> in move-heavy hot-path code. Treat it as hygiene: it costs nothing and prevents
> a whole class of portability failures.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/01_function_templates.cpp
```

Write a `template <class C> void printKeys(const C& m)` that declares `typename
C::key_type k;` and iterates; a `Derived : Base<T>` that needs `this->` to reach
a base member; a function using `obj.template method<int>()`. Compile with
`-Wall` on both GCC and (if available) Clang.

---

## ⚠️ Traps

### Trap 1 — missing `typename` on a dependent type
```cpp
template <class C> void f(C& c) { C::iterator it = c.begin(); }   // ❌ `typename C::iterator it`
```

### Trap 2 — missing `template` on a dependent member template
```cpp
alloc.rebind<U>::other p;   // ❌ (dependent) -> `typename Alloc::template rebind<U>::other p;`
```

### Trap 3 — unqualified name from a dependent base
```cpp
template <class T> struct D : Base<T> { void f() { helper(); } };   // ❌ -> this->helper() / Base<T>::helper()
```

### Trap 4 — expecting a later-declared free function to be found
```cpp
template <class T> void run(T x) { process(x); }   // process defined AFTER this -> not found (unless ADL on T's namespace)
void process(int);   // too late for a non-ADL call
```

### Trap 5 — "it compiles on MSVC so it's fine"
```cpp
// Old MSVC skipped two-phase lookup. Test on GCC/Clang (or MSVC /permissive-) -- missing typename/this-> will surface.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "The compiler checks a template only when it's used" | Two phases: definition-time (non-dependent) and instantiation-time (dependent) |
| "`typename` is optional noise" | Required before a dependent qualified name that is a type — the parser assumes "value" otherwise |
| "Base class members are always visible in a derived template" | Not for a *dependent* base — use `this->` / `Base<T>::` / `using` |
| "A function declared later will be found" | Non-dependent unqualified calls bind at definition; only ADL on dependent args is deferred |
| "Compiles on my compiler = portable" | Non-conformant compilers skip two-phase checks; test on GCC/Clang |

---

## Exercises

1. **Fix it:** `template <class M> void dump(const M& m) { for (M::const_iterator
   it = m.begin(); it != m.end(); ++it) use(it->second); }` — one compile error.

   <details><summary>Answer</summary>

   `M::const_iterator` is a dependent type → needs `typename`: `for (typename
   M::const_iterator it = m.begin(); ...)`. (Or just `for (auto it = m.begin();
   ...)`.)
   </details>

2. **Dependent base:** `template <class T> struct Logger : LogBase<T> { void
   info(const char* s) { write(s); } };` fails. Two fixes.

   <details><summary>Answer</summary>

   `this->write(s);` or `LogBase<T>::write(s);` — or add `using LogBase<T>::write;`
   inside `Logger` so `write` resolves via the (now brought-in) name.
   </details>

3. **`.template`:** when is `obj.template get<0>()` required vs just
   `obj.get<0>()`?

   <details><summary>Answer</summary>

   `.template` is required when `obj`'s type is **dependent** on a template
   parameter and `get` is a member template called with explicit `<...>` args —
   otherwise the parser reads `get < 0 > ()` as comparisons. If `obj`'s type is
   concrete (`std::tuple<int,double>`), plain `obj.get<0>()`... actually you'd
   use `std::get<0>(obj)`; the `.template` case is real for things like
   `x.template as<int>()` where `x`'s type is a template parameter.
   </details>

4. **ADL idiom:** why does `using std::swap; swap(a, b);` work for a type with a
   custom `swap` in its own namespace, but `std::swap(a, b);` doesn't use the
   custom one?

   <details><summary>Answer</summary>

   The unqualified `swap(a, b)` with dependent args triggers ADL at instantiation
   → it considers `swap` in `a`/`b`'s namespace **and** the `std::swap` brought
   in by `using` → overload resolution picks the more specific custom one.
   `std::swap(a, b)` is qualified → no ADL → always the generic `std::swap`.
   </details>

5. **Phase 1 error:** which of these errors at definition time (phase 1)?
   (a) `undeclared();` (b) `typename T::x y;` (c) `T::x * y;` without `typename`
   (d) `static_assert(sizeof(int) == 4);`

   <details><summary>Answer</summary>

   (a) phase 1 — non-dependent, not found. (d) phase 1 — non-dependent condition
   (passes here). (b) fine — `typename` resolves the ambiguity, checked in phase
   2. (c) phase 1 *parses* it as multiplication (no error yet), then phase 2 may
   error if `T::x` is actually a type — so it's a latent bug, diagnosed at
   instantiation.
   </details>

---

## Interview questions

1. Two-phase lookup ke do phases — kaunse checks kab?
2. `typename` dependent name se pehle kyun — parser kya assume karta?
3. `template` / `.template` disambiguator kab chahiye?
4. Dependent base class ke members `this->` ke bina kyun nahi dikhte?
5. ADL dependent call ke liye kab hota (phase 2) — `using std::swap` idiom kyun?
6. Purana MSVC two-phase lookup skip karta tha — kya bug surface hota GCC pe?

---

## Next
→ [`15-instantiation-and-bloat.md`](15-instantiation-and-bloat.md)
