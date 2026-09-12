# 02 — Construction & destruction order

## Prerequisites
- [`01-inheritance-basics.md`](01-inheritance-basics.md)
- Folder 15 files 04–06 (constructors, init lists, destructors)

## Yeh topic abhi kyun
Ek `Derived` object banne mein **kai constructors** chalte hain, ek fixed order
mein: base(s) → members → derived body. Destruction bilkul reverse. Yeh order
janna zaroori hai — kyunki base ctor ke dauraan derived abhi "poora nahi bana"
hota (virtual calls ka classic trap), aur destruction mein ulta.

---

## Construction order

```cpp
struct A { A() { std::cout << "A "; } };
struct B { B() { std::cout << "B "; } };
struct M { M() { std::cout << "M "; } };

struct Derived : A, B {
    M m1_;
    M m2_;
    Derived() { std::cout << "D "; }
};

Derived d;    // prints:  A B M M D
```

**Order:**
1. **Base classes** — declaration order (`: A, B` → `A` then `B`), **not**
   init-list order.
2. **Non-static data members** — declaration order (`m1_` then `m2_`), not
   init-list order (folder 15 file 05).
3. **Derived constructor body**.

(Virtual bases sabse pehle — file 10.)

```cpp
struct Derived : A, B {
    M m1_, m2_;
    Derived() : B(), m2_(), A(), m1_() { }   // ⚠️ likha order ignore -> A B M M (decl order). -Wreorder
};
```

---

## Destruction order — exact reverse

```cpp
struct A { ~A() { std::cout << "~A "; } };
struct B { ~B() { std::cout << "~B "; } };
struct M { ~M() { std::cout << "~M "; } };

struct Derived : A, B {
    M m1_, m2_;
    ~Derived() { std::cout << "~D "; }
};

{ Derived d; }    // prints:  ~D ~M ~M ~B ~A
```

**Order:**
1. **Derived destructor body**.
2. **Members** — reverse declaration order (`m2_` then `m1_`).
3. **Base classes** — reverse declaration order (`B` then `A`).

Reverse isliye ki har cheez apne "pehle bane" cheezon par depend kar sakti hai.

---

## Base ke pass args — init list

```cpp
struct Vehicle {
    std::string plate_;
    explicit Vehicle(std::string p) : plate_(std::move(p)) {}
};

struct Car : Vehicle {
    int doors_;
    Car(std::string plate, int doors)
        : Vehicle(std::move(plate)),      // ✅ base ctor -- init list mein, PEHLE
          doors_(doors) {
    }
};
```

- Base ctor call **init list mein** (member init list ke saath), aur woh
  **members se pehle** chalta hai (chahe aap likho baad mein).
- Agar base ka **default ctor hai** aur aap use init list mein na do → compiler
  automatically default ctor call karta.
- Agar base ka **koi default ctor nahi** aur aap explicitly na do → **compile
  error**.

```cpp
struct Base { explicit Base(int); };        // no default ctor
struct Derived : Base {
    Derived() { }                            // ❌ ERROR -- Base(int) chahiye
    Derived() : Base(0) { }                  // ✅
};
```

---

## ⚠️ Virtual calls in constructors/destructors

**Ctor/dtor ke andar `virtual` call karo → derived ka override NAHI chalta.**

```cpp
struct Base {
    Base()          { init(); }              // ⚠️ Base::init chalega, Derived::init NAHI
    virtual ~Base() { cleanup(); }           // ⚠️ Base::cleanup chalega
    virtual void init()    { std::cout << "Base::init\n"; }
    virtual void cleanup() { std::cout << "Base::cleanup\n"; }
};

struct Derived : Base {
    std::vector<int> data_;
    void init()    override { std::cout << "Derived::init (data_ size " << data_.size() << ")\n"; }
    void cleanup() override { std::cout << "Derived::cleanup\n"; }
};

Derived d;    // Base() runs init() -> "Base::init"  (NOT Derived::init -- d.data_ abhi nahi bana!)
```

