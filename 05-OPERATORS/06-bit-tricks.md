# 06 — Bit tricks aur `<bit>` (C++20)

## Prerequisites
`05-bitwise-operators.md`

## Yeh topic abhi kyun
Bitwise basics ho gaye. Ab **practical patterns** — jo real code mein use hote hain.

Aur C++20 ka `<bit>` header, jo in sab ko standard aur fast bana deta hai.

---

## ⚠️ Pehle ek warning

Bit tricks **clever** lagti hain. Par:

```cpp
// ❌ "Clever"
x = (x & 0x55555555) + ((x >> 1) & 0x55555555);
x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
// ... 3 aur lines

// ✅ Clear aur TEZ
int count = std::popcount(x);
```

**Rules:**
1. Standard function hai to **wahi use karo** — woh CPU instruction mein compile hota hai
2. Trick tabhi likho jab **measure** karke pata ho ki faayda hai
3. Trick likho to **comment** mein explain karo

Yeh section aapko yeh tricks **samajhne** ke liye hai (interviews, purana code padhne
ke liye), roz likhne ke liye nahi.

---

## C++20 `<bit>` — pehle yeh dekho

```cpp
#include <bit>
#include <cstdint>

std::uint32_t x = 0b00101100;

std::popcount(x);          // 3   -- kitne 1 bits
std::countl_zero(x);       // 26  -- leading zeros
std::countr_zero(x);       // 2   -- trailing zeros
std::countl_one(x);        // 0
std::countr_one(x);        // 0
std::bit_width(x);         // 6   -- represent karne ko kitne bits
std::has_single_bit(x);    // false -- power of 2 hai?
std::bit_ceil(x);          // 64  -- agla power of 2
std::bit_floor(x);         // 32  -- pichla power of 2
std::rotl(x, 2);           // rotate left
std::rotr(x, 2);           // rotate right
```

**Yeh sab single CPU instructions mein compile hote hain:**

| Function | x86 instruction | Cycles |
|---|---|---|
| `popcount` | `POPCNT` | ~3 |
| `countl_zero` | `LZCNT` | ~3 |
| `countr_zero` | `TZCNT` | ~3 |
| `rotl`/`rotr` | `ROL`/`ROR` | ~1 |

Manual loop: ~32 iterations = **10-30x slower**.

⚠️ `-march=native` ya `-mpopcnt` chahiye hai in instructions ke liye — warna
compiler fallback code banata hai.

---

## Trick 1: Power of 2

```cpp
// Check
bool isPow2 = (v != 0) && ((v & (v - 1)) == 0);
// C++20
bool isPow2 = std::has_single_bit(v);
```

**Kaise kaam karta hai:**
```
   v     = 1000  (8)
   v - 1 = 0111  (7)
   v & (v-1) = 0000  ✅ power of 2

   v     = 1010  (10)
   v - 1 = 1001  (9)
   v & (v-1) = 1000  ❌ not power of 2
```

Power of 2 mein exactly ek bit set hota hai. `v-1` us bit ko clear karke neeche
sab set kar deta hai. AND se 0 aata hai.

### Round up to power of 2
```cpp
// Manual
std::uint32_t roundUpPow2(std::uint32_t v) {
    if (v == 0) return 1;
    --v;
    v |= v >> 1;   v |= v >> 2;   v |= v >> 4;
    v |= v >> 8;   v |= v >> 16;
    return v + 1;
}

// C++20
std::uint32_t result = std::bit_ceil(v);
```

> **HFT relevance:** Ring buffer size hamesha power of 2 hoti hai — taaki index wrap
> `& (size-1)` se ho, `% size` se nahi. `bit_ceil` se user ki requested size ko
> upar round kar dete hain.

---

## Trick 2: Lowest set bit

```cpp
// Lowest set bit NIKALO
std::uint32_t lowest = v & (~v + 1);     // ya v & -v (unsigned pe safe)

// Lowest set bit CLEAR karo
v &= (v - 1);

// Lowest set bit ka INDEX
int index = std::countr_zero(v);          // C++20
```

