# 01 — Inheritance basics

## Prerequisites
- Folder 15 poora (classes, access, ctors, dtors, layout)
- Folder 11 (struct layout), folder 12/13 (pointers, references)

## Yeh topic abhi kyun
Inheritance = ek class doosri class ki **members reuse** kare aur usse
**specialize** kare. Yeh polymorphism (file 03), interfaces (file 06), aur bahut
saare design patterns ka base hai. Par yeh C++ ka sabse **misuse-hone-wala**
feature bhi hai — is folder mein hum yeh bhi seekhenge ki kab **nahi** karna
(composition — file 14, CRTP — file 13).

---

## Base aur Derived

```cpp
class Vehicle {                       // BASE class
protected:
    int speed_ = 0;
public:
    void accelerate(int by) { speed_ += by; }
    int  speed() const { return speed_; }
};

class Car : public Vehicle {          // DERIVED class -- "Car IS-A Vehicle"
    int doors_;
public:
    explicit Car(int doors) : doors_(doors) {}
    int doors() const { return doors_; }
    void honk() const { /* speed_ accessible (protected) */ }
};

Car c{4};
c.accelerate(30);        // ✅ Vehicle se inherited
c.honk();                // ✅ Car ka apna
std::cout << c.speed();  // 30 -- Vehicle::speed()
```

- `class Derived : public Base` — `Derived` ko `Base` ke **`public` + `protected`
  members** milte hain (`private` nahi — woh `Base` ke apne methods tak).
- `Base` subobject `Derived` ke andar **embedded** hota hai (offset 0, jab tak
  virtual/multiple inheritance na ho).
- "IS-A" relationship: `Car` ek tarah ka `Vehicle` hai.

---

## Inheritance ke 3 access modes

```cpp
class D1 : public    Base { };    // Base ke public -> D1 mein public,    protected -> protected
class D2 : protected Base { };    // Base ke public -> D2 mein protected, protected -> protected
class D3 : private   Base { };    // Base ke public -> D3 mein private,   protected -> private
```

| Mode | `Base::pub` in Derived | `Base::prot` | "IS-A" outside? | Common use |
|---|---|---|---|---|
| `public` | public | protected | **haan** — `Derived*` → `Base*` | normal inheritance |
| `protected` | protected | protected | sirf further-derived ko | rare |
| `private` | private | private | nahi | "implemented in terms of" (composition better) |

**99% cases `public` inheritance.** `private`/`protected` inheritance ka matlab
"Derived Base ki implementation reuse kar raha hai par Base *nahi hai*" — iske
liye **composition** (member banao) almost always cleaner (file 14).

```cpp
// ❌ private inheritance for reuse
class Stack : private std::vector<int> { public: using std::vector<int>::push_back; ... };

// ✅ composition
class Stack { std::vector<int> data_; public: void push(int x) { data_.push_back(x); } ... };
```

`struct` default inheritance `public`, `class` default `private` (folder 11 file
12) — isiliye `struct D : Base` = public, `class D : Base` = private. **Hamesha
`public` explicitly likho.**

---

## Upcasting — Derived → Base (implicit, safe)

```cpp
Car c{4};

Vehicle& vr = c;             // ✅ implicit -- Car IS-A Vehicle
Vehicle* vp = &c;            // ✅ implicit
vp->accelerate(10);          // Vehicle interface

void service(const Vehicle& v);
service(c);                  // ✅ Car -> const Vehicle&
```

- **Upcast** (Derived → Base) hamesha safe, implicit — har `Car` ek `Vehicle`
  hai.
- `vp` sirf `Vehicle` part dekh sakta hai (`vp->honk()` ❌ — `Vehicle` mein
  `honk` nahi).
- **Downcast** (Base → Derived) — implicit nahi, `static_cast` (unchecked) ya
  `dynamic_cast` (checked, needs virtual — file 11).

⚠️ **Upcast by VALUE = slicing** (`Vehicle v = c;` — Car part kat jaata, file 08).

---

## Name hiding

```cpp
struct Base {
    void f(int)    { }
    void f(double) { }
};
struct Derived : Base {
    void f(const char*) { }        // ⚠️ Base::f(int) aur f(double) ko HIDE kar deta
};

Derived d;
d.f("hi");        // ✅
d.f(42);          // ❌ ERROR -- Base::f(int) hidden! (d.f only sees f(const char*))
d.Base::f(42);    // ✅ explicit
```

Agar `Derived` mein ek naam (`f`) declare hai, to `Base` ke **saare** `f`
overloads hide ho jaate hain (return type / params se fark nahi). Wapas laane ke
liye:

```cpp
struct Derived : Base {
    using Base::f;                  // Base ke saare f overloads wapas scope mein
    void f(const char*) { }
};
```

Yeh `virtual` override se alag hai (file 05) — yeh plain name hiding hai.

---

## Andar kya hota hai

- **Layout:** `Derived` object = `[ Base subobject | Derived's own members ]`.
  Non-virtual, single inheritance → `Base` at offset 0, `&derived ==
  (Base*)&derived` (same address). `sizeof(Derived) >= sizeof(Base)`.
- **Upcast** (`Vehicle* vp = &car`) → **zero cost**, aksar literally same pointer
  value (offset 0). Multiple inheritance mein non-primary base → pointer
  **adjust** hota hai (offset add — file 09).
- **Inherited non-virtual methods** — `car.accelerate(10)` compile hota hai as
  `Vehicle::accelerate(&car.<Base subobject>, 10)` — normal call, `-O2` pe inline.
