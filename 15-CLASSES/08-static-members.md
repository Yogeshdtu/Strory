# 08 — Static members

## Prerequisites
- [`02-members-and-methods.md`](02-members-and-methods.md)
- Folder 14 file 07 (static storage duration, init order)

## Yeh topic abhi kyun
`static` member = class ka, kisi ek object ka nahi. Ek hi copy, sab objects
share. Use: instance counters, shared config/tables, factory functions, named
constants. Aur `static` methods (no `this`). Ek chhota par frequently-tested
topic, plus C++17 `inline static` ne isme ek irritating cheez fix ki.

---

## Static data member

```cpp
class Connection {
    int id_;                           // per-object -- har Connection ki apni

    static int s_nextId;               // per-CLASS -- ek hi copy sab Connections ke liye
    static int s_liveCount;
public:
    Connection() : id_(s_nextId++) { ++s_liveCount; }
    ~Connection() { --s_liveCount; }
};

// ⚠️ C++17 se pehle: definition ek .cpp mein ZAROORI (declaration class mein, storage yahan)
int Connection::s_nextId   = 1;
int Connection::s_liveCount = 0;
```

- Class ke andar `static int s_nextId;` sirf **declaration** hai (koi storage
  nahi).
- Ek `.cpp` mein `int Connection::s_nextId = 1;` — yeh **definition** (storage +
  init). Header mein karoge → multiple definition (ODR violation) linker error.

### C++17 `inline static` — header-only

```cpp
class Connection {
    static inline int s_nextId   = 1;    // ✅ definition + init RIGHT HERE
    static inline int s_liveCount = 0;   // no separate .cpp line
};
```

`inline` linker ko bolta "yeh multiple TUs mein dikhega par ek hi object hai" —
header-only classes ke liye yeh default choice ab.

### `static constexpr` — hamesha inline

```cpp
class Physics {
public:
    static constexpr double kGravity = 9.80665;   // implicitly inline (C++17), compile-time constant
    static constexpr int    kMaxNodes = 1024;
};
// Physics::kGravity -- koi separate definition nahi chahiye (odr-use na ho to)
```

---

## Access

```cpp
Connection::s_liveCount;        // ✅ ClassName::member  -- object ki zaroorat nahi
conn.s_liveCount;               // ✅ object.member bhi allowed (par ClassName:: clearer)

// Agar private hai:
class Connection {
    static inline int s_liveCount = 0;
public:
    static int liveCount() { return s_liveCount; }   // accessor
};
Connection::liveCount();        // ✅
```

---

## Static member functions

```cpp
class Connection {
    static inline int s_liveCount = 0;
    int id_;
public:
    static int  liveCount()  { return s_liveCount; }   // no `this`
    static bool atCapacity() { return s_liveCount >= 100; }

    // static method mein `this` NAHI:
    // int getId() { return id_; }     // ❌ agar yeh static hota -> id_ (non-static) access error
};

Connection::liveCount();        // object ke bina call
```

- **Koi `this` nahi** → non-static members / methods ko directly access nahi kar
  sakta (kis object ka `id_`? pata nahi).
- Static data members + parameters ke saath kaam karta.
- Use: factory functions, class-level queries, utility grouped under a class,
  callbacks jinhe C-style function pointer chahiye (static method ka signature
  free function jaisa — folder 12 file 11).

```cpp
class Widget {
public:
    static Widget fromConfig(const Config& c);      // factory -- named constructor pattern
    static Widget makeDefault() { return Widget{0, "default"}; }
private:
    Widget(int id, std::string name);               // private ctor -- sirf factory se banega
};

auto w = Widget::fromConfig(cfg);
```

---

## Static local vs static member (confuse mat karo)

```cpp
class C {
    static inline int s_shared = 0;     // static MEMBER -- class ka, sab objects share

    int nextTicket() {
        static int counter = 0;         // static LOCAL -- is function ka, sab C objects share bhi
        return ++counter;               // (function-local static -- folder 14 file 07)
    }
};
```

Dono "ek hi copy" hote hain, par: static **member** class scope mein, static
**local** function scope mein. Static local pehli call pe init hota (thread-safe
C++11+), static member program start pe (dynamic init — fiasco risk, folder 14
file 07).

---

## Andar kya hota hai

- Static data member = ek symbol `.data` / `.bss` mein (folder 14 file 01),
  bilkul ek global variable jaisa — bas naam `Connection::s_nextId` (mangled).
  Objects ke andar **koi storage nahi** leta → `sizeof(class)` pe asar nahi.
- `Connection::liveCount()` = ek normal free function `.text` mein, bina hidden
  `this` argument. Signature ek plain `int()` — isiliye C API callbacks
  (`pthread_create`, qsort comparator) ke liye usable jahan capturing lambda /
  member function pointer nahi chalte.
- `static constexpr` member = compile-time constant, aksar koi storage hi nahi
  (jab tak odr-use na ho — address liya, reference bind hua).
- **Init order:** static members (non-constexpr, dynamic-init) `main` se pehle,
  aur **cross-TU order unspecified** — static-init-order fiasco lagta hai (folder
  14 file 07). Fix: "construct on first use" (`static T& get() { static T t;
  return t; }`).

