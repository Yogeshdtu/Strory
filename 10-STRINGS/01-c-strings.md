# 01 — C-strings (`char[]` + `'\0'`)

## Prerequisites
- `09-ARRAYS/` (poora — arrays, decay, OOB)
- `03-VARIABLES-DATA-TYPES/07-char-and-ascii.md`
- `05-OPERATORS/03-comparison-operators.md` (string `==` trap)

## Yeh folder kyun
Text handling. Aur `std::string` ke andar **SSO** (Small String Optimization,
file 04) — HFT mein allocation avoidance ka classic example.

Yeh lesson foundation hai: **C-string** = `char` array jiske aakhir mein `'\0'`.
Fast aur allocation-free, par **poori tarah manual** — buffer size, terminator,
comparison, sab aapki zimmedaari.

---

## C-string kya hai

Ek `char` array jismein **pehla `'\0'` (value 0) string ka end mark karta hai.**

```cpp
char s[] = "hi";          // {'h', 'i', '\0'}  -- size 3, 2 NAHI!
```

```
   index:   0    1    2
          ┌────┬────┬────┐
   s:     │'h' │'i' │'\0'│      "hi" ka array 3 bytes ka hai
          └────┴────┴────┘
   'h'=104  'i'=105  '\0'=0
```

String literal `"hi"` ke saath ek **chhupa hua `'\0'`** aata hai. Isliye `char s[3]`.

Analogy: C-string ek train hai jiske aakhir mein ek laal "END" dibba laga hai. Train kitni
lambi hai, yeh kahin likha nahi — ginne ke liye shuru se END dibbe tak chalna padta hai.

### `strlen` — `'\0'` tak scan

```cpp
#include <cstring>
std::strlen("hello")      // 5  -- '\0' count NAHI hota
std::strlen(s)            // '\0' milne tak byte-by-byte scan  -> O(n)
```

⚠️ `strlen` **har call pe poora scan** karta hai. Loop condition mein `strlen` → O(n²).
Length ek baar nikaal ke variable mein rakh lo.

---

## `<cstring>` functions — sab `'\0'` pe rukte hain

```cpp
std::strlen(s)                 // length ('\0' tak)
std::strcpy(dst, src)          // '\0' samet copy  -- ⚠️ size check NAHI
std::strncpy(dst, src, n)      // zyada se zyada n copy  -- ⚠️ '\0' shayad na lage
std::strcmp(a, b)              // <0 / 0 / >0  (content compare)
std::strncmp(a, b, n)          // pehle n chars
std::strcat(dst, src)          // aakhir mein jodo  -- ⚠️ size check NAHI
std::strchr(s, 'x')            // pehle 'x' ka pointer (ya nullptr)
std::strstr(s, "sub")          // substring dhoondho
```

---

## ⚠️ Buffer overflow — C ki #1 vulnerability class

```cpp
char small[8];
std::strcpy(small, "this string is way too long");   // ⚠️ 8 ki jagah 28 bytes likhta hai
```

`strcpy` **destination ka size nahi jaanta** — jitne bytes `src` mein hain (`'\0'` tak) utne
likhta hai. Overflow → bagal ka stack corrupt → crash, ya chupchaap galat, ya **exploitable**
(return address overwrite).

### "Safer" — par abhi bhi manual

```cpp
char dst[8];
std::strncpy(dst, src, sizeof(dst) - 1);
dst[sizeof(dst) - 1] = '\0';                 // ⚠️ strncpy '\0' guarantee nahi karta -- khud lagao
```

`strncpy` bhi traps se bhara hai: src poora buffer bhar de to `'\0'` nahi lagta; src chhota ho
to baaki jagah `'\0'` se bharta hai (bekaar kaam). BSD ka `strlcpy` behtar hai (par standard
nahi). **Rule: `std::string` / `std::string_view` use karo.**

---

## ⚠️ Comparison — `==` pointers compare karta hai

```cpp
const char* a = "apple";
const char* b = "apple";

if (a == b) { }                      // ⚠️ POINTERS compare -- kabhi true (literal merge),
                                     //    kabhi false. Guarantee NAHI.
if (std::strcmp(a, b) == 0) { }      // ✅ content
```

(Folder 05 file 03 se.) `std::string` mein `==` content compare karta hai — isliye woh behtar hai.

---

## ⚠️ Terminator gayab = UB

```cpp
char bad[3] = {'a', 'b', 'c'};       // '\0' NAHI -- yeh C-string NAHI hai
std::strlen(bad);                     // ⚠️ array ke bahar scan -> OOB read -> UB
std::cout << bad;                     // ⚠️ wahi baat
```

Har C-string mein `'\0'` **hona hi chahiye**. `char x[N] = "..."` yeh apne aap sambhaal leta hai
(jab tak string terminator samet `N` mein fit ho).

---

## `char*` vs `char[]` vs string literal

```cpp
char        a[] = "hi";     // literal ki BADALNE-LAYAK copy, stack pe. a[0] = 'H' theek hai.
const char* p   = "hi";     // p seedha LITERAL pe point karta hai (read-only static). p[0] = 'H' -> compile error (const)
char*       q   = "hi";     // ⚠️ C++11 se ill-formed -- literal const hai. q[0] = 'H' -> UB (compile ho jaaye to bhi)
```

String literals ka type `const char[N]` hai aur woh **read-only** memory mein rehte hain. Unpe
point karne ke liye `const char*`; badalne layak buffer chahiye to `char[]`.

