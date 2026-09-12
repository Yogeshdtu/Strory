# 06 — Packed structs

## Prerequisites
- [`05-padding-and-alignment.md`](05-padding-and-alignment.md)
- `09-ARRAYS/05-array-decay.md`, `10-STRINGS/01-c-strings.md` (raw bytes)

## Yeh topic abhi kyun
Kabhi aapko struct mein **bilkul koi padding nahi** chahiye — jab woh struct ek
network packet ya file format ke exact bytes represent karta hai. `#pragma pack`
/ `__attribute__((packed))` yeh karte hain. Par unaligned access ka trade-off
samajhna zaroori hai.

---

## `#pragma pack` / `[[gnu::packed]]`

```cpp
#pragma pack(push, 1)                       // "alignment = 1 -> no padding"
struct WireMsg {
    std::uint8_t  type;      // offset 0
    std::uint32_t seq;       // offset 1  (normally 4 -- but packed, so 1)
    std::int64_t  price;     // offset 5  (normally 8)
    std::uint16_t qty;       // offset 13
};                           // sizeof == 15  (not 24)
#pragma pack(pop)

// GCC/Clang alternative:
struct [[gnu::packed]] WireMsg2 { ... };
// or:  struct __attribute__((packed)) WireMsg3 { ... };
```

`pack(1)` → every member packed tight, `sizeof` = exact sum, `alignof` = 1.
`examples/08_market_data_struct.cpp` — a 32-byte packed `QuoteMsg` with
`static_assert`s.

---

## The trade-off: UNALIGNED access

Without padding, `seq` sits at offset 1 — a **misaligned** `uint32_t`.

```cpp
WireMsg m;
std::uint32_t s = m.seq;                    // ⚠️ reading a uint32 from an odd address
```

- **x86-64**: unaligned scalar loads *work*, usually with a small penalty (a
  cycle or two, more if it straddles a cache line). Modern x86 is forgiving.
- **ARM / older / SIMD**: unaligned access can **fault (SIGBUS)** or be much
  slower.
- **`&m.seq`** has type `uint32_t*` but points to an unaligned address →
  dereferencing it (or passing it where alignment is assumed) is **UB**.
  `-Waddress-of-packed-member` warns.

```cpp
std::uint32_t* p = &m.seq;                  // ⚠️ -Waddress-of-packed-member -- p is misaligned
std::uint32_t s;
std::memcpy(&s, &m.seq, sizeof(s));         // ✅ always safe -- memcpy handles any alignment
```

---

## Safe pattern: `memcpy`, not `reinterpret_cast`

```cpp
// ❌ risky: strict-aliasing UB + unaligned deref
const WireMsg* m = reinterpret_cast<const WireMsg*>(recvBuffer);
process(m->price);

// ✅ safe: copy the bytes into a properly-aligned local
WireMsg m;
std::memcpy(&m, recvBuffer, sizeof(m));     // no aliasing UB; memcpy handles alignment
process(m.price);                            // m is a normal local -> aligned
```

`std::memcpy` compiles to efficient code (`-O2` often to a few `mov`s / a
`movups`), and it's the standard-blessed way to reinterpret bytes. (Folder 25:
aliasing, `std::bit_cast`, `std::start_lifetime_as` C++23.)

---

## `alignas` vs `pack` — opposite directions

| | `alignas(N)` | `#pragma pack(1)` / `[[gnu::packed]]` |
|---|---|---|
| Direction | **increase** alignment / add padding | **remove** padding / alignment |
| Use | false-sharing avoidance, SIMD | exact wire/file layout |
| `sizeof` | ≥ natural, multiple of N | exact sum of members |
| Access cost | fast (aligned) | possibly slower / UB pointer if misused |

---

## When (not) to pack

**Pack:**
- Wire protocols where you don't control the layout (exchange native formats,
  ITCH/OUCH/FIX-binary, file headers).
- Memory-constrained embedded.

**Don't pack:**
- Internal structs — reorder members instead (file 05) to get most of the size
  win with **zero** access penalty.
- Anything you take `&member` of and pass around.
- Structs used in `std::vector` for compute-heavy loops (unaligned SIMD hurts).

**Best of both:** a packed struct for the wire + `memcpy` into a naturally-ordered
internal struct for processing.

---

## Andar kya hota hai

- `#pragma pack(1)` tells the compiler `alignof = 1` for the struct → every
  member offset = previous end (no rounding). `sizeof` = sum.
- A load of a packed multi-byte member → the compiler emits an unaligned load
  (`mov` on x86 handles it; on strict-alignment targets it emits byte-by-byte
  assembly or a `memcpy`-style sequence).
