# 13 — vtable layout, exact. Multiple inheritance internals

## Prerequisites
- `16-OOP` (vtable/vptr intro, virtual dispatch), `12-casts-deep.md`
- [`examples/07_vtable_inspect.cpp`](examples/07_vtable_inspect.cpp)

## Yeh topic abhi kyun
Folder 16 mein "polymorphic object mein ek vptr hota hai" tha. Yahan **exact
layout** (Itanium ABI): vtable ke slots kya hain, `offset-to-top` aur `type_info`
kahan, multiple inheritance mein **do vptr** aur **thunks**, virtual inheritance ka
overhead. Yeh ABI knowledge (folder 24 file 07, 14) devirtualization,
`dynamic_cast` cost, aur "why is `sizeof` bigger than I expect" explain karta hai.

---

## Single inheritance — the common case

```cpp
struct Shape {
    virtual ~Shape();
    virtual double area() const;
    virtual const char* name() const;
    int tag;
};
struct Circle : Shape {
    double r;
    double area() const override;
    const char* name() const override;
};
```

### Object layout (Itanium, x86-64)

```
Circle object:
  offset 0:  vptr  ───────────────►  Circle's vtable
  offset 8:  int tag        (+ 4 padding)
  offset 16: double r
  sizeof(Circle) == 24
```

The vptr is **first** (offset 0). `Shape` subobject = `[vptr][tag][pad]` (16
bytes), `Circle` adds `r`.

### The vtable (in `.rodata`, one per class)

```
            ┌─ vtable for Circle ────────────────────────┐
  [-2]      │ offset-to-top  = 0   (this - complete obj)  │
  [-1]      │ &typeinfo for Circle                        │
  vptr ────►│ [0] Circle::~Circle()   (complete/D1)       │
  points    │ [1] Circle::~Circle()   (deleting/D0)       │
  here      │ [2] Circle::area()                          │
            │ [3] Circle::name()                          │
            └────────────────────────────────────────────┘
```

- **`offset-to-top`** — add this to `this` to get the complete object's address
  (0 for the primary base; non-zero for secondary bases in MI).
