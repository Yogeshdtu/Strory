# 06 — Abstract classes & interfaces

## Prerequisites
- [`03-virtual-functions.md`](03-virtual-functions.md), [`05-override-and-final.md`](05-override-and-final.md)

## Yeh topic abhi kyun
Kabhi ek base class ka **koi concrete meaning nahi** hota — `Shape` ka `area()`
kya ho? `Logger` ka `write()`? Yeh sirf ek **contract** define karti hai jo
derived classes fulfill karte hain. `= 0` (pure virtual) isse express karta hai,
aur class ko **abstract** (non-instantiable) bana deta. C++ ka "interface" yehi
hai.

---

## Pure virtual function — `= 0`

```cpp
struct Shape {
    virtual double area() const = 0;         // pure virtual -- NO implementation, MUST override
    virtual double perimeter() const = 0;
    virtual ~Shape() = default;              // virtual dtor -- file 07
};

// Shape s;                                   // ❌ ERROR -- cannot instantiate abstract class
```

- `= 0` → "is class ke paas iska implementation nahi; derived ko dena hi hoga".
- Ek bhi pure virtual → class **abstract** → `Shape s;`, `new Shape`,
  `std::vector<Shape>` — sab compile error.
- `Shape*` / `Shape&` — **allowed** (aur zaroori — polymorphism ke liye).

```cpp
struct Circle : Shape {
    double r_;
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }        // fulfill contract
    double perimeter() const override { return 2 * 3.14159265 * r_; }
};

Circle c{2.0};                               // ✅ concrete -- all pure virtuals overridden
Shape& s = c;                                // ✅ use through the interface
```

Agar `Circle` **saare** pure virtuals override na kare → `Circle` bhi abstract
(aur `Circle c;` error).

---

## Interface (pure abstract class)

```cpp
struct ISerializable {
    virtual ~ISerializable() = default;
    virtual std::vector<std::byte> serialize() const = 0;
    virtual void deserialize(std::span<const std::byte>) = 0;
};

struct IClock {
    virtual ~IClock() = default;
    virtual std::int64_t nowNs() const = 0;
};
```

**Interface** = ek class jisme:
- **Sirf pure virtual functions** (koi data, koi implementation).
- Ek virtual destructor.
- No state, no non-virtual behaviour.

Yeh "yeh type kya kar sakta hai" ka contract hai. C++ mein `interface` keyword
nahi — yeh convention hai (naming `I...`, ya just "pure abstract class").
Multiple interfaces inherit karna safe hai (file 09 — no diamond of state).

```cpp
class TcpClock : public IClock, public ISerializable {
    // implements both contracts
};
```

---

## Pure virtual WITH an implementation

Ajeeb par legal: ek pure virtual function ka body bhi ho sakta hai:

```cpp
struct Base {
    virtual void cleanup() = 0;              // pure -- derived MUST override
};
void Base::cleanup() {                        // ...par ek default implementation bhi hai
    std::cout << "Base::cleanup (common part)\n";
}

struct Derived : Base {
    void cleanup() override {
        Base::cleanup();                      // explicitly call the "default"
        std::cout << "Derived::cleanup (extra)\n";
    }
};
```

Use: derived ko override **force** karo, par ek shared default bhi provide karo
jise woh explicitly call kar sake. Rare, par pure virtual **destructor** ke liye
zaroori:

```cpp
struct AbstractBase {
    virtual ~AbstractBase() = 0;              // pure virtual dtor -> class abstract, no other pure virtual needed
};
AbstractBase::~AbstractBase() = default;       // ...par body DENA padta hai (dtor hamesha call hota)
```

("Mujhe abstract banao par mere paas koi aur natural pure virtual nahi" — pure
virtual dtor ka trick.)

---

## Abstract class ke members

```cpp
struct Processor {
    std::string name_;                        // ✅ abstract class can have data
    int retries_ = 3;

    explicit Processor(std::string n) : name_(std::move(n)) {}   // ✅ and constructors
                                                                 //    (derived se call hote)
    virtual void process(const Data&) = 0;    // pure virtual
    void logStart() const {                    // ✅ non-virtual concrete methods bhi
        std::cout << name_ << " starting\n";
    }
    virtual ~Processor() = default;
};
```

- Abstract class ka constructor derived class ke ctor se call hota hai (`:
  Processor("json")`).
- Data + concrete helpers rakhne se woh ek **abstract base** ban jaati hai
  (interface + shared implementation), pure interface nahi.

---

## Andar kya hota hai

- Abstract class ki bhi ek **vtable** hoti hai. Pure virtual slots mein aksar
  `__cxa_pure_virtual` (ek function jo abort karta) ka pointer hota — agar galti
  se ek pure virtual call ho jaaye (e.g. ctor/dtor ke dauraan — file 02) →
  `pure virtual method called` abort.
- `Shape s;` compile-time reject hota (abstract) — koi runtime cost nahi, bas
  ek check.
- Interface (pure abstract, no data) — object mein sirf vptr (8 bytes) jab
  concrete derived banti. Multiple interfaces → multiple vptrs (file 09).
- Calling through an interface pointer = normal virtual dispatch (file 04) —
  2 loads + indirect call, no inline.
- **`std::function` vs interface:** ek single-method interface ko aksar
  `std::function` / a callable se replace kiya ja sakta (kam ceremony, par
  `std::function` ka apna overhead — file 12 / folder 22).

