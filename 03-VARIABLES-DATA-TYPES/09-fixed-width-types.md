# 09 — Fixed-width types (`<cstdint>`)

## Prerequisites
`05-int-deep-dive.md`, `07-char-and-ascii.md`

## Yeh topic abhi kyun
Aapne dekha ki `int` ki size guaranteed nahi hai, aur `long` Linux pe 8 bytes hai
par Windows pe 4. Yeh **portability disaster** hai.

`<cstdint>` isi problem ka solution hai. Aur **HFT mein yeh mandatory hai** — kyunki
market data protocols mein har byte ka position fixed hota hai.

---

## Problem recap

```cpp
int   a;      // 2 bytes? 4 bytes? standard nahi batata
long  b;      // Linux: 8, Windows: 4  😱
char  c;      // signed? unsigned? platform pe depend
```

Ab socho aap ek market data message parse kar rahe ho:

```cpp
// ❌ KHATARNAK
struct MarketDataMessage {
    long     timestamp;      // Linux pe 8 bytes, Windows pe 4 -- STRUCT SIZE BADAL GAYA!
    int      price;
    short    quantity;
    char     side;
};
```

Yeh struct alag platforms pe **alag size** ki hogi. Aapka parser toot jaayega.

---

## Solution: `<cstdint>`

```cpp
#include <cstdint>

std::int8_t    a;      // exactly 8 bits, signed
std::int16_t   b;      // exactly 16 bits, signed
std::int32_t   c;      // exactly 32 bits, signed
std::int64_t   d;      // exactly 64 bits, signed

std::uint8_t   e;      // exactly 8 bits, unsigned
std::uint16_t  f;      // exactly 16 bits, unsigned
std::uint32_t  g;      // exactly 32 bits, unsigned
std::uint64_t  h;      // exactly 64 bits, unsigned
```

**Guarantee:** `int32_t` **hamesha** exactly 32 bits ka hoga. Har platform pe.
Har compiler pe. Ya woh type exist hi nahi karega (jo practically kabhi nahi hota).

### Ab struct safe hai

```cpp
// ✅ SAHI
#include <cstdint>

struct MarketDataMessage {
    std::uint64_t timestamp;      // hamesha 8 bytes
    std::int64_t  price;          // hamesha 8 bytes
    std::uint32_t quantity;       // hamesha 4 bytes
    std::uint8_t  side;           // hamesha 1 byte
};
```

Ab yeh struct har platform pe same layout ki hai.

---

## Poori list

### Exact width (`intN_t`) — ⭐ yahi use karo
```cpp
int8_t   uint8_t       // exactly 8 bits
int16_t  uint16_t      // exactly 16 bits
int32_t  uint32_t      // exactly 32 bits
int64_t  uint64_t      // exactly 64 bits
```
**Optional hain** technically (agar platform pe woh width exist na kare), par har
modern platform pe available hain.

### Least width (`int_leastN_t`)
```cpp
int_least8_t   uint_least8_t
int_least16_t  uint_least16_t
int_least32_t  uint_least32_t
int_least64_t  uint_least64_t
```
"Kam se kam N bits, aur sabse chhota available type." Hamesha available hote hain.

### Fastest (`int_fastN_t`)
```cpp
int_fast8_t   uint_fast8_t
int_fast16_t  uint_fast16_t
int_fast32_t  uint_fast32_t
int_fast64_t  uint_fast64_t
```
"Kam se kam N bits, aur is platform pe **sabse tez**." x86-64 pe `int_fast16_t`
actually 64 bits ka ho sakta hai (kyunki native word size tez hai).

### Pointer-sized
```cpp
intptr_t    uintptr_t      // pointer ko integer mein rakh sakta hai
```

### Maximum
```cpp
intmax_t    uintmax_t      // sabse bada available integer type
```

### Size aur difference types (`<cstddef>`)
```cpp
std::size_t       // sizeof ka result type; array indices ke liye (unsigned)
std::ptrdiff_t    // do pointers ka fark (signed)
std::byte         // C++17: raw byte (na number, na character)
```

---

## Limits

```cpp
#include <cstdint>

INT8_MIN     INT8_MAX     UINT8_MAX
INT16_MIN    INT16_MAX    UINT16_MAX
INT32_MIN    INT32_MAX    UINT32_MAX
INT64_MIN    INT64_MAX    UINT64_MAX
SIZE_MAX
```

