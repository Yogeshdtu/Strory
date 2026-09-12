# 02 — Struct initialization

## Prerequisites
- [`01-what-is-a-struct.md`](01-what-is-a-struct.md)
- `03-VARIABLES-DATA-TYPES/10-initialization-forms.md`

## Yeh topic abhi kyun
Struct ko bharne ke ki tareeke hain — aggregate init, default member initializers,
designated initializers (C++20). Sahi choose karne se "garbage member" bugs
khatam aur code readable.

---

## Aggregate initialization — `{}` with members in order

```cpp
struct Order { std::uint64_t id; std::string symbol; double price; std::int64_t qty; };

Order a{1001, "AAPL", 192.34, 100};       // members DECLARATION ORDER mein
Order b = {1001, "AAPL", 192.34, 100};    // = optional
Order c{};                                 // ALL members value-initialized (0 / "" / etc.)
Order d{1001, "AAPL"};                     // price, qty -> value-init (0.0, 0)
```

- Members fill **declaration order** mein — position matters.
- Fewer values → rest are **value-initialized** (0 for scalars, `""` for
  `std::string`, recursively `{}` for sub-structs).
- `Order c{}` → all zero. `Order c;` → **garbage** (for trivial members).
- `{}` catches **narrowing**: `Order{1, "x", 1, 3.5}` → error (3.5 → int64 qty).

An "aggregate" = a struct/array with no user-declared constructors, no private
non-static data, no virtual/base classes. Plain data structs qualify.

---

## Default member initializers (DMI)

```cpp
struct Config {
    int    retries    = 3;                 // default value baked into the type
    double timeout    = 5.0;
    bool   verbose    = false;
    std::string name  = "default";
};

Config a;                                  // {3, 5.0, false, "default"}  -- NOT garbage
Config b{10};                              // {10, 5.0, false, "default"}
Config c{10, 1.0, true, "prod"};           // all overridden
```

`= value` on a member → used unless the initializer overrides it. Makes `Config
a;` safe (no garbage). Very common for config / options structs.

⚠️ DMI + aggregate init still works (C++14+), and members without a DMI still
value-init when you use `{}`.

---

## Designated initializers (C++20)

```cpp
struct Rect { int x = 0, y = 0, w = 0, h = 0; };

Rect r{ .x = 10, .w = 100, .h = 50 };      // y stays 0 (its DMI / value-init)
```

- **Name the members** you set → readable, order-independent-ish, skip defaults.
- ⚠️ **Must be in declaration order** (C++20, unlike C): `.w` before `.x` → error.
- ⚠️ Can't mix designated and positional: `Rect{10, .h = 50}` → error.
- Only for aggregates.

Great for structs with many optional-ish fields (test data, config, API calls).

---

## Nested / array members

```cpp
struct Line { Point a; Point b; };
Line l{ {0, 0}, {3, 4} };                  // nested braces
Line m{ .a = {0, 0}, .b = {3, 4} };        // designated + nested

struct Board { int cells[9]; };
Board bd{ {1, 2, 3, 4, 5, 6, 7, 8, 9} };
Board z{};                                  // cells all 0
```

---

## `Order c;` vs `Order c{};` vs `Order c{...}`

| Form | Trivial members (`int`, `double`) | Non-trivial (`std::string`) |
|---|---|---|
| `Order c;` | **garbage** (if local) | default-constructed (`""`) — but scalars still garbage |
| `Order c{};` | **zero** | default-constructed |
| `Order c{1, "x"};` | given values; rest value-init | given / default |

**Rule: always `{}` or `{...}`.** Never bare `Order c;` for a struct with scalar
members you'll read.

---

## Andar kya hota hai

- Aggregate init → the compiler assigns each member in turn (or `memset` + assign
  for a partial `{}`); no constructor call (there isn't one).
- DMI → the compiler emits the default value's initialization wherever it isn't
  overridden.
- `Order c{}` on a trivial struct → often a single `memset(0)`. On a struct with
  a `std::string` → zero the scalars + call `std::string()` for the string.
- Designated init → same as positional, just written by name; zero runtime
  difference.

> **HFT relevance:** Config / parameter structs use DMI so a plain declaration is
> valid and self-documenting (no forgotten field = garbage). Wire/message structs
> are aggregates with explicit layout (file 08) and are usually zeroed (`msg{}`)
> or fully assigned. Designated initializers make test fixtures and "options
> structs" (the modern alternative to 8 default function arguments — folder 08
> lesson 07) readable.

---

## Hands-on

`examples/01_struct_basics.cpp` (aggregate init), and try designated init:

```cpp
struct Trade { std::string sym = "?"; double px = 0; long qty = 0; char side = '?'; };
Trade t{ .sym = "AAPL", .px = 192.34, .qty = 100, .side = 'B' };
```

```bash
./build.ps1 11-STRUCTS/examples/01_struct_basics.cpp
```

---

## ⚠️ Traps

### Trap 1 — `Order o;` (bare) for a struct with scalars
```cpp
struct P { int x, y; };  P p;  use(p.x);   // ⚠️ garbage. P p{};
```

### Trap 2 — wrong member order in positional init
```cpp
Order o{192.34, 1001, ...};   // ⚠️ silently: id=192 (narrowing!), price=1001. Use designated
```

### Trap 3 — designated init out of order
```cpp
Rect{ .w = 100, .x = 10 };    // ❌ C++20: must be declaration order
```

### Trap 4 — mixing designated + positional
```cpp
Rect{ 10, .h = 50 };          // ❌ error
```

### Trap 5 — DMI on a member of a wire struct that must have exact layout
DMI is fine layout-wise (doesn't add storage), but don't rely on it for
default-zeroing a message you'll `memcpy` from a socket — you overwrite it anyway.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`Order o;` zeroes members" | Garbage (trivial members, local). `Order o{};` |
| "Positional init order doesn't matter" | It does — declaration order, silently wrong otherwise |
| "Designated init can be any order (like C)" | C++20: declaration order only |
| "DMI adds storage to the struct" | No — it's just a default value |
| "`{...}` allows narrowing" | No — `{}` rejects narrowing (a feature) |

---

## Exercises

1. **Init forms:** `struct S { int a; double b; std::string c; };` — `S{}`,
   `S{1}`, `S{1, 2.0}`, `S{1, 2.0, "x"}`. Print each. What are the unset members?

2. **DMI:** add `= 10`, `= 3.14`, `= "def"` defaults to `S`. Now `S x;` — valid?
   Values?

3. **Designated:** `struct Opt { bool a=false, b=false, c=false; int n=0; };` —
   `Opt{ .b = true, .n = 5 }`. Print all four.

4. **Order bug:** `struct T { int id; double price; };  T t{192.5, 1001};` —
   compile with `-Wall`. Warning? What's `t.id`? Fix with designated init.

5. **Nested:** `struct Seg { Point p1, p2; };  Seg s{ .p1 = {0,0}, .p2 = {1,1} };`
   — length of `s`.

6. **Options struct:** convert a function with 5 default args (folder 08 lesson
   07 exercise) to take one designated-init options struct.

---

## Interview questions

1. Aggregate initialization — order, partial init behaviour?
2. `S s;` vs `S s{};` for a struct with `int` members?
3. Default member initializer — kya karta hai, storage add karta hai?
4. Designated initializers (C++20) — rules (order, mixing)?
5. `{}` init narrowing pe kya karta hai?
6. Options struct vs many default arguments — trade-off?

---

## Next
→ [`03-nested-structs.md`](03-nested-structs.md)
