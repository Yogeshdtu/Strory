# 10 — Folder 07 Revision + Exercises

## Prerequisites
Is folder ke saare lessons (01–09) aur chhe examples chalaye hue.

---

## PART A — Concept check

### while / do-while
1. `while` aur `do-while` — output same condition pe kaise alag?
2. Ek sahi `while` loop ke 3 zaroori elements?
3. `while (true)` kab justified? Kya hona chahiye andar?
4. `do-while` ka `;` kahan, kyun?
5. `do-while` body ke variable ko condition mein kyun nahi use kar sakte?

### for / range-for
6. `for` ke 3 parts kab-kab evaluate hote hain (exact order)?
7. `for` vs `while` mein `continue` ka behaviour kaise alag?
8. Loop variable ka scope? Yeh faayda kyun?
9. `for (auto x : v)` — `x` copy hai ya reference? `auto&` / `const auto&` kab?
10. Range-`for` andar se kis code mein expand hota hai?
11. Range-`for` chalte waqt container mein `push_back` — kya hota hai?

### nested / complexity
12. `for(i<n) for(j<i)` kitne iterations? Big-O?
13. O(n²) pe `n` 10x → runtime kitna? O(n³) pe?
14. Ek O(n²) nested loop ko O(n) kaise — example?

### break / continue / goto
15. `break` nested loops mein kya todta hai? Sab se nikalne ke 3 tareeke?
16. `while` + `continue` → infinite loop kaise? Fix?
17. C++ mein labelled break hai? Alternatives?
18. `goto` kyun avoid? Kaunsa 1 case thoda defensible?

### bugs
19. Off-by-one — half-open range convention se kaise bachte hain?
20. `for (size_t i = n-1; i >= 0; --i)` — 2 bugs, 3 correct reverse patterns?
21. Float ko loop counter kyun nahi? Sahi tareeka?
22. Iterator invalidation kya hai? Loop mein `erase` kaise?

### patterns
23. Accumulator pattern — identity value ka matlab (sum/product/min)?
24. `std::accumulate` ke initial value ka type kyun matter karta hai?
25. Two-pointer technique — 2 problems jahan O(n²) → O(n)?
26. Loop vs `<algorithm>` — decision kaise?

### performance
27. Cache line kya? Sequential vs strided access — misses mein farq?
28. Row-major array ko column-major traverse — kitna slow, 3 reasons?
29. Loop-carried dependency kya? Reduction loop kyun affected?
30. Manual unroll ka faayda kahaan se? Auto-vectorization ko kya rokta hai?
31. `-O0` pe benchmark kyun invalid? `-march=native` kya karta hai?

---

## PART B — Output prediction

### B1
```cpp
int i = 1;
while (i <= 100) { i *= 2; }
std::cout << i;
```
<details><summary>Answer</summary>`128` — 1,2,4,...,64,128. `128 <= 100` false, loop exit, `i` = 128.</details>

### B2
```cpp
int n = 3;
do { std::cout << n << " "; --n; } while (n > 0);
do { std::cout << n << " "; --n; } while (n > 0);
```
<details><summary>Answer</summary>`3 2 1 0 ` — pehla loop: `3 2 1`, `n` = 0. Doosra do-while: body ek baar (`0`), `n` = -1, `-1 > 0` false.</details>

### B3
```cpp
for (int i = 0; i < 5; ++i) {
    if (i == 2) continue;
    if (i == 4) break;
    std::cout << i << " ";
}
```
<details><summary>Answer</summary>`0 1 3 ` — 2 skip, 4 pe break.</details>

### B4
```cpp
std::vector<int> v = {1, 2, 3};
for (auto x : v) x *= 2;
for (auto& x : v) x *= 2;
for (int x : v) std::cout << x << " ";
```
<details><summary>Answer</summary>`2 4 6 ` — pehla loop copy (no effect), doosra reference (doubles).</details>

### B5
```cpp
int count = 0;
for (int i = 0; i < 4; ++i)
    for (int j = 0; j <= i; ++j)
        ++count;
std::cout << count;
```
<details><summary>Answer</summary>`10` — `1+2+3+4` (j: 0..i inclusive).</details>

### B6
```cpp
int i = 0, sum = 0;
while (i < 5) {
    if (i == 2) { ++i; continue; }
    sum += i;
    ++i;
}
std::cout << sum;
```
<details><summary>Answer</summary>`8` — `0+1+3+4` (2 skip). `++i` continue se pehle hai isliye infinite nahi.</details>