Ya C++ way (templates ke saath kaam karta hai):
```cpp
#include <limits>
std::numeric_limits<std::int32_t>::max();
std::numeric_limits<std::uint64_t>::max();
```

---

## Literals ke suffixes

```cpp
#include <cstdint>

std::int64_t big = 9000000000;         // ⚠️ literal int ho sakta hai -> overflow
std::int64_t ok  = 9000000000LL;       // ✅ long long literal

// Ya macros use karo
std::int64_t x = INT64_C(9000000000);
std::uint64_t y = UINT64_C(18000000000);
```

---

## ⚠️ Gotcha: `int8_t` aur `uint8_t` character types hain

Yaad hai file 07 ka gotcha?

```cpp
std::uint8_t val = 65;
std::cout << val;                        // "A" print hoga, "65" nahi!
std::cout << static_cast<int>(val);      // ✅ 65
std::cout << +val;                       // ✅ 65
```

**Kyun?** `uint8_t` = `unsigned char` ka typedef hai. Aur `cout` ke liye `unsigned char`
ek character hai.

Yeh `int16_t`, `int32_t`, `int64_t` ke saath nahi hota — sirf 8-bit types ke saath.

---

## `std::byte` (C++17) — raw bytes ke liye

```cpp
#include <cstddef>

std::byte b{0x2A};

// Bitwise operations chalte hain
b = b | std::byte{0x0F};
b = b & std::byte{0xF0};
b = b << 2;

// Arithmetic NAHI chalta -- yeh feature hai
// b = b + 1;                   // ❌ compile error

// Convert karne ke liye explicit cast
int val = std::to_integer<int>(b);
```

**`std::byte` ka point:** yeh batata hai ki "yeh **raw memory** hai, na number, na
character." Compiler accidental arithmetic rok deta hai.

```cpp
// Binary data ke liye best practices, order mein:
std::byte buffer[1024];         // ✅ best (C++17) -- intent clear
std::uint8_t buffer[1024];      // ✅ good
unsigned char buffer[1024];     // ✅ acceptable
char buffer[1024];              // ⚠️ signedness bug ka risk
```

---

## 🔴 HFT: yeh mandatory kyun hai

### 1. Wire protocol layout

Exchange se aane wala message ek exact byte layout mein hota hai:

```
   Offset  Size  Field
   ------  ----  -----
   0       1     Message Type
   1       8     Timestamp (nanoseconds)
   9       8     Order ID
   17      4     Price (in ticks)
   21      4     Quantity
   25      1     Side ('B' or 'S')
   ------
   Total: 26 bytes
```

Aapka struct **exactly** yehi layout ka hona chahiye:

```cpp
#include <cstdint>

#pragma pack(push, 1)          // padding mat daalo (folder 11 mein detail)
struct AddOrderMessage {
    std::uint8_t  messageType;    // offset 0,  1 byte
    std::uint64_t timestampNs;    // offset 1,  8 bytes
    std::uint64_t orderId;        // offset 9,  8 bytes
    std::int32_t  priceInTicks;   // offset 17, 4 bytes
    std::uint32_t quantity;       // offset 21, 4 bytes
    std::uint8_t  side;           // offset 25, 1 byte
};
#pragma pack(pop)

static_assert(sizeof(AddOrderMessage) == 26, "Message layout galat hai!");
```

**`static_assert`** compile time pe check karta hai. Agar layout galat hua, build fail
hoga — production mein bug nahi jaayega.

Agar aapne `int` aur `long` use kiya hota, to yeh struct alag platform pe alag size ki
hoti, aur aap **galat bytes se galat prices** parse karte.

### 2. Overflow ke bare mein pakka pata

```cpp
std::uint64_t orderId;      // 0 se 18,446,744,073,709,551,615
                            // ek exchange ek din mein ~10^9 orders bhejta hai
                            // 18 quintillion mein overflow practically impossible
```

Agar `int` use kiya hota (2.1 arab max), to 3 din mein overflow ho jaata.

### 3. Cache line planning