- `&packedMember` → a pointer the type system thinks is aligned but isn't →
  handing it to code that assumes alignment (SIMD, some libraries) is UB.
- `memcpy(&local, &packedMember, n)` → the compiler knows the source alignment is
  1 and generates a correct (possibly byte-wise, usually a single `mov`) copy.

> **HFT relevance:** Exchange native market-data / order-entry protocols are
> **packed binary**. The decoder: `memcpy` the received bytes into a
> `static_assert`-verified packed struct (or read fields via `memcpy` at known
> offsets), byteswap for endianness (file 08), then work with a
> naturally-aligned internal representation. `reinterpret_cast<Msg*>(buffer)` is
> avoided (aliasing UB + unaligned pointers); `-Waddress-of-packed-member` is
> `-Werror`. On x86 the unaligned-read penalty is small, but the *pointer* UB is
> the real hazard. Folders 25, 38, 42.

---

## Hands-on

`examples/08_market_data_struct.cpp` — packed `QuoteMsg`, `static_assert`s,
`memcpy` decode, byteswap:

```bash
./build.ps1 11-STRUCTS/examples/08_market_data_struct.cpp
g++ -std=c++20 -Wall -Wextra 11-STRUCTS/examples/08_market_data_struct.cpp -o md   # note: no packed-member warnings (we memcpy)
```

---

## ⚠️ Traps

### Trap 1 — `&packedMember`
```cpp
std::uint32_t* p = &msg.seq;   // ⚠️ -Waddress-of-packed-member. memcpy the value out
```

### Trap 2 — `reinterpret_cast<Msg*>(buffer)`
```cpp
auto* m = reinterpret_cast<Msg*>(buf);  // ⚠️ aliasing UB + m->field unaligned deref
```

### Trap 3 — packing internal structs "for size"
```cpp
#pragma pack(1)
struct HotThing { ... };   // ⚠️ unaligned access in hot loops. Reorder instead (file 05)
```

### Trap 4 — forgetting `#pragma pack(pop)`
```cpp
#pragma pack(push, 1)
struct A { ... };
// no pop -> EVERY struct after this is packed too
```

### Trap 5 — assuming `sizeof(packed) == wire size` across compilers
Mostly true with `pack(1)`, but `static_assert` it anyway (and beware
`long`/`int` size differences — use fixed-width types).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Pack everything to save memory" | Internal structs: reorder (no penalty). Pack only wire structs |
| "Unaligned access is fine on x86" | Small scalar penalty yes; but `&member` pointer is UB |
| "`reinterpret_cast<Msg*>(buf)` is the way to decode" | `memcpy` into a struct — no aliasing/alignment UB |
| "`#pragma pack(push,1)` alone is enough" | Need `pop` — else it leaks to later structs |
| "Packed `sizeof` is portable" | `static_assert` it; use fixed-width types |

---

## Exercises

1. **Pack it:** `struct M { std::uint8_t t; std::uint32_t a; std::uint64_t b; };`
   — `sizeof` unpacked vs `#pragma pack(1)`. Offsets of each member both ways.

2. **`memcpy` decode:** given a `std::array<std::byte, 13>` buffer, extract `t`,
   `a`, `b` via `memcpy` at the packed offsets. No `reinterpret_cast`.

3. **Packed-member warning:** `std::uint32_t* p = &m.a;` on the packed struct.
   `-Wall`? Now `std::uint32_t v; std::memcpy(&v, &m.a, 4);` — warning gone?

4. **Pop leak:** `#pragma pack(push,1) struct A{...};` (no pop) then `struct
   B{char c; double d;};` — `sizeof(B)`? Add the `pop` — `sizeof(B)` now?

5. **Wire ↔ internal:** a packed `WirePrice { uint8_t exp; int64_t mantissa; }`
   and an internal `Price { double value; };` — `memcpy` decode + convert.

6. **x86 penalty:** sum `m.a` over a `std::vector<PackedM>` vs an
   `std::vector<AlignedM>` (same fields, reordered, natural). `-O2`, time. Big
   difference on your machine?

---

## Interview questions

1. `#pragma pack(1)` kya karta hai? `sizeof` / `alignof` pe asar?
2. Packed struct member ka `&` lena kyun problematic?
3. `reinterpret_cast<Msg*>(buffer)` vs `memcpy(&msg, buffer, ...)` — kyun `memcpy`?
4. x86 pe unaligned scalar access — kya hota hai? ARM pe?
5. Internal struct ka size km karna hai — pack karo ya reorder? Kyun?
6. `#pragma pack(push, 1)` ke saath `pop` kyun zaroori?

---

## Next
→ [`07-aos-vs-soa.md`](07-aos-vs-soa.md)
