# 06 — Multidimensional arrays aur row-major layout

## Prerequisites
- [`05-array-decay.md`](05-array-decay.md)
- `07-LOOPS/05-nested-loops.md`, `07-LOOPS/09-loop-performance.md` (cache locality ~8x)

## Yeh topic abhi kyun
Grids, matrices, images, lookup tables — 2D data. C++ mein `int m[3][4]` ek
**contiguous 1D block** hai jise 2D "dikhaya" jaata hai. Layout **row-major** hai,
aur traversal order cache ke liye ~8x tak farq karta hai (folder 07 mein measure kiya).

---

## `int m[ROWS][COLS]` — ek block

```cpp
int m[3][4];        // "3 rows, har row 4 int" = 12 int = 48 bytes CONTIGUOUS
```

```
   Memory (row-major):
   ┌────────────────────┬────────────────────┬────────────────────┐
   │  m[0][0..3]         │  m[1][0..3]         │  m[2][0..3]         │
   └────────────────────┴────────────────────┴────────────────────┘
   offset:  0  1  2  3     4  5  6  7           8  9  10 11
```

**`m[i][j]` ka address = `&m[0][0] + (i * COLS + j)`.** Pehle row 0 poori, phir
row 1, phir row 2. (`examples/03_2d_arrays.cpp` isse addresses ke saath dikhata hai.)

```cpp
&m[0][0] + 5  ==  &m[1][1]        // true
```

---

## Initialization

```cpp
int m[3][4] = {
    {1, 2, 3, 4},
    {5, 6, 7, 8},
    {9, 10, 11, 12},
};
int z[3][4] = {};                    // sab 0
int p[3][4] = {{1, 2}};              // row 0 = {1,2,0,0}, rows 1-2 sab 0
int q[3][4] = {1, 2, 3, 4, 5};       // ⚠️ flat -- row 0 full, row 1 = {5,0,0,0}
int r[][4]  = {{1,2,3,4},{5,6,7,8}}; // ROWS deduce -> 2. COLS ZAROORI dena hai
```

**Sirf pehla dimension `[]` ho sakta hai** (compiler count kare); baaki
`[COLS]...` explicit chahiye — warna compiler `m[i]` ka size nahi jaanta.

---

## `m[i]` ek row hai — `int[COLS]`

```cpp
int m[3][4];

m[1]             // type: int[4]  -> decays to int*  (row 1 ka pehla element)
m[1][2]          // == *(m[1] + 2) == *(*(m + 1) + 2)
int* row = m[1]; // row 1 ko 1D array ki tarah use karo
int (*rp)[4] = &m[1];   // "pointer to array-of-4-int"
(*rp)[2]         // == m[1][2]
```

### Decay 2D pe
```cpp
void f(int m[3][4]);    //  ┐
void f(int m[][4]);     //  ├─ teenon SAME: void f(int (*m)[4])
void f(int (*m)[4]);    //  ┘   (pehla dim decay; COLS type mein rehta hai)
```

`f(int m[][])` ❌ — COLS pata nahi to `m[i]` ka stride pata nahi.

---

## ⚠️ Traversal order — cache

```cpp
// ✅ ROW-MAJOR -- inner loop = j (last index) -> sequential -> cache-friendly
for (std::size_t i = 0; i < ROWS; ++i)
    for (std::size_t j = 0; j < COLS; ++j)
        m[i][j] = f(i, j);

// ❌ COLUMN-MAJOR traversal -- inner loop = i -> stride COLS*4 bytes -> ~8x slower
for (std::size_t j = 0; j < COLS; ++j)
    for (std::size_t i = 0; i < ROWS; ++i)
        m[i][j] = f(i, j);
```

Folder 07 file 09 mein 4096×4096 matrix pe **8.4x** measure kiya. Rule: **inner
loop wahi index jo memory mein fastest badalta hai** (row-major → last index).

---

## Flat 1D + manual indexing — bade code mein prefer

```cpp
constexpr std::size_t ROWS = 3, COLS = 4;
std::vector<int> grid(ROWS * COLS);           // runtime size, heap

auto at = [&](std::size_t i, std::size_t j) -> int& { return grid[i * COLS + j]; };
at(1, 2) = 99;                                 // == "row 1, col 2"
```

Faayde:
- **Runtime dimensions** (2D C array ke dimensions compile-time)
- Ek allocation, clean pointer math
- `std::span` / `std::mdspan` (C++23) ke saath kaam karta hai
- Function ko pass karna simple (`span` + dims)

⚠️ `int**` (pointer-to-pointer) 2D **NAHI** hai — woh scattered rows (har row
alag `new`) — non-contiguous, cache-hostile, N+1 allocations. Avoid.

