# 15 — SOLID principles (practical)

## Prerequisites
- [`01-inheritance-basics.md`](01-inheritance-basics.md) … [`14-composition-vs-inheritance.md`](14-composition-vs-inheritance.md)

## Yeh topic abhi kyun
SOLID = 5 design principles jo classes ko maintainable, testable, aur
change-friendly banate hain. Yeh dogma nahi — practical guidelines hain jo aap
already partly follow kar rahe ho (encapsulation, "prefer composition"). Yahan
naam + concrete C++ examples + jab bend karna theek hai.

---

## S — Single Responsibility Principle

**"Ek class ke paas badalne ki ek hi wajah honi chahiye."**

```cpp
// ❌ 3 responsibilities: parse, validate, persist
class Order {
    void parseFromWire(std::span<const std::byte>);
    bool validate() const;
    void saveToDatabase();
    void sendToExchange();
};

// ✅ split
struct Order { /* just data + invariants */ };
class OrderParser    { Order parse(std::span<const std::byte>); };
class OrderValidator { bool validate(const Order&) const; };
class OrderStore     { void save(const Order&); };
class OrderGateway   { void send(const Order&); };
```

Fayda: ek concern badlo (wire format), sirf ek class touch. Test alag-alag.
**Bend:** chhoti utility types ko over-split mat karo — ek `Vec3` ke `length()`,
`normalize()`, `dot()` sab "vector math" ek responsibility hai.

---

## O — Open/Closed Principle

**"Extension ke liye open, modification ke liye closed."** Naya behaviour add
karo bina existing code badle.

```cpp
// ❌ har naye shape pe yeh function edit karna padta
double area(const Shape& s) {
    switch (s.kind) {
        case CIRCLE: return ...;
        case SQUARE: return ...;
        // har naya shape -> yahan case add -> ye function "closed" nahi
    }
}

// ✅ virtual -- naya shape = naya class, area() function untouched
struct Shape { virtual double area() const = 0; };
struct Hexagon : Shape { double area() const override { ... } };   // no existing code changed
```

**Bend (HFT):** open/closed via `virtual` costs dispatch (file 12). Agar set
**closed** hai (saare shapes/order-types pata hain), `std::variant` + `visit`
(compile-time exhaustive — naya type add karo, `visit` compile error deta jab tak
handle na karo) — that's "closed to modification" in a checked way, zero dispatch
cost. Tag `switch` — you DO edit the switch, but the compiler's
`-Wswitch`/exhaustiveness helps.

---

## L — Liskov Substitution Principle

**"Derived ko Base ki jagah use kiya ja sake bina program galat hue."** Derived
ko Base ka contract todna nahi chahiye.

```cpp
// ❌ classic violation
struct Rectangle {
    virtual void setWidth(int w)  { w_ = w; }
    virtual void setHeight(int h) { h_ = h; }
    int area() const { return w_ * h_; }
    int w_, h_;
};
struct Square : Rectangle {
    void setWidth(int w)  override { w_ = h_ = w; }   // ⚠️ breaks Rectangle's contract
    void setHeight(int h) override { w_ = h_ = h; }
};

void resizeAndCheck(Rectangle& r) {
    r.setWidth(4);
    r.setHeight(5);
    assert(r.area() == 20);        // ✅ for Rectangle, ❌ for Square (area == 25) -- LSP violated
}
```

`Square` "IS-A" `Rectangle` mathematically, par **behaviourally nahi** — code
jo `Rectangle` maan ke likha hai woh `Square` pe todta hai. Fix: `Square` `Rectangle`
se derive **na kare** (dono ko ek `Shape` interface se, ya `Square` ko separate
type).

LSP rules for an override:
- **Preconditions strengthen nahi kar sakte** (Base se zyada strict input na
  maango).
- **Postconditions weaken nahi kar sakte** (Base se kam guarantee na do).
- **Invariants preserve karo**, **no new exceptions** jo Base declare nahi
  karta.

---

## I — Interface Segregation Principle

**"Clients ko unpe depend na karwao jo woh use nahi karte."** Fat interfaces ko
chhote, focused mein todo.

