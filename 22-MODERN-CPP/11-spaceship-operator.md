# 11 — `operator<=>` (the spaceship)

## Prerequisites
- Folder 15 (operator overloading), folder 19 file 05 (`map`/`set` need ordering)
- [`examples/06_spaceship.cpp`](examples/06_spaceship.cpp)

## Yeh topic abhi kyun
C++20 se pehle ek comparable type ke liye **6 operators** (`==`, `!=`, `<`, `<=`,
`>`, `>=`) haath se likhne padte the — boilerplate aur bug-prone. `operator<=>`
ek **three-way comparison** deta (`< 0` / `== 0` / `> 0`), aur `= default` se
compiler baaki 5 (ya 6) generate kar deta. Plus **comparison categories** —
strong / weak / partial ordering — jo "equal" ka matlab define karte.

---

## Defaulted `<=>` — the common case

```cpp
struct Version {
    int major, minor, patch;
    auto operator<=>(const Version&) const = default;   // that's it
};

Version{1,2,3} <  Version{1,3,0};   // true
Version{1,2,3} == Version{1,2,3};   // true
Version{2,0,0} >= Version{1,9,9};   // true
std::sort(versions.begin(), versions.end());            // uses the generated operator<
```

`= default` generates:
- `operator<=>` itself — **member-wise lexicographic**: compare `major`, then
  `minor` if tied, then `patch`.
- **`operator==`** — also member-wise (short-circuits; can be faster than `<=>`
  for equality).
- `<`, `<=`, `>`, `>=`, `!=` — all **synthesized** from `<=>` and `==` (the
  compiler rewrites `a < b` as `(a <=> b) < 0`).

You write **one** function (or zero, with `= default`) and get all six.

---

## Comparison categories

`<=>` returns one of three types, saying **how strong** the ordering is:

| Category | Total order? | "equal" means | Example |
|---|---|---|---|
| **`std::strong_ordering`** | yes | **substitutable** — `a == b` ⇒ `f(a) == f(b)` for any `f` | integers, a price in ticks, a key |
| **`std::weak_ordering`** | yes | **equivalent** but not identical | case-insensitive strings, "same day" for timestamps |
| **`std::partial_ordering`** | **no** — some pairs are **unordered** | equivalent (when comparable) | floating point (NaN is unordered vs everything) |

```cpp
1 <=> 2                    // std::strong_ordering::less
1.0 <=> 2.0               // std::partial_ordering::less
1.0 <=> std::nan("")     // std::partial_ordering::unordered   -> (1.0 < NaN) and (1.0 >= NaN) are BOTH false
```

`examples/06_spaceship.cpp` shows all three plus a `weak_ordering` case-insensitive
string.

The result compares against **`0`** (literal): `(a <=> b) < 0`, `== 0`, `> 0`.
`std::is_lt`, `std::is_eq`, etc. are the named helpers.

---

## Custom `<=>`

When member-wise isn't the rule (compare by a derived key, ignore a field, custom
order):

```cpp
struct Price {
    long ticks;
    int  tickSize;                                   // NOT part of the ordering
    std::strong_ordering operator<=>(const Price& o) const { return ticks <=> o.ticks; }
    bool operator==(const Price& o) const { return ticks == o.ticks; }   // ⚠️ MUST write this yourself
};
```

**With a custom `<=>`, `==` is NOT auto-generated** — you must define it (the
committee chose this so a custom `<=>` can't silently give you a wrong/slow
`==`). `<`, `<=`, `>`, `>=` *are* still synthesized from your `<=>`.

Helper for mixed member types:
```cpp
struct Rec {
    std::string name;
    double score;
    auto operator<=>(const Rec& o) const {
        if (auto c = name <=> o.name; c != 0) return c;      // std::strong_ordering
        return std::partial_ordering(score <=> o.score);     // widen: double gives partial
    }
    // returns std::partial_ordering (the common/weakest category)
};
```

`std::compare_three_way` / `std::compare_weak_order_fallback` etc. help combine
heterogeneous members; the result category is the **weakest** of the members'.

