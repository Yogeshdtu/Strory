# 10 — `enum` vs `enum class`

## Prerequisites
- [`01-what-is-a-struct.md`](01-what-is-a-struct.md)
- `05-OPERATORS/05-bitwise-operators.md` (flags), `06-CONDITIONS/04-switch-statement.md` (`-Wswitch`)

## Yeh topic abhi kyun
Enum = naam wale integer constants. `OrderType::Limit` ek magic `1` se kahin behtar. Par purana
(unscoped) `enum` type-unsafe hai: chupchaap `int` conversion, naamon ki takkar,
namespace mein kachra. **`enum class` (C++11)** yeh sab theek karta hai. Modern
code: hamesha `enum class`.

---

## Unscoped `enum` (purana) — problems

```cpp
enum Color  { Red, Green, Blue };        // Red=0, Green=1, Blue=2
enum Status { Active, Inactive };        // Active=0

Color c = Green;
int  x = c;                              // ⚠️ chupchaap -> int
if (Red == Active) { }                   // ⚠️ compile hota hai! dono 0. -Wenum-compare
int y = Blue + Active;                   // ⚠️ do alag enums ka jod -- bematlab
// `Red`, `Green`... bahar wale scope mein aa jaate hain -- naamon ka kachra
```

GCC 16.2, `-std=c++20`, **bina kisi flag ke** (chala ke dekha):
```
warning: comparison between 'enum Color' and 'enum Status' [-Wenum-compare]
warning: arithmetic between different enumeration types 'Color' and 'Status' is deprecated [-Wdeprecated-enum-enum-conversion]
```
(C++20 ne alag enums ka arithmetic deprecated kiya, C++26 standard ne ise ill-formed bana diya — GCC 16.2
`-std=c++26` pe abhi bhi warning hi deta hai. Aur `int x = c;` pe koi warning nahi.)

- `int` mein chupchaap conversion → galat use pakda nahi jaata.
- Enumerators bahar ke scope mein leak → takkar (do enums ka `Red`).
- Alag-alag enums ek doosre se compare/jod ho jaate hain.
- Underlying type implementation-defined (jab tak bataya na ho) — values badi hon to badh bhi jaata hai:
  `enum U2 { W = 0x1'0000'0000 };` ka `sizeof` GCC 16.2 pe **8**.

Ulti disha chupchaap nahi hoti: `enum Color x = 5;` → ❌ `error: invalid conversion from 'int' to 'Color'`.

---

## Scoped `enum class` — ilaaj

```cpp
enum class Side { Buy, Sell };           // Buy=0, Sell=1

Side s = Side::Buy;                       // naam ke saath likhna ZAROORI: Side::Buy
// int i = s;                             // ❌ ERROR -- chupchaap conversion nahi (ACHHA)
int i = static_cast<int>(s);             // sirf khul ke
// if (s == 1) { }                        // ❌ ERROR -- raw int se compare nahi
if (s == Side::Buy) { }                  // ✅ type-safe

enum class Color { Red };
enum class Fruit { Apple };
// Color::Red aur Fruit::Apple -- koi takkar nahi (scoped)
```

- **`int` mein chupchaap conversion nahi** — `static_cast` karna padega.
- **Scoped** — `Side::Buy`, na kachra, na takkar.
- `int` ya doosre enums se **compare nahi** hota.

Analogy: unscoped enum = building ke saare flats ka ek common letterbox — "Sharma" naam do flats mein hai to
chitthi galat jaayegi. `enum class` = har flat ka apna letterbox, pata poora likhna padta hai (`Side::Buy`).

**Niyam: `enum class`, jab tak koi khaas wajah na ho (C interop, purana code).**

---

## Underlying type

```cpp
enum class Side : std::uint8_t { Buy = 1, Sell = 2 };   // khud bataya -- 1 byte
enum class Big  : std::int64_t { ... };

sizeof(Side)                              // 1  (: std::uint8_t ki wajah se)
```

- `enum class` ka default underlying type **hamesha `int`** hai. Koi value `int` mein na aaye to error:
  `enum class E { V = 0x1'0000'0000 };` → ❌ `enumerator value '4294967296' is outside the range of underlying
  type 'int'` (unscoped enum ke ulat, jo chupchaap bada ho jaata hai).
- **Khud batao** (`: std::uint8_t`) jab enum wire struct mein pack karna ho — wire struct ka `MsgType` exactly
  1 byte ka hona chahiye (file 06, 13).
