# 03 — Virtual functions (runtime polymorphism)

## Prerequisites
- [`01-inheritance-basics.md`](01-inheritance-basics.md), [`02-constructors-destructors-order.md`](02-constructors-destructors-order.md)
- Folder 12 file 11 (function pointers — vtable ka concept)

## Yeh topic abhi kyun
Ab tak inheritance sirf **code reuse** thi. `virtual` isme asli power daalti hai:
**runtime polymorphism** — ek `Base*` / `Base&` ke through call karo, aur
compiler **runtime pe** decide kare ki actual object ke type ka kaunsa override
chalega. Yeh interfaces, plugin systems, aur bahut saare patterns ka core hai —
aur iski **cost** (file 12) HFT mein isse avoid karne ki wajah.

---

## Bina `virtual` — static dispatch

```cpp
struct Animal {
    void speak() const { std::cout << "<generic noise>\n"; }   // NOT virtual
};
struct Dog : Animal {
    void speak() const { std::cout << "Woof\n"; }              // HIDES Animal::speak
};

Dog d;
Animal& a = d;
a.speak();        // "<generic noise>"   -- STATIC type (Animal&) se decide hua
d.speak();        // "Woof"              -- static type Dog
```

`a.speak()` — `a` ka **declared type** `Animal&` hai → `Animal::speak` chalega,
chahe `a` asli mein ek `Dog` ho. Yeh **static (compile-time) dispatch** hai.

---

## `virtual` — dynamic dispatch

```cpp
struct Animal {
    virtual void speak() const { std::cout << "<generic noise>\n"; }   // virtual
    virtual ~Animal() = default;
};
struct Dog : Animal {
    void speak() const override { std::cout << "Woof\n"; }             // override
};
struct Cat : Animal {
    void speak() const override { std::cout << "Meow\n"; }
};

void makeItSpeak(const Animal& a) {
    a.speak();        // DYNAMIC dispatch -- a ke ASLI type ka speak() chalega
}

Dog d; Cat c;
makeItSpeak(d);      // "Woof"
makeItSpeak(c);      // "Meow"
```

`a.speak()` ab **runtime pe** `a` ke asli (dynamic) type ko dekh ke resolve hota
hai. `makeItSpeak` ko pata bhi nahi `Dog` / `Cat` exist karte — bas `Animal`
interface. Naya animal add karo → `makeItSpeak` badalna nahi padta. **Yeh
open/closed principle hai** (file 15).

---

## Rules

```cpp
struct Base {
    virtual void f();
    virtual void g() const;
    virtual int  h(int);
    virtual ~Base() = default;         // virtual dtor -- file 07
};

struct Derived : Base {
    void f() override;                 // ✅ override -- signature match
    void g() const override;           // ✅ const bhi match hona chahiye
    int  h(int) override;              // ✅
    // int h(long) override;           // ❌ ERROR -- signature mismatch, "override" catches it
};
```

- **`virtual` keyword sirf Base mein** likhna zaroori (Derived mein woh
  automatically virtual reh jaata) — par `override` (file 05) **hamesha likho**.
- Override ka **exact signature match** chahiye: naam, parameters, `const`,
  ref-qualifier, `noexcept` (C++17 se part of type). Return type covariant ho
  sakta (`Base*` → `Derived*`).
- **`virtual` ek baar hierarchy mein aaya to poore neeche virtual rehta.**
- Constructor `virtual` nahi ho sakta. Destructor ho sakta (aur base mein hona
  **chahiye** — file 07).

---

## Kaise kaam karta hai (short — deep dive file 04)

Har class jisme `virtual` hai:
- Ek **vtable** — us class ke virtual functions ke pointers ka array (`.rodata`
  mein, per-class).
- Har object ke andar ek hidden **vptr** — us class ki vtable ko point karta
  (constructor set karta).

