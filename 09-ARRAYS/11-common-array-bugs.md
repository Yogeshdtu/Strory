# 11 — Common array bugs

## Prerequisites
- [`01-what-is-an-array.md`](01-what-is-an-array.md) … [`10-array-performance.md`](10-array-performance.md)
- `07-LOOPS/07-loop-bugs.md`, `06-CONDITIONS/07-conditional-bugs.md`

## Yeh topic abhi kyun
Array bugs C++ ke sabse khatarnak bugs hain — zyada tar **silent memory
corruption** (crash bhi nahi, bas galat behaviour, kabhi-kabhi). Yeh unka catalogue
hai. `examples/06_oob_asan.cpp` mein kai live hain.

---

## Bug 1 — Off-by-one (OOB by one)

```cpp
int a[5];
for (int i = 0; i <= 5; ++i) a[i] = 0;    // ⚠️ i == 5 -> a[5] OOB
```

Half-open range `[0, n)`: `i < n`, `<=` nahi. `n` elements, indices `0..n-1`.
Ya `std::size(a)`, range-for.

`-O2 -Warray-bounds` constant OOB kabhi pakadta hai. ASan runtime pe.

---

## Bug 2 — Out-of-bounds (arbitrary index)

```cpp
int field = buf[header.offset];           // ⚠️ offset network/config se -- bounded?
arr[userIndex] = value;                    // ⚠️ userIndex untrusted
```

**OOB READ** → garbage / kisi aur variable ki value. **OOB WRITE** → door ki
memory corrupt, dhoondhna sabse mushkil. Untrusted index → `.at()` ya explicit
`if (i < n)`.

---

## Bug 3 — Uninitialized array

```cpp
int counts[10];                            // local -> GARBAGE
for (int c : counts) total += c;           // ⚠️ garbage sum
int counts[10] = {};                       // ✅ sab 0
```

Global/static: auto zero. Local: garbage. `= {}` lagao.

---

## Bug 4 — Decay confusion (`sizeof` inside function)

```cpp
void process(int arr[]) {
    for (std::size_t i = 0; i < sizeof(arr) / sizeof(arr[0]); ++i) ...
    //                       8 / 4 = 2   -- sirf 2 elements (chahe array 1000 ka ho)
}
```

Function ke andar `arr` pointer hai. `sizeof(arr)` = 8. Pass the size, or use
`std::span` / `std::array` / reference-to-array. `-Wsizeof-array-argument` warns.
(File 05.)

---

## Bug 5 — VLA (variable-length array)

```cpp
int n = getCount();
int a[n];                                  // ⚠️ C++ mein illegal (C extension). Kuch compilers accept karte hain
```

Runtime-sized stack array → non-standard, stack-overflow risk (attacker controls
`n`), no `std::size`. Use `std::vector<int> a(n)`.

---

## Bug 6 — `delete` vs `delete[]` (heap arrays — preview, folder 14)

```cpp
int* a = new int[100];
delete a;                                  // ⚠️ UB -- should be delete[] a
delete[] a;                                // ✅
```

`new[]` → `delete[]`. `new` → `delete`. Mismatch = UB (heap corruption). **Better:
`std::vector` / `std::array` / `std::unique_ptr<T[]>` — no manual `delete` at all.**
(Folder 14, 17.)

---

## Bug 7 — Array comparison / assignment

```cpp
int a[3] = {1,2,3}, b[3] = {1,2,3};

if (a == b) { }                            // ⚠️ compares ADDRESSES (always false here)
a = b;                                     // ❌ compile error -- can't assign arrays

// ✅
if (std::equal(std::begin(a), std::end(a), std::begin(b))) { }
std::copy(std::begin(b), std::end(b), std::begin(a));
// or use std::array: a == b, a = b both work
```

---

## Bug 8 — Returning / storing pointer to local array

```cpp
int* makeData() {
    int a[5] = {1,2,3,4,5};
    return a;                              // ⚠️ decays to pointer to dead local -> UB
}
```

Return `std::array<int, 5>` (by value, RVO), or `std::vector`, or take an output
`std::span`. (Folder 08 lesson 05, 06.)

---

## Bug 9 — Iterator/pointer invalidation on `std::vector`

```cpp
std::vector<int> v = {1,2,3};
int& r = v[0];                             // or int* p = v.data();
v.push_back(4);                            // ⚠️ reallocation -> r / p dangling
std::span<int> s = v;  v.push_back(5);     // ⚠️ s dangling
```

