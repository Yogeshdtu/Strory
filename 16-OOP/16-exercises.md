# 16 — Folder 16 Revision + Exercises

## Prerequisites
Lessons 01–15 aur saare 8 examples chalaye hue.

---

## PART A — Concept check

1. `public`/`protected`/`private` inheritance — kya milta, "IS-A" kis mein?
2. Derived object ka layout, upcast ki cost?
3. Name hiding vs `virtual` override — `using` fix?
4. Construction order (bases, members, body)? Destruction — kyun reverse?
5. Ctor/dtor ke andar virtual call — kya hota, kyun?
6. Static vs dynamic dispatch — example, kab kaunsa?
7. Virtual call ke steps? Kaunse optimizations kho jaate?
8. `override` kya check karta? Bina iske kaunsa bug?
9. `final` — method/class pe, 2 effects, devirtualization?
10. Pure virtual (`= 0`), abstract class — pointer/reference allowed?
11. Pure virtual dtor kyun body chahiye?
12. Non-virtual base dtor + `delete basePtr` — leak? UB? Guideline (public+virtual / protected+non-virtual)?
13. `unique_ptr<Base>` vs `shared_ptr<Base>` — virtual dtor requirement?
14. Object slicing — kya kat-ta, kahan hota (4 places), prevent kaise?
15. Multiple inheritance — safe case (interfaces) vs avoid?
16. Non-primary base pointer adjustment, `this`-adjusting thunk?
17. Diamond — bina virtual kitne base subobjects? Virtual inheritance ka fix + cost (3)?
18. Virtual base ko init kaun karta (most-derived)?
19. `dynamic_cast` pointer vs reference failure? vs `static_cast` down?
20. `typeid` — exact ya is-a? `-fno-rtti` kya hatata?
21. Virtual call cost (measured) — direct se ratio, kis se depend?
22. BTB kya, virtual predictability kaise affect karta?
23. CRTP kya, virtual se cost/container fark?
24. `static_cast<Derived*>(this)` safe kyun, guard kaise?
25. Composition vs inheritance — "prefer composition" ke 3 reasons?
26. `class Stack : public std::vector` — kya galat?
27. SOLID — 5 principles ek-ek line?
28. HFT reconciliation — SOLID ke goals rakh ke `virtual` ki jagah kya?

---

## PART B — Output prediction

### B1
```cpp
struct A { void f() { puts("A::f"); } virtual void g() { puts("A::g"); } virtual ~A() = default; };
struct B : A { void f() { puts("B::f"); } void g() override { puts("B::g"); } };
int main() { A* p = new B; p->f(); p->g(); delete p; }
```
<details><summary>Answer</summary>`A::f` (non-virtual → static type `A*`), `B::g` (virtual → dynamic type `B`).</details>

### B2
```cpp
struct X { X(){c('X');} ~X(){c('x');} };
struct A { A(){c('A');} ~A(){c('a');} };
struct D : A { X m_; D(){c('D');} ~D(){c('d');} };
int main() { D d; }
```
<details><summary>Answer</summary>`A X D d x a` — construct: base A, member X, body D. Destruct: body d, member x, base a.</details>

