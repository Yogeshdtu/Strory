# 11 — CRTP deep, static polymorphism, mixins

## Prerequisites
- [`03-class-templates.md`](03-class-templates.md), folder 16 file 13 (CRTP intro), folder 16 (virtual dispatch)
- [`07-if-constexpr.md`](07-if-constexpr.md)

## Yeh topic abhi kyun
**CRTP** (Curiously Recurring Template Pattern) = `class Derived : Base<Derived>`.
Base ko pata hai apna Derived kaun hai → base Derived ke methods **compile time
par** call kar sakta. Yeh virtual dispatch ka **zero-cost** version hai —
`examples/07_crtp_policy.cpp`: virtual (real boundary) ~2.4 ns/call vs CRTP
~0.56 ns/call.

---

## The pattern

```cpp
template <class Derived>
class Shape {
public:
    double area()      const { return self().area_impl(); }
    double perimeter() const { return self().perimeter_impl(); }
    void   describe()  const {
        std::printf("area=%.2f perimeter=%.2f\n", area(), perimeter());   // "template method" -- calls into Derived
    }
private:
    const Derived& self() const { return static_cast<const Derived&>(*this); }
};

struct Circle : Shape<Circle> {
    double r;
    double area_impl()      const { return 3.14159265 * r * r; }
    double perimeter_impl() const { return 2 * 3.14159265 * r; }
};

Circle c{.r = 2};
c.describe();   // Shape<Circle>::describe -> area() -> self().area_impl() -> Circle::area_impl -- all inlined
```