```
   Dog object:  [ vptr ] --> Dog's vtable: [ &Dog::speak, &Dog::~Dog, ... ]
   Cat object:  [ vptr ] --> Cat's vtable: [ &Cat::speak, &Cat::~Cat, ... ]

   a.speak()  =>  vptr = load [a]            (object se vtable pointer)
                  fn   = load [vptr + slot]  (speak ka slot)
                  call fn(&a)                (indirect call)
```

`Animal&` ke through call — compiler ko pata nahi `Dog` hai ya `Cat`, to woh
vtable ke through jaata hai. Runtime pe object apna sahi `speak` "khud jaanta"
hai (uska vptr).

---

## Calling virtual from a non-virtual base method

```cpp
struct Shape {
    virtual double area() const = 0;
    void report() const {                       // non-virtual
        std::cout << "area = " << area() << "\n";  // virtual call -> derived ka area()
    }
};

struct Circle : Shape { double area() const override { return 3.14159 * r_ * r_; } double r_ = 2; };

Circle c;
c.report();      // "area = 12.566"  -- report() (non-virtual) ne area() (virtual) call kiya
Shape& s = c;
s.report();      // same -- report() ke andar `this->area()` dynamic dispatch
```

Template Method pattern (file 15) — non-virtual "skeleton" method jo virtual
"hook" methods call kare.

---

## Pure virtual — `= 0` (file 06)

```cpp
struct Shape {
    virtual double area() const = 0;      // pure virtual -> koi implementation nahi (must override)
};
// Shape s;                                // ❌ abstract class -- instantiate nahi ho sakti
```

`= 0` → derived ko override **karna hi padega**, aur `Shape` khud abstract ban
jaati (sirf interface). Detail file 06.

---

## Andar kya hota hai

- **Non-virtual call** — compiler ko exact function pata → direct `call` (ya
  inline). Zero indirection.
- **Virtual call** — `Base*`/`Base&` ke through → **2 dependent loads** (vptr,
  then slot) + **1 indirect `call`**. Compiler **inline nahi kar sakta** (dynamic
  type unknown at compile time) → aur us call ke aaspaas ki optimizations
  (constant propagation, vectorization) bhi ruk jaati.
- **Devirtualization** — kabhi compiler prove kar leta ki dynamic type kya hai
  (`Circle c; c.area();` — `c` local hai, exact type pata) → direct call. Par
  `Base*` from a container / factory → usually can't.
- **Cost:** measured (file 12, `examples/07_dispatch_benchmark.cpp`) — direct
  ~2 ns/call, virtual ~10-25 ns/call (indirect + no inline), aur data-dependent
  virtual calls BTB (branch target buffer) misses se tail badhaate.

> **HFT relevance:** virtual dispatch hot path pe **avoid** kiya jaata — har
> market-data tick pe ek virtual call = indirect call + lost inlining + possible
> BTB miss. Alternatives (files 12, 13): CRTP (compile-time dispatch, zero cost),
> `std::variant` + `std::visit` (closed set, jump table), function-pointer
> tables, ya `if/switch` on a type tag. `virtual` theek hai **cold paths** mein
> (config, startup, admin, error handling) jahan flexibility > nanoseconds.
> Rule: hot dispatch ka set **closed** hota hai (saare order types pata hain) →
> virtual ka open-ended-ness ki zaroorat hi nahi.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/02_virtual_functions.cpp
./build.ps1 fast 16-OOP/examples/07_dispatch_benchmark.cpp   # virtual vs direct vs CRTP measured
```

`02` — `Shape` hierarchy, `describe()` (non-virtual) calling `area()`/`name()`
(virtual), `Dog::speak` (hides) vs `Dog::speakV` (overrides). `07` — the cost.

---

## ⚠️ Traps

### Trap 1 — `virtual` bhoolna, "override" expect karna
```cpp
struct B { void f(); };  struct D : B { void f(); };  B& b = d;  b.f();   // ⚠️ B::f -- name hiding, not override
```

### Trap 2 — signature mismatch (silent hide instead of override)
```cpp
struct B { virtual void f(int); };  struct D : B { void f(long); };   // ⚠️ NEW virtual, B::f(int) still called via B&. `override` catches this
```

### Trap 3 — `const` mismatch
```cpp
struct B { virtual void f() const; };  struct D : B { void f() override; };   // ❌ non-const -> "override" error
```

### Trap 4 — virtual call in ctor/dtor (file 02)
```cpp
B() { f(); }   // ⚠️ B::f, not D::f
```

### Trap 5 — virtual in a hot loop
```cpp
for (auto* s : shapes) total += s->area();   // ⚠️ per-iteration indirect call, no inline (file 12)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Same-name method in Derived = override" | Only with `virtual` in Base + matching signature; else name hiding |
| "`virtual` sirf Derived mein likho" | Base mein zaroori; Derived mein auto-virtual (but write `override`) |
| "Virtual call thoda slow" | ~5-10x direct + lost inlining + BTB misses (file 12 measured) |
| "Ctor mein virtual call → Derived version" | Current class's version (file 02) |
| "Devirtualization hamesha ho jaati" | Sirf jab compiler exact dynamic type prove kar sake |

