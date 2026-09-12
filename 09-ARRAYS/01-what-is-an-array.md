# 01 — Array kya hai

## Prerequisites
- `08-FUNCTIONS/` (poora)
- `03-VARIABLES-DATA-TYPES/05-int-deep-dive.md` (bytes, `sizeof`)
- `01-PROGRAMMING-BASICS/11-memory-basics.md` (RAM, addresses)

## Yeh folder kyun
Ab tak ek variable = ek value. Real programs ko **kai values ek saath** chahiye —
100 scores, ek din ke saare trades, ek image ke pixels. Array iske liye sabse
basic tool hai: **same type ke elements, ek contiguous memory block mein**.

Aur is folder ka asli inaam: **array decay** (file 05) — jahan array ek pointer
ban jaata hai. Yeh folder 12 (pointers) ka darwaza hai.

---

## Array — ek block, kai slots

```cpp
int scores[5];          // 5 int ke liye ek block -- 5 * 4 = 20 bytes contiguous
```

```
   index:      0     1     2     3     4
             ┌─────┬─────┬─────┬─────┬─────┐
   scores:   │ 90  │ 82  │ 71  │ 65  │ 50  │
             └─────┴─────┴─────┴─────┴─────┘
   address:  1000  1004  1008  1012  1016      (har int 4 bytes aage)
```

- **Same type** — `int[5]` mein sab `int`. Mixed types nahi (uske liye `struct`, folder 11)
- **Fixed size** — `[5]` compile-time pe pakka. Baad mein badal nahi sakte
- **Contiguous** — ek continuous block, koi gap nahi
- **Zero-indexed** — pehla element `scores[0]`, aakhri `scores[4]` (na ki `[5]`)

---

## Access — `scores[i]`

```cpp
scores[0] = 90;                 // pehla
scores[4] = 50;                 // aakhri (size 5 -> index 0..4)
int x = scores[2];              // padho -> 71
```

`scores[i]` ka matlab: **base address + `i` * `sizeof(element)`** — ek
multiplication + ek load. O(1), constant time, chahe `i` 0 ho ya 4.

⚠️ `scores[5]` — **out of bounds**. C++ check **nahi** karta. Kabhi garbage,
kabhi crash — **undefined behaviour** (file 03, 11).

---

## Zero-based indexing — kyun

`scores[i]` = `*(scores + i)` (file 05). `scores` pehle element ka address hai.
Pehla element `scores + 0` pe hai → index `0`. Yeh pointer arithmetic ko
seedha rakhta hai — koi `-1` adjust nahi.

"`n` elements, indices `0` se `n-1`" — yeh half-open range `[0, n)` convention
loops mein (folder 07) isi se aata hai.

---

## Array vs alag-alag variables

```cpp
// ❌ 5 alag variables -- loop nahi laga sakte, function ko pass mushkil
int s0 = 90, s1 = 82, s2 = 71, s3 = 65, s4 = 50;

// ✅ ek array -- loop, function, algorithm sab kaam karte hain
int scores[5] = {90, 82, 71, 65, 50};
for (int s : scores) total += s;
```

Array = "yeh cheezein related hain, aur inke saath ek jaisa kaam hota hai."

---

## Size — `sizeof` aur `std::size`

```cpp
int a[5] = {10, 20, 30, 40, 50};

sizeof(a)                       // 20  -- poore array ke BYTES
sizeof(a) / sizeof(a[0])        // 5   -- elements (purana idiom)
std::size(a)                    // 5   -- elements (C++17, <iterator>, saaf)
```

⚠️ `sizeof(a)` sirf tab poora size deta hai jab `a` **abhi bhi ek array ho** —
function ko pass karne ke baad woh pointer ban jaata hai aur `sizeof` `8` de
deta hai (file 05, `02_array_decay.cpp`).

---

## Andar kya hota hai

- **Stack pe** (local array): frame ke andar `N * sizeof(T)` bytes reserve
  (folder 08 lesson 05). Allocation ~free (`rsp` move).
- **`scores[i]` access**: `mov eax, [scores + i*4]` — ek instruction. Isi liye
  arrays fast hain.