### B7
```cpp
for (std::size_t i = 3; i-- > 0; ) std::cout << i << " ";
```
<details><summary>Answer</summary>`2 1 0 ` — `i-- > 0`: test `3>0`✓ i→2 print 2; `2>0`✓ i→1 print 1; `1>0`✓ i→0 print 0; `0>0`✗ stop.</details>

### B8
```cpp
double x = 0;
int guard = 0;
for (; x != 0.3; x += 0.1) if (++guard > 5) break;
std::cout << guard;
```
<details><summary>Answer</summary>`6` — `0.1 + 0.2` ≠ exactly `0.3`, so `x` never equals `0.3`; guard breaks at 6.</details>

---

## PART B2 — What happens next?

Output prediction poora output poochta hai. Yahan sawaal ek **khaas pal** ka hai: "is line
ke baad control **kahan** jaayega?" Pehle jawab likho, phir chala ke check karo. (Saare
jawab GCC 16.2 pe chala ke liye gaye hain.)

### N1
```cpp
for (int i = 0; i < 3; ++i) {
    if (i == 1) continue;     // <- i == 1 pe yahan pahunche. AGLA kaunsa kaam hoga?
    std::cout << i;
}
```
<details><summary>Answer</summary>

`continue` body ka baaki hissa (`std::cout << i`) **chhod deta hai** aur seedha loop ke
update `++i` pe jaata hai, phir condition `i < 3` check hoti hai. Isliye 1 print nahi hota.
Poora output: `02`.
</details>

### N2
```cpp
int i = 5;
while (i-- > 0)               // <- pehli baar condition check hui. i kitna hai, aur kya print hoga?
    std::cout << i;
```
<details><summary>Answer</summary>

`i--` **purani value** (5) se compare karta hai, **phir** `i` ko 4 karta hai. Isliye body mein
pehla print `4` hai, `5` nahi. Output: `43210`. Aakhri check mein `0 > 0` false hua, par
decrement tab bhi hua — loop ke baad `i` ki value `-1` hai.
</details>

### N3
```cpp
for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
        if (j == 1) break;    // <- yeh break kaunsa loop todega? Agla kaunsa line chalega?
        std::cout << i << j << ' ';
    }
```
<details><summary>Answer</summary>

`break` sirf **sabse andar wala** loop (j wala) todta hai. Control bahar wale loop ke `++i`
pe jaata hai, aur j phir 0 se shuru hota hai. Output: `00 10 20 ` — har `i` ke liye sirf
`j = 0` print hua.
</details>

### N4
```cpp
int i = 0;
do {
    std::cout << i;           // <- condition to pehle se false hai. Kya yeh line chalegi?
} while (i > 0);
```
<details><summary>Answer</summary>

Haan, **ek baar**. `do-while` body pehle chalata hai, condition baad mein check karta hai.
Output: `0`. Wahi code `while (i > 0) { ... }` se likhte to kuch print nahi hota.
</details>

---

## PART C — Find the bug

### C1
```cpp
int a[10];
for (int i = 1; i <= 10; ++i) a[i] = i * i;
```
<details><summary>Answer</summary>Start `1` (a[0] uninitialized), `<= 10` (a[10] OOB). `for (int i = 0; i < 10; ++i)`.</details>

### C2
```cpp
int i = 0;
while (i < items.size()) {
    if (items[i].skip) continue;
    process(items[i]);
    ++i;
}
```
<details><summary>Answer</summary>`continue` `++i` se pehle → skip item pe `i` stuck → infinite. Aur `int` vs `size()` sign-compare. Use `for`, ya `++i` pehle.</details>

### C3
```cpp
for (auto row : bigMatrix) {
    for (auto val : row) total += val;
}
```
<details><summary>Answer</summary>`auto row` har row (poora vector) copy karta hai. `const auto& row`. (`auto val` chhota int — theek, par `const auto&` bhi fine.)</details>

### C4
```cpp
for (std::size_t i = v.size() - 1; i >= 0; --i)
    std::cout << v[i];
```
<details><summary>Answer</summary>`i >= 0` unsigned → hamesha true. `i == 0` pe `--i` wrap → v[huge] OOB. `for (size_t i = v.size(); i-- > 0;)`.</details>

### C5
```cpp
std::vector<int> v = {1,2,3,4,5,6};
for (auto it = v.begin(); it != v.end(); ++it)
    if (*it % 2 == 0) v.erase(it);
```
<details><summary>Answer</summary>Iterator invalidation — `erase(it)` ke baad `it` invalid, `++it` UB. `it = v.erase(it)` (aur else `++it`), ya `std::erase_if(v, ...)`.</details>

