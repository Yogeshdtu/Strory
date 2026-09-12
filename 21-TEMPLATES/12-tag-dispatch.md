# 12 — Tag dispatch and overload-based dispatch

## Prerequisites
- [`09-sfinae.md`](09-sfinae.md), [`07-if-constexpr.md`](07-if-constexpr.md)
- Folder 19 file 08 (iterator categories / tags)

## Yeh topic abhi kyun
**Tag dispatch** = ek trait ko ek chhote tag type mein badlo, aur overload
resolution ko us tag pe branch karne do. Yeh SFINAE se **simpler** hai (no
`enable_if`, no substitution failure) aur `if constexpr` se **purana** — par STL
(`std::advance`, `std::distance`, allocator machinery) isi pe khadi hai, aur kuchh
cases mein aaj bhi behtar hai.

---

## The mechanism

```cpp
// 1. tag TYPES -- empty structs, often in a hierarchy
struct input_iterator_tag {};
struct forward_iterator_tag       : input_iterator_tag {};
struct bidirectional_iterator_tag : forward_iterator_tag {};
struct random_access_iterator_tag : bidirectional_iterator_tag {};

// 2. helper overloads, one per tag
template <class It>
void advance_impl(It& it, long n, random_access_iterator_tag) { it += n; }              // O(1)

template <class It>
void advance_impl(It& it, long n, bidirectional_iterator_tag) {                          // O(n), handles n<0
    if (n >= 0) while (n--) ++it;
    else        while (n++) --it;
}

template <class It>
void advance_impl(It& it, long n, input_iterator_tag) { while (n-- > 0) ++it; }          // O(n), forward only

// 3. the public function picks the tag from a trait and dispatches
template <class It>
void advance(It& it, long n) {
    advance_impl(it, n, typename std::iterator_traits<It>::iterator_category{});
}
```

Calling `advance(vecIt, 5)` → the tag is `random_access_iterator_tag` → overload
resolution (exact match) picks the `+= n` version. Calling `advance(listIt, 5)` →
`bidirectional_iterator_tag` → the loop version. **Resolved at compile time, zero
cost.**

The tag hierarchy matters: if there's no exact `forward_iterator_tag` overload,
`forward_iterator_tag` **converts to its base** `input_iterator_tag` → the
input-iterator overload is used. So you only write overloads for the categories
whose behaviour actually differs.

---

## Where the STL uses it

- **`std::advance`, `std::distance`, `std::next`, `std::prev`** — iterator
  category tags (folder 19 file 08). `std::distance` on a random-access iterator
  is `last - first` (`O(1)`); on anything else it's a counting loop (`O(n)`).
- **`std::copy`, `std::fill`, `std::uninitialized_copy`** — dispatch on
  `is_trivially_copyable` (via a `true_type`/`false_type` tag) to a `memmove` /
  `memset` fast path.
- **`std::allocator_traits`** — dispatches on whether the allocator provides
  `construct` / `pointer` / propagation traits.
- **`std::move_if_noexcept`** — dispatches on `is_nothrow_move_constructible`
  (folder 18 file 13) to move vs copy during `std::vector` reallocation.

---

## Tag dispatch vs the alternatives

| Approach | When it shines | Downside |
|---|---|---|
| **Tag dispatch** | an existing tag hierarchy (iterators); want "pick nearest ancestor overload" semantics; pre-C++17 | needs the helper-overload boilerplate; extra function |
| **`if constexpr`** (file 07) | one function, a few branches on traits; C++17+ | can't do "nearest ancestor" fallback as elegantly; all branches in one body |
| **SFINAE / `enable_if`** (file 09) | removing an overload from a set the caller can also hit directly | unreadable, bad errors |
| **Concepts** (file 10) | same as SFINAE but readable; C++20+ | needs C++20 |
| **Overload on a real argument type** | the types themselves differ (not a trait) | only works when you have distinct argument types |

Modern default: **`if constexpr` + a trait** for internal branching, **concepts**
for overload inclusion/exclusion. Reach for tag dispatch when you're extending
something that already uses tags, or when the ancestor-conversion fallback is
exactly what you want.

---

## Building your own tags

```cpp
// tag from a trait
template <bool B> struct bool_tag {};
using true_tag  = bool_tag<true>;
using false_tag = bool_tag<false>;

template <class T>
void store_impl(std::byte* dst, const T& v, true_tag)  { std::memcpy(dst, &v, sizeof(T)); }   // trivially copyable
template <class T>
void store_impl(std::byte* dst, const T& v, false_tag) { v.serialize_to(dst); }

template <class T>
void store(std::byte* dst, const T& v) {
    store_impl(dst, v, bool_tag<std::is_trivially_copyable_v<T>>{});
}
```

Or a hierarchy for "levels of capability":
```cpp
struct slow_path  {};
struct fast_path  : slow_path {};      // fast_path is-a slow_path -> falls back if no fast overload
struct simd_path  : fast_path {};
```

---

## Andar kya hota hai

- The tag object is an **empty struct** passed by value — the ABI passes it in no
  register / it's elided entirely at `-O2`. It exists only to steer overload
  resolution.
- Overload resolution on the tag argument is ordinary: an exact match beats a
  derived-to-base conversion. So `random_access_iterator_tag` prefers a
  `random_access_iterator_tag` overload, but if only a `forward_iterator_tag`
  overload exists, the standard conversion (derived→base) makes it viable.
- Everything is compile-time. `advance(it, n)` compiles directly to `it += n` (or
  the loop) with no trace of the tag — same as if you'd hand-written the right
  version.
- `if constexpr` achieves the same *branching* but the "use the nearest ancestor
  implementation" behaviour needs an explicit chain of `if constexpr`s or a
  concept-subsumption setup; tag dispatch gets it for free from
  derived-to-base conversion ranking.

