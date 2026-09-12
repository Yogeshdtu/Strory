# 13 — CRTP (Curiously Recurring Template Pattern)

## Prerequisites
- [`12-virtual-dispatch-cost.md`](12-virtual-dispatch-cost.md)
- Folder 15 file 13 (EBO), templates intro (folder 21 preview — bas `template<class T>` samajh lo)

## Yeh topic abhi kyun
CRTP = **static polymorphism** — virtual jaisa "base calls derived's behaviour"
pattern, par sab **compile time** pe resolve, **zero runtime cost** (no vptr, no
indirect call, fully inlinable). HFT mein virtual dispatch ka #1 replacement.
Naam ajeeb hai, idea simple: **base class ko derived ka type template parameter
ke roop mein do.**

---

## The pattern

```cpp
template <class Derived>
struct Base {
    void interface() {
        static_cast<Derived*>(this)->implementation();   // "call derived's version"
    }
};

struct Thing : Base<Thing> {                              // <-- curiously recurring: Thing : Base<Thing>
    void implementation() { std::cout << "Thing::implementation\n"; }
};

Thing t;
t.interface();      // -> Base<Thing>::interface -> Thing::implementation   (all resolved at compile time)
```

- `Base` ek **template** hai jo `Derived` type leta hai.
- `Thing` `Base<Thing>` se derive karta — apne aap ko template arg deta.
- `Base::interface()` `static_cast<Derived*>(this)` karke derived ka method call
  karta — **no vtable, no virtual**. Compiler ko exact type pata (`Derived ==
  Thing`), to `-O2` pe poora inline.

`static_cast<Derived*>(this)` safe hai kyunki `Thing` genuinely `Base<Thing>` se
derive karta — downcast valid.

---

## Use 1 — static interface (no vptr, inlinable)

```cpp
template <class D>
struct Shape {
    double area()      const { return static_cast<const D*>(this)->areaImpl(); }
    double perimeter() const { return static_cast<const D*>(this)->perimeterImpl(); }
};

struct Circle : Shape<Circle> {
    double r_;
    double areaImpl()      const { return 3.14159265 * r_ * r_; }
    double perimeterImpl() const { return 2 * 3.14159265 * r_; }
};

// templated consumer -- type known at compile time
template <class S>
double totalArea(const std::vector<S>& shapes) {
    double t = 0;
    for (const auto& s : shapes) t += s.area();   // s.area() -> areaImpl() -> fully inlined
    return t;
}
```

`examples/07_dispatch_benchmark.cpp`: CRTP ~= direct call (~1x), virtual ~10x.

---

## Use 2 — mixin (add behaviour via base)

```cpp
template <class D>
struct Comparable {                                    // derive 5 ops from operator<
    friend bool operator>(const D& a, const D& b)  { return b < a; }
    friend bool operator<=(const D& a, const D& b) { return !(b < a); }
    friend bool operator>=(const D& a, const D& b) { return !(a < b); }
    friend bool operator!=(const D& a, const D& b) { return !(a == b); }
};

struct Version : Comparable<Version> {
    int major_, minor_;
    friend bool operator<(const Version& a, const Version& b) {
        return std::tie(a.major_, a.minor_) < std::tie(b.major_, b.minor_);
    }
    friend bool operator==(const Version& a, const Version& b) {
        return a.major_ == b.major_ && a.minor_ == b.minor_;
    }
};
// Version now has <, ==, and >, <=, >=, != for free -- zero runtime cost
```

(C++20 `<=>` = default mostly obsoletes this specific one, but the mixin
technique generalizes — `Printable<D>`, `Hashable<D>`, `Iterable<D>`, etc.)

---

## Use 3 — static dispatch strategy

```cpp
template <class D>
struct Strategy {
    double evaluate(const MarketState& m) const {
        return static_cast<const D*>(this)->signal(m);   // compile-time
    }
};

struct MeanRevert : Strategy<MeanRevert> {
    double signal(const MarketState& m) const { return (m.fairValue - m.mid) * kGain_; }
    double kGain_ = 0.5;
};

template <class S>
Pnl backtest(const Strategy<S>& strat, std::span<const Bar> bars) {
    Pnl pnl{};
    for (const auto& b : bars) pnl += apply(strat.evaluate(toState(b)));  // evaluate() fully inlines
    return pnl;
}
```

