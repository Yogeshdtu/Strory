# 05 — Pointer arithmetic

## Prerequisites
- [`03-dereferencing.md`](03-dereferencing.md)
- `09-ARRAYS/03-accessing-elements.md` (`arr[i] == *(arr+i)`), `09-ARRAYS/05-array-decay.md`

## Yeh topic abhi kyun
`p + 1` ek pointer ke liye "next element" hai — `sizeof(*p)` bytes aage, `1` byte
nahi. Yeh `arr[i] == *(arr + i)` ka mechanism hai, iterators ka base, aur
out-of-range arithmetic ek subtle UB hai.

---

## `p + n` `sizeof(*p)` ke hisaab se scale hota hai

```cpp
int arr[5] = {10, 20, 30, 40, 50};
int* p = arr;                  // -> &arr[0]

p + 1        // address + sizeof(int) = address + 4  -> &arr[1]
p + 3        // address + 12           -> &arr[3]
*(p + 3)     // 40
```

```
   p     -> 0x1000   arr[0]
   p + 1 -> 0x1004   arr[1]      (har +1 = +4 bytes, kyunki int hai)
   p + 2 -> 0x1008   arr[2]
```

`double* q; q + 1` → +8 bytes. `char* c; c + 1` → +1 byte. Yaad rakho: **scale
type se aata hai**, number se nahi.

---

## Ek identity: `arr[i]` ≡ `*(arr + i)`

```cpp
arr[3]        // == *(arr + 3)
*(arr + 3)    // == arr[3]
3[arr]        // == *(3 + arr) -- yeh bhi legal hai (par aisa likhna mat)
```

Subscript ki **definition hi** pointer arithmetic + dereference hai. Isiliye
`3[arr]` bhi compile ho jaata hai — mazedaar hai, par code mein kabhi mat likhna.
(Folder 09 file 03.)

---

## Kaunse operations allowed hain

```cpp
int* p = arr;

p + n         // pointer  (n elements aage)
p - n         // pointer  (n elements peeche)
p += n; p -= n
++p; --p; p++; p--
p2 - p1       // ptrdiff_t -- beech mein kitne ELEMENTS hain (signed)
p1 < p2       // ✅ SIRF tab jab dono ek hi array mein point karein (ya one-past-end)
p1 == p2      // ✅ hamesha
*p; p[n]      // deref
```

Yeh **allowed nahi**: `p1 + p2` (address + address ka koi matlab nahi), `p * 2`,
`p / 2`.

```cpp
int* start = &arr[1];
int* end   = &arr[4];
std::ptrdiff_t count = end - start;    // 3  (elements, bytes nahi)
```

---

## Pointer walk — iterator ka pattern

```cpp
for (int* it = arr; it != arr + 5; ++it)   // arr + 5 = "one past the end"
    std::cout << *it << " ";
```

- `arr + 5` (one-past-the-end) ko **banana aur compare karna valid hai** — par
  usse **deref karna nahi**.
- STL iterators isi idea ka general roop hain (`begin()`, `end()`, `++`, `*`).
  Matlab aap yahan iterators seekh hi rahe ho, bas naam alag hai.

---

## ⚠️ Valid range — ek array ke bahar arithmetic UB hai

```cpp
int arr[5];

arr, arr+1, ..., arr+5     // ✅ valid (arr+5 = one-past-end)
arr - 1                    // ⚠️ UB -- array se pehle
arr + 6                    // ⚠️ UB -- one-past-end se bhi aage
*(arr + 5)                 // ⚠️ UB -- one-past-end ka deref
&arr[5]                    // ⚠️ technically arr+5 banata hai (value theek), par arr[5] "padhta" hai -> dhyaan se
```

Yahan sabse important baat: out-of-range pointer ko sirf **banana** bhi UB hai —
deref karo ya na karo. Compiler maan ke chalta hai ki saara pointer arithmetic
in-bounds hai, aur usi hisaab se optimize karta hai.

Do **alag** arrays ke pointers: `p1 < p2` unspecified hai; `p1 - p2` UB hai;
sirf `==`/`!=` defined hai.

---

## `void*` — koi arithmetic nahi

```cpp
void* v = arr;
v + 1;        // ❌ ERROR (GCC extension ke taur pe allow karta hai, void ko 1 byte maan ke -- bharosa mat karo)
```

`void*` ka koi element type nahi → koi scale nahi → koi arithmetic nahi.
Byte-by-byte chalna ho to `char*` mein cast karo (file 10).

---

## `char*` se byte-level stepping

```cpp
int arr[3] = {1, 2, 3};
auto* bytes = reinterpret_cast<const unsigned char*>(arr);
bytes[0], bytes[1], ...     // arr ke alag-alag bytes

// kisi bhi byte offset pe jao (jaise packed buffer ke andar)
const std::byte* field = buf + offset;   // std::byte* bhi byte-by-byte chalta hai
```

