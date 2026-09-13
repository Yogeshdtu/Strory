# 11 — Bitfields

## Prerequisites
- [`05-padding-and-alignment.md`](05-padding-and-alignment.md), [`06-packed-structs.md`](06-packed-structs.md)
- `05-OPERATORS/05-bitwise-operators.md` (masks, shifts)

## Yeh topic abhi kyun
Bitfield = struct member jiske liye aap **exact kitne bits** batate ho.
`unsigned flags : 3;` → 3 bits. Memory-tight flags/packed fields ke liye — par
portability ki problems bahut hain, aur aksar `enum class` flags + manual masks
behtar.

---

## Syntax

```cpp
struct Packed {
    unsigned type     : 4;    // 4 bits  (0..15)
    unsigned priority : 3;    // 3 bits  (0..7)
    unsigned urgent   : 1;    // 1 bit   (0..1)
    unsigned          : 0;    // ⟵ agla field naye storage unit se shuru karo
    unsigned seq      : 24;   // 24 bits
};                            // sizeof 8 (GCC 16.2): pehla 4-byte unit (8 bits use), phir naya unit (24 bits)
```

```cpp
Packed p{};
p.type = 5;
p.urgent = 1;
if (p.priority == 7) { ... }
```

`: 0` (bina naam, zero width) → "agla bitfield ek naye underlying unit mein shuru karo" — ek sub-group ko align
karne ke kaam aata hai.

Analogy: ek hi register ke page pe khaane kheench lena — "pehle 4 khaane type ke, agle 3 priority ke, 1 urgent ka".
Jagah bachti hai, par doosre office ka register khaane kis taraf se ginta hai, woh unki marzi.

---

## sizeof — tight, par niyam dhundhle

```cpp
struct A { unsigned a : 4; unsigned b : 4; };          // 4 bytes -- 8 bits hi use, par unit `unsigned` (4 bytes, align 4)
struct B { unsigned a : 4; unsigned b : 6; };          // 4 bytes -- 10 bits, ek hi 32-bit unit mein
struct C { unsigned a : 20; unsigned b : 20; };        // 8 bytes -- 40 bits, do 32-bit units
struct Status { std::uint8_t active : 1, priority : 3, retries : 4; };   // 1 byte -- unit uint8_t
```

(Sab GCC 16.2 pe chala ke.) Compiler lagataar bitfields ko underlying type ke storage units mein bharta hai
(`unsigned` → 32-bit units), field fit na ho to agle unit mein. `sizeof` aur `alignof` underlying type ke hisaab se
round up hote hain — isliye sirf 8 bits wala `A` bhi 4 bytes ka hai. **1-byte bitfield struct chahiye to
underlying type bhi `uint8_t` rakho** (`Status`).

---

## ⚠️ Portability — bitfields implementation-defined hain

Standard bahut kuch khula chhodta hai:

| Pehlu | Standard tay karta hai? |
|---|---|
| Unit ke andar bit **order** (LSB-first vs MSB-first) | ❌ implementation-defined |
| Field unit ki boundary **paar** kar sakta hai ya nahi | ❌ |
| Fields ke beech padding | ❌ |
| Alag underlying types (`uint8_t : 4` phir `uint32_t : 4`) ek unit share karein ya nahi | ❌ |
| Compilers / architectures ke beech layout | ❌ — **alag** |

### Naapa hua: ek hi compiler, ek flag, alag layout
MinGW GCC Microsoft-compatible layout ke liye default mein `-mms-bitfields` on rakhta hai (file 05 mein `-Wpadded`
ke saath dekha tha):

```cpp
struct Mixed { std::uint8_t a : 4; std::uint32_t b : 4; };
```

| GCC 16.2 (MinGW) | `sizeof(Mixed)` |
|---|---|
| default (`-mms-bitfields`, MSVC jaisa) | **8** — type badla to naya unit |
| `-mno-ms-bitfields` (Linux GCC jaisa) | **4** — dono ek unit mein |

Wahi source, wahi machine, sirf ek flag → 8 vs 4 bytes. Ab socho do alag compilers ya do alag OS pe.

**→ Bitfields wire/file formats ke liye SAFE NAHI** jahan systems ke beech exact bytes chahiye. **Manual masks +
shifts** use karo:

