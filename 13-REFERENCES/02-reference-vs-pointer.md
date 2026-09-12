# 02 — Reference vs Pointer — poora comparison

## Prerequisites
- [`01-what-is-a-reference.md`](01-what-is-a-reference.md)
- Folder 12 poora

## Yeh topic abhi kyun
Dono "kisi doosre object tak pahunchne" ka zariya hain. Kaam bahut overlap karta
hai, to sawaal hamesha aata hai: **kab reference, kab pointer?** Ek clear mental
model + decision rule chahiye — yeh interview ka bhi favourite hai.

---

## Side-by-side

| Feature | Pointer `T* p` | Reference `T& r` |
|---|---|---|
| Init zaroori? | Nahi (`T* p;` legal, khatarnaak) | **Haan** (`T& r;` compile error) |
| Rebind? | Haan — `p = &y` | **Nahi** — kabhi nahi |
| Null / "kuch nahi"? | Haan — `nullptr` | **Nahi** |
| Arithmetic (`p + n`)? | Haan | Nahi |
| Use-site syntax | `*p`, `p->m`, `p[i]` | `r`, `r.m` — normal variable jaisa |
| `&x` chahiye call pe? | Haan — `f(&x)` | Nahi — `f(x)` |
| `sizeof` | 8 (pointer khud) | `sizeof(T)` (referent) |
| Container mein? | `std::vector<T*>` ✅ | `std::vector<T&>` ❌ |
| Address of it | `&p` → `T**` (p ka apna address) | `&r` → `T*` (referent ka address) |
| Levels | `T**`, `T***` … | sirf ek level (`T& &` collapse ho jaata) |
| Default "invalid" state | uninitialized / null / dangling | (sirf) dangling — bug se |

---

## Same kaam, dono se

```cpp
void addOne_ptr(int* n) { *n += 1; }      // call: addOne_ptr(&x);
void addOne_ref(int& n) { n  += 1; }      // call: addOne_ref(x);

struct P { int x, y; };
void movePtr(P* p) { p->x += 1; (*p).y += 1; }   // -> aur (*).
void moveRef(P& r) { r.x  += 1;  r.y   += 1; }    // seedha .
```

Generated code lagbhag identical hota hai — reference ABI pe aksar "ek address"
hi hai. Farq **source-level safety aur readability** ka hai.

---

## Kab kaunsa — decision rule

```
Kya "koi nahi / abhi nahi" ek valid state hai?           -> pointer  (ya std::optional<T&>-ish)
Kya baad mein kisi aur object ko point karna hai?         -> pointer
Kya pointer arithmetic / array traversal chahiye?         -> pointer
Kya ek data structure store karni hai (list node, tree)?  -> pointer (ya smart pointer)
C API se baat karni hai?                                  -> pointer

Warna (default):                                          -> reference
```

Aur bhi seedhe shabdon mein:

- **Function parameter, hamesha ek valid object aayega** → `T&` (mutate) ya
  `const T&` (read). Yeh sabse common use hai.
- **Function ko "shayad kuch nahi" bhejne dena hai** → `const T*` / `T*` (caller
  `nullptr` bhej sake) — ya modern: `std::optional`, `T* = nullptr` default arg.
- **Class ko kisi bahar ke object ka handle rakhna hai jo badal sakta hai / null
  ho sakta hai** → pointer member. **Hamesha wahi, non-null** → reference member
  (par constraints — file 07).

---

## `reference vs pointer` — chhote gotchas

### `&` ke do jawaab

```cpp
int x = 5;
int* p = &x;
int& r = x;

&p;   // int**  -- p apne aap mein ek object hai, uska address
&r;   // int*   -- &x. r ka koi "apna" address nahi (usually)
```

### Reference to pointer — dono ek saath

```cpp
void reset(int*& pp) { delete pp; pp = nullptr; }   // pp: "reference to int*"
int* p = new int(5);
reset(p);            // call site pe kuch extra nahi; p ab nullptr (folder 12 file 09 ka T*& yahi hai)
```

### Pointer to reference — banta hi nahi

```cpp
int&* p;   // ❌ ERROR -- "pointer to reference" allowed nahi. Reference addressable
           //    "cheez" nahi hai jise point kiya ja sake.
```

### Reference collapsing (folder 18 preview)

```cpp
using Ri = int&;
Ri&  x;   // int& &  -> collapse -> int&
```
`T& &`, `T& &&`, `T&& &` → `T&`. Sirf `T&& &&` → `T&&`. Templates + `auto` mein
matter karta hai (perfect forwarding — file 08 aur folder 18).

---

## Andar kya hota hai

- **Parameter passing:** `f(int&)` — compiler caller se `lea`/address bhijwaata
  hai, callee andar `[reg]` se access karta hai. `f(int*)` bilkul yahi. To
  runtime cost **same**.
- **Alias analysis:** compiler ke liye `T&` param "hamesha valid, ek object" hota
  hai → woh null-branch nahi daalta. Raw `T*` param ke saath, agar `-fno-delete-null-pointer-checks`
  ya visible null-compare ho, extra branch aa sakta hai. Chhota, par hot loop
  mein matter karta hai.
- **`restrict` alag baat hai:** na `T&` na `T*` by default "no aliasing" promise
  dete. Uske liye `__restrict` chahiye (folder 33/36).
- **Debug builds:** `-O0` pe reference bhi aksar ek asli stack slot (pointer)
  banti hai — `-O2` pe woh gaayab ho jaati hai jab compiler poora dekh raha ho.

