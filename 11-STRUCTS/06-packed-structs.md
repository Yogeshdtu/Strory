# 06 — Packed structs

## Prerequisites
- [`05-padding-and-alignment.md`](05-padding-and-alignment.md)
- `09-ARRAYS/05-array-decay.md`, `10-STRINGS/01-c-strings.md` (raw bytes)

## Yeh topic abhi kyun
Kabhi aapko struct mein **bilkul koi padding nahi** chahiye — jab woh struct ek
network packet ya file format ke exact bytes represent karta hai. `#pragma pack`
/ `__attribute__((packed))` yeh karte hain. Par unaligned access ka trade-off
samajhna zaroori hai — aur yeh bhi ki dono tareeqe compiler ki nazar mein
**ek jaise nahi** hain.

---

## `#pragma pack` / `[[gnu::packed]]`

```cpp
#pragma pack(push, 1)                       // "alignment = 1 -> koi padding nahi"
struct WireMsg {
    std::uint8_t  type;      // offset 0
    std::uint32_t seq;       // offset 1  (normally 4 -- par packed hai, isliye 1)
    std::int64_t  price;     // offset 5  (normally 8)
    std::uint16_t qty;       // offset 13
};                           // sizeof == 15  (24 nahi)
#pragma pack(pop)

// GCC/Clang ka doosra tareeqa:
struct [[gnu::packed]] WireMsg2 { ... };
// ya:  struct __attribute__((packed)) WireMsg3 { ... };
```

`pack(1)` → har member ekdum sata hua, `sizeof` = members ka exact jod, `alignof` = 1.
(GCC 16.2 pe chala ke: `sizeof(WireMsg)` 15, `alignof` 1, offsets 0/1/5/13.)
`examples/08_market_data_struct.cpp` — 32-byte packed `QuoteMsg`, `static_assert`s ke saath.

Analogy: suitcase mein kapde tah karke rakhna (normal struct — har cheez apne khaane mein, beech mein
jagah) vs sab kuch thoons dena (packed — jagah zero, par nikaalte waqt cheez ulti-seedhi milti hai).

---

## Trade-off: UNALIGNED access

Padding nahi hai, to `seq` offset 1 pe baitha hai — ek **misaligned** `uint32_t`.

```cpp
WireMsg m;
std::uint32_t s = m.seq;                    // odd address se uint32 padh rahe hain
```

- **x86-64**: member ka naam leke (`m.seq`) padhna *chalta hai* — compiler jaanta hai field packed hai aur
  normal `mov` nikaalta hai (x86 unaligned scalar load bina fault ke kar leta hai).
- **ARM / purane cores / aligned SIMD**: unaligned access **fault (SIGBUS)** kar sakta hai ya bahut slow ho
  sakta hai.
- **`&m.seq`** ka type `uint32_t*` hai par woh unaligned address pe point karta hai → usko dereference karna
  (ya aisi jagah dena jo alignment maan ke chalti hai) **UB** hai.

```cpp
std::uint32_t* p = &m.seq;                  // ⚠️ p misaligned hai
std::uint32_t s;
std::memcpy(&s, &m.seq, sizeof(s));         // ✅ hamesha safe -- memcpy kisi bhi alignment se copy karta hai
```

### ⚠️ Compiler kab bachata hai — `#pragma pack` vs `[[gnu::packed]]` (GCC 16.2 pe chala ke)

| Code | `[[gnu::packed]]` / `__attribute__((packed))` | `#pragma pack(push, 1)` |
|---|---|---|
| `alignof(struct)` | 1 | 1 |
| `uint32_t* p = &m.a;` | ⚠️ `taking address of packed member of 'G' may result in an unaligned pointer value [-Waddress-of-packed-member]` (bina kisi flag ke, default on) | **koi warning nahi** |
| `uint32_t& r = m.a;` | ❌ `error: cannot bind packed field 'g.G::a' to 'uint32_t&'` | **chupchaap compile** |

