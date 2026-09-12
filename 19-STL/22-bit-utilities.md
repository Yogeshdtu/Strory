# 22 — `<bit>` (C++20): `bit_cast`, `popcount`, `countl_zero`, `byteswap`

## Prerequisites
- [`21-type-traits.md`](21-type-traits.md), folder 05 file 05 (bitwise operators — this is the STL version of that)
- Folder 03 (integer representation, endianness)

## Yeh topic abhi kyun
Folder 05 mein humne bit operators haath se likhe (`x & (x-1)`, shift tricks).
C++20 ka `<bit>` header inhe **standard, portable, single-instruction** functions
deta — compiler inhe `POPCNT`, `LZCNT`, `BSWAP` jaise CPU instructions mein badal
deta. HFT mein bitsets, free-list scanning, hashing, network byte order — sab
yahan.

`#include <bit>`. Free functions on **unsigned** integer types.

---

## `std::bit_cast<To>(from)` — reinterpret bits, safely

```cpp
#include <bit>

float  f = 3.14f;
std::uint32_t bits = std::bit_cast<std::uint32_t>(f);     // the IEEE-754 bit pattern
float  back = std::bit_cast<float>(bits);

double d = 1.0;
std::uint64_t db = std::bit_cast<std::uint64_t>(d);
```

- Requires `sizeof(To) == sizeof(From)` and both **trivially copyable**.
- **Not UB**, unlike `*reinterpret_cast<uint32_t*>(&f)` (which breaks strict
  aliasing) or a `union` type-pun (technically UB in C++, though most compilers
  allow it). `bit_cast` is the blessed way.
- `constexpr` (when neither type has padding / is a pointer). Compiles to a
  register move or nothing.

Use for: fast float→int bit tricks, hashing a `double` key, reading a POD out of
a byte buffer (`std::bit_cast<Header>(firstNbytes)`), serialization.

## Counting bits

```cpp
std::uint32_t x = 0b0110'1000;

std::popcount(x);        // 3   -- number of set bits            (POPCNT)
std::countr_zero(x);     // 3   -- trailing zeros (from LSB)     (TZCNT/BSF)  -- x must be non-zero for a meaningful result; countr_zero(0)==width
std::countl_zero(x);     // 25  -- leading zeros (from MSB, 32-bit type)   (LZCNT/BSR)
std::countr_one(x);      // 0   -- trailing ones
std::countl_one(x);      // 0   -- leading ones
```

Uses:
- `popcount` — cardinality of a bitset word, Hamming distance (`popcount(a ^ b)`),
  cheap "how many flags set".
- `countr_zero` — index of the lowest set bit → **scan a free-list / ready-set
  bitmask**: `int i = std::countr_zero(mask); mask &= mask - 1;` iterates set bits.
- `countl_zero` — `31 - countl_zero(x)` = index of the highest set bit = ⌊log2 x⌋.

## Power-of-two helpers

```cpp
std::has_single_bit(x);   // true iff x is a power of two (exactly one bit set)   -- was "is_power_of_2"
std::bit_ceil(x);         // smallest power of two >= x     (5 -> 8)   -- e.g. round a capacity up
std::bit_floor(x);        // largest power of two <= x      (5 -> 4)
std::bit_width(x);        // number of bits needed to represent x = 1 + floor(log2 x)  (5 -> 3, 0 -> 0)
```

`bit_ceil` is the clean way to size a hash table / ring buffer to a power of two
(so index math is `& (n-1)` not `%` — file 06).

## Rotates

```cpp
std::rotl(x, 7);          // rotate left by 7 (bits shifted off the top re-enter at the bottom)   (ROL)
std::rotr(x, 7);          // rotate right                                                          (ROR)
```

Rotates are the core primitive of hash mixers (xxHash, wyhash) and many ciphers.
Writing `((x << n) | (x >> (W - n)))` by hand is UB when `n == 0` (`x >> 32` for
a 32-bit type); `std::rotl` handles it correctly.

