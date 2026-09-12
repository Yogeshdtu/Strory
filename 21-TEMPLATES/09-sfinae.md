# 09 — SFINAE

## Prerequisites
- [`08-type-traits-deep.md`](08-type-traits-deep.md), [`05-specialization.md`](05-specialization.md)
- Folder 08 (overload resolution)

## Yeh topic abhi kyun
**SFINAE** = "Substitution Failure Is Not An Error". Jab template argument
substitute karne se **signature ill-formed** ho jaati, woh candidate chup-chaap
overload set se hat jaata — **error nahi**. Isse aap ek overload ko "sirf agar T
ye property rakhta ho" par enable kar sakte ho. **C++20 concepts (file 10) iska
readable replacement hai** — par purane code aur interview ke liye SFINAE aana
zaroori.

`examples/05_sfinae.cpp` mein `enable_if`, detection idiom, expression SFINAE.

---

## The rule

During template argument deduction/substitution, if substituting the deduced (or
explicit) arguments into the function's **signature** (return type, parameter
types, template parameter list) produces an **ill-formed type or expression**,
the compiler:

- **does not** raise an error,
- **removes** that specialization from the overload set,
- continues with the other candidates.

If *no* candidate survives → *then* it's an error ("no matching function").

**Only the immediate context counts.** An error deep in a function *body* is a
hard error, not SFINAE. SFINAE sees: template param list, parameter types, return
type, `noexcept` specifier, default template args.

```cpp
template <class T>
typename T::value_type first(const T& c) { return c[0]; }   // T::value_type in the return type

first(std::vector<int>{1,2,3});   // T::value_type = int -> OK
first(42);                        // int::value_type -> ill-formed -> SFINAE, this overload removed
                                  // (if there's another `first`, it's tried; else: error)
```

---

## `std::enable_if` — the classic gate

```cpp
template <bool B, class T = void> struct enable_if {};              // primary: NO `type` member
template <class T>                struct enable_if<true, T> { using type = T; };  // spec: `type` exists only when B
template <bool B, class T = void> using enable_if_t = typename enable_if<B, T>::type;
```

`enable_if_t<false, T>` is **ill-formed** (`enable_if<false>::type` doesn't
exist) → SFINAE. `enable_if_t<true, T>` is just `T`.

### Three places to put it

```cpp
// 1. extra defaulted template parameter (the modern SFINAE placement)
template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void f(T x) { /* integral version */ }

template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
void f(T x) { /* float version */ }

// 2. return type
template <class T>
std::enable_if_t<std::is_pointer_v<T>, bool> isNull(T p) { return !p; }

// 3. parameter type (less common)
template <class T>
void g(T x, std::enable_if_t<std::is_enum_v<T>, int> = 0);
```

Placement (1) is preferred — it doesn't clutter the return type and works for
constructors (which have no return type). The `= 0` gives the phantom parameter a
default so callers never pass it.

---

## Expression SFINAE

Test whether an **expression** is valid, via `decltype` in the signature:

```cpp
// only viable if `a + b` compiles
template <class A, class B>
auto tryAdd(const A& a, const B& b) -> decltype(a + b) { return a + b; }

const char* tryAdd(...) { return "(not addable)"; }   // C-varargs fallback -- worst match, always viable

tryAdd(2, 3);              // -> 5           (decltype(2+3) = int)
tryAdd(NoAdd{}, NoAdd{});  // -> "(not addable)"   (decltype fails -> SFINAE -> falls to ...)
```

`std::declval<T>()` (file 08) gives you a `T` expression without constructing
one:

```cpp
template <class T>
auto call_size(const T& c) -> decltype(std::declval<const T&>().size(), void()) {
    /* enabled only if c.size() is valid */
}
```

The `, void()` (comma operator) discards the actual type and just uses the
expression's validity as the gate.

---

## The detection idiom (recap from file 08)

```cpp
template <class, class = void> struct has_size : std::false_type {};
template <class T> struct has_size<T, std::void_t<decltype(std::declval<T&>().size())>> : std::true_type {};
```