- **`&type_info`** — RTTI pointer, used by `dynamic_cast` / `typeid` (file 12).
- **Function slots** — in **declaration order** of the first class that declared
  them (dtor gets 2 slots: "complete object destructor" D1 and "deleting
  destructor" D0). Overrides **replace** the slot's pointer; new virtuals in a
  derived class **append**.
- A virtual call `p->area()`: `mov rax, [p]` (vptr) → `call [rax + 16]` (slot 2,
  each slot 8 bytes) — **one indirect call**, ~2-4 ns + a possible branch
  misprediction (folder 16 measured ~23 ns with a cold predictor).

`examples/07`: `c1` and `c2` (both `Circle`) share the same vptr; `s1` (`Square`)
has a different one.

---

## Multiple inheritance — two vptr, offset-to-top, thunks

```cpp
struct Shape     { virtual ~Shape();     virtual double area() const; };
struct Printable { virtual ~Printable();  virtual void print() const;  };
struct Both : Shape, Printable {
    double area() const override;
    void print() const override;
};
```

### Object layout

```
Both object:
  offset 0:  vptr(Shape)     ──►  Both's "Shape" vtable
  offset 8:  vptr(Printable) ──►  Both's "Printable" vtable
  sizeof(Both) == 16
```

**Two vptr** — one per polymorphic base. `examples/07` measures the `Printable`
subobject at **offset 8**.

### The pointer adjustment

```cpp
Both b;
Shape*     s = &b;   // s == &b        (offset 0)
Printable* p = &b;   // p == (char*)&b + 8   ← the cast CHANGES the pointer value!
```

`static_cast<Printable*>(&b)` isn't a no-op — it adds 8. `static_cast<Both*>(p)`
subtracts 8. `dynamic_cast<void*>(p)` uses the `Printable` vtable's `offset-to-top`
(−8) to recover `&b`.

### Thunks

`b.print()` called through a `Printable*`: `p` points at the `Printable`
subobject, but `Both::print()` expects a `Both*` (`this` at offset 0). So the
`Printable` vtable's `print` slot points at a **thunk**:

```
_ZThn8_N4Both5printEv:      ; "this-adjusting thunk, offset -8"
    sub  rdi, 8             ; adjust this: Printable* -> Both*
    jmp  _ZN4Both5printEv   ; tail-call the real function
```

Every MI override that needs a `this` adjustment gets a thunk — a tiny extra
indirection on that call path.

---

## Virtual inheritance — the vbase offset

```cpp
struct A { int a; virtual ~A(); };
struct B : virtual A { int b; };
struct C : virtual A { int c; };
struct D : B, C { int d; };   // one shared A
```

- The shared `A` subobject lives at **one** place in `D`; `B` and `C` reach it
  through a **virtual-base offset** stored in the vtable (not a fixed compile-time
  offset).
- Accessing `A::a` from a `B*` → load the vbase offset from `B`'s vtable → add to
  `this`. An **extra load per access** to a virtual base's members.
- `sizeof(D)` grows: multiple vptr + the vbase-offset machinery. The classic
  "diamond" cost.
- This is why virtual inheritance is rare outside `iostream`-style hierarchies —
  it's the most expensive layout.

---

## What breaks devirtualization

The compiler can turn `p->area()` into a direct call (or inline it) when it
**proves the dynamic type**:
- `Circle c; c.area();` — exact type known.
- `Circle c; Shape* s = &c; s->area();` — if `s` provably points at `c` and
  nothing could have changed it, GCC devirtualizes (folder 21 example 07's caveat:
  a single visible object → virtual == CRTP).
- `final` on the class or the method — no further override possible → devirtualize.
- LTO with whole-program visibility — if only one override exists.

Breaks it: a pointer/reference of base type whose target the compiler can't pin
(heterogeneous container, factory return, passed across a TU boundary without
LTO). Then it's a real indirect call.

---

## Andar kya hota hai (recap)

- vtable → `.rodata`, emitted in the TU that defines the class's **key function**
  (first non-inline non-pure virtual — usually the dtor). No key function → vtable
  emitted in every TU as COMDAT (folder 24 file 10: "undefined reference to
  vtable" = key function not defined).
- Constructor sets the vptr **as each subobject is constructed** — so during a
  base ctor, `this` has the *base's* vtable (virtual calls resolve to base
  overrides — folder 16 trap).
- `[[gnu::abi_tag]]`, `-fvtable-verify`, `-fstrict-vtable-pointers` — vtable
  hardening / optimization flags.

---

## > **HFT relevance**
> - **Single inheritance, one vptr, `final` where possible** — keeps virtual calls
>   cheap and devirtualizable. Avoid multiple inheritance of polymorphic bases on
>   hot types (thunks + pointer adjustments). Avoid virtual inheritance entirely
>   on the hot path (vbase-offset loads).
> - **CRTP / `std::variant` / tag dispatch** to eliminate the vtable indirection
>   where the type set is closed (folder 16, 21) — measured 10x (23 ns → 2 ns).
> - **`-fno-rtti`** drops the `type_info` slot; the class still has a vtable for
>   virtual calls, just no `dynamic_cast`/`typeid`.
> - **Layout stability** — a virtual method added/reordered in a base = vtable
>   layout change = **ABI break** (folder 24 file 07). Pin hot polymorphic
>   interfaces; add new virtuals only at the end, or use non-virtual extension
>   points.
> - **`sizeof` awareness** — a polymorphic type is `sizeof(fields) + 8` (one
>   vptr); MI adds 8 per polymorphic base. Budget it in cache-line calculations.

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/07_vtable_inspect.cpp
```

Shows: vptr at offset 0, same type → same vtable, MI → two vptr, `Printable`
subobject at offset 8, base-cast changes the pointer value. Then:
- `g++ -fdump-lang-class file.cpp` (GCC) → dumps the vtable layout for every class
  (slots, thunks, offset-to-top).
- `objdump -d -C -j .rodata` → find `vtable for Circle`, `_ZThn8_...` thunks.
- Add `final` to a class and check `objdump` — the virtual call becomes direct.

---

## ⚠️ Traps

### Trap 1 — assuming a base-cast pointer has the same value (MI)
`static_cast<Printable*>(&both)` adds an offset. Don't `reinterpret_cast` between
base pointers of an MI type.

### Trap 2 — `delete` through a non-virtual base pointer
```cpp
Base* p = new Derived;
delete p;   // ⚠️ if ~Base isn't virtual -> wrong dtor / wrong offset -> UB
```
Virtual destructor on any polymorphic base you `delete` through (folder 16).

### Trap 3 — virtual call in a constructor/destructor
Resolves to the *current* class's override, not the derived one — because the
vptr is set to the current subobject's vtable during (de)construction.

### Trap 4 — adding a virtual method "in the middle" of an interface
Shifts every later vtable slot → ABI break for anyone compiled against the old
header.

### Trap 5 — expecting `sizeof(Polymorphic) == sizeof(fields)`
`+ 8` for the vptr (one per polymorphic base).

### Trap 6 — MI thunks in a hot dispatch loop
Calling a `this`-adjusting override through the secondary base adds a thunk hop.
Prefer single inheritance for hot polymorphic types.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "one vtable per object" | One vtable per **class** (in `.rodata`); objects hold a vptr *to* it |
| "vtable is just function pointers" | Also `offset-to-top` and `&type_info` at negative indices |
| "base-cast is always a no-op" | Single inheritance: yes. MI: adds/subtracts an offset |
| "virtual call is basically free" | One indirect call + possible misprediction (~2–20+ ns) |
| "adding a virtual method is source-compatible = safe" | vtable layout changes → ABI break |
| "virtual inheritance is just multiple inheritance" | Adds vbase-offset loads per member access — the priciest layout |

---

## Exercises

1. **Layout:** `struct A { virtual ~A(); long x; }; struct B : A { long y; };` —
   `sizeof(A)`, `sizeof(B)`, offset of `y` in `B`?

   <details><summary>Answer</summary>

   `sizeof(A) == 16` (vptr 8 + `long x` 8). `sizeof(B) == 24` (vptr 8 + `x` 8 +
   `y` 8). `y` at offset 16.
   </details>

2. **Pointer values:** `struct S{virtual ~S();}; struct P{virtual ~P();}; struct
   Both:S,P{}; Both b;` — is `(void*)(S*)&b == (void*)(P*)&b`?

   <details><summary>Answer</summary>

   No. `(S*)&b` is `&b` (offset 0, primary base). `(P*)&b` is `&b + sizeof(S
   subobject)` (offset 8) — the `P` subobject has its own vptr and sits after `S`.
   </details>

3. **Devirtualize:** for each, can GCC turn `s->area()` into a direct call?
   (a) `Circle c; Shape& s = c;` (b) `Shape* s = factory();` (no LTO)
   (c) `struct Circle final : Shape {...}; Shape* s = get_circle();`

   <details><summary>Answer</summary>

   (a) Yes — `s` provably refers to a `Circle`. (b) No — `factory()`'s dynamic
   type is opaque without LTO/visibility info. (c) Often yes if the compiler can
   see it's a `Circle*` (the `final` means no `Circle` subclass can override), but
   it still needs to know `*s` is a `Circle` — `final` on `Circle::area()` or a
   known-`Circle` source helps.
   </details>

4. **Thunk:** why does `Both::print()` (from a `Printable*`) go through a
   `_ZThn8_...` thunk?

   <details><summary>Answer</summary>

   The `Printable*` points at the `Printable` subobject (offset 8 into `Both`),
   but `Both::print()` needs `this` to be a `Both*` (offset 0). The thunk does
   `this -= 8` then tail-calls the real `Both::print()`.
   </details>

5. **ABI break:** you add `virtual void reset();` between two existing virtual
   methods in a shipped base class header. What breaks and why?

   <details><summary>Answer</summary>

   Every virtual method declared after `reset()` shifts to a new vtable slot.
   Code compiled against the old header calls through the old slot indices → wrong
   function (or crash). Add new virtuals only at the **end**, or use a non-virtual
   forwarding method.
   </details>

---

## Interview questions

1. vtable ka exact layout — slots, `offset-to-top`, `type_info` kahan.
2. Polymorphic object ka `sizeof` — kitna extra, kyun (per polymorphic base)?
3. Multiple inheritance — kitne vptr, base-cast pointer value pe kya asar?
4. Thunk kya hai, kab generate hota (MI `this` adjustment)?
5. Virtual inheritance ka overhead — vbase offset.
6. Ctor/dtor ke andar virtual call kaunsa override resolve karta, kyun?
7. Devirtualization — compiler kab kar sakta, kya rokta hai?
8. Virtual method add karna kyun ABI break?

---

## Next
→ [`14-abi.md`](14-abi.md)
