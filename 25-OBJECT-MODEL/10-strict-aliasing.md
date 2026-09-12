# 10 — Strict aliasing rule

## Prerequisites
- `01-what-is-an-object.md`, `07-object-representation.md`
- `19-STL` file 22 (`std::bit_cast`), `24-COMPILATION-LINKING` file 04
- [`examples/06_strict_aliasing.cpp`](examples/06_strict_aliasing.cpp)

## Yeh topic abhi kyun
Strict aliasing = "compiler maan sakta hai ki alag types ke pointers **same memory
ko point nahi karte**." Yeh assumption powerful optimization deta hai (loads
reorder/reuse, no redundant re-load) — par agar aap ek object ko galat type ke
pointer se access karo, to **UB**, aur `-O2` pe stale values / wrong answers.
`std::bit_cast` aur `std::memcpy` iske around ka safe rasta hain.

---

## The rule

Ek object ki stored value ko **sirf in glvalue types se** access karna legal hai
(`[basic.lval]`):

1. Object ka **actual (dynamic) type** — `T` object ko `T&`/`T*` se.
2. Us type ka **cv-qualified** version — `T` ko `const T*` se.
3. Ek **signed/unsigned** variant — `int` ko `unsigned int*` se (kuch cases).
4. Ek type jismein woh object **member** hai (aggregate/union), recursively.
5. A base class type.
6. **`char`, `unsigned char`, ya `std::byte`** — YEH kisi bhi object ke bytes ko
   access kar sakte (the serialization escape hatch).

**Sab kuch aur = UB.** `float` object ko `int*` se padhna — UB. Ek struct ko ek
alag struct ke pointer se — UB. `double` ko `long long*` se — UB.

---

## Kya BREAK hota hai

```cpp
// ⚠️ UB — float object, int glvalue
static std::uint32_t bad(float f) {
    return *reinterpret_cast<std::uint32_t*>(&f);
}
```

Simple function mein compiler aksar "sahi" answer de deta (luck, guarantee nahi).
Real damage tab jab aliasing assumption ko **optimize karne** ka mauka mile:

```cpp
static int trap(int* ip, long* lp) {
    int a = *ip;          // load *ip
    *lp = 0x11223344L;    // write *lp
    int b = *ip;          // -O2 + strict-aliasing: compiler MAAN sakta b == a
    return b - a;         // -> "hamesha 0" bhale ip == (int*)lp
}
```

`examples/06` mein measured:
- `-O0`: `delta` = the real difference (`*ip` actually changed).
- `-O2 -fstrict-aliasing`: `delta = 0` — compiler ne pehla load reuse kiya, "jaan
  ke" ki `int*` aur `long*` alias nahi karte.

**Same source, same input, alag answer** — that's UB biting.

---

## Kya LEGAL hai — type punning ke sahi tareeke

### 1. `std::bit_cast<To>(from)` (C++20) — the intended tool

```cpp
#include <bit>
std::uint32_t bits = std::bit_cast<std::uint32_t>(3.14f);   // ✅
float back        = std::bit_cast<float>(bits);             // ✅
```

- `sizeof(To) == sizeof(From)` zaroori.
- Both trivially-copyable.
- `constexpr`-friendly.
- Compiler ise ek register move / no-op mein compile karta — zero cost.

### 2. `std::memcpy` — always legal, any size

```cpp
std::uint32_t out;
std::memcpy(&out, &f, sizeof out);   // ✅ memcpy "sees bytes"
```

- Works for partial copies, arrays, mismatched sizes.
- Compiler optimizes a small fixed-size `memcpy` into a plain load/store — **no
  actual function call, no cost** at `-O2`.

### 3. `char` / `unsigned char` / `std::byte` pointer — read bytes

```cpp
const auto* p = reinterpret_cast<const unsigned char*>(&obj);
for (std::size_t i = 0; i < sizeof(obj); ++i) hash ^= p[i];   // ✅ legal
```

This is why serialization, hashing, and hex-dumps are OK.

### 4. Placement new to actually *change* the type at an address (file 09)

```cpp
alignas(std::max_align_t) unsigned char box[8];
auto* n = ::new (box) std::int64_t{42};      // now an int64 object lives there
```

---

## `union` type punning — C vs C++

```cpp
union U { float f; std::uint32_t i; };
U u; u.f = 3.14f;
std::uint32_t bits = u.i;      // C: implementation-defined. C++: reading the
                               //    INACTIVE member is UB (only the last-written
                               //    member is "active").
```

- **C**: reading a different union member is implementation-defined (common
  extension, widely relied on).
