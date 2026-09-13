# 12 — `struct` vs `class`

## Prerequisites
- [`01-what-is-a-struct.md`](01-what-is-a-struct.md)
- (Aage ka reference: folder 15 — classes, encapsulation)

## Yeh topic abhi kyun
Chhoti par interview mein baar-baar poochhi jaane wali baat: C++ mein `struct` aur `class` mein **sirf ek
technical fark** hai — default access (public vs private). Baaki sab convention
hai. Yeh lesson woh saaf karta hai, aur teams ki conventions bhi.

---

## Ekmaatra technical fark

```cpp
struct S {
    int x;              // default mein PUBLIC
};

class C {
    int x;              // default mein PRIVATE
};
```

| | `struct` | `class` |
|---|---|---|
| Default member access | **public** | **private** |
| Default base class access | **public** | **private** |
| Baaki sab (members, methods, ctors, inheritance, templates, `virtual`, ...) | same | same |

```cpp
struct S {
private:               // struct mein private members ho sakte hain
    int secret;
public:
    void method();     // struct mein methods, ctors, virtual, inheritance... sab
};

class C {
public:                // class poori public ho sakti hai
    int data;
};
```

**`struct` = `public` default wali `class`.** Bas itna. Access specifiers khul ke likh do to `struct S { int x; };`
aur `class C { public: int x; };` ka **layout, behaviour, code — sab same** hai. (Type phir bhi alag hain, kyunki
naam alag hai — `std::is_same_v<S, C>` `false`. Keyword se koi fark nahi padta.)

Analogy: do ek jaise ghar, ek ka darwaza default mein khula (`struct`), ek ka band (`class`). Andar ka naksha
bilkul same; darwaza aap kabhi bhi khol/band kar sakte ho.

---

## Convention — teams asal mein kya karti hain

**Aam** (sab jagah nahi) convention:

### `struct` kab
- **Plain data aggregates** — saare members public, bachane ko koi niyam (invariant) nahi.
  `struct Point { double x, y; };`, `struct Config { ... };`, wire messages.
- "Aapas mein jude values ka bundle", jo khud kuch nahi karta.
- Aksar constructors nahi (ya `= default`), encapsulation nahi.

### `class` kab
- Aise types jinka ek **invariant** banaye rakhna hai (private data + public interface jo use valid rakhe).
  `class Account`, `class OrderBook`, `class TcpConnection`.
- Encapsulation: private implementation, public methods.
- RAII resource owners (folder 17).
- Polymorphic base classes (folder 16).

**Litmus test:** "Kya koi bhi member kisi bhi value pe, doosron se alag, set ho sakta hai bina kuch tode?" → `struct`.
"Kya members ke valid combinations ke niyam hain?" → `class`.

```cpp
struct Vec2 { double x, y; };                    // koi bhi x, koi bhi y -- theek. struct.

class Rational {                                 // invariant: denom != 0, hamesha reduced
    long num_, denom_;
public:
    Rational(long n, long d);                    // invariant yahin enforce hota hai
    // ... bahar se num_/denom_ ko inconsistent karne ka koi raasta nahi
};
```

Kuch codebases / style guides (jaise Google) kehte hain "`struct` sirf passive objects ke liye jinme ctors ke alawa
methods na hon; baaki sab `class`". Apne project ki follow karo.

---

## Methods wala `struct` — bilkul theek

```cpp
struct Timestamp {
    std::int64_t ns;

    double seconds() const { return ns / 1e9; }
    Timestamp operator+(std::int64_t delta) const { return {ns + delta}; }
    auto operator<=>(const Timestamp&) const = default;
};
```

`struct` mein member functions, operators, constructors, `static` members ho sakte hain — inhe jodne se woh "class
honi chahiye" nahi ban jaata. Agar concept ke hisaab se woh public data wali transparent value hai, to `struct` hi hai.

---

## Aggregate hona (file 02) alag baat hai

Type **aggregate** hai ya nahi (`{a, b, c}` init, koi user-declared constructor nahi, private data nahi...) — yeh
`struct` vs `class` se independent hai:

```cpp
struct A { int x; private: int y; };   // struct, par aggregate NAHI (private data)
class  B { public: int x, y; };        // class, par aggregate HAI (sab public, ctors nahi)
```

Plain-data structs ko aam taur pe aggregate hi rehne do (muft `{}` init, `= default` comparisons, trivially
copyable). User-declared constructor ya private data jodte hi aggregate-pan chala jaata hai.

⚠️ C++20 ne yeh niyam kada kiya: `struct F { int x; F() = default; };` — `-std=c++17` pe `F{1}` compile hota hai
(aggregate), `-std=c++20` pe ❌ `no matching function for call to 'F::F(<brace-enclosed initializer list>)'`
(GCC 16.2 pe dono chala ke). User-*declared* constructor, `= default` bhi, ab aggregate tod deta hai.

---

## Andar kya hota hai

- `struct` aur `class` **same** code, layout, `sizeof`, ABI banate hain. Keyword sirf access specifiers ka
  compile-time default hai.
- Type ko `struct` se declare karke `class` se define/refer bhi kar sakte ho. MSVC warn karta hai; GCC 16.2
  `-Wall -Wextra` pe **chup** rehta hai — sirf `-Wmismatched-tags` dene pe:
  `warning: 'Foo' declared with a mismatched class-key 'struct'`. Kaam karta hai, par consistent raho.
