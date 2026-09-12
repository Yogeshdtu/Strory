# 01 — Why templates

## Prerequisites
- Folder 15–16 (classes, OOP, virtual dispatch), folder 18 (move, forwarding)
- Folder 19 (the STL is all templates), folder 20 file 17 (cache-aware DSA)

## Yeh topic abhi kyun
Ab tak humne templates **use** kiye (`std::vector<int>`, `std::sort`). Ab unhe
**likhna** aur samajhna. Templates C++ ka **compile-time code generation** hai —
ek code likho jo har type ke liye kaam kare, **bina runtime cost** ke. HFT mein
yeh virtual functions ki jagah leta, kyunki dispatch compile time pe resolve
hota (`examples/08`: virtual ~2.5 ns/call vs template ~1.1 ns/call).

---

## The problem: code duplication

Without generics, "max of two things" needs one function per type:

```cpp
int    maxi(int a, int b)       { return a < b ? b : a; }
double maxd(double a, double b) { return a < b ? b : a; }
std::string maxs(const std::string& a, const std::string& b) { return a < b ? b : a; }
// ... and one more every time a new type shows up
```

Every copy is identical except the type. Copy-paste bugs, N places to fix, no
support for types you didn't foresee.

**Alternatives that don't work well:**
- **`void*` + function pointers** (the C way) — loses type safety, needs casts,
  can't inline, boxing overhead. `qsort` vs `std::sort`: `std::sort` is ~2× faster
  because it inlines the comparison (folder 20 file 04).
- **A common base class + virtual functions** — forces heap allocation and
  indirection, needs an "is-a" relationship that `int` and `std::string` don't
  share, and every call is an indirect non-inlined call (folder 16).
- **Macros** — text substitution, no type checking, no scoping, debugging hell.

---

## The template solution

```cpp
template <class T>                    // "for any type T ..."
T myMax(T a, T b) { return a < b ? b : a; }

myMax(3, 7);            // compiler generates myMax<int>
myMax(2.5, 1.5);       // compiler generates myMax<double>
myMax(std::string("a"), std::string("b"));   // myMax<std::string>
```

**A template is a code generator.** `myMax` itself compiles to nothing; each
*instantiation* (`myMax<int>`, `myMax<double>`, …) is a separate real function the
compiler stamps out, type-checked and fully optimized, only for the types you
actually use.

- **Type-safe** — `myMax(3, "x")` is a compile error, not UB.
- **Zero runtime cost** — no vtable, no `void*`, no boxing. `myMax<int>` is the
  same machine code you'd write by hand.
- **Fully inlinable** — the compiler sees the concrete type and body.
- **Open** — works with types defined after the template, including your own.

---

## Generic programming — the STL's founding idea

The STL (folder 19 file 01) is built on templates: **containers**, **iterators**,
and **algorithms** are all templates, decoupled so that `M` containers + `N`
algorithms need `M + N` pieces of code, not `M × N`. `std::sort` is written once
against iterator *requirements* (later: **concepts**) and works on any type that
meets them.

Templates enable:
- **Generic containers** — `std::vector<T>`, `FixedStack<T, N>`.
- **Generic algorithms** — `std::sort`, `std::accumulate`, your own.
- **Compile-time computation** — `constexpr` + templates → tables, type-level
  logic (file 13).
- **Policy-based design** — compose behaviour by picking template arguments, each
  inlined (file 11, file 16).
- **Static polymorphism** — CRTP: virtual-like dispatch, resolved at compile time
  (file 11).

---

## The cost side

Templates aren't free at **build time**:
- **Code bloat** — `std::vector<int>` and `std::vector<double>` are separate
  instantiations → larger binary. Usually fine; sometimes a real problem (file
  15) → `extern template`, type erasure at the edges.
- **Compile time** — the compiler re-parses and instantiates templates per
  translation unit; heavy metaprogramming can dominate build time.
- **Error messages** — a constraint violation deep in an instantiation used to
  produce pages of noise. **Concepts** (file 10) fix this — a one-line "constraint
  not satisfied".
- **Everything in headers** — a template definition must be visible where it's
  instantiated → templates live in headers, coupling build times.

The trade is **compile time + binary size** for **runtime speed + type safety +
reuse**. For latency-critical code that trade is almost always right.

---

## Andar kya hota hai

- **Two-phase compilation**: when the compiler first sees a template, it checks
  syntax and anything not dependent on the template parameters (phase 1). When
  the template is *instantiated* with concrete types, it re-checks the now-known
  parts (phase 2) — this is where "T has no member `foo`" errors appear (file
  14).
- **Instantiation on demand**: `myMax<int>` is only generated if something calls
  it. Unused member functions of a class template are never instantiated (so a
  `std::vector<T>` where `T` isn't sortable is fine as long as you don't call a
  member that needs `<`).
- **One Definition Rule**: identical instantiations in multiple translation units
  are merged by the linker (templates have "vague linkage"). This is why
  definitions go in headers without ODR violations.
- The generated code has **no trace of "template"** — `myMax<int>` in the binary
  is just `int myMax(int, int)`, indistinguishable from a hand-written one.