> **HFT relevance:** static members = shared read-mostly data ke liye — ek
> symbol table, ek config snapshot, compile-time constants (`static constexpr
> int kRingSize = 1 << 16;`). Hot path pe static **constants** ideal (compile-time
> folded, no per-object cost). Shared **mutable** static (counters, registries)
> multi-thread mein contention / false-sharing — un cases mein `thread_local`
> counters + aggregate, ya `alignas(64) std::atomic`. Static factory methods
> (`Order::fromWire(buf)`) private ctors ke saath — sirf validated Orders bante
> hain. Init-order fiasco se bachne ko critical shared state ko explicit startup
> sequence mein initialize karte, cross-TU static ctors pe rely nahi.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/05_static_members.cpp
```

`Connection` — `s_liveCount` / `s_totalEver` / `s_nextId` shared counters,
`inline static`, `liveCount()` / `atCapacity()` static methods. Block scope se
`Connection` nikalte hi count ghatta hai.

---

## ⚠️ Traps

### Trap 1 — static member ki definition bhool jaana (pre-C++17 style)
```cpp
class C { static int n; };   // declaration
// int C::n = 0;  <- yeh bhool gaye -> "undefined reference to C::n" linker error
```

### Trap 2 — static data member header mein `static int n = 0;` (bina inline)
```cpp
// har TU jo header include kare -> apni definition -> multiple definition linker error.
// static inline int n = 0;  use karo (C++17)
```

### Trap 3 — static method mein `this` / non-static member
```cpp
static int f() { return id_; }   // ❌ static method mein `this` nahi -> id_ access error
```

### Trap 4 — static local ko static member samajhna
```cpp
int next() { static int c = 0; return ++c; }   // static LOCAL -- alag concept from static member
```

### Trap 5 — cross-TU static member init order
```cpp
// a.cpp: int A::table[] = build();   b.cpp: int B::x = A::table[0];  -- order unspecified -> bug
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Static member har object ki copy" | Ek copy, sab objects share (class-level) |
| "Static member `sizeof(class)` badhata" | Nahi — storage `.data`/`.bss` mein, object mein nahi |
| "Static method `this` use kar sakta" | Nahi — no `this`, no non-static members directly |
| "`static int n;` class mein hi kaafi (pre-C++17)" | Declaration only; `.cpp` mein define, ya `inline` (C++17) |
| "Static member init order source order" | Ek TU mein haan; TUs ke beech unspecified |

---

## Exercises

1. **Instance counter:** `class Widget` jisme `static` `s_count` ho — har ctor
   (default + copy) `++`, dtor `--`. `{ Widget a; Widget b = a; { Widget c; } }`
   ke har point pe `Widget::count()`?

   <details><summary>Answer</summary>

   `a` → 1, `b = a` (copy ctor, +1) → 2, `c` → 3, `c` dtor → 2, block end `b`,`a`
   dtors → 0. (Copy ctor ko bhi `++` karna zaroori — warna count galat.)
   </details>

2. **inline static:** `class Config { static inline std::string s_env =
   "prod"; };` — 3 `.cpp` files include karein `config.hpp`. Linker error? Kyun
   / kyun nahi?

   <details><summary>Answer</summary>

   Koi error nahi — `inline` batata hai "multiple TUs mein dikhega, ek hi
   object". Bina `inline` → 3 definitions → "multiple definition" linker error.
   </details>

3. **Static factory:** `class Temperature` with private ctor, `static
   Temperature fromCelsius(double)`, `static Temperature fromFahrenheit(double)`.
   `Temperature t = Temperature::fromFahrenheit(98.6);` — internal celsius?

   <details><summary>Answer</summary>

   `fromFahrenheit(98.6)` → `(98.6 - 32) * 5.0/9.0 ≈ 37.0` celsius, private ctor
   se `Temperature{37.0}`. Direct `Temperature{37.0}` bahar se nahi ban sakta
   (private ctor) — sirf factories.
   </details>

4. **static method as callback:** C API `void run(void(*cb)(int));` — ek class
   `Handler` jiska `static void onEvent(int)` isse pass ho sake. Capturing
   lambda kyun nahi chalega?

   <details><summary>Answer</summary>

   `&Handler::onEvent` ka type `void(*)(int)` (static → no `this`) → directly
   passable. Non-static member function pointer ka type alag (`void(Handler::*)(int)`).
   Capturing lambda → function-pointer conversion nahi (folder 12 file 11).
   </details>

5. **Fiasco:** `struct Registry { static std::vector<int>& all(); };` — cross-TU
   safe "construct on first use" version likho.

   <details><summary>Answer</summary>

   `std::vector<int>& Registry::all() { static std::vector<int> v; return v; }` —
   pehli call pe init (thread-safe C++11+), koi cross-TU static-ctor order
   dependency nahi.
   </details>

---

## Interview questions

1. Static data member vs non-static — storage, count, `sizeof` pe asar?
2. Pre-C++17 static member ko define kahan karna padta, kyun?
3. `inline static` (C++17) kya solve karta?
4. Static member function — `this`? Kya access kar sakta, kya nahi? Use case?
5. Static member vs static local — dono "ek copy", fark kya?
6. Static factory method pattern — kyun (private ctor ke saath)?

---

## Next
→ [`09-operator-overloading.md`](09-operator-overloading.md)