```cpp
// portable "type bits 0-3 mein, priority bits 4-6 mein, urgent bit 7 mein"
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

Har bit aapke haath mein; har jagah same.

---

## Bitfields kab theek hain

- **Internal, ek hi compiler** wala data jahan memory sach mein matter kare aur kabhi serialize na ho.
- Local structs mein `p.urgent = 1` vs `flags |= URGENT_BIT` ki padhne mein aasaani.
- Jaane-pehchaane platform pe **hardware registers** map karna (embedded) — compiler ke documented layout ke saath.

## Kab bacho

- Wire/file formats → manual masks.
- Jo bhi compiler/arch/flags ki boundary paar kare.
- Hot paths jahan bitfield ka read-modify-write (unit load, mask, shift, or, store) zyada kaam lage — waise aam
  taur pe theek hai.
- Bitfield ka `&` → **allowed nahi** (`error: attempt to take address of bit-field` — GCC 16.2).

---

## Bitfields vs `enum class` flags vs `std::bitset`

| | Bitfield | `enum class` + masks | `std::bitset<N>` |
|---|---|---|---|
| Exact wire layout | ❌ (impl-defined) | ✅ (aapke haath mein) | ❌ (impl-defined) |
| Naam se access | ✅ `p.urgent` | ✅ `has(f, Flag::Urgent)` | ⚠️ index se |
| Bit count | koi bhi (1..) | aap hisaab lagao | N (compile-time) |
| Ek bit ka `&` | ❌ | n/a | n/a |
| Best for | internal packed structs | portable flag sets | bade fixed bit arrays |

HFT mein order flags / message flags ke liye: **`enum class : uint32_t` + defined `operator|`/`&`** (file 10) —
portable, naam wale, zero-cost.

---

## Andar kya hota hai

- Compiler field ke declared type ke storage units allocate karta hai (`unsigned` → 4 bytes) aur har bitfield ko
  unit ke andar (start bit, width) deta hai.
- `p.priority` padhna → `unit load; (unit >> start) & ((1 << width) - 1)`.
- Likhna → `unit load; unit = (unit & ~mask) | ((value << start) & mask); unit store` — ek bit ke liye bhi
  **read-modify-write**. Atomic nahi (concurrent access ke liye bura — folder 26).
- `: 0` → "agla bit" cursor ko naye unit ki shuruaat tak round up.
- Exact (start, width, byte) assignment hi woh hissa hai jo standard pakka nahi karta. GCC x86 pe LSB-first dikha:
  `Status{active=1, priority=5}` ka raw byte `0x0b` (`0b0000'1011` — bit 0 active, bits 1–3 priority).

> **HFT relevance:** HFT mein boundary paar karne wali kisi bhi cheez ke liye bitfields se bacha jaata hai —
> exchange protocols bits ekdum saaf batate hain, aur unhe **explicit masks/shifts** se match kiya jaata hai taaki
> GCC/Clang/koi bhi compiler same bytes banaye (upar dekha: ek flag se `sizeof` 8 vs 4). Internal packed status words
> kabhi readability ke liye bitfields use karte hain (ek compiler, kabhi serialize nahi). Concurrent flag updates
> kabhi bitfields se nahi (non-atomic RMW) — `std::atomic<uint32_t>` + bit ops. Folders 27, 38.

---

## Hands-on

```cpp
#include <cstdint>
#include <cstring>
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
    std::uint8_t raw; std::memcpy(&raw, &s, 1);
    std::cout << std::hex << (int)raw << "\n";   // GCC x86: b
    // masks wala portable version bhi likho -- dono ke bytes milao
}
```

```bash
g++ -std=c++20 -Wall -Wextra bf.cpp -o bf && ./bf
```

---

## ⚠️ Traps

### Trap 1 — wire format ke liye bitfields
```cpp
struct WireHdr { unsigned ver : 4; unsigned type : 4; };   // ⚠️ bit order impl-defined. Masks
```

### Trap 2 — value fit nahi hoti
```cpp
unsigned x : 3;  x = 10;   // ⚠️ 3 bits mein 10 nahi aata -> 10 mod 8 = 2
```
Unsigned bitfield ke liye yeh modulo (well-defined) hai. Constant ho to GCC 16.2 bina flag ke warn karta hai:
`conversion from 'unsigned int' to 'unsigned char:3' changes value from '10' to '2' [-Woverflow]`. Runtime value
ho to koi warning nahi.

### Trap 3 — bitfield ka `&`
```cpp
auto* p = &s.priority;   // ❌ error: attempt to take address of bit-field
```

