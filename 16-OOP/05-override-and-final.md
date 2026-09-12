# 05 — `override` and `final`

## Prerequisites
- [`03-virtual-functions.md`](03-virtual-functions.md), [`04-vtable-deep-dive.md`](04-vtable-deep-dive.md)

## Yeh topic abhi kyun
C++11 ke do chhote keywords jo virtual functions ke saath kaam karna **safe**
banate hain: `override` (compiler-checked "yeh base ke virtual ko replace karta
hai") aur `final` ("isse aage override / derive nahi hoga"). `override` ke bina
signature ki chhoti galti ek silent "naya virtual" ban jaati hai — sabse common
inheritance bug.

---

## `override` — compiler ko check karne do

```cpp
struct Base {
    virtual void handle(const Event& e);
    virtual int  score() const;
    virtual ~Base() = default;
};

struct Sub : Base {
    void handle(const Event& e) override;      // ✅ signature matches -> replaces Base's slot
    int  score() const override;               // ✅
};
```

`override` **kuch semantics add nahi karta** — woh already virtual hai. Woh
compiler se bolta hai: **"agar yeh kisi base virtual ko override NAHI kar raha,
to ERROR do."**

### `override` ke bina — silent bug

```cpp
struct Base {
    virtual void handle(const Event& e);       // by const ref
};

struct Sub : Base {
    void handle(Event e);                       // ⚠️ by VALUE -- alag signature!
};
```

- `Sub::handle(Event)` `Base::handle(const Event&)` ko **override nahi** karta
  (signature mismatch) — yeh ek **naya, alag virtual function** hai jo `Base` ka
  wala **hide** karta hai.
- `basePtr->handle(e)` → **`Base::handle` chalega** (`Sub::handle` slot alag
  hai). Aapko laga override kiya, par nahi hua.
- **`override` lagane pe** → compile error: `'void Sub::handle(Event)' marked
  'override', but does not override`. Bug turant pakda gaya.

### Kya-kya match hona chahiye

```cpp
struct Base { virtual void f(int) const noexcept; };

struct Sub : Base {
    void f(int) const noexcept override;   // ✅ exact match
    // void f(long) const override;        // ❌ param type
    // void f(int) override;               // ❌ missing const
    // void f(int) const override;         // ❌ missing noexcept (C++17: part of type)
};
```

- Function **name**, **parameter types** (exactly, after decay), **`const`**,
  **ref-qualifier** (`&`/`&&`), **`noexcept`** (C++17+).
- **Return type** — must be same OR **covariant** (`Base*`→`Derived*`,
  `Base&`→`Derived&`).

**Rule: har override pe `override` likho.** `-Wsuggest-override` (GCC/Clang,
not in `-Wall`) isse enforce karta hai — hot codebases ise `-Werror` ke saath
on karte hain.

---

## `final` — no further override / derivation

### On a virtual function

```cpp
struct Base   { virtual void f(); };
struct Middle : Base { void f() final; };       // Middle::f ko koi aur override nahi kar sakta
struct Leaf   : Middle {
    // void f() override;                        // ❌ ERROR -- Base::f is final in Middle
};
```

### On a class

```cpp
struct Widget final : Base {                     // Widget se koi class derive nahi kar sakti
    void render() override;
};
// struct SubWidget : Widget { };                // ❌ ERROR -- Widget is final
```

**Kyun use karein:**
1. **Design intent** — "yeh class/method extend karne ke liye nahi hai".
2. **Devirtualization** (file 04) — `final` compiler ko batata "no override
   below" → statically-known-type calls direct/inline ho jaate:
   ```cpp
   void draw(Widget& w) { w.render(); }   // Widget final -> compiler devirtualizes -> direct call
   ```

`final` on the whole class → all its virtual methods effectively final too.

---

## `override` + `final` together

```cpp
struct Sub : Base {
    void handle(const Event& e) override final;   // overrides Base's, and seals it
};
```

Valid — "yeh base ka override hai (checked), aur isse aage koi override nahi".

---

## `final` ke trade-offs

| Fayda | Nuksan |
|---|---|
| Devirtualization → faster calls on that static type | Extensibility khatam — testing mein mock/derive nahi kar sakte |
| Clear "sealed" design intent | Future requirements block ho sakte |
| Prevents accidental deep hierarchies | Libraries mein aggressive `final` users ko frustrate karta |

**Guideline:** `final` on **leaf classes** jo genuinely extend karne ke liye
nahi (aur jinke virtual calls hot hain) — reasonable. `final` on **library base
classes** — usually avoid (kisi ko extend karna pad sakta). Internal /
application code mein `final` liberally; public library API mein conservatively.

---

## Andar kya hota hai

- `override` — **pure compile-time check**, zero runtime effect. Generated code
  identical to not writing it.
- `final` on a method — compiler records "no override below" → devirtualizes
  calls where the static type is that class or below. On a class → same for all
  its virtuals, plus blocks derivation.
- Without `final`, the compiler must assume any `Base&` could be a
  yet-unknown-derived type → keeps calls virtual (unless it proves the exact
  type another way, e.g. a local of exact type, LTO).
- `-Wsuggest-override` — static lint, no codegen change.

> **HFT relevance:** `override` everywhere is a hard rule — the silent
> "signature mismatch → new virtual" bug is exactly the kind that passes tests
> and breaks in production (wrong handler runs). `final` on concrete leaf types
> in the hot path lets the compiler devirtualize + inline `render()`/`process()`
> calls when the static type is known — a real speedup where a small class
> hierarchy is unavoidable. Combined with `-flto -fdevirtualize-at-ltrans`,
> `final` can turn a virtual hierarchy's hot calls into direct calls. Public
> extension points stay non-`final`.

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/02_virtual_functions.cpp
```

`Circle`/`Rect`/`Square` use `override`. Try: ek override ka `const` hatao →
compile error. `Square` ko `final` banao, `-O2 -S` mein `Square&`-typed calls
direct dekho.

---

## ⚠️ Traps

### Trap 1 — `override` na likhna (silent new virtual)
```cpp
struct D : B { void f(long); };   // ⚠️ B::f(int) ko override nahi -- naya virtual. `override` -> error
```

### Trap 2 — `virtual` + `override` dono base mein
```cpp
struct B { virtual void f() override; };   // ❌ B mein kuch override nahi karne ko. Base mein sirf `virtual`
```

### Trap 3 — `final` on a library base class
```cpp
struct PluginBase final { virtual void run() = 0; };   // ⚠️ koi plugin derive nahi kar sakta. Purpose defeated
```

### Trap 4 — `override` "adds virtual"
`override` sirf checks. Agar base mein `virtual` nahi to `override` → error, but
it doesn't make it virtual.

### Trap 5 — `final` breaking a test double
```cpp
class OrderRouter final { virtual void send(...); };   // ⚠️ ab test mein MockRouter derive nahi hota
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`override` virtual banata / behaviour change karta" | Pure compile-time check; zero codegen change |
| "`override` optional, style choice" | Prevents the #1 inheritance bug (silent signature mismatch) |
| "`final` bas documentation" | Enables devirtualization + blocks derivation (real effects) |
| "`final` har class pe accha" | Kills extensibility — leaf/hot types only, not library bases |
| "return type bhi exactly match" | Same OR covariant (`Base*`→`Derived*`) |

---

## Exercises

1. **Catch the bug:** `struct Base { virtual void tick(std::uint64_t ns); };
   struct Impl : Base { void tick(std::int64_t ns) { /* ... */ } };` — add
   `override` to `Impl::tick`. What error? Why is this a real bug without
   `override`?

   <details><summary>Answer</summary>

   Error: `tick(std::int64_t)` doesn't override `tick(std::uint64_t)` (different
   param type). Without `override`: a new virtual → `basePtr->tick(x)` calls
   `Base::tick` (often empty/default) → `Impl::tick` never runs.
   </details>