`std::void_t<IllFormed>` → substitution failure → the partial spec is dropped →
`false_type`. This is SFINAE applied to a class-template partial specialization
rather than a function overload.

C++17's `std::is_detected` / the "detection toolkit" (in `<experimental>` or
hand-rolled) packages this.

---

## Tag dispatch — SFINAE's simpler cousin

Instead of enabling/disabling overloads, route to helper overloads by a **tag
type** computed from a trait:

```cpp
template <class It> void advance_impl(It& it, int n, std::random_access_iterator_tag) { it += n; }
template <class It> void advance_impl(It& it, int n, std::input_iterator_tag)         { while (n--) ++it; }

template <class It> void advance(It& it, int n) {
    advance_impl(it, n, typename std::iterator_traits<It>::iterator_category{});
}
```

No `enable_if`, no SFINAE failure — just overload resolution on the tag argument.
`std::advance` / `std::distance` work exactly this way. `if constexpr` (file 07)
now does the same in one function.

---

## Why concepts replaced this

`enable_if` SFINAE is:
- **unreadable** — `template <class T, std::enable_if_t<std::is_integral_v<T> &&
  !std::is_same_v<T, bool>, int> = 0>` vs `template <std::integral T> requires
  (!std::same_as<T, bool>)`.
- **terrible errors** — "no matching function call" with a dump of every rejected
  candidate and why, pages long.
- **fragile** — the `= 0` phantom param, the "two overloads with the same
  signature except the enable_if" ODR trap, ordering surprises.

Concepts (file 10) give the same overload control with named constraints,
one-line errors, and subsumption-based ordering. **Use concepts in new code.**
Know SFINAE for reading libraries and pre-C++20 code, and for the interview.

---

## Andar kya hota hai

- SFINAE is a consequence of how overload resolution builds its candidate set:
  for each template, deduce args, substitute into the signature; if that
  substitution is ill-formed *in the immediate context*, the candidate isn't
  added (rather than the whole compile failing). The set is then ranked normally.
- "Immediate context" = the substitution itself. `enable_if_t<false, T>` failing
  is in the immediate context. A `static_assert` or an undefined member *inside
  the body* is not — that's a hard error once the function is selected and
  instantiated.
- `decltype(expr)` in a trailing return type puts `expr` in the immediate context
  → expression SFINAE. `std::declval<T>()` makes `expr` writable without a
  constructible `T`.
- All of it is compile-time. The selected overload is a normal function; at `-O2`
  it inlines like any other. SFINAE has **zero** runtime footprint — it only
  chooses *which* code is generated.

> **HFT relevance:** you'll meet `enable_if` SFINAE constantly in libraries
> (libstdc++, Boost, Abseil, older in-house code) — you need to read it. In new
> low-latency code, **concepts** (file 10) do the same job with better errors and
> no phantom-parameter fragility, so that's what you write. The practical stack:
> **concept** to include/exclude an overload, **`if constexpr` + a trait** to
> branch inside it, **tag dispatch** where an existing tag hierarchy (iterators)
> makes it natural. All resolve at compile time → the hot path gets exactly one
> concrete, inlinable function with no dispatch cost.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/05_sfinae.cpp
```

Write, with `enable_if`: a `to_string(T)` with separate overloads for integral,
floating-point, and "has `.str()`" types. Then rewrite the whole thing with
concepts (file 10) and compare the error you get from a bad call
(`to_string(SomeStruct{})`).

---

## ⚠️ Traps

### Trap 1 — error in the body, expecting SFINAE
```cpp
template <class T> void f(T x) { x.nonexistent(); }
f(42);   // ❌ HARD ERROR -- the body isn't the immediate context. SFINAE only sees the signature
```

### Trap 2 — two overloads differing only by `enable_if` in the return type... ODR-ish trap
```cpp
template <class T> std::enable_if_t<A, void> f(T);
template <class T> std::enable_if_t<B, void> f(T);
// same signature after substitution can be a redefinition. Put the enable_if in a template param instead.
```

### Trap 3 — forgetting `= 0` on the phantom parameter
```cpp
template <class T, std::enable_if_t<cond, int>>   // ❌ no default -> caller must pass it. Add ` = 0`
void f(T);
```

### Trap 4 — `enable_if` condition that isn't dependent on the template params
```cpp
template <class T, std::enable_if_t<sizeof(int) == 4, int> = 0> void f(T);
// not SFINAE per-T -- either always enabled or a hard error. Depend on T.
```

### Trap 5 — using SFINAE where `if constexpr` or a concept is cleaner
```cpp
// One function + if constexpr beats a pair of enable_if overloads for branching (not for removing an overload).
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "SFINAE catches errors in the function body" | Only in the *signature* (immediate context); body errors are hard errors |
| "`enable_if` is a runtime check" | Compile-time — it removes an overload from consideration |
| "`if constexpr` can remove an overload" | No — it branches inside one function; use SFINAE/concepts to remove |
| "SFINAE and concepts are unrelated" | Concepts are the modern front-end; the underlying "constraint fails → candidate removed" is the same idea |
| "Put `enable_if` anywhere" | Prefer a defaulted extra template parameter — works for ctors, doesn't pollute the return type |

