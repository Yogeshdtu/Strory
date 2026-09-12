# 09 — Multiple inheritance

## Prerequisites
- [`01-inheritance-basics.md`](01-inheritance-basics.md), [`04-vtable-deep-dive.md`](04-vtable-deep-dive.md), [`06-abstract-classes.md`](06-abstract-classes.md)

## Yeh topic abhi kyun
C++ ek class ko **ek se zyada base classes** se derive karne deta hai. Interfaces
(pure abstract) ke saath yeh clean hai — ek class kai contracts implement kare.
State-full bases ke saath yeh problematic (name clashes, pointer adjustment, aur
diamond — file 10). Kya safe hai, kya nahi.

---

## Syntax

```cpp
struct Drawable   { virtual void draw() const = 0;  virtual ~Drawable() = default; };
struct Clickable  { virtual void onClick() = 0;     virtual ~Clickable() = default; };
struct Serializable{ virtual std::string save() const = 0; virtual ~Serializable() = default; };

class Button : public Drawable, public Clickable, public Serializable {   // 3 base classes
public:
    void draw() const override    { /* ... */ }
    void onClick() override        { /* ... */ }
    std::string save() const override { return "..."; }
};
```

`class D : public A, public B, public C` — comma-separated. Har base ka apna
access mode. Construction order = **declaration order** (`A`, `B`, `C`), not
init-list order (file 02).

---

## The "good" case — multiple interfaces

```cpp
Button b;

Drawable&    d = b;    d.draw();
Clickable&   c = b;    c.onClick();
Serializable& s = b;   std::cout << s.save();
```

Ek class kai **capabilities** expose karti hai. Yeh Java/C# ke "implements
multiple interfaces" jaisa — **safe aur common**, kyunki interfaces ki koi state
nahi (no diamond of data — file 10).

---

## Pointer adjustment — non-primary bases

```
   Button object layout (typical):
   offset 0:  [ Drawable vptr    ]  <- "primary" base, shares Button's vptr slot
   offset 8:  [ Clickable vptr   ]  <- secondary base, its own vptr
   offset 16: [ Serializable vptr]
   offset 24: [ Button's own members ]
```

```cpp
Button* bp = &b;
Drawable*    dp = bp;    // dp == bp        (offset 0)
Clickable*   cp = bp;    // cp == bp + 8    (POINTER ADJUSTED!)
Serializable* sp = bp;   // sp == bp + 16

std::cout << (void*)bp << " " << (void*)cp;   // different addresses, same object!
```

**Casting to a non-primary base adjusts the pointer value.** Consequences:
- `static_cast<void*>(bp) != static_cast<void*>(cp)` — same object, different
  addresses. `==` between `bp` and `cp` still works (compiler adjusts), but
  `reinterpret_cast` / raw pointer comparison across base types = bug.
- Calling a virtual through `cp` needs a **thunk** — a tiny stub that fixes
  `this` back to the object start, then jumps to the real function. Extra
  instructions per such call.
- `delete cp` (where `cp` is `Clickable*`) — needs virtual dtor so the runtime
  can compute offset-to-top and call `operator delete` with the object's real
  address.

---

## Name clashes

```cpp
struct Base1 { void log() { std::cout << "Base1::log\n"; } };
struct Base2 { void log() { std::cout << "Base2::log\n"; } };

struct D : Base1, Base2 { };

D d;
// d.log();               // ❌ ERROR -- ambiguous: Base1::log or Base2::log?
d.Base1::log();            // ✅ explicit
d.Base2::log();            // ✅

struct D2 : Base1, Base2 {
    void log() {           // D2's own -- resolves the ambiguity for D2 objects
        Base1::log();
        Base2::log();
    }
};
```

Agar do bases mein same-name member ho → derived mein us naam ka use **ambiguous**
→ explicitly qualify (`Base1::log`) ya derived mein override/shadow karo.

---

## Multiple inheritance with STATE — trouble

