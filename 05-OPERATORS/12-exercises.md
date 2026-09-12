# 12 — Folder 05 Revision + Exercises

## Prerequisites
Is folder ke saare lessons (01–11)

---

## PART A — Concept check

### Arithmetic
1. `7 / 2` kya deta hai aur kyun?
2. `-7 / 2` aur `-7 % 3` kya dete hain? Python se fark kyun?
3. Integer aur float division by zero mein kya fark?
4. Signed aur unsigned overflow mein fark?
5. Division optimization ke 2 tareeke batao.

### Increment
6. `++i` aur `i++` mein fark? Kaunsa prefer karein aur kyun?
7. `int` ke liye koi performance fark hai? Iterators ke liye?
8. `i = i++ + ++i` mein kya problem hai?

### Comparison
9. `-1 < 1u` ka result aur kyun?
10. Floats ko compare kaise karte hain?
11. `NaN == NaN` kya deta hai?
12. `a > b > c` C++ mein kya karta hai?
13. `<=>` operator ka faayda?

### Logical
14. Short-circuit evaluation kya hai? 2 use cases.
15. `&&` aur `&` mein fark?
16. De Morgan's laws batao.
17. Condition ka order kyun matter karta hai?

### Bitwise
18. `x % 8` ko bitwise mein kaise likhein? Kab safe hai?
19. Power of 2 kaise check karein?
20. Signed aur unsigned right shift mein fark?
21. `x & (x-1)` kya karta hai?
22. XOR ki 3 properties.
23. `std::popcount` manual loop se kitna tez? Kyun?

### Compound / ternary
24. `x += y` aur `x = x + y` mein 2 fark batao.
25. `s = s + "x"` loop mein kyun slow hai?
26. `?:` expression hai ya statement? Fark kya padta hai?

### Precedence / evaluation
27. `flags & MASK == 0` mein kya bug hai?
28. Bitwise operators ki precedence kam kyun hai?
29. Precedence aur evaluation order mein fark?
30. Function arguments kis order mein evaluate hote hain?

---

## PART B — Output prediction

### B1
```cpp
std::cout << 7/2 << " " << -7/2 << " " << 7%3 << " " << -7%3 << " " << 7.0/2;
```
<details><summary>Answer</summary>`3 -3 1 -1 3.5`</details>

### B2
```cpp
int i = 3;
int a = i++;
int b = ++i;
std::cout << i << " " << a << " " << b;
```
<details><summary>Answer</summary>`5 3 5`</details>

### B3
```cpp
std::cout << (12 & 10) << " " << (12 | 10) << " " << (12 ^ 10) << " ";
std::cout << (5 << 2) << " " << (20 >> 2) << " " << (-8 >> 1);
```
<details><summary>Answer</summary>`8 14 6 20 5 -4`</details>

### B4
```cpp
std::cout << (2 + 3 * 4) << " " << (1 << 2 + 3) << " " << ((1 << 2) + 3) << " ";
std::cout << (10 - 5 - 2) << " " << (2 + 10 % 3);
```
<details><summary>Answer</summary>`14 32 7 3 3`</details>

### B5
```cpp
int flags = 0b0110, MASK = 0b0100;
std::cout << (flags & MASK == 0) << " " << ((flags & MASK) == 0) << " ";
std::cout << ((flags & MASK) != 0);
```
<details><summary>Answer</summary>
`0 0 1`

Pehla hamesha `0` — `(MASK == 0)` = `false` = `0`, phir `flags & 0` = `0`.
</details>

### B6
```cpp
std::cout << std::boolalpha;
std::cout << (5 > 3 > 1) << " " << (-1 < 1u) << " " << (0.1+0.2 == 0.3);
```
<details><summary>Answer</summary>`false false false`</details>

### B7
```cpp
int arr[] = {10, 20, 30};
int* p = arr;
std::cout << *p++ << " " << *p << " " << *++p;
```
<details><summary>Answer</summary>
`10 20 30` — par ⚠️ yeh ek statement mein `p` ko kai baar modify kar raha hai.
C++17 mein `<<` left-to-right guaranteed hai, par aisa code likhna galat practice hai.
</details>

---

## PART C — Find the bug

### C1
```cpp
int correct = 45, total = 60;
double pct = correct / total * 100;
```
<details><summary>Answer</summary>Integer division → 0. Fix: `static_cast<double>(correct) / total * 100`</details>

