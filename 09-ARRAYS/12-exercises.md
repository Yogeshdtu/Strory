# 12 — Folder 09 Revision + Exercises

## Prerequisites
Lessons 01–11 aur saare 7 examples chalaye hue (`06_oob_asan` bhi — woh STL OOB pe
abort hota hai).

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
<details><summary>Answer</summary>`1 2 0 0 0 ` — partial init zeros the rest.</details>

### B2
```cpp
int a[] = {10, 20, 30, 40};
std::cout << std::size(a) << " " << sizeof(a) << " " << *(a + 2);
```
<details><summary>Answer</summary>`4 16 30`</details>

### B3
```cpp
void f(int a[]) { std::cout << sizeof(a); }
int main() { int x[10]; std::cout << sizeof(x) << " "; f(x); }
```
<details><summary>Answer</summary>`40 8` — array size, then pointer size (decay).</details>

### B4
```cpp
int m[2][3] = {1, 2, 3, 4, 5, 6};
std::cout << m[1][0] << " " << *(&m[0][0] + 4);
```
<details><summary>Answer</summary>`4 5` — row-major: `m[1][0]` is offset 3; `&m[0][0]+4` is offset 4 = `m[1][1]` = 5.</details>

### B5
```cpp
std::array<int, 3> a = {1, 2, 3}, b = {1, 2, 3};
std::cout << std::boolalpha << (a == b) << " " << (a.data() == b.data());
```
<details><summary>Answer</summary>`true false` — element-wise equal, different storage.</details>

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
std::cout << canary;   // (undefined -- may print 7, 42, or crash)
```
<details><summary>Answer</summary>UB — `a[5]` OOB write. Often prints `7` (canary overwritten) at `-O0`; anything at `-O2`. This is why we use tools.</details>

### B8
```cpp
std::array<int, 4> a;
std::cout << a[0];
```
<details><summary>Answer</summary>Garbage — local `std::array` of trivial type, no `{}`. Use `std::array<int,4> a{};`.</details>

---

## PART C — Find the bug

### C1
```cpp
int daily[7];
for (int i = 1; i <= 7; ++i) daily[i] = readTemp();
```
<details><summary>Answer</summary>Start `1` (misses `daily[0]`), `<= 7` (`daily[7]` OOB). `for (int i = 0; i < 7; ++i)`.</details>

### C2
```cpp
double avg(int scores[]) {
    int n = sizeof(scores) / sizeof(scores[0]);
    int sum = 0;
    for (int i = 0; i < n; ++i) sum += scores[i];
    return double(sum) / n;
}
```
<details><summary>Answer</summary>Decay: `sizeof(scores)` = 8, `n` = 2. Pass the size, or `std::span<const int>`.</details>

### C3
```cpp
int* firstN(int n) {
    int buf[100];
    for (int i = 0; i < n; ++i) buf[i] = i;
    return buf;
}
```
<details><summary>Answer</summary>Returns pointer to dead local (decay + local lifetime). Also `n > 100` → OOB. Return `std::array`/`std::vector`, or fill a caller's `std::span`.</details>

### C4
```cpp
std::vector<int> v = {1, 2, 3};
int* p = &v[0];
v.push_back(4);
std::cout << *p;
```
<details><summary>Answer</summary>`push_back` may reallocate → `p` dangling → UB. `reserve(4)` first, or re-fetch `&v[0]` after.</details>

### C5
```cpp
int a[3][3];
for (int j = 0; j < 3; ++j)
    for (int i = 0; i < 3; ++i)
        a[i][j] = i + j;
