# 05 — Member initializer lists

## Prerequisites
- [`04-constructors.md`](04-constructors.md)
- Folder 13 (references — `const` / reference members ki init requirement)

## Yeh topic abhi kyun
Constructor ke naam ke baad `: a_(x), b_(y)` wali list — yeh sirf style nahi.
Iska use vs constructor body mein `a_ = x;` likhne mein **do concrete farak**
hain: (1) performance (ek kaam vs do), (2) kuch members bina iske init hi nahi
hote (`const`, references). Aur ek **order trap** jo silent bugs deta hai.

---

## Init list vs body assignment

```cpp
class Widget {
    std::string name_;
    int         id_;
public:
    // ✅ member initializer list
    Widget(std::string n, int i) : name_(std::move(n)), id_(i) {}

    // ❌ body assignment
    Widget(std::string n, int i) {
        name_ = std::move(n);       // do kaam: pehle default-construct, phir assign
        id_ = i;
    }
};
```

**Kya hota hai body-assignment version mein:**
1. Body chalne se **pehle** har member construct ho chuka hota hai. `name_` ka
   **default constructor** chala (empty string bani).
2. Phir body mein `name_ = std::move(n)` — ek aur operation (move assignment).

To `std::string` ke liye: **1 default-construct + 1 move-assign** vs init list
ka **1 move-construct**. Chhote `int` ke liye fark negligible, par
`std::string` / `std::vector` / bade class members ke liye measurable.

[`examples/03_initializer_list.cpp`](examples/03_initializer_list.cpp) ek
`Tracer` type se yeh print karke dikhata hai: assignment version "ctor default"
+ "ASSIGN" dono karta hai, init-list version sirf "copy".

---

## Kuch members bina init list init HI nahi hote

```cpp
class Config {
    const int   maxRetries_;        // const -- ek baar set, phir kabhi nahi
    int&        counterRef_;        // reference -- bind karna zaroori (folder 13)
    Logger      logger_;            // no default constructor
public:
    Config(int retries, int& counter, LogSink& sink)
        : maxRetries_(retries),     // ✅ const -- sirf init list se
          counterRef_(counter),     // ✅ reference -- sirf init list se bind
          logger_(sink)             // ✅ no default ctor -- must init here
    {
        // maxRetries_ = retries;   // ❌ ERROR -- const ko assign nahi
        // counterRef_ = counter;   // ⚠️ yeh rebind NAHI -- referent ko likhta (folder 13)
    }
};
```

Init list ke bina:
- **`const` member** — body mein assign nahi ho sakta → compile error (agar DMI
  bhi nahi).
- **Reference member** — body mein "bind" nahi ho sakta → compile error.
- **Member without a default constructor** — body chalne se pehle use
  default-construct karna hoga → compile error.

DMI (`const int maxRetries_ = 3;`) in mein se kuch cases handle karta hai, par
jab value constructor param se aaye — init list hi rasta.

---

## ⚠️ THE ORDER TRAP — members declaration order mein init hote hain

```cpp
class BadOrder {
    int a_;                        // declared FIRST
    int b_;                        // declared SECOND
public:
    BadOrder(int x) : b_(x), a_(b_ + 1) {}   // ⚠️ likha: b_ pehle. Par...
};
```

**Members hamesha CLASS mein DECLARE hone ke order mein init hote hain** — init
list mein aap jo order likhte ho woh **ignore** hota hai (compiler `-Wreorder`
warn karta hai).

To upar:
1. `a_` init hota hai **pehle** (declared first) — `a_(b_ + 1)` — par `b_` abhi
   **garbage** hai! → `a_` galat value.
2. Phir `b_` init hota hai — `b_(x)` — ab sahi.

Result: `b_ == x` (sahi), `a_ == garbage + 1` (galat). Aur yeh **silent** hai
(bas ek warning) — output run-to-run alag ho sakta.

### Fix