- Encapsulation (`private`) ki **runtime cost zero** hai — yeh compile-time access check hai. Private data wali
  `class` ka layout bilkul waisa hi hota hai jaisa barabar ke `struct` ka.

> **HFT relevance:** Data-heavy HFT code `struct`s se bhara hota hai — messages, book levels, records, config —
> kyunki woh public data wali transparent values hain: `{}`-initializable, `memcpy`-able, layout ke liye
> `static_assert`-able. `class` wahan jahan invariant matter kare (ek `Book` jo hamesha sorted rahe, ek
> `SessionManager`). Performance mein koi fark nahi, isliye choice sirf irada batane ki hai: `struct` kehta hai
> "bas data", `class` kehta hai "niyam hain". Folders 15, 17.

---

## Hands-on

`examples/01_struct_basics.cpp` plain data ke liye `struct` use karta hai. Try karo:

```cpp
struct S { int x; };         // .x accessible
class  C { int x; };         // .x -> compile error (private)
class  C2 { public: int x; };// .x accessible -- ab S jaisa hi
static_assert(sizeof(S) == sizeof(C));   // same layout
```

---

## ⚠️ Traps

### Trap 1 — `class` mein `struct` se zyada overhead samajhna
Generated code mein zero fark.

### Trap 2 — `class` member "not accessible" ka surprise
```cpp
class P { int x, y; };  P p;  p.x = 1;   // ❌ private. struct lo, ya `public:` jodo
```

### Trap 3 — anjaane mein aggregate-pan todna
```cpp
struct S { int x; S() {} };        // ⚠️ user-declared ctor -> aggregate nahi -> S{1} fail
struct F { int x; F() = default; };// ⚠️ C++20 mein yeh bhi aggregate nahi -> F{1} fail (C++17 mein chalta tha)
struct S2 { int x = 0; };          // ✅ DMI aggregate-pan rakhta hai (C++14+)
```

### Trap 4 — declaration aur definition mein `struct`/`class` keyword alag
```cpp
struct Foo;          // struct se forward-declare
class Foo { ... };   // ⚠️ MSVC warn karta hai; GCC sirf -Wmismatched-tags pe. Consistent raho
```

### Trap 5 — wire message ke liye `class`
```cpp
class WireMsg { ... };   // ⚠️ private data -> aggregate nahi -> {}-init nahi, bahar se offsetof check nahi. struct lo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`struct` aur `class` buniyaadi taur pe alag hain" | Sirf default access alag |
| "`struct` mein methods / ctors / inheritance nahi ho sakte" | Sab ho sakta hai |
| "`class` slow hai (encapsulation ki cost)" | Runtime cost zero |
| "Object-oriented kuch bhi ho to `class`" | Transparent data → `struct`, invariants → `class` |
| "Struct ko class banana bada badlaav hai" | Sirf defaults palatte hain; access khul ke likho to same |
| "`F() = default;` jodne se aggregate nahi tootta" | C++20 se tootta hai |

---

## Exercises

1. **Same saabit karo:** `struct S { int a, b; };` aur `class C { public: int a, b; };` —
   `static_assert(sizeof(S) == sizeof(C))`, same offsets, dono `{}`-initializable.

2. **Access palto:** `class Money { long cents; };` — `m.cents = 5` kyun nahi chalta? Do tarah theek karo (`public:`
   jodo, ya `struct` banao).

3. **Invariant → class:** `struct Fraction { int num, den; };` (den 0 ho sakta hai!) — ise aisi `class` banao jo
   `den != 0` aur hamesha reduced ki guarantee de.

4. **Methods wala struct:** `struct Duration { long ns; double ms() const; Duration operator+(Duration) const;
   auto operator<=>(const Duration&) const = default; };` — kya yeh ab bhi "struct" hai? (Haan — public data,
   transparent.)

5. **Aggregate check:** inmein se kaun aggregate hai (`-std=c++20`)?
   ```cpp
   struct A { int x; };
   struct B { int x; B(int); };
   struct C { int x = 0; };
   class  D { public: int x, y; };
   struct E { private: int x; };
   ```
   <details><summary>Answer (`std::is_aggregate_v`, GCC 16.2)</summary>

   `A` ✅, `B` ❌ (user-declared ctor), `C` ✅ (DMI theek hai, C++14+), `D` ✅ (sab public, keyword `class` se fark
   nahi), `E` ❌ (private data). Bonus: `struct F { int x; F() = default; };` → C++20 mein ❌.
   </details>

6. **Convention audit:** apne code (ya course ke examples) mein list banao — invariant test ke hisaab se kaun "struct"
   hona chahiye, kaun `class`.

---

## Interview questions

1. `struct` aur `class` mein technical fark — sirf ek. Kaunsa?
2. `struct` mein methods / private / inheritance ho sakta hai?
3. `class` (encapsulation) ki runtime cost?
4. Kab `struct`, kab `class` (invariant test)?
5. Aggregate-pan `struct`/`class` se independent kaise?
6. Wire message — `struct` ya `class`, aur kyun?

---

## Next
→ [`13-exercises.md`](13-exercises.md)
