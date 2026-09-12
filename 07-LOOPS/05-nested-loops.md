# 05 — Nested loops aur complexity

## Prerequisites
- [`03-for-loop.md`](03-for-loop.md), [`04-range-based-for.md`](04-range-based-for.md)
- `06-CONDITIONS/03-nested-conditions.md` (nesting ka concept)

## Yeh topic abhi kyun
Ek loop ke andar doosra loop — 2D data (grids, matrices, tables), har-pair
comparisons, pattern printing. Yeh powerful hai, par ek chhupa hua khatra bhi:
**har extra nesting level kaam ko multiply kar deta hai.** 3 nested loops over
`n = 1000` = ek **arab** iterations. Isliye complexity ki samajh yahin se shuru
hoti hai.

---

## Basic shape

```cpp
for (int i = 0; i < rows; ++i) {         // OUTER -- har row
    for (int j = 0; j < cols; ++j) {     // INNER -- har col, HAR row ke liye poora chalta hai
        // yeh body  rows * cols  baar chalti hai
    }
}
```

**Inner loop poora chalta hai, phir outer ek step aage badhta hai, phir inner
DOBARA poora chalta hai.**

```cpp
for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 2; ++j)
        std::cout << "(" << i << "," << j << ") ";
// (0,0) (0,1) (1,0) (1,1) (2,0) (2,1)
```

Body `3 * 2 = 6` baar chali.

---

## Inner loop outer variable pe depend kar sakta hai

### Right triangle
```cpp
for (int row = 1; row <= 5; ++row) {
    for (int col = 1; col <= row; ++col) {   // <- col <= row  (row badhta hai)
        std::cout << "* ";
    }
    std::cout << "\n";
}
```
```
*
* *
* * *
* * * *
* * * * *
```
Total stars = `1 + 2 + 3 + 4 + 5 = 15` = `n(n+1)/2` ≈ `n²/2`.

### Pairs without repeat (i < j)
```cpp
for (std::size_t i = 0; i < v.size(); ++i)
    for (std::size_t j = i + 1; j < v.size(); ++j)     // j = i+1 -> har pair ek hi baar
        consider(v[i], v[j]);
```
`n(n-1)/2` pairs — still O(n²).

---

## COMPLEXITY — kitna kaam?

| Structure | Iterations | Notation |
|---|---|---|
| ek loop, `n` | `n` | **O(n)** |
| 2 nested, `n` each | `n * n` | **O(n²)** |
| 3 nested | `n³` | **O(n³)** |
| loop `n`, inner `m` | `n * m` | O(n·m) |
| loop jisme har baar aadha (`n /= 2`) | `log n` | O(log n) |
| outer `n`, inner `log n` | `n log n` | O(n log n) |

### Numbers mein socho (`~1 arab iterations/sec` maan lo)

| `n` | O(n) | O(n log n) | O(n²) | O(n³) |
|---|---|---|---|---|
| 1,000 | instant | instant | ~1 ms | ~1 sec |
| 10,000 | instant | instant | ~0.1 sec | ~17 min |
| 100,000 | instant | ~instant | ~10 sec | ~11 din |
| 1,000,000 | ~1 ms | ~20 ms | ~17 min | ~30 saal |

**`n` ko 10x karo → O(n²) 100x slow, O(n³) 1000x slow.** Yeh "thoda bada data"
pe program ko rok deta hai.

`examples/03_nested_patterns.cpp` section 5 — triple nested, `N = 40` → `64,000`
iterations. `N` = 400 → `64,000,000`. Khud badhakar dekho.

---

## Nested loop hatane ke tareeke (jab possible ho)

Nesting kabhi-kabhi **zaroori** hoti hai (matrix ops, sab-pairs). Par kabhi ek
better data structure O(n²) → O(n) kar deta hai:

```cpp
// ❌ O(n*m) -- har order ke liye poori list scan
for (const auto& order : orders)
    for (const auto& fill : fills)
        if (fill.orderId == order.id) order.filled += fill.qty;

// ✅ O(n + m) -- ek hash map se lookup O(1)
std::unordered_map<int, long long> fillByOrder;
for (const auto& fill : fills) fillByOrder[fill.orderId] += fill.qty;
for (auto& order : orders) order.filled += fillByOrder[order.id];
```

**Signal:** agar inner loop ka kaam "kisi cheez ko dhoondhna" hai → shayad ek
`map` / `set` / sorted array + binary search se woh O(1)/O(log n) ho jaaye.

---

## ⚠️ Traps

### Trap 1 — inner loop ka counter outer se share
```cpp
int i;
for (i = 0; i < n; ++i)
    for (i = 0; i < m; ++i)      // ⚠️ SAME i -- outer kabhi aage nahi badhta -> infinite/wrong
        ...
```
Alag naam do (`i`, `j`). `-Wshadow` inner-declared shadowing pe warn karta hai —
par yahan to same variable hai, warning bhi nahi.

### Trap 2 — inner loop ki condition/bound outer variable maan ke bhool jaana
```cpp
for (int i = 0; i < n; ++i)
    for (int j = 0; j < n; ++j)     // theek
        ...
// vs "sirf aage wale" chahiye the:
    for (int j = i; j < n; ++j)     // ya i+1
```