> **HFT relevance:** tag dispatch is mostly something you **read** in the STL and
> in older performance libraries — knowing it lets you follow how `std::copy`
> reaches `memmove` or how `std::advance` becomes `+= n`. In new hot-path code
> you'd write the same fast-path selection with **`if constexpr` + a trait**
> (`is_trivially_copyable`, `is_contiguous`, alignment) — one readable function,
> the dead branch not instantiated, zero runtime cost. The one place tag dispatch
> stays natural is extending iterator/range machinery that already speaks tags.
> Either way the result is identical: the compiler emits exactly one specialized
> path, fully inlined, no dispatch.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/04_if_constexpr.cpp   # the modern equivalent
```

Write `myDistance(first, last)` with tag dispatch: `O(1)` `last - first` for
random-access, `O(n)` loop otherwise. Then rewrite it with `if constexpr` on
`std::random_access_iterator<It>`. Write a `copyBytes` that dispatches to
`memcpy` for trivially copyable element types.

---

## ⚠️ Traps

### Trap 1 — no fallback overload for a mid-hierarchy tag
```cpp
// If you only write random_access and input overloads, a `forward_iterator_tag` argument
// converts to input_iterator_tag (its base) -> the input overload. That's the intended fallback -- design for it.
```

### Trap 2 — passing the tag by something other than a fresh value
```cpp
advance_impl(it, n, cat);   // ⚠️ if `cat` is a named object of a base type you lose the exact-match. Pass `Cat{}`
```

### Trap 3 — tag dispatch where `if constexpr` is simpler
```cpp
// Two helper overloads + a forwarding function, for a single boolean trait, when one `if constexpr` would do.
```

### Trap 4 — forgetting `typename` on the traits access
```cpp
advance_impl(it, n, std::iterator_traits<It>::iterator_category{});   // ❌ needs `typename ...::iterator_category`
```

### Trap 5 — a tag with data
```cpp
struct my_tag { int level; };   // ⚠️ tags should be EMPTY -- they're for overload selection, not carrying values
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Tag dispatch has runtime cost" | The tag is an empty struct, elided at `-O2` — compile-time selection only |
| "You need an overload per tag" | Only per tag whose behaviour differs — the hierarchy provides fallback via base conversion |
| "Tag dispatch and SFINAE are the same" | Tag dispatch is plain overload resolution on an argument; no substitution failure involved |
| "It's obsolete" | `if constexpr` / concepts replace it for new code, but the STL is built on it — you must read it |
| "Tags carry information" | They're empty markers; data goes in real parameters |

---

## Exercises

1. **distance:** implement `myDistance` with tag dispatch for random-access
   (`O(1)`) vs everything else (`O(n)`).

   <details><summary>Answer</summary>

   `template <class It> auto d_impl(It a, It b, std::random_access_iterator_tag)
   { return b - a; }` / `template <class It> auto d_impl(It a, It b,
   std::input_iterator_tag) { long n = 0; for (; a != b; ++a) ++n; return n; }` /
   `template <class It> auto myDistance(It a, It b) { return d_impl(a, b,
   typename std::iterator_traits<It>::iterator_category{}); }`
   </details>

2. **Fallback:** you write overloads for `random_access_iterator_tag` and
   `input_iterator_tag` only. What happens for a `std::list` iterator
   (`bidirectional_iterator_tag`)?

   <details><summary>Answer</summary>

   `bidirectional_iterator_tag` derives from `forward_iterator_tag` which derives
   from `input_iterator_tag`. No exact overload → derived-to-base conversion →
   the `input_iterator_tag` overload is selected (the `O(n)` forward loop). Fine
   for `distance`; for `advance` you'd want a `bidirectional` overload to support
   negative `n`.
   </details>

3. **Trait tag:** dispatch `serialize(dst, v)` to `memcpy` for trivially
   copyable `T`, else `v.write(dst)`, using a `bool_tag`.

   <details><summary>Answer</summary>

   `template <class T> void s_impl(std::byte* d, const T& v, std::true_type) {
   std::memcpy(d, &v, sizeof v); }` / `... std::false_type) { v.write(d); }` /
   `template <class T> void serialize(std::byte* d, const T& v) { s_impl(d, v,
   std::is_trivially_copyable<T>{}); }`
   </details>

4. **Modernize:** rewrite exercise 3 with `if constexpr`. Fewer moving parts?

   <details><summary>Answer</summary>

   `template <class T> void serialize(std::byte* d, const T& v) { if constexpr
   (std::is_trivially_copyable_v<T>) std::memcpy(d, &v, sizeof v); else
   v.write(d); }` — one function, no helper overloads, no forwarding call. Yes,
   fewer parts.
   </details>

5. **When tag dispatch still wins:** name a scenario where tag dispatch is a
   better fit than `if constexpr`.

   <details><summary>Answer</summary>

   Extending an algorithm across an existing multi-level capability hierarchy
   (iterator categories, or your own `slow_path`/`fast_path`/`simd_path`) where
   you want "use the most capable implementation available, otherwise fall back
   to the nearest ancestor" — derived-to-base conversion ranking gives that for
   free; an `if constexpr` chain has to spell every level.
   </details>

---

## Interview questions

1. Tag dispatch — trait ko tag mein, phir kya (overload resolution)?
2. Tag hierarchy (iterator categories) ka fayda — fallback kaise?
3. Tag dispatch vs SFINAE — kya fark (koi substitution failure nahi)?
4. Tag dispatch vs `if constexpr` — modern default kya, tag kab better?
5. `std::advance` / `std::distance` isse kaise use karte?
6. Tag object empty kyun hona chahiye?

---

## Next
→ [`13-template-metaprogramming.md`](13-template-metaprogramming.md)