---

## Exercises

1. **Static vs dynamic:** `struct A { void f() { puts("A::f"); } virtual void g()
   { puts("A::g"); } };  struct B : A { void f() { puts("B::f"); } void g()
   override { puts("B::g"); } };  A* p = new B;  p->f();  p->g();` — output?

   <details><summary>Answer</summary>

   `A::f` (non-virtual → static type `A*`), `B::g` (virtual → dynamic type `B`).
   </details>

2. **override catches bug:** `struct Base { virtual void handle(const Event& e);
   };  struct Sub : Base { void handle(Event e); };` — `override` lagao Sub pe.
   Compile error kyun? Bina `override` kya hota?

   <details><summary>Answer</summary>

   `override` → error: `void handle(Event)` (by value) ≠ `void handle(const
   Event&)` → doesn't override. Without `override`: silently a NEW virtual;
   `basePtr->handle(e)` still calls `Base::handle` → subtle bug.
   </details>

3. **Template method:** `struct Report { void generate() const { header();
   body(); footer(); } virtual void body() const = 0; void header() const {
   puts("=== REPORT ==="); } void footer() const { puts("==="); } };` — ek
   `SalesReport : Report` likho jo sirf `body()` de. `generate()` kaise kaam
   karta?

   <details><summary>Answer</summary>

   `SalesReport::body()` override. `report.generate()` (non-virtual) calls
   `header()` (non-virtual), `body()` (virtual → `SalesReport::body`),
   `footer()`. Skeleton fixed, one hook customized.
   </details>

4. **Devirtualization:** `Circle c; double a = c.area();` vs `Shape* p = &c;
   double a = p->area();` — kaunsa devirtualize ho sakta, kaunsa aksar nahi?
   `-O2 -S` se check.

   <details><summary>Answer</summary>

   `c.area()` — `c` is a local of exact type `Circle` → compiler devirtualizes →
   direct/inline. `p->area()` where `p` is `Shape*` — usually not (could point
   anywhere), unless the compiler can trace `p` back to `&c` (LTO / same
   function often can).
   </details>

5. **Count the calls:** ek `std::vector<std::unique_ptr<Shape>>` of 1000 shapes,
   loop `sum += s->area()` 1000 times. Kitne indirect calls? Agar `Shape` ki
   jagah CRTP / variant hota to?

   <details><summary>Answer</summary>

   1,000,000 indirect calls (no inline). CRTP (homogeneous) → 0 indirect, fully
   inlined. `variant` → jump-table dispatch (arms may inline). Benchmark: virtual
   ~10x direct (file 12).
   </details>

---

## Interview questions

1. Static vs dynamic dispatch — example har ek, kab kaunsa?
2. `virtual` function call ke steps (vptr, slot, indirect call)?
3. Override ke liye signature ka kya-kya match hona chahiye?
4. Virtual call se compiler kaunse optimizations kho deta?
5. Devirtualization kab possible?
6. Non-virtual method se virtual call (template method) — kaam kaise karta?

---

## Next
→ [`04-vtable-deep-dive.md`](04-vtable-deep-dive.md)
