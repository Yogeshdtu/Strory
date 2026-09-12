# 10 — Bits, Bytes, Binary aur Hex

## Prerequisites
`01-what-is-a-computer.md`

## Yeh topic abhi kyun
Agle folder mein hum `int` padhenge — aur `int` ki **size**, **range**, aur **overflow**
samajhne ke liye binary aani chahiye. Bitwise operators (folder 05), memory layout
(folder 11, 14), aur binary market data parsing (folder 38) — sab yahin se shuru hote hain.

**Yeh lesson HFT ke liye foundational hai.** Market data protocols binary hote hain.
Agar aapko bits nahi aate, aap feed handler nahi likh sakte.

---

## Bit — sabse chhoti cheez

**Bit = Binary digIT = ek 0 ya ek 1.**

Physically yeh ek transistor hai jo on hai ya off. Voltage high hai ya low.
Bas do state. Yehi computer ki poori duniya hai.

Ek bit se aap 2 cheezein represent kar sakte ho: `0` ya `1`.

---

## Zyada bits = zyada possibilities

```
   1 bit:    0, 1                          = 2 values     = 2^1
   2 bits:   00, 01, 10, 11                = 4 values     = 2^2
   3 bits:   000..111                      = 8 values     = 2^3
   4 bits:   0000..1111                    = 16 values    = 2^4
   8 bits:   00000000..11111111            = 256 values   = 2^8
```

**Formula: n bits se 2^n alag values ban sakti hain.**

Yeh formula yaad kar lo. Yeh poore course mein use hoga.

---

## Byte

**1 byte = 8 bits.**

Byte memory ki sabse chhoti **addressable** unit hai — matlab har byte ka apna address
hota hai. Aap ek single bit ka address nahi le sakte.

| Unit | Size |
|---|---|
| 1 byte | 8 bits |
| 1 KB (kilobyte) | 1024 bytes |
| 1 MB | 1024 KB = 1,048,576 bytes |
| 1 GB | 1024 MB |
| 1 TB | 1024 GB |

*(Technically KB = 1000 aur KiB = 1024, par programming mein aksar 1024 hi maante hain.)*

---

## Binary number system

Aap **decimal** (base 10) use karte ho — 10 digits: 0-9.
Computer **binary** (base 2) use karta hai — 2 digits: 0, 1.

### Decimal kaise kaam karta hai
```
   Number: 253

   2      5      3
   |      |      |
   |      |      +-- 3 x 10^0 = 3 x 1   = 3
   |      +--------- 5 x 10^1 = 5 x 10  = 50
   +---------------- 2 x 10^2 = 2 x 100 = 200
                                          ----
                                          253
```

### Binary bilkul waise hi, bas 10 ki jagah 2
```
   Number: 1101 (binary)

   1      1      0      1
   |      |      |      |
   |      |      |      +-- 1 x 2^0 = 1 x 1 = 1
   |      |      +--------- 0 x 2^1 = 0 x 2 = 0
   |      +---------------- 1 x 2^2 = 1 x 4 = 4
   +----------------------- 1 x 2^3 = 1 x 8 = 8
                                              ---
                                              13 (decimal)
```

### Place values yaad kar lo

```
   Bit position:   7    6    5    4    3    2    1    0
   Value:        128   64   32   16    8    4    2    1
```

Yeh 8 numbers yaad karo. Binary padhna instantly aa jayega.

---

## Binary → Decimal (conversion)

Bas jahan `1` hai, uski place value jod do.

```
   1 0 1 1 0 1 0 1
   | | | | | | | |
   | | | | | | | +-- 1  x 1   = 1
   | | | | | | +---- 0  x 2   = 0
   | | | | | +------ 1  x 4   = 4
   | | | | +-------- 0  x 8   = 0
   | | | +---------- 1  x 16  = 16
   | | +------------ 1  x 32  = 32
   | +-------------- 0  x 64  = 0
   +---------------- 1  x 128 = 128
                                ---
                                181
```