- **C++**: technically UB — only the active member. GCC/Clang support it as an
  extension, but **`std::bit_cast` / `memcpy` are the portable, standard way.**

---

## `-fstrict-aliasing` vs `-fno-strict-aliasing`

- **`-fstrict-aliasing`** — default at `-O2`/`-O3`. Compiler exploits the rule →
  better codegen, but your aliasing violations become visible bugs.
- **`-fno-strict-aliasing`** — tells the compiler "assume anything can alias
  anything." Makes broken code "work" — but it's a **crutch**, disables real
  optimizations, and the code is still non-portable. (The Linux kernel builds with
  this because of decades of legacy punning.)
- **The fix is `bit_cast`/`memcpy`, not a flag.**

`-Wstrict-aliasing` (with `-O2`) warns on some violations — not all. `-fsanitize=
undefined` doesn't catch aliasing directly; TBAA (type-based alias analysis) bugs
are among the hardest to detect.

---

## Andar kya hota hai

- The compiler builds an **alias analysis** graph: which memory accesses *might*
  refer to the same location. Under strict aliasing, two accesses of
  incompatible types are assumed **not** to alias → the optimizer can:
  - Keep a value in a register across an unrelated-typed store (no re-load).
  - Reorder a load before an unrelated-typed store.
  - Vectorize a loop that writes `float*` and reads `int*` without a dependency.
- `char*`/`unsigned char*`/`std::byte*` accesses **can** alias anything → they act
  as optimization barriers (a `memcpy` through them forces the compiler to
  materialize memory).
- `std::bit_cast` / `__builtin_bit_cast` → the compiler treats it as "copy the
  bits" with no aliasing implication; at `-O2` it's a `mov` (or nothing).
- `-fno-strict-aliasing` → the alias analysis conservatively assumes everything
  aliases → many optimizations disabled.

---

## > **HFT relevance**
> - **Wire / market-data parsing** — pull a `std::uint64_t` timestamp or a POD
>   header out of a `std::span<const std::byte>`: `std::bit_cast` (fixed-size POD)
>   or `std::memcpy` (variable) — **never** `reinterpret_cast<Header*>(buf)` +
>   deref. That's UB and the optimizer *will* eventually miscompile it under `-O2
>   -flto -march=native` (folder 38).
> - **Both are zero-cost at `-O2`** — a fixed-size `memcpy`/`bit_cast` compiles to
>   the same load/store you'd write by hand. No excuse to use the UB form.
> - **`-fno-strict-aliasing` is a code smell in a latency codebase** — it disables
>   optimizations you want. If you need it, you have punning bugs to fix.
> - **`char`/`std::byte` for the buffer type** — parse functions take
>   `std::span<const std::byte>`; the byte-pointer aliasing exception makes reading
>   the buffer legal, and `bit_cast`/`memcpy` extract typed values.

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/06_strict_aliasing.cpp        # -O0: all match
g++ -std=c++20 -O2 -fstrict-aliasing 25-OBJECT-MODEL/examples/06_strict_aliasing.cpp -o sa && ./sa
g++ -std=c++20 -O2 -fno-strict-aliasing 25-OBJECT-MODEL/examples/06_strict_aliasing.cpp -o sa2 && ./sa2
```

Compare `aliasing_trap`'s `delta`: real at `-O0` and `-fno-strict-aliasing`, but
**`0`** at `-O2 -fstrict-aliasing` (the load was reused). Then:
- `g++ -O2 -S` the file, find `trap` — see the single load of `*ip`.
- Replace the UB pun with `std::bit_cast` — the `-O2` divergence goes away, same speed.

---

## ⚠️ Traps

### Trap 1 — `reinterpret_cast<OtherType*>(&x)` then deref
```cpp
float f = 1.5f;
std::uint32_t b = *reinterpret_cast<std::uint32_t*>(&f);   // ⚠️ UB
std::uint32_t b = std::bit_cast<std::uint32_t>(f);         // ✅
```

### Trap 2 — reading an inactive union member (C++)
```cpp
union { float f; int i; } u; u.f = 1.0f; int x = u.i;   // ⚠️ UB in C++ (fine-ish in C)
```

### Trap 3 — `-fno-strict-aliasing` as "the fix"
It hides the bug and costs performance. Fix the pun.

### Trap 4 — casting a `char[]` buffer to a struct pointer
```cpp
Header* h = reinterpret_cast<Header*>(buf);   // ⚠️ no Header object there -> UB
h->field;
Header h; std::memcpy(&h, buf, sizeof h);      // ✅ (Header trivially copyable)
```

