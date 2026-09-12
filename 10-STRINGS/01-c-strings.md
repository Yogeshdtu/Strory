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
char s[] = "hi";          // {'h', 'i', '\0'}  -- size 3, NOT 2!
```

```
   index:   0    1    2
          ┌────┬────┬────┐
   s:     │'h' │'i' │'\0'│      "hi" ka array 3 bytes ka hai
          └────┴────┴────┘
   'h'=104  'i'=105  '\0'=0
```

String literal `"hi"` ke saath ek **hidden `'\0'`** aata hai. `char s[3]`.

### `strlen` — `'\0'` tak scan

```cpp
#include <cstring>
std::strlen("hello")      // 5  -- '\0' count NAHI hota
std::strlen(s)            // scans byte-by-byte till '\0'  -> O(n)
```

⚠️ `strlen` **har call pe poora scan** karta hai. Loop condition mein `strlen`
→ O(n²). Cache it.

---

## `<cstring>` functions — sab `'\0'` pe rukte hain

```cpp
std::strlen(s)                 // length (till '\0')
std::strcpy(dst, src)          // copy including '\0'  -- ⚠️ NO size check
std::strncpy(dst, src, n)      // copy at most n  -- ⚠️ may not add '\0'
std::strcmp(a, b)              // <0 / 0 / >0  (content compare)
std::strncmp(a, b, n)          // first n chars
std::strcat(dst, src)          // append  -- ⚠️ NO size check
std::strchr(s, 'x')            // pointer to first 'x' (or nullptr)
std::strstr(s, "sub")          // substring search
```

---

## ⚠️ Buffer overflow — C's #1 vulnerability class

```cpp
char small[8];
std::strcpy(small, "this string is way too long");   // ⚠️ writes 28 bytes into 8
```

`strcpy` **destination ka size nahi jaanta** — jitne bytes `src` mein hain
(till `'\0'`) utne likhta hai. Overflow → adjacent stack corrupt → crash, ya
silently wrong, ya **exploitable** (return-address overwrite).

### "Safer" — par abhi bhi manual

```cpp
char dst[8];
std::strncpy(dst, src, sizeof(dst) - 1);
dst[sizeof(dst) - 1] = '\0';                 // ⚠️ strncpy '\0' guarantee nahi karta -- khud lagao
```

`strncpy` bhi trap-heavy: src poora bhar de to `'\0'` nahi lagta; src chhota ho
to baaki `'\0'` se pad karta hai (waste). BSD ka `strlcpy` behtar hai (non-
standard). **Rule: `std::string` / `std::string_view` use karo.**

---

## ⚠️ Comparison — `==` compares pointers

```cpp
const char* a = "apple";
const char* b = "apple";

if (a == b) { }                      // ⚠️ POINTERS compare -- kabhi true (literal merge),
                                     //    kabhi false. Guarantee NAHI.
if (std::strcmp(a, b) == 0) { }      // ✅ content
```

(Folder 05 file 03 se.) `std::string` mein `==` content compare karta hai —
isli ye woh better.

---

## ⚠️ Missing terminator = UB

```cpp
char bad[3] = {'a', 'b', 'c'};       // NO '\0' -- yeh C-string NAHI hai
std::strlen(bad);                     // ⚠️ scans past the array -> OOB read -> UB
std::cout << bad;                     // ⚠️ same
```

Har C-string mein `'\0'` **hona hi chahiye**. `char x[N] = "..."` isse handle
karta hai (jab tak string `N` mein fit ho with terminator).

---

## `char*` vs `char[]` vs string literal

```cpp
char        a[] = "hi";     // MUTABLE copy of the literal, on the stack. a[0] = 'H' OK.
const char* p   = "hi";     // p points to the LITERAL (read-only static). p[0] = 'H' -> UB!
char*       q   = "hi";     // ⚠️ deprecated/ill-formed in C++ -- literal is const
```

String literals are `const char[N]` in **read-only** memory. `const char*` for
pointing at them. `char[]` if you need a mutable buffer.

---

## Andar kya hota hai

- `char s[] = "hello"` → 6 bytes on the stack, `memcpy`'d from the literal.
- `const char* p = "hello"` → `p` (8 bytes) holds the address of the literal in
  `.rodata`.
- `strcpy` → `rep movsb`-style byte copy till `'\0'`. No bounds.
- `strlen` → scan for zero byte (`repne scasb` or SIMD `pcmpeqb`).
- No length stored → every operation re-scans.

> **HFT relevance:** C-strings appear at C-API and wire boundaries (`char[]`
> fields in packed structs, fixed-width symbol fields). HFT code uses
> `std::string_view` over those bytes for parsing (no copy, no scan — length is
> known from the format), and fixed `std::array<char, N>` for outbound fixed-
> width fields. Raw `strcpy`/`strcat` are banned (overflow). `strlen` on hot
> paths is avoided — the length is already known from the protocol.

---

## Hands-on

`examples/01_c_strings.cpp` — layout, `strlen`, overflow risk, `strcmp`, missing
terminator:

```bash
./build.ps1 10-STRINGS/examples/01_c_strings.cpp
```

---

## ⚠️ Traps

### Trap 1 — `sizeof` vs `strlen`
```cpp
char s[] = "hi";
sizeof(s)      // 3 (array, includes '\0')
std::strlen(s) // 2
```

### Trap 2 — `char s[5] = "hello"`
```cpp
char s[5] = "hello";   // ⚠️ 5 chars, no room for '\0'. char s[6] or char s[]
```

### Trap 3 — modifying a string literal
```cpp
char* p = "hi";  p[0] = 'H';   // ⚠️ UB -- literal is read-only. Use char a[] = "hi"
```

### Trap 4 — `strlen` in a loop condition
```cpp
for (std::size_t i = 0; i < std::strlen(s); ++i)   // ⚠️ O(n^2)
const std::size_t n = std::strlen(s);
for (std::size_t i = 0; i < n; ++i)                 // ✅
```

### Trap 5 — `strncpy` and assuming `'\0'`
```cpp
strncpy(dst, src, n);   // ⚠️ dst may not be null-terminated. dst[n-1] = '\0'
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`\"hi\"` is 2 bytes" | 3 — hidden `'\0'` |
| "`strcpy` is safe if dst is big enough" | It never checks — you must |
| "`a == b` compares C-string contents" | Pointers — `strcmp` |
| "`char x[3] = {'a','b','c'}` is a string" | No `'\0'` — not a C-string |
| "`char* p = \"lit\"; p[0]='X'`" | UB — literal is const |

---

## Exercises

1. **Layout:** `char s[] = "abc";` — `sizeof(s)`, `strlen(s)`, each `s[i]` as
   `int`. Where's the `0`?

2. **Overflow (careful):** `char buf[4]; strcpy(buf, "hello");` — compile with
   `-O2` and `-fstack-protector-all`, run. Crash / "stack smashing detected"?

3. **strcmp:** `strcmp("apple", "apple")`, `strcmp("apple", "apply")`,
   `strcmp("app", "apple")` — signs?

4. **Manual strlen:** write `std::size_t myStrlen(const char* s)` (loop till
   `'\0'`). Test on `""`, `"a"`, `"hello"`.

5. **Safe copy:** `bool safeCopy(char* dst, std::size_t dstCap, const char* src)`
   — copy only if it fits (incl. `'\0'`), return success. Test with fitting /
   overflowing src.

6. **Literal mutation:** `const char* p = "hi";` vs `char a[] = "hi";` — try
   `p[0] = 'H'` and `a[0] = 'H'`. Which is UB?

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
