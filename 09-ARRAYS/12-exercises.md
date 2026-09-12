# 12 — Folder 09 Revision + Exercises

## Prerequisites
Lessons 01–11 aur saare 7 examples chalaye hue (`06_oob_asan` bhi — debug build mein woh STL OOB
pe abort hota hai).

---

## PART A — Concept check

1. Array ke 4 properties?
2. `a[i]` andar se kya hai? Time complexity?
3. `int a[5] = {1, 2}` — baaki 3 elements?
4. Local vs global array — uninitialized behaviour?
5. OOB access — read vs write, kya hota hai? "Chal gaya" ka matlab?
6. `[]` vs `.at()` — kab kaunsa?
7. Array decay kya hai? Kab hota hai, kab nahi?
8. `void f(int a[10])`, `f(int a[])`, `f(int* a)` — same? `sizeof(a)` inside?
9. Function ko array + size pass karne ke 4 tareeke?
10. `int m[3][4]` memory layout? `m[i][j]` address formula?
11. Row-major vs column-major traversal — farq aur kyun?
12. `int**` vs `int(*)[4]` — 2D array kaunsa hai?
13. `std::array` vs C array — 5 differences, overhead?
14. `std::array` pass by value — kya hota hai? Better?
15. `std::span` kya hai (contents, size, ownership)?
16. `std::span` kab dangle karta hai?
17. AoS vs SoA — kab kaunsa? Field-subset access pe?
18. Stack array vs heap `std::vector` — alloc, size, locality?
19. `std::vector` reallocation kya invalidate karta hai?
20. OOB detect karne ke tools (compile + runtime)?

---

## PART B — Output prediction

### B1
```cpp
int a[5] = {1, 2};
for (int x : a) std::cout << x << " ";
```
<details><summary>Answer</summary>`1 2 0 0 0 ` — adhoora init baaki elements ko zero kar deta hai.</details>

### B2
```cpp
int a[] = {10, 20, 30, 40};
std::cout << std::size(a) << " " << sizeof(a) << " " << *(a + 2);
```
<details><summary>Answer</summary>`4 16 30` (jahan `int` 4 bytes ka hai — practically har jagah).</details>

### B3
```cpp
void f(int a[]) { std::cout << sizeof(a); }
int main() { int x[10]; std::cout << sizeof(x) << " "; f(x); }
```
<details><summary>Answer</summary>`40 8` — pehle poore array ka size, phir pointer ka size (decay). GCC `-Wsizeof-array-argument` warning bhi deta hai.</details>

### B4
```cpp
int m[2][3] = {1, 2, 3, 4, 5, 6};
std::cout << m[1][0] << " " << *(&m[0][0] + 4);
```
<details><summary>Answer</summary>`4 5` — row-major: `m[1][0]` offset 3 pe hai; `&m[0][0]+4` offset 4 = `m[1][1]` = 5.</details>

### B5
```cpp
std::array<int, 3> a = {1, 2, 3}, b = {1, 2, 3};
std::cout << std::boolalpha << (a == b) << " " << (a.data() == b.data());
```
<details><summary>Answer</summary>`true false` — elements barabar hain, par storage alag-alag.</details>

### B6
```cpp
int a[3] = {1, 2, 3};
std::span<int> s = a;
s = s.subspan(1);
for (int x : s) std::cout << x << " ";
std::cout << s.size();
```
<details><summary>Answer</summary>`2 3 2`</details>

### B7
```cpp
int a[5] = {};
int canary = 42;
for (std::size_t i = 0; i <= 5; ++i) a[i] = 7;
std::cout << canary;   // (undefined -- 7, 42, ya crash)
```
<details><summary>Answer</summary>UB — `a[5]` OOB write hai. `-O0` pe aksar `7` print hota hai (canary overwrite ho gaya); `-O2` pe kuch bhi. Isi liye tools chahiye.</details>

### B8
```cpp
std::array<int, 4> a;
std::cout << a[0];
```
<details><summary>Answer</summary>Garbage — trivial type ka local `std::array`, bina `{}` ke. `std::array<int,4> a{};` likho.</details>

---

## PART C — Find the bug

### C1
```cpp
int daily[7];
for (int i = 1; i <= 7; ++i) daily[i] = readTemp();
```
<details><summary>Answer</summary>`1` se shuru (`daily[0]` chhoot gaya), aur `<= 7` (`daily[7]` OOB). `for (int i = 0; i < 7; ++i)` likho.</details>

