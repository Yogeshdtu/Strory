# 01 — Object kya hai (standard ke hisaab se)

## Prerequisites
- `24-COMPILATION-LINKING` (storage, linkage), `18-COPY-MOVE`
- `12-POINTERS`, `14-MEMORY` (addresses, storage)
- [`examples/01_lifetime_demo.cpp`](examples/01_lifetime_demo.cpp)

## Yeh topic abhi kyun
Ab tak "object" ka matlab tha "koi variable / cheez jise ek naam diya". Standard ki
definition **exact** hai, aur usse samajhne se aage ke saare topics — lifetime,
placement new, strict aliasing, UB — click karte hain. HFT interviews ka "hard
section" yahin se shuru hota hai: *"C++ mein object ki definition kya hai?"*

---

## Standard ki definition

> **Object** = ek **region of storage**. (`[intro.object]`)

Bas. Ek object ke paas hote hain:

| Property | Matlab |
|---|---|
| **Storage** | memory ka ek chunk (`sizeof` bytes), ek address se |
| **Type** | woh storage ko kaise interpret karna hai |
| **Lifetime** | kab se kab tak object "exist" karta hai (file 02) |
| **Value** | us waqt storage mein jo bytes hain, type ke through padhe |
| **Storage duration** | storage kitni der allocated rehta (file 03) |
| (optional) **Name** | ek identifier jo isse refer kare |
| (optional) **Alignment** | address kis multiple pe hona chahiye (file 08) |

**Object ≠ variable.** Ek *variable* = ek object (ya reference) jise ek **naam**
diya. Bahut objects ke naam nahi hote:
- `new int` ka result — koi naam nahi, sirf ek pointer.
- Ek `std::vector<int>` ke andar ke elements — koi individual naam nahi.
- Temporaries (`f() + g()` mein `f()` ka result) — file 05.
- Ek array `int a[10]` — `a` khud ek object hai, aur uske andar 10 aur objects
  (`a[0]`..`a[9]`), jinke naam nahi.

**Functions objects nahi hain.** **References objects nahi hain** (unhe storage lena
zaroori nahi — aksar compiler unhe pointer ki tarah implement karta, par language
level pe woh "ek existing object ka alias" hain, khud object nahi).

---

## Objects kaise "bante" hain

Ek object create hota hai:
1. **Definition** se — `int x;`, `Widget w;`, `int a[10];`
2. **`new` expression** se — `new int`, `new Widget(...)`
3. **Temporary** materialize hone pe — `Widget{}`, function ka by-value return
4. **`union` member switch** pe (kuch cases)
5. Compiler-implicit — e.g. `std::malloc`/`std::memcpy` **implicit-lifetime types**
   ke objects create kar sakta hai (C++20, `[intro.object]/10`), array ke elements,
   base class subobjects
6. **Placement new** se — `::new (ptr) Widget(...)` (file 09)

Har case mein: storage aata hai, phir (non-trivial types ke liye) constructor
chalta hai → **lifetime shuru** (file 02).

---

## Subobjects

Ek object doosre objects ko **contain** kar sakta hai:

```cpp
struct Point { double x, y; };
struct Segment { Point a, b; };

Segment s;              // s ek object
                        //   s.a, s.b -> member subobjects (Point)
                        //   s.a.x, s.a.y, s.b.x, s.b.y -> aur subobjects (double)
```

- **Member subobjects** — struct/class ke non-static data members.
- **Base class subobjects** — inherited base parts.
- **Array elements** — `a[i]`.

"Complete object" = woh object jo kisi aur ka subobject nahi hai (top-level).

**`sizeof(char) == 1` by definition.** Har complete object ka `sizeof >= 1` (do
alag objects ke alag addresses hone chahiye) — isliye empty class ka `sizeof` bhi
`1` (jab tak EBO na lage — folder 15).

---

## Object ka address aur `sizeof`

```cpp
struct S { int a; char b; };   // sizeof(S) == 8 (int 4 + char 1 + 3 padding)
S s;
```

- `&s` — object ka address (pehle byte ka).
- `sizeof(S)` — object **kitne bytes ghera** — including internal padding
  (file 07). `sizeof` array elements ke beech ka stride bhi hai.
- Standard-layout type ke liye: `&s == (void*)&s.a` (pehla member usi address pe).

**Har object jo `unsigned char`/`std::byte`/`char` array nahi hai, uske bytes ko
`unsigned char*` se access karna LEGAL hai** (aliasing exception — file 10). Yeh
serialization, hashing, `memcpy` ka basis hai.

---

## Value representation vs object representation (file 07 preview)

```cpp
struct T { char c; int i; };   // sizeof == 8
```
- **Object representation** — poore `sizeof(T)` = 8 bytes (c + 3 padding + i).
- **Value representation** — woh bytes jo *value* determine karte — 5 bytes
  (c + i), padding nahi.
- Padding bytes **indeterminate** — `memcmp` do "equal" `T` objects pe non-zero de
  sakta hai (`examples/04` mein measured: `memcmp == -1`).

---

## Andar kya hota hai

- **Automatic object** (`int x;` ek function mein) — storage stack frame ke andar,
  ek offset pe (`[rbp-4]`). Koi runtime "create" instruction nahi (trivial type) —
  bas frame allocate hone pe woh bytes uske ban jaate.
- **`new Widget`** — `operator new(sizeof(Widget))` (heap se storage) → uspe
  constructor → pointer return.
- **Object identity** = address + type + lifetime. Do objects same address pe *ek
  saath* exist nahi kar sakte (union/reuse ke alawa — file 02).
