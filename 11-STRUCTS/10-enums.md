# 10 — `enum` vs `enum class`

## Prerequisites
- [`01-what-is-a-struct.md`](01-what-is-a-struct.md)
- `05-OPERATORS/05-bitwise-operators.md` (flags), `06-CONDITIONS/04-switch-statement.md` (`-Wswitch`)

## Yeh topic abhi kyun
Enum = named integer constants. `OrderType::Limit` >> a magic `1`. Par purana
(unscoped) `enum` type-unsafe hai: implicit `int` conversion, name clashes,
namespace pollution. **`enum class` (C++11)** yeh sab theek karta hai. Modern
code: `enum class` hamesha.

---

## Unscoped `enum` (legacy) — the problems

```cpp
enum Color  { Red, Green, Blue };        // Red=0, Green=1, Blue=2
enum Status { Active, Inactive };        // Active=0

Color c = Green;
int  x = c;                              // ⚠️ implicit -> int (silent)
if (Red == Active) { }                   // ⚠️ compiles! both 0. -Wenum-compare
int y = Blue + Active;                    // ⚠️ arithmetic on enums, meaningless
// `Red`, `Green`... leak into the enclosing scope -- name pollution
```

- Implicit conversion to `int` → silent misuse.
- Enumerators leak into the surrounding scope → clashes (`Red` from two enums).
- Comparable/arithmetic across unrelated enums.
- Underlying type is implementation-defined (unless specified).

---

## Scoped `enum class` — the fix

```cpp
enum class Side { Buy, Sell };           // Buy=0, Sell=1

Side s = Side::Buy;                       // MUST qualify: Side::Buy
// int i = s;                             // ❌ ERROR -- no implicit conversion (GOOD)
int i = static_cast<int>(s);             // explicit only
// if (s == 1) { }                        // ❌ ERROR -- can't compare to raw int
if (s == Side::Buy) { }                  // ✅ type-safe

enum class Color { Red };
enum class Fruit { Apple };
// Color::Red and Fruit::Apple -- no clash (scoped)
```

- **No implicit `int` conversion** — you must `static_cast`.
- **Scoped** — `Side::Buy`, no pollution, no clashes.
- **Not comparable** to `int` or to other enums.

**Rule: `enum class` unless you have a specific reason (bitmask-with-ADL-ops, C
interop).**

---

## Underlying type

```cpp
enum class Side : std::uint8_t { Buy = 1, Sell = 2 };   // explicit -- 1 byte
enum class Big  : std::int64_t { ... };

sizeof(Side)                              // 1  (because : std::uint8_t)
```