2. **Covariant return:** `struct Animal { virtual Animal* clone() const; };
   struct Dog : Animal { Dog* clone() const override; };` — legal? `Cat* clone()`?

   <details><summary>Answer</summary>

   `Dog* clone()` overriding `Animal* clone()` — legal (covariant: `Dog*` is-a
   `Animal*`). `Cat*` would also be legal covariant for a `Cat` override. Not
   legal: `int clone()` (unrelated type).
   </details>

3. **final chain:** `struct A { virtual void f(); };  struct B : A { void f()
   final; };  struct C : B { ??? };` — can `C` override `f`? Can `C` add a new
   virtual `g`?

   <details><summary>Answer</summary>

   `C` cannot override `f` (final in `B`) — compile error. `C` can still add new
   virtuals like `g` and be derived from further (unless `C` itself is `final`).
   </details>

4. **Devirtualize:** `struct Shape { virtual double area() const = 0; };  struct
   Tri final : Shape { double b_, h_; double area() const override { return
   0.5*b_*h_; } };  double total(std::span<Tri> ts) { double s = 0; for (auto& t
   : ts) s += t.area(); return s; }` — `-O2`: virtual calls in the loop?

   <details><summary>Answer</summary>

   No — `t` is exactly `Tri` (`final`), compiler devirtualizes → inlines
   `0.5*b_*h_`, likely vectorizes the loop. Without `final` on `Tri`, each
   `t.area()` would stay an indirect virtual call.
   </details>

5. **When NOT final:** ek `class TcpConnection` jo unit tests mein mock hoti hai
   (`class MockConnection : TcpConnection`). `final` lagana chahiye?

   <details><summary>Answer</summary>

   No — `final` would break the mock/derive-for-test pattern. Keep it
   non-`final` (or use an interface + composition). `final` is for types you're
   sure won't be extended, including for testing.
   </details>

---

## Interview questions

1. `override` kya karta, kya nahi? Bina iske kaunsa bug?
2. Override ke liye signature ka kya-kya match hona chahiye?
3. Covariant return type kya hai?
4. `final` — method pe vs class pe? Do effects?
5. `final` devirtualization mein kaise help karta?
6. `final` ke trade-offs — kab lagao, kab nahi?

---

## Next
→ [`06-abstract-classes.md`](06-abstract-classes.md)