### Trap 3 — `break` sirf ek level todta hai
```cpp
for (...) {
    for (...) {
        if (found) break;      // ⚠️ sirf INNER loop se nikla; outer chalta rahega
    }
}
```
Fix: flag, `goto` (rare), ya loop ko function mein daal ke `return` (file 06,
folder 08).

### Trap 4 — accidental O(n²) inside a "single" loop
```cpp
for (const auto& x : items)
    if (std::find(seen.begin(), seen.end(), x) != seen.end())   // ⚠️ find = O(n) -> loop O(n²)
        ...
```
`std::unordered_set seen;` → `seen.count(x)` O(1).

### Trap 5 — nested loop mein wrong traversal order (cache)
```cpp
for (j ...) for (i ...) sum += m[i*N + j];   // ⚠️ column-major -> ~8x slower (file 09)
for (i ...) for (j ...) sum += m[i*N + j];   // ✅ row-major
```

---

## Andar kya hota hai

Nested loops ka koi special cost nahi — bas ek loop ke andar doosra loop ka
assembly. Outer loop ka branch predictable, inner loop ka branch bhi predictable
(sirf iteration boundaries pe "miss"). `-O2` inner loop ko vectorize kar sakta
hai; outer ko unroll.

**Cost complexity se aati hai, nesting ke "mechanism" se nahi.** `for(i) for(j)`
aur "ek loop jo `i*j` baar chale" — same machine work.

> **HFT relevance:** Order book updates, matching, risk aggregation — sab me
> "har order × har price level" jaisa kaam ho sakta hai. Naive O(n²) matching ek
> busy market mein deadline miss kara deta hai. Isi liye order books **sorted
> maps / intrusive lists / array-of-price-levels** se banaye jaate hain — best
> bid/ask O(1), insert/cancel O(log n). Folders 39–40 mein poora banayenge.
> Rule: nested loop dikhe hot path mein → complexity likho, data structure socho.

---

## Hands-on

`examples/03_nested_patterns.cpp` — rectangle, triangle, pyramid, multiplication
table, triple-nested counter:

```bash
./build.ps1 07-LOOPS/examples/03_nested_patterns.cpp
```

Experiment: section 5 mein `N` ko 40 → 100 → 200 karo. Runtime kaise badha?

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Nested loop khud slow hota hai" | Cost iterations ki ginti (complexity) se — mechanism se nahi |
| "`n` thoda badhega to thoda slow" | O(n²): 10x data = 100x time |
| "`break` saare loops se nikaalta hai" | Sirf apna (innermost) loop |
| "Inner `std::find` theek hai" | Loop ke andar O(n) search = O(n²); `unordered_set` |
| "2D array kaise traverse karo, farq nahi" | Row-major vs column-major: ~8x (file 09) |

---

## Exercises

1. **Iterations gino:**
   ```cpp
   for (int i = 0; i < 5; ++i)
       for (int j = 0; j < i; ++j)
           ++count;
   ```
   <details><summary>Answer</summary>`0+1+2+3+4 = 10`.</details>

2. **Patterns:** yeh 4 print karo (nested `for`):
   ```
   (a) 5x5 square of *      (b) inverted triangle (5 rows, 5..1 stars)
   (c) right-aligned triangle   (d) hollow square (border * , andar space)
   ```

3. **Multiplication table:** `1..12` ka table, columns aligned (`std::setw`).

4. **All pairs:** `std::vector<int> v` mein aise pairs `(i, j)` gino jinka
   `v[i] + v[j] == target` (`i < j`). O(n²) version likho. Phpir `unordered_map`
   se O(n) version — dono ka answer match karo.

5. **Complexity likho:** in snippets ke liye Big-O —
   ```cpp
   // A
   for (int i = 0; i < n; ++i) sum += a[i];
   // B
   for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) c[i][j] = 0;
   // C
   for (int i = 1; i < n; i *= 2) ++k;
   // D
   for (int i = 0; i < n; ++i) for (int j = 1; j < n; j *= 2) ++k;
   ```
   <details><summary>Answer</summary>A: O(n). B: O(n²). C: O(log n). D: O(n log n).</details>

6. **Break both loops:** ek `n x n` grid mein pehla `(i, j)` dhoondho jahan
   `grid[i][j] == target`, dono loops se turant niklo. 3 tareeke (flag, `goto`,
   function+`return`) — kaunsa clean?

7. **Cache order:** `examples/05_cache_locality.cpp` chalao (file 09 se). `for(i)
   for(j)` vs `for(j) for(i)` ka time ratio likho.

---

## Interview questions

1. Nested loop ki complexity kaise nikalte ho? `for(i<n) for(j<i)` kitne iterations?
2. Nested loop khud slow hai, ya cost kahin aur se aati hai?
3. `break` nested loops mein kya todta hai? Sab se nikalne ke 3 tareeke?
4. Ek O(n²) nested loop ko O(n) kaise bana sakte ho — example?
5. O(n²) algorithm pe `n` 10x karne se runtime kitna badhta hai?
6. Loop ke andar `std::find` / linear search — kya problem?

---

## Next
→ [`06-break-continue.md`](06-break-continue.md)
