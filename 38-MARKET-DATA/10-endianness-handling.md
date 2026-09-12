# 10 — Endianness handling: network byte order, byteswap, safe reads

## Prerequisites
- [`09-zero-copy-parsing.md`](09-zero-copy-parsing.md)
- `19-STL/22-bit-utilities.md` (`std::endian`, `std::byteswap` — C++23
  preview)
- `examples/02_endian_handling.cpp`

## Yeh topic abhi kyun
06 aur 09 dono ne "swap karna zaroori hai" kaha bina yeh dikhaye kaise, aur
kya galat ho sakta agar bhool jaao. Yeh lesson woh mechanics hai.

---

## Endianness — ek line mein

> **Endianness = ek multi-byte number ke bytes memory mein KIS ORDER mein
> store hote.**

```
Value 0x12345678 (4 bytes: 12 34 56 78)

Big-endian:     [12] [34] [56] [78]   -- MOST significant byte PEHLE
Little-endian:  [78] [56] [34] [12]   -- LEAST significant byte PEHLE
```

**x86 aur ARM (is course ke sab targets) little-endian hain.** Network
protocols historically **big-endian** convention follow karte ("network
byte order") — jismein hamara `wire_protocol.hpp` bhi hai (06).

---

## Bina swap kiye kya hota hai (measured demo)

```
02_endian_handling.cpp output:

host value (jo bhejna tha)              = 0x12345678
wire bytes (as if raw-read, NO swap)    = 0x78563412   <- GARBLED
wire bytes ko net_to_host32 karne ke baad = 0x12345678   <- SAHI
```

Bina swap kiye, `0x12345678` **`0x78563412`** ban jaata — bilkul galat
number, koi crash nahi, bas **silently galat** (03-VARIABLES/03-snapshots
ka "silent bug" theme yahan bhi). Price, qty, order_id — sab galat honge
agar swap chhoot gaya.

---

## Kaise swap karo — safe pattern

```cpp
// wire_protocol.hpp
inline std::uint32_t net_to_host32(std::uint32_t v) {
    if constexpr (std::endian::native == std::endian::little) return bswap32(v);
    else return v;
}
```

`if constexpr (std::endian::native == ...)` — **compile time** decide
hota, koi runtime branch nahi (little-endian target pe swap function
seedha inline hoti, big-endian target pe **literally no-op** compile hoti,
zero cost). `std::endian` (`<bit>`, C++20) portable tareeka hai host ka
byte order **compile time pe** jaanne ka.

**C++23 mein `std::byteswap` built-in hai** — is course ka `-std=c++20`
target pe woh available nahi, isliye hum GCC/Clang ke `__builtin_bswap16/
32/64` seedha use karte (19-STL/22 mein yeh already note kiya gaya tha).

---

## Byteswap ki cost — measured (05 se repeat, poori clarity ke liye)

```
02_endian_handling.cpp (-O2):
  bswap64() measured cost = 0.753 ns/swap
```

**Ek single CPU instruction (`BSWAP`)**, ~1-2 cycles. **Har multi-byte
field** ko individually swap karna padta — `AddOrderMsg` mein `order_id`
(64-bit), `symbol_id` (32-bit), `qty` (32-bit), `price_ticks` (64-bit) —
4 swaps, ~3 ns total. Ek 30ns parse operation ke against, yeh **~10%**
hai — measurable, par dominant nahi (allocation ka 20x+ effect, 09, kahin
zyada bada hai).

---

## Alignment-safe read — memcpy vs direct pointer cast

```cpp
// ❌ UNSAFE (kaam kar sakta x86 pe, UB hai, ARM pe fault ho sakta):
auto v = *reinterpret_cast<const std::uint64_t*>(misaligned_buf + 3);

// ✅ SAFE -- alignment ki parwaah nahi karta:
std::uint64_t v;
std::memcpy(&v, misaligned_buf + 3, sizeof v);
```

`02_endian_handling.cpp` mein isko demonstrate kiya gaya — ek `uint64_t`
ko offset 3 (misaligned) se `memcpy` se safely padha gaya, expected value
se match kiya:

```
=== Misaligned read (offset 3, memcpy se) ===
expected = 0xaabbccddeeff0011  memcpy se mila = 0xaabbccddeeff0011  match? haan
```

**`-O2` pe `memcpy` (fixed, small size) ek single (possibly unaligned)
load instruction mein compile hoti** — koi function-call overhead, koi
loop nahi. Yeh 36/22 ka Trap 3 hi hai, market-data context mein.

**Hamare wire structs `#pragma pack(1)` hain** — is se struct ka apna
`alignof == 1` (01 mein verified), isliye overlay-cast bhi safe hai
(member access khud unaligned-load karta compiler se, koi crash risk
nahi x86/ARM pe).

---

## Poora field-by-field example

```cpp
// 02_endian_handling.cpp
AddOrderMsg raw{};
std::memcpy(&raw, buf.data(), sizeof raw);   // alignment-safe copy out

// HAR multi-byte field individually swap:
net_to_host32(raw.hdr.seq_num);
net_to_host64(raw.order_id);
net_to_host32(raw.symbol_id);
net_to_host32(raw.qty);
net_to_host_i64(raw.price_ticks);
raw.side;   // 1 BYTE -- koi swap NAHI chahiye (single byte ka "order" nahi hota)
```

**Single-byte fields (`side`, `msg_type`) ko kabhi swap mat karo** — swap
sirf **multi-byte** values ke liye meaningful hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — ek field swap karna bhool jaana
Result: **ek specific field** galat, baaki sahi — debug karna confusing
hai kyunki sirf ek symptom dikhta ("price galat aa raha, qty theek hai").
Systematic approach: har multi-byte field ke liye ek accessor function
banao jo hamesha swap kare, kabhi raw field seedha mat access karo.

### Trap 2 — signed integers ko galat swap karna
`std::int64_t` ka swap `std::uint64_t` jaisa hi bytes-reverse hai (sign
bit bhi bytes mein hi hai) — par **type-punning** carefully karna padta
(`net_to_host_i64` mein `memcpy` se `uint64_t` mein convert, swap, wapas
`memcpy` se `int64_t` mein — koi UB-prone direct reinterpret_cast nahi).

### Trap 3 — double-swap karna (already-swapped value ko phir se swap)
Swap apna hi inverse hai (`swap(swap(x)) == x`) — agar galti se ek field
DO baar swap ho jaaye (jaise ek helper function jo already-hosted value
lekar phir se swap kare), result phir se galat wire-order mein aa jaata,
aur bug "sahi field, GALAT reason se" jaisa dikhta — debug karna trickier.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Byteswap expensive hai | 0.753 ns/op measured — sasta |
| Single byte fields bhi swap karne chahiye | Sirf multi-byte fields — 1 byte ka "order" nahi hota |
| `reinterpret_cast` misaligned pointer se read karna safe hai x86 pe | UB hai standard ke hisaab se; `memcpy` use karo |
| Swap ek-baar decide karke bhool sakte | Har multi-byte field, har baar, systematically |

---

## Exercises

1. `AddOrderMsg` ka `side` field (`uint8_t`) galti se `net_to_host32()`
   se "swap" kiya gaya (type-mismatch bug). Kya hoga?
   <details><summary>Answer</summary>
   Compile error (type mismatch, `uint8_t` `uint32_t` accept karne wale
   function mein implicitly convert to sakta hai warning ke saath — par
   agar chalega bhi, single-byte value ko 4-byte context mein swap karna
   completely galat result dega, kyunki adjacent memory (jo `side` ke
   baad ka byte hai) bhi accidentally read ho jaayega). Fix: single-byte
   fields ko kabhi endian-conversion function se mat guzaro — woh sirf
   multi-byte fields ke liye hain.
   </details>

2. `net_to_host32` aur `host_to_net32` alag functions hain, par
   `wire_protocol.hpp` mein unki IMPLEMENTATION same hai. Kyun?
   <details><summary>Answer</summary>
   Byteswap apna khud ka inverse hai — bytes ko reverse karna, phir se
   reverse karna original wapas de deta. Isliye "host to network" aur
   "network to host" conversion EXACTLY same operation hai (dono directions
   mein swap). Alag naam rakhna sirf CODE-READABILITY ke liye hai ("yeh
   line kis direction ka conversion kar rahi hai" turant samajh aata),
   underlying operation identical hai.
   </details>

---

## Interview questions

1. Big-endian aur little-endian ka fark batao ek example ke saath.
2. `if constexpr (std::endian::native == ...)` ka kya faayda hai runtime
   `if` ke against?
3. Byteswap ki measured cost kitni hai, aur kyun yeh "bottleneck" nahi
   hai?
4. Alignment-safe read ke liye `memcpy` kyun `reinterpret_cast` se behtar
   hai, aur `-O2` iski runtime cost kya karta hai?

---

## Next
→ [`11-message-framing.md`](11-message-framing.md)