## Endianness

```cpp
if constexpr (std::endian::native == std::endian::little) { ... }
// std::endian::big, std::endian::little, std::endian::native

std::uint32_t n = std::byteswap(host);   // C++23 -- reverse byte order (BSWAP). Pre-C++23: __builtin_bswap32 / htonl
```

Network protocols are big-endian; x86 is little-endian → you `byteswap` on the
boundary. `std::endian` lets you do it portably at compile time.

---

## Andar kya hota hai

- Every function here maps to **one CPU instruction** on modern x86-64/ARM when
  the target supports it: `std::popcount` → `POPCNT` (~3 cycle latency, 1/cycle
  throughput), `countr_zero` → `TZCNT`, `countl_zero` → `LZCNT`, `rotl` → `ROL`,
  `byteswap` → `BSWAP`. Without the ISA extension the compiler emits a short
  portable sequence (still branchless).
- `std::bit_cast` is `memcpy` semantically; the optimizer turns it into a
  register-to-register move (int reg ↔ xmm reg for float/int) or elides it
  entirely. No load/store, no aliasing hazard.
- These replace the classic hacks with something the optimizer **recognizes**:
  `x & (x - 1)` clears the lowest bit and the compiler may or may not see the
  intent; `std::has_single_bit` / `std::countr_zero` state it directly and get
  the instruction reliably.
- Before `<bit>`: `__builtin_popcount` / `__builtin_ctz` (GCC/Clang),
  `_mm_popcnt_u64`, `_BitScanForward` (MSVC) — non-portable. `<bit>` is the
  portable spelling of exactly those.

> **HFT relevance:** direct wins. **Bitmask scanning** — a `std::countr_zero` +
> `mask &= mask-1` loop walks the set bits of a "which slots are ready / free"
> word with one instruction per bit, no branches; used in pool allocators,
> lock-free slot maps, epoll-style ready sets. **Hashing** — `rotl` + multiply is
> the guts of fast hash mixers for flat hash maps. **`bit_cast`** — pull a
> fixed-layout struct out of a receive buffer, or hash a `double` price, with no
> aliasing UB and no copy. **`byteswap` / `std::endian`** — the wire is
> big-endian, the box is little-endian; the conversion is one `BSWAP` at the
> parse boundary. **`bit_ceil`** — size ring buffers to powers of two so the
> index wrap is `& (n-1)`.

---

## Hands-on

```cpp
// bits.cpp
#include <bit>
#include <cstdio>
#include <cstdint>

int main() {
    std::uint32_t m = 0b1001'0100;
    std::printf("popcount   = %d\n", std::popcount(m));       // 3
    std::printf("countr_zero= %d\n", std::countr_zero(m));    // 2
    std::printf("bit_width  = %d\n", std::bit_width(m));      // 8
    std::printf("bit_ceil(100) = %u\n", std::bit_ceil(100u)); // 128

    // iterate set bits, lowest first:
    for (std::uint32_t x = m; x; x &= x - 1)
        std::printf("set bit at %d\n", std::countr_zero(x));  // 2, 4, 7

    float f = 1.5f;
    std::printf("1.5f bits = 0x%08X\n", std::bit_cast<std::uint32_t>(f));  // 0x3FC00000
    std::printf("native little-endian? %d\n", std::endian::native == std::endian::little);
}
```
```bash
g++ -std=c++20 -O2 -Wall bits.cpp -o bits && ./bits
```
(`./build.ps1 asm` to see it lower to `popcnt` / `tzcnt`.)

---

## ⚠️ Traps

### Trap 1 — `<bit>` functions on signed types
```cpp
std::popcount(-1);   // ❌ needs an UNSIGNED integer type. std::popcount(0xFFFFFFFFu) / cast
```