- Compiler ke liye "object" ek abstraction hai jo optimization ke rules define
  karta — e.g. "is object ko dobara load karna padega ya cached value chalegi"
  (aliasing — file 10).

---

## > **HFT relevance**
> Yeh definition abstract lagti hai par iske **practical consequences** har hot
> path pe hain:
> - **"Region of storage" model** = aap pre-allocated storage (arena, pool, ring
>   buffer) mein objects ka lifetime **manually** manage kar sakte ho (placement
>   new — file 09), heap ko chhue bina.
> - **Trivially-copyable objects = raw bytes** — `memcpy` a `MarketUpdate` into a
>   ring buffer, `bit_cast` a POD out of a wire buffer (file 06, 10).
> - **Padding bytes indeterminate** — `memcmp`/hash a struct ke liye
>   `has_unique_object_representations` chahiye (no padding), warna wire structs
>   pe `#pragma pack` + explicit fields (file 07).
> - **Object lifetime rules** batate hain kab ek reference/pointer dangling hai —
>   ek poora class of latency-spike-in-testing bugs (file 02, 05).

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/01_lifetime_demo.cpp
```

Example: automatic object (scope), manual object in a raw buffer, storage reuse
(3 objects, ek buffer), ek buffer mein alag type ka object. Phir:
- `struct S { int a; char b; };` — `sizeof(S)`, `alignof(S)`, `offsetof(S, b)`,
  aur `(void*)&s == (void*)&s.a` check karo.
- Ek `int a[4]` — `&a`, `&a[0]`, `&a[1] - &a[0]` (stride = `sizeof(int)`).

---

## ⚠️ Traps

### Trap 1 — "object" aur "variable" ko interchangeable maanna
`new int` ek object banata hai, koi variable nahi. Array `a[10]` = 1 named object +
10 unnamed. Temporaries objects hain bina naam ke.

### Trap 2 — reference ko object samajhna
```cpp
int x = 5;
int& r = x;          // r koi naya object nahi — x ka alias
&r == &x;             // true — same object
sizeof(r) == sizeof(int);   // reference ka sizeof = referent ka
```

### Trap 3 — `sizeof` value representation samajhna
`sizeof(T)` = object representation (with padding). Value ke liye kam bytes lagte
(file 07).

### Trap 4 — empty class ka `sizeof` 0 expect karna
`sizeof` hamesha `>= 1` — do alag objects ke alag addresses chahiye. `1` (ya EBO se
0 as a base subobject).

### Trap 5 — do objects same storage pe ek saath
```cpp
int x;
new (&x) float(1.5f);   // ab `x` ki lifetime khatam, float ki shuru (file 02)
x + 1;                    // ⚠️ UB — `x` object ab nahi hai
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "object = variable" | Object = region of storage; variable = a *named* object/reference |
| "reference ek object hai" | Reference = ek existing object ka alias; storage optional |
| "function ek object hai" | Nahi — functions objects nahi (function *pointers* hain) |
| "`sizeof` = value ke bytes" | Object representation — includes padding |
| "empty struct ka `sizeof` 0" | `>= 1` always (distinct addresses); 0 only as an EBO base subobject |
| "ek address pe ek hi type ka object ho sakta" | Storage reuse se alag type ka object ban sakta (file 02) |

---

## Exercises

1. **Count the objects:** `int a[3] = {1, 2, 3};` — kitne objects, kitne named?

   <details><summary>Answer</summary>

   4 objects: the array `a` itself (1, named) + `a[0]`, `a[1]`, `a[2]` (3,
   unnamed). `sizeof(a) == 3 * sizeof(int)`.
   </details>

2. **Object or not:** `int x; int& r = x; int* p = &x; int f();` — inme se kaun
   object hai?

   <details><summary>Answer</summary>

   `x` — object. `r` — not an object (alias for `x`). `p` — an object (a pointer,
   has storage, holds an address). `f` — not an object (a function).
   </details>

3. **Address equality:** `struct S { int a; double b; };` standard-layout hai. `S
   s;` — `(void*)&s == (void*)&s.a`? `(void*)&s == (void*)&s.b`?

   <details><summary>Answer</summary>

   `&s == &s.a` — **yes** (first member of a standard-layout type shares the
   object's address). `&s == &s.b` — **no** (`b` is at `offsetof(S, b)`, likely 8).
   </details>

4. **`sizeof` reasoning:** `struct E {};` — `sizeof(E)`? `struct F : E { int x; };`
   — `sizeof(F)`?

   <details><summary>Answer</summary>

   `sizeof(E) == 1` (empty, needs a distinct address). `sizeof(F) == 4` — the
   empty base `E` is optimized away (EBO); `F` is just the `int`.
   </details>

5. **Subobjects:** `struct Line { struct P { int x, y; } a, b; };` — list every
   object in `Line L;`.

   <details><summary>Answer</summary>

   `L` (complete object) ; `L.a`, `L.b` (member subobjects of type `P`) ; `L.a.x`,
   `L.a.y`, `L.b.x`, `L.b.y` (int subobjects). 7 objects total.
   </details>

---

## Interview questions

1. C++ mein "object" ki definition — standard ke hisaab se.
2. Object vs variable vs reference — teen alag cheezein.
3. Kaunse cheezein objects nahi hain (functions, references)?
4. Object ke pass kaunsi properties hoti hain (storage / type / lifetime / value)?
5. Value representation vs object representation — farq, ek consequence.
6. Empty class ka `sizeof` kyun `>= 1`? EBO ka kya rishta?
7. Subobject kya hai — member / base / array element.
8. Kya ek address pe do objects ho sakte hain (storage reuse)?

---

## Next
→ [`02-object-lifetime.md`](02-object-lifetime.md)
