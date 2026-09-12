# 06 — Trivial, standard-layout, POD

## Prerequisites
- `01-what-is-an-object.md`, `15-CLASSES` (special members), `16-OOP` (inheritance, vtable)
- [`examples/04_type_properties.cpp`](examples/04_type_properties.cpp)

## Yeh topic abhi kyun
Yeh teen properties decide karti hain ki aap ek type pe kaunse **low-level
operations** legally kar sakte ho: `memcpy` the bytes? `offsetof` a member? `memcmp`
for equality? `malloc` + use without a constructor? HFT mein — ring buffers, wire
serialization, hashing structs — inpe sab time depend karta hai.

---

## Ek picture

```
                     is_trivial            is_standard_layout
                     (default-init +       (predictable layout,
                      copy/move/dtor        C-compatible)
                      are trivial)
                          │                        │
                          └──────────┬─────────────┘
                                     ▼
                          POD (C++ ≤ 17 term)
                          = trivial AND standard-layout
                     (C++20 mein "POD" concept remove — dono alag se use karo)
```

Plus do aur useful:
- **`is_trivially_copyable`** — copy/move ctor/assign + dtor sab trivial (par
  default ctor non-trivial ho sakta) → **`memcpy` the bytes**.
- **`has_unique_object_representations`** — no padding, no `bool`-with-slack, no
  float → **`memcmp`/hash the bytes**.

---

## Trivial

Ek class **trivial** hai agar:
- Trivial (compiler-generated, kuch special nahi) **default constructor**, AND
- Trivial **copy/move constructors**, **copy/move assignment**, AND
- Trivial **destructor**, AND
- No virtual functions, no virtual bases.

Trivial default ctor = "koi code nahi chalta" — `int x;` ki t/ raw storage
`.bss` mein ek trivial object bin ctor ban jaata.

```cpp
struct A { int x, y; };            // trivial  ✅
struct B { int x; B() : x(0) {} }; // NOT trivial (user default ctor) — but trivially_copyable
struct E { int id; std::string s; };  // NOT trivial (std::string members non-trivial)
```

**Kya milta:** raw storage se object "bina ctor" — `malloc` + use (C++20
implicit-lifetime), `.bss` placement, uninitialized arrays.

---

## Trivially copyable

- Trivial (or deleted) copy ctor, move ctor, copy assign, move assign, AND
- Trivial (non-deleted) destructor.

**Default ctor trivial hona zaroori NAHI.** `B` (user default ctor) abhi bhi
trivially-copyable hai.

```cpp
static_assert(std::is_trivially_copyable_v<B>);      // ✅ (examples/04)
```

**Kya milta:** **`std::memcpy(&dst, &src, sizeof)` legal** — copy the object
representation byte-for-byte. Serialization, ring buffers, `std::bit_cast` (file
10). `std::string` inside → **NOT** trivially-copyable → `memcpy` = UB (would
duplicate the internal pointer → double-free).

---

## Standard-layout

Ek class **standard-layout** hai agar (roughly):
- **Saare non-static data members ka access same** (sab `public`, ya sab `private`,
  ya sab `protected`), AND
- No virtual functions, no virtual bases, AND
- **Data members sirf ek class mein** — ya derived mein ya base mein, dono mein
  nahi, AND
- First non-static data member koi base-class type nahi (kuch cases), AND
- No two base subobjects of the same type, etc.

```cpp
struct D { int pub; private: int priv; };   // NOT standard-layout (mixed access)
struct GB { int a; }; struct G : GB { int b; };  // NOT (data in both base and derived)
struct F { virtual ~F(); };                  // NOT (virtual)
```

**Kya milta:**
- **`offsetof(T, member)`** legal aur meaningful.
- **First member ka address == object ka address** — `(void*)&s == (void*)&s.first`.
- **C struct ke saath layout-compatible** (same members, same order) → C interop,
  `extern "C"` structs, wire formats.
- Reinterpret between two standard-layout types sharing a "common initial sequence"
  — narrow, careful (file 11).

---

## POD (legacy term)

**POD = trivial AND standard-layout.** "Plain Old Data" — ek C struct jitna simple.
C++20 mein "POD" ko standard se **hata diya** — ab `is_trivial` + `is_standard_layout`
alag-alag use karo (zyada precise). `std::is_pod` deprecated (C++20).

---

## `has_unique_object_representations`

`true` agar type ke **har value ka exactly ek byte pattern** ho — no padding
bytes, no unused bits.

