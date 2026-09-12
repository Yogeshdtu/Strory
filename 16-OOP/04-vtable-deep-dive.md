# 04 — vtable & vptr deep dive

## Prerequisites
- [`03-virtual-functions.md`](03-virtual-functions.md)
- Folder 12 (pointers, function pointers), folder 14 file 01 (`.rodata`)
- Folder 15 file 13 (class layout, `is_polymorphic`)

## Yeh topic abhi kyun
"Virtual call runtime pe resolve hota hai" — **kaise?** Iska mechanism —
**vtable** (per-class function-pointer table) aur **vptr** (per-object hidden
pointer) — samajhna zaroori: iski cost (file 12), object size impact, aur `virtual`
kyun `memcpy`/pool/wire types ke saath incompatible hai.

---

## vptr — object ke andar hidden pointer

```cpp
struct NoVirtual { int a_; int b_; };            // sizeof == 8

struct OneVirtual {
    int a_; int b_;
    virtual void f();
};                                                // sizeof == 16  (8-byte vptr + 8)
```

Jaise hi class mein **ek bhi `virtual`** function (ya virtual base) aati hai:
- Object ke shuru mein (usually offset 0) ek **`vptr`** add hota hai — ek hidden
  pointer, 8 bytes on 64-bit.
- Baaki members uske baad.

```
   OneVirtual object:
   offset 0:  [ vptr        ]  (8 bytes)  --> points to OneVirtual's vtable
   offset 8:  [ a_ | b_     ]  (8 bytes)
```

`examples/03_vtable_layout.cpp` yeh print karke dikhata hai (`*(uint64*)&obj` =
vptr, members offset 8+).

---

## vtable — per-class function pointer table

```
   OneVirtual's vtable  (in .rodata, ONE per class):
   [ 0 ] &OneVirtual::f
   [ 1 ] &OneVirtual::~OneVirtual   (if virtual dtor)
   ...
```

- **Ek vtable per class** (per-object nahi). 1000 `OneVirtual` objects → 1000
  vptrs, par sab **same** vtable ko point karte.
