# 09 — Double pointers (`int**`)

## Prerequisites
- [`03-dereferencing.md`](03-dereferencing.md), [`06-pointers-and-arrays.md`](06-pointers-and-arrays.md)

## Yeh topic abhi kyun
`int**` = "pointer to a pointer to int." Kab chahiye: (1) ek function ko *pointer
ko modify* karne dena, (2) C-style dynamic 2D arrays, (3) `argv`. Modern C++ mein
kam, par C APIs aur legacy code mein bahut.

---

## `int**` — ek aur level ki indirection

```cpp
int    x   = 7;
int*   p   = &x;      // p -> x
int**  pp  = &p;      // pp -> p

pp          // p ka address
*pp         // == p    (ek int*)
**pp        // == x    (7)

**pp = 42;  // x -> 42
*pp = &other;  // ab p `other` pe point karta hai
```

```
   pp ──▶ p ──▶ x
   (int**) (int*) (int)
```

Har `*` ek arrow follow karta hai — bas yahi baat yaad rakho, sab saaf ho jaayega.

---

## Use 1 — function se pointer khud ko modify karna

Jo function `int*` leta hai woh *pointee* badal sakta hai. Par agar **pointer
khud** badalna ho (caller ka pointer kahin aur point karne lage), to `int**`
pass karo:

```cpp
void allocate(int** out, int n) {
    *out = new int[n];          // caller ka pointer ab naye block pe point karta hai
}

int* p = nullptr;
allocate(&p, 10);              // pointer ka ADDRESS pass karo
// ab p 10-int array pe point kar raha hai

void freeAndNull(int** pp) {
    delete[] *pp;
    *pp = nullptr;             // caller ka pointer ab null hai (dangling nahi)
}
```

C++ mein **reference to pointer** (`int*&`) zyada saaf tareeka hai (folder 13):
```cpp
void allocate(int*& out, int n) { out = new int[n]; }
allocate(p, 10);              // call karte waqt & lagane ki zaroorat nahi
```

---

## Use 2 — C-style dynamic 2D array (bikhra hua)

```cpp
int** grid = new int*[rows];              // row pointers ka array
for (int i = 0; i < rows; ++i)
    grid[i] = new int[cols];             // har row ek alag allocation

grid[1][2] = 5;                           // *(*(grid + 1) + 2)

for (int i = 0; i < rows; ++i) delete[] grid[i];   // pehle har row free karo
delete[] grid;                                     // phir row array
```

```
   grid ──▶ [•][•][•]
             │  │  └─▶ [ .. cols ints .. ]   (alag block)
             │  └────▶ [ .. cols ints .. ]   (alag block)
             └───────▶ [ .. cols ints .. ]   (alag block)
```

⚠️ **`rows + 1` allocations, memory mein bikhre hue, cache ke liye bura, aur leak
karna bahut aasaan.** Iski jagah **flat** `std::vector<int>(rows * cols)` + `i*cols
+ j` indexing use karo (folder 09 file 06). `int**` wala 2D ek C idiom hai jise
**pehchanna** hai, likhna nahi.

---

## Use 3 — `argv`

```cpp
int main(int argc, char** argv);        // char* argv[]  ==  char** argv
```

`argv` `char*` ka array hai (har ek ek C-string). `argv[i]` ek `char*` hai,
`argv[i][j]` ek `char` hai, aur `argv[argc]` `nullptr` hota hai (folder 08 file 13).

---

## `int**` aur uske jaise types padhna

```cpp
int**  a;      // int ke pointer ka pointer
int*   b[5];   // 5 int* ka array          (b[i] ek int* hai)
int (*c)[5];   // int[5] ka pointer        (*c ek int[5] hai)
int*   (*d)(int);   // ek function(int) ka pointer jo int* return karta hai
```

Inside-out / spiral rule use karo, ya zindagi aasaan karo — `using` bana lo:
```cpp
using IntPtr = int*;
IntPtr* a;     // ab saaf dikh raha hai: "IntPtr ka pointer"
```

---

## Andar kya hota hai

- `int**` bhi 8 bytes ka hai (ek address), baaki pointers ki tarah.
- `**pp` → **do dependent loads**: `load [pp]` se `p` milta hai, phir `load [p]`
  se `x`. Doosra load pehle ke poora hone se pehle **shuru hi nahi ho sakta** →
  latency serialize ho jaati hai.