### Trap 5 — assuming "it worked at `-O0`" means it's fine
Aliasing UB shows up at `-O2`+ with TBAA. `-O0` is not a test.

### Trap 6 — `bit_cast` with mismatched sizes
`std::bit_cast<std::uint64_t>(someFloat)` — compile error (`sizeof` mismatch). Sizes
must match exactly.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "you can read any object through any pointer type" | Only the actual type, cv/sign variants, enclosing types, or `char`/`byte` |
| "`union` punning is fine in C++" | Reading the inactive member is UB (extension in GCC/Clang); use `bit_cast` |
| "`-fno-strict-aliasing` fixes aliasing bugs" | It masks them and disables optimizations; fix the code |
| "`memcpy` for type punning is slow" | A fixed-size `memcpy` compiles to a load/store at `-O2` — free |
| "works at `-O0` so it's legal" | Aliasing UB manifests at `-O2`+ |
| "`char*` access to any object is UB" | It's the *exception* — always legal (serialization) |

---

## Exercises

1. **Legal or UB:** (a) `int i; char* c = (char*)&i; c[0] = 0;`
   (b) `int i; float* f = (float*)&i; *f = 1.0f;`
   (c) `struct A{int x;}; struct B{int x;}; A a; B* b = (B*)&a; b->x;`
   (d) `int i; unsigned* u = (unsigned*)&i; *u = 5;`

   <details><summary>Answer</summary>

   (a) Legal — `char*` access to any object. (b) UB — `float` glvalue for an `int`
   object. (c) UB — `A` and `B` are different types even with identical layout.
   (d) Borderline/allowed — `int`↔`unsigned int` is one of the sanctioned
   sign-variant cases for *reading*; writing is also generally OK. Prefer
   `bit_cast` anyway.
   </details>

2. **Rewrite:** `float half(float x) { std::uint32_t b = *(std::uint32_t*)&x; b -=
   (1u << 23); return *(float*)&b; }` — make it legal.

   <details><summary>Answer</summary>

   ```cpp
   float half(float x) {
       auto b = std::bit_cast<std::uint32_t>(x);
       b -= (1u << 23);                 // subtract 1 from the exponent
       return std::bit_cast<float>(b);
   }
   ```
   (This exponent trick only works for normal, positive `x`; the point is the
   `bit_cast`.)
   </details>

3. **Explain the `0`:** in `examples/06`'s `aliasing_trap`, why does `-O2
   -fstrict-aliasing` report `delta == 0`?

   <details><summary>Answer</summary>

   The compiler assumes `int*` and `long*` can't alias, so the store `*lp = ...`
   can't affect `*ip`. It reuses the first load of `*ip` for `b`, making `b == a`
   and `delta == 0` — even though the pointers actually point at the same bytes
   (which is itself UB).
   </details>

4. **Buffer → struct:** you receive `std::span<const std::byte> buf` and need a
   `MdHeader` (trivially copyable, `sizeof == 16`) from its first 16 bytes.
   Correct code.

   <details><summary>Answer</summary>

   ```cpp
   MdHeader h;
   std::memcpy(&h, buf.data(), sizeof h);   // buf.size() >= 16 checked first
   ```
   (Or `std::bit_cast<MdHeader>(...)` if you have exactly 16 bytes in an array.)
   Not `reinterpret_cast<const MdHeader*>(buf.data())->field` — no `MdHeader`
   object lives in `buf`.
   </details>

5. **When is `-fno-strict-aliasing` justified?** Give one real case and the cost.

   <details><summary>Answer</summary>

   A large legacy codebase (e.g. the Linux kernel) full of union/pointer punning
   that can't all be rewritten safely — building with `-fno-strict-aliasing` is
   pragmatic. Cost: the compiler must assume broad aliasing → fewer loads kept in
   registers, less reordering/vectorization → measurably slower code in
   memory-heavy loops.
   </details>

---

## Interview questions

1. Strict aliasing rule — kaunse glvalue types se ek object access kar sakte ho?
2. `char`/`unsigned char`/`std::byte` ka special role.
3. Type punning ka sahi tarika — `std::bit_cast` vs `std::memcpy`, kab kaunsa.
4. `union` punning — C vs C++.
5. `-fno-strict-aliasing` — kya karta, kyun crutch hai?
6. `examples/06`'s `aliasing_trap` — `-O2` pe `delta == 0` kyun?
7. Fixed-size `memcpy` ki cost `-O2` pe (≈ 0) — kyun?

---

## Next
→ [`11-type-punning.md`](11-type-punning.md)