---

## Decimal → Binary (conversion)

**Tareeka: baar-baar 2 se divide karo, remainder note karo, ulta padho.**

```
   Convert 13 to binary:

   13 / 2 = 6  remainder 1    <- LSB (Least Significant Bit)
    6 / 2 = 3  remainder 0
    3 / 2 = 1  remainder 1
    1 / 2 = 0  remainder 1    <- MSB (Most Significant Bit)

   Neeche se upar padho: 1101
```

**Tez tareeka (place values se):**
```
   13 = ? 
   128? nahi (bada hai)
   64? nahi
   32? nahi
   16? nahi
   8?  HAAN -> 13 - 8 = 5,  bit = 1
   4?  HAAN -> 5 - 4 = 1,   bit = 1
   2?  nahi                  bit = 0
   1?  HAAN -> 1 - 1 = 0,   bit = 1
   
   Result: 1101
```

---

## Hexadecimal (base 16) — programmers ka favourite

Binary padhna mushkil hai. `11010110101110001` — kaun padhega?

**Hex = base 16.** 16 digits: `0-9` phir `A B C D E F`.

| Dec | Bin | Hex |  | Dec | Bin | Hex |
|---|---|---|---|---|---|---|
| 0 | 0000 | 0 | | 8 | 1000 | 8 |
| 1 | 0001 | 1 | | 9 | 1001 | 9 |
| 2 | 0010 | 2 | | 10 | 1010 | **A** |
| 3 | 0011 | 3 | | 11 | 1011 | **B** |
| 4 | 0100 | 4 | | 12 | 1100 | **C** |
| 5 | 0101 | 5 | | 13 | 1101 | **D** |
| 6 | 0110 | 6 | | 14 | 1110 | **E** |
| 7 | 0111 | 7 | | 15 | 1111 | **F** |

### Hex kyun? Kyunki 1 hex digit = exactly 4 bits 🔑

```
   Binary:  1101 0110 1011 0001
            |    |    |    |
   Hex:     D    6    B    1     ->  0xD6B1
```

Poora byte = **exactly 2 hex digits**. Isliye memory dumps hamesha hex mein hoti hain.

C++ mein hex likhne ke liye `0x` prefix:
```cpp
int a = 0xFF;      // 255
int b = 0x10;      // 16
int c = 0b1010;    // 10 (binary literal, C++14 se)
```

---

## Ek byte ki range

```
   Sabse chhota: 00000000 = 0
   Sabse bada:   11111111 = 255

   Total: 256 alag values (0 se 255)
```

Isliye colours mein RGB values 0-255 hoti hain — har colour ek byte hai.

---

## Signed vs Unsigned — bahut important

Ab tak humne sirf positive numbers dekhe. Negative kaise?

### Unsigned (sirf positive)
8 bits, saare 8 value ke liye:
```
   Range: 0 se 255
```

### Signed — Two's Complement

Sabse upar wala bit (**MSB**) sign batata hai:
- MSB = 0 → positive
- MSB = 1 → negative

Lekin negative ko simple "sign bit" se nahi, **two's complement** se represent karte hain.

**Two's complement nikalne ka tareeka:**
```
   -5 kaise banayein (8 bits mein)?

   Step 1: 5 ka binary        = 00000101
   Step 2: sab bits ulte karo = 11111010   (one's complement)
   Step 3: 1 jodo             = 11111011   <- yeh -5 hai
```

Check karo: `5 + (-5)` = `00000101 + 11111011` = `1 00000000` → 8 bits mein `00000000` = 0 ✓

**Two's complement kyun?** Kyunki isse subtraction bhi addition ban jaata hai. CPU ko
alag subtract circuit nahi banana padta. Genius design.

### Ranges (8-bit example)

| Type | Range | Formula |
|---|---|---|
| `unsigned` 8-bit | 0 to 255 | 0 to 2^8 − 1 |
| `signed` 8-bit | −128 to 127 | −2^7 to 2^7 − 1 |

