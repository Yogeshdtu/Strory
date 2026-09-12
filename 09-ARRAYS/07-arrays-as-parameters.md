# 07 — Arrays as function parameters

## Prerequisites
- [`05-array-decay.md`](05-array-decay.md)
- `08-FUNCTIONS/03-parameters-and-arguments.md`

## Yeh topic abhi kyun
File 05 mein dekha ki array function ko pass karte hi pointer ban jaata hai
(decay) — size lost. Yeh lesson **practical decision** deta hai: "mujhe ek
function ko array dena hai — kaunsa parameter type?"

---

## Options — ek nazar mein

| Parameter type | Kya accept karta hai | Size milta hai? | Kab use karo |
|---|---|---|---|
| `int* arr, std::size_t n` | koi bhi contiguous data | ✅ (aap khud pass karte ho) | C API ki boundary pe |
| `std::span<int>` (C++20) | C array, `std::array`, `std::vector` | ✅ apne aap | **modern default** |
| `std::span<const int>` | wahi sab, sirf padhne ke liye | ✅ apne aap | read-only ka default |
| `const int (&arr)[N]` (template) | sirf compile-time size wala C array | ✅ apne aap (`N`) | fixed-size C array, `std::array` nahi |
| `const std::array<int, N>&` | sirf wahi exact `std::array` | ✅ (type mein hai) | jab caller `std::array` hi use karta ho |
| `int arr[]` / `int arr[10]` | **= `int*`** — purana tareeka | ❌ | bacho (dhokha deta hai) |

**Default: padhne ke liye `std::span<const T>`, badalne ke liye `std::span<T>`.**

---

## 1. `std::span` — modern jawab

```cpp
#include <span>

long long sum(std::span<const int> data) {
    long long s = 0;
    for (int x : data) s += x;
    return s;
}

int cArr[] = {1, 2, 3};
std::array<int, 4> stdArr = {10, 20, 30, 40};
std::vector<int> vec = {100, 200, 300};

sum(cArr);      // chalta hai
sum(stdArr);    // chalta hai
sum(vec);       // chalta hai
```

Ek hi function, teeno tarah ke containers. Kaise? `std::span` sirf do cheezein rakhta hai:
**pointer** (data kahan shuru hota hai) aur **length** (kitne elements). Woh data ka maalik
nahi — bas usko "dekhne ki khidki" hai.

- Size: 16 bytes (`ptr` + `len`) — array chahe kitna bhi bada ho, copy sirf yeh 16 bytes
  hoti hai.
- `.size()`, `.subspan()`, `.first()`, `.last()`, range-for, `[]`, aur `.at()` (C++26).
- `std::span<T>` → badal sakte ho; `std::span<const T>` → sirf padh sakte ho.
- ⚠️ Non-owning hai — caller ka data function call ke dauraan zinda rehna chahiye. Parameter
  ke liye yeh aam taur pe apne aap sach hota hai; khatra tab hai jab span ko **store** karo
  (Trap 2).

## 2. Pointer + size (C style)

```cpp
long long sum(const int* arr, std::size_t n) {
    long long s = 0;
    for (std::size_t i = 0; i < n; ++i) s += arr[i];
    return s;
}
sum(data, std::size(data));
```

C code ke saath kaam karne ke liye theek hai. Khatra: caller galat `n` de sakta hai — span mein
size data ke saath hi aati hai, isliye yeh galti hoti hi nahi.

## 3. Reference-to-array (template)

```cpp
template <std::size_t N>
long long sum(const int (&arr)[N]) {          // N compiler khud nikaalta hai -- decay nahi hota
    long long s = 0;
    for (int x : arr) s += x;
    return s;
}
sum(data);   // size dene ki zaroorat nahi
```

- Decay nahi hota → `N` pata hai, range-for chalta hai, `sizeof` sahi aata hai.
- ⚠️ Sirf compile-time size wale C arrays. `std::array`, `std::vector`, ya decay ho chuka
  pointer — match nahi karenge. Aur har alag `N` ke liye function ki **alag copy** banti hai
  (bahut saare sizes ho to code bloat).

