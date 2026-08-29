# 05 — Bitwise operators

## Prerequisites
Folder 01 lesson 10 (bits, binary, two's complement), folder 03 file 09 (fixed-width types)

## Yeh topic abhi kyun
**Yeh is folder ka sabse important lesson hai.**

Bitwise operations har jagah hain:
- Market data protocols (flags, packed fields)
- Permissions aur options
- Hash functions
- Compression
- Low-level optimization
- Ring buffer indexing

Aur HFT mein yeh **roz ka kaam** hai.

---

## Six operators

```cpp
a & b       // AND      -- dono bits 1?
a | b       // OR       -- koi ek bit 1?
a ^ b       // XOR      -- bits ALAG hain?
~a          // NOT      -- saare bits ulte
a << n      // LEFT SHIFT  -- bits n jagah left
a >> n      // RIGHT SHIFT -- bits n jagah right
```

⚠️ Yeh **bit-by-bit** kaam karte hain, poore number pe nahi.

---

## AND (`&`) — masking

```
   Rule: dono bits 1 hon to 1, warna 0

   1100      (12)
 & 1010      (10)
   ----
   1000      (8)
```

```cpp
std::cout << (12 & 10);      // 8
```

### Use 1: Specific bit check karna
```cpp
bool isBitSet(std::uint32_t value, int bit) {
    return (value & (1u << bit)) != 0;
}
```

### Use 2: Bits clear karna (mask)
```cpp
std::uint32_t x = 0xABCD;
std::uint32_t lowByte = x & 0xFF;         // sirf lowest 8 bits -> 0xCD
std::uint32_t highBits = x & 0xFF00;      // -> 0xAB00
```

### Use 3: Modulo by power of 2 (FAST)
```cpp
// Yeh dono same hain (jab n power of 2 ho):
index % 8            // division -- 20-40 cycles
index & 7            // bitwise  -- 1 cycle       ✅ 20-40x faster

// General: x % (2^n)  ==  x & (2^n - 1)
index % 16   ==  index & 15
index % 64   ==  index & 63
index % 1024 ==  index & 1023
```

**⚠️ Sirf unsigned aur positive values ke liye safe hai.**

> **HFT relevance:** Ring buffers ka size **hamesha power of 2** rakha jaata hai,
> sirf isliye ki index wrap `& (size-1)` se ho — division ki jagah. Folder 28/36.

---

## OR (`|`) — bits set karna

```
   Rule: koi ek bit 1 ho to 1

   1100      (12)
 | 1010      (10)
   ----
   1110      (14)
```

### Use: Flags combine karna
```cpp
std::uint32_t flags = FLAG_READ | FLAG_WRITE | FLAG_EXECUTE;
```

### Use: Specific bit set karna
```cpp
value |= (1u << bit);        // bit ko 1 kar do
```

---

## XOR (`^`) — toggle aur tricks

```
   Rule: bits ALAG hon to 1, same hon to 0

   1100      (12)
 ^ 1010      (10)
   ----
   0110      (6)
```

### Properties (yeh yaad rakho)
```
   a ^ a = 0           // apne saath XOR = 0
   a ^ 0 = a           // 0 ke saath XOR = wahi
   a ^ b ^ b = a       // do baar XOR = wapas original  ← yeh magic hai
```

### Use 1: Bit toggle
```cpp
value ^= (1u << bit);        // bit ulta kar do (0->1, 1->0)
```

### Use 2: Swap without temp (interview classic)
```cpp
a ^= b;
b ^= a;
a ^= b;
// ⚠️ Agar a aur b SAME variable hon to dono 0 ban jaate hain!
// Real code mein std::swap use karo.
```

### Use 3: Find the odd one out
```cpp
// Array mein har number 2 baar hai, ek number 1 baar. Woh dhoondho.
int findSingle(const std::vector<int>& v) {
    int result = 0;
    for (int x : v) result ^= x;      // pairs cancel ho jaate hain
    return result;
}
```

### Use 4: Simple checksum
```cpp
std::uint8_t checksum = 0;
for (std::uint8_t byte : message) checksum ^= byte;
```

Yeh bahut se binary protocols mein use hota hai.

---

## NOT (`~`) — saare bits ulte

```
   ~00001100  (12)
   =11110011  (-13 in two's complement, 8-bit signed)
```

```cpp
std::uint8_t a = 12;
std::cout << +static_cast<std::uint8_t>(~a);      // 243
```

### ⚠️ Signed types ke saath dhyaan
```cpp
int x = 12;
std::cout << ~x;              // -13  (two's complement)
```

**Rule:** `~x == -x - 1` (signed two's complement mein)

### Use: Bit clear karna
```cpp
value &= ~(1u << bit);       // bit ko 0 kar do
//        ^^^^^^^^^^^^  us bit ke alawa sab 1
```

---

## LEFT SHIFT (`<<`) — bits left, multiply by 2

```
   00000101  (5)
   << 1
   00001010  (10)     -- 5 * 2

   << 2
   00010100  (20)     -- 5 * 4
```

```cpp
x << n      ==      x * 2^n        (jab tak overflow na ho)
```

```cpp
1 << 0      // 1
1 << 1      // 2
1 << 2      // 4
1 << 3      // 8
1 << 10     // 1024
1 << 20     // 1048576  (1 MB)
```

### Use: Bit masks banana
```cpp
constexpr std::uint32_t FLAG_A = 1u << 0;      // 0b0001
constexpr std::uint32_t FLAG_B = 1u << 1;      // 0b0010
constexpr std::uint32_t FLAG_C = 1u << 2;      // 0b0100
constexpr std::uint32_t FLAG_D = 1u << 3;      // 0b1000
```

### ⚠️ Shift ke UB traps

```cpp
int x = 1;
x << 32;              // ⚠️ UB! (int 32-bit hai, shift >= width = UB)
x << -1;              // ⚠️ UB! (negative shift)

int neg = -1;
neg << 1;             // ⚠️ UB C++17 se pehle; C++20 mein defined
```

**Rules:**
- Shift amount **`0 <= n < bit_width`** hona chahiye
- C++20 se pehle signed left shift jo sign bit overflow kare = UB
- C++20 mein signed shifts well-defined hain (two's complement guaranteed)

```cpp
// ✅ Safe
if (n < static_cast<int>(sizeof(x) * CHAR_BIT)) {
    result = x << n;
}
```

---

## RIGHT SHIFT (`>>`) — bits right, divide by 2

```
   00010100  (20)
   >> 1
   00001010  (10)     -- 20 / 2

   >> 2
   00000101  (5)      -- 20 / 4
```

### ⚠️ Signed vs unsigned mein ALAG behaviour

```cpp
unsigned int u = 0x80000000;    // 1000...0000
u >> 1;                          // 0100...0000  -- LOGICAL shift (0 aata hai)

int s = -8;                      // 1111...1000
s >> 1;                          // 1111...1100 = -4  -- ARITHMETIC shift (sign bit copy)
```

| Type | Shift type | Kya aata hai left se |
|---|---|---|
| **unsigned** | logical | hamesha `0` |
| **signed** | arithmetic (implementation-defined, par har real compiler pe) | sign bit ki copy |

**C++20 se signed right shift arithmetic guaranteed hai.**

### ⚠️ Division se fark

```cpp
-7 / 2      // -3   (zero ki taraf truncate)
-7 >> 1     // -4   (negative infinity ki taraf!)
```

**`>>` `/` ke barabar nahi hai negative numbers ke liye.**

```cpp
// ✅ Safe: sirf unsigned pe shift-as-division use karo
unsigned int half = value >> 1;
```

---

## Bit manipulation ka toolkit

```cpp
#include <cstdint>

// 1. BIT SET karo (0 -> 1)
value |= (1u << bit);

// 2. BIT CLEAR karo (1 -> 0)
value &= ~(1u << bit);

// 3. BIT TOGGLE karo
value ^= (1u << bit);

// 4. BIT TEST karo
bool isSet = (value & (1u << bit)) != 0;

// 5. BIT ko specific value do (branchless)
value = (value & ~(1u << bit)) | (static_cast<unsigned>(cond) << bit);

// 6. Lowest set bit nikalo
unsigned lowest = value & (~value + 1);     // ya value & -value

// 7. Lowest set bit CLEAR karo
value &= (value - 1);

// 8. Power of 2 check
bool isPow2 = value != 0 && (value & (value - 1)) == 0;

// 9. Round up to power of 2
unsigned roundUp(unsigned v) {
    --v;
    v |= v >> 1;  v |= v >> 2;  v |= v >> 4;
    v |= v >> 8;  v |= v >> 16;
    return v + 1;
}

// 10. Saare bits set
value = ~0u;                                // ya 0xFFFFFFFF
```

---

## C++20 `<bit>` — ab yeh sab standard mein hai

```cpp
#include <bit>

std::popcount(x);            // kitne 1 bits hain
std::countl_zero(x);         // leading zeros
std::countr_zero(x);         // trailing zeros
std::countl_one(x);          // leading ones
std::countr_one(x);          // trailing ones
std::bit_width(x);           // kitne bits chahiye x represent karne ko
std::has_single_bit(x);      // power of 2 hai?
std::bit_ceil(x);            // agla power of 2 (>=)
std::bit_floor(x);           // pichla power of 2 (<=)
std::rotl(x, n);             // rotate left
std::rotr(x, n);             // rotate right
std::byteswap(x);            // C++23 -- endianness ke liye
std::bit_cast<T>(x);         // safe type punning
```

**Yeh sab CPU instructions mein compile hote hain** (`POPCNT`, `LZCNT`, `TZCNT`) —
manual loops se **bahut** tez.

```cpp
// ❌ Manual -- ~32 iterations
int countBits(std::uint32_t v) {
    int count = 0;
    while (v) { count += v & 1; v >>= 1; }
    return count;
}

// ✅ std::popcount -- 1 CPU instruction (3 cycles)
int count = std::popcount(v);
```

---

## Practical: bit flags

```cpp
#include <cstdint>

enum class OrderFlag : std::uint32_t {
    None        = 0,
    IsBuy       = 1u << 0,
    IsLimit     = 1u << 1,
    IsIOC       = 1u << 2,
    IsFOK       = 1u << 3,
    IsHidden    = 1u << 4,
    IsPostOnly  = 1u << 5,
};

// enum class ke liye operators define karne padte hain
constexpr OrderFlag operator|(OrderFlag a, OrderFlag b) {
    return static_cast<OrderFlag>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
constexpr OrderFlag operator&(OrderFlag a, OrderFlag b) {
    return static_cast<OrderFlag>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}
constexpr bool hasFlag(OrderFlag value, OrderFlag flag) {
    return (value & flag) == flag;
}

// Use
OrderFlag flags = OrderFlag::IsBuy | OrderFlag::IsLimit | OrderFlag::IsIOC;
if (hasFlag(flags, OrderFlag::IsIOC)) { /* ... */ }
```

**Faayda:** 6 flags **ek hi `uint32_t`** mein — 6 alag `bool` (6 bytes + padding)
ki jagah **4 bytes**. Cache mein zyada orders fit honge.

---

## Practical: packed fields

```cpp
// Ek 32-bit word mein 3 fields pack karo
//   bits 0-15  : price (16 bits)
//   bits 16-27 : quantity (12 bits)
//   bits 28-31 : flags (4 bits)

constexpr std::uint32_t PRICE_MASK  = 0x0000FFFF;
constexpr std::uint32_t QTY_SHIFT   = 16;
constexpr std::uint32_t QTY_MASK    = 0x0FFF0000;
constexpr std::uint32_t FLAGS_SHIFT = 28;

std::uint32_t pack(std::uint16_t price, std::uint16_t qty, std::uint8_t flags) {
    return static_cast<std::uint32_t>(price)
         | (static_cast<std::uint32_t>(qty)   << QTY_SHIFT)
         | (static_cast<std::uint32_t>(flags) << FLAGS_SHIFT);
}

std::uint16_t getPrice(std::uint32_t packed) {
    return static_cast<std::uint16_t>(packed & PRICE_MASK);
}
std::uint16_t getQty(std::uint32_t packed) {
    return static_cast<std::uint16_t>((packed & QTY_MASK) >> QTY_SHIFT);
}
```

Yeh market data protocols mein bahut common hai.

---

## Hands-on

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 02_bitwise_basics.cpp -o bits && ./bits
g++ -std=c++20 -Wall -Wextra 03_bit_manipulation.cpp -o bitman && ./bitman
g++ -std=c++20 -Wall -Wextra 06_bit_flags.cpp -o flags && ./flags
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`&` aur `&&` same hain" | `&` bitwise, `&&` logical + short-circuit |
| "`>>` division jaisa hai" | Negative numbers ke liye alag (`-7>>1 = -4`, `-7/2 = -3`) |
| "Shift kitna bhi kar sakte ho" | `n >= bit_width` = **UB** |
| "`~x` `-x` jaisa hai" | `~x == -x - 1` |
| "Bit counting loop se karo" | `std::popcount` — 1 instruction |

---

## Exercises

1. Binary mein solve karo (kagaz pe), phir code se verify:
   ```
   12 & 10 = ?      12 | 10 = ?      12 ^ 10 = ?
   ~12 (8-bit) = ?  5 << 3 = ?       20 >> 2 = ?
   ```
   <details><summary>Answers</summary>8, 14, 6, 243 (as uint8_t), 40, 5</details>

2. Yeh functions likho:
   ```cpp
   bool isBitSet(std::uint32_t v, int bit);
   std::uint32_t setBit(std::uint32_t v, int bit);
   std::uint32_t clearBit(std::uint32_t v, int bit);
   std::uint32_t toggleBit(std::uint32_t v, int bit);
   ```

3. Power-of-2 check likho aur test karo (1, 2, 3, 4, 7, 8, 16, 100, 1024).

4. `%` vs `&` benchmark:
   ```cpp
   // 100 million iterations
   // Version 1: i % 1024
   // Version 2: i & 1023
   // -O2 ke saath timing compare karo
   ```
   <details><summary>Note</summary>
   `-O2` pe compiler compile-time constant `1024` ke liye khud yeh optimization
   kar deta hai. Fark dekhne ke liye divisor ko `volatile` banao ya runtime se lo.
   </details>

5. XOR se "single number" problem solve karo:
   ```cpp
   // {4, 1, 2, 1, 2} -> 4
   ```

6. `std::popcount` vs manual loop benchmark.

7. Order flags system banao (upar wala example) aur test karo.

8. Pack/unpack functions likho aur round-trip verify karo.

9. Signed right shift ka behaviour test karo:
   ```cpp
   std::cout << (-8 >> 1) << " " << (-7 >> 1) << " " << (-7 / 2) << "\n";
   ```

---

## Interview questions

1. `x % 8` ko bitwise mein kaise likhein? Kab safe hai?
2. Power of 2 kaise check karein?
3. Signed aur unsigned right shift mein fark?
4. `x & (x-1)` kya karta hai?
5. XOR ki 3 properties batao.
6. Bit flags ka faayda `bool` array ke mukable?

---

## Next
→ [`06-bit-tricks.md`](06-bit-tricks.md)
