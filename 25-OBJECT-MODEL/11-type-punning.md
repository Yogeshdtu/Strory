# 11 — Type punning: every method, which is legal

## Prerequisites
- `10-strict-aliasing.md`, `07-object-representation.md`
- [`examples/06_strict_aliasing.cpp`](examples/06_strict_aliasing.cpp)

## Yeh topic abhi kyun
"Type punning" = ek object ke bytes ko ek **alag type** ki tarah interpret karna
(float ke bits, POD out of a buffer, network byte order, tagged unions). Isse
karne ke ~6 tareeke hain — **sirf 2-3 legal hain**. Ek interview classic, aur ek
real source of `-O2 -flto` miscompiles.

---

## Har method — verdict

| # | Method | C++ verdict | Notes |
|---|---|---|---|
| 1 | `std::bit_cast<To>(from)` | ✅ **legal** (C++20) | same size, both trivially-copyable, `constexpr`, zero cost |
| 2 | `std::memcpy(&to, &from, n)` | ✅ **legal** | any size/partial, zero cost at `-O2` for small fixed `n` |
| 3 | Read bytes via `char*` / `unsigned char*` / `std::byte*` | ✅ **legal** | the aliasing exception; for *reading bytes*, not reinterpreting as another scalar |
| 4 | Placement `new` to put a new object at the address | ✅ **legal** | actually *changes* what object lives there (file 09) |
| 5 | `union { float f; uint32_t i; }` — read inactive member | ⚠️ **UB in C++** | implementation-defined in C; GCC/Clang support as extension |
| 6 | `*reinterpret_cast<To*>(&from)` then deref | ❌ **UB** | strict-aliasing violation; miscompiles at `-O2` |
| 7 | `*(To*)&from` (C-style cast) | ❌ **UB** | same as #6 |

**Rule of thumb: `std::bit_cast` for a fixed-size scalar/POD, `std::memcpy` for
everything else. Never `reinterpret_cast` + deref.**

---

## Worked examples

### Float ↔ bits

```cpp
std::uint32_t bits = std::bit_cast<std::uint32_t>(3.14f);   // ✅
float back        = std::bit_cast<float>(bits);             // ✅
```

### POD out of a byte buffer

```cpp
struct Header { std::uint32_t magic, len; };   // trivially copyable, standard layout
static_assert(std::is_trivially_copyable_v<Header> && sizeof(Header) == 8);

Header h;
std::memcpy(&h, buf.data(), sizeof h);          // ✅  (buf.size() >= 8 checked)
// ❌ Header* hp = reinterpret_cast<Header*>(buf.data());  hp->magic;  // UB
```

Or, if you have exactly the right bytes in an array:
```cpp
Header h = std::bit_cast<Header>(*reinterpret_cast<const std::byte(*)[8]>(buf.data()));
```
(usually `memcpy` is clearer.)

### Network byte order

```cpp
std::uint32_t be;
std::memcpy(&be, buf, 4);
std::uint32_t host = std::byteswap(be);        // C++23; or __builtin_bswap32 / ntohl
```

### Tagged storage (instead of a punning union)

```cpp
struct Value {
    enum class Tag { I, D } tag;
    alignas(std::max_align_t) unsigned char storage[8];   // hold an int64 or a double

    void set(std::int64_t v) { tag = Tag::I; std::memcpy(storage, &v, 8); }
    void set(double v)       { tag = Tag::D; std::memcpy(storage, &v, 8); }
    std::int64_t as_int()  const { std::int64_t v; std::memcpy(&v, storage, 8); return v; }
    double       as_double() const { double v; std::memcpy(&v, storage, 8); return v; }
};
```

`std::variant` does this properly (with lifetime tracking). For a POD-only case a
hand-rolled tagged struct with `memcpy` accessors is fine and UB-free.

### `std::span<std::byte>` view of an object

```cpp
template <class T>
std::span<const std::byte> as_bytes(const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>);
    return {reinterpret_cast<const std::byte*>(&obj), sizeof(T)};   // ✅ char/byte exception
}
```
`std::as_bytes` / `std::as_writable_bytes` do exactly this for a `std::span`.

---

## Why `reinterpret_cast` + deref actually breaks

Not just theory — at `-O2` with type-based alias analysis:
- The compiler assumes a `float` object and a `std::uint32_t*` don't refer to the
  same memory → it can keep the `float` in a register across the `uint32_t` write,
  reorder the read, or vectorize a loop wrongly.
- `examples/06`'s `aliasing_trap`: `-O2 -fstrict-aliasing` reuses a load → wrong
  answer. `bit_cast`/`memcpy` don't have this problem — they carry no aliasing
  claim.

---

## `reinterpret_cast` — what it's actually FOR (legal uses)

`reinterpret_cast` itself isn't evil; **dereferencing the result as an unrelated
type** is. Legal uses:
- **Pointer ↔ integer**: `reinterpret_cast<std::uintptr_t>(p)` and back (round-trip
  preserves the value).
- **To `char*`/`unsigned char*`/`std::byte*`** and reading bytes (the exception).
- **Function pointer ↔ function pointer** (call through the *original* type only).
- **Pointer ↔ pointer** for later cast *back* to the original type before use.
- **`std::launder`-adjacent** placement-new plumbing.

---