- ⚠️ Par internal struct mein `: uint8_t` akele memory nahi bachata: `struct { Tiny t; int32_t x; }` aur
  `struct { Wide t; int32_t x; }` dono **8 bytes** (GCC 16.2) — `int32_t` ki alignment padding bache hue 3 bytes
  kha jaati hai (file 05).
- C++23: `std::to_underlying(e)` — `static_cast<std::underlying_type_t<E>>(e)` se saaf.

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

Apne aap: 0 se shuru, har agla +1. Khud values de sakte ho, repeat ho sakti hain, beech mein gap ho sakta hai.

---

## `enum class` + `switch` — exhaustiveness

```cpp
std::string_view name(OrderType t) {
    switch (t) {
        case OrderType::Market:    return "Market";
        case OrderType::Limit:     return "Limit";
        case OrderType::Stop:      return "Stop";
        case OrderType::StopLimit: return "StopLimit";
    }                             // default NAHI
    return "?";
}
```

**`default` nahi** → `OrderType::Iceberg` jodo aur `case` bhool jao, to `-Wswitch` (`-Wall` ka hissa) **warn
karta hai**: `warning: enumeration value 'Iceberg' not handled in switch [-Wswitch]` (GCC 16.2). Yeh `enum class`
ka bada faayda hai (folder 06 file 04). Aakhri `return "?"` zaroori hai — invalid cast wali value (Trap 4) kisi
case se match nahi karegi.

---

## `enum class` ke saath flags

`enum class` mein chupchaap `int` nahi → bitwise operators seedhe kaam nahi karte. Khud define karo:

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

(Ya `magic_enum` jaisi library / ek `Flags<E>` wrapper. Folder 05 file 05.)

---

## `enum` → string

C++ mein enum ke naam ke liye **koi built-in** reflection nahi (C++26 ka static reflection GCC 16.2 mein nahi):

```cpp
// haath se switch (-Wswitch ke saath compile-time safe)
std::string_view toString(OrderType t) { switch (t) { case ...: return "..."; } }

// ya value se index kiya hua array (agar 0 se lagataar ho)
constexpr std::string_view names[] = { "Market", "Limit", "Stop", "StopLimit" };
std::string_view toString(OrderType t) { return names[static_cast<std::size_t>(t)]; }   // ⚠️ out-of-range value -> OOB

// libraries: magic_enum (compile-time, macros nahi) -- modern codebases mein aam
```

---

## Andar kya hota hai

- Enum value bas underlying type ka ek integer hai. `Side::Buy` = `uint8_t` mein rakha `1`.
- `static_cast<int>(e)` / `static_cast<Side>(1)` — no-op (wahi bits), sirf type badalta hai.
- Lagataar values wale enum pe `switch` → jump table (folder 06 file 05).
- Bare integer ke comparison mein zero runtime overhead. Saari safety compile-time hai.

> **HFT relevance:** Message types, sides, order types, venues, states — sab jagah `enum class`, **chhote explicit
> underlying type** (`: std::uint8_t`) ke saath taaki wire structs mein tight pack hon (file 06, 13; internal structs
> mein padding ka dhyaan — file 05). Unpe bina `default` ka `switch` → naya message type jo handler bhool gaya woh
> compile-time `-Wswitch` warning (`-Werror` → build fail). Order attributes ke liye flag enums, `operator|`/`&`
> define karke. Runtime cost zero. Aur wire se aayi byte ko `static_cast<MsgType>` karne se pehle validate karo —
> enum koi range check nahi karta (Trap 4). Folders 06, 38.

---

## Hands-on

`examples/07_enums.cpp` — plain vs `enum class`, underlying type, `switch` exhaustiveness, flags:

```bash
./build.ps1 11-STRUCTS/examples/07_enums.cpp
```

---

## ⚠️ Traps

### Trap 1 — unscoped enum ke naamon ki takkar
```cpp
enum A { X }; enum B { X };   // ❌ error: 'X' conflicts with a previous declaration. enum class A { X }; enum class B { X }; -- theek
```

### Trap 2 — chupchaap conversion (unscoped)
```cpp
enum Level { Low, High };
void setVolume(int);  setVolume(High);   // ⚠️ compile hota hai -- 1 chala gaya. enum class ise rokta hai
```

