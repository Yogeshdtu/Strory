# 11 — Bitfields

## Prerequisites
- [`05-padding-and-alignment.md`](05-padding-and-alignment.md), [`06-packed-structs.md`](06-packed-structs.md)
- `05-OPERATORS/05-bitwise-operators.md` (masks, shifts)

## Yeh topic abhi kyun
Bitfield = struct member jiske aap **exact number of bits** specify karte ho.
`unsigned flags : 3;` → 3 bits. Memory-tight flags/packed fields ke liye — par
portability issues bahut hain, aur aksar `enum class` flags + manual masks
behtar.

---

## Syntax

```cpp
struct Packed {
    unsigned type     : 4;    // 4 bits  (0..15)
    unsigned priority : 3;    // 3 bits  (0..7)
    unsigned urgent   : 1;    // 1 bit   (0..1)
    unsigned          : 0;    // ⟵ force next field to a new storage unit
    unsigned seq      : 24;   // 24 bits
};
// (compiler packs these into as few bytes as it can)
```

```cpp
Packed p{};
p.type = 5;
p.urgent = 1;
if (p.priority == 7) { ... }
```

`: 0` (unnamed, zero width) → "start the next bitfield in a fresh underlying
unit" — useful for aligning a sub-group.

---

## sizeof — packed, but rules are fuzzy

```cpp
struct A { unsigned a : 4; unsigned b : 4; };          // 1 byte (both fit in 8 bits)
struct B { unsigned a : 4; unsigned b : 6; };          // 2 bytes (10 bits -> 2 units)
struct C { unsigned a : 20; unsigned b : 20; };        // 8 bytes (can't share a 32-bit unit)
```

The compiler packs consecutive bitfields into the underlying type's storage units
(`unsigned` → 32-bit units), spilling to the next unit when a field won't fit.
`alignof` follows the underlying type.

---

## ⚠️ Portability — bitfields are implementation-defined

The standard leaves a LOT unspecified:

| Aspect | Specified? |
|---|---|
| Bit **order** within a unit (LSB-first vs MSB-first) | ❌ implementation-defined |
| Whether a field can **straddle** a unit boundary | ❌ |
| Padding between fields | ❌ |
| Signedness of plain `int : n` (`int` vs `unsigned`) | ❌ (use explicit `unsigned`/`signed`) |
| Layout across compilers / architectures | ❌ — **different** |

**→ Bitfields are NOT safe for wire/file formats** where you need exact bytes
across systems. GCC and MSVC lay them out differently. Use **manual masks +
shifts** for anything portable:

```cpp
// portable "type in bits 0-3, priority in bits 4-6, urgent in bit 7"
constexpr std::uint8_t TYPE_MASK = 0x0F;
constexpr std::uint8_t PRIO_SHIFT = 4, PRIO_MASK = 0x70;
constexpr std::uint8_t URGENT_BIT = 0x80;

std::uint8_t pack(std::uint8_t type, std::uint8_t prio, bool urgent) {
    return (type & TYPE_MASK)
         | ((prio << PRIO_SHIFT) & PRIO_MASK)
         | (urgent ? URGENT_BIT : 0);
}
std::uint8_t getType(std::uint8_t b) { return b & TYPE_MASK; }
```

You control every bit; it's identical everywhere.

---

## When bitfields are OK

- **Internal, single-compiler** data where memory really matters and you never
  serialize it.
- Readability of `p.urgent = 1` vs `flags |= URGENT_BIT` — for local structs.
- Mapping **hardware registers** on a known platform (embedded) — with the
  compiler's documented layout.

## When to avoid

- Wire/file formats → manual masks.
- Anything crossing compiler/arch boundaries.
- Hot paths where the compiler's read-modify-write for a bitfield (load unit,
  mask, shift, or, store) is more work than you'd like — though usually fine.
- Taking `&` of a bitfield → **not allowed** (`error: cannot bind pointer to
  bit-field`).

---

## Bitfields vs `enum class` flags vs `std::bitset`

| | Bitfield | `enum class` + masks | `std::bitset<N>` |
|---|---|---|---|
| Exact wire layout | ❌ (impl-defined) | ✅ (you control) | ❌ (impl-defined) |
| Named access | ✅ `p.urgent` | ✅ `has(f, Flag::Urgent)` | ⚠️ indexed |
| Bit count | any (1..) | you compute | N (compile-time) |
| `&` of a single bit | ❌ | n/a | n/a |
| Best for | internal packed structs | portable flag sets | large fixed bit arrays |

For order flags / message flags in HFT: **`enum class : uint32_t` + defined
`operator|`/`&`** (file 10) — portable, named, zero-cost.

---

## Andar kya hota hai

