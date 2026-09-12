# 15 — Instantiation model, code bloat, compile times

## Prerequisites
- [`14-two-phase-lookup.md`](14-two-phase-lookup.md)
- Folder 24 preview (compilation / linking, ODR)

## Yeh topic abhi kyun
Templates ka runtime cost zero hai — **build cost nahi**. Har `.cpp` file har
template ko dobara instantiate karta, binary mein duplicate code aata, aur heavy
metaprogramming compile time kha jaata. Bade codebases (jaise trading systems)
mein yeh real problem hai. Yeh lesson: model, symptoms, aur fixes (`extern
template`, type erasure, hidden friends).

---

## The instantiation model

1. A template definition lives in a **header**.
2. Every `.cpp` that `#include`s it and *uses* `Foo<int>` **instantiates**
   `Foo<int>` — generates its code — in **that translation unit**.
3. So `Foo<int>` gets compiled in TU-A, TU-B, TU-C… once per TU that uses it.
4. The linker sees N copies of `Foo<int>`'s functions (they have **vague /
   COMDAT linkage**) and **merges them to one** in the final binary.

The merge means no ODR violation and one copy at runtime — but the compiler still
did the work N times, and each `.o` file carried the code until link time.

Costs:
- **Compile time** — N instantiations of the same thing, parsed and codegen'd per
  TU. For a heavy class template used everywhere, this dominates.
- **Object file size** — each `.o` is fat with template code (stripped at link,
  but slow to write/read).
- **Binary size** — many *distinct* instantiations (`Foo<int>`, `Foo<double>`,
  `Foo<Order>`, `Foo<Trade>`, …) each add real code. Not deduped — they're
  different functions.

---

## Symptoms of template bloat

- Link times creeping up; `.o` files hundreds of KB.
- `nm --size-sort binary | tail` shows dozens of near-identical
  `Foo<X>::method` symbols.
- A one-line change triggers a multi-minute rebuild (everything including the
  fat header recompiles).
- Debug builds enormous; `strip` removes a lot.
- Instantiating a big template with 40 types → 40× the code for the type-dependent
  parts.

---

## Fix 1 — `extern template` (suppress + one explicit instantiation)

```cpp
// in a widely-included header:
extern template class std::vector<Order>;         // "don't instantiate me here"
extern template int process<Order>(const Order&);

// in exactly ONE .cpp:
template class std::vector<Order>;                 // "instantiate me here, once"
template int process<Order>(const Order&);
```

Every TU that includes the header skips instantiation and just references the
symbol; the one `.cpp` emits it. Cuts compile time for the common instantiations.
`std::string` and a few others are `extern template`'d in libstdc++'s own headers
for this reason.

---

## Fix 2 — factor out type-independent code

A class template where most of the body doesn't depend on `T`:

```cpp
// ❌ everything re-instantiated per T
template <class T>
class SmallVec {
    T* data_; std::size_t size_, cap_;
    void grow() { /* 40 lines of pointer bookkeeping, mostly T-independent */ }
    // ...
};

// ✅ push the T-independent part into a non-template base
class SmallVecBase {
protected:
    void* data_; std::size_t size_, cap_, elemSize_;
    void grow();                         // one copy, compiled once
};
template <class T>
class SmallVec : SmallVecBase {
    T& operator[](std::size_t i) { return static_cast<T*>(data_)[i]; }   // thin T-dependent shims
};
```

This is how `std::vector` (via `_Vector_base`), `std::function`, and many Boost
containers are structured — a type-erased or `void*`-based core, a thin typed
wrapper.

---

## Fix 3 — type erasure at the boundary

Where the speed of a monomorphized template doesn't matter (a cold path, an API
boundary), erase the type:

- `std::function<Sig>` instead of a template callable parameter (folder 19 file
  16) — one implementation, accepts anything, at the cost of an indirect call.
- `std::span<const std::byte>` / `std::string_view` instead of templating on the
  container type.
