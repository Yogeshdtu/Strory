# 10 — `friend` functions & classes

## Prerequisites
- [`03-access-specifiers.md`](03-access-specifiers.md), [`09-operator-overloading.md`](09-operator-overloading.md)

## Yeh topic abhi kyun
`friend` ek class ko bolne ka tareeka hai: "yeh function / yeh doosri class mere
`private` members dekh sakti hai." Zaroori kab: symmetric binary operators
(`operator==`, `operator<<`) jinhe free function hona chahiye par private data
chahiye. Overuse = encapsulation kamzor. Balance samajhna.

---

## `friend` free function

```cpp
class Money {
    std::int64_t cents_;
public:
    explicit Money(std::int64_t c) : cents_(c) {}

    // yeh free functions Money ke private cents_ ko access kar sakte hain:
    friend Money operator+(Money a, Money b);
    friend std::ostream& operator<<(std::ostream& os, const Money& m);
    friend bool operator==(const Money& a, const Money& b);
};

Money operator+(Money a, Money b) { return Money{a.cents_ + b.cents_}; }   // cents_ visible

std::ostream& operator<<(std::ostream& os, const Money& m) {
    return os << (m.cents_ / 100) << '.' << (m.cents_ % 100);              // cents_ visible
}

bool operator==(const Money& a, const Money& b) { return a.cents_ == b.cents_; }
```

- `friend` **declaration class ke andar** hoti hai (kahin bhi — `public`/`private`
  section se fark nahi padta, `friend`-ness access-block-independent hai).
- Function khud **class ka member nahi** — usme `this` nahi, `Money::` prefix
  nahi. Bas usse private access mil jaata hai.
- Yeh **class deti hai** friendship — koi bahar se "main friend hoon" declare
  nahi kar sakta. Encapsulation class ke control mein rehti hai.

---

## `friend` kab genuinely chahiye

### 1. Symmetric binary operators as free functions

`operator+` free hona chahiye (symmetry — file 09), par usse `cents_` chahiye →
`friend`. (Alternative: public accessor `cents()` — tab `friend` nahi chahiye.)

### 2. `operator<<` — always free, often needs internals

```cpp
friend std::ostream& operator<<(std::ostream&, const Matrix&);   // rows/cols/data private
```

### 3. Do closely-coupled classes

```cpp
class Iterator;

class Container {
    int data_[100];
    friend class Iterator;          // Iterator ko Container ke internals chahiye
};

class Iterator {
    Container* c_;
    std::size_t pos_;
public:
    int& operator*() { return c_->data_[pos_]; }   // ✅ friend -> data_ access
};
```

### 4. Factory / builder jo private ctor use kare

```cpp
class Widget {
    Widget(int, std::string);              // private ctor
    friend class WidgetFactory;            // sirf factory bana sakti
};
```

---

## `friend` class

```cpp
class Engine {
    int rpm_;
    friend class Dashboard;      // Dashboard poori Engine ke private members dekh sakti
};

class Dashboard {
public:
    void show(const Engine& e) { std::cout << e.rpm_; }   // ✅
};
```

