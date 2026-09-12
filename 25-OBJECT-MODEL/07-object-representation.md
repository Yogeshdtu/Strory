# 07 — Object representation, padding, value representation

## Prerequisites
- `06-trivial-standard-layout-pod.md`, `11-STRUCTS` (padding basics), `05-OPERATORS` (bit ops)
- [`examples/04_type_properties.cpp`](examples/04_type_properties.cpp)

## Yeh topic abhi kyun
Ek object ke `sizeof(T)` bytes mein sab "value" nahi hote — kuch **padding** hote
hain jinki value **indeterminate** hai. Yeh `memcmp`, hashing, serialization,
`memcpy` sab ko affect karta hai. Aur "why is my `struct` 16 bytes when the fields
add up to 10" ka jawab yahin hai.

---

## Do representations

| | Kya | Bytes |
|---|---|---|
| **Object representation** | `sizeof(T)` bytes — the whole storage footprint | includes padding |
| **Value representation** | woh bits jo *value* determine karte | subset — no padding |

```cpp
struct T { char c; int i; };   // sizeof(T) == 8
// object representation: 8 bytes  = [c][pad][pad][pad][i0][i1][i2][i3]
// value representation:  5 bytes  = c + i  (the 3 pad bytes don't count)
```

**Padding bytes ki value indeterminate hai** — compiler unhe kuch bhi chhod sakta,
copy pe preserve karna zaroori nahi (`memcpy` of the object representation *does*
copy them, but their content is unspecified).

---

## Padding kyun

### Alignment (file 08)
Har type ka ek alignment requirement — `int` usually 4, `double` usually 8. Ek
member ka offset uske alignment ka multiple hona chahiye.

```cpp
struct T {
    char c;    // offset 0
    // 3 bytes padding  <- `i` ko offset 4 (multiple of 4) pe laane ke liye
    int i;     // offset 4
};              // sizeof 8
```

### Tail padding
`sizeof(T)` bhi `alignof(T)` ka multiple hota — taaki `T arr[N]` mein har element
aligned rahe.

```cpp
struct U {
    int  i;    // offset 0
    char c;    // offset 4
    // 3 bytes tail padding  <- sizeof ko 8 (multiple of alignof(int)=4)... 
                              //    actually multiple of 4 -> sizeof 8
};
```

### Member reordering se kam padding
```cpp
struct Bad  { char a; int b; char c; };   // 1 + 3pad + 4 + 1 + 3pad = 12
struct Good { int b; char a; char c; };   // 4 + 1 + 1 + 2pad       = 8
```
Largest members pehle → less padding. Folder 11 mein measured (~30-45% struct-array
memory + cache impact).

---

## Kya break hota padding se

### 1. `memcmp` for equality — jhoota mismatch
```cpp
struct C { char c; int i; };
C a, b;
a.c = 'x'; a.i = 42;
b.c = 'x'; b.i = 42;
std::memcmp(&a, &b, sizeof(C));   // ⚠️ may be non-zero — padding bytes differ
```
`examples/04` mein: **`memcmp == -1`** (padding was uninitialized garbage).

**Fixes:**
- `std::memset(&x, 0, sizeof x)` before filling (padding zeroed) — then `memcmp`
  works if you're careful.
- Compare **member-wise** (`operator==`, `= default` `<=>`).
- Use a type with `has_unique_object_representations` (no padding).

### 2. Hashing the raw bytes — inconsistent hashes
Same value, different padding → different hash. Same fix set.

### 3. `memcpy` copies padding (harmless-ish, but not meaningful)
`memcpy` of the object representation is fine for trivially-copyable types; it
copies padding too, but you can't rely on the padding content.

### 4. `union` — reading through a smaller member leaves "extra" bytes
```cpp
union U { std::int64_t all; std::int32_t lo; };
U u; u.lo = 5;
// u.all — the high 4 bytes are indeterminate (in C++ reading u.all is UB anyway)
```