- The base is a **template** on the derived type.
- `static_cast<const Derived&>(*this)` is safe because `*this` really *is* a
  `Derived` (that's the contract — a class must pass **itself** as the argument).
- No `virtual`, no vptr → `sizeof(Circle)` is unchanged (`examples/07`: 8 bytes,
  just the `double`).
- The call chain resolves at compile time → the whole `describe()` inlines to
  the arithmetic.

---

## CRTP vs virtual

| | `virtual` (dynamic) | CRTP (static) |
|---|---|---|
| dispatch resolved | runtime (vtable lookup) | compile time |
| per-call cost | load vptr + load slot + indirect call, **not inlined** | inlined — zero |
| object size | +8 bytes (vptr) | unchanged |
| heterogeneous containers | `vector<unique_ptr<Base>>` — yes | no — each `Shape<Circle>` is a distinct type |
| runtime type switching | yes (a `Base*` can point at any derived) | no — type is fixed at compile time |
| binary size | one function | one instantiation per derived |
| errors | clean | template errors |

**Measured** (`examples/07`, `-O2`, this box): virtual through a real polymorphic
boundary (heterogeneous `vector<unique_ptr<Base>>`) ~2.4 ns/call; CRTP ~0.56
ns/call. Note: if the concrete type is *visible* at the call site, the compiler
**devirtualizes** and virtual == CRTP — the gap is only at a genuine boundary
where it can't see the target.

**Use CRTP when** the set of types is known at compile time and each call site
knows which one it has. **Use `virtual`** when you need a container of mixed types
or runtime type selection.

---

## Mixins — inject behaviour via CRTP

A CRTP base can add whole capabilities to any class that opts in:

```cpp
// give any class == and != for free, given it defines a `key()`
template <class Derived>
struct EqualityComparable {
    friend bool operator==(const Derived& a, const Derived& b) { return a.key() == b.key(); }
    friend bool operator!=(const Derived& a, const Derived& b) { return !(a == b); }
};

// a counter of live instances
template <class Derived>
struct InstanceCounter {
    static inline std::size_t live = 0;
    InstanceCounter()  { ++live; }
    ~InstanceCounter() { --live; }
    InstanceCounter(const InstanceCounter&) { ++live; }
};

struct Widget : EqualityComparable<Widget>, InstanceCounter<Widget> {
    int id;
    int key() const { return id; }
};

Widget a{1}, b{1};
a == b;                    // from EqualityComparable<Widget>
Widget::live;              // from InstanceCounter<Widget>
```

Each mixin's members are inlined into `Widget` with no indirection. This is how
`boost::operators`, `std::enable_shared_from_this`, and `std::ranges::view_
interface` work.

---

## `std::enable_shared_from_this` — a real CRTP in the standard

```cpp
class Session : public std::enable_shared_from_this<Session> {
    void schedule() {
        auto self = shared_from_this();     // a shared_ptr<Session> -- CRTP base holds a weak_ptr back to the control block
        asyncOp([self]{ self->run(); });    // keep the object alive for the duration of the async op
    }
};
```

The CRTP base `enable_shared_from_this<Session>` stores a `weak_ptr<Session>`;
`shared_from_this()` locks it. Must be created via `std::make_shared<Session>()` /
`shared_ptr<Session>(new Session)` — never a stack object (folder 17 file 05).

---

## Pitfalls of CRTP

- **Wrong `Derived` argument** — `struct B : Shape<A>` (should be `Shape<B>`) →
  `static_cast<A&>(*this)` is a lie → UB. Guard with a `static_assert` or a
  `friend Derived` + private constructor:
  ```cpp
  template <class Derived>
  class Shape {
      Shape() = default;
      friend Derived;         // only Derived can construct the base -> can't pass someone else's type
  };
  ```
- **Calling a `Derived` method the derived class forgot to define** → recursion or
  a confusing error (the base's fallback calls itself).
- **No common base type** — you can't hold `Shape<Circle>` and `Shape<Square>` in
  one container. If you need that, add a runtime interface *on top*, or use
  `std::variant<Circle, Square>` + `std::visit` (folder 19 file 15).
- **Diagnostics** — an error in a mixin shows up at the point of instantiation
  with template context.

---

## Andar kya hota hai

- `static_cast<Derived&>(*this)` is a **compile-time** cast — the offset of the
  `Base<Derived>` subobject within `Derived` is known, so it's pointer
  arithmetic by a constant (usually 0). No runtime check, no vtable.
- Because `self().area_impl()` names a concrete function, the compiler inlines it;
  `describe()` collapses to the body of `area_impl` + `perimeter_impl` + the
  `printf`. The optimizer then treats it like straight-line code — can vectorize
  a loop over `describe()` calls on a `std::vector<Circle>`.
- `virtual` through a `Base*` the compiler can't resolve: `mov rax, [obj]` (load
  vptr), `call [rax + offset]` (indirect) — the indirect call also blocks
  inlining and hurts the branch-target predictor for a mixed call site.
  `examples/08` shows the same: template/variant ~1.1 ns vs virtual ~2.5 ns.
- Mixin members are ordinary members of the final class — `EqualityComparable<
  Widget>` contributes hidden-friend `operator==` found by ADL, zero size (empty
  base → EBO, folder 15).

> **HFT relevance:** CRTP is the standard way to get **polymorphic-style code
> organization with zero dispatch cost** on the hot path — a family of strategy /
> handler / feed-parser classes sharing a template-method base, each call
> inlined, `sizeof` unchanged (no vptr → tighter cache packing). Mixins add
> capabilities (comparison, serialization hooks, instance counting, intrusive
> list links) without indirection. The constraint — no heterogeneous container,
> type fixed at compile time — matches the hot path, where you *do* know which
> strategy is active. When you genuinely need runtime selection among a **small
> known set**, `std::variant<A, B, C>` + `std::visit` (jump-table dispatch, one
> predictable indirect call) is the middle ground; `virtual` is reserved for the
> control plane / large open type sets.

---

## Hands-on

```bash
./build.ps1 fast 21-TEMPLATES/examples/07_crtp_policy.cpp
./build.ps1 fast 21-TEMPLATES/examples/08_compile_time_dispatch.cpp
```

Write a CRTP `Comparable<Derived>` mixin (`<`, `<=`, `>`, `>=` from a single
`compare()`), a CRTP `Printable<Derived>` (`to_string()` calling
`Derived::fields()`), and confirm `sizeof` is unchanged and the calls inline
(`./build.ps1 asm`).

---

## ⚠️ Traps

### Trap 1 — passing the wrong type as `Derived`
```cpp
struct Bad : Shape<Good> { ... };   // ⚠️ static_cast<Good&>(*this) on a Bad -> UB. friend Derived + private ctor guards this
```

### Trap 2 — forgetting to define an `_impl` in the derived class
```cpp
struct Tri : Shape<Tri> { /* no area_impl */ };
// Shape<Tri>::area() calls self().area_impl() -> not found, or (with a base fallback) infinite recursion.
```

### Trap 3 — wanting a heterogeneous container
```cpp
std::vector<Shape<???>> shapes;   // ❌ no single type. Use vector<variant<Circle, Square>> or a runtime interface
```

### Trap 4 — `enable_shared_from_this` on a stack object
```cpp
Session s; s.shared_from_this();   // ⚠️ no controlling shared_ptr -> std::bad_weak_ptr. make_shared only
```

### Trap 5 — assuming virtual is always slower
```cpp
// If the concrete type is visible, the compiler devirtualizes -> virtual == CRTP. The gap is at a real Base* boundary.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "CRTP needs a vtable" | No `virtual`, no vptr — `sizeof` unchanged; dispatch is a compile-time cast |
| "CRTP replaces `virtual` in all cases" | Only when types are known at compile time — no heterogeneous containers |
| "A CRTP mixin adds size / indirection" | Empty base → EBO → 0 bytes; members inline |
| "Virtual is always ~10× slower" | Only through an unresolvable `Base*`; visible types get devirtualized |
| "`static_cast<Derived&>(*this)` is a runtime downcast" | Compile-time, constant-offset — safe *if* the contract (pass yourself) holds |

---

## Exercises

1. **Template method:** implement a CRTP `Serializer<Derived>` with a public
   `save()` that calls `Derived::write_fields(buf)`. Show a `Trade` using it.

   <details><summary>Answer</summary>

   `template <class D> struct Serializer { std::vector<std::byte> save() const {
   std::vector<std::byte> buf; static_cast<const D&>(*this).write_fields(buf);
   return buf; } };` then `struct Trade : Serializer<Trade> { ... void
   write_fields(std::vector<std::byte>& b) const { ... } };`
   </details>

2. **Guard:** add the `friend Derived` + private-constructor guard to a CRTP base
   and explain what it prevents.

   <details><summary>Answer</summary>

   `template <class D> class Base { Base() = default; friend D; };` — only `D`
   can construct `Base<D>`, so `struct X : Base<Y>` fails to compile (X can't
   call `Base<Y>`'s private ctor). Prevents the "wrong `Derived`" UB.
   </details>

3. **Mixin:** write `Incrementable<Derived>` that provides `operator++(int)`
   (post-increment) in terms of the derived's `operator++()` (pre-increment).

   <details><summary>Answer</summary>

   `template <class D> struct Incrementable { D operator++(int) { D& self =
   static_cast<D&>(*this); D copy = self; ++self; return copy; } };` — derived
   defines only `++x`; `x++` comes from the mixin.
   </details>

4. **CRTP vs variant:** you have 3 strategy types chosen at startup and called
   millions of times. CRTP, `variant`+`visit`, or virtual? Why?

   <details><summary>Answer</summary>

   If each call site knows its strategy at compile time (templated on it) → CRTP,
   fully inlined. If the choice is a single runtime value shared across call
   sites → `std::variant<S1,S2,S3>` + `std::visit` (jump table, one predictable
   indirect call, callee inlines inside the visitor). `virtual` only if the set
   is open/large or you need a heterogeneous container.
   </details>

5. **Devirtualization:** why did `examples/07` need a *heterogeneous*
   `vector<unique_ptr<VBase>>` to show the virtual-vs-CRTP gap?

   <details><summary>Answer</summary>

   With a single concrete object (`VPercent vp; VBase* p = &vp;`), GCC sees the
   dynamic type and devirtualizes the call → virtual becomes a direct inlined
   call, same as CRTP. A vector of mixed derived types hides the target from the
   optimizer → a real vtable indirect call → the ~2.4 ns vs 0.56 ns gap appears.
   </details>

---

## Interview questions

1. CRTP ka shape — `Base<Derived>`, base Derived ko kaise call karta?
2. CRTP vs virtual — dispatch, size, container, runtime switching?
3. Devirtualization — virtual kab CRTP jitna fast ho jaata?
4. Mixin CRTP se — ek capability inject karne ka example?
5. `enable_shared_from_this` CRTP kaise use karta?
6. Galat `Derived` argument ka UB — kaise guard karein?

---

## Next
→ [`12-tag-dispatch.md`](12-tag-dispatch.md)
