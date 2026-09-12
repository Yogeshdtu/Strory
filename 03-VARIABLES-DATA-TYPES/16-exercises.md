# 16 — Phase 2 Revision + Exercises

## Prerequisites
Is folder ke saare lessons (01–15)

---

## PART A — Concept check

### Data aur variables
1. Type kya batata hai? 4 cheezein.
2. Variable ka naam runtime pe exist karta hai?
3. `int age = 20;` ke chaar hisse batao aur har ek ka kaam.
4. `int a = 5; int b = a;` — `a` badalne se `b` badlega? Kyun?
5. Uninitialized local variable ki value kya hoti hai? Global ki?

### Declaration/definition/assignment
6. Declaration, definition, initialization mein fark?
7. `extern` kya karta hai?
8. ODR kya hai?
9. `=` aur `==` mein fark? Ek bug example do.
10. `i++` aur `++i` mein fark? Kaunsa prefer karein?

### `int`
11. `int` ki size kya hai? (trap question)
12. `long` Linux aur Windows pe kitne bytes hai?
13. Signed overflow aur unsigned overflow mein fark?
14. `-1 < 1u` ka result kya hai? Kyun?
15. Integer promotion kya hai?
16. `7 / 2`, `-7 / 2`, `7 % 2`, `-7 % 2` — kya hai?

### Floating point
17. `0.1 + 0.2 == 0.3` kya deta hai? Kyun?
18. Floats compare kaise karte hain?
19. `NaN == NaN` kya deta hai?
20. Finance mein `double` kyun use nahi karte? Alternative kya hai?
21. `numeric_limits<double>::min()` aur `lowest()` mein fark?

### char, bool
22. `char` signed hai ya unsigned?
23. `char`, `signed char`, `unsigned char` — kitne types hain?
24. `'a' - 'A'` kya hai? Uska use?
25. `std::cout << uint8_t(65)` kya print karega?
26. `std::vector<bool>` mein kya problem hai?
27. `&&` aur `&` mein fark?

### Fixed-width, initialization, const
28. `int32_t` aur `int` mein fark? Kab kaunsa use karein?
29. `int a{3.99}` kyun compile nahi hota?
30. Most Vexing Parse kya hai?
31. `vector<int> v(5,10)` aur `v{5,10}` mein fark?
32. `const` aur `constexpr` mein fark?
33. `const int* p` aur `int* const p` mein fark?
34. `consteval` kya hai?

### auto, conversions, sizeof
35. `auto` dynamic typing hai?
36. `auto x = v;` copy banata hai ya reference?
37. `for (auto x : bigVector)` mein kya problem hai?
38. `static_cast` aur `reinterpret_cast` mein fark?
39. C-style cast kyun avoid karein?
40. `sizeof(i++)` ke baad `i` badalta hai?
41. Function parameter mein `sizeof(arr)` kya deta hai?
42. `sizeof(EmptyStruct)` kya hai? Kyun?

---

## PART B — Output prediction

Guess likho, phir chalao.

### B1
```cpp
int a = 10;
int b = a++;
int c = ++a;
std::cout << a << " " << b << " " << c;
```
<details><summary>Answer</summary>`12 10 12`</details>

### B2
```cpp
std::cout << 7/2 << " " << 7%2 << " " << -7/2 << " " << 7.0/2 << " " << (double)7/2;
```
<details><summary>Answer</summary>`3 1 -3 3.5 3.5`</details>

### B3
```cpp
char c = 'A';
std::cout << c << " " << (int)c << " " << c+1 << " " << (char)(c+1);
```
<details><summary>Answer</summary>`A 65 66 B`</details>

### B4
```cpp
std::cout << sizeof('A') << " " << sizeof("A") << " " << sizeof("Hello") 
          << " " << sizeof("a\n") << " " << sizeof(int);
```
<details><summary>Answer</summary>`1 2 6 3 4`</details>

### B5
```cpp
unsigned int a = 0;
std::cout << a - 1 << "\n";
std::cout << (int)(a - 1) << "\n";
```
<details><summary>Answer</summary>`4294967295` phir `-1`</details>

### B6
```cpp
std::uint8_t x = 65;
std::cout << x << " " << +x << " " << (int)x;
```
<details><summary>Answer</summary>`A 65 65`</details>

### B7
```cpp
std::cout << std::boolalpha;
std::cout << (0.1 + 0.2 == 0.3) << " ";
std::cout << (0.5 + 0.25 == 0.75) << " ";
std::cout << (1.0/3.0*3.0 == 1.0);
```
<details><summary>Answer</summary>
`false true false`

