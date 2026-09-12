# 04 — Layer 3: classes, inheritance, vtables, virtual destructors

## Prerequisites
Folders `15-CLASSES`, `16-OOP`, `25-OBJECT-MODEL`. Yeh Layer almost har
C++ interview mein aata hai — vtables aur virtual dtors especially.

---

## A — Class fundamentals

### A1. `struct` aur `class` mein fark?
<details><summary>Answer</summary>
Sirf **default access**: `struct` members/base public by default,
`class` private. Aur kuch nahi. Convention: `struct` for
plain-data/aggregates, `class` for types with invariants/encapsulation.
(`15-CLASSES/01`.)
</details>

### A2. Constructor initializer list vs body assignment — fark aur kab
zaroori?
<details><summary>Answer</summary>
Init list members ko **directly construct** karta; body assignment pehle
default-construct karta phir assign (do steps, waste). **Zaroori** for:
`const` members, reference members, members without a default ctor, base
class args. Trap: members **declaration order** mein init hote hain, init
list ke order mein nahi — `-Wreorder`. (`15/06`.)
</details>

### A3. `const` member function kya guarantee karta?
<details><summary>Answer</summary>
`this` `const T*` ban jaata — function object ke non-`mutable` members ko
modify nahi kar sakta, aur sirf doosre `const` members call kar sakta.
`const` object pe sirf `const` methods call ho sakte. `mutable` members
(caches, mutexes) `const` method mein bhi modifiable. Const-correctness
chain — ek `const` reference sirf `const` API dekhti. (`15/09`.)
</details>

### A4. `explicit` constructor kyun?
<details><summary>Answer</summary>
Single-argument (ya effectively single) constructor implicit conversion
allow karta — `void f(Widget); f(42);` chal jaata agar `Widget(int)`
non-explicit. `explicit` isse rokta — sirf direct-init (`Widget w(42)` /
`Widget w{42}`). Bugs aur surprising overloads avoid. Modern guidance:
default `explicit` for single-arg ctors unless conversion genuinely
wanted. `explicit` conversion operators bhi. (`15/04`, `25/05`.)
</details>

### A5. Aggregate kya hai? Kab `{}` aggregate-init karta?
<details><summary>Answer</summary>
Aggregate = class with no user-provided/`explicit`/inherited ctors, no
private/protected non-static data, no virtual functions, no virtual/
private/protected bases. `T t{1, 2, 3}` members ko positionally init
karta (C++20 se designated: `T t{.x=1, .y=2}`). Ek user-declared ctor add
karte hi aggregate nahi raha. (`11-STRUCTS`, `15`.)
</details>

---

## B — Inheritance & polymorphism

### B1. `virtual` function kaise kaam karta — vtable mechanism.
<details><summary>Answer</summary>
Polymorphic class ke har object mein ek hidden **vptr** (usually first
member) hota jo class ki **vtable** ko point karta — function pointers ka
array, ek per virtual function. `p->f()` → load vptr → load vtable[f's
slot] → indirect call. Cost: ~1 extra load + an indirect (unpredictable)
call — ~2–20 ns depending on prediction (`16/measured`, `21`). Devirtualization
kabhi isse hata deta (final/exact-type/LTO). (`25-OBJECT-MODEL`, `16-OOP`.)
</details>

### B2. Virtual destructor kyun zaroori — exactly kya galat hota bina?
<details><summary>Answer</summary>
`Base* p = new Derived; delete p;` — agar `~Base` non-virtual, to sirf
`~Base` chalta, `~Derived` **nahi** → Derived ke members leak / UB (formally
UB "deleting through a base pointer without a virtual destructor").
Virtual dtor se `delete p` vtable dekh ke `~Derived` → `~Base` chain
chalata. Rule: **agar class polymorphically delete hoti hai, virtual
dtor**. Value-type / never-base classes ko nahi chahiye (`16/`,
`45/12` E4).
</details>