- vtable `.rodata` (read-only) mein — compiler/linker generate karta.
- Slot order = declaration order of virtual functions (base's first, then
  derived's new ones).

```cpp
struct Base    { virtual void a(); virtual void b(); virtual ~Base(); };
struct Derived : Base {
    void a() override;              // slot 0 -> &Derived::a
    // b() not overridden           // slot 1 -> &Base::b  (inherited)
    virtual void c();               // slot N -> &Derived::c  (new)
};
```

`Derived`'s vtable: `[ &Derived::a, &Base::b, &Base::~... / &Derived::~..., &Derived::c ]`.

---

## Virtual call — the exact sequence

```cpp
void call(Base* p) {
    p->a();
}
```

Compiles roughly to:

```asm
    mov  rax, [rdi]          ; rax = p->vptr        (load 1: object -> vtable)
    mov  rax, [rax + 0]      ; rax = vtable[slot_a] (load 2: vtable -> function)
    call rax                 ; indirect call, this=rdi
```

- **2 dependent loads** — `vtable` ka address object se, phir `function` ka
  address vtable se. Second can't start until first done → serialized latency.
- **1 indirect call** — target address ek register mein hai, compile-time
  constant nahi → CPU ko BTB (branch target buffer) se predict karna padta;
  data-dependent targets → mispredicts → ~15-20 cycle bubble.
- **No inlining** — compiler ko target function pata hi nahi → call ke aar-paar
  koi optimization nahi.

Compare non-virtual: `call OneVirtual::f` (direct, address in the instruction) —
often inlined away entirely.

---

## Constructor sets the vptr (step by step)

```cpp
Derived d;
// 1. Base::Base() runs   -> sets d.vptr = &Base_vtable
// 2. Derived::Derived()  -> sets d.vptr = &Derived_vtable
// (isiliye ctor mein virtual call = current level ka version -- file 02)
```

Destructor reverse: `~Derived()` runs (vptr = Derived's), then before `~Base()`,
vptr reset to Base's.

---

## `sizeof` impact

```cpp
struct Empty {};                         // 1
struct Poly  { virtual void f(); };      // 8  (just the vptr)
struct M     { int x; };                 // 4
struct PolyM { int x; virtual void f(); }; // 16 (vptr 8 + int 4 + pad 4)

static_assert(sizeof(Poly)  == 8);
static_assert(sizeof(PolyM) == 16);
```

- Adding the **first** virtual: **+8 bytes** (vptr).
- Adding **more** virtuals: **+0** (same vtable grows, not the object).
- Multiple inheritance with virtuals: **multiple vptrs** (one per polymorphic
  base — file 09).

---

## `virtual` breaks triviality & memcpy

```cpp
struct Wire { std::int64_t seq; double px; };            // trivially copyable, 16 bytes
struct WireV { std::int64_t seq; double px; virtual ~WireV(); };

static_assert(std::is_trivially_copyable_v<Wire>);
static_assert(!std::is_trivially_copyable_v<WireV>);      // virtual -> not trivial

WireV a, b;
std::memcpy(&a, &b, sizeof(WireV));   // ⚠️ UB -- copies b's vptr into a.
                                     //    a.vptr now points to... b's class vtable (same here,
                                     //    but with derived types -> a "becomes" wrong type). Corruption.
```

Isliye **wire messages, pool objects, `std::vector`-relocatable types mein
`virtual` nahi**. `virtual` = "yeh object apni identity carry karta hai" — usse
byte-copy nahi kar sakte.

---

## `final` enables devirtualization

```cpp
struct Shape { virtual double area() const = 0; };
struct Circle final : Shape { double area() const override; };   // `final` -> no further derivation

void f(Circle& c) {
    c.area();     // compiler KNOWS c is exactly Circle (final) -> direct call / inline
}
```

`final` (on class or method — file 05) tells the compiler "no override below
this" → it can devirtualize calls on statically-`Circle` references. `-flto` +
`-fdevirtualize` also help across TUs.

---

## Andar kya hota hai

- **vtable layout** is part of the **ABI** (Itanium C++ ABI on Linux/GCC/Clang) —
  vptr at offset 0, vtable entries in a fixed order, plus RTTI pointer and
  offset-to-top at negative offsets. Yeh stable hai across compilers on the same
  platform.
- vtable pointer in the object is set by `mov [this], &vtable_symbol` in each
  ctor.
- vtables live in `.data.rel.ro` / `.rodata` — shared, read-only, and (with PIE)
  relocated at load.
- The RTTI pointer (`&typeinfo`) sits just before the function-pointer array —
  `dynamic_cast` / `typeid` use it (file 11).
- **thunks:** in multiple inheritance, calling a virtual through a secondary base
  needs a `this`-adjusting thunk — a tiny stub that fixes the pointer then jumps
  (file 09).

> **HFT relevance:** vptr = +8 bytes per object (cache-line budget mein matter
> karta jab objects dense pack karne hain), aur har virtual call = 2 dependent
> loads + indirect call + no inline + BTB pressure. Hot data structures
> (`OrderBook` levels, message structs, pool objects) **zero virtual** —
> `static_assert(std::is_trivially_copyable && sizeof == N)` layout lock. Jahan
> polymorphism-jaisa chahiye: CRTP (file 13, no vptr), `std::variant` (tag +
> union, no vptr), function-pointer table, ya a plain type-tag `switch`. `final`
> + LTO se residual virtual calls devirtualize karo agar hierarchy zaroori ho.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/03_vtable_layout.cpp
g++ -std=c++20 -O2 -S -masm=intel 16-OOP/examples/03_vtable_layout.cpp -o - | less   # dekho vtable + indirect call
```

`sizeof` with/without virtual, vptr at offset 0, `ov.vptr == ov2.vptr` (shared
vtable), `Base*`→`Derived` object ka vptr = Derived's.

---

## ⚠️ Traps

### Trap 1 — `sizeof` surprise after adding `virtual`
```cpp
struct Msg { int a, b; };  // 8
struct Msg { int a, b; virtual ~Msg(); };  // 16 -- +8 vptr. static_assert catches it
```

### Trap 2 — `memcpy` / `bit_cast` a polymorphic object
```cpp
memcpy(&dst, &src, sizeof(Poly));   // ⚠️ copies vptr -> UB / type confusion
```

### Trap 3 — expecting one vtable per object
Ek vtable **per class**. Objects share it via their vptr.

### Trap 4 — virtual call cost = "just a pointer deref"
2 dependent loads + indirect call + **lost inlining** + BTB miss risk (file 12).

### Trap 5 — assuming devirtualization
Only when the compiler can prove the exact dynamic type (`final`, local of exact
type, LTO). `Base*` from a container → usually not.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "vtable object ke andar hota" | vptr object mein (8 B); vtable per-class in `.rodata` |
| "Har virtual +8 bytes" | First virtual +8 (vptr); more virtuals +0 |
| "Virtual call = 1 pointer deref" | 2 dependent loads + indirect call + no inline |
| "Polymorphic object `memcpy`-able" | No — vptr copy = UB / type confusion |
| "`virtual` triviality pe asar nahi" | Breaks `is_trivially_copyable` — no byte-copy |

---

## Exercises

1. **sizeof:** `struct A { char c; };  struct B { char c; virtual void f(); };
   struct C { char c; virtual void f(); virtual void g(); virtual void h(); };` —
   `sizeof` of each?

   <details><summary>Answer</summary>

   `A` = 1. `B` = 16 (vptr 8 + char 1 + pad 7). `C` = 16 too (3 virtuals → same
   vptr, vtable just has 3 slots; object unchanged).
   </details>

2. **Shared vtable:** `struct S { virtual void f(); };  S a, b, c;` — `*(void**)&a`,
   `*(void**)&b`, `*(void**)&c` — same? Kitni vtables total?

   <details><summary>Answer</summary>

   All three vptrs equal (same address). **One** vtable for class `S`, shared by
   all instances.
   </details>

3. **Slot inheritance:** `struct B { virtual void x(); virtual void y(); };
   struct D : B { void y() override; virtual void z(); };` — `D`'s vtable slots
   (in order, which function)?

   <details><summary>Answer</summary>

   `[0] &B::x` (inherited, not overridden), `[1] &D::y` (overridden), `[2]
   &D::z` (new). Base slots keep their positions; overrides replace the pointer;
   new virtuals appended.
   </details>

4. **memcpy UB:** `struct Base { virtual const char* who() const { return "B"; }
   };  struct Der : Base { const char* who() const override { return "D"; } };
   Base b;  Der d;  std::memcpy(&b, &d, sizeof(Base));  b.who();` — what might
   print, why is it UB?

   <details><summary>Answer</summary>

   `b`'s vptr is overwritten with `d`'s (→ `Der`'s vtable) → `b.who()` may print
   `"D"`. UB: `b` is still a `Base` object; its dynamic type was changed by raw
   bytes. With different layouts / offsets → crash or corruption.
   </details>

5. **final devirtualize:** `struct Shape { virtual double area() const = 0; };
   struct Sq final : Shape { double s_; double area() const override { return
   s_*s_; } };  double f(Sq& q) { return q.area(); }` — `-O2 -S`: direct call ya
   indirect? Bina `final`?

   <details><summary>Answer</summary>

   With `final`: compiler knows `q` is exactly `Sq` → direct call / inlined to
   `s_*s_`. Without `final`: `q` could be a further-derived class → indirect
   virtual call.
   </details>

---

## Interview questions

1. vptr aur vtable — kaunsa per-object, kaunsa per-class, kahan store?
2. Virtual call ke exact steps (loads, call)? Kyun 2 loads?
3. First virtual `sizeof` kyun +8, doosra virtual kyun +0?
4. `virtual` `is_trivially_copyable` / `memcpy` ko kyun todta?
5. Constructor vptr kaise set karta (multi-level)?
6. `final` devirtualization mein kaise help karta?

---

## Next
→ [`05-override-and-final.md`](05-override-and-final.md)