### C6
```cpp
for (int i = 0; i < n; ++i) {
    process(data[i]);
    if (data[i].done) ++i;
}
```
<details><summary>Answer</summary>Body mein conditional `++i` + header `++i` → kabhi 2 ka step → element skip. Skip karna hai to `continue`; step chahiye to header mein.</details>

### C7
```cpp
double sum = 0;
for (double d = 1.0; d <= 1000000.0; d += 1.0) sum += 1.0 / d;
```
<details><summary>Answer</summary>Kaam karta hai par `d` float counter — bade values pe `d + 1.0` precision khota hai (eventually `d` badhna band). Integer counter: `for (int k = 1; k <= 1000000; ++k) sum += 1.0 / k;`</details>

### C8
```cpp
for (int i = 0; i < rows; ++i)
    for (int j = 0; j < cols; ++j)
        for (int k = 0; k < cols; ++k)
            result[i][j] += a[i][k] * b[k][j];   // b column-major access
```
<details><summary>Answer</summary>Correctness theek (matrix multiply), par `b[k][j]` inner loop `k` pe column-major → cache-hostile. Loop order `i, k, j` (b[k][j] sequential in j) ya b ko transpose. Folder 32.</details>

---

## PART D — Practical tasks

### D1. Loop-type triathlon
Ek hi kaam (`1..N` ka sum, print running total har 10th step) — teen versions:
`while`, `do-while`, `for`. Teenon same output. Kaunsa is task ke liye natural?

### D2. Input validation loop
`do-while` se: user se `1-100` maango jab tak valid na de. Invalid input
(`std::cin` fail state) bhi handle karo — `cin.clear()`, `cin.ignore()`
(folder 04 se). Retry count bhi print karo.

### D3. Pattern printer
Command-line se `n` lo, yeh print karo (nested `for`):
```
    *          1          *  *  *  *
   ***         22          *        *
  *****        333         *        *
 *******       4444        *        *
*********      55555       *  *  *  *
```
(pyramid, right-triangle numbers, hollow square) — teenon.

### D4. Loop-bug gauntlet
`examples/04_loop_bugs.cpp` ke har bug ka **fixed** version ek naye file mein
likho. `-Wall -Wextra -Werror` clean. `-fsanitize=address,undefined` clean.

### D5. Pattern library
`std::vector<int>` pe yeh functions (raw loop **aur** `<algorithm>` version):
```cpp
long long sum(const std::vector<int>&);
std::optional<std::size_t> firstIndexOf(const std::vector<int>&, int target);
std::vector<int> filterPositive(const std::vector<int>&);
std::pair<int,int> minMax(const std::vector<int>&);          // throw/optional on empty
bool isPalindrome(const std::vector<int>&);                  // two-pointer
long long maxWindowSum(const std::vector<int>&, int k);      // sliding window
```
Har ek ke 5+ test cases (empty, size 1, all same, sorted, random).

### D6. Cache locality benchmark
`examples/05_cache_locality.cpp` extend — ek table:

| N (matrix size) | data size | row-major ms | column-major ms | ratio |
|---|---|---|---|---|
| 512 | 1 MiB | ? | ? | ? |
| 1024 | 4 MiB | ? | ? | ? |
| 2048 | 16 MiB | ? | ? | ? |
| 4096 | 64 MiB | ? | ? | ? |

Ratio N badhne pe kaise badla? (Hint: jab data L2/L3 mein fit hota hai.)

### D7. Unroll investigation
`examples/06_loop_unroll.cpp` ko 5 flag-sets pe chalao (`-O0`, `-O2`, `-O3`,
`-O2 -march=native`, `-O3 -march=native`). Table banao (naive / manual / std).
Har row explain karo — kab manual jeeta aur kyun.

### D8. AoS vs SoA
`struct Particle { double x, y, z, vx, vy, vz; };` — 5M particles.
- AoS: `std::vector<Particle>`, sabhi `x` ka average
- SoA: 6 alag `std::vector<double>`, `xs` ka average
`-O2 -march=native` pe time. Ratio + cache-line explanation.

### D9. Prime sieve
Sieve of Eratosthenes se `1..N` primes (`N = 10^7`). Nested loop structure. Time
karo. `std::vector<bool>` vs `std::vector<char>` — kaunsa tez aur kyun?

---

## PART E — Self-assessment