Note: negative side mein ek extra value hai (−128), positive side mein 127 tak.
Kyunki 0 ko positive side mein gina jaata hai.

### Common sizes

| Bits | Unsigned range | Signed range |
|---|---|---|
| 8 | 0 .. 255 | −128 .. 127 |
| 16 | 0 .. 65,535 | −32,768 .. 32,767 |
| 32 | 0 .. 4,294,967,295 | −2,147,483,648 .. 2,147,483,647 |
| 64 | 0 .. ~1.8 × 10^19 | ~−9.2 × 10^18 .. ~9.2 × 10^18 |

**32-bit signed ki max value ~2.1 arab hai.** Yeh number yaad rakho — yeh `int` ki limit hai
aur bahut bugs ka source hai.

---

## Overflow — ek chhota preview

Kya ho jab aap max se aage badho?

```
   8-bit unsigned:
   254 -> 11111110
   255 -> 11111111
   256 -> 1 00000000   <- 9 bits chahiye! par sirf 8 hain
                       -> upar wala bit gir jaata hai
        ->   00000000  = 0    😱 wrap around!
```

Unsigned mein yeh **defined behaviour** hai (wrap around hota hai).
Signed mein yeh **UNDEFINED BEHAVIOUR** hai — kuch bhi ho sakta hai!

Folder 03 mein iska poora detail. Abhi bas yaad rakho: **numbers ki limits hoti hain.**

> **Real disaster:** 1996 mein Ariane 5 rocket phat gaya kyunki ek 64-bit float ko
> 16-bit signed integer mein daala gaya. $370 million ka nuksaan. Ek overflow se.

---

## Endianness — bytes ka order

Multi-byte number memory mein kis order mein rakhein?

Number `0x12345678` (4 bytes) ko store karna hai:

```
   LITTLE ENDIAN (x86, ARM default) - chhota byte pehle
   Address:  1000  1001  1002  1003
   Value:     78    56    34    12

   BIG ENDIAN (network byte order, kuch older CPUs) - bada byte pehle
   Address:  1000  1001  1002  1003
   Value:     12    34    56    78
```

> **HFT relevance — YEH BAHUT IMPORTANT HAI:**
> Network protocols (TCP/IP, aur zyadatar exchange protocols) **big endian** use karte
> hain. Aapka x86 server **little endian** hai. Matlab har market data message mein aapko
> bytes swap karne padte hain (`ntohl`, `ntohs`, `std::byteswap` C++23 mein).
> Agar aap yeh bhool gaye, aapki price `0x00002710` (10000) ban jayegi `0x10270000`
> (270,991,360). Aur aap galat price pe trade kar doge. Folder 38 mein detail.

Apna system check karo:
```cpp
#include <bit>
#include <iostream>
int main() {
    if constexpr (std::endian::native == std::endian::little)
        std::cout << "Little endian\n";
    else
        std::cout << "Big endian\n";
}
```

---

## Character encoding — ek jhalak

Characters bhi numbers hain.

```
   'A' = 65  = 01000001
   'B' = 66  = 01000010
   'a' = 97  = 01100001
   '0' = 48  = 00110000
   ' ' = 32  = 00100000
```

Kaam ka trick: `'a' - 'A' = 32`. Isliye uppercase/lowercase conversion aasan hai.
Aur `'5' - '0' = 5` — isliye character ko digit mein badal sakte ho.

Yeh folder 03 (`char`) aur folder 10 (strings) mein aayega.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > bits.cpp << 'END'
#include <iostream>
#include <bitset>
#include <climits>