- A small hand-rolled `AnyRef` / `function_ref` for "call this, don't own it".
- A `virtual` interface for a genuinely open, cold set of implementations.

Trade: one indirect call + no inlining, vs one instantiation instead of 40. Fine
off the hot path.

---

## Fix 4 — hidden friends, `constexpr` over recursive templates

- **Hidden friends** (folder 15) — `friend bool operator==(const T&, const T&)`
  defined inside the class: only found by ADL, not added to the overload set for
  every comparison everywhere → less overload-resolution work, smaller symbol
  tables.
- **`constexpr` functions** instead of recursive-template value computation
  (file 13) — a `constexpr` loop is one function; `Fact<N>` is N class
  instantiations.
- **Fewer template parameters** — every extra `<Policy>` combination is a new
  instantiation. Keep the policy set small, or default aggressively.
- **`-ftime-report` / `-ftime-trace`** (Clang) / `templight` — profile which
  templates cost the most to instantiate.

---

## Andar kya hota hai

- Template instantiations get **`__attribute__((weak))` / COMDAT** linkage: the
  linker keeps one and discards the rest, and it's not an ODR violation to have
  many identical ones. Non-identical ones (different `T`) are just different
  symbols.
- The compiler *caches* instantiations **within** a TU (`Foo<int>` used twice in
  one file → instantiated once) but **not across** TUs — hence the N× compile
  cost.
- Precompiled headers, `-fno-implicit-templates` + explicit instantiation, and
  C++20 **modules** (file 16 in folder 22) attack the "re-parse the header every
  TU" cost from different angles; modules also naturally instantiate once.
- `extern template` works because the symbol has external linkage from the one
  defining TU; other TUs emit an *undefined reference* the linker resolves.
- Bloat that survives to the binary is *distinct* instantiations. `-Os` /
  identical-code-folding (`-ffunction-sections` + `--icf=all` with lld/gold) can
  merge instantiations that compiled to byte-identical code (e.g. `vector<int>`
  and `vector<unsigned>` methods).