```
<details><summary>Answer</summary>Correct output, but column-major traversal (inner loop `i`) — cache-hostile. Swap loop order to `i` outer, `j` inner.</details>

### C6
```cpp
int n = getCount();
int buffer[n];
```
<details><summary>Answer</summary>VLA — non-standard in C++, stack-overflow risk if `n` attacker-controlled. `std::vector<int> buffer(n)`.</details>

### C7
```cpp
int a[4] = {1,2,3,4}, b[4];
b = a;
```
<details><summary>Answer</summary>Compile error — arrays not assignable. `std::copy`, or `std::array` (`b = a` works).</details>

### C8
```cpp
char name[5] = "hello";
std::cout << name;
```
<details><summary>Answer</summary>No room for `'\0'` (5 chars + terminator needs 6). Some compilers error; if it compiles, `std::cout << name` reads past the array (OOB). `char name[6]` or `char name[]`.</details>

---

## PART D — Practical tasks

### D1. Stats over a span
`struct Stats { int min, max; double mean; long long sum; };`
`Stats analyze(std::span<const int> data);` — one pass, handle empty (throw or
`std::optional<Stats>`). Test with C array, `std::array`, `std::vector`, subspan.

### D2. In-place operations
`std::span<int>` versions of: `reverse`, `rotateLeft(k)`, `partitionEvens`
(evens before odds, stable), `dedupSorted` (return new logical size). No
allocation. Compare to `std::` equivalents.

### D3. Matrix library (flat storage)
```cpp
struct Matrix {
    std::size_t rows, cols;
    std::vector<double> data;                 // row-major, rows*cols
    double& at(std::size_t i, std::size_t j);
};
Matrix multiply(const Matrix& a, const Matrix& b);   // i, k, j loop order
```
Benchmark `i,j,k` vs `i,k,j` loop order at `-O2` (folder 07 file 09 idea).

### D4. AoS ↔ SoA converter
`struct ParticleAoS { float x,y,z,vx,vy,vz; };`
`struct ParticleSoA { std::vector<float> x, y, z, vx, vy, vz; };`
Write `toSoA(std::span<const ParticleAoS>)` and `toAoS(const ParticleSoA&)`.
Benchmark a "gravity step" (touches all 6) and a "sum of x" (touches 1) on both.

### D5. Ring buffer (power-of-2, `std::array` storage)
```cpp
template <class T, std::size_t Cap>          // Cap must be power of 2
class Ring {
    static_assert((Cap & (Cap - 1)) == 0);
    std::array<T, Cap> buf_;
    std::size_t head_ = 0, tail_ = 0;
    // push, pop, size, empty, full -- wrap with & (Cap - 1), no %
};
```
Zero heap. Test wrap-around thoroughly.

### D6. Safe wire decoder
`std::span<const std::byte>` input. Read a header (`u16 len`, `u8 type`), then
`len` bytes of payload. Every read bounds-checked against `.size()`. Feed it
truncated / oversized inputs — never OOB, always a clean error.

### D7. `_GLIBCXX_ASSERTIONS` / ASan lab
Take `06_oob_asan.cpp`. On this machine: `./build.ps1 san` catches BUG 4. If you
have WSL/Linux/Clang: `-fsanitize=address,undefined` — does it catch BUG 1–3
(raw array) too? Write down each tool's coverage.

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
[ ] AoS vs SoA -- kab kaunsa, ~4x measured dekha
[ ] Stack vs heap array, reserve() ka role
[ ] delete vs delete[], iterator invalidation pata hai
[ ] ASan / _GLIBCXX_ASSERTIONS / -Warray-bounds ka coverage pata hai
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
    std::array<Level, Depth> levels_{};       // sorted, best at [0]
    std::size_t count_ = 0;
public:
    void insert(std::int64_t price, std::int64_t qty);   // keep sorted
    void erase(std::int64_t price);
    void modify(std::int64_t price, std::int64_t qty);
    Level best() const;                                    // O(1)
    std::span<const Level> view() const;
};
```

Requirements: **zero heap**, best bid/ask O(1), insert/erase keep sorted (shift in
array — measure vs a sorted `std::vector`). Bounds-safe. Bench 1M random ops.
`-Wall -Wextra -Wshadow -Werror` clean. Foundation for folder 39.

### Challenge 2: "Array bug museum"

Every bug from lesson 11 as a live, safety-instrumented demo: buggy version +
compiler warning (exact text) + fixed version + an input where the bug produces
visibly wrong output/behaviour. Run under `./build.ps1 san` and (if available)
ASan — record what each tool catches. Extend `06_oob_asan.cpp`.

### Challenge 3: "2D convolution — layout & loop-order study"

3×3 kernel over an `N×N` (`N = 2048`) `float` image, flat `std::vector<float>`
storage. Implement:
1. Naive `i, j, ki, kj`
2. Loop reorder for sequential inner access
3. Tiled (`32×32` blocks)
4. `-O3 -march=native` (vectorized)

Benchmark all four at `-O2` and `-O3 -march=native`. GFLOP/s table. Explain each
jump (cache, prefetch, SIMD). `-fopt-info-vec` to confirm vectorization.

---

## 🎉 Folder 09 complete

Arrays: contiguous layout, `a[i] == *(a+i)`, **array decay** (the pointer
gateway), 2D row-major, `std::array` (size-safe, zero overhead), `std::span`
(the modern array parameter), AoS vs SoA (~4x measured), and the full bug
catalogue with tooling.

Agla: **strings** — jo andar se `char` arrays hain, plus `std::string` ka SSO.

---

## Next
→ [`../10-STRINGS/00-README.md`](../10-STRINGS/00-README.md)