### B3. Constructor / destructor ke andar virtual call — kya hota?
<details><summary>Answer</summary>
`Base` ctor ke andar `virtual f()` call → **`Base::f`** chalega, Derived
ka nahi — kyunki Derived part abhi construct nahi hua, vptr abhi `Base`
ki vtable pe set hai. Dtor mein bhi ulta (Derived part destruct ho chuka).
Isliye ctor/dtor se virtual dispatch pe rely mat karo. (`25/02`, `16`.)
</details>

### B4. Object slicing kya hai?
<details><summary>Answer</summary>
`Base b = derived;` — sirf `Base` sub-object copy hota, Derived ke
members aur vptr chhoot jaate. `std::vector<Base>` mein `Derived` push
karo → sliced, virtual dispatch "kaam nahi karta". Fix: polymorphic
types ko **reference/pointer** se handle karo (`vector<unique_ptr<Base>>`),
by value nahi. `Base(const Base&) = delete` accidental slicing ko compile
error banata. (`16`, `45/12` E7.)
</details>

### B5. Pure virtual function aur abstract class?
<details><summary>Answer</summary>
`virtual void f() = 0;` — no (required) implementation, class **abstract**
ho jaati (instantiate nahi kar sakte). Derived ko override karna padta
warna woh bhi abstract. Pure virtual ka body **de** bhi sakte ho (`void
Base::f(){}` — explicit call `Base::f()` se). Interface = all-pure-virtual
+ virtual dtor. (`16-OOP`.)
</details>

### B6. `override` aur `final` keywords ka faayda?
<details><summary>Answer</summary>
`override` — compiler check karta ki tum sach mein ek base virtual ko
override kar rahe (signature typo / missing `const` / non-virtual base →
compile error, warna silent new function). `final` — is virtual ko aage
override nahi kar sakte (ya poori class inherit nahi kar sakte). `final`
devirtualization enable karta (exact type known). Hamesha `override`
likho. (`16/`.)
</details>

### B7. Diamond inheritance aur `virtual` base?
<details><summary>Answer</summary>
`B : A`, `C : A`, `D : B, C` — `D` mein `A` **do baar** (ambiguity,
double state). `B : virtual A`, `C : virtual A` → `D` mein `A` ka ek hi
shared sub-object. Cost: virtual base access ek extra indirection
(offset vtable se). HFT/perf code mein multiple inheritance generally
avoid; composition prefer. (`16/`.)
</details>

### B8. `dynamic_cast` kaise kaam karta, cost kya?
<details><summary>Answer</summary>
Runtime type check using RTTI (vtable ke paas type_info). `dynamic_cast<Derived*>(base_ptr)`
success pe adjusted pointer, fail pe `nullptr` (reference version →
`std::bad_cast`). Cost: type hierarchy walk, ~tens of ns, non-trivial.
Hot path pe avoid — design se pata hona chahiye type (virtual function,
`std::variant`, tag). `-fno-rtti` isse disable karta. (`25`, `16`.)
</details>

---

## C — Special member functions

### C1. Rule of Zero / Three / Five — batao.
<details><summary>Answer</summary>
**Rule of Zero:** aise members use karo jo apne resources khud manage
karte (`std::vector`, `unique_ptr`) → koi special member likhna hi mat
padta, compiler-generated theek. **Rule of Three (pre-C++11):** agar
destructor, copy ctor, ya copy assignment mein se ek chahiye → teeno
chahiye (raw resource ownership). **Rule of Five:** + move ctor + move
assignment. Aaj: **Rule of Zero pehle**; agar raw resource, to Five (ya
`= delete`). (`17-RAII`, `18-COPY-MOVE`.)
</details>

### C2. Compiler kab move constructor generate karta?
<details><summary>Answer</summary>
Jab tumne **koi bhi** destructor, copy ctor, copy assignment, ya move
assignment declare **nahi** kiya (aur move ctor deleted nahi). Ek
user-declared dtor move ctor ko suppress kar deta (copy still generated,
deprecated). Isliye Rule of Five: ek likha to sab explicitly manage karo
(`= default` / `= delete`). (`18-COPY-MOVE`.)
</details>