### Trap 2 — `countr_zero(0)` / `countl_zero(0)`
```cpp
int i = std::countr_zero(mask);   // ⚠️ if mask == 0 this returns the type width (32/64), not "no bit". Guard mask != 0
```

### Trap 3 — hand-rolled rotate with `n == 0`
```cpp
uint32_t r = (x << n) | (x >> (32 - n));   // ⚠️ UB when n == 0 (x >> 32). std::rotl(x, n)
```

### Trap 4 — `reinterpret_cast` instead of `bit_cast`
```cpp
uint32_t b = *reinterpret_cast<uint32_t*>(&myFloat);   // ⚠️ strict-aliasing UB. std::bit_cast<uint32_t>(myFloat)
```

### Trap 5 — `std::bit_cast` with mismatched sizes
```cpp
auto x = std::bit_cast<std::uint64_t>(3.14f);   // ❌ 8 vs 4 bytes -> won't compile. Sizes must be equal
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`popcount` is a loop over bits" | One `POPCNT` instruction on modern CPUs (~3-cycle latency) |
| "`bit_cast` is just `reinterpret_cast`" | It's `memcpy` semantics — no aliasing UB, `constexpr`, sizes must match |
| "`countr_zero(0)` returns -1 / 0" | Returns the bit width of the type — guard against zero |
| "`x & (x-1) == 0` is the clearest power-of-two test" | `std::has_single_bit(x)` — states intent, and 0 is handled (false) |
| "Rotate = shift" | Bits shifted out re-enter the other end; `std::rotl`/`rotr`, not `<<`/`>>` |

---

## Exercises

1. **Bitmask iteration:** given `std::uint64_t ready`, call `handle(i)` for every
   set bit index `i`, lowest first, in O(popcount) not O(64).

   <details><summary>Answer</summary>

   `for (std::uint64_t x = ready; x; x &= x - 1) handle(std::countr_zero(x));` —
   `countr_zero` gives the lowest set bit's index; `x &= x-1` clears it.
   </details>

2. **Round up capacity:** size a ring buffer to hold at least `n` elements as a
   power of two, and give the index-wrap expression.

   <details><summary>Answer</summary>

   `std::size_t cap = std::bit_ceil(n);` then `idx & (cap - 1)` wraps (valid
   because `cap` is a power of two) — no `%`.
   </details>

3. **log2 floor:** compute ⌊log2(x)⌋ for `x > 0` with `<bit>`.

   <details><summary>Answer</summary>

   `std::bit_width(x) - 1` (bits needed minus one), or `std::numeric_limits<T>
   ::digits - 1 - std::countl_zero(x)` (width minus leading zeros minus one).
   </details>

4. **Parse a big-endian u32:** you have `const std::byte* p` pointing at 4
   network-order bytes. Get the host `std::uint32_t`.

   <details><summary>Answer</summary>

   `std::uint32_t raw; std::memcpy(&raw, p, 4); std::uint32_t host =
   (std::endian::native == std::endian::big) ? raw : std::byteswap(raw);` (C++23;
   pre-C++23 use `__builtin_bswap32` / `ntohl`).
   </details>

5. **Hamming distance:** number of differing bits between two `std::uint64_t`
   fingerprints.

   <details><summary>Answer</summary>

   `int dist = std::popcount(a ^ b);` — XOR marks differing bits, `popcount`
   counts them.
   </details>

---

## Interview questions

1. `std::bit_cast` vs `reinterpret_cast` / union type-pun — kya safe, kyun?
2. `std::popcount` / `countr_zero` kaunse CPU instructions mein compile hote?
3. Bitmask ke set bits iterate karne ka O(popcount) tareeka?
4. `std::bit_ceil` power-of-two capacity ke liye kyun (index wrap)?
5. `countr_zero(0)` kya return karta — kaise guard karein?
6. Hand-rolled rotate `n == 0` pe UB kyun, `std::rotl` kaise theek?

---

## Next
→ [`23-allocators.md`](23-allocators.md)
