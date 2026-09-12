# 02 — Declaring aur initializing

## Prerequisites
- [`01-what-is-an-array.md`](01-what-is-an-array.md)
- `03-VARIABLES-DATA-TYPES/10-initialization-forms.md` (`{}` init, narrowing)

## Yeh topic abhi kyun
Array declare karne aur bharne ke ki forms hain — aur "partial init" ka ek chhupa
hua rule (baaki elements zero ho jaate hain) jise samajhna zaroori hai, warna
"kabhi zero kabhi garbage" wale bugs aate hain.

---

## Declaration

```cpp
int   scores[5];               // 5 int -- UNINITIALIZED (local -> garbage)
double prices[100];
char  name[32];

constexpr int N = 10;
int   buf[N];                  // ✅ size constexpr ho to chalega
// int  bad[getSize()];        // ❌ runtime size -- VLA, C++ mein illegal
```

`[SIZE]` **compile-time integral constant** hona chahiye — literal, `constexpr`,
`enum` value, `sizeof(...)`.

---

## Initialization forms

```cpp
int a[5] = {10, 20, 30, 40, 50};   // full aggregate init
int b[5] = {1, 2};                  // PARTIAL -> {1, 2, 0, 0, 0}
int c[5] = {};                      // sab 0
int d[5] = {0};                     // pehla 0, baaki bhi 0 (== {})
int e[]  = {7, 8, 9};               // size compiler counts -> 3
int f[5] {10, 20, 30, 40, 50};     // = optional (brace init)
```

### 🔑 Partial init rule

Jitne values diye, wahi set hote hain. **Baaki elements value-initialized (zero
for scalars).**

```cpp
int arr[1000] = {0};          // saare 1000 elements ZERO -- ek line
int arr[1000] = {};           // same
int arr[1000] = {1};          // arr[0]=1, arr[1..999]=0
```

⚠️ **`= {}` / `= {0}` diye bina, local array garbage hai:**
```cpp
int a[5];                     // garbage
int a[5] = {};                // sab 0
```

Global/static arrays automatically zero-init hote hain (folder 08 lesson 06) —
sirf local arrays garbage.

### Narrowing — `{}` pakadta hai

```cpp
int a[3] = {1, 2, 3.5};       // ⚠️ ERROR (or -Wnarrowing): 3.5 -> int narrowing
int a[3] = {1, 2, 300000000000};   // ⚠️ ERROR: won't fit
```

`{}` brace-init narrowing conversions ko error/warn karta hai — yeh achha hai
(folder 03).

---

## Char arrays — string literals

```cpp
char s1[] = "hello";          // {'h','e','l','l','o','\0'} -- size 6 (null terminator!)
char s2[6] = "hello";         // theek -- 5 chars + '\0'
char s3[5] = "hello";         // ⚠️ '\0' ke liye jagah nahi -- kuch compilers error
char s4[10] = "hi";           // {'h','i','\0',0,0,0,0,0,0,0}
```

C-strings folder 10 mein poora. Abhi yaad rakho: string literal ke saath ek
**hidden `'\0'`** aata hai.

---

## Multi-dimensional (file 06 mein poora)

```cpp
int grid[3][4] = {
    {1, 2, 3, 4},
    {5, 6, 7, 8},
    {9, 10, 11, 12},
};
int z[3][4] = {};                    // sab 0
int p[3][4] = {{1, 2}};              // row 0 = {1,2,0,0}, rows 1-2 sab 0
```

---

## `std::array` — same init, behtar type (file 08)

```cpp
#include <array>
std::array<int, 5> a = {10, 20, 30, 40, 50};
std::array<int, 5> b{};                          // sab 0
std::array c = {1, 2, 3};                         // CTAD -> std::array<int, 3>
```

Modern code mein `std::array` prefer karo — size yaad rakhta hai, `.at()`,
compare, copy sab kaam karta hai.

---

## Andar kya hota hai

- `int a[5] = {1, 2}` → compiler: `a[0]=1; a[1]=2;` phir `a[2..4]` ke liye
  memset-zero (ya loop). `-O2` pe optimal.