> **HFT relevance:** interfaces (pure abstract classes) HFT mein mostly **cold
> path** — pluggable venue adapters, config providers, strategy loaders,
> serializers — jahan har call latency-critical nahi. **Hot path** dispatch
> interfaces se nahi hota: ek closed set of concrete types + CRTP / `std::variant`
> / function table (files 12, 13). Ek common pattern: interface **at the
> boundary** (venue adapter), phir andar concrete + templated hot loop. Abstract
> base with shared data + one hot virtual → measure; agar hot, devirtualize
> (`final` + LTO) ya redesign.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/02_virtual_functions.cpp
```

`Shape` abstract (`area()`/`name()` pure virtual), `Circle`/`Rect`/`Square`
concrete. Try: `Shape s;` uncomment → "cannot declare variable 's' to be of
abstract type". `Rect` se `area()` override hatao → `Rect` bhi abstract.

---

## ⚠️ Traps

### Trap 1 — abstract class instantiate karne ki koshish
```cpp
std::vector<Shape> shapes;          // ❌ Shape abstract. std::vector<std::unique_ptr<Shape>>
Shape s;                            // ❌
```

### Trap 2 — derived ne saare pure virtuals override nahi kiye
```cpp
struct Partial : Shape { double area() const override { ... } };   // perimeter() abstract -> Partial abstract
Partial p;                          // ❌
```

### Trap 3 — abstract class mein non-virtual dtor
```cpp
struct I { virtual void f() = 0; ~I() {} };   // ⚠️ delete via I* -> ~Derived skip (file 07). virtual ~I()
```

### Trap 4 — pure virtual call in ctor/dtor
```cpp
Base() { process(data); }   // ⚠️ pure virtual, ctor mein -> "pure virtual method called" abort (file 02)
```

### Trap 5 — interface with state
```cpp
struct IHandler { int count_; virtual void handle() = 0; };   // ⚠️ "interface" ab state carry karti -> diamond risk (file 10)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Abstract class mein data / ctor nahi ho sakta" | Ho sakta — data, ctors, non-virtual methods sab |
| "Pure virtual ka body nahi ho sakta" | Ho sakta (opt-in default); pure virtual dtor ko body chahiye hi |
| "Abstract class ka pointer bhi nahi bana sakte" | `T*` / `T&` allowed (zaroori) — sirf `T obj` nahi |
| "Derived ne ek pure virtual chhoda to bas warning" | Derived bhi abstract → uske objects bhi error |
| "Interface = zero cost abstraction" | Virtual dispatch cost (file 12) — hot path pe measure/replace |

---

## Exercises

1. **Abstract check:** `struct A { virtual void f() = 0; virtual void g() = 0;
   };  struct B : A { void f() override {} };  struct C : B { void g() override
   {} };` — `A a;`, `B b;`, `C c;` — kaunse compile?

   <details><summary>Answer</summary>

   `A a;` ❌, `B b;` ❌ (`g` still pure), `C c;` ✅ (both `f` and `g` now
   provided). `A*`/`B*` pointers are fine.
   </details>

2. **Interface design:** ek `IMarketDataFeed` interface likho — methods:
   `subscribe(SymbolId)`, `unsubscribe(SymbolId)`, `poll() -> std::optional<Tick>`.
   Virtual dtor. Koi data?

   <details><summary>Answer</summary>

   `struct IMarketDataFeed { virtual ~IMarketDataFeed() = default; virtual void
   subscribe(SymbolId) = 0; virtual void unsubscribe(SymbolId) = 0; virtual
   std::optional<Tick> poll() = 0; };` — no data (pure interface). Concrete
   `UdpFeed`, `ReplayFeed` implement it.
   </details>

3. **Pure virtual with default:** `struct Validator { virtual bool validate(const
   Order& o) = 0; };  void Validator::validate ...` — implement a default that
   checks `o.qty > 0`, and a `PriceValidator` that calls the default then also
   checks price.

   <details><summary>Answer</summary>

   `bool Validator::validate(const Order& o) { return o.qty > 0; }` (out of
   line). `struct PriceValidator : Validator { bool validate(const Order& o)
   override { return Validator::validate(o) && o.price > 0; } };`
   </details>

4. **Pure virtual dtor:** ek `struct Resource` jo abstract honi chahiye par
   uske paas koi natural pure virtual method nahi. Kaise?

   <details><summary>Answer</summary>

   `struct Resource { virtual ~Resource() = 0; };  Resource::~Resource() =
   default;` — pure virtual dtor makes it abstract; the body is required because
   derived destructors always call `~Resource()`.
   </details>

5. **Interface vs std::function:** ek `ILogSink { virtual void write(std::string_view)
   = 0; }` single-method interface. Isse `std::function<void(std::string_view)>`
   se replace karo. Trade-offs?

   <details><summary>Answer</summary>

   `std::function<void(std::string_view)> sink;` — less boilerplate, any callable
   (lambda, function, functor). Trade-offs: `std::function` may heap-allocate
   (large captures), has its own indirect-call overhead, and you lose named
   type / multiple related methods. For one method, often fine; for a real
   contract with several methods + lifetime, keep the interface.
   </details>

---

## Interview questions

1. Pure virtual (`= 0`) kya karta? Abstract class kya?
2. Abstract class ke objects / pointers / references — kya allowed?
3. Interface (pure abstract class) — kya rules, C++ mein keyword?
4. Pure virtual function ka body — allowed? Pure virtual dtor kyun body chahiye?
5. Abstract class mein data/ctor ho sakta? Ctor kaun call karta?
6. Derived ne ek pure virtual override nahi kiya — kya hota?

---

## Next
→ [`07-virtual-destructors.md`](07-virtual-destructors.md)
