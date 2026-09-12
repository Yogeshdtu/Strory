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

Function ke andar `arr` pointer hai. `sizeof(arr)` = 8. Size alag se pass karo, ya
`std::span` / `std::array` / reference-to-array use karo. `-Wsizeof-array-argument` warning
deta hai. (File 05.)

---

## Bug 5 — VLA (variable-length array)

```cpp
int n = getCount();
int a[n];                                  // ⚠️ C++ mein illegal (C extension). Kuch compilers accept karte hain
```

Runtime size wala stack array → standard C++ nahi, stack overflow ka khatra (agar `n` attacker ke
haath mein ho), aur `std::size` bhi nahi chalta. `std::vector<int> a(n)` lo.

---

## Bug 6 — `delete` vs `delete[]` (heap arrays — jhalak, folder 14)

```cpp
int* a = new int[100];
delete a;                                  // ⚠️ UB -- delete[] a hona chahiye
delete[] a;                                // ✅
```

`new[]` ki jodi `delete[]`, `new` ki jodi `delete`. Galat jodi = UB (heap corruption). **Isse
behtar: `std::vector` / `std::array` / `std::unique_ptr<T[]>` — haath se `delete` likhna hi na
pade.** (Folder 14, 17.)

---

## Bug 7 — Arrays ko compare / assign karna

```cpp
int a[3] = {1,2,3}, b[3] = {1,2,3};

if (a == b) { }                            // ⚠️ ADDRESSES compare hote hain (yahan hamesha false)
a = b;                                     // ❌ compile error -- arrays assign nahi hote

// ✅
if (std::equal(std::begin(a), std::end(a), std::begin(b))) { }
std::copy(std::begin(b), std::end(b), std::begin(a));
// ya std::array lo: a == b, a = b dono chalte hain
```

---

## Bug 8 — Local array ka pointer return / store karna

```cpp
int* makeData() {
    int a[5] = {1,2,3,4,5};
    return a;                              // ⚠️ mar chuke local ka pointer ban jaata hai -> UB
}
```

`std::array<int, 5>` value se return karo (RVO), ya `std::vector`, ya output `std::span` lo.
(Folder 08 lesson 05, 06.)

---

## Bug 9 — `std::vector` pe iterator/pointer invalidation

```cpp
std::vector<int> v = {1,2,3};
int& r = v[0];                             // ya int* p = v.data();
v.push_back(4);                            // ⚠️ reallocation -> r / p dangling
std::span<int> s = v;  v.push_back(5);     // ⚠️ s dangling
```

Capacity ke bahar `push_back`/`insert`/`resize` → reallocation → `v` ke andar ke saare
references, pointers, iterators, spans invalid. Pehle se `reserve` karo, ya baad mein dobara lo.

---

## Bug 10 — Manual indexing mein galat element size

```cpp
std::byte* buf = ...;
int x = *reinterpret_cast<int*>(buf + i * 2);   // ⚠️ stride 2, par int 4 bytes -- galat padha / overlap
int x = *reinterpret_cast<int*>(buf + i * sizeof(int));   // ✅
```

Haath se `i * stride` likho to `stride` element ke size ke barabar hona chahiye — `sizeof(T)`,
andaaze wala number nahi.

---

## Detection — tools

| Bug class | Compile-time | Runtime |
|---|---|---|
| Constant OOB | `-O2 -Warray-bounds` | — |
| Koi bhi raw-array OOB | — | **ASan** (`-fsanitize=address`) — Linux/Clang |
| STL container `[]` OOB | — | `_GLIBCXX_ASSERTIONS` — GCC 16.2 pe **sirf `-O0` pe default on**; `-O2` ke liye `-D_GLIBCXX_ASSERTIONS` khud lagao |
| Decay `sizeof` | `-Wsizeof-array-argument` | — |
| Uninitialized read | `-Wuninitialized` (thoda-bahut) | ASan / MSan / Valgrind |
| Heap misuse | — | ASan / Valgrind |

### ⚠️ `_GLIBCXX_ASSERTIONS` ka "default" — chala ke check kiya
Ek hi program, `std::vector<int> v(3); int x = v[10];`, GCC 16.2 pe:

| Build | `_GLIBCXX_ASSERTIONS` | Nateeja |
|---|---|---|
| `-O0` (koi flag nahi) | defined | `Assertion '__n < this->size()' failed.` — program ruk gaya ✅ |
| `-O2` (koi flag nahi) | **defined nahi** | chupchaap garbage padha (`x=-1025965744`), koi error nahi ❌ |
| `-O2 -D_GLIBCXX_ASSERTIONS` | defined | assertion fail — pakda gaya ✅ |

Matlab: debug build mein aapko suraksha milti hai, par jis `-O2` build pe aap benchmark / release
karte ho, usme **nahi**. Optimized testing builds mein `-D_GLIBCXX_ASSERTIONS` khud lagao.