```cpp
struct Timer  { long long ticks_ = 0; void tick() { ++ticks_; } };
struct Logger { std::vector<std::string> lines_; void log(std::string s) { lines_.push_back(std::move(s)); } };

class Worker : public Timer, public Logger {   // 2 state-full bases -- ok-ish here (no shared base)
    // Worker has ticks_ AND lines_ -- fine as long as no DIAMOND
};
```

Do **unrelated** state-full bases → mostly fine (bas layout bada, pointer
adjustment). Problem tab jab dono bases ek **common base** share karein →
**diamond** → us common base ki 2 copies → ambiguity + double state → **virtual
inheritance** chahiye (file 10), jiska apna cost hai.

---

## Guidance

| Case | Verdict |
|---|---|
| Multiple **interfaces** (pure abstract, no state) | ✅ Safe, common — "implements many contracts" |
| One state-full base + several interfaces | ✅ Usually fine ("primary" base has the state) |
| Multiple state-full **unrelated** bases | ⚠️ Works; prefer composition (members) |
| Diamond (shared base with state) | ⚠️⚠️ Needs `virtual` inheritance (file 10) — avoid if possible |
| "Mixin" style (CRTP or empty policy bases) | ✅ Zero-cost (EBO), fine |

**Default advice:** multiple inheritance of **interfaces** = yes. Multiple
inheritance of **implementation/state** = prefer composition. Diamond = design
your way out.

---

## Andar kya hota hai

- **Layout:** primary base at offset 0 (shares vptr). Each additional
  polymorphic base gets its own vptr and lives at a later offset. Non-polymorphic
  bases just contribute their members.
- **Upcast to non-primary base** = `this + offset` (an `add`) — non-zero,
  compile-time-known constant.
- **Virtual call through a secondary base pointer** → the vtable entry points to
  a **thunk**: `sub rdi, 8` (fix `this`) then `jmp real_function`. A few extra
  instructions + an extra jump per such call.
- **`dynamic_cast` / `typeid`** use the "offset-to-top" and RTTI stored in the
  vtable to navigate between base subobjects (file 11).
- Multiple non-virtual bases: `sizeof` = sum of base sizes + own members +
  padding + N vptrs.

> **HFT relevance:** multiple inheritance of **interfaces** is fine and used at
> boundaries (an adapter that is both `IFeed` and `ILifecycle`). But in the hot
> path: (1) each secondary-base virtual call may go through a `this`-adjusting
> thunk (extra work), (2) non-primary base offsets mean pointer arithmetic on
> upcast, (3) diamonds + virtual inheritance add vbase pointers and
> variable-offset member access — all latency and complexity. Hot types are flat
> (single or no base, no virtual). Mixin-style behaviour reuse → CRTP (file 13,
> zero-cost via EBO), not runtime multiple inheritance.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/06_diamond.cpp
```

The diamond example also shows multiple inheritance. Add a small test: a `Button`
inheriting `Drawable` + `Clickable`, print `(void*)&b`, `(void*)(Clickable*)&b`
— see the pointer adjustment. Then a name clash between two bases and resolve it
with `Base1::` qualification.

---

## ⚠️ Traps

### Trap 1 — raw pointer compare across base types
```cpp
if (reinterpret_cast<void*>(clickablePtr) == reinterpret_cast<void*>(buttonPtr)) { }
// ⚠️ different values, same object. Use proper typed comparison (compiler adjusts)
```

### Trap 2 — name clash unresolved
```cpp
struct D : A, B { };  D d;  d.commonName();   // ❌ ambiguous. d.A::commonName()
```

### Trap 3 — `delete secondaryBasePtr` without virtual dtor
```cpp
Clickable* c = new Button;  delete c;   // ⚠️ needs virtual ~Clickable() -- else wrong address to operator delete
```

### Trap 4 — multiple state-full bases where composition fits
```cpp
class Widget : public Rectangle, public EventQueue { };   // ⚠️ "Widget IS-A EventQueue"? Nahi. Member banao
```

### Trap 5 — accidental diamond
```cpp
struct A {}; struct B : A {}; struct C : A {}; struct D : B, C {};   // ⚠️ 2x A subobject (file 10)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "C++ doesn't allow multiple inheritance" | It does; Java/C# restrict to interfaces |
| "All base pointers of an object are equal" | Only the primary base; others are adjusted |
| "Multiple inheritance is always bad" | Multiple **interfaces** is fine; multiple **state** — prefer composition |
| "Name clash between bases picks one" | Ambiguity error — qualify explicitly |
| "Secondary base virtual call = same cost" | May go through a `this`-adjusting thunk |