### Trap 3 — bina operators ke `enum class` bitwise
```cpp
enum class F { A = 1, B = 2 };  F f = F::A | F::B;   // ❌ ERROR. operator| define karo
```

### Trap 4 — range ke bahar ki value cast karna
```cpp
Side s = static_cast<Side>(99);   // ⚠️ koi error nahi. Fixed underlying type hai to 99 legal value hai,
                                  //    par koi enumerator nahi -> switch ka koi case match nahi
```
(GCC 16.2 pe chala ke: `name(static_cast<OrderType>(42))` ne `"?"` diya — isiliye switch ke baad wala `return` zaroori.)

### Trap 5 — `enum class` pe `switch` mein `default`
```cpp
switch (t) { case A: ...; default: ...; }   // ⚠️ -Wswitch ki "missing case" warning chhup jaati hai
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`enum` aur `enum class` ek hi hain" | `enum class`: scoped, chupchaap int nahi, takkar nahi |
| "`enum class` mein overhead hai" | Zero — integer hi hai; safety compile-time |
| "`enum class` bitwise flags bas chal jaate hain" | `operator|`/`&` define karo (chupchaap int nahi) |
| "Underlying type hamesha `int`" | `enum class`: default `int`; unscoped: implementation-defined, bada ho sakta hai |
| "`: uint8_t` lagao, struct chhota ho jaayega" | Padding kha sakti hai — `{Tiny, int32_t}` bhi 8 bytes |
| "C++ enum ke naam print kar sakta hai" | Built-in nahi — `switch` / array / `magic_enum` |
| "`static_cast<Enum>(x)` range check karta hai" | Koi check nahi — pehle validate karo |

---

## Exercises

1. **Convert:** `enum Direction { N, S, E, W };` jo implicit int conversions ke saath use ho raha hai. Ise `enum class`
   banao. Har error theek karo — errors ne kya pakda?

2. **Underlying type:** `enum class Tiny : std::uint8_t { A, B, C };` vs `enum class Wide { A, B, C };` — dono ka
   `sizeof`. Har ek ko `std::int32_t` ke saath ek struct mein daalo — struct size?
   <details><summary>Answer (GCC 16.2)</summary>

   `sizeof(Tiny)` **1**, `sizeof(Wide)` **4**. `struct { Tiny t; std::int32_t x; }` aur `struct { Wide t; std::int32_t x; }`
   dono **8** — `x` ko 4-byte alignment chahiye, to `Tiny` ke baad 3 bytes padding. Faayda tab dikhega jab kai
   `uint8_t` fields saath hon ya struct packed ho.
   </details>

3. **`-Wswitch`:** bina `default` ka `toString(enum class)`. Naya enumerator jodo, `case` mat jodo. `-Wall` se compile —
   warning? Ab `-Werror` jodo.
   <details><summary>Answer</summary>

   `warning: enumeration value 'Iceberg' not handled in switch [-Wswitch]`. `-Werror` ke saath yahi error ban ke build
   rok deta hai.
   </details>

4. **Flags:** `enum class Perm : uint8_t { None=0, Read=1, Write=2, Exec=4 };` — `operator|`, `operator&`, `has()`
   define karo. `Read | Write` banao, har ek check karo.

5. **Enum → string:** 4-value `enum class` ke liye `toString` teen tarah (switch, array, aur batao `magic_enum` kaise
   karega).

6. **Out-of-range cast:** `static_cast<Perm>(200)` — `has(p, Perm::Read)` samajhdaar jawab deta hai? `switch` kya karta hai?
   <details><summary>Answer (chala ke)</summary>

   200 = `0b1100'1000` → Read (1), Write (2), Exec (4) teeno bits 0 → `has` teeno ke liye `false`. Jawab "technically sahi"
   hai par 200 koi valid permission set nahi — `has` ise pakad nahi sakta. `switch` ka koi case match nahi karta. Wire
   se aayi value pehle validate karo.
   </details>

---

## Interview questions

1. `enum` vs `enum class` — 3 thos farq?
2. `enum class` ka runtime overhead?
3. Underlying type — default kya, khud kab dena?
4. `enum class` + `switch` (bina `default`) ka `-Wswitch` faayda?
5. `enum class` flags — bitwise operators kyun define karne padte hain?
6. `static_cast<E>(invalidValue)` — kya hota hai?

---

## Next
→ [`11-bitfields.md`](11-bitfields.md)