**HFT / serious CI: `-Wall -Wextra -Werror` + ASan + UBSan har test run pe**, aur test builds mein
`-D_GLIBCXX_ASSERTIONS`. Parsers ko fuzz karo (folder 45).

> **HFT relevance:** Market-data decoder ya order book mein inme se har bug crash ya usse bura
> hai: OOB read bagal ka state leak karta hai, OOB write book corrupt karta hai, buffer swap ke
> baad dangling span matching engine ko kachra khilata hai. Bachaav: `std::span`/`std::array`
> (size data ke saath chalti hai), untrusted offsets pe `.at()`, release mein bounded indices +
> `[[assume]]`, CI mein ASan/UBSan/fuzz, aur `reserve()` taaki hot path pe vector kabhi na badhe.
> Aur yaad rakho: `-O2` build mein libstdc++ ki assertions apne aap on nahi hoti.

---

## Hands-on

```bash
./build.ps1 09-ARRAYS/examples/06_oob_asan.cpp          # compile hota hai; chalane pe bugs dikhte hain
./build.ps1 san 09-ARRAYS/examples/06_oob_asan.cpp      # STL OOB pakda jaata hai (MinGW); Linux pe ASan
./build.ps1 09-ARRAYS/examples/02_array_decay.cpp        # -Wsizeof-array-argument
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "OOB crash karega, to pata chal jaayega" | Aksar chupchaap — galat data / corruption |
| "`a == b` array ka content compare karta hai" | Addresses — `std::equal` / `std::array` lo |
| "`int a[n]` compile ho gaya to theek hai" | Non-standard VLA — `std::vector` lo |
| "`delete` `new[]` wala array free kar deta hai" | UB — `delete[]`. Isse behtar: manual delete hi nahi |
| "Compiler array bugs pakad leta hai" | Kuch; zyada ke liye ASan / `_GLIBCXX_ASSERTIONS` / tests |
| "`_GLIBCXX_ASSERTIONS` is toolchain pe hamesha on hai" | GCC 16.2 pe sirf `-O0` pe; `-O2` pe khud lagao |

---

## Exercises

1. **5 bugs dhoondho:** is snippet mein har array bug pakdo —
   ```cpp
   int scores[5];
   for (int i = 1; i <= 5; ++i) scores[i] = i * 10;
   int* p = getScores();     // returns &local[0]
   if (scores == p) { }
   int n = readN();  int buf[n];
   ```

2. **OOB write ka asar:** `int a[4] = {}; int canary = 0xABCD; a[4] = 999;` —
   `canary` badla? `-O0` vs `-O2`?

3. **Decay bug:** `int sumBuggy(int a[])` jo count ke liye `sizeof(a)/sizeof(a[0])` use kare.
   10 elements wale array se call karo. Kya return hua? `std::span` se fix karo.

4. **`_GLIBCXX_ASSERTIONS` demo:** `std::vector<int> v = {1,2,3}; v[10] = 0;` — pehle `-O0` pe,
   phir `-O2` pe, phir `-O2 -D_GLIBCXX_ASSERTIONS` pe chalao. Teeno ka message aur behaviour likho.
   <details><summary>Answer</summary>

   GCC 16.2 pe: `-O0` → `Assertion '__n < this->size()' failed` aur program rukta hai; `-O2` →
   koi check nahi, chupchaap OOB (UB); `-O2 -D_GLIBCXX_ASSERTIONS` → phir se assertion. Wajah:
   libstdc++ is macro ko default mein sirf unoptimized builds mein on karta hai.
   </details>

5. **Invalidation:** `std::vector<int> v = {1}; int& r = v[0]; for (int i=0;i<100;
   ++i) v.push_back(i); std::cout << r;` — `-fsanitize=address` (Linux) se chalao ya reason karo.
   `reserve` se fix karo.

6. **Safe rewrite:** C-array se bhara function lo (raw `int*`, manual size, `for i <= n`) aur
   use `std::span` + range-for se dobara likho. Bug ki gunjaish kitni kam hui, compare karo.

---

## Interview questions

1. Off-by-one bug — kaise, aur half-open range se kaise bache?
2. OOB read vs OOB write — kaunsa zyada khatarnak, kyun?
3. Decay `sizeof` bug — mechanism aur fix?
4. `a == b` (do C arrays) kya karta hai? Content compare kaise?
5. `std::vector` reallocation kya invalidate karta hai?
6. Array bugs pakadne ke tools — compile-time aur runtime?
7. `-O2` release build mein `std::vector::operator[]` OOB kyun nahi pakda gaya, jabki `-O0` pe pakda gaya?

---

## Next
→ [`12-exercises.md`](12-exercises.md)