Dono ka layout same hai, par GCC `-Waddress-of-packed-member` aur reference-binding error **sirf attribute
wale struct pe** deta hai. `#pragma pack` wale struct pe wahi UB bina kisi aawaz ke nikal jaata hai.

Practical rule: GCC/Clang pe wire structs ke liye **`[[gnu::packed]]` behtar hai** (compiler pointer galtiyan
pakadta hai). MSVC ke saath bhi chalana hai to `#pragma pack` hi portable hai — tab `&member` kabhi mat lo,
aur code review/`memcpy` discipline pe bharosa karo.

---

## Safe pattern: `memcpy`, `reinterpret_cast` nahi

```cpp
// ❌ risky: strict-aliasing UB + unaligned deref
const WireMsg* m = reinterpret_cast<const WireMsg*>(recvBuffer);
process(m->price);

// ✅ safe: bytes ko ek properly-aligned local mein copy karo
WireMsg m;
std::memcpy(&m, recvBuffer, sizeof(m));     // aliasing UB nahi; memcpy alignment sambhalta hai
process(m.price);                            // m ek normal local hai -> aligned
```

`std::memcpy` efficient code banata hai (`-O2` pe aksar kuch `mov` / ek `movups`), aur bytes ko dobara
interpret karne ka yahi standard-approved tareeqa hai. (Folder 25: aliasing, `std::bit_cast`,
`std::start_lifetime_as` C++23 — GCC 16.2 mein available.)

---

## `alignas` vs `pack` — ulti dishayein

| | `alignas(N)` | `#pragma pack(1)` / `[[gnu::packed]]` |
|---|---|---|
| Disha | alignment **badhao** / padding jodo | padding / alignment **hatao** |
| Kaam | false sharing se bachav, SIMD | exact wire/file layout |
| `sizeof` | ≥ natural, N ka multiple | members ka exact jod |
| Access | aligned, normal | naam se padhna theek; `&member` pointer UB |

---

## Kab pack karo (aur kab nahi)

**Pack karo:**
- Wire protocols jahan layout aapke haath mein nahi (exchange native formats, ITCH/OUCH/FIX-binary, file headers).
- Memory ki tangi wala embedded.

**Pack mat karo:**
- Internal structs — members reorder karo (file 05); padding ka bada hissa chala jaata hai aur pointer UB ka
  koi khatra nahi.
- Koi bhi struct jiske `&member` lekar idhar-udhar dete ho.
- SIMD-heavy loops jo aligned data maan ke chalte hain.

**Dono ka faayda:** wire ke liye packed struct + processing ke liye naturally-ordered internal struct mein `memcpy`.

### Naapa hua: x86 pe packed hamesha slow nahi
`struct PackedM { uint8_t t; uint32_t a; uint64_t b; }` (packed, 13 bytes) vs wahi fields reorder karke
natural `AlignedM` (16 bytes). 20M elements, `a` ka sum, 10 passes, `-O2`, GCC 16.2:

| | Time (2 runs × 3 rounds) |
|---|---|
| packed (13 bytes) | 179–212 ms |
| aligned (16 bytes) | 233–261 ms |

**Packed ~20% TEZ nikla.** Assembly dekha: dono loops bilkul same — `mov ecx, [rax]` / `add rax, 13` vs
`add rax, 16`. Unaligned load ka x86 pe koi extra instruction nahi; farq sirf itna hai ki 13-byte records
kam memory bandwidth khaate hain. Sabak: **"packed = slow" ek aadha-sach hai** — asli khatra speed nahi,
misaligned *pointer* ka UB aur non-x86 portability hai. (Benchmark likhte waqt pehle `[[gnu::noinline]]` use
kiya tha — GCC ne function ko pure maan ke 10 calls ek mein hoist kar diye aur ~20 ms dikhaye; `[[gnu::noipa]]`
se sahi number aaya. Yeh bhi Rule 2 ka sabak hai.)

---

## Andar kya hota hai

- `#pragma pack(1)` compiler ko bolta hai: is struct ke members ki max alignment = 1 → har member ka offset
  = pichhle ka end (koi rounding nahi). `sizeof` = jod.