```cpp
// ❌ fat interface -- har implementer ko sab dena padta
struct IDevice {
    virtual void read() = 0;
    virtual void write() = 0;
    virtual void seek(int) = 0;
    virtual void print() = 0;
    virtual void scan() = 0;
};
struct KeyboardDevice : IDevice {
    void read() override { ... }
    void write() override { throw NotSupported{}; }   // ⚠️ forced to stub
    void seek(int) override { throw NotSupported{}; }
    void print() override { throw NotSupported{}; }
    void scan() override { throw NotSupported{}; }
};

// ✅ segregated
struct IReadable { virtual void read() = 0; };
struct IWritable { virtual void write() = 0; };
struct ISeekable { virtual void seek(int) = 0; };

struct KeyboardDevice : IReadable { void read() override { ... } };          // only what it does
struct File : IReadable, IWritable, ISeekable { /* all three */ };
```

Multiple **interface** inheritance (file 9 — safe, no state) makes this clean.

---

## D — Dependency Inversion Principle

**"High-level modules low-level details pe depend na karein — dono abstractions
pe depend karein."**

```cpp
// ❌ Engine (high-level) directly depends on ConsoleLogger (low-level detail)
class Engine {
    ConsoleLogger logger_;                      // hard-wired
public:
    void run() { logger_.log("started"); }
};

// ✅ depend on an abstraction; inject the concrete one
struct ILogger { virtual void log(std::string_view) = 0; virtual ~ILogger() = default; };

class Engine {
    ILogger& logger_;                           // depends on abstraction
public:
    explicit Engine(ILogger& l) : logger_(l) {}
    void run() { logger_.log("started"); }
};

// tests: Engine{mockLogger};   prod: Engine{fileLogger};
```

Dependency **injection** (ctor pe pass karo) makes `Engine` testable and
swappable. **Bend (HFT):** the abstraction can be a **template parameter** (CRTP
/ concept — file 13) instead of a virtual interface — same inversion, zero
dispatch cost: `template <class Logger> class Engine { Logger& logger_; ... };`.

---

## SOLID vs performance — the HFT reconciliation

SOLID mostly guides **structure**, not runtime cost. But naive SOLID reaches for
`virtual` interfaces everywhere. HFT reconciliation:

| Principle | Naive OOP | HFT-friendly |
|---|---|---|
| Open/Closed | `virtual` hierarchy | `std::variant` + `visit` (checked-exhaustive, zero dispatch) |
| Dependency Inversion | `ILogger&` virtual | `template <class Logger>` (compile-time injection) |
| Interface Segregation | small virtual interfaces | small **concepts** / CRTP bases |
| Single Responsibility | many small classes | same — free (composition inlines) |
| Liskov | — | still applies to CRTP/variant "hierarchies" |

The structural benefits (testable, changeable, focused) are kept; the dispatch
mechanism moves from runtime (`virtual`) to compile-time (`template`/`variant`)
on the hot path.

---

## Andar kya hota hai

- SOLID has **no direct codegen** — it's about which classes exist and how they
  reference each other. The runtime cost is entirely in the **mechanism you
  pick** to satisfy it (virtual vs template vs variant vs plain function).
- "Many small classes" (SRP) — at `-O2`, forwarding and delegation inline away;
  the binary looks the same as one big class, but the source is maintainable.
- Dependency injection via `virtual` interface — one indirect call per use
  (file 12). Via template — zero, but code bloat per instantiation and slower
  compiles.

> **HFT relevance:** SOLID's goals (change one thing without breaking others,
> test in isolation, extend without editing) matter as much in HFT as anywhere —
> a trading system evolves constantly. The trap is implementing every "depends
> on an abstraction" as a `virtual` interface in the hot path. HFT teams keep
> the **structure** SOLID suggests but realize it with **compile-time**
> polymorphism (templates, concepts, CRTP, `std::variant`) on hot paths, and
> `virtual` interfaces only at cold boundaries (venue plugins, config sources,
> admin). "Single Responsibility" and "composition over inheritance" are free
> and always applied.

---

## Hands-on

Refactor `examples/02_virtual_functions.cpp`'s `Shape` hierarchy two ways:
1. **DIP with template:** `template <class ShapeT> double report(const ShapeT&
   s) { return s.area(); }` — no `Shape` base needed.
2. **OCP with variant:** `using AnyShape = std::variant<Circle, Rect,
   Square>;` + `std::visit`. Add a `Hexagon` — what does the compiler tell you?

```bash
./build.ps1 fast 16-OOP/examples/07_dispatch_benchmark.cpp   # virtual vs variant vs CRTP cost of these choices
```

---

## ⚠️ Traps

### Trap 1 — SRP → over-splitting tiny value types
```cpp
class Vec3Length { }; class Vec3Normalize { };   // ⚠️ "vector math" is one responsibility
```

