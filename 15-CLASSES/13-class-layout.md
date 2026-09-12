# 13 — Class layout — `sizeof`, padding, empty base optimization

## Prerequisites
- Folder 11 files 05–07 (padding & alignment deep dive — must know this cold)
- [`03-access-specifiers.md`](03-access-specifiers.md), [`08-static-members.md`](08-static-members.md)

## Yeh topic abhi kyun
Folder 11 mein `struct` ka layout kiya. Class ke saath 3 naye sawaal: (1) kya
`private`/methods layout badalte hain (nahi), (2) empty class ka `sizeof` kyun
1, (3) empty base optimization (EBO) — jab ek empty base 0 bytes le. Yeh
`sizeof` surprises aur zero-cost abstraction patterns ki jad hai.

---

## Non-virtual class ka layout = data members ka layout

```cpp
class Point {
    double x_;                        // @0, 8 bytes
    double y_;                        // @8, 8 bytes
public:
    Point(double x, double y);
    double distanceFromOrigin() const;   // methods -> 0 bytes
    void   translate(double, double);
};

static_assert(sizeof(Point) == 16);              // sirf 2 doubles
static_assert(std::is_standard_layout_v<Point>); // sab private, no virtual
```

- **`private`/`public`/`protected` — layout badalte nahi** (compile-time access
  check only). `class Point { double x_, y_; }` ka `sizeof` == `struct P {
  double x, y; }` ka.
- **Methods, `static` members, `friend` declarations — per-object 0 bytes.**
- Padding & alignment rules folder 11 se **exactly same** apply hote hain
  (largest member ka alignment, member order matters, `alignas`, `#pragma pack`).

```cpp
class Bad  { char a_; double b_; char c_; };   // 1+7pad + 8 + 1+7pad = 24
class Good { double b_; char a_; char c_; };   // 8 + 1 + 1 + 6pad = 16
```

[`examples/07_class_layout.cpp`](examples/07_class_layout.cpp) yeh sab measure
karta hai.

---

## `is_standard_layout` — mixed access breaks it

```cpp
class A { int x_; int y_; };                 // sab private -> standard-layout ✅
class B { public: int x_; private: int y_; }; // MIXED access -> NOT standard-layout ❌
```

Standard-layout ke liye (among other rules): **saare non-static data members ka
same access control**. Toota to:
- `offsetof` → `-Winvalid-offsetof` (UB technically).
- Members ka relative order compiler decide kar sakta.
- `memcpy` / `bit_cast` / wire-format assumptions risky.

**Rule:** `memcpy`-able / wire / `static_assert(offsetof...)` types → saare data
members **ek hi access** mein (aksar `struct`, all public). Invariant types →
`class`, all private, layout still fine but don't rely on `offsetof`.

---

## Empty class — `sizeof >= 1`

```cpp
class Empty {};
class Tag   { void mark() const {} };          // methods only, no data

static_assert(sizeof(Empty) == 1);             // NOT 0
static_assert(sizeof(Tag)   == 1);
```

Kyun 1, 0 nahi: **har object ka ek unique address hona chahiye**. `Empty a, b;`
→ `&a != &b` guaranteed. Agar `sizeof(Empty) == 0` hota to `Empty arr[3]` ke
saare elements same address pe → identity toot jaata. To compiler 1 padding byte
deta.

```cpp
Empty a, b;
Empty arr[3];
assert(&a != &b);
assert(&arr[0] != &arr[1]);      // sizeof 1 -> elements 1 byte apart
```

---

## Empty Base Optimization (EBO)

Ek empty class **member** hone par 1 byte (+ padding) leti hai, par **base
class** hone par **0 bytes** le sakti hai:

```cpp
struct Empty {};

struct AsMember {
    Empty e_;                     // 1 byte
    int   n_;                     // + 3 pad + 4
};                                 // sizeof == 8

struct AsBase : Empty {           // EBO -- Empty base 0 bytes
    int n_;
};                                 // sizeof == 4   ✅

static_assert(sizeof(AsMember) == 8);
static_assert(sizeof(AsBase)   == 4);
```