- Packed multi-byte member ka load → compiler unaligned load banata hai (x86 pe normal `mov`; strict-alignment
  targets pe byte-by-byte ya `memcpy` jaisa sequence).
- `&packedMember` → aisa pointer jise type system aligned samajhta hai par hai nahi → alignment maan ke chalne
  wale code (SIMD, kuch libraries) ko dena UB.
- `memcpy(&local, &packedMember, n)` → compiler jaanta hai source ki alignment 1 hai aur sahi copy banata hai
  (aam taur pe ek `mov`).

> **HFT relevance:** Exchange ke native market-data / order-entry protocols **packed binary** hote hain.
> Decoder: received bytes ko `static_assert`-verified packed struct mein `memcpy` karo (ya known offsets se
> `memcpy` karke fields padho), endianness ke liye byteswap (file 08), phir naturally-aligned internal
> representation pe kaam. `reinterpret_cast<Msg*>(buffer)` se bacha jaata hai (aliasing UB + unaligned
> pointers). `-Waddress-of-packed-member` ko `-Werror` rakhte hain — **par yaad rahe, GCC woh warning
> `#pragma pack` structs pe nahi deta**, isliye GCC/Clang-only codebases wire structs `[[gnu::packed]]` se
> likhte hain. x86 pe unaligned read ki penalty negligible hai (upar naapa); *pointer* UB asli khatra hai.
> Folders 25, 38, 42.

---

## Hands-on

`examples/08_market_data_struct.cpp` — packed `QuoteMsg`, `static_assert`s, `memcpy` decode, byteswap:

```bash
./build.ps1 11-STRUCTS/examples/08_market_data_struct.cpp
g++ -std=c++20 -Wall -Wextra 11-STRUCTS/examples/08_market_data_struct.cpp -o md
# koi warning nahi -- hum memcpy karte hain, &member nahi lete.
# (Dhyaan: yeh #pragma pack use karta hai, jahan GCC waise bhi packed-member warning nahi deta.)
```

---

## ⚠️ Traps

### Trap 1 — `&packedMember`
```cpp
std::uint32_t* p = &msg.seq;   // ⚠️ misaligned pointer. [[gnu::packed]] pe warning; #pragma pack pe CHUP. memcpy karo
```

### Trap 2 — `reinterpret_cast<Msg*>(buffer)`
```cpp
auto* m = reinterpret_cast<Msg*>(buf);  // ⚠️ aliasing UB + m->field unaligned deref
```

### Trap 3 — internal structs ko "size ke liye" pack karna
```cpp
#pragma pack(1)
struct HotThing { ... };   // ⚠️ &member pointers UB, non-x86 pe fault. Pehle reorder (file 05)
```

### Trap 4 — `#pragma pack(pop)` bhoolna
```cpp
#pragma pack(push, 1)
struct A { ... };
// pop nahi -> iske baad ka HAR struct bhi packed
struct B { char c; double d; };   // sizeof 9 (16 hona chahiye tha) -- chala ke dekha
```

### Trap 5 — maan lena ki `sizeof(packed) == wire size` har compiler pe
`pack(1)` ke saath aksar sach, par `static_assert` phir bhi lagao (aur `long`/`int` ke size ke farq se bacho —
fixed-width types use karo).

### Trap 6 — `#pragma pack` struct pe warning na aana = safe
```cpp
#pragma pack(push, 1)
struct M { std::uint8_t t; std::uint32_t a; };
#pragma pack(pop)
std::uint32_t& r = m.a;   // ⚠️ GCC 16.2: koi error/warning nahi. [[gnu::packed]] hota to ❌ error
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Memory bachane ke liye sab pack karo" | Internal structs: reorder. Pack sirf wire structs |
| "x86 pe unaligned access bilkul theek hai" | Naam se padhna theek; `&member` pointer UB |
| "Packed struct hamesha slow" | x86 scalar scan pe naapa: packed 13-byte ~20% tez (kam bytes) |
| "`reinterpret_cast<Msg*>(buf)` hi decode ka tareeqa hai" | Struct mein `memcpy` — aliasing/alignment UB nahi |
| "`#pragma pack` aur `[[gnu::packed]]` bilkul same" | Layout same; par GCC warning/error sirf attribute wale pe |
| "Sirf `#pragma pack(push,1)` kaafi hai" | `pop` chahiye — warna aage ke structs mein leak |
| "Packed `sizeof` portable hai" | `static_assert` karo; fixed-width types |