- Default underlying type: `int` for `enum class` (unless a value doesn't fit).
- **Specify it** (`: std::uint8_t`) when packing enums into structs — a
  `MsgType` in a wire struct should be exactly 1 byte (file 08).
- C++23: `std::to_underlying(e)` — cleaner than `static_cast<std::underlying_type_t<E>>(e)`.

---

## Values

```cpp
enum class OrderType {
    Market,          // 0
    Limit,           // 1
    Stop,            // 2
    StopLimit,       // 3
};

enum class HttpStatus {
    OK          = 200,
    NotFound    = 404,
    ServerError = 500,
};

enum class Flag : std::uint32_t {
    None     = 0,
    Hidden   = 1u << 0,     // 1
    PostOnly = 1u << 1,     // 2
    IOC      = 1u << 2,     // 4
};
```

Auto: start at 0, +1 each. Explicit values allowed, can repeat, can be
non-contiguous.

---

## `enum class` + `switch` — exhaustiveness

```cpp
std::string_view name(OrderType t) {
    switch (t) {
        case OrderType::Market:    return "Market";
        case OrderType::Limit:     return "Limit";
        case OrderType::Stop:      return "Stop";
        case OrderType::StopLimit: return "StopLimit";
    }                             // NO default
    return "?";
}
```

**No `default`** → if you add `OrderType::Iceberg` and forget a `case`,
`-Wswitch` (part of `-Wall`) **warns**. This is a key `enum class` benefit
(folder 06 file 04).

---

## Flags with `enum class`

`enum class` has no implicit `int` → bitwise operators don't work out of the box.
Define them:

```cpp
enum class Flag : std::uint32_t { None = 0, Hidden = 1, PostOnly = 2, IOC = 4 };

constexpr Flag operator|(Flag a, Flag b) {
    return static_cast<Flag>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
constexpr Flag operator&(Flag a, Flag b) {
    return static_cast<Flag>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}
constexpr bool has(Flag set, Flag f) { return (set & f) != Flag::None; }

Flag f = Flag::Hidden | Flag::IOC;
if (has(f, Flag::Hidden)) { ... }
```

(Or use a library like `magic_enum` / a `Flags<E>` wrapper. Folder 05 file 05.)

---

## `enum` → string

C++ has **no built-in** reflection for enum names:

```cpp
// manual switch (compile-time safe with -Wswitch)
std::string_view toString(OrderType t) { switch (t) { case ...: return "..."; } }

// or std::array indexed by the value (if contiguous from 0)
constexpr std::string_view names[] = { "Market", "Limit", "Stop", "StopLimit" };
std::string_view toString(OrderType t) { return names[static_cast<std::size_t>(t)]; }

// libraries: magic_enum (compile-time, no macros) -- common in modern codebases
```

---

## Andar kya hota hai

- An enum value is just an integer of the underlying type. `Side::Buy` is `1`
  stored in a `uint8_t`.
- `static_cast<int>(e)` / `static_cast<Side>(1)` — no-op (same bits), just a type
  change.
- `switch (enumValue)` on a contiguous enum → jump table (folder 06 file 05).
- Zero runtime overhead vs a bare integer. The safety is entirely compile-time.

> **HFT relevance:** `enum class` everywhere for message types, sides, order
> types, venues, states — with an **explicit small underlying type** (`:
> std::uint8_t`) so they pack tightly into wire and internal structs (file 05,
> 08). `switch` on them with no `default` → adding a new message type that a
> handler forgot is a compile-time `-Wswitch` warning (`-Werror` → build fails).
> Flag enums for order attributes with defined `operator|`/`&`. Zero runtime
> cost. Folders 06, 38.

---

## Hands-on

`examples/07_enums.cpp` — plain vs `enum class`, underlying type, `switch`
exhaustiveness, flags:

```bash
./build.ps1 11-STRUCTS/examples/07_enums.cpp
```

---

## ⚠️ Traps

### Trap 1 — unscoped enum name clash
```cpp
enum A { X }; enum B { X };   // ❌ redefinition. enum class A { X }; enum class B { X }; -- fine
```

### Trap 2 — implicit conversion (unscoped)
```cpp
enum Level { Low, High };
void setVolume(int);  setVolume(High);   // ⚠️ compiles -- passes 1. enum class stops this
```

### Trap 3 — `enum class` bitwise without operators
```cpp
enum class F { A = 1, B = 2 };  F f = F::A | F::B;   // ❌ ERROR. Define operator|
```

### Trap 4 — casting an out-of-range value
```cpp
Side s = static_cast<Side>(99);   // ⚠️ no error, but 99 isn't a valid Side -> switch falls through
```

### Trap 5 — `switch` on `enum class` with a `default`
```cpp
switch (t) { case A: ...; default: ...; }   // ⚠️ hides the -Wswitch "missing case" warning
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`enum` and `enum class` are the same" | `enum class`: scoped, no implicit int, no clashes |
| "`enum class` has overhead" | Zero — it's an integer; safety is compile-time |
| "`enum class` bitwise flags just work" | Define `operator|`/`&` (no implicit int) |
| "Underlying type is always `int`" | Default `int`; specify `: uint8_t` for packing |
| "C++ can print enum names" | Not built-in — `switch` / array / `magic_enum` |

---

## Exercises

1. **Convert:** take `enum Direction { N, S, E, W };` used with implicit int
   conversions. Change to `enum class`. Fix every resulting error — what did the
   errors catch?

2. **Underlying type:** `enum class Tiny : std::uint8_t { A, B, C };` vs `enum
   class Wide { A, B, C };` — `sizeof` each. Put each in a struct — struct size?

3. **`-Wswitch`:** a `toString(enum class)` with no `default`. Add a new
   enumerator, don't add a `case`. Compile `-Wall` — warning? Add `-Werror`.

4. **Flags:** `enum class Perm : uint8_t { None=0, Read=1, Write=2, Exec=4 };` —
   define `operator|`, `operator&`, `has()`. Build `Read | Write`, check each.

5. **Enum → string:** implement `toString` for a 4-value `enum class` three ways
   (switch, array, and describe how `magic_enum` would do it).

6. **Out-of-range cast:** `static_cast<Perm>(200)` — does `has(p, Perm::Read)`
   give a sane answer? What does a `switch` do?

---

## Interview questions

1. `enum` vs `enum class` — 3 concrete differences?
2. `enum class` ka runtime overhead?
3. Underlying type — default kya, explicit kab dena?
4. `enum class` + `switch` (no `default`) ka `-Wswitch` faayda?
5. `enum class` flags — bitwise operators kyun define karne padte hain?
6. `static_cast<E>(invalidValue)` — kya hota hai?

---

## Next
→ [`11-bitfields.md`](11-bitfields.md)