### Trap 2 — OCP via virtual in a hot loop
```cpp
for (auto& s : shapes) total += s->area();   // ⚠️ open/closed achieved, but ~10x cost (file 12). variant/CRTP
```

### Trap 3 — LSP violation that "compiles fine"
```cpp
struct Square : Rectangle { };   // compiles, IS-A "true" -- but breaks Rectangle-assuming code at runtime
```

### Trap 4 — ISP → interface explosion
```cpp
struct IReadByte {}; struct IReadTwoBytes {}; struct IReadFourBytes {};   // ⚠️ too granular -- one IReader
```

### Trap 5 — DIP → `virtual` everywhere for testability
```cpp
struct IAdd { virtual int add(int,int) = 0; };   // ⚠️ don't abstract trivial pure functions. Template if you must
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "SOLID = use lots of `virtual` interfaces" | SOLID is structural; realize it with templates/variant on hot paths |
| "SRP = one method per class" | One *reason to change*; related ops stay together |
| "Square IS-A Rectangle, so inherit" | Behavioural substitutability (LSP) — Square breaks Rectangle's contract |
| "DIP needs a runtime interface" | Compile-time (`template`/concept) inversion works and is zero-cost |
| "SOLID hurts performance" | The principles don't; a lazy `virtual`-everywhere implementation does |

---

## Exercises

1. **Spot the violation:** `class ReportGenerator { void fetchData(); void
   computeStats(); void renderHtml(); void emailReport(); };` — which principle,
   how to fix?

   <details><summary>Answer</summary>

   SRP — 4 reasons to change (data source, stats logic, output format, delivery).
   Split into `DataFetcher`, `StatsEngine`, `HtmlRenderer`, `Mailer`;
   `ReportGenerator` orchestrates them (composition + DI).
   </details>

2. **OCP without virtual:** `double price(const Instrument& i)` with a
   `switch(i.type)` over `{Stock, Bond, Option}`. Convert to `std::variant` +
   `visit`. What happens when you add `Future` but forget to handle it?

   <details><summary>Answer</summary>

   `using Instrument = std::variant<Stock, Bond, Option, Future>;` — if `visit`'s
   visitor doesn't handle `Future` (e.g. a non-generic overload set), it's a
   **compile error**. Checked exhaustiveness — "closed to modification" enforced,
   zero dispatch cost.
   </details>

3. **LSP check:** `struct FileStream { virtual void write(std::span<const std::byte>);
   };  struct ReadOnlyStream : FileStream { void write(...) override { throw
   std::logic_error("read-only"); } };` — LSP violation? Fix.

   <details><summary>Answer</summary>

   Violation — code holding a `FileStream&` expects `write` to work; `ReadOnlyStream`
   throws where the base doesn't. Fix: segregate — `IReadable` / `IWritable`;
   `ReadOnlyStream : IReadable` only. Don't inherit a capability you can't
   provide.
   </details>

4. **DIP two ways:** `class MatchingEngine` needs a clock. Show (a) `IClock&`
   virtual injection, (b) `template <class Clock>` injection. Hot-path
   implication of each?

   <details><summary>Answer</summary>

   (a) `class MatchingEngine { IClock& clock_; ... clock_.nowNs(); };` — one
   virtual call per timestamp. (b) `template <class Clock> class MatchingEngine {
   Clock& clock_; ... };` — `clock_.nowNs()` inlines (e.g. to `rdtsc`). Same
   testability (`MockClock`), zero hot-path cost for (b).
   </details>

5. **ISP:** a `IOrderBook` interface with 12 methods; a read-only view component
   only needs `bestBid()`, `bestAsk()`, `depth()`. Redesign.

   <details><summary>Answer</summary>

   Split: `IOrderBookView { bestBid(); bestAsk(); depth(); }` (read) and
   `IOrderBookMutator { add(); cancel(); modify(); ... }` (write). The view
   component depends only on `IOrderBookView` — can't accidentally mutate, and a
   test double is 3 methods not 12.
   </details>

---

## Interview questions

1. SOLID — 5 principles, ek line har ek?
2. Open/Closed — `virtual` se kaise, aur `std::variant` se kaise (aur kyun HFT variant prefer kare)?
3. LSP violation ka classic example (Square/Rectangle) — kyun tut-ta?
4. Interface Segregation — fat interface ka problem, fix?
5. Dependency Inversion — virtual injection vs template injection, hot-path fark?
6. "SOLID performance ko hurt karta" — sach kya hai?

---

## Next
→ [`16-exercises.md`](16-exercises.md)
