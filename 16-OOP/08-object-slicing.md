# 08 — Object slicing

## Prerequisites
- [`01-inheritance-basics.md`](01-inheritance-basics.md), [`03-virtual-functions.md`](03-virtual-functions.md)
- Folder 15 file 13 (class layout), folder 18 preview (copy)

## Yeh topic abhi kyun
"Slicing" = ek `Derived` object ko ek `Base` (**by value**) mein daalna → sirf
`Base` subobject copy hota hai, `Derived` ka part **kat** jaata, aur polymorphism
tut jaata hai (vptr `Base` ki set ho jaati). Yeh silent hai (koi warning aksar
nahi), aur `std::vector<Base>` jaisi common galtiyon mein hota hai.

---

## The slice

```cpp
struct Employee {
    std::string name_;
    virtual double pay() const { return 30000; }
    virtual ~Employee() = default;
};
struct Manager : Employee {
    double bonus_ = 20000;
    double pay() const override { return 80000 + bonus_; }
};

Manager m;
Employee e = m;              // ⚠️ SLICE
                            //  - sirf Employee subobject copy hua (name_)
                            //  - bonus_ chhut gaya
                            //  - e.vptr = Employee's vtable  (Manager-ness gayab)

std::cout << e.pay();       // 30000  -- Employee::pay, NOT Manager::pay
```

`Employee e = m;` — `e` ek **poora Employee object** hai (not a Manager). Copy
constructor `Employee(const Employee&)` chala, jo `m` ke `Employee` part se `e`
banaya. `Manager`-specific data aur behaviour gaya.

---

## Slicing kahan-kahan hota hai

### 1. Assignment / copy-init

```cpp
Employee e = manager;              // ⚠️
Employee e2(manager);              // ⚠️
e = manager;                       // ⚠️ copy assignment -- Employee::operator= slices
```

### 2. Pass by value

```cpp
void processEmployee(Employee e);   // ⚠️ by value
processEmployee(manager);           // Manager -> Employee copy -> slice inside the function
```

### 3. Return by value (base type)

```cpp
Employee getEmployee() {
    Manager m;
    return m;                       // ⚠️ Manager -> Employee return -> slice
}
```

### 4. Container of base (by value) — the big one

```cpp
std::vector<Employee> staff;
staff.push_back(Manager{});         // ⚠️ Manager sliced into an Employee element
staff.emplace_back(/* Manager args */);   // ⚠️ still constructs an Employee

for (const auto& e : staff)
    total += e.pay();               // always Employee::pay -- all Managers lost
```

`std::vector<Base>` ke elements **`sizeof(Base)`** ke hote hain — `Derived` (jo
bada hai) samaata hi nahi. Insert pe slice.

---

## Fix — base ko reference / pointer se rakho

```cpp
// pass by reference
void processEmployee(const Employee& e);   // ✅ no copy, polymorphic
processEmployee(manager);                   // Manager& -> Employee& -- pay() = Manager::pay

// container of pointers
std::vector<std::unique_ptr<Employee>> staff;
staff.push_back(std::make_unique<Manager>());   // ✅ full Manager on the heap
for (const auto& e : staff) total += e->pay();  // virtual dispatch -> correct

// or non-owning
std::vector<Employee*> view;                     // ✅ if lifetime managed elsewhere
```

**Rule: polymorphism ke liye base ko HAMESHA `Base&` / `Base*` / smart pointer
se handle karo. `Base` by value = slice.**

---

## Prevent slicing at compile time

Agar ek class ka slicing kbhi galat hi hai, copy ops disable karo:

```cpp
struct Employee {
    std::string name_;
    virtual double pay() const;
    virtual ~Employee() = default;

    Employee(const Employee&)            = delete;   // no slicing possible
    Employee& operator=(const Employee&) = delete;
    // (move ops decide karo -- folder 18)
protected:
    Employee() = default;                            // par derived construct kar sakein
};
```

Ab `Employee e = manager;` → **compile error** (deleted copy ctor). Polymorphic
base classes ko aksar **non-copyable** banaya jaata hai isi wajah se (unhe
`unique_ptr` se manage karo).

C++ Core Guidelines: **"Don't slice"** (ES.63), aur "A polymorphic class should
suppress public copy/move" (C.67) — ya to `= delete`, ya ek `virtual clone()`
method do.

---

## `clone()` — polymorphic copy jab zaroori ho

```cpp
struct Shape {
    virtual std::unique_ptr<Shape> clone() const = 0;   // "deep polymorphic copy"
    virtual double area() const = 0;
    virtual ~Shape() = default;
};
struct Circle : Shape {
    double r_;
    std::unique_ptr<Shape> clone() const override { return std::make_unique<Circle>(*this); }
    double area() const override { return 3.14159 * r_ * r_; }
};

std::unique_ptr<Shape> a = std::make_unique<Circle>();
std::unique_ptr<Shape> b = a->clone();               // full Circle copy, no slice
```

Jab aapko genuinely ek polymorphic object ki copy chahiye — `clone()` (virtual)
use karo, direct copy nahi.

---

## Andar kya hota hai

- `Employee e = manager;` → `Employee::Employee(const Employee&)` invoked with
  `manager` (auto-converted to `const Employee&` — upcast). Copy ctor sirf
  `Employee` members copy karta. `e` ka vptr ctor mein `Employee`'s vtable pe
  set hota. `e` is genuinely, fully an `Employee` — no trace of `Manager`.
- **No runtime error** — it's a valid `Employee`. That's why it's dangerous:
  silent wrong behaviour, not a crash.