`backtest<MeanRevert>` compiles to a tight loop with `signal()` inlined — no
vptr, no indirect call. The trade-off: `backtest` must be a template (or you
bind the concrete strategy at each call site).

---

## CRTP vs virtual — the trade-off

| | virtual | CRTP |
|---|---|---|
| Dispatch | runtime (vtable) | compile-time (`static_cast`) |
| Cost | indirect call + no inline (~5-15x) | **zero** — inlined |
| `sizeof` | +8 (vptr) | **+0** (EBO if `Base<D>` empty) |
| Heterogeneous container | ✅ `vector<unique_ptr<Base>>` | ❌ `Base<Circle>` ≠ `Base<Square>` |
| Add a new type without recompiling users | ✅ | ❌ (templates → recompile) |
| Binary size | one vtable | code bloat (one instantiation per type) |
| Error messages | clean | template errors (verbose) |
| Where | cold paths, plugin boundaries, open sets | hot paths, known types, mixins, policies |

**Use CRTP when:** the concrete type is known at the call site (a templated
algorithm, a strategy bound at startup), you need zero-cost, or you're building
a mixin. **Use virtual when:** you need a runtime-heterogeneous collection or an
open extension point and the cost doesn't matter.

Middle ground: `std::variant` (closed set, runtime, contiguous, no vptr — file
12).

---

## `static_cast` safety & the "static polymorphism doesn't check" caveat

```cpp
struct Wrong : Base<Thing> { };   // ⚠️ Wrong : Base<Thing> but Wrong is NOT Thing
Wrong w;
w.interface();                     // static_cast<Thing*>(this) -> UB (this isn't a Thing)
```

CRTP has no compile-time check that `Derived` is the class that inherited. A
common guard:

```cpp
template <class D>
struct Base {
    void interface() { impl().implementation(); }
private:
    D&       impl()       { return static_cast<D&>(*this); }
    const D& impl() const { return static_cast<const D&>(*this); }
    Base() = default;
    friend D;              // only D can construct Base<D> -> `Wrong : Base<Thing>` fails (can't construct)
};
```

`private Base()` + `friend D` → only the intended derived class can construct the
base → `Wrong : Base<Thing>` won't compile.

---

## Andar kya hota hai

- **No vptr** — `Base<D>` has no virtual functions → `Circle : Shape<Circle>` is
  just `[ Circle's members ]`, `Shape<Circle>` contributes 0 bytes (empty base →
  EBO, file 15 file 13).
- `s.area()` → `Shape<Circle>::area()` → `static_cast<const Circle*>(this)`
  (a no-op offset, `this` unchanged for single inheritance) → `Circle::areaImpl()`
  → **all inlined** at `-O2` into the arithmetic. Zero call overhead.
- **Code bloat:** `totalArea<Circle>` and `totalArea<Square>` are separate
  instantiations — more code in the binary (vs one shared virtual function). For
  a few types, fine; for hundreds, weigh it.
- Template error messages when the derived class is missing `areaImpl()` — verbose
  (a wall of "no member named areaImpl"). Concepts (folder 21) tame this.

> **HFT relevance:** CRTP is the standard way to get "polymorphic-looking" code
> with **zero dispatch cost** on the hot path — strategy objects bound at
> startup and run in templated hot loops, mixins for common behaviour
> (comparison, serialization boilerplate) with no vtable, and static interfaces
> for pluggable-but-compile-time components (a `Feed<UdpFeed>` used by a
> templated engine). The cost is compile time + binary size + template error
> noise — acceptable for the handful of hot types. Runtime-open extension
> (loading a strategy from a config string) still needs virtual/variant at the
> boundary, then hands off to a templated inner loop.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/08_crtp.cpp
./build.ps1 fast 16-OOP/examples/07_dispatch_benchmark.cpp   # CRTP ~= direct, virtual ~10x
```

`08_crtp.cpp` — `Printable<D>` mixin, `Ordered<D>` (5 ops from `operator<`),
static-dispatch `Strategy<D>` in a templated `backtest`.

---

## ⚠️ Traps

### Trap 1 — wrong derived type in the template arg
```cpp
struct A : Base<B> { };   // ⚠️ A : Base<B> -- static_cast<B*>(this) is UB. Use `friend D` + private ctor guard
```

### Trap 2 — expecting a heterogeneous container
```cpp
std::vector<Shape*> v;   // ❌ Shape is a template; Shape<Circle>* and Shape<Square>* are unrelated
```

### Trap 3 — forgetting `const` overload of the cast helper
```cpp
double area() const { return static_cast<D*>(this)->areaImpl(); }   // ❌ this is const D* -> need static_cast<const D*>
```

### Trap 4 — CRTP where virtual was actually needed (runtime open set)
```cpp
// plugin system loading unknown strategy types at runtime -> CRTP can't; use virtual/variant at the boundary
```

### Trap 5 — code bloat from many instantiations
```cpp
// process<T1>, process<T2>, ... process<T200> -> 200 copies of the loop. Measure binary size / I-cache
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "CRTP is just a weird template trick, no real use" | Zero-cost static polymorphism — heavily used (STL, Eigen, HFT) |
| "CRTP replaces virtual everywhere" | Only when the type is known at compile time; no heterogeneous container |
| "CRTP has some runtime cost" | Zero — resolved at compile time, inlined, no vptr (+0 sizeof via EBO) |
| "`static_cast<Derived*>(this)` is unsafe" | Safe *if* `Derived` really inherited; guard with `friend D` + private ctor |
| "CRTP is free with no downsides" | Compile time, binary bloat, template error noise |