- Access mode (`public`/`private`) — **compile-time only**, layout/codegen pe
  koi asar nahi.
- `using Base::f` — sirf name lookup ko affect karta, koi runtime cost nahi.

> **HFT relevance:** plain (non-virtual) `public` inheritance zero-cost hai —
> ek `Base` subobject at offset 0, inherited methods inline. Use for genuine
> "IS-A" + shared non-virtual behaviour (e.g. a `TimestampedMessage` base with
> `seq_`/`ts_` + accessors, derived concrete message types). Par: **virtual**
> aate hi cost aata hai (vptr, indirect calls — files 03, 04, 12), aur deep
> hierarchies cache/maintenance ke dushman hain. HFT codebases aksar flat
> hierarchies + composition + CRTP/variant for dispatch prefer karte. `private`
> inheritance "reuse" ke liye — composition se replace.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/01_inheritance.cpp
```

`Vehicle` → `Car` — construction order (Base first), inherited `accelerate()`,
`protected speed_` access from `Car::honk()`, upcast to `Vehicle&`, layout
(`&c == (Vehicle*)&c` — offset 0).

---

## ⚠️ Traps

### Trap 1 — `class D : Base` (bhoolna `public`)
```cpp
class Car : Vehicle { };   // ⚠️ PRIVATE inheritance (class default). Car -> Vehicle* nahi hoga
class Car : public Vehicle { };   // ✅
```

### Trap 2 — private base member access
```cpp
class Base { int secret_; };
class Derived : public Base { void f() { secret_; } };   // ❌ private -- Derived se bhi nahi
```

### Trap 3 — name hiding
```cpp
struct D : Base { void f(char); };  D d;  d.f(10);   // ❌ Base::f(int) hidden. `using Base::f;`
```

### Trap 4 — upcast by value (slice)
```cpp
Vehicle v = car;   // ⚠️ Car part gaya (file 08). Vehicle& / Vehicle* use karo
```

### Trap 5 — `private` inheritance for code reuse
```cpp
class Timer : private std::chrono::steady_clock { };   // ⚠️ composition (member) cleaner
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Derived ko Base ke private bhi milte" | Sirf `public` + `protected` |
| "`class D : Base` public inheritance hai" | `private` (class default) — likho `public` |
| "Upcast expensive/copy" | Zero-cost (single, non-virtual — same pointer); by-value = slice |
| "Derived mein same-name method = override" | Plain name hiding — override needs `virtual` (file 05) |
| "Inheritance = reuse ka default tool" | "IS-A" ke liye; reuse ke liye composition (file 14) |

---

## Exercises

1. **Access modes:** `class B { public: int p; protected: int q; private: int r;
   };` — `class D : public B` mein `p`, `q`, `r` ka access? `main` mein `D d;` —
   `d.p`, `d.q`?

   <details><summary>Answer</summary>

   In `D`: `p` public, `q` protected, `r` inaccessible. `main`: `d.p` ✅, `d.q` ❌
   (protected).
   </details>

2. **Layout:** `struct Base { int a; }; struct Derived : Base { int b; };` —
   `sizeof(Base)`, `sizeof(Derived)`, `offsetof(Derived, b)`? `(Base*)&d == &d`?

   <details><summary>Answer</summary>

   `sizeof(Base)==4`, `sizeof(Derived)==8`, `offsetof(Derived,b)==4`. Yes —
   `(Base*)&d == (void*)&d` (Base subobject at offset 0).
   </details>

3. **Name hiding fix:** `struct Base { void log(int); void log(const char*); };
   struct D : Base { void log(double); };  D d;  d.log(5);` — error? Fix with
   `using`.

   <details><summary>Answer</summary>

   `d.log(5)` — `5` (int) → `D::log(double)` chosen (Base overloads hidden), OR
   ambiguous/narrowing. Add `using Base::log;` in `D` → all 3 overloads visible,
   `d.log(5)` → `Base::log(int)`.
   </details>

4. **private vs composition:** `class Logger` reuse karke ek `class Service` jo
   `logger.write(...)` chahta hai — private inheritance vs `Logger logger_`
   member. Dono likho, kaunsa better aur kyun?

   <details><summary>Answer</summary>

   `class Service { Logger logger_; public: void doWork() { logger_.write("..."); } };`
   — composition. Better: no accidental "Service IS-A Logger", Logger ka
   interface leak nahi, swap-able. Private inheritance sirf tab jab `protected`
   members ya virtual overriding chahiye.
   </details>

5. **Upcast + downcast:** `struct Animal {}; struct Dog : Animal {};  Dog d;
   Animal* a = &d;  Dog* back = ???;` — safe downcast kaise (no virtual)? Agar
   `a` actually ek `Cat` ko point karta to?

   <details><summary>Answer</summary>

   `Dog* back = static_cast<Dog*>(a);` — unchecked; safe only if `a` genuinely
   points to a `Dog`. If it's a `Cat` → UB. `dynamic_cast` (checked, returns
   `nullptr` on mismatch) needs a `virtual` in `Animal` (file 11).
   </details>

---

## Interview questions

1. `public` / `protected` / `private` inheritance — kya milta, "IS-A" kis mein?
2. Derived object ka layout? Upcast ki cost?
3. Name hiding kya hai, `virtual` override se kaise alag, `using` fix?
4. `class D : Base` vs `class D : public Base` — default?
5. `private` inheritance vs composition — kab kaunsa?
6. Upcast by value ka kya problem (slicing preview)?

---

## Next
→ [`02-constructors-destructors-order.md`](02-constructors-destructors-order.md)
