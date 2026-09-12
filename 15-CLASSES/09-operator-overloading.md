# 09 — Operator overloading

## Prerequisites
- [`02-members-and-methods.md`](02-members-and-methods.md), [`07-const-member-functions.md`](07-const-member-functions.md)
- Folder 05 (operators, precedence), folder 13 (references, `return *this`)

## Yeh topic abhi kyun
`a + b`, `a == b`, `std::cout << a`, `v[i]` — yeh operators aapki apni types ke
liye bhi define ho sakte hain. Sahi kiya jaaye to code natural padhta hai
(`Money total = a + b;`). Galat kiya jaaye to surprise-heavy. Rules, member vs
free, aur C++20 ka `<=>` (spaceship) — jo saare comparisons ek line mein deta.

---

## Rules — kya allowed, kya nahi

```cpp
Money operator+(Money a, Money b);      // ✅ apni type ke liye +
```

- **Kam se kam ek operand user-defined type** hona chahiye (`int operator+(int,
  int)` redefine nahi kar sakte).
- **Precedence / associativity / arity nahi badal sakte** — `*` hamesha `+` se
  pehle bind hoga, chahe aap kuch bhi karo.
- Yeh operators overload **NAHI** ho sakte: `.` `.*` `::` `?:` `sizeof`
  `typeid` `co_await`.
- Naye operators invent nahi kar sakte (`**`, `<>` nahi).
- `&&`, `||`, `,` overload ho sakte hain **par short-circuit / sequencing
  khatam** ho jaati — practically kabhi mat karna.

---

## Member vs free function

```cpp
class Money {
    std::int64_t cents_;
public:
    // COMPOUND ASSIGNMENT -> member (left operand hamesha Money, modify hota hai)
    Money& operator+=(Money rhs) { cents_ += rhs.cents_; return *this; }

    // UNARY -> member
    Money operator-() const { return Money{-cents_}; }

    std::int64_t cents() const { return cents_; }
};

// BINARY ARITHMETIC -> free function (symmetric conversions ke liye)
Money operator+(Money a, Money b) { a += b; return a; }   // += ke terms mein -- DRY
```

**Guideline:**
- **`@=`** (`+=`, `-=`, `*=`, ...) → **member**. Left operand hamesha aapki
  type, aur woh modify hota hai → `*this` return.
- **Binary `@`** (`+`, `-`, `==`, `<`, ...) → **free function** (aksar `@=` ke
  terms mein). Kyun free: taaki `2 + money` bhi kaam kare (agar `Money(int)`
  implicit ho) — member hota to left operand `Money` hona majboori.
- **Unary** (`-x`, `!x`, `++x`) → **member**.
- **`[]`, `()`, `->`, `=`** → **member** (majboori — language rule).
- **`<<`, `>>`** for streams → **free** (left operand `std::ostream&`, aapki type
  nahi).

---

## Stream output — `operator<<`

```cpp
std::ostream& operator<<(std::ostream& os, const Money& m) {
    std::int64_t c = m.cents();
    // ... format ...
    os << (c / 100) << '.' << (c % 100);
    return os;                          // ostream& return -> chaining: cout << a << b
}

std::cout << money << "\n";
```

- Free function (left operand `std::ostream&`).
- `const T&` right operand (no copy).
- **`std::ostream&` return** — taaki `cout << a << b << c` chain ho.
- Agar private members chahiye → `friend` (file 10) ya public accessors.

---

## Comparison — C++20 `<=>` (spaceship)

Pre-C++20: `==`, `!=`, `<`, `<=`, `>`, `>=` — 6 operators, saare likhna dard.

C++20: **ek `operator<=>`** define karo (ya `= default`), compiler baaki sab
derive kar deta:

```cpp
#include <compare>

class Version {
    int major_, minor_, patch_;
public:
    auto operator<=>(const Version&) const = default;   // <, <=, >, >= AUTO
    bool operator==(const Version&) const = default;    // ==, != AUTO
};

Version a{1, 2, 0}, b{1, 3, 0};
a < b;      // ✅ auto-derived from <=>  (lexicographic: major, minor, patch)
a == b;     // ✅ auto-derived from ==
a != b;     // ✅
```

- `= default` → **member-wise lexicographic** comparison (declaration order).
- Return type auto: `std::strong_ordering` (int-like), `std::partial_ordering`
  (float — NaN), `std::weak_ordering` (equivalent but not identical).
