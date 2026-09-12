# 10 — Virtual inheritance & the diamond problem

## Prerequisites
- [`09-multiple-inheritance.md`](09-multiple-inheritance.md), [`04-vtable-deep-dive.md`](04-vtable-deep-dive.md)

## Yeh topic abhi kyun
Jab do base classes ek **common base** se derive karti hain, aur ek class dono
se derive kare — us common base ki **do copies** ban jaati hain (diamond). Yeh
ambiguity aur double state deta hai. **Virtual inheritance** isse fix karta hai
(ek shared copy), par uska real layout + performance cost hai. HFT mein isse
avoid kiya jaata; par `std::iostream` jaise standard cheezein ispe bani hain, to
janna zaroori.

---

## The diamond

```cpp
struct Device {
    std::string serial_;
    explicit Device(std::string s) : serial_(std::move(s)) {}
};
struct Scanner : Device { Scanner() : Device("SCN") {} };
struct Printer : Device { Printer() : Device("PRN") {} };

struct Photocopier : Scanner, Printer { };   // ⚠️ diamond
```

```
        Device
        .    .
   Scanner    Printer      (dono Device se derive)
        .    .
     Photocopier
```

`Photocopier` mein **do `Device` subobjects** — ek `Scanner` ke through, ek
`Printer` ke through:

```cpp
Photocopier pc;
// pc.serial_;              // ❌ ERROR -- ambiguous: Scanner::Device::serial_ or Printer::Device::serial_?
static_cast<Scanner&>(pc).serial_;   // "SCN"
static_cast<Printer&>(pc).serial_;   // "PRN"  -- DIFFERENT object!
// pc.status();             // ❌ ambiguous (agar Device::status())
```

Do serial numbers, do sets of Device state, ambiguous access — clearly wrong for
a "Photocopier IS-A Device (one device)".

---

## The fix — `virtual` inheritance

```cpp
struct Device {
    std::string serial_;
    explicit Device(std::string s) : serial_(std::move(s)) {}
};
struct Scanner : virtual Device { Scanner() : Device("ignored") {} };   // virtual
struct Printer : virtual Device { Printer() : Device("ignored") {} };   // virtual

struct Photocopier : Scanner, Printer {
    Photocopier() : Device("COPY-42") { }    // MOST-DERIVED class initializes the virtual base
};
```

- `Scanner : virtual Device` — "meri `Device` shared ho sakti hai".
- Ab `Photocopier` mein **ek hi `Device`** subobject (Scanner aur Printer dono
  usi ko share karte).
- `pc.serial_` → **unambiguous** ("COPY-42").

### Virtual base initialization rule

**Virtual base ka constructor sirf MOST-DERIVED class call karti hai.**

```cpp
Scanner s;               // Scanner most-derived -> Scanner ka `: Device("ignored")` chalta
Photocopier pc;          // Photocopier most-derived -> Photocopier ka `: Device("COPY-42")` chalta;
                         //   Scanner/Printer ke `: Device("ignored")` IGNORED
```

Jab `Photocopier` banta hai, woh sabse pehle **directly** `Device` ko init
karta hai (Scanner/Printer ke Device-init statements skip). Agar `Photocopier`
apne init list mein `Device` na de aur `Device` ka default ctor na ho → error.

---

## Construction order with virtual bases

```
1. Virtual base classes   (Device)   -- depth-first, left-to-right, ONCE
2. Non-virtual base classes (Scanner, Printer)  -- declaration order
3. Members
4. Constructor body
```

Virtual bases **sabse pehle** — even before non-virtual bases — kyunki everyone
shares them.

---

## The cost

```cpp
struct A { int x; };
struct B1 : A          { int y; };          // non-virtual
struct B2 : virtual A   { int z; };          // virtual

std::cout << sizeof(B1);   // 8   (A's int + own int)
std::cout << sizeof(B2);   // 24  (vbase pointer + own int + A's int, with padding) -- varies
```

Virtual inheritance adds:
1. **Vbase pointer(s)** — each virtually-derived class stores a hidden pointer
   (or an offset in the vtable) to find the shared virtual base subobject. Extra
   size.
2. **Non-constant member offset** — accessing `serial_` from `Scanner&` needs an
   indirection (load the vbase offset, then add) — a virtual base member is not
   at a fixed offset from the derived object. Slower than a plain member access.
3. **Construction complexity** — most-derived-initializes rule, extra
   bookkeeping.
4. **`static_cast` to/from virtual base** — needs runtime offset lookup (can't
   `static_cast` **down** from a virtual base at all — must use `dynamic_cast`).

For a hierarchy that's hit millions of times/sec, that per-access indirection is
real.

---

## Where it's actually used

`std::basic_iostream` **is** a diamond, resolved with virtual inheritance:

```
        basic_ios
        .        .
basic_istream   basic_ostream      (both virtually inherit basic_ios)
        .        .
     basic_iostream
```

So `std::stringstream` (an `iostream`) has **one** `basic_ios` (one stream
state, one error flag set) shared by its input and output sides. Without virtual
inheritance you'd get two independent stream states — nonsense.

You'll also see virtual inheritance in some plugin / COM-style architectures.
**In application / HFT code — almost never write it yourself.**

---

## Andar kya hota hai

- Each class that virtually inherits stores a way to locate the shared vbase:
  GCC/Clang (Itanium ABI) put a **"virtual base offset"** in the vtable; the
  object holds the vptr, and member access to a vbase does `load vbase_offset
  from vtable; this + offset`.
- The **most-derived constructor** passes a hidden flag to base subobject
  constructors telling them "you're not most-derived, skip the vbase init".
- `dynamic_cast` and `dynamic_cast<void*>` rely on the offset-to-top +
  vbase-offset machinery to navigate.