```cpp
struct alignas(64) HotData {      // exactly ek cache line
    std::uint64_t bestBid;        // 8
    std::uint64_t bestAsk;        // 8
    std::uint32_t bidQty;         // 4
    std::uint32_t askQty;         // 4
    std::uint64_t lastUpdateNs;   // 8
    // 32 bytes used, 32 bytes padding
};
static_assert(sizeof(HotData) == 64);
```

Aap exactly plan kar sakte ho ki kya-kya ek cache line mein aayega. Yeh `int` ke saath
possible nahi (kyunki size guaranteed nahi).

Folder 32 aur 39 mein poora.

---

## Kab kya use karein — practical guide

| Situation | Use karo |
|---|---|
| **Wire protocols, file formats, structs jo serialize hote hain** | `int32_t`, `uint64_t` (exact width) |
| **Array index / size** | `std::size_t` |
| **Do pointers ka fark** | `std::ptrdiff_t` |
| **Raw binary data** | `std::byte` (C++17) ya `uint8_t` |
| **Loop counter (chhota)** | `int` theek hai |
| **Generic code** | `auto` ya template parameter |
| **Bahut speed-critical local variable** | `int_fast32_t` (rarely matters) |
| **Paisa / price** | `int64_t` (ticks ya paise mein) |

### Simple rule
> **Agar size matter karti hai (memory layout, protocol, overflow) → fixed-width use karo.
> Agar sirf "ek number chahiye" → `int` theek hai.**

---

## Namespace ka note

```cpp
#include <cstdint>
std::int32_t a;      // ✅ technically sahi (std namespace mein)
int32_t b;           // ✅ practically bhi kaam karta hai (global mein bhi hote hain)
```

Standard kehta hai `<cstdint>` inhe `std::` mein daalta hai, aur global mein daalna
optional hai. Practically har implementation dono jagah daalta hai.

**Salah:** `std::` likho — technically sahi hai aur clear hai. Par team convention
follow karo.

---

## Hands-on

`examples/07_fixed_width.cpp` chalao:

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 07_fixed_width.cpp -o fw && ./fw
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int` hi kaafi hai" | Portable code aur protocols mein nahi |
| "`int32_t` aur `int` same hain" | Practically x86 pe haan, par guarantee `int32_t` deta hai |
| "`uint8_t` number ki tarah print hoga" | ❌ Character print hoga |
| "`int_fast32_t` hamesha tez hai" | Aksar koi fark nahi. `int32_t` se shuru karo |
| "`std::byte` par arithmetic kar sakte ho" | Nahi — jaan-boojh kar rokа gaya hai |

---

## Exercises

1. Yeh struct portable banao:
   ```cpp
   struct Trade {
       long   timestamp;
       int    price;
       short  quantity;
       char   side;
   };
   ```
   <details><summary>Answer</summary>

   ```cpp
   #include <cstdint>
   struct Trade {
       std::uint64_t timestampNs;
       std::int64_t  priceInTicks;
       std::uint32_t quantity;
       std::uint8_t  side;
   };
   ```
   </details>

2. `static_assert` add karo jo verify kare ki `Trade` exactly 24 bytes ka hai
   (padding ke saath). Kya sizeof match hua? Agar nahi, kyun?
   <details><summary>Hint</summary>
   Padding! `sizeof` 24 aayega (alignment ke kaaran 21 nahi). Folder 11 mein detail.
   </details>

3. Saare fixed-width types ki sizes aur ranges print karo.

4. `uint8_t` printing trap test karo:
   ```cpp
   std::uint8_t a = 200;
   std::cout << a << " " << +a << " " << static_cast<int>(a) << "\n";
   ```

5. `std::byte` try karo:
   ```cpp
   #include <cstddef>
   std::byte b{0xAB};
   // b = b + 1;                    // uncomment -> error dekho
   std::cout << std::to_integer<int>(b) << "\n";
   ```

6. Ek 26-byte market data message struct banao (upar wala layout) aur `static_assert`
   se verify karo.

---

## Interview questions

1. `int32_t` aur `int` mein kya fark hai?
2. Wire protocols mein fixed-width types kyun zaroori hain?
3. `size_t` kya hai aur kab use karna chahiye?
4. `std::byte` kya hai aur `uint8_t` se kya fark?
5. `int_fast32_t` kab use karenge?

---

## Next
→ [`10-initialization-forms.md`](10-initialization-forms.md)
