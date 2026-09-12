# 11 — RTTI, `typeid`, `dynamic_cast`

## Prerequisites
- [`03-virtual-functions.md`](03-virtual-functions.md), [`04-vtable-deep-dive.md`](04-vtable-deep-dive.md)
- Folder 12 file 13, folder 15 (casts context)

## Yeh topic abhi kyun
**RTTI** (Run-Time Type Information) — runtime pe poochna "yeh `Base*` asli mein
kaunse type ko point karta hai?" Do tools: `typeid` (exact type identity) aur
`dynamic_cast` (safe downcast / cross-cast). Dono virtual functions par depend
karte hain, dono ki cost hai — aur HFT hot path mein dono **avoid** kiye jaate.

---

## `dynamic_cast` — safe polymorphic cast

```cpp
struct Shape { virtual ~Shape() = default; };
struct Circle : Shape { double r_; };
struct Square : Shape { double s_; };

Shape* sp = getShape();          // Circle? Square? pata nahi

if (Circle* c = dynamic_cast<Circle*>(sp)) {     // pointer version -> nullptr on mismatch
    std::cout << "circle radius " << c->r_;
} else {
    std::cout << "not a circle";
}

Shape& sr = *sp;
try {
    Square& sq = dynamic_cast<Square&>(sr);       // reference version -> throws std::bad_cast on mismatch
    std::cout << sq.s_;
} catch (const std::bad_cast&) {
    std::cout << "not a square";
}
```

- **`dynamic_cast<Derived*>(basePtr)`** — checked at runtime. Match → adjusted
  pointer. Mismatch → **`nullptr`**.
- **`dynamic_cast<Derived&>(baseRef)`** — match → reference. Mismatch → **throws
  `std::bad_cast`** (koi "null reference" nahi ho sakti).
- Requires the source type to be **polymorphic** (at least one `virtual`) — warna
  compile error.
- Can also do **cross-casts** (sibling base to sibling base in multiple
  inheritance) and cast to `void*` (gives the address of the most-derived
  object).

### `dynamic_cast` vs `static_cast` for downcast

```cpp
Circle* c1 = static_cast<Circle*>(sp);    // UNCHECKED -- UB if sp isn't a Circle. Fast (compile-time offset)
Circle* c2 = dynamic_cast<Circle*>(sp);   // CHECKED -- nullptr if wrong. Slower (runtime walk)
```

`static_cast` down = "main guarantee karta hoon yeh Circle hai" (jaise ek
variant/tag ne pehle bata diya). `dynamic_cast` = "check karo".

---

## `typeid` — type identity

```cpp
#include <typeinfo>

Shape* sp = getShape();

if (typeid(*sp) == typeid(Circle)) {          // EXACT type match (not "is-a")
    // *sp is exactly a Circle, not a subclass of Circle
}

std::cout << typeid(*sp).name();               // implementation-defined mangled name, e.g. "6Circle"
std::cout << typeid(int).name();               // "i"
```