### 5. Uninitialized reads of padding are... fine to read as `unsigned char`
Reading a padding byte via `unsigned char*` is not UB (it's storage), but the
value is unspecified.

---

## Seeing the bytes

```cpp
template <class T>
void dump(const T& obj) {
    const auto* p = reinterpret_cast<const unsigned char*>(&obj);
    for (std::size_t i = 0; i < sizeof(T); ++i)
        std::printf("%02x ", p[i]);
    std::puts("");
}
```

`reinterpret_cast<const unsigned char*>` is **always legal** (the aliasing
exception — file 10). This is how serialization, hex dumps, and hashing see an
object.

---

## Bit-fields — a sub-byte layout

```cpp
struct Flags {
    unsigned ready : 1;
    unsigned mode  : 3;
    unsigned       : 0;   // force next field to a new allocation unit
    unsigned seq   : 12;
};
```
- Layout of bit-fields is **implementation-defined** (order, straddling,
  allocation unit). Don't use for wire formats across compilers.
- `sizeof` / `alignof` still apply to the underlying storage.
- `offsetof` doesn't work on bit-fields; can't take their address.

---

## Andar kya hota hai

- Compiler layout algorithm (Itanium ABI): members in declaration order, each at
  the next offset that satisfies its alignment (insert padding), then round
  `sizeof` up to `alignof(T)` (tail padding).
- `[[no_unique_address]]` (C++20) — an empty member can share/overlap padding or
  another member's tail → smaller structs (EBO for members).
- Non-standard-layout types: the compiler has *some* freedom (e.g. reusing an
  empty base's tail padding for a derived member) — `sizeof` can be smaller than
  the "naive" sum.
- `-Wpadded` — warns wherever the compiler inserts padding (noisy, but useful for
  auditing hot structs).

---

## > **HFT relevance**
> - **Hot structs: hand-order fields largest→smallest** to minimize padding →
>   smaller footprint → more per cache line → fewer misses (folder 32). Measured
>   in folder 11 (~30-45%).
> - **`static_assert(sizeof(T) == N)` + `offsetof` asserts** on every hot / wire /
>   ABI struct — catches "someone added a field and the layout drifted" at compile
>   time (folder 23 file 12, folder 24 file 04).
> - **Never `memcmp` a padded struct for order-book key equality** — compare
>   member-wise, or design the key to be `has_unique_object_representations`
>   (packed, no float), then a byte comparator / hash is valid.
> - **`memset` a message struct to 0 before filling** if you'll `memcmp`/hash it
>   or send its raw bytes (deterministic padding).
> - **`-Wpadded` in a periodic audit build** to see where hot structs waste space.

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/04_type_properties.cpp
```

Look at section 3 — `memcmp` with and without `memset` first (the `-1`). Then:
- Write a `dump<T>()` helper (above) and dump a `struct C { char c; int i; };` —
  see the 3 garbage padding bytes.
- `g++ -Wpadded` on a struct with mixed-size fields — see the padding warnings;
  reorder to eliminate them.
- `struct MdTick { std::uint64_t ts; std::int64_t px; std::uint32_t qty;
  std::uint8_t side; };` — `sizeof`, `has_unique_object_representations` (fails),
  add `std::uint8_t _pad[3]` or reorder, re-check.

---

## ⚠️ Traps

### Trap 1 — `memcmp` for struct equality
Padding bytes indeterminate → false mismatches. Member-wise compare, or
`has_unique_object_representations` + `memset`-first discipline.

### Trap 2 — hashing raw struct bytes with padding
Same value, different hash. Same fix.

### Trap 3 — assuming `sizeof` == sum of member sizes
Alignment padding + tail padding. `struct{char;int;}` is 8, not 5.

### Trap 4 — bit-field layout for wire formats
Implementation-defined order/straddling. Use explicit shifts/masks over a
fixed-width integer.

### Trap 5 — reading padding as the field's type
```cpp
int* pad = reinterpret_cast<int*>(reinterpret_cast<char*>(&c) + 1);  // ⚠️ not an int object
```
Padding is storage, not an object of any type.

### Trap 6 — `[[no_unique_address]]` changing `sizeof` unexpectedly
An empty member with `[[no_unique_address]]` may take 0 bytes (overlapping
padding) — great for size, but `offsetof` of a following member can surprise you.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`sizeof(T)` is all value bytes" | Object representation — includes indeterminate padding |
| "equal fields → `memcmp` == 0" | Padding differs → possible non-zero; compare member-wise |
| "`sizeof` = sum of field sizes" | + alignment padding + tail padding |
| "bit-field layout is portable" | Implementation-defined; not for wire formats |
| "padding bytes are always zero" | Indeterminate — `memset` first if you need them defined |
| "field order doesn't matter" | Largest-first minimizes padding — measurable cache/memory win |

---

## Exercises

1. **`sizeof`:** `struct A { char a; double b; char c; };` on x86-64 — size and
   member offsets?

   <details><summary>Answer</summary>

   `a` at 0, 7 bytes pad, `b` at 8, `c` at 16, 7 bytes tail pad → `sizeof(A) ==
   24`, `alignof(A) == 8`. Reorder to `{double b; char a; char c;}` → 8 + 1 + 1 +
   6 tail pad = 16.
   </details>

2. **`memcmp` result:** `struct P { std::uint8_t kind; std::uint32_t id; }; P
   x{1, 100}, y{1, 100};` (no `memset`). Is `memcmp(&x, &y, sizeof(P)) == 0`
   guaranteed?

   <details><summary>Answer</summary>

   No. 3 padding bytes after `kind` are indeterminate and may differ between `x`
   and `y` → `memcmp` can be non-zero. Compare `x.kind == y.kind && x.id == y.id`,
   or `memset` both to 0 before filling.
   </details>

3. **Dump it:** what does `dump()` print for `struct C { char c = 'A'; int i =
   0x11223344; };` on little-endian x86-64 (approx)?

   <details><summary>Answer</summary>

   `41 ?? ?? ?? 44 33 22 11` — `'A'` = `0x41`, then 3 indeterminate padding bytes
   (`??`), then the `int` little-endian. The padding bytes are whatever was in the
   stack slot.
   </details>

4. **Fix for hashing:** you want to hash an order-book key `struct K { std::int32_t
   px_ticks; std::uint32_t qty; std::uint16_t venue; };`. Two ways to make a
   byte-hash valid.

   <details><summary>Answer</summary>

   (1) `memset(&k, 0, sizeof k)` before setting fields → padding is defined (0),
   byte-hash is stable. (2) Design it with no padding: `{std::int32_t px_ticks;
   std::uint32_t qty; std::uint16_t venue; std::uint16_t _pad = 0;}` → 12 bytes,
   `has_unique_object_representations` → byte-hash always valid. (Best: just hash
   the members individually — no padding concern at all.)
   </details>

5. **`-Wpadded`:** compiling with `-Wpadded`, you get warnings on `struct Msg {
   std::uint8_t type; std::uint64_t ts; std::uint16_t len; };`. Reorder to remove
   them.

   <details><summary>Answer</summary>

   `struct Msg { std::uint64_t ts; std::uint16_t len; std::uint8_t type; };` — `ts`
   at 0, `len` at 8, `type` at 10, 5 bytes tail pad. Still one warning (tail pad);
   add `std::uint8_t _pad[5]` to make it explicit and `has_unique_object_representations`.
   </details>

---

## Interview questions

1. Object representation vs value representation — farq.
2. Padding bytes ki value — determinate ya nahi? Kya implication?
3. `struct{char; int;}` ka `sizeof` kyun 8, 5 nahi?
4. `memcmp` for struct equality — kab galat, teen fixes.
5. Tail padding kyun (`sizeof` ka `alignof` ka multiple hona)?
6. Field reordering — memory/cache pe kya asar, kaunsa order?
7. Bit-field layout portable kyun nahi?

---

## Next
→ [`08-alignment-deep.md`](08-alignment-deep.md)