---

## Rewrite rules and mixed-type comparison

The compiler **rewrites** `a @ b` (for `@` in `< <= > >=`) as `(a <=> b) @ 0`,
and `a != b` as `!(a == b)`. It also tries **reversed** candidates: for `a < b`
it will consider `0 < (b <=> a)` if `a <=> b` isn't viable. This makes
heterogeneous comparison work with one operator:

```cpp
struct Meters {
    double v;
    auto operator<=>(const Meters&) const = default;
    auto operator<=>(double km) const { return v <=> km * 1000.0; }   // Meters <=> double
    bool operator==(double km) const  { return v == km * 1000.0; }
};
Meters{1500} < 2.0;    // rewritten via Meters <=> double
2.0 < Meters{1500};    // rewritten + REVERSED: 0 < (Meters{1500} <=> 2.0)
```

Pre-C++20 you needed ~6 free functions per type pair, in both orders.

---

## Andar kya hota hai

- `= default` `<=>` compiles to member-by-member comparison that stops at the
  first non-equal member — the same code you'd write by hand, inlined at `-O2`.
- `= default` `==` is a separate member-wise equality (not `(a<=>b)==0`) so it
  can short-circuit and skip the ordering work for members where equality is
  cheaper (e.g. compare lengths before contents for strings).
- Synthesized `<` etc. are `(a <=> b) < 0` — one call to `<=>`, then a compare of
  the result enum against 0. No extra overhead vs a hand-written `operator<`.
- The comparison-category types are tiny enums-in-a-struct; returning one is
  returning an `int`-sized value.
- `partial_ordering::unordered` is why `<`, `>`, `<=`, `>=`, `==` can **all** be
  false for one pair (NaN) — the synthesized operators check `< 0` / `> 0` /
  `== 0`, and `unordered` is none of those.

> **HFT relevance:** modest but real. Domain value types — a price in integer
> ticks, an order id, a `(symbol, venue)` key, a `Version` — get all six
> comparisons from one `= default` line, so they drop into `std::map`/`std::set`,
> `std::sort`, and `std::lower_bound` (folder 19 files 05, 12) with zero
> boilerplate and no chance of an inconsistent hand-written `<` vs `<=`. Use
> **`strong_ordering`** for keys (substitutable — safe as a map key and for
> dedup). Be deliberate with **`partial_ordering`** on anything holding a
> `double` (a NaN price makes the type unsortable in the "all false" sense — you
> may want to reject NaN at construction and use `strong_ordering` on the
> underlying integer representation). Custom `<=>` when comparing by a subset of
> fields (ignore a cached/derived member). Zero runtime cost — it's the same
> comparison code, just written once.

---

## Hands-on

```bash
./build.ps1 22-MODERN-CPP/examples/06_spaceship.cpp
```

Write: a `Tick` struct with `= default` `<=>` and put it in a `std::set`; a
`Price` with custom `<=>` by `ticks` only (and the mandatory `==`); a type with a
`double` member — observe the `partial_ordering` "all comparisons false vs NaN".

---

## ⚠️ Traps

### Trap 1 — custom `<=>` without `==`
```cpp
struct P { int v; auto operator<=>(const P& o) const { return v <=> o.v; } };
P{1} == P{1};   // ❌ no `==` -- a CUSTOM <=> does not synthesize ==. Add `bool operator==(const P&) const`
```

### Trap 2 — `= default` `<=>` on a type with a `double` and expecting a total order
```cpp
struct M { double x; auto operator<=>(const M&) const = default; };   // returns partial_ordering
std::sort(v.begin(), v.end());   // ⚠️ fine unless a NaN is present -> then the comparator isn't a strict weak order -> UB
```

### Trap 3 — comparing the result against a variable, not the literal `0`
```cpp
auto c = a <=> b;
if (c < someVar) ...   // ⚠️ compare against the literal 0: `if (c < 0)`. The category types only compare to 0
```