```cpp
class GoodOrder {
    int a_;
    int b_;
public:
    GoodOrder(int x) : a_(x + 1), b_(x) {}    // ✅ a_ ko b_ pe depend mat karao
};
```

Rules:
- Init list ka order **declaration order se match** rakho (warning se bachne ke
  liye + reader ke liye).
- Ek member ko init karte waqt **doosre member pe depend mat karo** (jab tak
  pakka na ho ki woh pehle declared hai).
- Best: independent expressions use karo (`x + 1`, `x`), ya param se derive karo.

---

## Init list mein kya likh sakte ho

```cpp
class C {
    int         a_;
    std::string s_;
    std::vector<int> v_;
    Base        base_;             // (folder 16 -- base class bhi init list mein)
public:
    C(int n)
        : a_(n * 2)                        // expression
        , s_(n, 'x')                       // constructor with args
        , v_{1, 2, 3}                      // brace init
        , base_(n)                         // base class ctor
    {}
};
```

- Member ke naam ke baad `(...)` ya `{...}` — us member ke constructor ke args.
- Base classes bhi yahan (folder 16), aur woh members se **pehle** init hote
  (base pehle — folder 16 file 02).
- DMI + init list: agar dono hain, **init list jeetta** hai (DMI fallback hai).

---

## Andar kya hota hai

- **Init list = actual initialization.** Compiler har member (declaration order
  mein) ke liye uska constructor call emit karta — direct, ek baar.
- **Body assignment = init + reassign.** Member pehle apne default ctor se banta
  (body se pehle), phir body mein assignment operator chalta. `std::string` ke
  liye: default ctor (SSO buffer setup) + move/copy assign. Do calls jahan ek
  kaafi tha.
- **Trivial members** (`int`, `double`, POD): init-list mein `a_(n)` → ek `mov`.
  Body mein `a_ = n;` → **same `mov`** (compiler optimize kar deta, kyunki
  garbage → value ka intermediate observable nahi). To POD ke liye fark
  **practically zero** — matlab init-list ka asli fayda non-trivial members +
  const/ref members hai.
- **`-Wreorder`:** compiler init-list ko re-sort karke declaration order mein
  laata hai aur warn karta — generated code declaration order follow karta,
  aapka likha order nahi.

> **HFT relevance:** hot-path classes ke members aksar POD (init-list vs body
> zero fark), par jinke members `std::string`/`std::vector`/nested objects hain
> (config, session setup) — init list se ek redundant construct+assign bachta.
> Bada win: **`const` members** (`const int capacity_`, `const SymbolId sym_`) se
> invariant compile-time lock hota aur compiler ko value fix pata → better
> optimization. Order trap ki wajah se hot structs mein members ko
> **cross-dependent** init karne se bachte — har member ek independent
> expression se.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/03_initializer_list.cpp
```

`Tracer` ke prints se dekho: assignment version 2 kaam karta, init-list 1.
`NeedsInitList` — const/ref members sirf init list se. `BadOrder(5).print()` —
`a_` garbage (jaan-boojh kar `-Wreorder` + `-Wuninitialized`).

---

## ⚠️ Traps

### Trap 1 — `const` / reference member body mein set
```cpp
C(int x) { limit_ = x; }   // ❌ agar limit_ const/ref -> compile error. init list use karo
```

### Trap 2 — order trap
```cpp
C(int x) : b_(x), a_(b_) {}   // ⚠️ a_ declared first -> a_ garbage. -Wreorder
```

### Trap 3 — body assignment for expensive members
```cpp
C(std::string s) { name_ = s; }   // ⚠️ default ctor + copy assign. : name_(std::move(s)) behtar
```

### Trap 4 — DMI aur init list dono, confusion
```cpp
class C { int x_ = 10; C() : x_(20) {} };   // x_ == 20 (init list DMI ko override karta) -- theek, par jaan lo
```

### Trap 5 — init list mein member ka function call jo abhi-un-init member use kare
```cpp
C() : a_(1), b_(makeB()) {}   // makeB() agar a_ ya kisi baad-wale member ko padhe -> risky
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Init list aur body assignment same" | Init list = 1 construct; body = default-construct + assign |
| "Init list ka order execution order decide karta" | Nahi — **declaration order** decide karta |
| "POD members ke liye init list zaroori" | Fark negligible; zaroori sirf const/ref/no-default-ctor members |
| "DMI hone pe init list bekaar" | Init list DMI ko override karta jab constructor value deta |
| "`-Wreorder` bas cosmetic hai" | Order trap ka signal — silent garbage-init ho sakta |