### B3
```cpp
struct Base {
    Base() { init(); }
    virtual void init() { puts("Base::init"); }
    virtual ~Base() = default;
};
struct Derived : Base {
    void init() override { puts("Derived::init"); }
};
int main() { Derived d; }
```
<details><summary>Answer</summary>`Base::init` — during `Base()`, the dynamic type is `Base` (Derived part not built; vptr = Base's).</details>

### B4
```cpp
struct Employee { virtual double pay() const { return 100; } virtual ~Employee() = default; };
struct Manager : Employee { double pay() const override { return 500; } };
void byVal(Employee e) { std::cout << e.pay() << " "; }
void byRef(const Employee& e) { std::cout << e.pay() << " "; }
int main() { Manager m; byVal(m); byRef(m); Employee e = m; std::cout << e.pay(); }
```
<details><summary>Answer</summary>`100 500 100` — `byVal` slices (Employee::pay), `byRef` polymorphic (Manager::pay), `Employee e = m` slices.</details>

### B5
```cpp
struct NoV { int a, b; };
struct V   { int a, b; virtual ~V() = default; };
int main() { std::cout << sizeof(NoV) << " " << sizeof(V) << " "
             << std::is_trivially_copyable_v<V>; }
```
<details><summary>Answer</summary>`8 16 0` — first virtual adds an 8-byte vptr; `virtual` → not trivially copyable.</details>

### B6
```cpp
struct A { virtual const char* who() const { return "A"; } virtual ~A() = default; };
struct B : A { const char* who() const override { return "B"; } };
int main() {
    B b; A& a = b;
    std::cout << (typeid(a) == typeid(B)) << " "
              << (typeid(a) == typeid(A)) << " "
              << (dynamic_cast<B*>(&a) != nullptr);
}
```
<details><summary>Answer</summary>`1 0 1` — `typeid` gives exact dynamic type (`B`); `dynamic_cast<B*>` succeeds (`B` is-a `A`).</details>

### B7
```cpp
struct Device { std::string s; Device(std::string x) : s(std::move(x)) {} };
struct Scanner : virtual Device { Scanner() : Device("SCN") {} };
struct Printer : virtual Device { Printer() : Device("PRN") {} };
struct Copier : Scanner, Printer { Copier() : Device("COPY") {} };
int main() { Copier c; std::cout << c.s; }
```
<details><summary>Answer</summary>`COPY` — virtual base → one shared `Device`; the most-derived class (`Copier`) initializes it; `Scanner`/`Printer`'s `Device("SCN"/"PRN")` are ignored.</details>

### B8
```cpp
template <class D> struct Base { double run() const { return static_cast<const D*>(this)->impl() * 2; } };
struct Impl : Base<Impl> { double impl() const { return 21; } };
int main() { Impl i; std::cout << i.run() << " " << sizeof(Impl); }
```
<details><summary>Answer</summary>`42 8` — CRTP static dispatch (`run` → `impl` → 21, ×2 = 42); `Base<Impl>` is an empty base → EBO → `sizeof(Impl)` == 1 double == 8.</details>

---

## PART C — Find the bug

### C1
```cpp
struct Widget {
    virtual void render();
    ~Widget() { }
};
struct Button : Widget {
    std::vector<int> pixels_;
    ~Button() { }
};
int main() { Widget* w = new Button; delete w; }
```
<details><summary>Answer</summary>`~Widget` non-virtual + polymorphic + `delete` via `Widget*` → only `~Widget` runs, `~Button` (and `pixels_` cleanup) skipped → leak + UB (`-Wdelete-non-virtual-dtor`). Fix: `virtual ~Widget() = default;`.</details>

### C2
```cpp
struct Base { virtual void handle(const Msg& m); };
struct Impl : Base { void handle(Msg m) { /* ... */ } };
```
<details><summary>Answer</summary>`Impl::handle(Msg)` (by value) does NOT override `Base::handle(const Msg&)` — different signature → it's a new virtual. `basePtr->handle(m)` calls `Base::handle`. Add `override` → compile error surfaces the bug; fix the signature.</details>

### C3
```cpp
std::vector<Shape> shapes;
shapes.push_back(Circle{2.0});
shapes.push_back(Square{3.0});
double total = 0;
for (const auto& s : shapes) total += s.area();
```
<details><summary>Answer</summary>If `Shape` is concrete: `push_back(Circle{...})` **slices** into a `Shape` — `Circle`/`Square` behaviour lost. If `Shape` is abstract, it won't compile. Fix: `std::vector<std::unique_ptr<Shape>>` or `std::vector<std::variant<Circle, Square>>`.</details>

### C4
```cpp
struct A { int x; };
struct B : A { int y; };
struct C : A { int z; };
struct D : B, C { int w; };
int main() { D d; d.x = 1; }
```
<details><summary>Answer</summary>`d.x` is ambiguous — `D` has two `A` subobjects (via `B` and via `C`). Non-virtual diamond. Fix: `struct B : virtual A`, `struct C : virtual A` → one shared `A`.</details>

### C5
```cpp
struct Rectangle {
    virtual void setW(int w) { w_ = w; }
    virtual void setH(int h) { h_ = h; }
    int area() const { return w_ * h_; }
    int w_ = 0, h_ = 0;
};
struct Square : Rectangle {
    void setW(int w) override { w_ = h_ = w; }
    void setH(int h) override { w_ = h_ = h; }
};
void f(Rectangle& r) { r.setW(3); r.setH(4); assert(r.area() == 12); }
```
<details><summary>Answer</summary>LSP violation — `f` assumes `setW`/`setH` are independent (Rectangle's contract). Passing a `Square` → `area() == 16` → assert fails. `Square` should not derive from `Rectangle`; both should derive from a `Shape` interface, or `Square` is its own type.</details>

### C6
```cpp
struct Base {
    virtual ~Base() = default;
    Base() { doSetup(); }
    virtual void doSetup() = 0;
};
struct Impl : Base { void doSetup() override { /* ... */ } };
int main() { Impl i; }
```
<details><summary>Answer</summary>Pure virtual call in a constructor. During `Base()`, the dynamic type is `Base`; `doSetup()` is pure → "pure virtual method called" → `std::terminate`/abort. Fix: don't call virtuals from ctors — use two-phase init or a factory.</details>

### C7
```cpp
class Logger {
public:
    virtual void log(std::string_view) = 0;
    virtual ~Logger() = default;
};
class Engine {
    Logger& logger_;
public:
    explicit Engine(Logger& l) : logger_(l) {}
    void tick() {
        for (int i = 0; i < 1'000'000; ++i)
            logger_.log("tick");     // hot loop
    }
};
```
<details><summary>Answer</summary>DIP applied, but the abstraction is a `virtual` interface called 1M times in a hot loop → 1M indirect calls + no inline (file 12). Fix: `template <class Logger> class Engine { Logger& logger_; ... };` (compile-time injection, `log` inlines) — same testability, zero hot-path cost. Or batch: `logger_.log(spanOfTicks)`.</details>

### C8
```cpp
template <class D>
struct Shape {
    double area() const { return static_cast<const D*>(this)->areaImpl(); }
};
struct Circle : Shape<Square> {          // <-- note
    double r_;
    double areaImpl() const { return 3.14159 * r_ * r_; }
};
int main() { Circle c{{}, 2.0}; std::cout << c.area(); }
```
<details><summary>Answer</summary>`Circle : Shape<Square>` — wrong CRTP argument. `Shape<Square>::area()` does `static_cast<const Square*>(this)` but `this` is a `Circle` → UB (and `Square::areaImpl` may not even exist / has different layout). Fix: `Circle : Shape<Circle>`. Guard: `private` ctor in `Shape<D>` + `friend D` makes `Shape<Square>` unconstructible by `Circle`.</details>

---

## PART D — Write it

### D1 — Shape hierarchy, 3 ways
Implement `area()` dispatch for `{Circle, Rect, Triangle}` via (a) `virtual` +
`std::vector<std::unique_ptr<Shape>>`, (b) `std::variant` + `std::visit`, (c)
CRTP + templated `totalArea<T>`. Same output; compare `sizeof` and (with `-O2
-S`) the generated dispatch.

### D2 — Non-copyable polymorphic base + clone
`struct Order` polymorphic base (`virtual ~`, `= delete` copy) with
`LimitOrder`, `IcebergOrder`. Add `virtual std::unique_ptr<Order> clone() const
= 0;`. A `std::vector<std::unique_ptr<Order>>` book; duplicate order `i` without
knowing its concrete type.

### D3 — Template Method
`class TradeReport` with non-virtual `generate()` calling `header()` (non-virtual),
`rows()` (pure virtual), `footer()` (virtual, default impl). `EquityReport`
overrides `rows()`. Show `report.generate()` runs the skeleton with the hook.

### D4 — Interface segregation
Design `IOrderBookView` (read: `bestBid`, `bestAsk`, `depth`, `midPrice`) and
`IOrderBookMutator` (`add`, `cancel`, `modify`). A `class BookSnapshotPrinter`
that depends only on the view. A `class MatchingEngine` that needs both.

### D5 — CRTP mixin
`template <class D> struct Serializable { std::string toJson() const { /* calls
D::fields() and formats */ } };` — a `struct Config : Serializable<Config>` that
provides `fields()` returning name/value pairs. `cfg.toJson()`. `sizeof(Config)`
unchanged by the mixin?

### D6 — Dispatch benchmark extension
Extend `07_dispatch_benchmark.cpp` with a **tag-switch** approach: `struct Shape
{ uint8_t kind; double a, b; }` + a `switch`-based `area()`. Add it to the
comparison. Also add a "sorted by kind" run of the virtual path — show the BTB
effect on `ns/call`.

---

## PART E — HFT angle

1. **Zero-virtual hot type:** `class PriceLevel` — kaunse features (virtual,
   `std::string`, user dtor, non-primary base) `static_assert(std::is_trivially_copyable
   && !is_polymorphic)` ko todenge? Ek trivially-copyable design likho.

2. **Dispatch choice:** market-data messages — `{Add, Modify, Delete, Trade,
   Snapshot}`, ~5M/sec, closed set. `virtual` / `variant` / tag-switch — kaunsa,
   kyun? Measure roughly from file 12's numbers.

3. **Boundary vs core:** ek venue adapter `IVenue` (virtual, cold) jo andar ek
   templated hot loop drive karta hai. Sketch the layering — kahan virtual,
   kahan template.

4. **Slicing in a book:** `std::vector<Order>` where `Order` has `IcebergOrder`
   derived — kya silently toot-ta? 2 fixes (one flat, one polymorphic).

5. **DIP without cost:** `MatchingEngine` needs a `Clock` and a `RiskCheck`.
   Virtual injection vs template injection — testability same? Hot-path cost
   different? Likho dono.

---

## PART F — Challenge

**"`Dispatcher` — a message router, three implementations, benchmarked"**

Ek market-data message router banao jo har message ko uske type ke handler pe
route kare. Message types (closed set): `Add`, `Modify`, `Delete`, `Trade`.

Implement **three** versions with the **same external behaviour**:

1. **`VirtualDispatcher`** — `struct Handler { virtual void onAdd(const Add&);
   virtual void onModify(...); ... };` + `std::vector<std::unique_ptr<Handler>>`
   subscribers; router iterates and calls virtuals.
2. **`VariantDispatcher`** — `using Msg = std::variant<Add, Modify, Delete,
   Trade>;` + subscribers are callables; router `std::visit`s.
3. **`TagDispatcher`** — `struct Msg { MsgType tag; ... payload ... };` +
   subscribers register a `void(*)(const Msg&)` per tag in a table; router reads
   `tag` and calls the function pointer (or a `switch`).

Requirements:
- Same test: feed 1M messages (mix of types, some runtime-decided), each handler
  accumulates a checksum; assert all three produce the **same** checksum.
- `-O2`, `rdtsc` per message: report p50 / p99 / p99.9 per dispatch approach.
- `perf stat -e branch-misses,instructions` (Linux) for each — correlate
  branch-misses with the ns gap.
- A `#ifdef SORTED` mode that groups messages by type before dispatch — show the
  virtual path speeds up (BTB), the others barely change.
- Short writeup: which you'd ship for a 5M-msg/sec feed handler and why; where
  `virtual` would still be acceptable.

Yeh folder 12 (function pointers), 14 (pools/no-alloc), is folder ka poora
dispatch spectrum, aur folder 38/39 (market data / order book) ka seed hai.

---

## Next
→ [`../17-RAII/00-README.md`](../17-RAII/00-README.md)