int main() {
    // <bitset> se hum binary dekh sakte hain
    int x = 13;
    std::cout << "13 in binary (32 bits): " << std::bitset<32>(x) << "\n";
    std::cout << "13 in binary (8 bits):  " << std::bitset<8>(x)  << "\n";

    // hex mein print
    std::cout << "255 in hex: " << std::hex << 255 << std::dec << "\n";

    // negative number ka two's complement
    int neg = -5;
    std::cout << "-5 in binary: " << std::bitset<32>(neg) << "\n";

    // types ki sizes
    std::cout << "\nsizeof(char)   = " << sizeof(char)   << " byte(s)\n";
    std::cout << "sizeof(short)  = " << sizeof(short)  << " byte(s)\n";
    std::cout << "sizeof(int)    = " << sizeof(int)    << " byte(s)\n";
    std::cout << "sizeof(long)   = " << sizeof(long)   << " byte(s)\n";

    // ranges
    std::cout << "\nint max = " << INT_MAX << "\n";
    std::cout << "int min = " << INT_MIN << "\n";

    // OVERFLOW dekho (unsigned - defined behaviour)
    unsigned char uc = 255;
    std::cout << "\nunsigned char 255 + 1 = " << (int)(unsigned char)(uc + 1) << "\n";

    return 0;
}
END
g++ -std=c++20 -Wall bits.cpp -o bits && ./bits
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Byte = 8 bits hamesha" | 99.99% haan. Standard technically `CHAR_BIT` se define karta hai |
| "Negative number ke liye ek bit sign hai" | Two's complement hai, simple sign bit nahi |
| "Hex ek alag number hai" | Nahi, wahi number hai — bas likhne ka tareeka alag |
| "Overflow pe error aata hai" | ❌ **Nahi!** Signed overflow = UB, unsigned = silently wrap |
| "Endianness matter nahi karti" | Network programming aur file formats mein **bahut** karti hai |

---

## Exercises

Kagaz pe karo, phir code se verify karo.

1. Binary → Decimal: `1010`, `11111`, `10000000`, `01010101`
   <details><summary>Answers</summary>10, 31, 128, 85</details>

2. Decimal → Binary: `7`, `20`, `100`, `255`
   <details><summary>Answers</summary>111, 10100, 1100100, 11111111</details>

3. Hex → Decimal: `0xF`, `0x10`, `0xFF`, `0xA0`
   <details><summary>Answers</summary>15, 16, 255, 160</details>

4. Binary `11010110` ko hex mein badlo (4-4 bits mein todo).
   <details><summary>Answer</summary>`1101` = D, `0110` = 6 → `0xD6`</details>

5. 16 bits se kitne alag values ban sakti hain?
   <details><summary>Answer</summary>2^16 = 65,536</details>

6. 8-bit signed mein `-1` kaise dikhega?
   <details><summary>Answer</summary>
   1 = 00000001 → invert = 11111110 → +1 = **11111111**.
   Interesting: `-1` sabhi bits 1 hote hain, chahe kitne bhi bits ho.
   </details>

7. Ek market data message mein price 4 bytes mein big-endian hai: `00 00 27 10`.
   Little-endian x86 pe seedha padhne se kya value aayegi? Sahi value kya hai?
   <details><summary>Answer</summary>
   Sahi (big-endian) value: `0x00002710` = **10000**.
   Seedha little-endian padhne se bytes ulte hoke: `0x10270000` = **271,056,896**. ❌
   Isliye byte-swap zaroori hai. Yeh real HFT bug hai.
   </details>

8. Upar wala `bits.cpp` chalao. `-5` ka binary dekho. Verify karo ki woh two's
   complement hai.

---

## Interview questions

1. Two's complement kya hai aur kyun use hota hai?
2. Signed overflow aur unsigned overflow mein kya fark hai?
3. Little endian aur big endian kya hai? Network byte order kaunsa hai?
4. 32-bit signed integer ki max value kya hai?
5. `char` signed hota hai ya unsigned?
   <details><summary>Answer</summary>
   **Implementation-defined!** x86 Linux pe signed, ARM pe aksar unsigned.
   Isliye jab aapko pakka chahiye to `signed char` ya `unsigned char` likho.
   Yeh ek classic gotcha hai.
   </details>

---

## Next
→ [`11-memory-basics.md`](11-memory-basics.md)