- `typeid(expr)` → a `const std::type_info&`.
- On a **polymorphic** glvalue (`*sp`) → **dynamic** type (runtime lookup via
  vtable's RTTI pointer).
- On a non-polymorphic type or a type name → **static** type (compile-time).
- `type_info::name()` — mangled, implementation-defined (`c++filt` /
  `abi::__cxa_demangle` to read).
- `typeid` checks **exact** type, `dynamic_cast` checks **is-a** — different
  tools.

```cpp
struct A { virtual ~A() = default; };
struct B : A {};
B b; A& a = b;
typeid(a) == typeid(B);          // true  (exact dynamic type)
typeid(a) == typeid(A);          // false
dynamic_cast<A*>(&b) != nullptr; // true  (B is-a A)
```

---

## The cost

```cpp
// dynamic_cast<Derived*>(basePtr):
//   - load vptr, load RTTI pointer from vtable
//   - walk the inheritance graph comparing type_info (string compare on some
//     implementations, pointer compare on others), computing offsets
//   - single inheritance, simple hierarchy: ~a few ns
//   - multiple / virtual inheritance, deep graph: 10s-100s of ns, non-constant
```

- **`dynamic_cast`** — runtime type-graph traversal. Cost depends on hierarchy
  shape. GCC/Clang (Itanium ABI) — reasonably fast for simple single
  inheritance, slow for MI/virtual bases. Not constant-time.
- **`typeid` comparison** — usually a pointer compare of `type_info` addresses
  (fast) **within one binary**; across shared library boundaries it can fall
  back to string compare (slower, and can give surprising `!=` for the "same"
  type — an ODR/visibility issue).
- **`-fno-rtti`** — disables both. Removes RTTI data from vtables (smaller
  binary), makes `dynamic_cast` (except to/from `void*`) and `typeid` on
  polymorphic types a compile error. Some HFT / embedded builds use this.

---

## Kab RTTI use hota hai (aur alternatives)

| Situation | RTTI approach | Better approach |
|---|---|---|
| "Is this shape a Circle?" in a hot loop | `dynamic_cast<Circle*>` | **Virtual method** (`shape->area()`) — polymorphism, no cast |
| Closed set of known types | `dynamic_cast` chain | **`std::variant` + `std::visit`** (compile-time, exhaustive) |
| Deserializing to the right type | `typeid` map | **Type tag / enum** in the message + factory |
| Debug logging "what type is this" | `typeid(*p).name()` | fine (cold path) |
| Plugin: "does this support IExtra?" | `dynamic_cast<IExtra*>` | fine at a boundary (cold) |

**Guideline:** if you're reaching for `dynamic_cast` in application logic, ask
"why doesn't the base class have a virtual method for this?" — usually the design
should push the behaviour into the type, not query the type from outside
(open/closed — file 15). `dynamic_cast` is legitimate at **boundaries** (plugin
capability queries) and in **generic frameworks**, rarely in hot business logic.

---

## Andar kya hota hai

- Every polymorphic class's vtable has, just before the function pointers, a
  pointer to its **`std::type_info`** object (in `.rodata`), plus an
  **offset-to-top** (how far this subobject is from the most-derived object).
- `typeid(*polymorphicPtr)` → `load vptr; load type_info* at fixed negative
  offset`.
- `dynamic_cast<D*>(bp)` → get the most-derived object (via offset-to-top), get
  its `type_info`, then `__dynamic_cast` walks the recorded base-class graph in
  the RTTI structures matching `D`'s `type_info`, returning the adjusted pointer
  or null.
- `dynamic_cast<void*>(bp)` — special: just `bp - offset_to_top` (gives the
  address of the whole object). Cheap. Works even with `-fno-rtti`? No — still
  needs the vtable RTTI. (`-fno-rtti` allows only up/`static_cast`s.)
- With `-fno-rtti`, no `type_info` in vtables → smaller, but no `typeid` on
  polymorphic types, no `dynamic_cast`.

> **HFT relevance:** `dynamic_cast` and `typeid` are **off the hot path** —
> non-constant cost, and their presence usually signals a design that should use
> virtual dispatch or `std::variant` instead. Many HFT builds compile with
> **`-fno-rtti`** (smaller binaries, faster link, forces the discipline).
> Type discrimination in the hot path is done with an explicit **type tag**
> (`enum class MsgType : uint8_t`) read directly from the message + a `switch`
> or a function table — O(1), branch-predictable, no vtable walk. `dynamic_cast`
> survives only at cold boundaries (loading a venue plugin and asking "do you
> implement `IExtendedQuotes`?").

---

## Hands-on

```cpp
// rtti.cpp
#include <iostream>
#include <typeinfo>
struct Base { virtual ~Base() = default; };
struct D1 : Base { int a = 1; };
struct D2 : Base { int b = 2; };

void inspect(Base* p) {
    if (auto* d1 = dynamic_cast<D1*>(p)) std::cout << "D1 a=" << d1->a << "\n";
    else if (auto* d2 = dynamic_cast<D2*>(p)) std::cout << "D2 b=" << d2->b << "\n";
    else std::cout << "unknown\n";
    std::cout << "  typeid: " << typeid(*p).name() << "\n";
}
int main() { D1 x; D2 y; inspect(&x); inspect(&y); }
```

```bash
g++ -std=c++20 -Wall -Wextra rtti.cpp -o rtti && ./rtti
g++ -std=c++20 -fno-rtti rtti.cpp        # -> compile errors on dynamic_cast / typeid
```

---

## ⚠️ Traps

### Trap 1 — `dynamic_cast` on a non-polymorphic type
```cpp
struct A { };  A a;  dynamic_cast<B*>(&a);   // ❌ compile error -- A has no virtual
```

### Trap 2 — `dynamic_cast<D&>` and forgetting it can throw
```cpp
D& d = dynamic_cast<D&>(baseRef);   // ⚠️ std::bad_cast if wrong -- wrap or use pointer version
```

### Trap 3 — `typeid` "is-a" ki tarah use karna
```cpp
if (typeid(*p) == typeid(Base)) ...   // ⚠️ exact match -- a Derived object is NOT typeid Base
```

### Trap 4 — `dynamic_cast` in a hot loop
```cpp
for (auto* s : shapes) if (auto* c = dynamic_cast<Circle*>(s)) total += c->area();
// ⚠️ per-iteration type-graph walk. virtual area() or variant instead
```

### Trap 5 — `typeid` across shared library boundary
```cpp
// plugin returns a Base*; typeid(*p) == typeid(HostKnownType) may be FALSE
// even for the "same" type -- separate type_info per binary (visibility/ODR)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`dynamic_cast` is O(1) like `static_cast`" | Runtime graph walk; MI/virtual bases → slow, non-constant |
| "`dynamic_cast<D&>` returns null on failure" | Pointer version → null; **reference version → throws** |
| "`typeid` checks is-a" | Exact type identity; `dynamic_cast` checks is-a |
| "`typeid` compare is always a fast pointer compare" | Within one binary usually; across `.so` may string-compare |
| "RTTI is free, always available" | Adds vtable data; `-fno-rtti` removes it (and `dynamic_cast`/`typeid`) |

---

## Exercises

1. **Pointer vs reference:** `Shape* sp = new Circle;` — `dynamic_cast<Square*>(sp)`
   ka result? `dynamic_cast<Square&>(*sp)` ka?

   <details><summary>Answer</summary>

   `dynamic_cast<Square*>(sp)` → `nullptr`. `dynamic_cast<Square&>(*sp)` →
   throws `std::bad_cast`.
   </details>

2. **typeid exact:** `struct A { virtual ~A() = default; };  struct B : A {};
   struct C : B {};  C c;  A& a = c;` — `typeid(a) == typeid(C)`?
   `== typeid(B)`? `== typeid(A)`? `dynamic_cast<B*>(&a) != nullptr`?

   <details><summary>Answer</summary>

   `typeid(a) == typeid(C)` → true (exact dynamic type). `== typeid(B)` → false.
   `== typeid(A)` → false. `dynamic_cast<B*>(&a)` → non-null (`C` is-a `B`).
   </details>

3. **Replace dynamic_cast:** `void render(Shape* s) { if (auto* c =
   dynamic_cast<Circle*>(s)) drawCircle(c); else if (auto* r =
   dynamic_cast<Rect*>(s)) drawRect(r); }` — redesign with a virtual method.

   <details><summary>Answer</summary>

   Add `virtual void render(Renderer&) const = 0;` to `Shape`; `Circle::render`
   calls `drawCircle`, `Rect::render` calls `drawRect`. Caller: `s->render(rr);`
   — no cast, open to new shapes.
   </details>

4. **Closed set → variant:** same `Shape` problem but you have exactly
   `{Circle, Rect, Triangle}`. `std::variant` version of the render dispatch?

   <details><summary>Answer</summary>

   `using AnyShape = std::variant<Circle, Rect, Triangle>;` then `std::visit([&]
   (const auto& s) { s.render(rr); }, shape);` — compile-time exhaustive, no
   RTTI, arms can inline.
   </details>

5. **-fno-rtti:** ek codebase jo `-fno-rtti` build karti hai. `dynamic_cast<Derived*>`
   kahin chahiye — 2 alternatives (design-level)?

   <details><summary>Answer</summary>

   (a) Add a `virtual` method to the base for the behaviour you were switching
   on. (b) Add an explicit type tag (`enum class Kind`) + a `static_cast` after
   checking the tag, or a `std::variant`. (c) A hand-rolled `virtual Kind
   kind() const` + `static_cast`.
   </details>

---

## Interview questions

1. `dynamic_cast` pointer vs reference version — failure behaviour?
2. `dynamic_cast` vs `static_cast` for downcasting — safety vs cost?
3. `typeid` — exact type ya is-a? Polymorphic vs non-polymorphic operand?
4. `dynamic_cast` ki cost — kis par depend karti?
5. `-fno-rtti` kya hatata, kya break hota?
6. Hot path mein type discrimination — RTTI ki jagah kya (2 approaches)?

---

## Next
→ [`12-virtual-dispatch-cost.md`](12-virtual-dispatch-cost.md)