- Custom logic chahiye → khud likho:
  ```cpp
  std::strong_ordering operator<=>(const Money& o) const {
      return cents_ <=> o.cents_;
  }
  ```
- **`==` alag** — `<=>` default karne se `==` bhi implicitly aata hai, par jab
  `==` sasta ho sakta hai (e.g. size compare pehle) to `==` alag define karo.

---

## Common overloads reference

| Operator | Kahan | Signature (Money example) |
|---|---|---|
| `+=` `-=` `*=` | member | `Money& operator+=(Money)` |
| `+` `-` | free | `Money operator+(Money, Money)` |
| `-` (unary) | member | `Money operator-() const` |
| `==` | member/default | `bool operator==(const Money&) const = default` |
| `<=>` | member/default | `auto operator<=>(const Money&) const = default` |
| `<<` | free | `std::ostream& operator<<(std::ostream&, const Money&)` |
| `[]` | member | `T& operator[](std::size_t)` + `const T& ... const` |
| `()` | member | `R operator()(Args...)` (function objects) |
| `++`/`--` prefix | member | `Money& operator++()` |
| `++`/`--` postfix | member | `Money operator++(int)` (dummy `int` param) |
| `*` `->` (deref) | member | smart pointers, iterators |
| `=` (copy/move) | member | folder 18 |

---

## ⚠️ Don't-overload / be-careful list

- **`&&` `||` `,`** — short-circuit / sequencing tootti hai. Never.
- **`&` (address-of)** — `std::addressof` bypass karta hai; confusing. Rarely.
- **Surprising semantics** — `operator+` jo modify kare, `operator==` jo
  expensive ho aur log na kare, `operator bool` jo implicit ho aur ajeeb jagah
  fire kare (`explicit operator bool()` use karo).
- **Asymmetry** — agar `a == b` define kiya to `b == a` bhi kaam karna chahiye
  (free function + implicit conversions se milta; C++20 mein `==` reversed
  candidate automatic).

---

## Andar kya hota hai

- `a + b` (jahan `a`, `b` `Money`) → compiler `operator+(a, b)` ka call emit
  karta — ek normal function. `-O2` pe agar chhota (jaise `cents_ += rhs.cents_`)
  → **fully inline**, ek `add` instruction.
- `<=>` default → compiler member-wise comparison code generate karta
  (declaration order, short-circuit on first non-equal). `int` members ke liye
  yeh 3-4 compares + branches, aksar branch-predictable.
- `std::cout << x` overload → ek `call` to your `operator<<` (jo aksar khud kai
  `<<` calls karta) — **I/O bound**, operator overhead ignorable.
- `operator[]` on a class → agar inline aur bounds-check-free (`data_[i]`) → raw
  array index jaisa. `.at()` (bounds check) → ek extra compare + branch.
- Overloaded operators **normal functions** hain — ADL, overload resolution,
  templates sab lagta hai. Koi special "operator magic" runtime pe nahi.

> **HFT relevance:** value types jaise `Price`, `Qty`, `Timestamp` ke liye
> operator overloading se code arithmetic jaisa padhta hai (`mid = (bid + ask) /
> 2`) bina raw `int64` juggling ke — aur `-O2` pe woh raw int ops hi ban jaata
> (strong typedef pattern). `<=>` se order-book key comparisons ek line mein.
> Watch-outs: (1) `operator<<` for logging — hot path pe logging hi avoid, to
> operator ki cost moot; (2) implicit-conversion-triggering operators
> (`operator double()`) hot code mein surprise conversions → `explicit`; (3)
> expensive `operator==` (deep compare) jo tight loop mein call ho — profile.
> Strong-typedef value classes zero-cost hain jab operators inline aur type
> transparent.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/06_operator_overload.cpp
```

`Money` — `+=` member, `+`/`-`/`*` free, unary `-`, prefix/postfix `++`, `<<`
free, `<=>`/`==` default. `3 * b` symmetric overload. `Money(int)` `explicit`.

---

## ⚠️ Traps

### Trap 1 — `+` member banane se asymmetry
```cpp
Money operator+(Money rhs) const;   // ⚠️ money + 2 chalega (agar Money(int)), par 2 + money NAHI. Free banao
```

### Trap 2 — `<<` return type `void`
```cpp
void operator<<(std::ostream& os, const T& t);   // ⚠️ cout << a << b tootega. std::ostream& return karo
```

### Trap 3 — `<=>` bhool ke sirf `<` likhna
```cpp
bool operator<(const T&) const;   // pre-C++20 style -> ==, >, <=, >= sab alag likhne padenge. <=> = default
```

### Trap 4 — postfix `++` galat signature
```cpp
Money operator++();      // yeh PREFIX hai. postfix: Money operator++(int)  (dummy int)
```

### Trap 5 — `&&`/`||` overload
```cpp
bool operator&&(const T&, const T&);   // ⚠️ short-circuit gaya -> dono operands hamesha evaluate. Never
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Operator overload precedence badal sakta" | Nahi — `*` hamesha `+` se pehle |
| "`+=` aur `+` dono member" | `+=` member; `+` free (symmetry) |
| "`<<` member function" | Free — left operand `std::ostream&` |
| "`<=>` sirf ordering deta" | `= default` se `==` bhi (implicitly); custom mein `==` alag socho |
| "Operators special runtime magic hain" | Normal functions — overload resolution, inline, ADL |