### C2
```cpp
int size = 10, current = 0;
int prev = (current - 1) % size;
arr[prev] = x;
```
<details><summary>Answer</summary>`-1 % 10 = -1` → out of bounds. Fix: `(current + size - 1) % size`</details>

### C3
```cpp
if (flags & FLAG_ACTIVE == 0) { }
```
<details><summary>Answer</summary>Precedence — `==` pehle. Fix: `if ((flags & FLAG_ACTIVE) == 0)`</details>

### C4
```cpp
if (ptr != nullptr & ptr->value > 5) { }
```
<details><summary>Answer</summary>`&` short-circuit nahi karta → null deref crash. Fix: `&&`</details>

### C5
```cpp
std::vector<int> v;
for (std::size_t i = v.size() - 1; i >= 0; --i) { }
```
<details><summary>Answer</summary>
Khali vector pe `0-1` wrap → huge number. Aur `size_t >= 0` hamesha true → infinite loop.
Fix: range-based for, ya `for (std::size_t i = v.size(); i-- > 0; )`
</details>

### C6
```cpp
std::string s;
for (int i = 0; i < 10000; ++i) s = s + "x";
```
<details><summary>Answer</summary>Har baar poori string copy → O(n²). Fix: `s += "x"`</details>

### C7
```cpp
int i = 0;
process(data[i++], data[i++], data[i++]);
```
<details><summary>Answer</summary>Argument order unspecified. Fix: alag lines mein values nikalo.</details>

### C8
```cpp
std::cout << cond ? "yes" : "no";
```
<details><summary>Answer</summary>Precedence — `(cout << cond) ? ...`. Fix: `std::cout << (cond ? "yes" : "no")`</details>

---

## PART D — Practical tasks

### D1. Safe arithmetic library
```cpp
std::optional<std::int32_t> safeAdd(std::int32_t a, std::int32_t b);
std::optional<std::int32_t> safeMul(std::int32_t a, std::int32_t b);
std::optional<std::int32_t> safeDiv(std::int32_t a, std::int32_t b);
```
Overflow aur division-by-zero detect karo. Do tareekon se: `__builtin_*_overflow` se,
aur manual checks se.

### D2. Bit manipulation toolkit
```cpp
constexpr bool testBit(std::uint32_t v, int bit);
constexpr std::uint32_t setBit(std::uint32_t v, int bit);
constexpr std::uint32_t clearBit(std::uint32_t v, int bit);
constexpr std::uint32_t toggleBit(std::uint32_t v, int bit);
```
`static_assert` se compile-time pe test karo.

### D3. Power-of-2 utilities
```cpp
constexpr bool isPowerOf2(std::uint32_t v);
constexpr std::uint32_t roundUpPow2(std::uint32_t v);
constexpr std::size_t alignUp(std::size_t value, std::size_t alignment);
```
`std::has_single_bit` aur `std::bit_ceil` se compare karo.

### D4. Ring buffer (power-of-2 wrap)
```cpp
template <typename T, std::size_t N>      // N power of 2 hona chahiye
class RingBuffer {
    static_assert((N & (N - 1)) == 0, "N power of 2 hona chahiye");
    static constexpr std::size_t kMask = N - 1;
    // push, pop, size, empty, full
};
```
Wrap `& kMask` se karo, `% N` se nahi.

### D5. Order flags system
Ek complete flags system banao 16 flags ke saath:
- `enum class` + operators
- `hasFlag`, `setFlag`, `clearFlag`, `toggleFlag`
- `describe()` jo readable string de
- `static_assert` se compile-time tests
- Memory comparison `bool[16]` se

### D6. Endianness converter
```cpp
constexpr std::uint16_t swap16(std::uint16_t v);
constexpr std::uint32_t swap32(std::uint32_t v);
constexpr std::uint64_t swap64(std::uint64_t v);
```
Manual likho, phir `__builtin_bswap*` se compare karo.
**Godbolt pe assembly dekho** — kitni instructions?

### D7. Float comparison library
```cpp
bool nearlyEqual(double a, double b, double relEps, double absEps);
bool nearlyZero(double a, double absEps);
```
Test cases: `0.1+0.2` vs `0.3`, `1e10` vs `1e10+1`, `0.0` vs `1e-15`, NaN vs NaN.

### D8. Benchmark suite
In sab ka timing measure karo:
1. `x / d` vs `x * (1/d)`
2. `i % 1024` vs `i & 1023` (runtime divisor ke saath)
3. Manual popcount vs `std::popcount`
4. `cheap && expensive` vs `expensive && cheap`
5. Branch vs branchless counting (predictable aur random data dono pe)