```cpp
struct A { int x, y; };       // ✅ (no padding — 8 bytes fully used)
struct C { char c; int i; };  // ❌ (3 padding bytes after `c` — indeterminate)
double d;                     // ❌ (multiple NaN patterns, ±0)
```

**Kya milta:** **`memcmp` for equality** aur **hash the raw bytes** — bina
"padding bytes garbage → jhoota not-equal" ke. `examples/04` mein measured:
`C c1, c2` with same field values but no `memset` first → **`memcmp == -1`**
(padding differed).

---

## Ek table (examples/04 ka output)

| Type | trivial | triv-copy | std-layout | aggregate | uniq-obj-rep |
|---|---|---|---|---|---|
| `int` | ✅ | ✅ | ✅ | — | ✅ |
| `double` | ✅ | ✅ | ✅ | — | ❌ (±0, NaNs) |
| `struct{int,int}` | ✅ | ✅ | ✅ | ✅ | ✅ |
| `+ user default ctor` | ❌ | ✅ | ✅ | ❌ | ✅ |
| `struct{char,int}` (pad) | ✅ | ✅ | ✅ | ✅ | ❌ |
| mixed public/private | ❌ | ✅ | ❌ | ❌ | ✅ |
| `struct{int,std::string}` | ❌ | ❌ | ✅ | ✅ | ❌ |
| `struct{virtual ~}` | ❌ | ❌ | ❌ | ❌ | ❌ |
| `Derived` (data in base+derived) | ✅ | ✅ | ❌ | ✅ | ✅ |

---

## Andar kya hota hai

- Yeh sab **compile-time type traits** — `std::is_trivial_v<T>` etc. compiler
  intrinsics (`__is_trivial`, `__is_standard_layout`, `__has_unique_object_representations`)
  se compute hote. Zero runtime.
- **Trivially-copyable** → compiler internally already `memcpy` use karta hai us
  type ke copy/move ke liye (loop nahi). `std::copy` of a trivially-copyable range
  → `memmove`.
- **Standard-layout** → compiler layout ko C ABI ke rules se karta (members in
  order, natural alignment, tail padding) — no reordering freedom (non-std-layout
  mein compiler thoda freedom le sakta, e.g. multiple empty bases).
- `has_unique_object_representations` → compiler check karta ki `sizeof ==` sum of
  member sizes (no padding) recursively, aur no `bool`/float/pointer-with-slack.

---