- **Contiguous + predictable stride** → hardware prefetcher aage ka data pehle
  se laata hai → cache-friendly (folder 07 file 09, file 10 yahan).
- No bounds metadata stored — array bas bytes hain, size compiler ke paas hai
  (jab tak decay na ho).

> **HFT relevance:** Arrays (aur `std::array`) HFT ka bread-and-butter hain —
> fixed-size, stack pe, **zero heap allocation**, cache-friendly, SIMD-friendly.
> Order book price levels, fixed-size message buffers, ring buffers — sab
> arrays. `std::vector` (dynamic) tab jab size runtime pe pata ho, par uski
> reallocation hot path se door rakhte hain (`reserve`, folders 19, 36). Rule:
> agar max size compile-time pe pata hai → array / `std::array`.

---

## Hands-on

`examples/01_array_basics.cpp` — declaration, init forms, traversal, contiguous
layout ke addresses:

```bash
./build.ps1 09-ARRAYS/examples/01_array_basics.cpp
```

---

## ⚠️ Traps

### Trap 1 — index `n` (off-by-one)
```cpp
int a[5];
a[5] = 0;              // ⚠️ OOB -- valid indices 0..4
```

### Trap 2 — runtime size (VLA)
```cpp
int n = getCount();
int a[n];             // ⚠️ C++ mein illegal (C mein VLA hai). std::vector use karo
```

### Trap 3 — uninitialized array
```cpp
int a[5];             // local -- GARBAGE values
std::cout << a[0];    // ⚠️ undefined
int b[5] = {};        // ✅ sab 0
```

### Trap 4 — `sizeof` after decay
```cpp
void f(int a[]) { std::cout << sizeof(a); }   // 8 (pointer), not array size
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int a[5]` mein index 0..5" | 0..4 — `n` elements, indices `[0, n)` |
| "Array ka size runtime pe de sakte ho" | Nahi (VLA) — compile-time constant, ya `std::vector` |
| "Local `int a[5]` sab 0 hote hain" | Garbage — `= {}` se zero karo |
| "`a[i]` linear search jaisa hai" | O(1) — `base + i*size`, ek load |
| "`sizeof(a)` hamesha array size" | Sirf jab `a` array ho (decay ke baad pointer) |

---

## Exercises

1. **Declare + fill:** `int temps[7]` — ek hafte ke temperatures. Loop se fill
   karo (`temps[i] = 20 + i`), phir print karo, phir average nikalo.

2. **Layout:** `int a[4]` ke har element ka address print karo
   (`&a[i]`). Consecutive addresses ka difference kya? (`sizeof(int)`)

3. **Off-by-one:** yeh code likho, `-O2` se compile —
   ```cpp
   int a[5] = {1,2,3,4,5};
   int sum = 0;
   for (int i = 0; i <= 5; ++i) sum += a[i];
   ```
   `-Warray-bounds` warning? `sum` kya aaya (kabhi kuch, kabhi crash)?

4. **`std::size`:** ek `double d[10]` pe `sizeof(d)`, `sizeof(d)/sizeof(d[0])`,
   `std::size(d)` — teenon print karo.

5. **VLA:** `int n = 5; int a[n];` compile karo. Error kya? `constexpr int n = 5;`
   se? `std::vector<int> a(n);` se?

6. **Contiguity use:** `int a[6] = {10,20,30,40,50,60}` — `*(a + 3)` kya deta
   hai? `a[3]` ke barabar? (Preview — file 05)

---

## Interview questions

1. Array kya hai — 4 properties (same type, fixed size, contiguous, zero-indexed)?
2. `a[i]` ki time complexity aur woh kyun?
3. Zero-based indexing kyun (pointer arithmetic se connection)?
4. C++ mein VLA (variable-length array) hai? Alternative?
5. `sizeof(array)` kab poora size deta hai, kab nahi?
6. Local array initialize na karne pe kya hota hai?

---

## Next
→ [`02-declaring-and-initializing.md`](02-declaring-and-initializing.md)
