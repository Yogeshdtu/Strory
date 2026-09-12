# 07 — Virtual destructors (kyun mandatory)

## Prerequisites
- [`02-constructors-destructors-order.md`](02-constructors-destructors-order.md), [`06-abstract-classes.md`](06-abstract-classes.md)
- Folder 15 file 06 (destructors), folder 14 files 04–06 (delete, leaks, UAF)

## Yeh topic abhi kyun
Ek chhota rule jiski galti bada nuksan karti hai: agar aap ek object ko **base
class pointer ke through `delete`** karte ho, aur base ka destructor **`virtual`
nahi** hai — to sirf base ka destructor chalta hai, derived ka **skip** ho jaata
→ derived ke members leak, aur technically **UB**. Iska measured demo `examples/
04_virtual_destructor.cpp` mein hai.

---

## The bug

```cpp
struct Base {
    virtual void process();
    ~Base() { std::cout << "~Base\n"; }        // ⚠️ NOT virtual
};

struct Derived : Base {
    std::vector<int> buffer_;                   // owns heap memory
    std::string      label_;
    ~Derived() { std::cout << "~Derived\n"; }
};

Base* p = new Derived;
delete p;                                        // ⚠️ prints only "~Base"
                                                // ~Derived() NAHI chalta -> buffer_, label_ LEAK, UB
```

`delete p` jaha `p` ka **static type `Base*`** hai:
- Base dtor `virtual` → runtime `p` ke asli type ko dekhta → `~Derived()` phir
  `~Base()` — **poora cleanup**. ✅
- Base dtor non-virtual → compiler seedha `Base::~Base()` call karta (static
  dispatch) → derived part ke destructors + `operator delete` **wrong size se**
  → members leak, heap corruption possible. ❌ UB.

`examples/04_virtual_destructor.cpp` (counted `operator new`/`delete`):

```
=== BAD: non-virtual base destructor ===
  BadDerived::process (buffer 500)
  ~BadBase()
[LEAK] 2 block(s) never freed -- BadDerived ka buffer_/label_
```

GCC warns: **`-Wdelete-non-virtual-dtor`** (part of `-Wall`).

---

## The rule

> **Agar ek class polymorphic hai (koi `virtual` method) AND uske objects
> `Base*` / `Base&` / `unique_ptr<Base>` ke through delete honge — uska
> destructor `public virtual` hona chahiye.**

```cpp
struct Base {
    virtual void process();
    virtual ~Base() = default;                  // ✅ virtual dtor
};
```

`= default` best (compiler generate karta, optimal). Bas usse `virtual` mark
karo.

---

## Alternative: `protected` non-virtual destructor

Agar aap **kabhi** `Base*` se delete nahi karna chahte (ownership hamesha
concrete type ke paas), to virtual ki jagah:

```cpp
struct Base {
    virtual void process() = 0;
protected:
    ~Base() = default;                          // non-virtual, protected -> Base* se delete BAND (compile error)
};

Base* p = new Derived;
// delete p;                                    // ❌ compile ERROR -- ~Base() protected
```

- `protected` dtor → bahar se `delete basePtr` compile hi nahi hota → bug
  impossible.
- Aur `virtual` na hone se ek vtable slot + call bacha (marginal).
- Use jab: interface jise sirf value / concrete-owned use kiya jaata (e.g.
  policy classes, mixins). Herb Sutter's guideline: **"base class destructor
  should be either public and virtual, or protected and non-virtual."**

---

## Smart pointers ke saath

```cpp
std::unique_ptr<Base> p = std::make_unique<Derived>();
// p destroy hote hi -> delete (Base*) -> needs virtual ~Base() for correct cleanup
```

`std::unique_ptr<Base>` ka default deleter `delete (Base*)` karta → **virtual
dtor zaroori**.

