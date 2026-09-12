# 14 — Composition vs inheritance

## Prerequisites
- [`01-inheritance-basics.md`](01-inheritance-basics.md) … [`13-crtp.md`](13-crtp.md)

## Yeh topic abhi kyun
Sabse important OOP design decision: jab do types related hain, **inheritance**
use karein ya **composition** (ek type ko doosre ka member banayein)? Beginners
inheritance overuse karte hain. Modern guidance clear hai: **"prefer composition
over inheritance"** — aur kab exception hai.

---

## Do relationships

```cpp
// INHERITANCE -- "IS-A"
class Car : public Vehicle { };          // a Car IS-A Vehicle

// COMPOSITION -- "HAS-A"
class Car {
    Engine engine_;                       // a Car HAS-A(N) Engine
    Wheel  wheels_[4];
    FuelTank tank_;
};
```

- **Inheritance:** `Derived` **is a kind of** `Base`. Substitutable —
  `Base*`/`Base&` ke through use hota hai. Interface + behaviour inherit.
- **Composition:** ek class doosre ko **contain** karti hai, uske through kaam
  karti hai, par usse "hai" nahi. Zyada control, zyada flexibility.

---

## "Prefer composition" — kyun

### 1. Inheritance tightly couples

`Derived` `Base` ke `protected` members, virtual layout, aur ABI se bandhi hoti
hai. `Base` badlo → saare deriveds affect. Composition mein sirf **public
interface** pe depend.

### 2. Inheritance is compile-time & permanent

`Car : Vehicle` hamesha ke liye fixed. Composition mein aap member ko runtime pe
swap kar sakte (`std::unique_ptr<Engine> engine_` — petrol ya electric).

### 3. Inheritance exposes the base's whole interface

```cpp
class Stack : public std::vector<int> { };   // ⚠️ Stack ab .insert(), .erase(), [] sab expose karta -- Stack ki invariant tut sakti
```

Composition mein aap **sirf woh operations expose** karte ho jo aap chahte ho:

```cpp
class Stack {
    std::vector<int> data_;
public:
    void push(int x) { data_.push_back(x); }
    void pop()       { data_.pop_back(); }
    int  top() const { return data_.back(); }
    bool empty() const { return data_.empty(); }
    // koi .insert(), .erase() -- Stack ki abstraction clean
};
```

### 4. No slicing, no diamond, no vtable cost

Composition mein object slicing (file 08), diamond (file 10), aur virtual
dispatch cost (file 12) ka sawaal hi nahi.

### 5. Multiple "roles" easily

```cpp
class Player {
    Transform  transform_;      // has position/rotation
    Health     health_;         // has HP
    Inventory  inventory_;       // has items
    // add/remove "components" freely -- entity-component pattern
};
```

Multiple inheritance se yeh mumkin hai par pointer-adjustment / diamond risk ke
saath. Composition clean.

---

## Kab inheritance genuinely sahi hai

1. **Genuine "IS-A" + Liskov substitutability** — har jagah jahan `Base` use
   hota hai, `Derived` bhi sahi kaam kare. (Liskov Substitution Principle — file
   15.)

   ```cpp
   double totalArea(const std::vector<std::unique_ptr<Shape>>& shapes);
   // har Shape subclass yahan valid hai -> inheritance justified
   ```

2. **Runtime polymorphism over an open set** — plugin architectures, where new
   types come from other code / config, and you dispatch through `Base*`.

3. **Framework "fill in the blank"** — a base with a non-virtual algorithm +
   virtual hooks (Template Method, file 03), where derived classes customize
   specific steps.

4. **Implementing an interface** — `class UdpFeed : public IFeed` — this is
   inheritance of a **pure abstract** class (no state), which is really "declare
   that I satisfy this contract". Safe, common.

Even here, in the hot path, prefer CRTP (file 13) or `std::variant` (file 12)
over `virtual`.

---

## The classic mistake

