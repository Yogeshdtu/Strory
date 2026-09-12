# 05 — Template specialization

## Prerequisites
- [`03-class-templates.md`](03-class-templates.md), [`04-template-parameters.md`](04-template-parameters.md)
- Folder 19 file 21 (`<type_traits>` are specializations)

## Yeh topic abhi kyun
Specialization = "is generic template ka is (in) type(s) ke liye ek alag version
do". `std::hash<T>`, `std::vector<bool>`, saare type traits — sab specializations
hain. **Full** (exact type) aur **partial** (a pattern) — dono zaroori.
Aur ek trap: function templates ko partially specialize **nahi** kar sakte —
overload karo.

`examples/02_class_templates.cpp` mein `TypeName<T>` full + partial specialized hai.

---

## The primary template + specializations

```cpp
// primary template -- the general case
template <class T>
struct Serializer {
    static std::string to_string(const T& v) { return "<opaque>"; }
};

// FULL specialization -- for exactly `int`
template <>
struct Serializer<int> {
    static std::string to_string(int v) { return std::to_string(v); }
};

// FULL specialization -- for exactly `bool`
template <>
struct Serializer<bool> {
    static std::string to_string(bool v) { return v ? "true" : "false"; }
};

Serializer<double>::to_string(1.5);   // primary  -> "<opaque>"
Serializer<int>::to_string(42);       // int spec -> "42"
Serializer<bool>::to_string(true);    // bool spec -> "true"
```

`template <>` with the concrete type in `<...>` after the name = **explicit
(full) specialization**. It's a completely separate definition — it doesn't even
have to have the same members as the primary (though it usually should).

---

## Partial specialization (class templates only)

A specialization for a **pattern** of arguments, not a single type:

```cpp
template <class T>
struct Serializer<T*> {                          // any pointer type
    static std::string to_string(T* p) {
        return p ? "&" + Serializer<T>::to_string(*p) : "null";
    }
};

template <class T, std::size_t N>
struct Serializer<std::array<T, N>> {            // any std::array
    static std::string to_string(const std::array<T, N>& a) { /* join elements */ return "[...]"; }
};

template <class A, class B>
struct Serializer<std::pair<A, B>> {             // any pair
    static std::string to_string(const std::pair<A, B>& p) {
        return "(" + Serializer<A>::to_string(p.first) + ", " + Serializer<B>::to_string(p.second) + ")";
    }
};
```

The specialization's own template parameter list (`<class T>`) declares what's
still generic; the `<...>` after the name gives the pattern.

**Which specialization is chosen?** The most specialized matching one (partial
ordering — like function overloads). `Serializer<int*>` → the `T*` partial with
`T = int`. `Serializer<int>` → the full `int` specialization.

Type traits are built entirely from partial specialization:
```cpp
template <class T> struct is_pointer      : std::false_type {};
template <class T> struct is_pointer<T*>  : std::true_type {};   // partial: matches any pointer
```

---

## Function templates: **specialize by overloading, not `template <>`**

```cpp
// primary
template <class T> void process(T v)        { /* generic */ }

// ❌ full specialization -- LEGAL but discouraged, interacts badly with overload resolution
template <> void process<int>(int v)        { /* ... */ }

// ✅ just add an overload
void process(int v)                          { /* ... */ }
template <class T> void process(T* p)        { /* pointer overload -- NOT a partial spec, an overload */ }
```

Why: overload resolution picks the best match **before** it considers explicit
specializations. A specialization attaches to whichever primary was chosen —
which can be a surprising one if there are overloads. **Function templates cannot
be partially specialized at all** — `template <class T> void f<T*>(T*)` is
ill-formed. Use an overload (`template <class T> void f(T*)`), or dispatch inside
one function with `if constexpr` (file 07) or a concept (file 10).

---

## Rules and gotchas

- A specialization must be declared **before** the first use that would
  instantiate the primary for those arguments — otherwise the primary is
  instantiated and you get an ODR violation or the wrong code.
- Specializations of standard templates: you **may** specialize `std::hash<YourType>`
  and a few others in namespace `std` for your own types; you may **not** add
  new overloads to `std`.
- `std::vector<bool>` is a full specialization of `std::vector` — it packs bits,
  and its `operator[]` returns a proxy, not `bool&` (folder 19 file 02). A
  cautionary tale: a specialization that changes the interface surprises everyone.
- You can partially specialize on: which template it is, NTTP values/patterns,
  `const`/`volatile`/reference/pointer qualifiers, and combinations.

---

## Andar kya hota hai

- When the compiler needs `Serializer<X>`, it collects the primary and every
  visible specialization whose pattern can match `X`, runs partial ordering to
  pick the most specialized, and instantiates that. No runtime cost — it's all
  resolved during instantiation.
- `is_pointer<T*>` works because `T*` is *more specialized* than `T`: any `T*`
  also matches `T`, but not vice versa, so the pointer partial wins for pointer
  arguments and the primary handles everything else.
- The "function templates can't be partially specialized" rule exists because
  function overload resolution already provides the mechanism (and mixing the two
  would make resolution order ambiguous). Overloads are chosen by argument
  matching; a `template <>` specialization is just a specific instantiation of
  one already-chosen overload.