> **HFT relevance:** templates are *the* zero-cost abstraction mechanism, so
> low-latency C++ leans on them hard: **compile-time dispatch** (templates / CRTP
> / `if constexpr`) instead of `virtual` on the hot path — the call inlines, the
> loop can vectorize, no vtable indirection (`examples/07`, `08`: ~2–4× on the
> call itself, and it unblocks the surrounding optimization). **Policy classes**
> configure a component (locking policy, stats policy, overflow policy) with no
> runtime branch. `std::from_chars`, ring buffers, lock-free queues, and fixed
> containers are all templates so they specialize to your exact `T`. The price —
> longer builds, bigger binaries, header coupling — is paid gladly. Where it
> bites (instantiation bloat, glacial compiles), type erasure is pushed to the
> non-hot edges.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/01_function_templates.cpp
./build.ps1 fast 21-TEMPLATES/examples/08_compile_time_dispatch.cpp
```

`01` shows deduction, non-type params, and overload resolution. `08` measures
virtual vs template vs `variant`+`visit` dispatch. Try `./build.ps1 asm
21-TEMPLATES/examples/08_compile_time_dispatch.cpp` and find the `call` in the
virtual loop that isn't in the template loop.

---

## ⚠️ Traps

### Trap 1 — expecting a template to behave like a runtime generic
```cpp
// myMax(3, 2.5) -> ERROR: T can't be deduced as both int and double.
// Templates don't do implicit conversions during deduction. Cast, or myMax<double>(3, 2.5).
```

### Trap 2 — template definition in a .cpp file
```cpp
// template <class T> T f(T);   in foo.cpp, called from bar.cpp -> "undefined reference".
// Definitions must be visible at instantiation -> put them in the header (file 15).
```

### Trap 3 — assuming zero build cost
```cpp
// #include <regex> / heavy metaprogramming -> noticeable compile-time hit per TU.
```

### Trap 4 — one giant template used with 50 types
```cpp
// 50 instantiations of a 2000-line class template -> binary bloat. Factor common non-dependent code out.
```

### Trap 5 — thinking `void*`/virtual is "simpler and just as fast"
```cpp
// qsort (void* + fn ptr) is ~2x slower than std::sort -- the comparison can't inline. Same story for virtual on hot loops.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Templates have runtime overhead" | Zero — each instantiation is concrete, hand-written-equivalent code |
| "A template is one function" | It's a *generator*; `f<int>` and `f<double>` are two separate functions |
| "Templates do type conversions" | Deduction is exact-match; conversions happen only after the type is fixed |
| "Templates are like C# / Java generics" | Those are one runtime-erased impl; C++ generates specialized code per type |
| "Put template code wherever" | Must be visible at instantiation → headers (or explicit instantiation) |

---

## Exercises

1. **Why not `void*`:** `qsort` takes `void*` + a comparison function pointer;
   `std::sort` is a template. Name two concrete advantages of the template.

   <details><summary>Answer</summary>

   (1) The comparison **inlines** — `std::sort` on ints is ~2× `qsort` because
   `qsort` pays an indirect non-inlined call per comparison. (2) **Type safety** —
   `std::sort` won't compile with a mismatched comparator; `qsort` casts `void*`
   and trusts you.
   </details>

2. **Instantiation count:** a program calls `myMax` with `int`, `int`, `double`,
   `std::string`, `int`. How many instantiations does the compiler generate?

   <details><summary>Answer</summary>

   Three: `myMax<int>`, `myMax<double>`, `myMax<std::string>`. Repeated uses of
   the same type reuse the one instantiation.
   </details>

3. **Header rule:** you write `template <class T> T square(T x) { return x*x; }`
   in `math.cpp` and call it from `main.cpp`. What error, and the fix?

   <details><summary>Answer</summary>

   Link error: `undefined reference to square<int>` — `main.cpp` can't instantiate
   it without the definition. Fix: move `square` to `math.hpp`, or add explicit
   instantiations (`template int square<int>(int);`) in `math.cpp`.
   </details>

4. **Zero-cost check:** compile `myMax(3, 7)` and a hand-written `int maxi(int,
   int)` at `-O2` and compare the assembly. What do you expect?

   <details><summary>Answer</summary>

   Identical — both compile to a `cmp` + `cmov` (or a branch) and a return. The
   template leaves no runtime trace; `myMax<int>` *is* `maxi`.
   </details>

5. **Trade-off:** you have a 3000-line class template instantiated for 40 unit
   types in a trading system. What's the risk and one mitigation?

   <details><summary>Answer</summary>

   Binary bloat + compile time (40 copies of 3000 lines). Mitigations: move
   type-independent logic into a non-template base or free functions;
   `extern template` for the common instantiations; erase the type at API
   boundaries that don't need the speed.
   </details>

---

## Interview questions

1. Templates code duplication ka problem kaise solve karte — `void*`/virtual/macro se behtar kaise?
2. "Template ek code generator hai" — instantiation kya matlab?
3. Templates ki runtime cost kya (zero) — build-time cost kya?
4. Template definition header mein kyun (ODR, instantiation visibility)?
5. HFT hot path pe virtual ki jagah template kyun (`examples/08` numbers)?
6. C++ templates vs Java/C# generics — code generation vs type erasure?

---

## Next
→ [`02-function-templates.md`](02-function-templates.md)