```cpp
// ❌ "Timer has useful methods, let me inherit"
class RequestHandler : public Timer {
    void handle() {
        start();            // Timer::start
        process();
        stop();             // Timer::stop
        log(elapsed());     // Timer::elapsed
    }
};
// Problem: RequestHandler IS-A Timer? No. Now RequestHandler* -> Timer*,
// someone calls handler->reset() thinking it resets the timer... coupling, confusion.

// ✅ composition
class RequestHandler {
    Timer timer_;
    void handle() {
        timer_.start();
        process();
        timer_.stop();
        log(timer_.elapsed());
    }
};
```

Litmus test: **"Would I ever pass a `RequestHandler` where a `Timer` is
expected?"** No → composition.

---

## `private` inheritance — the middle ground (still prefer composition)

```cpp
class Widget : private Gadget {          // "implemented in terms of" -- not IS-A
    using Gadget::doThing;               // selectively expose
};
```

`private` inheritance = "reuse Gadget's implementation, but Widget is NOT a
Gadget" (no implicit upcast). Legit reasons: need to override Gadget's virtuals,
or access its `protected` members. **Otherwise composition is clearer** — 99% of
"private inheritance for reuse" should be a member.

---

## Andar kya hota hai

- **Composition** = the member's bytes are just laid out inside the containing
  object (like any struct — folder 11). `car.engine_.start()` = a direct call on
  a sub-object at a known offset. Zero indirection, fully inlinable.
- **Inheritance** (non-virtual) = same layout (base subobject at offset 0),
  same zero-cost method calls. The cost difference is **not** runtime — it's
  **coupling, extensibility, and the risks** (slicing, diamond, vtable if
  virtual).
- **Composition with `unique_ptr<Impl>`** (pImpl-style, runtime-swappable) — adds
  one indirection + one allocation. Use when you need the flexibility.
- Forwarding methods (`void push(int x) { data_.push_back(x); }`) — inlined away
  at `-O2`, so the "boilerplate" has no runtime cost, only source-code cost.