- `int a[1000] = {}` → aksar ek `memset(a, 0, 4000)` — bahut fast.
- Uninitialized `int a[5]` → **kuch nahi** hota. Frame ka jo bhi bytes the, wahi
  "values" hain (garbage).
- Bade arrays (`int a[100000]` on stack) → stack overflow risk (folder 08). Heap
  (`std::vector`) ya `static` use karo.

> **HFT relevance:** Fixed-size buffers `= {}` se zero-init karke start karte
> hain (deterministic, no garbage). Par **hot path mein bade buffer ko har baar
> zero karna** (memset cost) avoid — sirf jo bytes likhne hain wahi likho, ya
> ek baar init karke reuse karo. `std::array<T, N>` + designated fill patterns
> (folder 11) common hain.

---

## Hands-on

`examples/01_array_basics.cpp` — section 2 (partial init `{1,2}` → `{1,2,0,0,0}`):

```bash
./build.ps1 09-ARRAYS/examples/01_array_basics.cpp
```

---

## ⚠️ Traps

### Trap 1 — local array bina init
```cpp
int counts[10];           // garbage
for (int c : counts) sum += c;   // ⚠️ garbage sum
int counts[10] = {};      // ✅
```

### Trap 2 — `{1}` se "sab 1" ki ummeed
```cpp
int a[5] = {1};           // {1, 0, 0, 0, 0} -- sirf pehla 1!
std::array<int, 5> a;  a.fill(1);   // ✅ sab 1
```

### Trap 3 — char array mein `'\0'` bhoolna
```cpp
char s[5] = "hello";      // ⚠️ 5 chars, '\0' ke liye jagah nahi
```

### Trap 4 — bada array stack pe
```cpp
void f() { int big[1'000'000]; }   // ⚠️ 4 MB -- stack overflow. std::vector ya static
```

### Trap 5 — `int a[] = {}` (empty)
```cpp
int a[] = {};             // ⚠️ zero-size array -- non-standard extension, avoid
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int a[5] = {1}` sab 1" | `{1, 0, 0, 0, 0}` — partial init zeros baaki |
| "Local array default 0" | Garbage — `= {}` chahiye |
| "`int a[n]` runtime `n` chalega" | VLA — illegal in C++ |
| "`char s[5] = \"hello\"`" | `'\0'` chahiye — `char s[6]` ya `char s[]` |
| "Global aur local array same init" | Global zero-init; local garbage |

---

## Exercises

1. **Init forms:** in sabko print karo —
   ```cpp
   int a[4] = {1, 2, 3, 4};
   int b[4] = {1, 2};
   int c[4] = {};
   int d[]  = {5, 6, 7};
   std::cout << std::size(d);
   ```

2. **Garbage hunt:** `int x[5];` (bina init) ka har element print karo, 3 baar
   chalao. Values same hain ya alag?

3. **Zero a big array:** `int big[10000] = {};` — `-O2 -S` se dekho — `memset`
   call hai? Time karo (chrono) — kitna lagta hai?

4. **`std::array::fill`:** `std::array<int, 8> a;  a.fill(7);` — sab 7? Ab
   `std::array<int, 8> b = {7};` — kya?

5. **Narrowing:** `int a[3] = {1, 2, 3.9};` compile karo. Error/warning? `{1, 2,
   256}` for `char a[3]`?

6. **Designated (C++20, preview):** `int a[5] = {[2] = 30};` — ⚠️ C++ mein array
   designated init C++20 se pehle nahi tha aur ab bhi limited. `std::array` +
   loop use karo.

---

## Interview questions

1. `int a[5] = {1, 2}` — baaki 3 elements ki value?
2. Local vs global array — uninitialized behaviour?
3. Array size ke liye kya valid hai (constexpr, literal, ...)? Runtime value?
4. `char s[] = "abc"` ka size? Kyun?
5. `{}` brace-init narrowing pe kya karta hai — array init mein?
6. Bade array ko stack pe declare karne ka risk?

---

## Next
→ [`03-accessing-elements.md`](03-accessing-elements.md)