## 4. Badalna — `std::span<T>` (non-const)

```cpp
void doubleAll(std::span<int> data) {
    for (int& x : data) x *= 2;
}
doubleAll(vec);
```

---

## Output arrays — caller ka buffer bharna

```cpp
// Caller ka buffer + uski capacity; kitne characters likhe, woh return
std::size_t formatHex(std::uint32_t value, std::span<char> out) {
    // out.size() se bounds check kar sakte ho
    ...
    return written;
}

char buf[16];
std::size_t n = formatHex(0xDEADBEEF, buf);
```

`std::span<char>` mein buffer aur uski capacity **ek saath** aati hai → "buffer ke bahar likh
diya" wali galti rokna aasaan.

---

## Multi-dimensional (file 06)

```cpp
void f(int (*m)[COLS], std::size_t rows);      // COLS compile-time
void f(std::span<int> flat, std::size_t rows, std::size_t cols);   // flat + dims
void f(std::mdspan<int, std::dextents<std::size_t, 2>> m);          // C++23 -- sabse saaf
```

---

## Andar kya hota hai

- **Span parameter kaise pahunchta hai — platform pe depend karta hai.** Yeh GCC 16.2
  (Windows) pe `-O2` assembly dekh ke check kiya:
  ```
  sum_span(std::span<int const>):        sum_ptr(int const*, unsigned long long):
      mov rdx, QWORD PTR 8[rcx]   ; len       ; ptr pehle se rcx mein
      mov rax, QWORD PTR [rcx]    ; ptr       ; len pehle se rdx mein
  ```
  **Windows x64 ABI** pe 16-byte struct ek **pointer ke through** jaata hai — function ko
  shuru mein do memory loads karne padte hain. **Linux (SysV ABI)** pe wahi span do registers
  mein aata hai. (Calling conventions: folder 34 file 07.)
- Kya yeh fark matter karta hai? Naapa: 10M ints ka sum, dono functions `noinline`, `-O2`,
  best of 7 — span 7.55–8.06 ms, pointer+len 7.69–7.92 ms (3 runs). **Koi fark nahi** — do
  extra loads 10M-element loop ke saamne kuch nahi. Fark sirf tab dikh sakta hai jab ek bahut
  chhota function **karodon baar** call ho aur inline na ho. Aam taur pe `-O2` chhote functions
  ko inline kar deta hai, aur tab parameter passing hi gayab ho jaata hai.
- Reference-to-array template → har `N` ek alag function; `N` compile-time constant hai, isliye
  chhote `N` pe compiler loop poora khol (unroll) sakta hai.
- Pointer + size → machine code span jaisa, bas size galat hone ki safety nahi.

> **HFT relevance:** `std::span<const T>` / `std::span<T>` HFT mein array parameter ka standard
> type hai — size hamesha sahi, aur stack arrays, `std::array` pools, `std::vector` sab ke saath
> chalta hai. Wire decoders `std::span<const std::byte>` (buffer + length) lete hain, taaki
> buffer ke bahar padhne se pehle ek `.size()` check ho — chupchaap overrun nahi. C-style
> `(ptr, len)` sirf OS/library ki boundary pe. Aur "zero overhead" ka claim platform ke saath
> naapo: Windows x64 pe span pointer se aata hai — hot, non-inlined, bahut chhote functions mein
> woh dekhne layak ho sakta hai.

---

## Hands-on

`examples/02_array_decay.cpp` (size param + reference-to-array),
`examples/05_span_demo.cpp` (ek function, saare containers):

```bash
./build.ps1 09-ARRAYS/examples/02_array_decay.cpp
./build.ps1 09-ARRAYS/examples/05_span_demo.cpp
```

---

## ⚠️ Traps