Table banao. Har result ko **explain** karo.

---

## PART E — Self-assessment

```
[ ] Mujhe integer division ka trap pata hai
[ ] Mujhe modulo aur negative numbers ka behaviour pata hai
[ ] Mujhe signed vs unsigned overflow ka fark pata hai
[ ] Main safe arithmetic likh sakta hoon
[ ] Mujhe ++i vs i++ ka fark aur kab matter karta hai, pata hai
[ ] Mujhe signed/unsigned comparison ka trap pata hai
[ ] Main floats sahi tareeke se compare kar sakta hoon
[ ] Mujhe NaN ki properties pata hain
[ ] Mujhe a > b > c ka trap pata hai
[ ] Mujhe short-circuit evaluation samajh aati hai
[ ] Mujhe && vs & ka fark pata hai
[ ] Main De Morgan's laws use kar sakta hoon
[ ] Mujhe saare 6 bitwise operators aate hain
[ ] Main bit set/clear/toggle/test likh sakta hoon
[ ] Mujhe power-of-2 check pata hai
[ ] Mujhe x % 8 == x & 7 ka trick aur uski limitation pata hai
[ ] Mujhe signed vs unsigned right shift ka fark pata hai
[ ] Mujhe C++20 <bit> ke functions pata hain
[ ] Mujhe compound assignment ke 2 faayde pata hain
[ ] Mujhe ?: expression hone ka faayda pata hai
[ ] Mujhe bitwise precedence ka trap pata hai
[ ] Mujhe precedence vs evaluation order ka fark pata hai
[ ] Maine sabhi 6 examples chalaye hain
```

**Scoring:**
- **20-23** → Excellent. **Phase 2 complete!** Folder 06 pe jao. 🎉
- **16-19** → Achha. Jo miss hua padho.
- **11-15** → Files 05, 09, 10 dobara karo.
- **< 11** → Poora folder dobara. Examples zaroor chalao.

---

## PART F — Challenge

### Challenge 1: "Bit Manipulation Library"

Ek complete, tested, `constexpr` library banao:

```cpp
namespace bits {
    constexpr bool test(std::uint32_t v, int bit);
    constexpr std::uint32_t set(std::uint32_t v, int bit);
    constexpr std::uint32_t clear(std::uint32_t v, int bit);
    constexpr std::uint32_t toggle(std::uint32_t v, int bit);
    constexpr std::uint32_t assign(std::uint32_t v, int bit, bool value);

    constexpr int popcount(std::uint32_t v);
    constexpr int leadingZeros(std::uint32_t v);
    constexpr int trailingZeros(std::uint32_t v);

    constexpr bool isPowerOf2(std::uint32_t v);
    constexpr std::uint32_t roundUpPow2(std::uint32_t v);
    constexpr std::uint32_t lowestSetBit(std::uint32_t v);
    constexpr std::uint32_t clearLowestSetBit(std::uint32_t v);

    constexpr std::uint32_t reverse(std::uint32_t v);
    constexpr std::uint32_t swapBytes(std::uint32_t v);
}
```

Requirements:
- Sab `constexpr` — `static_assert` se compile-time tests likho
- Har function ke liye kam se kam 5 test cases
- `<bit>` ke standard functions se compare karo (correctness aur speed dono)
- Benchmark: aapka vs standard

### Challenge 2: "Expression Evaluator Traps"

Ek program likho jo yeh **saare** precedence traps ek jagah demonstrate kare,
aur har ek ke liye:
1. Galat version (warning ke saath)
2. Sahi version
3. Explanation ki parse kaise hua

Traps:
- `flags & MASK == 0`
- `1 << n + 1`
- `cout << a < b`
- `a || b && c`
- `!x == y`
- `*p++` vs `(*p)++`
- `a > b > c`
- `-1 < 1u`

Har ek ke liye **compiler warning** bhi capture karo.

---

## 🎉 PHASE 2 COMPLETE!

Aapne complete kar liya:
- **Folder 03** — Variables aur data types
- **Folder 04** — Input/Output
- **Folder 05** — Operators

Ab aapke paas C++ ka **core toolkit** hai. Aap data store kar sakte ho, user se
baat kar sakte ho, aur us data pe kaam kar sakte ho.

**Agla phase (PHASE 3)** mein control flow aayega — conditions, loops, functions.
Wahan aapka program **faisle** lega aur **repeat** karega.

---

## Next
→ [`../06-CONDITIONS/00-README.md`](../06-CONDITIONS/00-README.md)