---

## Exercises

1. **Predict:** `class C { int a_; int b_; public: C(int x) : b_(x*10), a_(b_+1)
   {} void p() const { std::cout << a_ << " " << b_; } };  C(5).p();` — output?
   Warning?

   <details><summary>Answer</summary>

   `-Wreorder`. `a_` init hota pehle (declared first): `a_(b_+1)` par `b_`
   garbage → `a_` = garbage+1. Phir `b_ = 50`. Output: `<garbage> 50`.
   </details>

2. **Count operations:** `struct Log { Log(){c("def");} Log(const Log&){c("cp");}
   Log& operator=(const Log&){c("as"); return *this;} };` — `class X { Log
   l_; public: X(const Log& src) { l_ = src; } };  Log s; X x{s};` — kaunse
   prints? Init-list version se compare.

   <details><summary>Answer</summary>

   Body version: `l_` default-constructs (`def`) before body, then `l_ = src`
   (`as`). Total: `def`, `as`. Init-list `X(const Log& src) : l_(src) {}` →
   `cp` only.
   </details>

3. **const member:** `class Buf { const std::size_t cap_; char* data_; public:
   Buf(std::size_t n); };` — ctor likho jo `cap_` set kare aur `data_` allocate.
   `cap_` ko body mein set karne ki koshish → kya hota?

   <details><summary>Answer</summary>

   `Buf(std::size_t n) : cap_(n), data_(new char[n]) {}`. Body mein `cap_ = n;`
   → "assignment of read-only member 'Buf::cap_'" compile error.
   </details>

4. **Reference member:** `class Widget { Renderer& r_; public: Widget(Renderer&
   r); };` — init list se bind karo. Body mein `r_ = r;` likho — kya woh
   "rebind" karta hai?

   <details><summary>Answer</summary>

   `Widget(Renderer& r) : r_(r) {}`. Body mein `r_ = r;` reference ko rebind
   NAHI karta — woh `r_` jis object ko point karti hai us par assignment karta
   (folder 13). Aur agar `r_` bind hi nahi hua (init list nahi) → compile error.
   </details>

5. **Fix the order:** `class Range { int hi_; int lo_; int mid_; public:
   Range(int lo, int hi) : lo_(lo), hi_(hi), mid_((lo_ + hi_) / 2) {} };` —
   `mid_` sahi banega? Members reorder karo taaki safe ho, ya expression badlo.

   <details><summary>Answer</summary>

   Declaration order: `hi_`, `lo_`, `mid_`. So `hi_` first, `lo_` second, `mid_`
   third. `mid_((lo_+hi_)/2)` — dono already init → **actually safe** here! But
   init-list order (`lo_, hi_, mid_`) ≠ decl order → `-Wreorder`. Fix: reorder
   init list to `hi_(hi), lo_(lo), mid_(...)`, or better use params:
   `mid_((lo + hi) / 2)`.
   </details>

---

## Interview questions

1. Member init list vs constructor body assignment — 2 concrete farak?
2. Kaunse members bina init list init nahi ho sakte (3)?
3. Members kis order mein init hote hain? Init list ka likha order?
4. `-Wreorder` warning kya signal karti, kaunsa bug?
5. POD members ke liye init list ka fayda kitna?
6. DMI + init list dono hon to kya jeetta?

---

## Next
→ [`06-destructors.md`](06-destructors.md)