Kyun allowed as base: base subobject ka address derived object ke saath share ho
sakta hai (no separate identity needed) — to compiler use 0 bytes deta.

**EBO kahan use hota hai (aap indirectly benefit lete ho):**
- `std::unique_ptr<T, Deleter>` — agar `Deleter` empty (default), `unique_ptr`
  sirf ek pointer jitna (8 bytes), deleter 0.
- `std::vector<T, Alloc>` — empty allocator 0 bytes.
- Policy-based design, `std::tuple` of empty types, EBCO wrappers.
- C++20 **`[[no_unique_address]]`** — same effect for **members**:
  ```cpp
  struct Better {
      [[no_unique_address]] Empty e_;   // 0 bytes (if empty)
      int n_;
  };
  static_assert(sizeof(Better) == 4);
  ```

---

## `is_polymorphic` — no virtual, no vptr

```cpp
class Plain    { int x_; void f(); };
class Poly     { int x_; virtual void g(); };

static_assert(!std::is_polymorphic_v<Plain>);
static_assert( std::is_polymorphic_v<Poly>);

std::cout << sizeof(Plain);   // 4
std::cout << sizeof(Poly);    // 16  (int + 4 pad + 8-byte vptr) -- folder 16
```

Jaise hi ek `virtual` method (ya virtual base) aata hai, object ko ek **vptr**
(hidden pointer to vtable) milta hai — usually 8 bytes, object ke shuru mein.
Poora mechanism folder 16 file 04. Is folder ke classes (koi `virtual` nahi) →
layout bilkul struct jaisa.

---

## Andar kya hota hai

- Compiler class ke non-static data members ko declaration order mein lay out
  karta (standard-layout ke liye guaranteed; mixed-access mein flexibility hai
  par GCC/Clang aksar phir bhi declaration order). Har member ke aage alignment
  padding, class ke end pe tail padding taaki `sizeof` largest alignment ka
  multiple ho (arrays ke liye).
- `private` = symbol table mein ek access flag; generated machine code identical.
- Empty class → compiler 1 dummy byte. Empty **base** → derived ke layout mein
  overlap allowed → 0 bytes (EBO).
- `[[no_unique_address]]` member → agar empty, compiler use kisi aur member ke
  padding mein "overlap" karne deta → 0 effective bytes.
- vptr (folder 16) → non-EBO 8 bytes, object ke offset 0 pe (usually), har
  virtual call us pointer se ek indirection.

> **HFT relevance:** hot data structures ka layout **exactly** designed hota hai
> — folder 11 ke padding rules + yeh: classes with all-private data are fine to
> lay out tightly, `static_assert(sizeof(X) == N)` layout ko lock karta, aur
> cache-line packing (`alignas(64)`, `[[no_unique_address]]`) se density
> maximize. **Zero `virtual`** hot types mein (koi vptr nahi → 8 bytes bache,
> koi indirect call nahi → folder 16 dispatch cost avoid). Empty policy/tag
> types (comparators, allocators, strong-typedef tags) EBO / `[[no_unique_address]]`
> se free — yeh zero-cost abstraction ka mechanism hai. `std::is_standard_layout`
> + `std::is_trivially_copyable` static_asserts wire/pool types pe standard.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/07_class_layout.cpp
```

`Empty` (1), `WithMethodsOnly` (1), `Accessors` (8 — 2 ints, 4 methods ka koi
fark nahi), `BadLayout` (24) vs `GoodLayout` (16), `offsetof`, `is_polymorphic`
(false). Try: ek `virtual` method add karo, `sizeof` +8 dekho.

---

## ⚠️ Traps

### Trap 1 — sochna `private` / methods `sizeof` badhate
Zero. Layout = data members + padding only.

### Trap 2 — `sizeof(EmptyClass) == 0` expect karna
`>= 1` — address uniqueness. `Empty arr[N]` ke elements alag addresses chahiye.

### Trap 3 — empty type ko **member** rakhna jab base/`[[no_unique_address]]` se free milta
```cpp
struct S { Comparator cmp_; int n_; };   // ⚠️ cmp_ empty? 1+pad bytes. : Comparator base, ya [[no_unique_address]]
```

### Trap 4 — mixed access + `offsetof`
```cpp
struct M { public: int a; private: int b; };  offsetof(M, a);   // ⚠️ not standard-layout -> -Winvalid-offsetof
```

### Trap 5 — `virtual` add karne ke baad `memcpy` / `static_assert(size)`
```cpp
struct Wire { int a, b; virtual ~Wire(); };   // ⚠️ ab vptr hai -> memcpy corrupt, size badal gaya
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`class` `struct` se bada (encapsulation)" | Same layout — access is compile-time |
| "Methods object ke andar store hote" | `.text` mein — per-object 0 bytes |
| "Empty class 0 bytes" | `>= 1` (address uniqueness); base ke roop mein 0 (EBO) |
| "Empty member hamesha jagah leta" | EBO (base) / `[[no_unique_address]]` (member) → 0 |
| "Non-virtual class mein bhi vptr" | Sirf `virtual` (method ya base) aane pe |

