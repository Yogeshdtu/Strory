# 02 — Struct initialization

## Prerequisites
- [`01-what-is-a-struct.md`](01-what-is-a-struct.md)
- `03-VARIABLES-DATA-TYPES/10-initialization-forms.md`

## Yeh topic abhi kyun
Struct ko bharne ke ki tareeke hain — aggregate init, default member initializers,
designated initializers (C++20). Sahi choose karne se "garbage member" bugs
khatam aur code readable.

---

## Aggregate initialization — `{}` mein members order se

```cpp
struct Order { std::uint64_t id; std::string symbol; double price; std::int64_t qty; };

Order a{1001, "AAPL", 192.34, 100};       // members DECLARATION ORDER mein
Order b = {1001, "AAPL", 192.34, 100};    // = likhna optional
Order c{};                                 // SAARE members value-initialized (0 / "" / wagairah)
Order d{1001, "AAPL"};                     // price, qty -> value-init (0.0, 0)
```

- Members **declaration order** mein bharte hain — position matter karti hai.
- Kam values di → baaki **value-initialized** (scalars ke liye 0, `std::string` ke liye `""`, andar ke
  structs ke liye recursively `{}`).
- `Order c{}` → sab zero. `Order c;` → trivial members mein **garbage**.
- `{}` **narrowing pakadta hai**: `Order{1, "x", 1, 3.5}` → error (3.5 ko int64 `qty` mein nahi daal sakte).

"Aggregate" = aisa struct/array jisme koi user-declared constructor nahi, koi private/protected
non-static data nahi, koi virtual function nahi, aur koi virtual/private/protected base class nahi.
(C++17 se **public** base class wala struct bhi aggregate hai — GCC 16.2 pe `std::is_aggregate` se
check kiya.) Plain data structs is mein aate hain.

---

## Default member initializers (DMI)

```cpp
struct Config {
    int    retries    = 3;                 // default value type mein hi likh di
    double timeout    = 5.0;
    bool   verbose    = false;
    std::string name  = "default";
};

Config a;                                  // {3, 5.0, false, "default"}  -- garbage NAHI
Config b{10};                              // {10, 5.0, false, "default"}
Config c{10, 1.0, true, "prod"};           // sab badal diye
```

Member pe `= value` → woh value use hoti hai jab tak initializer use badal na de. Isse `Config a;`
safe ban jaata hai (garbage nahi). Config / options structs mein bahut common.

⚠️ DMI + aggregate init saath mein chalte hain (C++14+), aur jin members pe DMI nahi, woh `{}` use
karne pe value-init hi hote hain.

---

## Designated initializers (C++20)

```cpp
struct Rect { int x = 0, y = 0, w = 0, h = 0; };

Rect r{ .x = 10, .w = 100, .h = 50 };      // y 0 hi raha (uska DMI / value-init)
```

- Jo members set karne hain unka **naam likho** → padhne mein saaf, defaults chhod sakte ho.
- ⚠️ **Declaration order mein hi likhne padte hain** (C++20, C ke ulat): `.x` se pehle `.w` → error.
- ⚠️ Designated aur positional mila nahi sakte: `Rect{10, .h = 50}` → error.
- Sirf aggregates ke liye.

Bahut saare optional-jaise fields wale structs ke liye badhiya (test data, config, API calls).

---

## Nested / array members

```cpp
struct Line { Point a; Point b; };
Line l{ {0, 0}, {3, 4} };                  // andar braces
Line m{ .a = {0, 0}, .b = {3, 4} };        // designated + nested

struct Board { int cells[9]; };
Board bd{ {1, 2, 3, 4, 5, 6, 7, 8, 9} };
Board z{};                                  // cells sab 0
```

---

## `Order c;` vs `Order c{};` vs `Order c{...}`

| Form | Trivial members (`int`, `double`) | Non-trivial (`std::string`) |
|---|---|---|
| `Order c;` | **garbage** (local ho to) | default-constructed (`""`) — par scalars phir bhi garbage |
| `Order c{};` | **zero** | default-constructed |
| `Order c{1, "x"};` | di hui values; baaki value-init | di hui / default |

**Rule: hamesha `{}` ya `{...}`.** Jis struct ke scalar members padhne hain uske liye akela
`Order c;` kabhi nahi.

---

## Andar kya hota hai

- Aggregate init → compiler har member ko baari-baari assign karta hai (adhoore `{}` ke liye `memset`
  + assign); koi constructor call nahi (hai hi nahi).
- DMI → jahan override nahi hua, wahan compiler default value ka initialization daal deta hai.
- Trivial struct pe `Order c{}` → aksar ek `memset(0)`. `std::string` wale struct pe → scalars zero +
  string ke liye `std::string()` call.
- Designated init → positional jaisa hi, bas naam se likha; runtime pe zero farq.