---

## Exercises

1. **Basic CRTP:** `template <class D> struct Counter { void bump() {
   static_cast<D*>(this)->onBump(); ++count_; } int count_ = 0; };` — a `Clicker
   : Counter<Clicker>` that prints on each `onBump()`. `Clicker c; c.bump();
   c.bump();` — `c.count_`?

   <details><summary>Answer</summary>

   `Clicker` defines `void onBump() { std::cout << "click\n"; }`. `c.bump()`
   twice → prints "click" twice, `c.count_ == 2`. All inlined, no vtable.
   </details>

2. **Mixin:** `template <class D> struct Negatable { D operator-() const { D
   r = static_cast<const D&>(*this); r.negateInPlace(); return r; } };` — a
   `Vec2 : Negatable<Vec2>` with `x_, y_` and `negateInPlace()`. `-Vec2{1,2}`?

   <details><summary>Answer</summary>

   `Vec2::negateInPlace() { x_ = -x_; y_ = -y_; }`. `-Vec2{1,2}` → copies, negates
   → `Vec2{-1,-2}`. `Negatable<Vec2>` is an empty base (EBO → `sizeof(Vec2)` ==
   2 doubles).
   </details>

3. **sizeof:** `template <class D> struct Tag {};  struct Widget : Tag<Widget> {
   int a, b; };` — `sizeof(Widget)`? Compare to `struct Widget2 { Tag<Widget2>
   t_; int a, b; };`.

   <details><summary>Answer</summary>

   `sizeof(Widget) == 8` (EBO — empty base contributes 0). `sizeof(Widget2)` ==
   16 (empty **member** `t_` still takes 1 byte + 3 pad + 8). Base beats member
   for empty types.
   </details>

4. **CRTP vs virtual bench:** in `07_dispatch_benchmark.cpp`, the CRTP path
   (`crtp` vector of `CCircle`) — why is it ~1x direct, while virtual is ~10x?

   <details><summary>Answer</summary>

   CRTP: `c.area()` → `Shape<CCircle>::area()` → `static_cast<const
   CCircle*>(this)->areaImpl()` — all resolved at compile time, inlined to
   `3.14159*r*r`, loop vectorizes. Virtual: indirect call per element + no inline
   + pointer chase (`unique_ptr`).
   </details>

5. **Guard against misuse:** add the `friend D` + `private` default ctor guard to
   a CRTP `Base<D>`. Show that `struct Bad : Base<Good>` no longer compiles.

   <details><summary>Answer</summary>

   `template <class D> struct Base { private: Base() = default; friend D; public:
   /* interface */ };` — `struct Bad : Base<Good>` needs to construct
   `Base<Good>`, but only `Good` is a friend → "Base() is private" → compile
   error. Prevents the `static_cast<Good*>(this)` UB.
   </details>

---

## Interview questions

1. CRTP kya hai — pattern aur "curiously recurring" ka matlab?
2. CRTP static polymorphism virtual se kaise different (cost, container)?
3. `static_cast<Derived*>(this)` — safe kyun, kab UB, guard kaise?
4. CRTP se `sizeof` pe kya asar (EBO)?
5. CRTP mixin — ek example (comparison ops se)?
6. CRTP ke downsides (3)?

---

## Next
→ [`14-composition-vs-inheritance.md`](14-composition-vs-inheritance.md)