> **HFT relevance:** build time is developer iteration latency, and trading
> codebases are template-heavy (fixed containers, policy classes, CRTP families,
> `<chrono>`, serialization). The discipline: keep the hot-path templates lean
> and monomorphized (that's where the runtime win is), but **`extern template`**
> the handful of instantiations used in hundreds of TUs, **factor T-independent
> bulk** into non-template bases, and **erase types at cold boundaries**
> (`std::function` for config-time callbacks, `std::span` for API params). Profile
> with `-ftime-trace`. C++20 modules cut the re-parse cost significantly where
> the toolchain supports them. The goal is: pay the template tax only where it
> buys runtime speed, not everywhere by default.

---

## Hands-on

```bash
g++ -std=c++20 -O2 -c 21-TEMPLATES/examples/02_class_templates.cpp -o /tmp/ct.o
nm --size-sort --demangle /tmp/ct.o | tail -20        # see the instantiated symbols
g++ -std=c++20 -ftime-report -c 21-TEMPLATES/examples/03_variadic.cpp -o /dev/null   # where time goes
```

Add a second `.cpp` that also uses `FixedStack<int, 4>`, compile both, and check
with `nm` that the linker merges the methods. Then add `extern template class
FixedStack<int, 4>;` and one explicit instantiation and observe the `.o` shrink.

---

## ⚠️ Traps

### Trap 1 — a huge class template instantiated with many types
```cpp
// 3000-line Engine<T> for 40 order types -> 40 copies of the T-dependent parts. Factor out the T-independent bulk.
```

### Trap 2 — `extern template` without the matching explicit instantiation
```cpp
extern template class Foo<int>;   // in the header
// ... and NO `template class Foo<int>;` anywhere -> link error: undefined reference
```

### Trap 3 — recursive-template metaprogramming in a hot header
```cpp
template <int N> struct X { ... X<N-1> ... };   // N instantiations PER use, PER TU. constexpr function instead
```

### Trap 4 — templating on the container type where a span/view would do
```cpp
template <class C> void parse(const C& bytes);   // C = vector / array / string / ... -> N instantiations
void parse(std::span<const std::byte> bytes);    // one function, all callers convert
```

### Trap 5 — assuming the linker dedups distinct instantiations
```cpp
// Foo<int> and Foo<long> are DIFFERENT symbols -- not merged (unless ICF proves them byte-identical).
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Templates are compiled once" | Instantiated per TU that uses them; linker merges *identical* ones later |
| "The linker dedups all template code" | Only byte-identical duplicates; `Foo<int>` vs `Foo<double>` stay separate |
| "`extern template` alone suppresses the code" | You also need one `template class Foo<T>;` to define it, or it's an undefined reference |
| "Bloat is a runtime problem" | It's compile time + `.o` size + binary size; runtime is unaffected (one copy) |
| "More policy parameters = more flexible, no cost" | Each `<Policy...>` combination is a distinct instantiation |

---

## Exercises

1. **Where's the bloat:** a `template <class T> class Ring` has a 60-line
   `grow()` that only touches `data_` (a `void*`), `size_`, `cap_`. Used with 20
   types. How much of `grow()` is duplicated, and the fix?

   <details><summary>Answer</summary>

   All 20 copies of `grow()` are essentially identical (it doesn't touch `T`).
   Move `grow()` (and the `void* data_` bookkeeping) into a non-template
   `RingBase`; `Ring<T>` inherits it and only adds typed `operator[]` / element
   construction shims. 1 copy of `grow()` instead of 20.
   </details>

2. **extern template:** you have `std::vector<Order>` used in 80 `.cpp` files.
   Write the two lines and say where each goes.

   <details><summary>Answer</summary>

   In a common header: `extern template class std::vector<Order>;`. In exactly
   one `.cpp` (e.g. `order.cpp`): `template class std::vector<Order>;`. The 80
   TUs now reference the one definition instead of each instantiating it.
   </details>

3. **Type erasure trade:** a config system takes a callback per event type. On
   the hot path you'd template it; here it's called once at startup. Template
   parameter or `std::function`?

   <details><summary>Answer</summary>

   `std::function<void(const Event&)>` — it's cold, so the indirect call is
   irrelevant, and you avoid an instantiation of the registration/dispatch
   machinery per callback type. Reserve the templated form for the per-tick path.
   </details>

4. **ICF:** why might `--icf=all` (identical code folding) shrink a binary with
   `vector<int>` and `vector<unsigned>`?

   <details><summary>Answer</summary>

   Many `vector<int>` and `vector<unsigned>` member functions compile to
   **byte-identical** machine code (same size, same ops — `int` and `unsigned`
   arithmetic is identical at the bit level). ICF detects the duplicate function
   bodies and keeps one, redirecting both symbols to it.
   </details>

5. **Profile:** which flag shows you which templates dominate compile time, and
   what would you do with a hot one?

   <details><summary>Answer</summary>

   GCC `-ftime-report` (coarse) or Clang `-ftime-trace` (per-symbol, viewable in
   a flame graph). For a template that dominates: `extern template` its common
   instantiations, factor out T-independent code, replace recursive-template
   computation with `constexpr`, reduce policy-parameter combinations, or
   precompile the header / use a module.
   </details>

---

## Interview questions

1. Template instantiation model — per-TU kyun, linker kya karta (COMDAT)?
2. Template bloat ke symptoms — compile time, `.o` size, binary size?
3. `extern template` + explicit instantiation — kaise kaam karta, kya bachata?
4. T-independent code non-template base mein kyun nikaalte (`std::vector` example)?
5. Type erasure (`std::function` / `std::span`) — kab, kya trade?
6. Distinct instantiations (`Foo<int>` vs `Foo<long>`) linker dedup karta? (nahi, unless ICF)

---

## Next
→ [`16-templates-in-hft.md`](16-templates-in-hft.md)