Doosra `true` hai kyunki 0.5, 0.25, 0.75 sab **exact powers of 2** hain — binary mein
exactly represent ho jaate hain.
</details>

### B8
```cpp
struct A { char c; int i; };
struct B { int i; char c; char d; };
struct C { };
std::cout << sizeof(A) << " " << sizeof(B) << " " << sizeof(C);
```
<details><summary>Answer</summary>`8 8 1`</details>

---

## PART C — Find the bug

### C1
```cpp
int correct = 45, total = 60;
double pct = correct / total * 100;
```
<details><summary>Answer</summary>
Integer division pehle hui → `0`. Fix: `static_cast<double>(correct) / total * 100`
</details>

### C2
```cpp
std::vector<int> v;
for (std::size_t i = v.size() - 1; i >= 0; --i) { }
```
<details><summary>Answer</summary>
Khali vector pe `0 - 1` = 4294967295 (wrap). Aur `size_t >= 0` hamesha true → infinite loop.
Fix: range-based for, ya forward loop, ya `std::ssize`.
</details>

### C3
```cpp
double price = 0.1;
double total = 0;
for (int i = 0; i < 10; ++i) total += price;
if (total == 1.0) { /* kabhi nahi chalega */ }
```
<details><summary>Answer</summary>
Float accumulation error. Fix: integer paise use karo, ya `nearlyEqual` se compare karo.
</details>

### C4
```cpp
int sum;
for (int i = 1; i <= 10; ++i) sum += i;
```
<details><summary>Answer</summary>`sum` uninitialized — garbage pe add ho raha hai. Fix: `int sum = 0;`</details>

### C5
```cpp
int flag = 0;
if (flag = 1) { std::cout << "set\n"; }
```
<details><summary>Answer</summary>`=` assignment hai, `==` chahiye. Hamesha true chalega aur `flag` corrupt hoga.</details>

### C6
```cpp
struct Message { long timestamp; int price; };
// yeh struct network se aaye bytes se parse hoti hai
```
<details><summary>Answer</summary>
`long` platform-dependent hai (Linux 8, Windows 4). Fix: `std::uint64_t timestampNs;
std::int64_t priceInTicks;`
</details>

### C7
```cpp
std::vector<std::string> names(1000000, "long string...");
for (auto name : names) { process(name); }
```
<details><summary>Answer</summary>
Har iteration mein ek string COPY (allocation!). Fix: `for (const auto& name : names)`
</details>

### C8
```cpp
double minVal = std::numeric_limits<double>::min();
for (double v : {-100.0, -50.0, -200.0}) if (v < minVal) minVal = v;
```
<details><summary>Answer</summary>
`min()` sabse chhota **positive** hai (2.22e-308), sabse chhota nahi.
Fix: `lowest()` use karo.
</details>

---

## PART D — Practical tasks

### D1. Type explorer
Ek program likho jo aapke system pe har fundamental type ki size, min, max print kare.
`<limits>` use karo, hardcode mat karo.

### D2. Safe average
Ek function likho:
```cpp
double average(int sum, int count);
```
Jo integer division bug se bache aur `count == 0` handle kare.

### D3. Float comparison library
```cpp
bool nearlyEqual(double a, double b, double relEps = 1e-9, double absEps = 1e-12);
```
Likho aur in cases pe test karo:
- `0.1 + 0.2` vs `0.3`
- `1e10` vs `1e10 + 1`
- `0.0` vs `1e-15`
- `NaN` vs `NaN`

### D4. Money class (integer paise)
```cpp
struct Money {
    std::int64_t paise;
    static Money fromRupees(std::int64_t rupees, std::int64_t p);
    std::string toString() const;      // "Rs 100.50"
};
```
Add, subtract, multiply implement karo. Test karo ki `0.1 * 3` exactly `0.30` aata hai.

### D5. Wire protocol struct
Yeh spec diya hai:
```
Offset  Size  Field
0       1     Message Type ('A' = Add, 'D' = Delete)
1       4     Sequence Number
5       8     Timestamp (nanoseconds)
13      8     Order ID
21      4     Price (ticks)
25      4     Quantity
29      1     Side ('B'/'S')
Total: 30 bytes
```
Struct banao, `static_assert` se verify karo, ek message serialize aur deserialize karo.

### D6. Overflow-safe arithmetic
```cpp
bool safeAdd(std::int32_t a, std::int32_t b, std::int32_t& result);
bool safeMultiply(std::int32_t a, std::int32_t b, std::int32_t& result);
```
Overflow detect karo aur `false` return karo. Do tareekon se karo:
(a) `__builtin_*_overflow` se, (b) manual checks se.