- **Friendship symmetric nahi** — `Dashboard` `Engine` ka friend hai, iska ulta
  nahi (jab tak `Engine` bhi `friend class Dashboard`... wait, uska ulta:
  `Engine` ko `Dashboard`'s privates chahiye to `Dashboard` mein `friend class
  Engine`).
- **Friendship inherited nahi hoti** — `Dashboard` ka derived class `Engine` ka
  friend nahi.
- **Friendship transitive nahi** — `A` friend of `B`, `B` friend of `C` ≠ `A`
  friend of `C`.

---

## `friend` vs alternatives

| Chahiye | `friend` | Alternative (aksar behtar) |
|---|---|---|
| `operator<<` ko internals | `friend` | public accessors, ya inline `friend` definition |
| `operator==` symmetric | `friend` | `= default` (C++20) — no friend needed |
| Do coupled classes | `friend class` | ek combined class, ya public "internal" API, ya nested class |
| Testing private state | `friend class Test` | testable design (test via public behaviour); ya pImpl |

**C++20 ne bahut `friend` ki zaroorat khatam ki:** `operator==` / `operator<=>`
`= default` se bante hain (private members khud compiler dekhta) — koi `friend`
nahi.

---

## Inline `friend` definition (hidden friend idiom)

```cpp
class Money {
    std::int64_t cents_;
public:
    // definition RIGHT HERE -- yeh function sirf ADL se milta (namespace mein "chhupa")
    friend Money operator+(Money a, Money b) {
        return Money{a.cents_ + b.cents_};
    }
    friend std::ostream& operator<<(std::ostream& os, const Money& m) {
        return os << m.cents_;
    }
};
```

**"Hidden friend"** — function class ke andar define, sirf **ADL** (argument-
dependent lookup) se findable. Fayda: namespace pollute nahi karta, overload
resolution faster (kam candidates), aur accidental conversions kam. Modern
guideline: symmetric operators ko hidden friends banao.

---

## Andar kya hota hai

- `friend` **zero runtime cost** — bas ek compile-time access grant. Generated
  code bilkul waisa jaise function `public` members use kar raha ho.
- `friend` function ka symbol namespace scope mein hota (member nahi) — mangled
  naam member function jaisa nahi. Hidden friend → sirf ADL, par symbol phir bhi
  emit hota (agar odr-used).
- `friend` declaration class ki **completeness / layout / `sizeof`** pe koi asar
  nahi.
- Zyada `friend`s → coupling badhta (jo classes ek doosre ke internals jaante
  hain), refactoring mushkil — par yeh design cost hai, runtime nahi.

> **HFT relevance:** `friend` yahan mostly hidden-friend operators ke liye —
> `Price`, `Qty`, `Timestamp` strong types ke `operator+`, `operator<=>`,
> `operator<<` (logging) jinhe internals chahiye but free/symmetric hona chahiye.
> Zero-cost (inline, ADL). `friend class` coupled internal components ke liye
> (ek `OrderBook` aur uska `LevelIterator`) — par aksar nested class ya ek
> unified design cleaner. C++20 `= default` comparisons ne routine `friend`
> boilerplate hata di. Testing ke liye `friend` avoid — behaviour test karo,
> internals nahi.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/06_operator_overload.cpp
```

`Money` ke operators public accessor (`cents()`) use karte hain — `friend` ke
bina. Try: `cents()` hatao aur operators ko `friend` bana ke `cents_` seedha
access karwao. Phir hidden-friend style mein convert karo.

---

## ⚠️ Traps

### Trap 1 — `friend` overuse (encapsulation kamzor)
```cpp
class C { friend class A; friend class B; friend void f(); friend void g(); };
// ⚠️ ab C ki "private" 4 jagah se accessible -- barely encapsulated
```

### Trap 2 — friendship transitive/inherited samajhna
```cpp
// A friend of B, C : public A  -> C is NOT friend of B
```

### Trap 3 — `friend` for what a public accessor would do
```cpp
friend std::ostream& operator<<(...);   // agar public getter kaafi hai -> friend zaroorat nahi
```

### Trap 4 — C++20 mein `friend` `operator==` likhna
```cpp
friend bool operator==(const T&, const T&);   // ⚠️ `bool operator==(const T&) const = default;` -- no friend
```

### Trap 5 — `friend` declaration ko access specifier se link karna
```cpp
class C { private: friend void f(); };   // 'private:' ka `friend` pe koi asar nahi -- f ko full access
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`friend` runtime cost hai" | Zero — compile-time access grant |
| "`friend` function class ka member hai" | Nahi — no `this`, no `C::` prefix; bas private access |
| "Friendship mutual/inherited/transitive" | None of those — har direction explicitly grant karo |
| "`friend` `private:` section mein rakhna zaroori" | Access-block-independent — kahin bhi |
| "Symmetric operators ke liye `friend` hi rasta" | C++20 `= default`; ya public accessors; ya hidden friend |

---

## Exercises

1. **friend operator==:** `class Vec2 { double x_, y_; public: Vec2(double,
   double); };` — `operator==` do tareeke: (a) `friend` free function, (b) C++20
   `= default` member. Dono likho.

   <details><summary>Answer</summary>

   (a) `friend bool operator==(const Vec2& a, const Vec2& b) { return a.x_ ==
   b.x_ && a.y_ == b.y_; }`. (b) `bool operator==(const Vec2&) const = default;`
   — compiler khud private members compare karta, no friend.
   </details>

2. **Hidden friend:** upar wale `Vec2` ke liye `operator+` ko hidden-friend
   style mein (class body ke andar define) likho. Yeh normal free function se
   kaise alag milta hai?

   <details><summary>Answer</summary>

   `friend Vec2 operator+(Vec2 a, Vec2 b) { return {a.x_ + b.x_, a.y_ + b.y_};
   }` — inside the class. Sirf ADL se findable (`Vec2` argument se lookup),
   namespace scope mein "visible" nahi — kam overload candidates, no pollution.
   </details>

3. **friend class:** `class LinkedList` aur `class ListNode` — `LinkedList` ko
   `ListNode` ke `next_`/`prev_` chahiye. `friend` kis class mein declare karo?

   <details><summary>Answer</summary>

   `ListNode` ke andar `friend class LinkedList;` — `ListNode` grants access to
   `LinkedList`. (Alternative: `ListNode` ek `LinkedList` ki nested private
   class ho.)
   </details>

4. **Not transitive:** `class Safe { int k_; friend class Trusted; };  class
   SubTrusted : public Trusted {};` — `SubTrusted` `Safe::k_` access kar sakta?

   <details><summary>Answer</summary>

   Nahi — friendship inherited nahi hoti. `SubTrusted` ko explicitly `friend
   class SubTrusted;` chahiye `Safe` mein.
   </details>

5. **Remove friend:** ek class jisme `friend std::ostream& operator<<(...)` sirf
   isliye hai ki ek `id_` aur `name_` print karne hain. `friend` hataao ek
   minimal public API add karke.

   <details><summary>Answer</summary>

   `public: int id() const { return id_; } const std::string& name() const {
   return name_; }` — ab `operator<<` in accessors se kaam chala le, `friend`
   nahi chahiye. (Trade-off: 2 getters public ho gaye — par woh anyway
   read-only, invariant-safe.)
   </details>

---

## Interview questions

1. `friend` kya karta? Runtime cost?
2. `friend` function member function se kaise alag (`this`, prefix)?
3. Friendship — mutual? inherited? transitive? (teenon)
4. `friend` kab genuinely chahiye (2-3 cases)?
5. Hidden friend idiom — kya aur kyun?
6. C++20 ne `friend` ki kaunsi common zaroorat khatam ki?

---

## Next
→ [`11-explicit-constructors.md`](11-explicit-constructors.md)