- `int**` wala 2D traversal → `grid[i][j]` matlab `load [grid + i*8]` phir
  `load [that + j*4]` — do indirections, aur rows alag-alag cache lines/pages
  mein → miss pe miss.
- Flat 2D (`v[i*cols + j]`) → ek scaled load, contiguous → prefetcher khush.

> **HFT relevance:** Hot HFT code mein `int**` / triple-pointers avoid kiye jaate
> hain — indirection ka har level ek dependent load hai (latency), aur arrays ke
> case mein bikhri memory (cache misses). Jahan sach mein "caller ka pointer
> badalna" ho (bahut kam), wahan `T*&` reference use hoti hai. `**` legitimately
> sirf C-API boundaries pe dikhta hai (`getaddrinfo`, `strtol` ka `char**`
> endptr, `argv`) — aur usse turant wrap kar diya jaata hai. `T**` wale 2D ki
> jagah har jagah flat contiguous buffers aate hain.

---

## Hands-on

`examples/07_pointer_diagrams.cpp` (usme `**ppx` wala section):

```bash
./build.ps1 12-POINTERS/examples/07_pointer_diagrams.cpp
```

Ek `int**` 2D array banao, phir usi ko flat `std::vector<int>` se dobara likho —
allocation count aur `grid[1][2]` access dono compare karo.

---

## ⚠️ Traps

### Trap 1 — asli 2D array ke liye `int**`
```cpp
void f(int** m);  int grid[3][4];  f(grid);   // ❌ int[3][4] -> int(*)[4] banta hai, int** nahi
```

### Trap 2 — rows leak kar dena
```cpp
delete[] grid;   // ⚠️ har grid[i] leak ho gaya. Pehle rows free karo, phir grid
```

### Trap 3 — `T**` function ko `&p` ki jagah `p` dena
```cpp
void alloc(int** out);  int* p;  alloc(p);   // ❌ ERROR -- alloc(&p) likho
```

### Trap 4 — `**pp` jab `*pp` null ho sakta hai
```cpp
int** pp = ...;  **pp;   // ⚠️ agar *pp == nullptr -> crash. Pehle *pp check karo
```

### Trap 5 — zaroorat se zyada indirection
```cpp
Config*** cfg;   // 😱 yeh lagbhag kabhi sahi nahi hota -- design flatten karo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int**` ek 2D array hai" | Double indirection hai; rows bikhre hue. Flat buffer lo |
| "`int**` 16 bytes ka hai" | 8 — har pointer jitna |
| "`**pp` ek load hai" | Do dependent loads (latency serialize hoti hai) |
| "`int**` se modify karne ke liye `p` pass karo" | `&p` pass karo (ya `int*&` use karo) |
| "`int**` 2D ek `delete[]` se free hota hai" | Pehle har row, phir row array |

---

## Exercises

1. **Triple deref:** `int v = 5; int* p = &v; int** pp = &p; int*** ppp = &pp;`
   — sirf `ppp` use karke `v` ko `100` banao.

2. **Pointer modify:** `void pointToSecond(int** pp, int* arr) { *pp = &arr[1]; }`
   — call karo, aur verify karo ki caller ka pointer sach mein hila.

3. **`int*&` version:** exercise 2 ko `int*& p` se dobara likho — call site
   zyada saaf laga?

4. **2D `int**`:** 3×4 ka `int**` grid allocate karo, `grid[i][j] = i*4+j` bharo,
   print karo, aur sahi order mein free karo (pehle rows, phir array). `new` aur
   `delete` kitne baar chale, gino.

5. **Flat rewrite:** wahi 3×4 grid `std::vector<int>(12)` + `at(i,j)` se banao.
   Ab `new` kitne baar chala? `grid(1,2)` kya deta hai?

6. **Type padho:** `char* (*handlers[3])(const char*);` — `handlers` kya hai,
   `handlers[0]` kya, aur `handlers[0]("x")` kya dega?

---

## Interview questions

1. `int**` kya hai? `*pp`, `**pp` kya dete hain?
2. `int**` ka size?
3. Function ko caller ka *pointer* modify karne dena — kaise (`int**` vs `int*&`)?
4. `int**` 2D array — memory layout, kitne allocations, cache issue?
5. `**pp` ki cost (dependent loads)?
6. `int**` 2D array free karna — sahi order?

---

## Next
→ [`10-void-pointers.md`](10-void-pointers.md)