---

## Exercises

1. **Immediate context:** why does `template <class T> void f(T) { typename
   T::x y; }` hard-error for `f(1)`, but `template <class T> typename T::x
   f(T);` SFINAEs?

   <details><summary>Answer</summary>

   In the first, `T::x` is in the *body* — not the immediate context — so once
   `f<int>` is selected and instantiated, `int::x` is a hard error. In the
   second, `T::x` is in the *return type* (immediate context) → substituting
   `int` fails there → the candidate is silently removed.
   </details>

2. **enable_if overloads:** write `describe(T)` returning `"int"` for integral
   `T` and `"real"` for floating-point `T`, using `enable_if` on a defaulted
   template parameter.

   <details><summary>Answer</summary>

   `template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0> const
   char* describe(T){ return "int"; }` and a second with
   `std::is_floating_point_v<T>` returning `"real"`.
   </details>

3. **Expression SFINAE:** write `maybe_call_clear(T& c)` that calls `c.clear()`
   if it exists and does nothing otherwise (two overloads).

   <details><summary>Answer</summary>

   `template <class T> auto maybe_call_clear(T& c) -> decltype(c.clear(), void())
   { c.clear(); }` and `template <class T> void maybe_call_clear(...) {}` (the
   `...` overload is the always-viable fallback, ranked worst).
   </details>

4. **Detection:** write `has_serialize<T>` — does `T` have a member
   `serialize(std::vector<std::byte>&)`?

   <details><summary>Answer</summary>

   `template <class, class = void> struct has_serialize : std::false_type {};
   template <class T> struct has_serialize<T, std::void_t<decltype(
   std::declval<T&>().serialize(std::declval<std::vector<std::byte>&>()))>> :
   std::true_type {};`
   </details>

5. **Concept rewrite:** convert `template <class T, std::enable_if_t<
   std::is_integral_v<T> && sizeof(T) >= 4, int> = 0> void f(T);` to a concept.

   <details><summary>Answer</summary>

   `template <class T> concept WideIntegral = std::integral<T> && sizeof(T) >= 4;`
   then `template <WideIntegral T> void f(T);` — same constraint, readable, and a
   one-line error if it's not met.
   </details>

---

## Interview questions

1. SFINAE ka full form aur rule — "immediate context" kya?
2. `std::enable_if` kaise kaam karta (missing `::type` → substitution failure)?
3. `enable_if` ko 3 jagah rakh sakte — kaunsi preferred, kyun?
4. Expression SFINAE — `decltype` + `declval` se member/expression detect kaise?
5. Tag dispatch SFINAE se kaise alag (overload on a tag arg)?
6. Concepts ne SFINAE ki kaunsi 3 problems fix kiin?

---

## Next
→ [`10-concepts.md`](10-concepts.md)