### Trap 1 — `void f(int a[])` likh ke size ki ummeed
```cpp
void f(int a[]) { /* size? nahi hai */ }
```

### Trap 2 — `std::span` apne data se zyada jee gaya
```cpp
std::span<int> s;
{ std::vector<int> v = {1,2,3}; s = v; }   // ⚠️ v khatam -> s dangling
```

### Trap 3 — reference-to-array ko `std::array` dena
```cpp
template <std::size_t N> void f(int (&a)[N]);
std::array<int, 5> a;  f(a);    // ❌ match nahi -- std::array int[N] nahi hai. span use karo
```

### Trap 4 — "safe rehne ke liye" `std::vector` by value
```cpp
void f(std::vector<int> v);     // ⚠️ poori copy. std::span<const int> ya const& lo
```

### Trap 5 — output buffer bina capacity ke
```cpp
void format(std::uint32_t v, char* out);   // ⚠️ overrun ka khatra. std::span<char> out lo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int a[]` parameter array leta hai" | Pointer leta hai — size kho jaati hai |
| "`std::span` hamesha do registers mein jaata hai" | Linux (SysV) pe haan; Windows x64 pe pointer ke through — naapne pe loop mein fark nahi mila |
| "Reference-to-array `std::array` pe chalega" | Nahi — sirf C arrays. Dono ke liye `std::span` |
| "Output buffer ke saath capacity dena optional hai" | `std::span<char>` do — overrun check aasaan |
| "`std::vector` by value safe hai" | Poori copy — `const&` / `span` lo |

---

## Exercises

1. **4 sum signatures:** `sum(const int*, size_t)`, `sum(span<const int>)`,
   `template<size_t N> sum(const int(&)[N])`, `sum(const array<int,10>&)` — sab
   likho. Kaunsa `std::vector` pe chalega? Kaunsa C array pe? `std::array` pe?

2. **`std::span` se badlo:** `void addOne(std::span<int>)` — C array, `std::array`,
   `std::vector` teenon pe chalao. Original data badla?

3. **Output buffer:** `std::size_t toBinary(std::uint8_t v, std::span<char> out)`
   — `v` ka binary string `out` mein likho, bits ki ginti return karo. `out` chhota ho to?

4. **Dangling span:** ek function likho jo local array ka `std::span<int>` return kare.
   `-Wall` warning deta hai? Chalao (UB).

5. **2D param:** `int (*m)[4]` parameter wala `rowSum` likho, `int grid[3][4]` pe use karo.
   Phir flat `std::span<int>` + `cols` parameter wala version.

6. **Bench + assembly:** `sum(span<const int>)` vs `sum(const int*, size_t)` — 10M ints,
   `-O2`, dono pe `[[gnu::noinline]]`. Time lo, phir `-S` se function ki pehli lines dekho.
   <details><summary>Answer</summary>

   Is box (GCC 16.2, Windows) pe time mein koi fark nahi mila (7.55–8.06 ms vs 7.69–7.92 ms).
   Assembly mein span version shuru mein `rcx` se ptr aur len **load** karta hai (Windows x64
   ABI, struct pointer se aata hai); pointer version ko dono seedhe `rcx`/`rdx` mein milte hain.
   Linux pe span bhi registers mein aata. Sabak: ABI ka fark asli hai, par is loop mein
   naapne layak nahi.
   </details>

---

## Interview questions

1. Function ko array pass karne ke saare tareeke — trade-offs?
2. `std::span` kya hai (size, cost)? C-style `(ptr, len)` se kya behtar?
3. Reference-to-array parameter — faayda aur limitation?
4. `std::span<T>` vs `std::span<const T>` — kab kaunsa?
5. Output buffer parameter — safe design?
6. `std::span` kis situation mein dangling ho sakta hai?
7. `std::span` parameter Windows x64 aur Linux pe alag tarah kyun pass hota hai? Kab matter karta hai?

---

## Next
→ [`08-std-array.md`](08-std-array.md)