---

## `std::array` 2D

```cpp
std::array<std::array<int, 4>, 3> m = {};     // contiguous, size yaad, no decay
m[1][2] = 5;
for (auto& row : m) for (int& x : row) x = 0;
```

Verbose hai par safe. Ya `std::mdspan` (C++23) ek flat buffer pe 2D view.

---

## Andar kya hota hai

- `int m[3][4]` = 48 contiguous bytes, ek stack frame slot. No pointers, no
  indirection — `m[i][j]` = `base + (i*4 + j)*4`, ek scaled load.
- `int**` = pointer to array of pointers, har `p[i]` ek alag heap block → **2
  indirections** per access + har row alag cache line/page.
- Row-major traversal → prefetcher happy → memory-bound loop runs at bandwidth.
- Column-major → cache line ka 1/16 use → 16x memory traffic + TLB misses.

> **HFT relevance:** Multi-level structures (order book: price levels × orders,
> or a correlation matrix) **flat contiguous buffers** hain with manual
> `i*stride + j` indexing — never `vector<vector<T>>` or `T**` (scattered, cache
> death). Traversal always in memory order. `std::mdspan` gives a clean 2D API
> over a flat buffer with zero overhead. Folders 32, 39.

---

## Hands-on

`examples/03_2d_arrays.cpp` — row-major layout with addresses, `m[i]` as 1D,
flat indexing:

```bash
./build.ps1 09-ARRAYS/examples/03_2d_arrays.cpp
```

---

## ⚠️ Traps

### Trap 1 — column-major traversal
```cpp
for (j) for (i) sum += m[i][j];    // ⚠️ ~8x slower
```

### Trap 2 — `int**` samajhna 2D array
```cpp
void f(int** m);   f(realMatrix);   // ❌ int[3][4] -> int(*)[4], NOT int**
```

### Trap 3 — flat init `{1,2,3,4,5}` for `[3][4]`
```cpp
int m[3][4] = {1,2,3,4,5};   // row 0 full, row 1 = {5,0,0,0}, row 2 = {0,0,0,0}
```

### Trap 4 — COLS omit karna
```cpp
void f(int m[][]);       // ❌ -- COLS chahiye stride ke liye
int m[][3];              // ✅ ROWS deduce, COLS given
```

### Trap 5 — `vector<vector<int>>` for performance-critical grid
```cpp
std::vector<std::vector<int>> grid(R, std::vector<int>(C));   // ⚠️ R+1 allocations, scattered
std::vector<int> grid(R * C);                                  // ✅ one block
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int m[3][4]` = 3 alag arrays" | Ek 48-byte contiguous block |
| "`int**` = 2D array" | Scattered rows; `int(*)[4]` = decayed 2D array |
| "Traversal order se farq nahi" | Row vs column ~8x (folder 07) |
| "`vector<vector<int>>` fast 2D" | Scattered + extra allocs; use flat `vector<int>` |
| "2D C array ke dimensions runtime ho sakte" | Compile-time. Runtime → flat `vector` + stride |

---

## Exercises

1. **Layout:** `int m[2][3]` — har `&m[i][j]` print, aur `&m[0][0]` se offset.
   `i*3 + j` se match?

2. **Row vs column:** `int m[1000][1000]` — dono traversal orders time karo
   (`-O2`). Ratio? (Folder 07 file 09 wala experiment 2D C array pe.)

3. **Flat helper:** `struct Grid { int rows, cols; std::vector<int> data; int&
   at(int i, int j); };` — implement + test.

4. **2D param:** `int rowSum(int m[][4], int rows, int rowIdx)` — likho, use karo.
   `int m[][3]` pe pass karne se? (Type mismatch.)

5. **`std::array` 2D:** `std::array<std::array<int,3>,2>` — fill, print, pass by
   `const&` to a function.

6. **`int**` trap:** `int** m` allocate karke 3x4 grid banao (`new`). Ab batao
   kitne allocations, aur `m[1][2]` mein kitni indirections. Flat version se
   compare.

---

## Interview questions

1. `int m[3][4]` memory mein kaise laid out hai? `m[i][j]` ka address formula?
2. `int[3][4]` function ko pass karne pe kya ban jaata hai? `int**` kyun nahi?
3. Row-major vs column-major traversal — performance farq aur kyun?
4. `vector<vector<int>>` vs flat `vector<int>` for a grid — trade-offs?
5. `int m[i]` (ek row) ka type kya? Decay?
6. 2D array ke dimensions runtime pe kaise? (Flat + stride / `mdspan`.)

---

## Next
→ [`07-arrays-as-parameters.md`](07-arrays-as-parameters.md)