> **HFT relevance:** specialization is how a generic serializer / hasher /
> formatter gets a **fast path for POD types** (`memcpy` the whole struct) and a
> field-by-field path for the rest — chosen at compile time, no branch (though
> `if constexpr` on a trait, file 07, is now the more common way to do the same).
> `std::hash<OrderId>` specialization plugs your key type into `std::unordered_map`.
> The lesson from `std::vector<bool>`: a specialization that changes behaviour or
> the interface is a footgun — prefer a differently-named type. On the hot path,
> the practical rule is: prefer `if constexpr` + traits (one function, easy to
> read) over a spread of specializations, and use partial specialization mainly
> for trait definitions.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/02_class_templates.cpp
```

Write: `template <class T> struct TypeTag { static constexpr const char* name = "?"; };`
with full specializations for `int`/`double`/`char` and a partial for `T*` and
`std::vector<T>`. Then try to partially specialize a function template and read
the error.

---

## ⚠️ Traps

### Trap 1 — partially specializing a function template
```cpp
template <class T> void f(T);
template <class T> void f<T*>(T*);   // ❌ ill-formed. Use an overload: template <class T> void f(T*);
```

### Trap 2 — specialization declared after first use
```cpp
Serializer<int>::to_string(1);         // instantiates the PRIMARY for int
template <> struct Serializer<int> {}; // ⚠️ too late -> ODR violation / ignored
```

### Trap 3 — specializing `std::` with an overload instead of a specialization
```cpp
namespace std { void swap(MyType&, MyType&); }   // ❌ adding to std = UB. Provide a `swap` in YOUR namespace (ADL finds it)
```

### Trap 4 — full-specializing a function template to "customize" it, then adding overloads
```cpp
// The specialization binds to a primary chosen by overload resolution -- which overload it binds to can surprise you.
```

### Trap 5 — a specialization with a different interface
```cpp
// std::vector<bool>::operator[] returns a proxy, not bool&. Callers of the generic code break. Don't do this.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Function templates can be partially specialized" | They can't — use overloads or `if constexpr` |
| "`template <>` is the normal way to customize a function template" | Add an overload; specialization interacts badly with resolution |
| "A specialization must match the primary's members" | It's a separate definition; it *should* match for sanity, but isn't required to |
| "You can't touch `std`" | You may specialize `std::hash` etc. for your types; you may not add overloads |
| "Partial specialization is one exact type" | It's a *pattern* (`T*`, `pair<A,B>`, `array<T,N>`) — the primary's params stay generic |

---

## Exercises

1. **Trait by hand:** write `is_reference<T>` using partial specialization (match
   `T&` and `T&&`).

   <details><summary>Answer</summary>

   `template <class T> struct is_reference : std::false_type {}; template <class
   T> struct is_reference<T&> : std::true_type {}; template <class T> struct
   is_reference<T&&> : std::true_type {};`
   </details>

2. **Most specialized:** for `Serializer` with primary `<T>`, full `<int>`, and
   partial `<T*>`, which is chosen for `Serializer<int*>`? `Serializer<int>`?
   `Serializer<double*>`?

   <details><summary>Answer</summary>

   `Serializer<int*>` → the `<T*>` partial with `T = int`. `Serializer<int>` →
   the full `<int>` specialization. `Serializer<double*>` → the `<T*>` partial
   with `T = double`.
   </details>

3. **Function "specialization":** you want `swap` to do something special for
   your `Buffer` type. How, correctly?

   <details><summary>Answer</summary>

   Define a free `void swap(Buffer& a, Buffer& b) noexcept` **in `Buffer`'s
   namespace** (ideally a hidden friend). Generic code that does `using std::swap;
   swap(a, b);` will find yours via ADL. Do **not** specialize `std::swap`.
   </details>

4. **POD fast path:** implement `write(Sink&, const T&)` that `memcpy`s trivially
   copyable `T` and calls `T::serialize` otherwise — with specialization, then
   with `if constexpr`. Which is cleaner?

   <details><summary>Answer</summary>

   Specialization needs a primary + a partial on a trait predicate (awkward — you
   can't partial-spec on `is_trivially_copyable_v<T>` directly, you'd use
   `enable_if`). `if constexpr (std::is_trivially_copyable_v<T>)` in one function
   is far cleaner and is the modern idiom (file 07).
   </details>

5. **`std::hash`:** plug `struct OrderId { uint32_t v; uint64_t s; };` into
   `std::unordered_map`. What must you specialize?

   <details><summary>Answer</summary>

   `template <> struct std::hash<OrderId> { std::size_t operator()(const OrderId&
   o) const noexcept { /* combine v and s */ } };`, plus `operator==` for
   `OrderId`. This is a permitted specialization of a standard template for a
   user type.
   </details>

---

## Interview questions

1. Full vs partial specialization — syntax aur kab kaunsa?
2. Function templates partially specialize kyun nahi kar sakte — kya karein?
3. Type traits partial specialization se kaise bante (`is_pointer<T*>`)?
4. Specialization "before first use" rule — na follow karo to kya?
5. `std::` templates specialize karne ke rules?
6. `std::vector<bool>` kis tarah ka specialization, kya galat hai usme?

---

## Next
→ [`06-variadic-templates.md`](06-variadic-templates.md)
