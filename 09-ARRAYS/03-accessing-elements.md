# 03 — Elements access — `arr[i]` actually kya karta hai

## Prerequisites
- [`01-what-is-an-array.md`](01-what-is-an-array.md), [`02-declaring-and-initializing.md`](02-declaring-and-initializing.md)
- `05-OPERATORS/01-arithmetic-operators.md`

## Yeh topic abhi kyun
`arr[i]` sirf syntax nahi hai — woh **pointer arithmetic ka shortcut** hai:
`*(arr + i)`. Yeh samajhna array decay (file 05), pointers (folder 12), aur "OOB
kyun UB hai" ka core hai.

---

## `arr[i]` == `*(arr + i)`

```cpp
int arr[5] = {10, 20, 30, 40, 50};

arr[2]           //  ==  *(arr + 2)   ==  30
*(arr + 2)       //  same
2[arr]           //  ⚠️ ALSO 30 (== *(2 + arr)) -- legal par kabhi mat likho
```

`arr` (array name) apne pehle element ke **address** mein decay hota hai. `arr +
i` = "us address se `i` elements aage" (`i * sizeof(int)` bytes). `*` = "wahan ki
value."

```
   arr  ─────► 1000    arr[0] = 10
                1004    arr[1] = 20
   arr+2 ─────► 1008    arr[2] = 30   ← *(arr + 2)
                1012    arr[3] = 40
                1016    arr[4] = 50
```

**Isi liye zero-based:** `arr[0]` = `*(arr + 0)` = pehla element.

---

## Read aur write — `arr[i]` ek lvalue hai

```cpp
int x = arr[2];        // read
arr[2] = 99;           // write -- arr[i] assign ho sakta hai (lvalue)
arr[i]++;              // read-modify-write
int& ref = arr[2];     // reference to element
```

---

## Index ka type

```cpp
arr[i]                 // i koi integral type ho sakta hai (int, size_t, char...)
arr[2]                 // literal
arr['A']               // ⚠️ legal -- 'A' = 65 -> arr[65]. Bug ka source
arr[-1]                // ⚠️ compiles -- *(arr - 1) -> OOB (UB)
```

Container indexing ke liye **`std::size_t`** use karo (`std::vector::operator[]`
`size_type` leta hai — signed `int` se `-Wsign-conversion`).

⚠️ `arr[i - j]` jaisa — agar `i - j` negative ho jaaye → OOB. Aur agar
`i`, `j` unsigned hain → wrap → huge index → OOB (folder 06 file 07, folder 07
file 07).

---

## 🔑 OUT OF BOUNDS = UNDEFINED BEHAVIOUR

```cpp
int arr[5];
arr[5]  = 1;           // ⚠️ OOB -- valid 0..4
arr[-1] = 1;           // ⚠️ OOB
arr[100] = 1;          // ⚠️ OOB
```

C++ **bounds check nahi karta** — speed ke liye. `arr[i]` = ek `mov` instruction,
koi `if (i < size)` nahi. Consequence:

| | OOB READ | OOB WRITE |
|---|---|---|
| Kya hota | kisi aur variable ki value / garbage | kisi aur cheez ki memory corrupt |
| Kab dikhta | turant ya kabhi nahi (data galat) | door ka bug, dhoondhna mushkil |
| "Chal gaya" | UB hai — "kaam kiya" ka matlab "sahi" NAHI | same |

### Pakadne ke tareeke
- **`std::array::at(i)` / `std::vector::at(i)`** — bounds-checked, `throw
  std::out_of_range`. Untrusted index pe use karo.
- **`-D_GLIBCXX_ASSERTIONS`** — libstdc++ ke `[]` pe assert (STL containers only;
  is course ke MinGW build pe default ON). Raw C arrays pe **nahi**.
- **AddressSanitizer** (`-fsanitize=address`) — best, raw arrays bhi. Linux/Clang.
- **`-O2 -Warray-bounds`** — compile-time, constant/provable OOB.

`examples/06_oob_asan.cpp` — teenon raw-array OOB + ek `std::vector[]` OOB.

---

## `.at()` vs `[]` — trade-off

```cpp
std::array<int, 5> a = {10, 20, 30, 40, 50};

a[10]              // ⚠️ UB (no check) -- fast
a.at(10)           // throws std::out_of_range -- safe, thoda cost (ek compare + branch)
```

Rule:
- **Hot loop, index tumne khud generate kiya (`0..n-1`)** → `[]`
- **Untrusted / external index** (user input, network, config) → `.at()` ya
  manual check