## > **HFT relevance**
> - **All protocol parsing uses `memcpy`/`bit_cast`** to pull typed fields out of
>   `std::span<const std::byte>` — see folder 38. `reinterpret_cast<Msg*>(buf)` is
>   the classic bug that survives testing and blows up under `-O2 -flto
>   -march=native` in production.
> - **`std::byteswap` (C++23) / `__builtin_bswap*`** for endianness — compiles to
>   a single `bswap`/`movbe`.
> - **Tagged POD storage with `memcpy` accessors** where `std::variant`'s
>   machinery is more than you want — still UB-free.
> - **Zero cost** — a fixed-size `memcpy` / `bit_cast` is the same load/store the
>   UB version would emit. There is never a performance reason to pun unsafely.

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/06_strict_aliasing.cpp
```

Then write a tiny `parse_header(std::span<const std::byte>)` that returns a
`Header` via `memcpy`, and a `bad_parse` that `reinterpret_cast`s — compile both
`-O2 -S` and diff the assembly (often identical for the simple case; the point is
one is UB and can break with more context).

---

## ⚠️ Traps

### Trap 1 — `*(T*)&x` / `reinterpret_cast<T*>(&x)` deref
UB for unrelated `T`. `std::bit_cast` / `std::memcpy`.

### Trap 2 — union punning assumed portable
UB in C++ (implementation-defined in C). Use `bit_cast`.

### Trap 3 — `reinterpret_cast<Struct*>(byte_buffer)`
No `Struct` object lives there → UB. `memcpy` into a real `Struct`.

### Trap 4 — `bit_cast` size mismatch
Compile error — sizes must be exactly equal.

### Trap 5 — `bit_cast` a type with padding
The padding bytes come from `from`'s object representation — usually fine, but if
you then `memcmp`/hash the result, padding bytes bite (file 07).

### Trap 6 — punning a non-trivially-copyable type
`std::bit_cast` / `memcpy` require trivially-copyable. Punning a `std::string` is
always wrong.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "there are many ways to pun, pick one" | Only `bit_cast` / `memcpy` / byte-pointer-read are legal |
| "union punning is the C++ way" | UB in C++; a compiler extension at best |
| "`reinterpret_cast` is the punning operator" | It's for pointer↔int and cast-back; deref-as-other-type is UB |
| "`bit_cast` is slower than a cast" | Same codegen at `-O2` (often a `mov` or nothing) |
| "`memcpy` needs a real function call" | Small fixed-size → inlined to load/store |
| "it worked, so the pun is fine" | UB; `-O2`/`-flto` can miscompile it later |

---

## Exercises

1. **Rank by legality:** (a) `bit_cast`, (b) `memcpy`, (c) union inactive read,
   (d) `*(T*)&x`, (e) byte-pointer read — for extracting a `uint32_t` from a
   `float`.

   <details><summary>Answer</summary>

   Legal: (a), (b), and (e) *if* you assemble the 4 bytes yourself. (c) is UB in
   C++ (extension in practice). (d) is UB. Prefer (a).
   </details>

2. **Fix:** `struct P { uint16_t a; uint16_t b; }; uint32_t combined =
   *reinterpret_cast<uint32_t*>(&p);` — legal version.

   <details><summary>Answer</summary>

   `uint32_t combined = std::bit_cast<uint32_t>(p);` (P is 4 bytes, trivially
   copyable). Or `std::memcpy(&combined, &p, 4);`. The result's byte order matches
   the machine's.
   </details>

3. **Parse:** `std::span<const std::byte> b` (≥ 12 bytes) holds `{uint32_t seq;
   uint64_t ts;}` (packed, no padding). Extract both, safely.

   <details><summary>Answer</summary>

   ```cpp
   std::uint32_t seq; std::uint64_t ts;
   std::memcpy(&seq, b.data(),     4);
   std::memcpy(&ts,  b.data() + 4, 8);
   ```
   (If the wire is big-endian, `std::byteswap` each.) Don't
   `reinterpret_cast<const Packed*>(b.data())`.
   </details>

4. **Union alternative:** replace `union { double d; std::uint64_t u; } x;` used
   for bit inspection.

   <details><summary>Answer</summary>

   Drop the union. `std::uint64_t u = std::bit_cast<std::uint64_t>(d);` for the
   bits, `double d = std::bit_cast<double>(u);` back.
   </details>

5. **`reinterpret_cast` legal use:** name two things `reinterpret_cast` is
   genuinely for.

   <details><summary>Answer</summary>

   (1) Pointer ↔ integer round-trip (`std::uintptr_t`). (2) Casting an object
   pointer to `char*`/`unsigned char*`/`std::byte*` to read its bytes. (Also:
   pointer→pointer where you'll cast *back* to the original type before use;
   function-pointer conversions used only via the original type.)
   </details>

---

## Interview questions

1. Type punning ke saare tareeke — kaunse 2-3 legal hain?
2. `union` inactive-member read — C vs C++ verdict.
3. `std::bit_cast` ki requirements (size, trivially-copyable).
4. `reinterpret_cast<T*>(buf)` + deref kyun UB, kab bite karta?
5. `reinterpret_cast` kis ke liye actually hai (legal uses)?
6. Buffer se POD nikaalne ka sahi tarika.
7. `bit_cast` vs `memcpy` — kab kaunsa, cost?

---

## Next
→ [`12-casts-deep.md`](12-casts-deep.md)