---

## Exercises

1. **Money free `+`:** `class Money { std::int64_t c_; public: explicit
   Money(std::int64_t); Money& operator+=(Money); std::int64_t c() const; };` —
   free `operator+` likho `+=` ke terms mein. `a + b` aur (agar `Money(int)`
   non-explicit hota) `2 + a`?

   <details><summary>Answer</summary>

   `Money operator+(Money a, Money b) { a += b; return a; }`. `a + b` OK. `2 + a`
   → sirf tab jab `Money(int)` **non-**explicit (implicit conversion `2` →
   `Money`). Free function isliye taaki dono operands convert ho sakein.
   </details>

2. **Stream:** `class Frac { int n_, d_; };` — `operator<<` likho jo `n/d` print
   kare, chaining support kare (`cout << f1 << " " << f2`).

   <details><summary>Answer</summary>

   `std::ostream& operator<<(std::ostream& os, const Frac& f) { return os <<
   f.n_ << '/' << f.d_; }` — `friend` chahiye agar `n_/d_` private, ya
   accessors. `os` return → chaining.
   </details>

3. **Spaceship:** `struct Time { int h, m, s; };` — `<=>` aur `==` default karo.
   `Time{9,30,0} < Time{9,45,0}` kaise evaluate hota?

   <details><summary>Answer</summary>

   `auto operator<=>(const Time&) const = default;` → member-wise: `h` compare
   (9==9), then `m` (30 < 45) → `less` → `<` true. Lexicographic in declaration
   order.
   </details>

4. **Postfix/prefix:** `class Counter { int n_ = 0; public: ... };` — prefix
   `++` (`Counter&`) aur postfix `++` (`Counter`, dummy `int`) dono. `Counter c;
   auto x = c++; auto y = ++c;` — `x`, `y`, `c` ki `n_`?

   <details><summary>Answer</summary>

   `c++`: returns old (n_=0), c.n_ becomes 1. `x.n_ == 0`. `++c`: c.n_ becomes
   2, returns ref. `y` aliases/copies c → `y.n_ == 2`, `c.n_ == 2`.
   </details>

5. **Bad overload:** `bool operator==(const BigMatrix& o) const { /* element-wise
   */ }` ek tight loop mein `if (m == identity)` call hota hai (10000x/sec, 1M
   elements). Kya problem, kya karein?

   <details><summary>Answer</summary>

   Har call O(1M) — hidden cost, loop ko O(N*1M) bana deta. Fix: cheap early-out
   (dimensions, a hash/checksum, dirty flag), ya loop se `==` hatao (structural
   check kahin aur cache karo). Operator "==" ki syntactic sasti dikhawat cost
   chhupa deti.
   </details>

---

## Interview questions

1. Operator overloading ke rules (kam se kam 3 constraints)?
2. `+=` member kyun, `+` free kyun?
3. `operator<<` free function kyun, return type kya aur kyun?
4. C++20 `<=>` — `= default` kya deta, `==` ka kya?
5. Postfix vs prefix `++` — signatures?
6. `&&` / `||` overload kyun mana?

---

## Next
→ [`10-friend-functions.md`](10-friend-functions.md)