---

## Exercises

1. **Pack karo:** `struct M { std::uint8_t t; std::uint32_t a; std::uint64_t b; };` — `sizeof` unpacked vs
   `#pragma pack(1)`. Dono tarah har member ka offset.
   <details><summary>Answer (GCC 16.2 pe chala ke)</summary>

   Unpacked: **16** bytes, offsets 0 / 4 / 8. Packed: **13** bytes, offsets 0 / 1 / 5.
   </details>

2. **`memcpy` decode:** ek `std::array<std::byte, 13>` buffer diya hai — packed offsets pe `memcpy` se `t`, `a`,
   `b` nikaalo. `reinterpret_cast` nahi.

3. **Packed-member warning:** packed struct pe `std::uint32_t* p = &m.a;` — `-Wall` pe warning aayi? Ab struct
   ko `#pragma pack` ki jagah `[[gnu::packed]]` se banao — ab? Aur `std::uint32_t v; std::memcpy(&v, &m.a, 4);` —
   warning?
   <details><summary>Answer (GCC 16.2)</summary>

   `#pragma pack` struct: **koi warning nahi**. `[[gnu::packed]]`: `taking address of packed member ... may
   result in an unaligned pointer value [-Waddress-of-packed-member]` (default on, `-Wall` ki zaroorat nahi).
   `memcpy(&v, &m.a, 4)`: dono pe koi warning nahi — `void*` mein convert hone pe alignment ka sawaal nahi.
   </details>

4. **Pop leak:** `#pragma pack(push,1) struct A{...};` (pop nahi) phir `struct B{char c; double d;};` —
   `sizeof(B)`? `pop` jodo — ab `sizeof(B)`?
   <details><summary>Answer</summary>

   Leak ke saath **9**; `pop` ke baad **16**. (Chala ke dekha.)
   </details>

5. **Wire ↔ internal:** packed `WirePrice { uint8_t exp; int64_t mantissa; }` aur internal
   `Price { double value; };` — `memcpy` decode + convert.

6. **x86 penalty:** `std::vector<PackedM>` pe `m.a` ka sum vs `std::vector<AlignedM>` (wahi fields, reorder,
   natural). `-O2`, time lo. Apni machine pe bada fark? **Benchmark function pe `[[gnu::noipa]]` lagao**, warna
   GCC repeat calls hoist kar sakta hai.
   <details><summary>Answer (is machine pe)</summary>

   20M elements × 10 passes: packed 179–212 ms, aligned 233–261 ms — **packed ~20% tez**. Loops ka assembly
   same hai, sirf stride 13 vs 16. Unaligned load ki x86 pe koi cost nahi dikhi; chhote records = kam memory
   traffic. Ulti expectation — isiliye naapna zaroori hai.
   </details>

---

## Interview questions

1. `#pragma pack(1)` kya karta hai? `sizeof` / `alignof` pe asar?
2. Packed struct member ka `&` lena kyun problematic?
3. `reinterpret_cast<Msg*>(buffer)` vs `memcpy(&msg, buffer, ...)` — kyun `memcpy`?
4. x86 pe unaligned scalar access — kya hota hai? ARM pe?
5. Internal struct ka size kam karna hai — pack karo ya reorder? Kyun?
6. `#pragma pack(push, 1)` ke saath `pop` kyun zaroori?
7. GCC pe `#pragma pack` aur `[[gnu::packed]]` ke diagnostics mein kya farq hai?

---

## Next
→ [`07-aos-vs-soa.md`](07-aos-vs-soa.md)
