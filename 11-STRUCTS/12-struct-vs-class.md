# 12 — `struct` vs `class`

## Prerequisites
- [`01-what-is-a-struct.md`](01-what-is-a-struct.md)
- (Forward reference: folder 15 — classes, encapsulation)

## Yeh topic abhi kyun
Chhoti par frequently-asked baat: C++ mein `struct` aur `class` mein **sirf ek
technical fark** hai — default access (public vs private). Baaki sab convention
hai. Yeh lesson woh clear karta hai, aur team conventions.

---

## The only technical difference

```cpp
struct S {
    int x;              // PUBLIC by default
};

class C {
    int x;              // PRIVATE by default
};
```

| | `struct` | `class` |
|---|---|---|
| Default member access | **public** | **private** |
| Default base class access | **public** | **private** |
| Everything else (members, methods, ctors, inheritance, templates, `virtual`, ...) | identical | identical |

```cpp
struct S {
private:               // struct can have private members
    int secret;
public:
    void method();     // struct can have methods, ctors, virtual, inheritance...
};

class C {
public:                // class can be all-public
    int data;
};
```

**A `struct` is a `class` with `public` default.** That's it. `S` and `C` above,
with the access specifiers made explicit, are the same type.

---

## Convention — what teams actually do

The **common** (not universal) convention:

### Use `struct` for
- **Plain data aggregates** — all public members, no invariant to protect.
  `struct Point { double x, y; };`, `struct Config { ... };`, wire messages.
- Passive "bundle of related values".
- Often no (or `= default`) constructors, no encapsulation.

### Use `class` for
- Types with an **invariant** to maintain (private data + a public interface that
  keeps it valid). `class Account`, `class OrderBook`, `class TcpConnection`.
- Encapsulation: private implementation, public methods.
- RAII resource owners (folder 17).
- Polymorphic base classes (folder 16).

**Litmus test:** "Can any member be set to any value independently without
breaking anything?" → `struct`. "Are there rules about valid combinations of the
members?" → `class`.

```cpp
struct Vec2 { double x, y; };                    // any x, any y -- fine. struct.

class Rational {                                 // invariant: denom != 0, always reduced
    long num_, denom_;
public:
    Rational(long n, long d);                    // enforces the invariant
    // ... no way to make num_/denom_ inconsistent from outside
};
```

Some codebases / style guides (e.g. Google) say "`struct` only for passive
objects with no methods beyond ctors; `class` otherwise". Follow your project's.

---

## `struct` with methods — totally fine

```cpp
struct Timestamp {
    std::int64_t ns;

    double seconds() const { return ns / 1e9; }
    Timestamp operator+(std::int64_t delta) const { return {ns + delta}; }
    auto operator<=>(const Timestamp&) const = default;
};
```

A `struct` can have member functions, operators, constructors, `static` members —
adding those doesn't make it "should be a class". It's still a `struct` if it's
conceptually a transparent value with public data.

---

## Aggregate-ness (file 02) is separate

Whether a type is an **aggregate** (usable with `{a, b, c}` init, no
user-declared constructors, no private data...) is independent of `struct` vs
`class`:

```cpp
struct A { int x; private: int y; };   // struct, but NOT an aggregate (private data)
class  B { public: int x, y; };        // class, but IS an aggregate (all public, no ctors)
```

For plain-data structs you usually want them to stay aggregates (free `{}` init,
`= default` comparisons, trivially copyable). Adding a user-declared constructor
or private data breaks aggregate-ness.

---

## Andar kya hota hai

- `struct` and `class` produce **identical** code, layout, `sizeof`, ABI. The
  keyword is purely a compile-time default for access specifiers.
- You can even declare a type with `struct` and define/refer to it with `class`
  (a mismatch warning on MSVC, harmless on GCC/Clang, but don't).
- Encapsulation (`private`) has **zero runtime cost** — it's a compile-time
  access check. A `class` with private data is laid out exactly like the
  equivalent `struct`.

> **HFT relevance:** Data-heavy HFT code is full of `struct`s — messages, book
> levels, records, config — because they're transparent values with public data,
> `{}`-initializable, `memcpy`-able, `static_assert`-able for layout. `class` is
> used where an invariant matters (a `Book` that must stay sorted, a
> `SessionManager`). Since there's no performance difference, the choice is
> purely about communicating intent: `struct` says "just data", `class` says
> "there are rules". Folders 15, 17.

---

## Hands-on

`examples/01_struct_basics.cpp` uses `struct` for plain data. Try:

```cpp
struct S { int x; };        // .x accessible
class  C { int x; };        // .x -> compile error (private)
class  C2 { public: int x; };// .x accessible -- identical to S now
static_assert(sizeof(S) == sizeof(C));   // same layout
```

---

## ⚠️ Traps

### Trap 1 — thinking `class` has overhead vs `struct`
Zero difference in generated code.

### Trap 2 — `class` member "not accessible" surprise
```cpp
class P { int x, y; };  P p;  p.x = 1;   // ❌ private. Use struct, or add `public:`
```

### Trap 3 — breaking aggregate-ness unintentionally
```cpp
struct S { int x; S() {} };   // ⚠️ user-declared ctor -> no longer an aggregate -> S{1} fails
struct S { int x = 0; };      // ✅ DMI keeps aggregate-ness (C++14+)
```

### Trap 4 — `struct`/`class` keyword mismatch between decl and def
```cpp
struct Foo;   // forward-declared as struct
class Foo { ... };   // ⚠️ MSVC warns. Keep it consistent
```

### Trap 5 — using `class` for a wire message
```cpp
class WireMsg { ... };   // ⚠️ private data -> not an aggregate -> can't {}-init / static_assert layout as easily. struct
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`struct` and `class` are fundamentally different" | Only default access differs |
| "`struct` can't have methods / ctors / inheritance" | It can — all of it |
| "`class` is slower (encapsulation cost)" | Zero runtime cost |
| "Use `class` for everything object-oriented" | `struct` for transparent data, `class` for invariants |
| "Making a struct a class is a big change" | It flips defaults; make access explicit and it's the same |

---

## Exercises

1. **Prove identical:** `struct S { int a, b; };` and `class C { public: int a,
   b; };` — `static_assert(sizeof(S) == sizeof(C))`, same offsets, both
   `{}`-initializable.

2. **Access flip:** `class Money { long cents; };` — why can't you `m.cents = 5`?
   Make it work two ways (add `public:`, or switch to `struct`).

3. **Invariant → class:** `struct Fraction { int num, den; };` (den can be 0!) —
   convert to a `class` that guarantees `den != 0` and always reduced.

4. **Struct with methods:** `struct Duration { long ns; double ms() const;
   Duration operator+(Duration) const; auto operator<=>(const Duration&) const =
   default; };` — is this still "a struct"? (Yes — public data, transparent.)

5. **Aggregate check:** which of these are aggregates?
   ```cpp
   struct A { int x; };
   struct B { int x; B(int); };
   struct C { int x = 0; };
   class  D { public: int x, y; };
   struct E { private: int x; };
   ```

6. **Convention audit:** in your own code (or the course examples), list which
   types "should" be `struct` and which `class` by the invariant test.

---

## Interview questions

1. `struct` aur `class` mein technical fark — sirf ek. Kaunsa?
2. `struct` mein methods / private / inheritance ho sakta hai?
3. `class` (encapsulation) ka runtime cost?
4. Kab `struct` use karo, kab `class` (invariant test)?
5. Aggregate-ness `struct`/`class` se independent kaise?
6. Wire message — `struct` ya `class`, aur kyun?

---

## Next
→ [`13-exercises.md`](13-exercises.md)