### C2
```cpp
double avg(int scores[]) {
    int n = sizeof(scores) / sizeof(scores[0]);
    int sum = 0;
    for (int i = 0; i < n; ++i) sum += scores[i];
    return double(sum) / n;
}
```
<details><summary>Answer</summary>Decay: `sizeof(scores)` = 8, to `n` = 2. Size alag se pass karo, ya `std::span<const int>` lo.</details>

### C3
```cpp
int* firstN(int n) {
    int buf[100];
    for (int i = 0; i < n; ++i) buf[i] = i;
    return buf;
}
```
<details><summary>Answer</summary>Mar chuke local ka pointer return (decay + local lifetime). Saath mein `n > 100` → OOB. `std::array`/`std::vector` return karo, ya caller ka `std::span` bharo.</details>

### C4
```cpp
std::vector<int> v = {1, 2, 3};
int* p = &v[0];
v.push_back(4);
std::cout << *p;
```
<details><summary>Answer</summary>`push_back` realloc kar sakta hai → `p` dangling → UB. Pehle `reserve(4)` karo, ya baad mein `&v[0]` dobara lo.</details>

### C5
```cpp
int a[3][3];
for (int j = 0; j < 3; ++j)
    for (int i = 0; i < 3; ++i)
        a[i][j] = i + j;
```
<details><summary>Answer</summary>Output sahi hai, par traversal column-major hai (inner loop `i`) — cache ke liye bura. Loop order badlo: `i` bahar, `j` andar.</details>

### C6
```cpp
int n = getCount();
int buffer[n];
```
<details><summary>Answer</summary>VLA — C++ mein standard nahi, aur agar `n` attacker ke haath mein ho to stack overflow ka khatra. `std::vector<int> buffer(n)` lo.</details>

### C7
```cpp
int a[4] = {1,2,3,4}, b[4];
b = a;
```
<details><summary>Answer</summary>Compile error — arrays assign nahi hote. `std::copy` use karo, ya `std::array` (usme `b = a` chalta hai).</details>

### C8
```cpp
char name[5] = "hello";
std::cout << name;
```
<details><summary>Answer</summary>`"hello"` ko 6 bytes chahiye (5 letters + `'\0'`). **C++ mein yeh compile error hai** — GCC 16.2: `initializer-string for 'char [5]' is too long`. (C language mein yahi line chupchaap compile ho jaati hai, bina `'\0'` ke — phir print karna OOB padhta.) Fix: `char name[6]` ya `char name[] = "hello";`.</details>

---

## PART D — Practical tasks

### D1. Span pe stats
`struct Stats { int min, max; double mean; long long sum; };`
`Stats analyze(std::span<const int> data);` — ek hi pass, khaali input handle karo (throw ya
`std::optional<Stats>`). C array, `std::array`, `std::vector`, aur subspan pe test karo.

### D2. In-place operations
`std::span<int>` wale versions likho: `reverse`, `rotateLeft(k)`, `partitionEvens` (evens pehle,
odds baad mein, order bana rahe), `dedupSorted` (naya logical size return karo). Koi allocation
nahi. `std::` wale equivalents se compare karo.

### D3. Matrix library (flat storage)
```cpp
struct Matrix {
    std::size_t rows, cols;
    std::vector<double> data;                 // row-major, rows*cols
    double& at(std::size_t i, std::size_t j);
};
Matrix multiply(const Matrix& a, const Matrix& b);   // i, k, j loop order
```
`-O2` pe `i,j,k` vs `i,k,j` loop order benchmark karo (folder 07 file 09 wala idea).

### D4. AoS ↔ SoA converter
`struct ParticleAoS { float x,y,z,vx,vy,vz; };`
`struct ParticleSoA { std::vector<float> x, y, z, vx, vy, vz; };`
`toSoA(std::span<const ParticleAoS>)` aur `toAoS(const ParticleSoA&)` likho. Dono layouts pe ek
"gravity step" (saare 6 fields) aur ek "sum of x" (1 field) benchmark karo.

### D5. Ring buffer (power-of-2, `std::array` storage)
```cpp
template <class T, std::size_t Cap>          // Cap power of 2 hona chahiye
class Ring {
    static_assert((Cap & (Cap - 1)) == 0);
    std::array<T, Cap> buf_;
    std::size_t head_ = 0, tail_ = 0;
    // push, pop, size, empty, full -- & (Cap - 1) se wrap, % nahi
};
```
Heap zero. Wrap-around achhe se test karo.