### Trap 4 — plain `int : n` ki signedness
```cpp
struct S { int flag : 1; };  s.flag = 1;  if (s.flag == 1) // ⚠️ GCC: flag = -1, condition FALSE
```
Signed 1-bit field sirf 0 aur -1 rakh sakta hai. GCC 16.2 pe `-Wall -Wextra` **chup** rehta hai; sirf `-Wconversion`
bolta hai: `changes value from '1' to '-1'`. `unsigned x : n` use karo.

### Trap 5 — concurrent bitfield writes
```cpp
// thread A: s.a = 1;   thread B: s.b = 1;   -- ⚠️ dono ek hi unit ka RMW -> data race
```

### Trap 6 — `sizeof` ko bits ka jod samajhna
```cpp
struct A { unsigned a : 4; unsigned b : 4; };   // ⚠️ 8 bits, par sizeof 4 -- unit type ki alignment
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Bitfields ka layout portable hai" | Bit order / boundary paar / padding sab impl-defined — ek MinGW flag se 8 vs 4 bytes |
| "Bitfields network structs ke liye safe hain" | Nahi — explicit masks/shifts |
| "8 bits ke bitfields = 1-byte struct" | Underlying type ki alignment: `unsigned` → 4 bytes |
| "Ek bit likhna = ek store" | Poore unit ka read-modify-write |
| "`int flag : 1` mein 0 ya 1" | Signed → 0 ya -1. `unsigned` use karo |
| "Ek struct ke alag bitfield writes independent hain" | Same unit → RMW → concurrent ho to data race |

---

## Exercises

1. **sizeof:** `struct A { unsigned a:4, b:4; };`, `struct B { unsigned a:4, b:6; };`,
   `struct C { unsigned a:20, b:20; };` — har ek ka `sizeof`, samjhao.
   <details><summary>Answer (GCC 16.2)</summary>

   `A` **4**, `B` **4**, `C` **8**. `A` aur `B` ek 32-bit unit mein aa jaate hain (8 aur 10 bits); `sizeof` unit ke
   size/alignment (4) tak round up. `C` ke 20 + 20 = 40 bits ek 32-bit unit mein nahi aate → do units → 8.
   </details>

2. **Portable version:** Hands-on wale `Status` ko ek `std::uint8_t` + masks wale `pack`/`unpack` functions se dobara
   banao. Bitfield version ka byte milao — mela ya nahi (yahi to point hai).

3. **Overflow:** `unsigned x : 3; x = 9;` — `x` print karo. Kya hua?
   <details><summary>Answer</summary>

   **1** (9 mod 8). GCC ne compile time pe warn kiya: `changes value from '9' to '1' [-Woverflow]`.
   </details>

4. **Signedness:** `struct S { int f : 1; };  s.f = 1;  std::cout << s.f;` — 1 ya -1? `unsigned f : 1;` karo — ab?
   <details><summary>Answer</summary>

   `int f : 1` → **-1** (GCC 16.2; warning sirf `-Wconversion` pe). `unsigned f : 1` → **1**.
   </details>

5. **`: 0`:** `struct P { unsigned a:4; unsigned :0; unsigned b:4; };` — `sizeof`? `:0` hatao — `sizeof`?
   <details><summary>Answer</summary>

   `:0` ke saath **8** (b naye unit mein), bina **4**.
   </details>

6. **Wire-format sahi tareeqe se:** 8-bit flags byte jismein `HIDDEN` (bit 0), `POST_ONLY` (bit 1), `IOC` (bit 2) —
   `enum class Flag : uint8_t` + `operator|` + `has()`. Raw byte ke through round-trip karo.

7. **Layout flag:** `struct Mixed { std::uint8_t a : 4; std::uint32_t b : 4; };` ka `sizeof` default aur
   `-mno-ms-bitfields` dono se (MinGW pe). Linux pe ho to `-mms-bitfields` try karo.
   <details><summary>Answer (MinGW GCC 16.2)</summary>

   Default **8**, `-mno-ms-bitfields` **4**.
   </details>

---

## Interview questions

1. Bitfield kya hai? `sizeof` kaise tay hota hai?
2. Bitfields wire formats ke liye kyun unsafe (kya impl-defined hai)?
3. Bitfield write — kaunsa operation hota hai (ek bit ke liye bhi)?
4. `int x : 1` vs `unsigned x : 1` — signedness ka issue?
5. Bitfield ka `&` kyun nahi le sakte?
6. Portable packed flags — bitfield ya masks? Kyun?

---

## Next
→ [`12-struct-vs-class.md`](12-struct-vs-class.md)