---

## Exercises

1. **Pointer adjust:** `struct A { virtual void a(); int x; };  struct B {
   virtual void b(); int y; };  struct C : A, B { int z; };  C c;` — is
   `(void*)&c == (void*)(A*)&c`? `== (void*)(B*)&c`? Approx `sizeof(C)`?

   <details><summary>Answer</summary>

   `(A*)&c == &c` (primary, offset 0). `(B*)&c == &c + 16` (A's vptr+int+pad,
   then B). `sizeof(C)` ≈ 8(vptrA)+4(x)+4pad + 8(vptrB)+4(y)+4(z) ... ≈ 40 (with
   padding). Two vptrs.
   </details>

2. **Resolve clash:** `struct Reader { std::string read(); };  struct Writer {
   std::string read(); /* returns last-written */ };  struct RW : Reader, Writer
   { };  RW rw;` — `rw.read()` error. Make `RW::read()` return the reader's.

   <details><summary>Answer</summary>

   `struct RW : Reader, Writer { std::string read() { return Reader::read(); }
   };` — or `using Reader::read;` if signatures were compatible (here both are
   `std::string read()`, so `using` both → still ambiguous; an explicit override
   is cleanest).
   </details>

3. **Interfaces are fine:** design a `class UdpFeed` implementing
   `IMarketDataFeed` + `ILifecycle` (`start()`, `stop()`) + `IStatsProvider`
   (`stats() -> Stats`). Any diamond risk?

   <details><summary>Answer</summary>

   No diamond — the three interfaces are independent, no shared base with state.
   `class UdpFeed : public IMarketDataFeed, public ILifecycle, public
   IStatsProvider { ... };` — clean multiple-interface inheritance.
   </details>

4. **Composition instead:** `class GameObject : public Transform, public
   RigidBody, public Renderable` (all state-full) — rewrite with composition.
   Trade-offs?

   <details><summary>Answer</summary>

   `class GameObject { Transform transform_; RigidBody body_; Renderable
   render_; public: ... };` — explicit sub-parts, no "IS-A" confusion, no
   pointer-adjust / diamond risk, easy to add/remove components. Trade-off:
   `gameObject.transform_.move(...)` vs inherited `gameObject.move(...)` (a bit
   more typing); no implicit upcast to `Transform&`.
   </details>

5. **Accidental diamond:** `struct Stream {};  struct InStream : Stream {};
   struct OutStream : Stream {};  struct IOStream : InStream, OutStream {};` —
   how many `Stream` subobjects in `IOStream`? Fix?

   <details><summary>Answer</summary>

   Two `Stream` subobjects (one via `InStream`, one via `OutStream`). Fix:
   `struct InStream : virtual Stream {}; struct OutStream : virtual Stream {};`
   → one shared `Stream` in `IOStream` (this is exactly why `std::iostream` uses
   virtual inheritance — file 10).
   </details>

---

## Interview questions

1. Multiple inheritance — kaunsa case safe (interfaces), kaunsa avoid?
2. Non-primary base ka pointer adjustment — kya, kyun matter karta?
3. `this`-adjusting thunk kab lagta?
4. Name clash between two bases — kya hota, kaise resolve?
5. Multiple state-full bases vs composition — trade-offs?
6. Accidental diamond kaise banta, ek-line preview of the fix?

---

## Next
→ [`10-virtual-inheritance.md`](10-virtual-inheritance.md)