**Kyun:** jab `Base()` chal raha hai, `Derived` part abhi construct nahi hua
(`data_` uninitialized). Agar `Derived::init` chal jaata to woh
uninitialized `data_` touch karta → disaster. Isliye standard rule: ctor/dtor
ke dauraan object ka dynamic type **current class hi hota hai** — vptr
step-by-step set hota hai (Base ctor → vptr = Base's vtable; Derived ctor → vptr
= Derived's vtable).

**Fix:** ctor mein virtual dispatch mat expect karo. Options:
- Two-phase init: ctor + ek separate `initialize()` jo caller explicitly call kare.
- Factory function jo object bana ke phir setup kare.
- Pass the needed data as ctor arguments.

---

## Exception during construction

```cpp
struct Derived : Base {
    Resource r_;
    Derived() : Base(), r_() {
        if (bad) throw std::runtime_error("x");    // r_, Base already constructed
    }
};
```

Agar `Derived` ctor body (ya member/base ctor) throw kare:
- Jo sub-objects **fully constructed** ho chuke (Base, members before the
  throwing one) — unke **destructors chalte hain** (reverse order).
- `Derived` object "kabhi existed nahi" — uska `~Derived()` **nahi** chalta.
- Memory (`new Derived` case mein) release ho jaati.

Ek base/member ctor throw kare to jo uske pehle bane, unke dtors — aur exception
propagate.

---

## Andar kya hota hai

- Compiler `Derived` ka ctor generate karta as: **[set vptr to Base's] → call
  Base ctor(s) in decl order → [set vptr to this class's] → init members in decl
  order → run body**.
- vptr **har level pe update** hota hai — isliye ctor mein virtual call current
  level ka version deta.
- Destructor: **run body → destroy members (reverse) → [reset vptr to Base's] →
  call Base dtor(s) (reverse)**.
- Non-virtual, trivial bases/members → yeh sab **zero code** (POD-like). Cost
  sirf non-trivial ctors/dtors se.
- Multiple inheritance → non-primary base ke ctor call se pehle `this` pointer
  adjust hota hai (file 09).

> **HFT relevance:** hot-path objects ki construction chain chhoti + trivial
> rakhi jaati (pool se nikaalna → placement new + minimal ctor). Deep hierarchy
> = kai ctor/dtor calls per object (aur agar non-trivial → real cost). Virtual-
> call-in-ctor bug production mein subtle — "config load nahi hua" type. Rule:
> ctors mein sirf member init, koi virtual dispatch, koi heavy work. Complex
> setup → explicit `init()` / factory, jise hot loop ke bahar call karo.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/01_inheritance.cpp
```

`Vehicle(plate)` → `Car(doors)` ctor order (prints), phir reverse dtor. Aur
khud: A, B bases + 2 M members wali `Derived` banao, `A B M M D` / `~D ~M ~M ~B
~A` verify karo. Phir Base ctor se `virtual` call karke dekho konsa version
chalta.

---

## ⚠️ Traps

### Trap 1 — virtual call in ctor/dtor
```cpp
Base() { setup(); }   // ⚠️ agar setup() virtual -> Base::setup, Derived::setup NAHI
```

### Trap 2 — base ctor init-list mein bhoolna (no default ctor)
```cpp
struct D : Base { D() {} };   // ❌ agar Base ka default ctor nahi. D() : Base(args) {}
```

### Trap 3 — init-list order = execution order samajhna
```cpp
D() : memberB_(), baseA_() {}   // ⚠️ baseA_ pehle (base), phir members (decl order). -Wreorder
```

### Trap 4 — `~Derived()` expect karna jab ctor throw ho
```cpp
D() { throw ...; }   // ~D() NAHI chalta -- object incomplete. Constructed members ke dtors chalte
```

### Trap 5 — base dtor non-virtual, `Base*` se delete (file 07)
```cpp
Base* p = new Derived;  delete p;   // ⚠️ ~Derived() skip agar ~Base non-virtual
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Derived ctor pehle chalta" | Base(s) → members → Derived body. Dtor reverse |
| "Init-list ka order execution order" | Base decl order, then member decl order |
| "Ctor mein virtual call → Derived override" | Current class ka version — Derived part abhi nahi bana |
| "Ctor throw → `~Derived()` chalta" | Nahi — sirf constructed sub-objects ke dtors |
| "Base ka default ctor na ho to bhi chalega" | Compile error — explicitly `: Base(args)` |

---

## Exercises

1. **Order:** `struct A{A(){c('A');}~A(){c('a');}}; struct B{B(){c('B');}~B(){c('b');}};
   struct M{M(){c('M');}~M(){c('m');}}; struct D:A,B{M m; D(){c('D');}~D(){c('d');}};`
   — `{ D d; }` ka full output?

   <details><summary>Answer</summary>

   `A B M D d m b a` — construct: A, B (bases decl order), M (member), D body.
   Destruct: D body, M, B, A (reverse).
   </details>

2. **Virtual in ctor:** `struct Base { Base() { whoami(); } virtual void
   whoami() { puts("Base"); } }; struct Derived : Base { void whoami() override
   { puts("Derived"); } }; Derived d;` — output? Kyun?

   <details><summary>Answer</summary>

   `Base` — during `Base()`, dynamic type is `Base` (vptr = Base's vtable,
   Derived part not yet built). `Derived::whoami` would touch un-built state.
   </details>

3. **No default base ctor:** `struct Engine { explicit Engine(int hp); };  struct
   Car : Engine { int doors_; Car(int doors) : doors_(doors) {} };` — compile
   error kahan, fix?

   <details><summary>Answer</summary>

   `Car(int)` doesn't initialize the `Engine` base and `Engine` has no default
   ctor → error. Fix: `Car(int hp, int doors) : Engine(hp), doors_(doors) {}`.
   </details>

4. **Ctor throw:** `struct Log { Log(){puts("L");} ~Log(){puts("~L");} };  struct
   Base { Base(){puts("B");} ~Base(){puts("~B");} };  struct D : Base { Log l_;
   D() : Base(), l_() { throw 1; } ~D(){puts("~D");} };  try { D d; } catch(...) {}`
   — output?

   <details><summary>Answer</summary>

   `B L ~L ~B` — Base ctor, Log ctor, then `throw` → Log dtor, Base dtor (reverse,
   constructed sub-objects). `~D` does NOT run (D never completed).
   </details>

5. **Fix virtual-in-ctor:** ek `Widget` base jo ctor mein `render()` call karta
   hai (jo derived override karte hain). Redesign taaki derived ka `render`
   chale.

   <details><summary>Answer</summary>

   Remove the call from the ctor. Add `void initialize() { render(); }` (public),
   or a factory `static std::unique_ptr<Widget> create<T>() { auto w =
   std::make_unique<T>(); w->render(); return w; }`. Caller/factory triggers
   `render()` after the object is fully built.
   </details>

---

## Interview questions

1. `Derived` object construction ka order (bases, members, body)?
2. Destruction order — kyun reverse?
3. Ctor/dtor ke andar virtual call — kya hota, kyun?
4. Base ka default ctor na ho to Derived ctor mein kya karna?
5. Derived ctor throw kare — kaunse destructors chalte, kaunsa nahi?
6. vptr construction ke dauraan kaise set hota (step by step)?

---

## Next
→ [`03-virtual-functions.md`](03-virtual-functions.md)