### D6. Safe wire decoder
Input `std::span<const std::byte>`. Ek header padho (`u16 len`, `u8 type`), phir `len` bytes ka
payload. Har read `.size()` ke against check ho. Adhoore / zaroorat se bade inputs do — kabhi OOB
nahi, hamesha saaf error.

### D7. `_GLIBCXX_ASSERTIONS` / ASan lab
`06_oob_asan.cpp` lo. Is machine pe: `./build.ps1` (debug, `-O0`) aur `./build.ps1 san` BUG 4
pakadte hain; `./build.ps1 fast` (`-O2`) nahi pakadta — kyun? (Lesson 11.) Agar WSL/Linux/Clang
hai: `-fsanitize=address,undefined` — kya BUG 1–3 (raw array) bhi pakde gaye? Har tool ki pahunch
likho.

---

## PART E — Self-assessment

```
[ ] Array ke properties, a[i] == *(a+i), O(1) access -- clear
[ ] Init forms aur partial-init zeroing pata hai
[ ] OOB = UB, read vs write ka farq pata hai
[ ] [] vs .at() -- kab kaunsa
[ ] Array decay -- kab hota hai, sizeof trap
[ ] Function ko array pass karne ke tareeke (span best)
[ ] std::span use kar sakta hoon, dangling se bachta hoon
[ ] std::array -- no decay, value semantics, zero overhead
[ ] Row-major 2D layout, m[i][j] address formula
[ ] Traversal order aur cache (~8x) samajh aata hai
[ ] AoS vs SoA -- kab kaunsa, kai guna farq khud naapa (GCC 16.2 pe ~5x)
[ ] Stack vs heap array, reserve() ka role
[ ] delete vs delete[], iterator invalidation pata hai
[ ] ASan / _GLIBCXX_ASSERTIONS (-O0 vs -O2!) / -Warray-bounds ka coverage pata hai
[ ] Saare 7 examples chalaye
```

**Scoring:**
- **13–15** → Folder 10 (Strings) pe jao. 🎯
- **9–12** → Files 05, 07, 09, 10 dobara.
- **< 9** → Poora folder, examples zaroor, ASan/hardening lab.

---

## PART F — Challenge

### Challenge 1: "Order book — price level array"

```cpp
struct Level { std::int64_t price; std::int64_t qty; };

template <std::size_t Depth>
class BookSide {
    std::array<Level, Depth> levels_{};       // sorted, best [0] pe
    std::size_t count_ = 0;
public:
    void insert(std::int64_t price, std::int64_t qty);   // sorted rakho
    void erase(std::int64_t price);
    void modify(std::int64_t price, std::int64_t qty);
    Level best() const;                                    // O(1)
    std::span<const Level> view() const;
};
```

Shartein: **heap zero**, best bid/ask O(1), insert/erase sorted order bana ke rakhein (array mein
shift — sorted `std::vector` ke against naapo). Bounds-safe. 1M random ops ka benchmark.
`-Wall -Wextra -Wshadow -Werror` clean. Folder 39 ki neev.

### Challenge 2: "Array bug museum"

Lesson 11 ka har bug ek live, safety-instrumented demo ki tarah: buggy version + compiler warning
(exact text) + fixed version + ek aisa input jahan bug saaf galat output/behaviour de. `./build.ps1
san` aur (agar ho to) ASan ke neeche chalao — likho kaunsa tool kya pakadta hai. `06_oob_asan.cpp`
ko extend karo.

### Challenge 3: "2D convolution — layout & loop-order study"

`N×N` (`N = 2048`) `float` image pe 3×3 kernel, flat `std::vector<float>` storage. Implement karo:
1. Naive `i, j, ki, kj`
2. Loop reorder taaki inner access sequential ho
3. Tiled (`32×32` blocks)
4. `-O3 -march=native` (vectorized)

Chaaron ko `-O2` aur `-O3 -march=native` pe benchmark karo. GFLOP/s ki table. Har uchhaal samjhao
(cache, prefetch, SIMD). Vectorization confirm karne ke liye `-fopt-info-vec`.

---

## 🎉 Folder 09 complete

Arrays: contiguous layout, `a[i] == *(a+i)`, **array decay** (pointers ka darwaza), 2D row-major,
`std::array` (size-safe, zero overhead), `std::span` (modern array parameter), AoS vs SoA (kai guna,
naapa hua), aur tooling ke saath poora bug catalogue.

Agla: **strings** — jo andar se `char` arrays hain, plus `std::string` ka SSO.

---

## Next
→ [`../10-STRINGS/00-README.md`](../10-STRINGS/00-README.md)