---

## Andar kya hota hai

```cpp
int x = arr[i];
```

```asm
        movsxd  rax, dword ptr [i]      ; i (32-bit) -> 64-bit
        mov     eax, dword ptr [rbp + rax*4 - N]   ; arr + i*4 se load
```

Ek scaled load. `i*4` — CPU ka addressing mode (`base + index*scale`) yeh ek
instruction mein karta hai. Isi liye `arr[i]` free hai.

`.at(i)` extra: `cmp rax, size ; jae throw_path`.

> **HFT relevance:** Hot path mein `[]` (indices code se generate hote hain,
> bounded). Wire-format parsers mein **`.at()` ya explicit bounds check** — ek
> malicious/corrupt packet ka index buffer se bahar le ja sakta hai (RCE /
> crash). Debug/CI builds mein `_GLIBCXX_ASSERTIONS` + ASan; release mein bounded
> indices + `[[assume]]` (folder 08 lesson 12). "OOB = UB = disaster" is a core
> HFT security concern (folder 30, 45).

---

## Hands-on

```bash
./build.ps1 09-ARRAYS/examples/01_array_basics.cpp
./build.ps1 san 09-ARRAYS/examples/06_oob_asan.cpp     # STL OOB caught; raw silent on MinGW
```

---

## ⚠️ Traps

### Trap 1 — `arr[size]`
```cpp
for (int i = 0; i <= n; ++i) sum += arr[i];   // ⚠️ i == n -> OOB. `i < n`
```

### Trap 2 — negative / unsigned-wrapped index
```cpp
std::size_t i = 0;
arr[i - 1];            // ⚠️ 0 - 1 -> SIZE_MAX -> OOB
```

### Trap 3 — `char` / `bool` as index accidentally
```cpp
arr[getChar()];        // ⚠️ 'A' -> 65. Shayad tumne enum-to-int socha tha
```

### Trap 4 — `.at()` hot loop mein bina zaroorat
```cpp
for (std::size_t i = 0; i < n; ++i) sum += v.at(i);   // ⚠️ har iteration bounds check
for (std::size_t i = 0; i < n; ++i) sum += v[i];      // ✅ i bounded hai
```

### Trap 5 — `[]` untrusted index pe
```cpp
int field = buf[header.offset];   // ⚠️ offset network se aaya -- .at() ya check
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`arr[i]` bas syntax" | `*(arr + i)` — pointer arithmetic |
| "OOB access se crash hota hai" | Kabhi crash, kabhi garbage, kabhi "kaam" — UB |
| "C++ index check karta hai" | `[]` nahi. `.at()` karta hai (throws) |
| "`.at()` free hai" | Ek compare + branch per access |
| "Negative index illegal hai (compile error)" | Compiles — `*(arr - 1)` OOB |

---

## Exercises

1. **Equivalence:** `int a[5] = {10,20,30,40,50};` — `a[3]`, `*(a + 3)`, `3[a]`
   — teenon print. Same?

2. **OOB read:** `int a[3] = {1,2,3}; std::cout << a[5];` — 3 baar chalao. Value
   kya? Same har baar? Ab `-O2 -Warray-bounds` se compile — warning?

3. **`.at()` throw:** `std::array<int,5> a{}; try { a.at(10); } catch (const
   std::out_of_range& e) { std::cout << e.what(); }`

4. **Unsigned wrap index:** `std::size_t i = 0; std::cout << a[i - 1];` — ASan/
   `_GLIBCXX_ASSERTIONS` (vector version) se pakdo.

5. **`_GLIBCXX_ASSERTIONS`:** `std::vector<int> v = {1,2,3}; v[10] = 0;` —
   `./build.ps1 san <file>` se run. Kya bola?

6. **Hot vs untrusted:** ek function `int lookup(const std::array<int,256>& table,
   int idx)` — kab `[]`, kab `.at()`? Dono versions likho.

---

## Interview questions

1. `arr[i]` andar se kya hai? `2[arr]` kyun legal hai?
2. OOB access — kya hota hai (read vs write)? "Chal gaya" ka matlab?
3. `[]` aur `.at()` mein fark? Kab kaunsa?
4. C++ arrays bounds-check kyun nahi karte?
5. Negative ya unsigned-wrapped index — kya hota hai?
6. OOB detect karne ke 3 tools?

---

## Next
→ [`04-arrays-and-loops.md`](04-arrays-and-loops.md)