## > **HFT relevance**
> - **Wire / IPC structs** — `static_assert(std::is_trivially_copyable_v<MdPacket>)`
>   + `static_assert(std::is_standard_layout_v<MdPacket>)` + `static_assert(sizeof
>   == N)` + `offsetof` checks. Then `memcpy`/`bit_cast` in and out of byte buffers
>   safely (file 10, folder 38).
> - **Ring buffers of messages** — slots hold trivially-copyable types → `memcpy`
>   into place, no per-element ctor, `std::atomic` publish. If a message needs a
>   `std::string`, that breaks trivial-copyability → use a fixed `char[N]` +
>   length, or an index into a pool.
> - **Hashing / dedup a struct key** — need `has_unique_object_representations`
>   (no padding). Design the key struct so fields pack tight (largest first, or
>   `#pragma pack`), then `std::hash` over the bytes / a `memcmp` comparator.
> - **`offsetof` for zero-copy field access** into a raw buffer — needs
>   standard-layout.
> - **Trivially-destructible types in arenas** — no need to run dtors on teardown;
>   just reset the arena pointer (folder 36).

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/04_type_properties.cpp
```

The trait matrix + which operation each property unlocks (`memcpy`, byte
round-trip, `memcmp`, `offsetof`) — including the **`memcmp == -1` without
`memset` first** (padding bytes). Then:
- Add a `struct MdTick { std::uint64_t ts; std::int64_t px; std::uint32_t qty;
  std::uint8_t side; };` — check `sizeof`, `is_trivially_copyable`,
  `has_unique_object_representations` (fails — padding after `side`). Add explicit
  padding or reorder; re-check.

---

## ⚠️ Traps

### Trap 1 — `memcpy` a type with a `std::string`/`std::vector`/`unique_ptr` member
```cpp
struct E { int id; std::string name; };
std::memcpy(&e2, &e1, sizeof(E));   // ⚠️ UB — copies the internal pointer -> double free
```
Only `is_trivially_copyable` types.

### Trap 2 — `memcmp` a struct with padding for equality
```cpp
struct C { char c; int i; };
std::memcmp(&c1, &c2, sizeof(C)) == 0   // ⚠️ may be non-zero even if c/i match — padding
```
`memset` to zero first, or use `has_unique_object_representations` types, or
compare member-wise (`operator==` / `= default` `<=>`).

### Trap 3 — `offsetof` on a non-standard-layout type
Conditionally-supported (may warn / be UB). Only on standard-layout types.

### Trap 4 — "trivial" == "trivially copyable"
Different. A user default ctor breaks *trivial* but not *trivially-copyable*.

### Trap 5 — mixing access specifiers breaks standard-layout silently
Adding one `private:` to an otherwise all-public struct → not standard-layout →
`offsetof` / C interop assumptions break.

### Trap 6 — relying on `is_pod` (C++20)
Deprecated. Use `is_trivial && is_standard_layout` (or just the one you need).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "trivial = trivially copyable" | Trivial also needs a trivial default ctor; triv-copy doesn't |
| "any struct can be `memcpy`'d" | Only `is_trivially_copyable` — no owning members, no vtable |
| "same field values → `memcmp` returns 0" | Padding bytes are indeterminate; need `has_unique_object_representations` |
| "standard-layout = trivial" | Independent — a type can be one, both, or neither |
| "POD is still a thing in C++20" | Removed as a concept; use the individual traits |
| "`offsetof` works on anything" | Standard-layout only |

---

## Exercises

1. **Classify:** for `struct S { int a; private: int b; public: S(); };` — trivial?
   trivially-copyable? standard-layout?

   <details><summary>Answer</summary>

   Trivial — no (user default ctor). Trivially-copyable — yes (copy/move/dtor are
   trivial). Standard-layout — no (mixed `public`/`private` data members).
   </details>

2. **`memcpy` legality:** which can be safely `memcpy`'d as bytes? (a) `struct{int
   x[4];}` (b) `struct{std::array<int,4> a;}` (c) `struct{std::string s;}`
   (d) `struct{int x; double y;}`

   <details><summary>Answer</summary>

   (a), (b), (d) — trivially copyable (`std::array` of trivial type is trivially
   copyable). (c) — not (owns heap via `std::string`).
   </details>

3. **`memcmp` trap:** `struct K { std::uint16_t a; std::uint64_t b; };` — is
   `memcmp` a valid equality check? Why / fix?

   <details><summary>Answer</summary>

   No — 6 bytes of padding after `a` (to align `b` to 8) are indeterminate, so two
   `K`s with equal `a`/`b` can `memcmp` non-zero. Fix: reorder (`b` first, then
   `a` — still 6 pad at the tail... actually `{u64 b; u16 a;}` is 16 bytes with 6
   tail pad — same issue). Best: `memset` the key to 0 before filling, or compare
   member-wise, or make it `has_unique_object_representations` by packing to 10
   bytes (`#pragma pack(1)`) — but then watch alignment.
   </details>

4. **Standard-layout use:** you want `(char*)&pkt + offsetof(Pkt, price)` to point
   at the price field of a raw buffer. What must `Pkt` be?

   <details><summary>Answer</summary>

   Standard-layout (so `offsetof` is valid and the layout matches the wire
   format) — and ideally also `is_trivially_copyable` so you can `memcpy` the
   whole thing. Add `static_assert`s for both plus `sizeof`.
   </details>

5. **Arena teardown:** an arena holds 10^6 objects of type `T`. When can you skip
   running destructors on teardown (just reset the pointer)?

   <details><summary>Answer</summary>

   When `std::is_trivially_destructible_v<T>` — no destructor side effects to run.
   For non-trivially-destructible `T` you must destroy each (or track and destroy
   them), or you leak whatever they own.
   </details>

---

## Interview questions

1. Trivial vs trivially-copyable — farq, ek example jahan sirf ek true hai.
2. Standard-layout — kya requirements, kya unlock karta (`offsetof`, C interop)?
3. POD — C++20 mein kya hua?
4. `has_unique_object_representations` — kis operation ke liye zaroori, kyun?
5. `memcpy` a struct — kaunsi property, aur `std::string` member kyun todta hai?
6. `memcmp` for equality — kab galat, kaise fix?
7. Mixed access specifiers ka standard-layout pe kya asar?

---

## Next
→ [`07-object-representation.md`](07-object-representation.md)