> **HFT relevance:** Config / parameter structs DMI use karte hain taaki seedha declaration valid aur
> apne aap samjhane wala ho (bhoola hua field = garbage nahi). Wire/message structs aggregates hote hain
> jinka layout explicit hota hai (file 08) aur aam taur pe zero kiye jaate hain (`msg{}`) ya poore assign.
> Designated initializers test fixtures aur "options structs" (8 default function arguments ka modern
> alternative — folder 08 lesson 07) ko padhne layak banate hain. Aur bid/ask jaise **same type ke
> fields** ke liye designated init khaas zaroori hai — neeche Trap 2.

---

## Hands-on

`examples/01_struct_basics.cpp` (aggregate init), aur designated init try karo:

```cpp
struct Trade { std::string sym = "?"; double px = 0; long qty = 0; char side = '?'; };
Trade t{ .sym = "AAPL", .px = 192.34, .qty = 100, .side = 'B' };
```

```bash
./build.ps1 11-STRUCTS/examples/01_struct_basics.cpp
```

---

## ⚠️ Traps

### Trap 1 — scalars wale struct ke liye akela `Order o;`
```cpp
struct P { int x, y; };  P p;  use(p.x);   // ⚠️ garbage. P p{};
```

### Trap 2 — positional init mein galat member order
```cpp
struct Quote { double bid; double ask; };
Quote q{101.5, 101.0};        // ⚠️ bid/ask ulta -- dono double hain, isliye CHUPCHAAP compile, quote crossed
Quote q{ .bid = 101.0, .ask = 101.5 };   // ✅ designated -- naam se, galti dikh jaati hai
```
GCC 16.2 pe chala ke: `bid=101.5 ask=101` — koi warning nahi, aur book "crossed" (bid > ask). Agar
galat order mein **type** alag hota (jaise `double` ko `int` field mein), to `{}` ka narrowing check
compile error de deta — asli khatra same type ke fields ka hai.

### Trap 3 — designated init order se bahar
```cpp
Rect{ .w = 100, .x = 10 };    // ❌ C++20: declaration order hi chahiye
```

### Trap 4 — designated + positional milana
```cpp
Rect{ 10, .h = 50 };          // ❌ error
```

### Trap 5 — exact layout wale wire struct ke member pe DMI
DMI layout mein koi jagah nahi jodta, par socket se `memcpy` karke bharne wale message ki default
zeroing ke liye uspe bharosa mat karo — aap use overwrite kar hi doge.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`Order o;` members zero karta hai" | Garbage (trivial members, local). `Order o{};` |
| "Positional init mein order se fark nahi padta" | Padta hai — same type ke fields pe chupchaap galat |
| "Designated init kisi bhi order mein (C jaisa)" | C++20: sirf declaration order |
| "DMI struct mein storage jodta hai" | Nahi — bas default value hai |
| "`{...}` narrowing allow karta hai" | Nahi — `{}` narrowing reject karta hai (feature) |
| "Base class wala struct aggregate nahi ho sakta" | C++17 se public base ke saath ho sakta hai |

---

## Exercises

1. **Init forms:** `struct S { int a; double b; std::string c; };` — `S{}`, `S{1}`, `S{1, 2.0}`,
   `S{1, 2.0, "x"}`. Har ek print karo. Jo set nahi kiye unki value?

2. **DMI:** `S` mein `= 10`, `= 3.14`, `= "def"` defaults jodo. Ab `S x;` — valid? Values?

3. **Designated:** `struct Opt { bool a=false, b=false, c=false; int n=0; };` —
   `Opt{ .b = true, .n = 5 }`. Chaaron print karo.

4. **Order bug:** `struct T { int id; double price; };  T t{192.5, 1001};` — `-Wall` se compile karo.
   Warning aayi ya kuch aur? Phir `struct Quote { double bid, ask; }; Quote q{101.5, 101.0};` — ab?
   <details><summary>Answer</summary>

   `T t{192.5, 1001}` **compile hi nahi hota** — GCC 16.2: `error: narrowing conversion of '1.925e+2'
   from 'double' to 'int' [-Wnarrowing]`. `{}` ne bachaya. `Quote q{101.5, 101.0}` chupchaap compile
   hota hai aur `bid > ask` (crossed) — dono `double` hain, compiler ke paas pakadne ka koi raasta nahi.
   Fix: `Quote q{ .bid = 101.0, .ask = 101.5 }`. (Dono chala ke check kiye.)
   </details>

5. **Nested:** `struct Seg { Point p1, p2; };  Seg s{ .p1 = {0,0}, .p2 = {1,1} };` — `s` ki length.

6. **Options struct:** 5 default arguments wale function (folder 08 lesson 07 ki exercise) ko ek
   designated-init options struct lene wala banao.

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