Parsers mein yahi use hota hai — kisi known byte offset pe pada field nikalne ke
liye (folder 11 file 06).

---

## Andar kya hota hai

- `p + n` → `p + n * sizeof(*p)` — multiply compiler fold kar deta hai (aksar
  addressing mode `[base + index*scale]` mein, jahan scale ∈ {1,2,4,8}).
- `arr[i]` → `mov reg, [arr + i*4]` — ek scaled load, O(1).
- `p2 - p1` → `(addr2 - addr1) / sizeof(*p)` — ek subtract + shift.
- "in-bounds rehna zaroori hai" wala rule bekaar ka niyam nahi hai — usi ki wajah
  se optimizer loops ke baare mein reason kar paata hai (jaise prove karna ki
  pointer badhane wala loop khatam hoga, aur phir usse vectorize karna).

> **HFT relevance:** Pointer arithmetic hi woh tareeka hai jisse aap preallocated
> arena, ring buffer (`base + (idx & mask)`), flat 2D book (`base + i*stride + j`),
> ya wire message ke raw bytes (`buf + fieldOffset`) pe chalte ho. Cost ke hisaab
> se yeh free hai (ek `lea` ya scaled address). Par in-bounds wala rule yahan
> serious hai: parser ko `buf + offset` banane se **pehle** offset ko buffer
> length se check karna hoga (`std::span<const std::byte>` ise ek `.size()` check
> bana deta hai). Decoder mein out-of-bounds pointer math sirf UB nahi — woh ek
> exploit ka darwaza hai. Folder 38.

---

## Hands-on

`examples/02_pointer_arithmetic.cpp` — `p+n` scaling, `arr[i] == *(arr+i)`,
pointer walk, subtraction, `char*` byte view:

```bash
./build.ps1 12-POINTERS/examples/02_pointer_arithmetic.cpp
```

---

## ⚠️ Traps

### Trap 1 — samajhna ki `p + 1` ek byte jodta hai
```cpp
int* p = arr;  (char*)(p + 1) - (char*)p;   // 4, na ki 1
```

### Trap 2 — one-past-the-end ko deref kar dena
```cpp
for (int* it = arr; it <= arr + 5; ++it) *it;   // ⚠️ aakhri iteration arr+5 deref karti hai
```

### Trap 3 — do alag arrays ke beech arithmetic
```cpp
int a[3], b[3];
&b[0] - &a[0];   // ⚠️ UB. Alag arrays hain
```

### Trap 4 — out-of-range pointer sirf bana dena
```cpp
int* p = arr - 1;   // ⚠️ deref na karne pe bhi UB
int* p = arr + 100; // ⚠️ UB
```

### Trap 5 — `void*` pe arithmetic
```cpp
void* v = buf;  v + 4;   // ⚠️ portable nahi. ((char*)v) + 4 likho
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`p + 1` = address + 1 byte" | `+ sizeof(*p)` bytes |
| "`p2 - p1` bytes deta hai" | Elements ki ginti deta hai (`ptrdiff_t`) |
| "One-past-the-end pointer invalid hai" | Banana/compare karna valid, deref karna invalid |
| "OOB pointer theek hai agar deref na karun" | Usse banana hi UB hai |
| "`void*` pe `+` chalta hai" | Element size hi nahi — `char*` mein cast karo |

---

## Exercises

1. **Scaling:** `int a[4]; double d[4]; char c[4];` — har ek ke liye
   `(char*)(ptr+1) - (char*)ptr` print karo.

2. **Identity:** `int a[6] = {...}` ke liye `i = 0..5` pe `a[i]`, `*(a+i)`, aur
   `i[a]` print karo.

3. **Walk:** ek array ka sum pointer loop se nikalo (`for (int* it = a; it != a +
   n; ++it)`). Phir `std::accumulate(a, a + n, 0)` se.

4. **Subtraction:** `int a[10]; int* p = &a[7]; int* q = &a[2];` — `p - q`?
   `q - p`? Result ka type kya hai?

5. **Byte view:** `int a[2] = {1, 256};` ke 8 bytes
   `reinterpret_cast<unsigned char*>` se print karo. Layout samjhao (little-endian).

6. **UB pakdo:** inme se kaunse UB hain?
   `arr + 5`, `*(arr + 5)`, `arr - 1`, `&arr[5]`, `arr + 5 == arr + 5`,
   `(arr + 2) < (arr + 4)`.

---

## Interview questions

1. `p + 1` — kitne bytes aage? Kyun?
2. `arr[i]` andar se kya hai?
3. `p2 - p1` kya deta hai — bytes ya elements? Type?
4. One-past-the-end pointer — form/compare/deref mein se kya defined?
5. Do alag arrays ke pointers — kya operations defined?
6. `void*` pe arithmetic kyun nahi?

---

## Next
→ [`06-pointers-and-arrays.md`](06-pointers-and-arrays.md)