### Use: Set bits pe iterate karo
```cpp
// ✅ Sirf SET bits pe loop -- saare 32 bits pe nahi
std::uint32_t bits = 0b10010100;
while (bits) {
    const int idx = std::countr_zero(bits);
    std::cout << "Bit " << idx << " set hai\n";
    bits &= (bits - 1);                   // lowest set bit clear
}
// Output: Bit 2, Bit 4, Bit 7
```

**Yeh sparse bitsets ke liye bahut tez hai** — sirf set bits ki count jitni iterations.

---

## Trick 3: Fast modulo (power of 2)

```cpp
// Sirf jab n power of 2 ho:
index % n        ==      index & (n - 1)

// Examples
i % 8      ==   i & 7
i % 16     ==   i & 15
i % 1024   ==   i & 1023
```

**Speed:** division 20-40 cycles, AND 1 cycle.

⚠️ **Sirf unsigned ya positive signed ke liye:**
```cpp
-7 % 8         // -7
-7 & 7         //  1     ⚠️ ALAG!
```

```cpp
// ✅ Safe pattern -- ring buffer
class RingBuffer {
    static constexpr std::size_t kCapacity = 1024;    // power of 2
    static constexpr std::size_t kMask = kCapacity - 1;
    std::array<int, kCapacity> data_;
    std::size_t head_ = 0;
public:
    void push(int v) {
        data_[head_ & kMask] = v;         // ✅ 1 cycle wrap
        ++head_;
    }
};
```

---

## Trick 4: Absolute value (branchless)

```cpp
// Branch ke saath
int abs1(int x) { return x < 0 ? -x : x; }

// Branchless (32-bit)
int abs2(int x) {
    const int mask = x >> 31;          // -1 agar negative, 0 agar positive
    return (x + mask) ^ mask;
}
```

⚠️ **Practically `std::abs` use karo** — compiler usko optimal instruction mein
badal deta hai. Yeh trick sirf samajhne ke liye hai.

---

## Trick 5: Min/max (branchless)

```cpp
int min1(int a, int b) { return a < b ? a : b; }              // branch

int min2(int a, int b) { return b + ((a - b) & ((a - b) >> 31)); }   // branchless
```

⚠️ **`std::min`/`std::max` use karo.** Modern compilers `cmov` instruction
generate karte hain — jo already branchless hai.

**Verify karo godbolt pe:**
```cpp
int f(int a, int b) { return std::min(a, b); }
// -O2 pe:  cmp edi, esi / cmovle eax, edi   <- already branchless
```

---

## Trick 6: Swap without temp

```cpp
// XOR swap
a ^= b;
b ^= a;
a ^= b;
```

⚠️ **Problems:**
1. Agar `a` aur `b` **same variable** hon → dono 0 ban jaate hain
2. Modern compilers ke saath `std::swap` se **slow** hai (dependency chain)
3. Padhne mein confusing

```cpp
// ✅ Real code mein
std::swap(a, b);
```

Yeh interview question hai, production technique nahi.

---

## Trick 7: Sign check

```cpp
bool isNegative = (x < 0);                    // ✅ clear
bool isNegative = (x >> 31) != 0;             // 32-bit signed
bool sameSign = ((a ^ b) >= 0);               // dono ka sign same?
```

---

## Trick 8: Endianness swap

```cpp
// Manual (16-bit)
std::uint16_t swap16(std::uint16_t v) {
    return static_cast<std::uint16_t>((v >> 8) | (v << 8));
}

// Manual (32-bit)
std::uint32_t swap32(std::uint32_t v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0xFF000000u) >> 24);
}

// ✅ Better: compiler intrinsics (1 instruction -- BSWAP)
__builtin_bswap16(v);        // GCC/Clang
__builtin_bswap32(v);
__builtin_bswap64(v);

// ✅ Best: C++23
std::byteswap(v);

// ✅ Network byte order (<arpa/inet.h>)
htons(v);  ntohs(v);         // 16-bit host<->network
htonl(v);  ntohl(v);         // 32-bit
```

> **HFT relevance:** Market data protocols aksar **big-endian** hote hain,
> x86 servers **little-endian**. Har message field ko swap karna padta hai.
> `BSWAP` 1 cycle ka hai — manual version 8-10 cycles. Folder 38 mein.

---

## Trick 9: Bit reversal