`push_back`/`insert`/`resize` beyond capacity → reallocation → all references,
pointers, iterators, spans into `v` invalid. `reserve` upfront, or re-fetch after.

---

## Bug 10 — Wrong element size in manual indexing

```cpp
std::byte* buf = ...;
int x = *reinterpret_cast<int*>(buf + i * 2);   // ⚠️ stride 2, but int is 4 -- misread/overlap
int x = *reinterpret_cast<int*>(buf + i * sizeof(int));   // ✅
```

Manual `i * stride` — `stride` must match the element size. `sizeof(T)`, not a
guessed number.

---

## Detection — tools

| Bug class | Compile-time | Runtime |
|---|---|---|
| Constant OOB | `-O2 -Warray-bounds` | — |
| Any raw-array OOB | — | **ASan** (`-fsanitize=address`) — Linux/Clang |
| STL container `[]` OOB | — | `-D_GLIBCXX_ASSERTIONS` (this course's MinGW: default) |
| Decay `sizeof` | `-Wsizeof-array-argument` | — |
| Uninitialized read | `-Wuninitialized` (partial) | ASan / MSan / Valgrind |
| Heap misuse | — | ASan / Valgrind |

**HFT/serious CI: `-Wall -Wextra -Werror` + ASan + UBSan on every test run**, plus
`_GLIBCXX_ASSERTIONS` in debug builds. Fuzz parsers (folder 45).

> **HFT relevance:** Every one of these in a market-data decoder or order book is
> a crash-or-worse: an OOB read leaks adjacent state, an OOB write corrupts the
> book, a dangling span after a buffer swap feeds garbage to the matching engine.
> Defenses: `std::span`/`std::array` (size travels with the data), `.at()` on
> untrusted offsets, bounded indices + `[[assume]]` in release, ASan/UBSan/fuzz
> in CI, `reserve()` so vectors never regrow on the hot path.

---

## Hands-on

```bash
./build.ps1 09-ARRAYS/examples/06_oob_asan.cpp          # compiles; run shows the bugs
./build.ps1 san 09-ARRAYS/examples/06_oob_asan.cpp      # STL OOB caught (MinGW); ASan on Linux
./build.ps1 09-ARRAYS/examples/02_array_decay.cpp        # -Wsizeof-array-argument
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "OOB crashes, so I'd notice" | Often silent — wrong data / corruption |
| "`a == b` compares array contents" | Addresses — `std::equal` / `std::array` |
| "`int a[n]` works if it compiled" | Non-standard VLA — use `std::vector` |
| "`delete` frees a `new[]` array" | UB — `delete[]`. Better: no manual delete |
| "Compiler catches array bugs" | Some; most need ASan / `_GLIBCXX_ASSERTIONS` / tests |

---

## Exercises

1. **Spot 5:** in this snippet, find every array bug —
   ```cpp
   int scores[5];
   for (int i = 1; i <= 5; ++i) scores[i] = i * 10;
   int* p = getScores();     // returns &local[0]
   if (scores == p) { }
   int n = readN();  int buf[n];
   ```

2. **OOB write blast radius:** `int a[4] = {}; int canary = 0xABCD; a[4] = 999;` —
   `canary` badla? `-O0` vs `-O2`?

3. **Decay bug:** `int sumBuggy(int a[])` using `sizeof(a)/sizeof(a[0])` as the
   count. Call with a 10-element array. What does it return? Fix with `std::span`.

4. **`_GLIBCXX_ASSERTIONS` demo:** `std::vector<int> v = {1,2,3}; v[10] = 0;` —
   `./build.ps1 san <file>`. Message + exit code.

5. **Invalidation:** `std::vector<int> v = {1}; int& r = v[0]; for (int i=0;i<100;
   ++i) v.push_back(i); std::cout << r;` — `-fsanitize=address` (Linux) or reason
   about it. Fix with `reserve`.

6. **Safe rewrite:** take a C-array-heavy function (raw `int*`, manual size,
   `for i <= n`) and rewrite it with `std::span` + range-for. Diff the bug surface.

---

## Interview questions

1. Off-by-one bug — kaise, aur half-open range se kaise bache?
2. OOB read vs OOB write — kaunsa zyada khatarnak, kyun?
3. Decay `sizeof` bug — mechanism aur fix?
4. `a == b` (do C arrays) kya karta hai? Content compare kaise?
5. `std::vector` reallocation kya invalidate karta hai?
6. Array bugs pakadne ke tools — compile-time aur runtime?

---

## Next
→ [`12-exercises.md`](12-exercises.md)