- `std::vector<Base>` — element storage `sizeof(Base)` stride. `push_back(derived)`
  → copy ctor into a `Base` slot → slice. `emplace_back(args)` → constructs a
  `Base` directly (can't even make a `Derived`).
- `= delete` copy → the conversion `Manager` → `Employee` needs the copy ctor →
  compile error. Clean prevention.

> **HFT relevance:** slicing bugs are silent correctness bugs — "why is this
> Manager being paid like a junior?" translated to trading: "why did this
> IOC order behave like a plain limit?" A `std::vector<Order>` where `Order` has
> derived types (`IcebergOrder`, `PeggedOrder`) silently drops the derived
> behaviour. HFT practice: **either** no polymorphism in these types (a tag +
> one struct, or `std::variant`), **or** a non-copyable polymorphic base
> managed via `unique_ptr` in a `std::vector<std::unique_ptr<Order>>` (cold
> path). Hot path avoids both — closed set, flat storage, `variant`/tag
> dispatch. Polymorphic bases get `= delete`d copy or a `clone()`.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/05_slicing.cpp
```

`Manager` → `Employee` by value (`pay()` = 30000, sliced) vs by reference
(`pay()` = 100000, polymorphic). `std::vector<Employee>` (slices) vs
`std::vector<std::unique_ptr<Employee>>` (correct). Try: add `Employee(const
Employee&) = delete;` → the by-value paths stop compiling.

---

## ⚠️ Traps

### Trap 1 — `std::vector<Base>` with derived types
```cpp
std::vector<Animal> zoo;  zoo.push_back(Dog{});   // ⚠️ Dog sliced. vector<unique_ptr<Animal>>
```

### Trap 2 — pass polymorphic by value
```cpp
void feed(Animal a);  feed(dog);   // ⚠️ slice inside feed. const Animal&
```

### Trap 3 — `*basePtr = *otherBasePtr` (slicing assignment)
```cpp
Animal& a = dog;  Animal& b = cat;  a = b;   // ⚠️ Animal::operator= -> dog's Animal part = cat's. Frankenstein
```

### Trap 4 — returning base by value from a factory
```cpp
Shape makeShape(int kind) { if (kind) return Circle{}; return Square{}; }   // ⚠️ both sliced. return unique_ptr<Shape>
```

### Trap 5 — thinking `emplace_back` avoids slicing
```cpp
std::vector<Base> v;  v.emplace_back(derivedArgs);   // ⚠️ constructs a Base, not a Derived
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Slicing throws / crashes" | Silent — produces a valid, wrong-behaviour `Base` |
| "`emplace_back` avoids slicing" | No — it constructs the element type (`Base`) |
| "Passing `Base` by value is fine if small" | Any polymorphic by-value copy slices |
| "`std::vector<Base>` works with derived" | Elements are `sizeof(Base)` — derived can't fit |
| "Just be careful" | Prevent it: `= delete` copy on polymorphic bases, or `clone()` |

---

## Exercises

1. **Spot slices:** which lines slice?
   ```cpp
   Manager m;
   Employee a = m;
   Employee& b = m;
   Employee* c = &m;
   std::vector<Employee> v; v.push_back(m);
   void f(Employee e); f(m);
   void g(const Employee& e); g(m);
   ```

   <details><summary>Answer</summary>

   `a` slices, `v.push_back(m)` slices, `f(m)` slices (by value). `b`, `c`, `g`
   — no slice (reference / pointer).
   </details>

2. **Fix the container:** `std::vector<Shape> shapes;` where `Shape` is an
   abstract base — this doesn't even compile. Two correct designs?

   <details><summary>Answer</summary>

   `std::vector<Shape>` won't compile (abstract). Fix A:
   `std::vector<std::unique_ptr<Shape>>` (owned polymorphic). Fix B (if closed
   set): `std::vector<std::variant<Circle, Square, Triangle>>` — no polymorphism.
   </details>

3. **Assignment slice:** `struct A { int x = 1; virtual ~A() = default; };
   struct B : A { int y = 2; };  B b1, b2;  b1.y = 10;  A& r = b1;  r = b2;
   std::cout << b1.x << " " << b1.y;` — output?

   <details><summary>Answer</summary>

   `1 10` — `r = b2` invokes `A::operator=`, copying only `b2`'s `A` part into
   `b1` (x: 1→1). `b1.y` stays 10 (untouched). `b1` is now a mix — Frankenstein
   object.
   </details>

4. **Prevent it:** add `= delete` copy to `struct A` above. Which lines from
   exercise 3 now fail to compile?

   <details><summary>Answer</summary>

   `A(const A&) = delete; A& operator=(const A&) = delete;` → `r = b2` fails
   (deleted `A::operator=`). Also `B b2 = b1;` would fail (B's implicit copy
   needs A's). Forces `unique_ptr<A>` + explicit `clone()`.
   </details>

5. **clone():** ek `struct Order` polymorphic base with `IcebergOrder` derived.
   `std::vector<std::unique_ptr<Order>> book;` — ek order ka duplicate banao
   without knowing its concrete type.

   <details><summary>Answer</summary>

   Add `virtual std::unique_ptr<Order> clone() const = 0;` to `Order`;
   `IcebergOrder::clone() { return std::make_unique<IcebergOrder>(*this); }`.
   Then `auto dup = book[i]->clone();` — full concrete copy, no slice.
   </details>

---

## Interview questions

1. Object slicing kya hai — exactly kya "kat" jaata?
2. Slicing kahan-kahan hota hai (4 places)?
3. Slicing crash karta hai? Kyun yeh khatarnaak hai?
4. `std::vector<Base>` vs `std::vector<unique_ptr<Base>>` — slicing?
5. Polymorphic base ko slice-proof kaise banao (2 approaches)?
6. `clone()` pattern — kab, kyun?

---

## Next
→ [`09-multiple-inheritance.md`](09-multiple-inheritance.md)