```cpp
std::uint32_t reverseBits(std::uint32_t v) {
    v = ((v >> 1)  & 0x55555555u) | ((v & 0x55555555u) << 1);
    v = ((v >> 2)  & 0x33333333u) | ((v & 0x33333333u) << 2);
    v = ((v >> 4)  & 0x0F0F0F0Fu) | ((v & 0x0F0F0F0Fu) << 4);
    v = ((v >> 8)  & 0x00FF00FFu) | ((v & 0x00FF00FFu) << 8);
    return (v >> 16) | (v << 16);
}
```

Divide-and-conquer: pehle adjacent bits swap, phir pairs, phir nibbles, phir bytes,
phir halves.

Yeh FFT aur kuch hash functions mein use hota hai.

---

## Trick 10: Clamp to range (branchless)

```cpp
// Saturating add (overflow pe max pe ruk jao)
std::uint32_t satAdd(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t sum = a + b;
    return sum < a ? 0xFFFFFFFFu : sum;      // overflow detect
}
```

---

## Alignment tricks

```cpp
// Round UP to alignment (alignment must be power of 2)
std::size_t alignUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

// Round DOWN
std::size_t alignDown(std::size_t value, std::size_t alignment) {
    return value & ~(alignment - 1);
}

// Aligned hai?
bool isAligned(std::size_t value, std::size_t alignment) {
    return (value & (alignment - 1)) == 0;
}
```

**Examples:**
```cpp
alignUp(1, 64)    // 64
alignUp(65, 64)   // 128
alignUp(128, 64)  // 128
```

> **HFT relevance:** Memory pools aur cache-line alignment mein yeh constantly use
> hota hai. Har allocation ko 64-byte boundary pe align karna (folder 36).

---

## Hands-on

`examples/03_bit_manipulation.cpp` chalao.

```bash
cd examples
g++ -std=c++20 -O2 -march=native -Wall -Wextra 03_bit_manipulation.cpp -o bitman
./bitman
```

⚠️ `-march=native` zaroori hai `POPCNT`/`LZCNT` instructions ke liye.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Bit tricks hamesha tez hain" | Standard functions aksar tez hain (CPU instructions) |
| "XOR swap tez hai" | `std::swap` se **slow** hai |
| "Branchless hamesha better" | Predictable branches free hote hain |
| "`x % 8 == x & 7` hamesha" | Sirf non-negative values ke liye |
| "`popcount` slow hai" | 1 instruction — 3 cycles |

---

## Exercises

1. `<bit>` ke saare functions try karo:
   ```cpp
   std::uint32_t x = 0b00101100;
   // popcount, countl_zero, countr_zero, bit_width,
   // has_single_bit, bit_ceil, bit_floor, rotl, rotr
   ```

2. Power-of-2 check likho aur test karo. `std::has_single_bit` se compare karo.

3. Set bits pe iterate karne wala loop likho:
   ```cpp
   std::uint32_t bits = 0b10010100;
   // Output: Bit 2, Bit 4, Bit 7
   ```

4. `popcount` benchmark: manual loop vs `std::popcount` (10 million calls).
   `-march=native` ke saath aur bina.

5. Endianness swap likho (manual) aur `__builtin_bswap32` se compare karo.
   Godbolt pe assembly dekho — kitni instructions?

6. `alignUp` likho aur test karo:
   ```cpp
   alignUp(1, 64), alignUp(63, 64), alignUp(64, 64), alignUp(65, 64)
   ```
   <details><summary>Answers</summary>64, 64, 64, 128</details>

7. Ring buffer banao jisme size power of 2 ho aur wrap `&` se ho.

8. XOR swap ka bug reproduce karo:
   ```cpp
   int a = 5;
   int* p1 = &a;
   int* p2 = &a;      // SAME variable
   *p1 ^= *p2; *p2 ^= *p1; *p1 ^= *p2;
   std::cout << a;    // 0!
   ```

---

## Interview questions

1. Power of 2 kaise check karein?
2. `x & (x-1)` kya karta hai?
3. Set bits kaise count karein? Sabse tez tareeka?
4. Ring buffer mein power-of-2 size kyun?
5. XOR swap ke problems?
6. Endianness swap kaise karte hain?

---

## Next
→ [`07-compound-assignment.md`](07-compound-assignment.md)