---

## Exercises

1. **Predict sizeof:** `class A { char c_; int i_; char d_; };`, `class B { int
   i_; char c_; char d_; };`, `class C {};`, `class D { void f(); void g(); };`

   <details><summary>Answer</summary>

   `A`: 1+3pad + 4 + 1 + 3pad = 12. `B`: 4 + 1 + 1 + 2pad = 8. `C`: 1. `D`: 1
   (methods no storage).
   </details>

2. **EBO:** `struct Tag {};  struct Wrap1 { Tag t_; long n_; };  struct Wrap2 :
   Tag { long n_; };` — `sizeof(Wrap1)`, `sizeof(Wrap2)`?

   <details><summary>Answer</summary>

   `Wrap1`: `Tag` 1 byte + 7 pad + `long` 8 = 16. `Wrap2`: EBO → `Tag` 0 bytes +
   `long` 8 = 8.
   </details>

3. **no_unique_address:** `Wrap1` ko `[[no_unique_address]] Tag t_;` se rewrite
   karo. Naya `sizeof`?

   <details><summary>Answer</summary>

   `struct Wrap3 { [[no_unique_address]] Tag t_; long n_; };` → `sizeof == 8`
   (empty `t_` `n_` ke saath overlap kar leta). (GCC/Clang honor karte;
   guaranteed nahi har compiler.)
   </details>

4. **standard-layout:** in mein se kaun `std::is_standard_layout_v`? `struct A {
   int x, y; };`, `class B { int x; public: int y; };`, `class C { int x_; int
   y_; };`, `struct D { int x; virtual void f(); };`

   <details><summary>Answer</summary>

   `A` ✅, `C` ✅ (all same access). `B` ❌ (mixed access). `D` ❌ (virtual).
   </details>

5. **virtual cost:** `class Msg { std::int64_t seq_; double px_; };` (16 bytes).
   Ek `virtual ~Msg();` add karo. Naya `sizeof`? `is_trivially_copyable`?
   `memcpy`-able still?

   <details><summary>Answer</summary>

   `sizeof` → 24 (8-byte vptr + 16). `is_trivially_copyable` → false (virtual).
   `memcpy` between `Msg` objects → UB (vptr copy corrupts). Isliye wire/pool
   types mein `virtual` avoid.
   </details>

---

## Interview questions

1. `private` / methods / `static` members — class layout pe asar?
2. Empty class ka `sizeof` — kya aur kyun?
3. Empty Base Optimization — kya, kyun allowed, kahan use hota (STL)?
4. `[[no_unique_address]]` — kya karta?
5. Mixed access aur `is_standard_layout` — relation, kya toot-ta?
6. `virtual` add karne se layout / `sizeof` / triviality pe kya asar?

---

## Next
→ [`14-exercises.md`](14-exercises.md)