> **HFT relevance:** hot path signatures mein `const T&` / `T&` default hai —
> zero-cost, aur compiler ko "valid object" guarantee. Pointer tab jab
> genuinely optional (`Order* resting = nullptr;`), ya pointer-arithmetic
> chahiye (ring buffer, arena), ya ek intrusive data structure (`next`/`prev`).
> `T*&` (reference to pointer) allocation-swap / pool hand-back mein dikhta hai.
> Rule of thumb: **agar null ka koi matlab nahi, to null banane hi mat do —
> reference lo.**

---

## Hands-on

```bash
./build.ps1 13-REFERENCES/examples/05_ref_vs_ptr.cpp
```

Ek hi jagah: init, rebind, null, arithmetic, use-syntax, containers, `sizeof` —
sab ka farq print hota hai.

---

## ⚠️ Traps

### Trap 1 — "reference bas syntactic sugar hai pointer ka"
90% cases mein haan, par: reference **rebind/null nahi ho sakti**, aur compiler
usse alias-analysis / no-null-branch ke liye use kar sakta hai. Semantic farq hai.

### Trap 2 — reference member ko "free" samajhna
```cpp
struct Bad { int& r; };   // sizeof(Bad) == 8 -- alias ko store karne ke liye pointer chahiye
```

### Trap 3 — `T&` param ko null "bhejne" ki koshish
```cpp
void f(int& r);
f(*ptr);            // agar ptr == nullptr -> yahi pe UB (deref), function ke andar nahi
```

### Trap 4 — `int&* `
```cpp
int&* p;   // ❌ pointer-to-reference banta hi nahi
int*&  r;  // ✅ reference-to-pointer -- bilkul valid
```

### Trap 5 — container of references
```cpp
std::vector<int&> v;                    // ❌
std::vector<std::reference_wrapper<int>> v;   // ✅ agar zaroorat ho
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Reference = pointer, bas alag likha" | Rebind/null nahi; alias-analysis benefit; source-level safety |
| "Reference member 0 bytes leta hai" | ~8 (pointer jitna) — object ko alias store karna padta hai |
| "`T&` param slower/faster than `T*`" | Runtime pe same; farq null-branch / readability ka |
| "`std::vector<T&>` bas rare hai" | Illegal hai — `reference_wrapper` use karo |
| "`&r` gives r's own address" | `&r` == referent ka address; reference ka apna address nahi |

---

## Exercises

1. **Decision drill:** har case mein reference ya pointer? (a) "ek int badalne
   wala param", (b) "shayad ek `Config` milega, shayad nahi", (c) "linked list
   node ka `next`", (d) "bade `struct` ko read-only pass karna", (e) "ek buffer
   pe chalna, `p++`".

   <details><summary>Answer</summary>

   (a) `int&`  (b) `const Config*` ya `std::optional`  (c) pointer (`Node*`)
   (d) `const T&`  (e) pointer (arithmetic chahiye).
   </details>

2. **Sizeof:** `int x; int* p = &x; int& r = x; struct S { int& m; } s{x};` —
   `sizeof(p)`, `sizeof(r)`, `sizeof(S)`? Kyun `S` 8?

   <details><summary>Answer</summary>

   `sizeof(p) == 8`. `sizeof(r) == sizeof(int) == 4` (`sizeof` referent report
   karta hai). `sizeof(S) == 8` — reference member ko alias store karne ke liye
   ek pointer jitni jagah chahiye.
   </details>

3. **`T*&`:** `void grow(std::vector<int>*& vp)` — function ke andar `vp` ko ek
   naye `std::vector<int>` pe point karwao. Call site kaisa dikhega? Ab yahi
   `std::vector<int>**` se — call site?

   <details><summary>Answer</summary>

   `T*&`: `grow(vp);` — call site clean, `&` nahi. `T**`: `grow(&vp);` — `&`
   lagana padta, aur body mein `*vp = ...`. Dono same kaam.
   </details>

4. **Illegal types:** in mein se kaun compile nahi karega? `int*&`, `int&*`,
   `int&&`, `int& arr[3]`, `std::vector<int&>`.

   <details><summary>Answer</summary>

   `int*&` ✅. `int&*` ❌ (pointer to reference). `int&&` ✅ (rvalue ref — file 08).
   `int& arr[3]` ❌ (array of references nahi banti). `std::vector<int&>` ❌.
   </details>

5. **Null branch:** ek function `int sumFirst(const int* a, int n)` aur
   `int sumFirst(const std::vector<int>& v)` — kaunsa version compiler ke liye
   "input hamesha valid" hai? `-O2 -S` se dekho kya null-check aata hai.

   <details><summary>Answer</summary>

   `const std::vector<int>&` version — reference "valid object" maani jaati hai,
   compiler null-check nahi daalta. Pointer version, agar kahin `a` ko null se
   compare kiya jaaye, branch aa sakta hai.
   </details>

---

## Interview questions

1. Reference aur pointer ka poora farq (kam se kam 5 points).
2. Kab pointer chahiye jahan reference kaam nahi karti? (3 concrete)
3. `T*&` kya hai? Ek use case.
4. `int&*` kyun invalid, `int*&` kyun valid?
5. Reference member wali class ka `sizeof` — kyun non-zero?
6. Compiler `T&` param se `T*` param ke muqable kya extra maan leta hai?

---

## Next
→ [`03-references-as-parameters.md`](03-references-as-parameters.md)
