# 01 — Class kya hai

## Prerequisites
- Folder 11 poora (structs, aggregate init, layout, `struct` vs `class`)
- Folder 14 (memory — object kahan rehta hai, lifetime)
- Folder 12/13 (pointers, references — `this` samajhne ke liye)

## Yeh topic abhi kyun
`struct` ne related data ko ek naam ke neeche baandha. Par ek `struct` "bewakoof"
hai — koi bhi member ko kuch bhi set kar sakta hai, chahe woh galat ho. **Class**
data ke saath **behaviour** (methods) aur **access control** (`private`) jodti
hai — taaki object hamesha ek **valid state** mein rahe. Yeh OOP ka core idea
hai, aur RAII (folder 17), STL containers, sab isi par khade hain.

---

## Class = data + behaviour + control

```cpp
class BankAccount {
    long long balanceCents_ = 0;      // DATA -- private (default)

public:
    void deposit(long long c)  { if (c > 0) balanceCents_ += c; }          // BEHAVIOUR
    bool withdraw(long long c) {                                           // + CONTROL
        if (c <= 0 || c > balanceCents_) return false;   // invariant: balance >= 0
        balanceCents_ -= c;
        return true;
    }
    long long balance() const { return balanceCents_; }
};
```

Teen cheezein ek jagah:
1. **Data** (`balanceCents_`) — kya cheez store hoti hai.
2. **Behaviour** (`deposit`, `withdraw`) — us data pe kaunse operations valid hain.
3. **Control** (`private`) — data ko seedha koi haath nahi laga sakta; sirf
   in-house methods se.

`struct` bhi yeh sab kar sakta hai (folder 11 file 12) — `class` ka matlab bas
**"default private"** + convention "ismein rules hain".

---

## Blueprint vs object (instance)

```cpp
class BankAccount { ... };          // BLUEPRINT -- ek type. Koi memory nahi.

BankAccount asha;                    // OBJECT (instance) -- asli memory, apna balance
BankAccount ravi;                    // doosra OBJECT -- alag memory, alag balance

asha.deposit(5000);
ravi.deposit(200);
// asha.balance() == 5000, ravi.balance() == 200  -- independent
```

- **Class** = design (`sizeof` batata hai object kitni jagah lega, par class khud
  jagah nahi leti).
- **Object** = us design se bani ek asli cheez, apni memory ke saath.
- Ek class se jitne chaaho objects.

Analogy: class ek "cookie cutter", objects "cookies".

---

## Encapsulation — invariant ki hifazat

**Invariant** = ek rule jo object ke liye hamesha sach hona chahiye.
`BankAccount` ka invariant: **`balanceCents_ >= 0`**.

```cpp
// struct ke saath -- invariant tootne se koi nahi rok sakta
struct RawAccount { long long balanceCents; };
RawAccount r{100};
r.balanceCents = -999999;            // 😱 invalid state -- kisi ne roka nahi

// class ke saath -- balance sirf deposit/withdraw ke rules se badalta
BankAccount a;
a.deposit(100);
// a.balanceCents_ = -999999;        // ❌ compile ERROR -- private
a.withdraw(999999);                  // returns false, balance unchanged
```

Encapsulation = **"data private, access controlled"**. Fayda:
- Galat state banana **impossible** (ya at least mushkil).
- Implementation baad mein badal sakte ho (cents → paise → decimal) bina caller
  ka code toote — interface (methods) same rehta hai.
- Debugging aasan — "balance galat kaise hua?" ka jawaab sirf 2 methods mein.

---

## Ek chhota class — poora anatomy

```cpp
class Point {
    double x_ = 0;                   // data member (private)
    double y_ = 0;

public:
    Point() = default;                              // default constructor
    Point(double x, double y) : x_(x), y_(y) {}     // parameterized constructor

    double x() const { return x_; }                 // const accessor (getter)
    double y() const { return y_; }

    void translate(double dx, double dy) {          // mutating method
        x_ += dx;
        y_ += dy;
    }

    double distanceFromOrigin() const {             // computed, const
        return std::sqrt(x_ * x_ + y_ * y_);
    }
};

Point p{3, 4};
p.translate(1, 0);
std::cout << p.x() << "," << p.y() << " -> " << p.distanceFromOrigin();
```

Yeh sab is folder ke agle lessons mein detail se: members/methods (02), access
(03), constructors (04), init lists (05), destructor (06), `const` (07), static
(08), operators (09)...

---

## Andar kya hota hai

- Class ka **object** = uske non-static data members ka blob, bilkul equivalent
  `struct` jaisa (folder 11 file 12). `private`/`public` **layout nahi badalte**
  — woh compile-time access check hain, zero runtime cost.
- **Methods object mein store nahi hote.** Method code ek normal function hai
  `.text` mein. `p.translate(1, 0)` compile hota hai as `translate(&p, 1, 0)` —
  object ka address ek hidden pehla argument (`this`) ban jaata hai (file 02).
- Ek `class BankAccount` object aur ek equivalent `struct { long long c; }` ka
  `sizeof` **same** hai.
- Koi `virtual` nahi → koi vptr nahi → koi hidden pointer nahi (virtual folder
  16 mein).