### D7. Struct optimizer
```cpp
struct Order {
    char   side;
    double price;
    char   type;
    int    quantity;
    char   status;
    long   timestamp;
};
```
Current `sizeof` kya hai? Members reorder karke minimum banao. Kitna bacha?

### D8. Compile-time table
```cpp
constexpr std::array<double, 100> makeSqrtTable();
```
Banao aur `static_assert` se verify karo. Phir `g++ -O2 -S` se check karo ki
values assembly mein literally hain.

---

## PART E — Self-assessment

```
[ ] Mujhe pata hai type kya batata hai
[ ] Main variable ka memory model bana sakta hoon
[ ] Mujhe declaration/definition/initialization ka fark pata hai
[ ] Mujhe pata hai `=` "daalo" hai, "barabar" nahi
[ ] Mujhe int ki size ka sach pata hai (guaranteed nahi)
[ ] Mujhe signed vs unsigned overflow ka fark pata hai
[ ] Mujhe pata hai signed overflow UB hai
[ ] Mujhe signed/unsigned comparison bug samajh aata hai
[ ] Mujhe integer promotion samajh aata hai
[ ] Mujhe pata hai 0.1 + 0.2 != 0.3 aur kyun
[ ] Main floats ko sahi tareeke se compare kar sakta hoon
[ ] Mujhe pata hai finance mein integer paise use karte hain
[ ] Mujhe char ka signedness gotcha pata hai
[ ] Mujhe uint8_t printing trap pata hai
[ ] Mujhe vector<bool> ka problem pata hai
[ ] Mujhe pata hai kab fixed-width types use karne hain
[ ] Mujhe pata hai {} narrowing rokta hai
[ ] Mujhe Most Vexing Parse pata hai
[ ] Mujhe const vs constexpr ka fark pata hai
[ ] Main const pointers padh sakta hoon
[ ] Mujhe auto ke traps pata hain (copy, const dropping)
[ ] Mujhe chaar casts ka fark pata hai
[ ] Mujhe struct padding samajh aati hai
[ ] Maine sabhi 8 examples chalaye hain
```

**Scoring:**
- **21-24** → Excellent. Folder 04 pe jao. 🎉
- **17-20** → Achha. Jo miss hua padho.
- **12-16** → Files 05, 06, 09, 13 dobara karo.
- **< 12** → Poora folder dobara. Examples chalao.

---

## PART F — Challenge

### Challenge 1: "Type Detective"

Ek program likho jo **runtime pe** yeh sab detect kare aur report banaye:
- Har type ki size
- `char` signed hai ya unsigned
- System little-endian hai ya big-endian
- `int` overflow kis value pe hoga
- `double` mein kitne decimal digits reliable hain
- Cache line size (`hardware_destructive_interference_size`)

Output ek clean table mein ho.

### Challenge 2: "Price Engine"

Ek mini price handling system banao jo:
1. Prices ko `std::int64_t` ticks mein rakhe (1 tick = Rs 0.01)
2. `fromString("21500.75")` → 2150075 ticks
3. `toString(2150075)` → "21500.75"
4. Add, subtract, multiply by quantity
5. Overflow detect kare
6. Ek test suite ho jo verify kare ki `0.1 + 0.2 == 0.3` **exactly** kaam karta hai
   (integer arithmetic mein)

Yeh exactly wahi hai jo HFT systems mein hota hai.

### Challenge 3: "Struct Golf"

Yeh struct diya hai:
```cpp
struct MarketUpdate {
    char     symbol[8];
    double   bidPrice;
    char     side;
    int      bidQty;
    short    exchangeId;
    double   askPrice;
    char     flags;
    int      askQty;
    long     timestamp;
};
```
1. Current `sizeof` kya hai?
2. Fixed-width types use karke rewrite karo
3. Members reorder karke **minimum possible size** paao
4. Kitne bytes bache? Ek 64-byte cache line mein kitne zyada objects aayenge?

---

## Aapne Phase 2 complete kar liya! 🎉

Ab aapko pata hai:
- Data aur types kya hain
- Variable memory mein kaise rehta hai
- `int`, `float`, `char`, `bool` ke saare gotchas
- Overflow, conversions, aur unke bugs
- Fixed-width types aur unki HFT relevance
- `const`, `constexpr`, `auto`
- Struct padding aur alignment ka basic

**Yeh foundation aage har jagah kaam aayegi** — pointers (12), memory (14),
object model (25), aur market data parsing (38) mein.

---

## Next
→ [`../04-INPUT-OUTPUT/00-README.md`](../04-INPUT-OUTPUT/00-README.md)