```
[ ] while / do-while / for -- teenon ka execution order clear hai
[ ] Mujhe pata hai for aur while mein continue kaise alag behave karta hai
[ ] Main loop variable ka scope samajhta hoon aur uska faayda
[ ] Mujhe half-open range [0, n) convention aati hai (i < n, <= nahi)
[ ] Mujhe unsigned reverse loop ka bug + 3 fixes pata hain
[ ] Main float ko loop counter nahi banata
[ ] Mujhe range-for mein auto vs auto& vs const auto& ka fark pata hai
[ ] Maine copy vs reference ka ~50x cost measure kiya hai
[ ] Mujhe iterator invalidation ka basic idea aur erase_if pata hai
[ ] Main nested loop ki complexity (O(n^2), O(n^3)) nikaal sakta hoon
[ ] Mujhe pata hai break sirf innermost loop todta hai + 3 exit tareeke
[ ] Main goto avoid karta hoon aur jaanta hoon kyun
[ ] Mujhe accumulator/search/filter/minmax/two-pointer patterns pehchaanne aate hain
[ ] Mujhe pata hai kab raw loop, kab <algorithm>
[ ] Mujhe cache line (64 B) aur sequential vs strided access ka fark pata hai
[ ] Maine row-major vs column-major ~8x farq khud measure kiya
[ ] Mujhe loop-carried dependency aur multi-accumulator ka idea hai
[ ] Maine loop-unroll benchmark alag flags pe chalaya aur dekha result badalta hai
[ ] Mujhe pata hai -O0 pe benchmark invalid hai
[ ] Mujhe auto-vectorization aur -march=native ka basic role pata hai
[ ] Maine chhe examples chalaye hain
```

**Scoring:**
- **18–20** → Excellent. Folder 08 (Functions) pe jao. 🎯
- **13–17** → Achha. Miss hue lessons dobara.
- **8–12** → Files 04, 07, 09 dobara, examples chalao.
- **< 8** → Poora folder dobara, har benchmark khud chalao.

---

## PART F — Challenge

### Challenge 1: "Conway's Game of Life"

`R x C` grid, nested loops. Rules: har cell ke 8 neighbours gino —
- alive + (2 ya 3 neighbours) → alive
- dead + (exactly 3) → alive
- warna → dead

Requirements:
- Double-buffer (naya grid alag, phir swap) — in-place mat karo (bug ka source)
- Boundary handling (edge cells ke neighbours kam) — clean, bina 8 alag `if`
- `N` generations simulate karo, har generation print
- `1D vector<char>` + `i*C + j` indexing (row-major) — cache-friendly
- Ek "glider" pattern se shuru, dekho woh move karta hai
- `-Wall -Wextra -Wshadow -Werror` clean

Extension: `R = C = 2000`, 100 generations time karo. Neighbour-count loop ko
optimize karo (measure pehle).

### Challenge 2: "Loop bug museum"

Ek program jisme har loop bug ka live demo ho (safety-capped), aur har ek ke liye:
1. Buggy version + compiler warning (exact text comment mein)
2. Fixed version
3. Ek input jahan bug **actually galat output / behaviour** deta hai

Bugs: off-by-one (dono taraf), infinite (no update), unsigned reverse, float
counter, counter-in-body, wrong direction, `while`+`continue`, iterator
invalidation, `break` wrong level, stray `;`.

`examples/04_loop_bugs.cpp` ko base banao, extend karo. `-fsanitize=address,undefined`
se bhi chalao — kya extra pakda?

### Challenge 3: "Matrix multiply — 4 versions"

`N x N` (`N = 512`) `double` matrices. `C = A * B`. Chaar versions, sabka `-O2`
aur `-O3 -march=native` benchmark:
1. Naive `i, j, k` (B column-major access — slow)
2. Loop reorder `i, k, j` (B row-major access)
3. Version 2 + tiling (`64 x 64` blocks)
4. Version 2/3 + `-march=native` auto-vectorization

Table: GFLOP/s per version per flag-set. Har jump explain karo (cache, vectorize).
`-fopt-info-vec` se check karo kaunsa loop vectorize hua.

---

## 🎉 Folder 07 complete

Aapne seekha:
- `while`, `do-while`, `for` — anatomy, equivalence, kab kaunsa
- Range-`for` — `auto` vs `auto&` vs `const auto&`, ~50x copy cost measured
- Nested loops — complexity O(n²)/O(n³), kab data structure badalna
- `break` / `continue` / `goto` — sahi use, nested exit, `while`+`continue` trap
- 7+ loop bugs — off-by-one, infinite, unsigned underflow, iterator invalidation
- Loop patterns — accumulator, search, filter, min/max, two-pointer, sliding window
- Loop performance — **cache locality ~8x measured**, unrolling (flag-dependent,
  surprising), auto-vectorization

Program ab **faisla leta hai** (folder 06) aur **repeat karta hai** (folder 07).
Agla: use **reusable** banana — functions, aur call stack.

---

## Next
→ [`../08-FUNCTIONS/00-README.md`](../08-FUNCTIONS/00-README.md)