> **HFT relevance:** classes yahan invariant-heavy cheezon ke liye hain — ek
> `OrderBook` jo sorted rehna chahiye, ek `RiskEngine` jiske limits kabhi cross
> na hon, ek `Session` jiska state machine valid rahe. Encapsulation ka **runtime
> cost zero** hai (compile-time check), to hot path pe bhi `class` use hota hai
> jahan rules matter karte hain. Plain data (messages, book levels, config) abhi
> bhi `struct` (folder 11) — transparent, `memcpy`-able, `static_assert`-able.
> Class ka misuse: chhoti value type pe bhari getter/setter boilerplate jo
> `struct` se saaf hota — us case mein `struct` behtar.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/01_first_class.cpp
```

`BankAccount` — deposit/withdraw invariant enforce karte hain. Comment-out lines
(`acc.balanceCents_ = ...`) uncomment karke dekho: compile error. Phir
`08_order_class.cpp` — ek poori HFT-style `Order` class with state machine.

---

## ⚠️ Traps

### Trap 1 — `class` ko `struct` se "alag cheez" samajhna
Sirf default access ka fark (folder 11 file 12). Baaki sab identical.

### Trap 2 — sab kuch `public` karke encapsulation "skip" karna
```cpp
class Account { public: long long balance; };   // ⚠️ ab yeh bas ek struct hai, invariant nahi
```

### Trap 3 — getter/setter har member pe blind
```cpp
class Point { double x_; public: double getX() const { return x_; } void setX(double v) { x_ = v; } };
// ⚠️ agar koi invariant nahi to yeh sirf boilerplate hai -- `struct Point { double x, y; };` behtar
```

### Trap 4 — class ko object samajhna
```cpp
BankAccount.deposit(100);   // ❌ BankAccount ek TYPE hai. object chahiye: a.deposit(100)
```

### Trap 5 — "class = slow" myth
Encapsulation compile-time hai. `class` vs `struct` — zero runtime difference.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Class object banane pe methods bhi copy hote" | Methods `.text` mein — ek copy, sab objects share |
| "`private` se performance girti" | Compile-time access check — zero runtime cost |
| "Har class ko getter/setter chahiye" | Sirf jab invariant ho; warna `struct` |
| "Class aur struct fundamentally alag" | Sirf default access; folder 11 file 12 |
| "`ClassName.method()` valid hai" | Object chahiye: `obj.method()` (ya `static` — file 08) |

---

## Exercises

1. **Invariant identify karo:** in types ke invariants likho — `class
   Percentage` (0-100), `class NonEmptyString`, `class SortedList`, `class
   DateOfBirth`. Kaunsa `struct` reh sakta hai?

   <details><summary>Answer</summary>

   `Percentage`: value ∈ [0,100]. `NonEmptyString`: size ≥ 1. `SortedList`:
   elements non-decreasing. `DateOfBirth`: valid calendar date, past. Sab
   `class` (invariant hai). Ek `struct Point { double x, y; }` jaisa koi
   invariant-less type `struct` reh sakta.
   </details>

2. **Break the struct:** `struct Temperature { double celsius; };` — ek `main`
   likho jo isse invalid bana de (e.g. -500, absolute zero se neeche). Phir
   `class` version jo `>= -273.15` enforce kare.

   <details><summary>Answer</summary>

   Struct: `Temperature t; t.celsius = -500;` — koi rok nahi. Class: ctor +
   setter mein `if (c < -273.15) c = -273.15;` (ya throw). Bahar se `celsius_`
   private.
   </details>

3. **Blueprint vs object:** `class Counter { int n_ = 0; public: void tick() {
   ++n_; } int value() const { return n_; } };` — 3 alag `Counter` objects
   banao, alag-alag tick karo, values print. Independent?

   <details><summary>Answer</summary>

   Haan — har object ka apna `n_`. `c1.tick(); c1.tick(); c2.tick();` →
   `c1.value() == 2`, `c2.value() == 1`, `c3.value() == 0`.
   </details>

4. **sizeof check:** `class A { int x, y; public: void f(); void g() const; int
   h() const; };` vs `struct B { int x, y; };` — `sizeof(A)` vs `sizeof(B)`?
   Methods add karne se badla?

   <details><summary>Answer</summary>

   `sizeof(A) == sizeof(B) == 8`. Methods per-object storage nahi lete. `private`
   bhi layout nahi badalta.
   </details>

5. **Refactor:** `struct Account { long long balance; std::string owner; bool
   frozen; };` jahan code jagah-jagah `if (!acc.frozen) acc.balance -= x;` karta
   hai. Ise `class` bana ke `withdraw()` mein `frozen` + non-negative dono
   enforce karo.

   <details><summary>Answer</summary>

   `class Account { long long balance_; std::string owner_; bool frozen_ = false;
   public: bool withdraw(long long x) { if (frozen_ || x <= 0 || x > balance_)
   return false; balance_ -= x; return true; } void freeze() { frozen_ = true; }
   ... };` — ab har withdraw ek jagah se, rules guaranteed.
   </details>

---

## Interview questions

1. Class aur struct mein fark? (technical + convention)
2. Encapsulation kya hai, kya fayda (kam se kam 3)?
3. Invariant kya hai — ek example class ka?
4. Class vs object — analogy aur technical difference?
5. Methods object ke andar store hote hain? `sizeof` pe asar?
6. "Class encapsulation ka runtime cost hai" — sach ya nahi, kyun?

---

## Next
→ [`02-members-and-methods.md`](02-members-and-methods.md)