- `sizeof` and layout with virtual bases are **implementation-defined** in the
  details (though ABI-stable per platform) — never `static_assert` exact layout
  or `memcpy` such types.

> **HFT relevance:** virtual inheritance is essentially banned in hot code —
> non-constant vbase member offsets (an extra dependent load per access), larger
> objects (vbase pointers), and complex construction all cost. If a diamond
> appears in a design, that's a signal to **flatten it**: use composition (each
> "side" holds a reference/pointer to one shared component), or a single class,
> or interfaces-only (pure abstract bases have no state → no diamond of data
> even without `virtual`). The one place you meet it is standard library
> iostreams; you rarely touch that layout directly.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/06_diamond.cpp
```

`bad::Photocopier` (2 `Device` subobjects, ambiguous `serial_`, `sizeof` bigger)
vs `good::Photocopier` (`virtual` inheritance, 1 shared `Device`, `serial_`
unambiguous, `Photocopier()` initializes `Device` directly).

---

## ⚠️ Traps

### Trap 1 — diamond without `virtual` → double base
```cpp
struct D : B, C { };   // B, C : A (non-virtual) -> 2x A. Ambiguous A members
```

### Trap 2 — forgetting the most-derived initializes the vbase
```cpp
struct D : virtual Base { };  struct MD : D { MD() {} };
// ⚠️ if Base has no default ctor -> error. MD() : Base(args) {}
```

### Trap 3 — `static_cast` down from a virtual base
```cpp
Base& b = md;  static_cast<D&>(b);   // ❌ can't static_cast from a virtual base. dynamic_cast
```

### Trap 4 — assuming fixed offsets / `memcpy` with virtual bases
```cpp
static_assert(offsetof(MD, x) == 8);   // ⚠️ virtual base member -> not a stable offset
```

### Trap 5 — using virtual inheritance to "share" a state-full base by design
```cpp
struct Config {}; struct A : virtual Config {}; struct B : virtual Config {}; struct AB : A, B {};
// ⚠️ works, but composition (AB holds one Config, A/B reference it) is usually cleaner + faster
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Diamond automatically shares the base" | Non-virtual diamond → **two** base subobjects |
| "`virtual` inheritance is free / like normal inheritance" | Vbase pointers, non-constant member offset, complex ctors |
| "Any class in the diamond can init the virtual base" | Only the **most-derived** class |
| "Interfaces need virtual inheritance to avoid diamonds" | Pure abstract (no state) → no diamond of data anyway |
| "You'll write virtual inheritance often" | Almost never in app code; iostreams use it internally |

---

## Exercises

1. **Count subobjects:** `struct A { int v; };  struct B : A {};  struct C : A
   {};  struct D : B, C {};` — how many `A` in `D`? `D d; d.v` — legal? Make `A`
   shared.

   <details><summary>Answer</summary>

   Two `A` subobjects. `d.v` ambiguous. Fix: `struct B : virtual A {}; struct C
   : virtual A {};` → one shared `A`, `d.v` unambiguous.
   </details>

2. **Most-derived init:** `struct Base { Base(int); };  struct L : virtual Base {
   L() : Base(1) {} };  struct R : virtual Base { R() : Base(2) {} };  struct LR
   : L, R { LR() : ??? {} };` — LR ctor kya likhega, `Base` mein kya value?

   <details><summary>Answer</summary>

   `LR() : Base(99) {}` — LR is most-derived, so **its** `Base(99)` runs; the
   `Base(1)` in `L` and `Base(2)` in `R` are ignored. `Base` ends up with 99.
   </details>

3. **sizeof:** `struct A { long x; };  struct B : A { long y; };  struct C :
   virtual A { long z; };` — `sizeof(A)`, `sizeof(B)`, `sizeof(C)` (approx, and
   why C is bigger).

   <details><summary>Answer</summary>

   `A` = 8, `B` = 16. `C` ≈ 24 — a vbase pointer (8) + `z` (8) + `A::x` (8),
   placed after, plus the indirection machinery. (Exact value ABI-dependent.)
   </details>

4. **iostream diamond:** why does `std::stringstream` need `basic_ios` to be a
   **virtual** base of `basic_istream` and `basic_ostream`? What breaks without
   it?

   <details><summary>Answer</summary>

   `stringstream` is both an istream and an ostream. Without virtual inheritance
   it would have two `basic_ios` → two independent stream states, two `rdstate()`
   / error flag sets, two buffers-of-record. Reading wouldn't see write errors,
   etc. Virtual → one shared stream state.
   </details>

5. **Flatten a diamond:** `struct Logger { void log(...); };  struct Producer :
   virtual Logger {};  struct Consumer : virtual Logger {};  struct Pipe :
   Producer, Consumer {};` — rewrite with composition so there's one `Logger`,
   no virtual inheritance.

   <details><summary>Answer</summary>

   `class Pipe { Logger logger_; ProducerLogic prod_{logger_}; ConsumerLogic
   cons_{logger_}; ... };` — `Pipe` owns one `Logger`; the producer/consumer
   parts take a `Logger&` in their constructors. No inheritance diamond, fixed
   offsets, easy to test.
   </details>

---

## Interview questions

1. Diamond problem — bina `virtual`, `Photocopier` mein kitne `Device`?
2. `virtual` inheritance kya fix karta, syntax?
3. Virtual base ko init kaun karta (most-derived rule)?
4. Virtual inheritance ki 3 costs?
5. `static_cast` down from a virtual base — allowed?
6. Standard library mein virtual inheritance kahan (iostreams), kyun zaroori?

---

## Next
→ [`11-rtti-and-dynamic-cast.md`](11-rtti-and-dynamic-cast.md)