### C3. Copy-and-swap idiom — kya aur kyun?
<details><summary>Answer</summary>
`T& operator=(T other) { swap(*this, other); return *this; }` — parameter
by value (copy/move construct), phir member-wise swap. Faayda: (1) copy
aur move assignment dono ek function se, (2) **strong exception safety**
(copy pehle hota, swap `noexcept`), (3) self-assignment safe. Cost: ek
extra move usually. (`18-COPY-MOVE`.)
</details>

### C4. `= default` aur `= delete` — use cases.
<details><summary>Answer</summary>
`= default` — compiler-generated version chahiye but tumne dusre special
members declare kiye (so it wasn't implicit), ya visibility/`noexcept`
control. `= delete` — is operation ko forbid karo (non-copyable type:
`T(const T&) = delete;`), ya ek overload ko explicitly ban karo
(`f(int)`, `f(double) = delete;`). (`15/04`, `17`.)
</details>

### C5. Non-copyable, movable type kaise banao?
<details><summary>Answer</summary>
```cpp
T(const T&) = delete;
T& operator=(const T&) = delete;
T(T&&) noexcept = default;          // ya custom
T& operator=(T&&) noexcept = default;
```
(Aur ek dtor agar resource hai.) `unique_ptr`, threads, locks yeh shape
follow karte. (`18-COPY-MOVE`, `17-RAII`.)
</details>

---

## D — Layout & the object model

### D1. `sizeof` an empty class — kyun 1, kabhi 0?
<details><summary>Answer</summary>
Empty class ka `sizeof` **1** (distinct address guarantee — do objects ka
alag pata). **Empty Base Optimization (EBO):** empty base as a subobject 0
bytes le sakta. `[[no_unique_address]]` (C++20) empty members ke liye same.
Stateless functors / allocators isse free rehte. (`25/08`.)
</details>

### D2. Struct padding — `struct { char a; int b; char c; }` ka sizeof?
<details><summary>Answer</summary>
Usually **12** (x86-64): `a` at 0, 3 bytes pad, `b` at 4, `c` at 8, 3
bytes trailing pad (array-ability ke liye alignment 4). Reorder
`{ int b; char a; char c; }` → **8**. HFT: hot structs mein fields ko
size-descending order karo, aur hot/cold split (`43/07`). `#pragma pack`
padding hata deta par misaligned access slow / on some archs faults.
(`11-STRUCTS`, `25/08`.)
</details>

### D3. Standard-layout aur trivial type — kya matlab, kyun matter?
<details><summary>Answer</summary>
**Trivial:** trivial default ctor + trivial copy/move + trivial dtor —
`memcpy` se copy legal, no init needed. **Standard-layout:** C-compatible
layout (no virtuals, single access control for non-static data, ...) —
`offsetof` legal, C se interop. **POD** = dono. Wire parsing / `memcpy`
of market data structs ke liye yeh chahiye (`38`, `43`, `25/`).
</details>

### D4. `this` pointer ka type kya hai, aur `const` method mein?
<details><summary>Answer</summary>
`X::f()` mein `this` = `X* const` (const pointer). `X::f() const` mein
`this` = `const X* const`. `X::f() &&` mein rvalue-ref-qualified. `this`
implicit first argument hai — vtable dispatch ke liye bhi yahi object
locate karta. (`15/`, `25/`.)
</details>

---

## Interview tips for Layer 3

- Virtual destructor: sirf "rule" mat bolo — "base pointer se delete,
  bina virtual dtor → derived dtor skip → leak/UB" bolo.
- vtable: "vptr per object, vtable per class, `p->f()` = load vptr → load
  slot → indirect call, ~cost of a mispredicted branch" — yeh ek strong
  answer hai.
- HFT angle: "virtual dispatch hot path pe measured ~2–20 ns; hum CRTP /
  `std::variant` / tag-dispatch / templates se compile-time bind karte
  jab type set of possibilities chhota hai" (`21`, `36`).
- Rule of Zero pehle bolo — Five sirf raw-resource case ke liye.

## Next
→ [`05-stl-questions.md`](05-stl-questions.md)