⚠️ Dhyaan: `char* q = "hi";` standard ke hisaab se galat hai, par **GCC 16.2 default pe sirf
warning deta hai aur compile kar deta hai**: `ISO C++ forbids converting a string constant to
'char*' [-Wwrite-strings]`. Error chahiye to `-pedantic-errors` lagao (chala ke dekha). "Compile
ho gaya" ka matlab "sahi hai" nahi.

---

## Andar kya hota hai

- `char s[] = "hello"` → stack pe 6 bytes, literal se `memcpy` karke.
- `const char* p = "hello"` → `p` (8 bytes) mein `.rodata` mein pade literal ka address.
- `strcpy` → `'\0'` tak byte-by-byte copy (`rep movsb` jaisa). Koi bounds nahi.
- `strlen` → zero byte ki talaash (`repne scasb`, ya SIMD `pcmpeqb` se 16/32 bytes ek saath).
- Length kahin store nahi hoti → har operation dobara scan karta hai.

> **HFT relevance:** C-strings C-API aur wire ki boundary pe dikhte hain (packed structs mein
> `char[]` fields, fixed-width symbol fields). HFT code parsing ke liye un bytes pe
> `std::string_view` rakhta hai (na copy, na scan — length format se hi pata hai), aur bahar
> jaane wale fixed-width fields ke liye `std::array<char, N>`. Raw `strcpy`/`strcat` ban hain
> (overflow). Hot path pe `strlen` se bachte hain — length protocol se pehle hi pata hoti hai.

---

## Hands-on

`examples/01_c_strings.cpp` — layout, `strlen`, overflow ka khatra, `strcmp`, gayab terminator:

```bash
./build.ps1 10-STRINGS/examples/01_c_strings.cpp
```

---

## ⚠️ Traps

### Trap 1 — `sizeof` vs `strlen`
```cpp
char s[] = "hi";
sizeof(s)      // 3 (array, '\0' samet)
std::strlen(s) // 2
```

### Trap 2 — `char s[5] = "hello"`
```cpp
char s[5] = "hello";   // ❌ C++ mein compile error: 'initializer-string for char [5] is too long'
```
`"hello"` ko 6 bytes chahiye. (C language mein yahi line chupchaap bina `'\0'` ke compile hoti hai
— C code padhte waqt yaad rakhna.) `char s[6]` ya `char s[]` likho.

### Trap 3 — string literal ko badalna
```cpp
char* p = "hi";  p[0] = 'H';   // ⚠️ UB -- literal read-only hai (aur GCC sirf warning deta hai). char a[] = "hi" lo
```

### Trap 4 — loop condition mein `strlen`
```cpp
for (std::size_t i = 0; i < std::strlen(s); ++i)   // ⚠️ O(n^2)
const std::size_t n = std::strlen(s);
for (std::size_t i = 0; i < n; ++i)                 // ✅
```

### Trap 5 — `strncpy` ke baad `'\0'` maan lena
```cpp
strncpy(dst, src, n);   // ⚠️ dst shayad null-terminated na ho. dst[n-1] = '\0' khud lagao
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`\"hi\"` 2 bytes ka hai" | 3 — chhupa hua `'\0'` |
| "dst bada ho to `strcpy` safe hai" | Woh kabhi check nahi karta — aapko karna padega |
| "`a == b` C-string ka content compare karta hai" | Pointers — `strcmp` lo |
| "`char x[3] = {'a','b','c'}` ek string hai" | `'\0'` nahi — C-string nahi hai |
| "`char* p = \"lit\";` compile ho gaya to theek hai" | C++11 se ill-formed; GCC sirf warning deta hai. `p[0]='X'` UB |

---

## Exercises

1. **Layout:** `char s[] = "abc";` — `sizeof(s)`, `strlen(s)`, aur har `s[i]` ko `int` ki tarah
   print karo. `0` kahan hai?

2. **Overflow (dhyaan se):** `char buf[4]; strcpy(buf, "hello");` — `-O2` aur
   `-fstack-protector-all` se compile karke chalao. Crash / "stack smashing detected" aaya?

3. **strcmp:** `strcmp("apple", "apple")`, `strcmp("apple", "apply")`, `strcmp("app", "apple")` —
   har ek ka sign?

4. **Apna strlen:** `std::size_t myStrlen(const char* s)` likho (`'\0'` tak loop). `""`, `"a"`,
   `"hello"` pe test karo.

5. **Safe copy:** `bool safeCopy(char* dst, std::size_t dstCap, const char* src)` — sirf tab copy
   karo jab (`'\0'` samet) fit ho, success return karo. Fit hone wale aur overflow wale `src` se
   test karo.

6. **Literal mutation:** `const char* p = "hi";` vs `char a[] = "hi";` — `p[0] = 'H'` aur
   `a[0] = 'H'` try karo. Kaunsa UB hai? Aur `char* q = "hi";` pe GCC kya kehta hai?
   <details><summary>Answer</summary>

   `const char* p` pe `p[0] = 'H'` compile hi nahi hoga (const). `a[0] = 'H'` bilkul theek — `a`
   apni copy hai. `char* q = "hi"; q[0] = 'H';` — GCC 16.2 sirf `-Wwrite-strings` warning deta hai,
   compile ho jaata hai, aur chalane pe **UB** (read-only memory mein likhna — aksar crash).
   </details>

---

## Interview questions

1. C-string kya hai? `"hi"` ka size?
2. `strlen` ki complexity? Loop mein kyun problem?
3. `strcpy` ka danger? Safer alternatives ka trade-off?
4. `char*` (to literal) vs `char[]` — mutability?
5. `==` do `const char*` pe — kya compare hota hai?
6. Missing `'\0'` — kya hota hai?

---

## Next
→ [`02-std-string-basics.md`](02-std-string-basics.md)