`std::shared_ptr<Base>` **exception hai**: woh apne control block mein **actual
type ka deleter** store karta hai (jo `make_shared` / `shared_ptr(new Derived)`
ke time capture hota) → `shared_ptr<Base>` correctly `~Derived()` call karta
**even without a virtual dtor**. Par yeh subtle hai — **fir bhi virtual dtor
likho** (documentation + `unique_ptr` / raw delete safe).

```cpp
std::shared_ptr<Base> sp = std::make_shared<Derived>();   // ~Derived() chalega (type-erased deleter)
std::unique_ptr<Base> up = std::make_unique<Derived>();   // ~Base() only agar non-virtual -> BUG
```

---

## Cost of a virtual destructor

- **+0 to `sizeof`** agar class mein already koi `virtual` hai (vtable already
  exists, dtor bas ek aur slot).
- **First virtual** (agar dtor hi pehla virtual) → **+8 bytes** (vptr) +
  `is_trivially_*` false (file 04). Isliye POD/wire/pool types mein `virtual`
  dtor bhi avoid — un types ka polymorphic base hona hi nahi chahiye.
- Destruction ke waqt: ek indirect call (vtable ke through) instead of direct —
  negligible vs the actual cleanup work.

---

## Andar kya hota hai

- Virtual dtor → vtable mein ek (ya do — "complete object destructor" aur
  "deleting destructor") slots. `delete p` → deleting destructor call jo
  `~Derived()` → `~Base()` chain karta phir **sahi size** se `operator delete`.
- Non-virtual dtor + `delete basePtr` → compiler `Base::~Base()` + `operator
  delete(p, sizeof(Base))` emit karta — derived part untouched, aur sized-delete
  ko galat size milta.
- `-Wdelete-non-virtual-dtor` — static check at the `delete` site (class has
  virtuals + non-virtual dtor).
- `protected` non-virtual dtor → `delete basePtr` ka access check fail → compile
  error (no codegen).

> **HFT relevance:** yeh less about latency, more about **correctness under
> load** — a leak per destroyed polymorphic object slowly eats RSS (folder 14),
> and the UB can corrupt the heap. Rule enforced in review + `-Werror=delete-non-virtual-dtor`.
> For hot types the real fix is upstream: **hot objects aren't polymorphic** (no
> base, no virtual, trivially destructible, pool-managed). Where a small
> polymorphic hierarchy exists (cold: adapters, config), `public virtual ~Base()
> = default;`. Where a base is a pure mixin never deleted polymorphically,
> `protected ~Base()`.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/04_virtual_destructor.cpp
```

`BadBase` (non-virtual dtor) vs `GoodBase` (virtual). Counted `operator new`/
`delete` → BAD leaks 2 blocks (`buffer_` + `label_`), GOOD frees all. 1
intentional `-Wdelete-non-virtual-dtor`. Linux pe ASan: `new-delete-type-mismatch`
/ leak.

---

## ⚠️ Traps

### Trap 1 — polymorphic base, non-virtual dtor, `delete basePtr`
```cpp
struct B { virtual void f(); ~B(); };  B* p = new D;  delete p;   // ⚠️ ~D skipped -> leak/UB
```

### Trap 2 — `unique_ptr<Base>` without virtual dtor
```cpp
std::unique_ptr<Base> p = std::make_unique<Derived>();   // ⚠️ ~Base() only. virtual ~Base()
```

### Trap 3 — `shared_ptr` "works" → assume it's always fine
`shared_ptr` type-erases the deleter (safe), but `unique_ptr` / raw delete
don't. Write the virtual dtor anyway.

### Trap 4 — virtual dtor on a wire/pool type
```cpp
struct Msg { int a, b; virtual ~Msg(); };   // ⚠️ +8 vptr, not trivially copyable -- no polymorphism needed here
```

### Trap 5 — `protected` dtor but you DO need polymorphic delete
```cpp
struct I { virtual void f() = 0; protected: ~I(); };
std::unique_ptr<I> p = ...;   // ❌ can't delete through I*. Make it `public virtual ~I() = default;`
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Non-virtual dtor + `delete basePtr` = just a leak" | Leak **and** UB (wrong-size delete, skipped cleanup) |
| "`shared_ptr` needs virtual dtor too" | It type-erases the deleter — safe; but write it anyway |
| "Every base class needs `virtual ~`" | Only if deleted polymorphically. Else `protected` non-virtual |
| "Virtual dtor always costs +8 bytes" | Only if it's the first virtual; else +0 |
| "The compiler catches all such bugs" | `-Wdelete-non-virtual-dtor` catches many, not all paths |