### Trap 4 — wrong category (too strong)
```cpp
// Returning strong_ordering from a case-insensitive string compare is a LIE ("HELLO" and "hello" aren't substitutable).
// Use weak_ordering.
```

### Trap 5 — expecting `<=>` where the type needs a specific `<` (e.g. a custom sort predicate)
```cpp
// std::sort with the DEFAULT comparator uses operator<. <=> gives you that for free, but a lambda comparator is separate.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`= default` `<=>` gives 5 operators" | Six — including `==` (which it generates member-wise, separately) |
| "A custom `<=>` also generates `==`" | No — you must write `==` yourself; only `< <= > >=` are synthesized |
| "`partial_ordering` still gives a total sort" | No — unordered pairs make `<`, `>`, `==` all false; not a strict weak order |
| "`(a <=> b) < c` for any `c`" | The result compares only to the literal `0` |
| "`<=>` has runtime overhead vs hand-written operators" | Same code, inlined — you just write it once |

---

## Exercises

1. **What's generated:** `struct T { int a; std::string b; auto operator<=>(const
   T&) const = default; };` — list every operator you can now use and how `T{1,"x"}
   < T{1,"y"}` is evaluated.

   <details><summary>Answer</summary>

   `<=>`, `==`, `!=`, `<`, `<=`, `>`, `>=`. `T{1,"x"} < T{1,"y"}` → the compiler
   rewrites as `(T{1,"x"} <=> T{1,"y"}) < 0` → compare `a` (1 == 1, tie), then
   `b` (`"x" <=> "y"` → less) → the `<=>` result is `less` → `< 0` is true.
   </details>

2. **Category choice:** for each, pick strong/weak/partial: (a) an order id, (b)
   a `double` price, (c) "same trading session" (date-only) comparison of
   timestamps.

   <details><summary>Answer</summary>

   (a) `strong_ordering` — ids are substitutable. (b) `partial_ordering` — NaN is
   unordered (or reject NaN and use `strong_ordering` on the bit pattern /
   integer ticks). (c) `weak_ordering` — two timestamps in the same session are
   *equivalent* for this comparison but not identical.
   </details>

3. **Custom + ==:** write `Money { long cents; std::string currency; }` ordered
   by `cents` only (ignore `currency`), with the required `==`.

   <details><summary>Answer</summary>

   `std::strong_ordering operator<=>(const Money& o) const { return cents <=>
   o.cents; } bool operator==(const Money& o) const { return cents == o.cents; }`
   — `currency` is deliberately not compared.
   </details>

4. **NaN behaviour:** `M{1.0} < M{nan}`, `M{1.0} > M{nan}`, `M{1.0} == M{nan}`
   for `struct M { double x; auto operator<=>(const M&) const = default; };` —
   values?

   <details><summary>Answer</summary>

   All three are `false`. `1.0 <=> nan` is `partial_ordering::unordered`, which is
   not `< 0`, not `> 0`, not `== 0` → every synthesized comparison is false.
   </details>

5. **Pre-C++20 equivalent:** how many functions did you write for a type to be
   fully comparable *and* comparable against a second type, both orders?

   <details><summary>Answer</summary>

   6 for the type itself (`== != < <= > >=`), plus 6 more for `T @ U`, plus 6 for
   `U @ T` (reversed) = up to 18, all consistent by hand. `<=>` + one `==`
   collapses this to ~2, with the compiler synthesizing and reversing the rest.
   </details>

---

## Interview questions

1. `= default` `operator<=>` — kitne operators generate, `==` bhi?
2. Custom `<=>` ke saath `==` kyun likhna padta?
3. Comparison categories — strong / weak / partial, "equal" ka matlab har mein?
4. `partial_ordering::unordered` — NaN ke saath sab comparisons false kyun?
5. Compiler `a < b` ko kaise rewrite karta (`(a<=>b) < 0`, reversed candidates)?
6. `<=>` ki runtime cost (zero) — boilerplate se kya bacha?

---

## Next
→ [`12-attributes.md`](12-attributes.md)