> **HFT relevance:** composition is the default for building hot components —
> an `OrderBook` HAS-A `std::vector<PriceLevel>` + a `SymbolId` + a `SequenceNo`,
> not IS-A anything. No base class → no vptr, flat layout, `static_assert`-able
> size, `memcpy`-relocatable where needed. Inheritance appears only as
> **interface implementation at boundaries** (a venue adapter IS-A `IVenue`),
> and even there the hot inner loop is templated (CRTP) or tag-dispatched. The
> "forwarding boilerplate" of composition is free at `-O2` and buys a clean,
> minimal, testable abstraction — worth it.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/01_inheritance.cpp     # inheritance (IS-A) baseline
```

Rewrite `01`'s `Car : public Vehicle` as `class Car { Vehicle chassis_; ... };`
with forwarding methods. Compare: which lets you `serviceVehicle(car)`? Which is
safer against `car` being used as a `Vehicle` accidentally? `-O2 -S` — same
generated code for the method calls?

---

## ⚠️ Traps

### Trap 1 — inheriting for method reuse
```cpp
class Cache : public std::unordered_map<K,V> { };   // ⚠️ exposes .clear(), .erase() -- Cache invariant leaks. Compose
```

### Trap 2 — "IS-A" that isn't
```cpp
class Square : public Rectangle { };   // ⚠️ classic LSP violation -- setWidth() breaks Square's invariant (file 15)
```

### Trap 3 — deep inheritance chains
```cpp
class A {}; class B : A {}; class C : B {}; class D : C {}; class E : D {};
// ⚠️ change A -> ripples through all. Hard to reason. Flatten + compose
```

### Trap 4 — private inheritance where a member fits
```cpp
class Logger : private FileHandle { };   // ⚠️ FileHandle member + forwarding is clearer
```

### Trap 5 — assuming composition has runtime cost
Forwarding methods inline away. Non-`unique_ptr` composition = zero overhead.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Inheritance is the main OOP tool for reuse" | Composition is; inheritance for genuine IS-A + polymorphism |
| "Composition has forwarding overhead" | Inlined at `-O2` — zero runtime cost |
| "If it has useful methods, inherit it" | Only if IS-A + substitutable; else HAS-A |
| "`private` inheritance = safe reuse" | A member is usually clearer; `private` inherit for virtuals/protected only |
| "More inheritance = better design" | Flat + composed is easier to change, test, reason about |

---

## Exercises

1. **Refactor:** `class EventLogger : public std::ofstream { public: void
   logEvent(const Event& e) { *this << format(e) << '\n'; } };` — rewrite with
   composition. What does the caller lose / gain?

   <details><summary>Answer</summary>

   `class EventLogger { std::ofstream out_; public: explicit EventLogger(const
   std::string& path) : out_(path) {} void logEvent(const Event& e) { out_ <<
   format(e) << '\n'; } };` — loses: `logger << "raw text"` and all ofstream API
   (good — not the abstraction). Gains: `EventLogger` can't be misused as a
   generic stream; can swap the sink later.
   </details>

2. **IS-A test:** for each, inheritance or composition? `Button`/`Rectangle`,
   `Car`/`Engine`, `Circle`/`Shape`, `PasswordField`/`TextField`,
   `Thread`/`Runnable`.

   <details><summary>Answer</summary>

   `Button` HAS-A `Rectangle` (bounds) — composition (or `Button IS-A Widget`).
   `Car` HAS-A `Engine` — composition. `Circle` IS-A `Shape` — inheritance
   (polymorphism). `PasswordField` IS-A `TextField` — arguably inheritance (with
   care re: LSP). `Thread` runs a `Runnable` — composition (`Thread` holds a
   callable).
   </details>

3. **Forwarding cost:** `class BoundedQueue { std::deque<int> q_; size_t cap_;
   public: void push(int x) { if (q_.size() < cap_) q_.push_back(x); } ... };` —
   `-O2 -S`: is `push` a call into `deque::push_back` + a compare, or is there
   extra "wrapper" overhead?

   <details><summary>Answer</summary>

   Just the `size() < cap_` compare + a (possibly inlined) `deque::push_back`.
   No wrapper overhead — the forwarding method is inlined. Composition's cost is
   in source code, not machine code.
   </details>

4. **Component style:** design a `class MarketDataHandler` that needs: a
   `RingBuffer` for inbound, a `GapDetector`, a `Decoder`, and a `SubscriberList`.
   Composition or a 4-base multiple inheritance? Why?

   <details><summary>Answer</summary>

   Composition — `MarketDataHandler` HAS-A each of those; it is not "a RingBuffer
   and a GapDetector and ...". Members give named access
   (`gapDetector_.check(seq)`), no pointer-adjust/diamond, easy to unit-test each
   part, easy to mock.
   </details>

5. **When inheritance wins:** you're writing a library where users provide
   `class MyStrategy : public Strategy { void onTick(const Tick&) override; }`
   and the engine holds `std::vector<std::unique_ptr<Strategy>>`. Is inheritance
   right here? What if `onTick` is called 1M/sec?

   <details><summary>Answer</summary>

   For an **open** user-extensible set dispatched at runtime — yes, inheritance
   (a pure abstract `Strategy`). If `onTick` is 1M/sec and the strategy set is
   actually **closed** (you control all of them), prefer CRTP/`variant` +
   templated engine. If it's genuinely user-plugin and 1M/sec — measure; you may
   accept the virtual cost, or provide a batched `onTicks(span<Tick>)` so one
   virtual call amortizes over many ticks.
   </details>

---

## Interview questions

1. "IS-A" vs "HAS-A" — inheritance vs composition, ek example har ek?
2. "Prefer composition over inheritance" — 3 concrete reasons?
3. Composition ka runtime cost? (`unique_ptr` composition vs value composition)
4. `class Stack : public std::vector<int>` — kya galat?
5. Inheritance genuinely kab sahi (3-4 cases)?
6. `private` inheritance vs a member — kab `private` inheritance?

---

## Next
→ [`15-solid-principles.md`](15-solid-principles.md)