---

## Exercises

1. **Predict + count:** `struct B { virtual void run(); ~B(){puts("~B");} };
   struct D : B { std::string s_{"looooooong string not in SSO"}; ~D(){puts("~D");}
   };  B* p = new D;  p->run();  delete p;` — output? What leaks?

   <details><summary>Answer</summary>

   Prints `~B` only. `~D` skipped → `D::s_`'s heap buffer leaks. UB
   (`-Wdelete-non-virtual-dtor`). Fix: `virtual ~B()`.
   </details>

2. **Fix 3 ways:** given the buggy `B`/`D` above, show the fix (a) virtual dtor,
   (b) protected non-virtual dtor + delete via `D*`, (c) `std::shared_ptr`.

   <details><summary>Answer</summary>

   (a) `virtual ~B() = default;`. (b) `protected: ~B() = default;` and delete via
   `D*` / `unique_ptr<D>`. (c) `std::shared_ptr<B> p = std::make_shared<D>();` —
   type-erased deleter runs `~D` even with non-virtual `~B` (still add virtual).
   </details>

3. **sizeof cost:** `struct A { int x, y; };` vs `struct A2 { int x, y; virtual
   ~A2(); };` vs `struct A3 { int x, y; virtual void f(); virtual ~A3(); };` —
   `sizeof` of each, `is_trivially_copyable`?

   <details><summary>Answer</summary>

   `A` = 8, trivially copyable. `A2` = 16 (first virtual → vptr), not trivially
   copyable. `A3` = 16 too (dtor is not the first virtual → no extra), not
   trivially copyable.
   </details>

4. **shared_ptr magic:** `struct Widget { ~Widget(){puts("~Widget");} };  struct
   Button : Widget { ~Button(){puts("~Button");} };  std::shared_ptr<Widget> p =
   std::make_shared<Button>();  p.reset();` — output? Why, despite non-virtual
   `~Widget`?

   <details><summary>Answer</summary>

   `~Button` then `~Widget`. `make_shared<Button>` captured a deleter that knows
   the object is a `Button` → calls `~Button` correctly. (A `unique_ptr<Widget>`
   here would print only `~Widget`.)
   </details>

5. **protected dtor design:** ek `struct ComparablePolicy { virtual bool less(...)
   const = 0; };` jo sirf composition/value use hota hai, kabhi `ComparablePolicy*`
   se delete nahi hota. `protected ~` ya `public virtual ~`? Kyun?

   <details><summary>Answer</summary>

   `protected: ~ComparablePolicy() = default;` (non-virtual) — makes accidental
   `delete policyPtr` a compile error, saves a vtable slot / indirect dtor call.
   Only use `public virtual ~` if you actually own instances through base
   pointers.
   </details>

---

## Interview questions

1. Non-virtual base dtor + `delete basePtr` — exactly kya hota (leak? UB?)?
2. "Public and virtual, OR protected and non-virtual" — yeh guideline kya kehta?
3. `unique_ptr<Base>` vs `shared_ptr<Base>` — virtual dtor requirement?
4. Virtual dtor ka `sizeof` cost — kab +8, kab +0?
5. `-Wdelete-non-virtual-dtor` kab fire karta?
6. Wire / pool type ka polymorphic base kyun nahi hona chahiye?

---

## Next
→ [`08-object-slicing.md`](08-object-slicing.md)