- The compiler allocates storage units of the field's declared type (`unsigned`
  → 4 bytes) and assigns each bitfield a (start bit, width) within a unit.
- Reading `p.priority` → `load unit; (unit >> start) & ((1 << width) - 1)`.
- Writing → `load unit; unit = (unit & ~mask) | ((value << start) & mask); store
  unit` — a **read-modify-write** even for one bit. Not atomic (bad for
  concurrent access — folder 26).
- `: 0` → round the "next bit" cursor up to the start of a new unit.
- The exact (start, width, byte) assignment is the part the standard doesn't pin
  down.

> **HFT relevance:** Bitfields are mostly avoided in HFT for anything that
> crosses a boundary — exchange protocols specify bits precisely, and you match
> them with **explicit masks/shifts** so GCC/Clang/whatever produce identical
> bytes. Internal packed status words sometimes use bitfields for readability
> (single compiler, never serialized). Concurrent flag updates never use
> bitfields (non-atomic RMW) — `std::atomic<uint32_t>` + bit ops. Folders 27, 38.

---

## Hands-on

```cpp
#include <cstdint>
#include <iostream>
struct Status {
    std::uint8_t active   : 1;
    std::uint8_t priority : 3;
    std::uint8_t retries  : 4;
};
int main() {
    std::cout << sizeof(Status) << "\n";     // 1
    Status s{};
    s.priority = 5; s.active = 1;
    std::cout << (int)s.priority << " " << (int)s.active << "\n";
    // portable equivalent with masks -- write both, compare bytes
}
```

```bash
g++ -std=c++20 -Wall -Wextra bf.cpp -o bf && ./bf
```

---

## ⚠️ Traps

### Trap 1 — bitfields for a wire format
```cpp
struct WireHdr { unsigned ver : 4; unsigned type : 4; };   // ⚠️ bit order impl-defined. Masks
```

### Trap 2 — value doesn't fit
```cpp
unsigned x : 3;  x = 10;   // ⚠️ 10 doesn't fit in 3 bits -> truncated to 2 (implementation-defined-ish)
```

### Trap 3 — `&` of a bitfield
```cpp
auto* p = &s.priority;   // ❌ ERROR -- cannot take address of a bit-field
```

### Trap 4 — plain `int : n` signedness
```cpp
struct S { int flag : 1; };  s.flag = 1;  if (s.flag == 1) // ⚠️ int:1 may be signed -> holds -1, not 1
```
Use `unsigned x : n`.

### Trap 5 — concurrent bitfield writes
```cpp
// thread A: s.a = 1;   thread B: s.b = 1;   -- ⚠️ both RMW the same byte -> data race
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Bitfields have a portable layout" | Bit order / straddling / padding all impl-defined |
| "Bitfields are safe for network structs" | No — use explicit masks/shifts |
| "Writing one bit is a single store" | Read-modify-write of the whole unit |
| "`int flag : 1` holds 0 or 1" | May be signed → holds 0 or -1. Use `unsigned` |
| "Different bitfield writes to one struct are independent" | Same unit → RMW → data race if concurrent |

---

## Exercises

1. **sizeof:** `struct A { unsigned a:4, b:4; };`, `struct B { unsigned a:4, b:6;
   };`, `struct C { unsigned a:20, b:20; };` — `sizeof` each, explain.

2. **Portable equivalent:** re-implement the `Status` struct from Hands-on as a
   single `std::uint8_t` + `pack`/`unpack` functions with masks. Verify the byte
   matches (or differs — that's the point) vs the bitfield version.

3. **Overflow:** `unsigned x : 3; x = 9;` — print `x`. What happened?

4. **Signedness:** `struct S { int f : 1; };  s.f = 1;  std::cout << s.f;` — 1 or
   -1? Change to `unsigned f : 1;` — now?

5. **`: 0`:** `struct P { unsigned a:4; unsigned :0; unsigned b:4; };` — `sizeof`?
   Remove the `:0` — `sizeof`?

6. **Wire-format the right way:** an 8-bit flags byte with `HIDDEN` (bit 0),
   `POST_ONLY` (bit 1), `IOC` (bit 2) — `enum class Flag : uint8_t` + `operator|`
   + `has()`. Round-trip through a raw byte.

---

## Interview questions

1. Bitfield kya hai? `sizeof` kaise decide hota hai?
2. Bitfields wire formats ke liye kyun unsafe (kya impl-defined hai)?
3. Bitfield write — kya operation (single bit ke liye bhi)?
4. `int x : 1` vs `unsigned x : 1` — signedness issue?
5. Bitfield ka `&` kyun nahi le sakte?
6. Portable packed flags — bitfield ya masks? Kyun?

---

## Next
→ [`12-struct-vs-class.md`](12-struct-vs-class.md)
