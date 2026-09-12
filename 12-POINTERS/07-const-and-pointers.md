# 07 — `const` aur pointers

## Prerequisites
- [`03-dereferencing.md`](03-dereferencing.md)
- `03-VARIABLES-DATA-TYPES/11-const-and-constexpr.md`

## Yeh topic abhi kyun
`const` + `*` mila ke 3 alag cheezein banti hain — kya lock hai, data ya
pointer? Yeh interviews mein poocha jaata hai, aur const-correct APIs likhne ke
liye zaroori.

---

## Teen combinations

```cpp
int x = 10, y = 20;

//  1. pointer to CONST int  -- data lock, pointer free
const int* p1 = &x;        // (aise bhi likhte hain: int const* p1)
// *p1 = 5;                //  ❌ *p1 badal nahi sakte
p1 = &y;                   //  ✅ kahin aur point kara sakte ho

//  2. CONST pointer to int  -- pointer lock, data free
int* const p2 = &x;        //  yahin initialize karna PADEGA
*p2 = 5;                   //  ✅ *p2 badal sakte ho
// p2 = &y;                //  ❌ dobara point nahi kara sakte

//  3. CONST pointer to CONST int  -- dono lock
const int* const p3 = &x;
// *p3 = 5;   ❌
// p3 = &y;   ❌
```

---

## Padhne ka rule: `*` dhoondo

```
   const int *  p     ->  p ek pointer hai (const int) ka
   int const *  p     ->  WAHI baat  (const int pe lagta hai, dono taraf se)
   int * const  p     ->  p ek (const pointer) hai int ka
   const int * const p ->  const int ka const pointer
```

**Shortcut yaad rakho:** `*` dhoondo. `const` agar `*` se **pehle** hai → jispe
point kar rahe ho woh (pointee) const hai. `const` agar `*` ke **baad** hai →
pointer khud const hai.

```cpp
const int* p;      //  * ke pehle const -> pointee const
int* const p;      //  * ke baad const  -> pointer const
```

---

## Conversions — `const` jod sakte ho, hata nahi sakte

```cpp
int x = 5;
int* p = &x;
const int* cp = p;       // ✅ const jod diya (int* -> const int*)
int* bad = cp;           // ❌ ERROR -- const chupchaap hata nahi sakte
int* forced = const_cast<int*>(cp);   // ⚠️ const hata deta hai -- agar asli object
                                      //    sach mein const tha aur ab likha, to UB
```

`const_cast` ek code smell hai — iski zaroorat aksar wahi padti hai jahan koi
purani C API `const` lagana bhool gayi ho. Sach-much `const` object mein iske
zariye likhna UB hai.

---

## Function parameters mein `const`

```cpp
void print(const int* data, std::size_t n);   // "main sirf padhunga"
void fill (int* data, std::size_t n);          // "main aapke data mein likhunga"

void log(const std::string& msg);              // read-only ref (folder 13)
```

Yeh `const` ek **compiler-enforced vaada** hai. Read-only access ke liye
`const T*` / `const T&` hi default hone chahiye — caller ko pata rehta hai ki
uska data safe hai.

Iska ek aur faayda: `const` objects aur temporaries bhi pass ho jaate hain:
```cpp
const int arr[] = {1, 2, 3};
print(arr, 3);        // ✅ (isse const int* chahiye)
fill(arr, 3);         // ❌ arr const hai
```

---

## `const` aur multi-level pointers

```cpp
int x = 0;
int* p = &x;
int** pp = &p;

const int** cpp = pp;        // ❌ ERROR (chaunkane wala!) -- yeh const tod sakta tha
int* const* ok = pp;         // ✅ "pointer to (const pointer to int)"
```

Deep-const conversions ke rules subtle hain; pointer-array ka "read-only view"
safely `const int* const*` se banta hai.

---

## `constexpr` pointers (folder 03, 08)

```cpp
constexpr int arr[] = {1, 2, 3};
constexpr const int* p = &arr[1];   // compile-time constant pointer

// constexpr T* -- POINTER khud ek constant expression hai (T* const jaisa, aur usse zyada)
```

---

## Andar kya hota hai

- Pointer pe `const` **poori tarah compile-time** cheez hai — machine code bilkul
  wahi banta hai jo bina `const` ke banta. Yeh sirf ek access-check annotation hai.
- `const int* p` — compiler `*p = ...` ko compile time pe reject karta hai;
  runtime pe `*p` ek normal load hi hai.
- Sach-much `const` object (`const int x = 5;`) read-only memory (`.rodata`) mein
  ja *sakta* hai → `const_cast` karke likhne pe fault aayega.
- `const` optimizer ki thodi madad kar sakta hai (use pata hai ki `*p` **p ke
  zariye** nahi badlega), par aliasing rules is faayde ko kaafi limit kar dete
  hain — asli strong hint `__restrict` hai.

> **HFT relevance:** const-correctness ek **discipline** hai, performance feature
> nahi (runtime cost zero hai). Iska faayda yeh hai ki hot-path APIs khud apna
> matlab bata deti hain — `decode(const std::byte* buf, ...)` saaf kehta hai
> "main tumhare receive buffer pe likhunga nahi." Galti se hone wale writes
> compile time pe hi pakde jaate hain. HFT code mein `const_cast` dikhe to woh
> red flag hai (matlab kisi C API ko wrap karna baaki hai). Aur `constexpr`
> tables mein `constexpr` pointers zero-cost compile-time lookup dete hain
> (folder 08 file 11).

---

## Hands-on

`examples/03_const_pointers.cpp` — teeno combinations, declarations padhna, aur
API const-correctness:

```bash
./build.ps1 12-POINTERS/examples/03_const_pointers.cpp
```

---

## ⚠️ Traps

### Trap 1 — samajhna ki `const int* p` pointer ko lock karta hai
```cpp
const int* p = &x;  p = &y;   // ✅ allowed hai -- sirf *p lock hai
```

### Trap 2 — `int* const p` bina initializer ke
```cpp
int* const p;   // ❌ ERROR -- const ko initialize karna zaroori hai
```

### Trap 3 — chupke se const gira dena
```cpp
void f(int* p);
const int* cp = ...;
f(cp);   // ❌ ERROR (achhi baat hai!). Isse "theek" karne ke liye const_cast mat karo
```

### Trap 4 — const object mein `const_cast` se likhna
```cpp
const int x = 5;
int* p = const_cast<int*>(&x);
*p = 10;   // ⚠️ UB -- x read-only memory mein ho sakta hai
```

### Trap 5 — `const int**` wala conversion
```cpp
int** pp;  const int** cpp = pp;   // ❌ ERROR. Aapko int* const* chahiye
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`const int* p` = const pointer" | Pointee const hai; pointer dobara point kar sakta hai |
| "Pointer pe `const` ka runtime cost hai" | Zero — sirf compile-time check |
| "`const_cast` ek normal tool hai" | Smell — sirf legacy interop; const object mein likha to UB |
| "`const int** = int**` chalna chahiye" | Reject hota hai — warna const safety tootti |
| "`int* const p;` pehle declare, baad mein assign" | Declaration ke waqt hi initialize karna padega |

---

## Exercises

1. **Pehchano:** har ek ke liye batao kya lock hai (pointer / pointee / dono):
   `const char* a`, `char* const b`, `const char* const c`, `char const* d`.

2. **Kaunsa compile hoga?** `int x, y; const int* p = &x;` diya hai —
   `*p = 1;`, `p = &y;`, `int z = *p;`.

3. **API:** `void reverse(int* a, size_t n)` aur `int sum(const int* a, size_t n)`
   likho. Dono ko ek mutable array aur ek `const` array pe call karo — kaunse
   combinations compile hue?

4. **`const_cast` ka khatra:** `const int k = 42; int* p = const_cast<int*>(&k);
   *p = 7; std::cout << k << " " << *p;` — `-O0` aur `-O2` dono pe chalao. Output
   same aaya?

5. **Declaration padho:** `char* (*fns[4])(const char*, int);` kya hai? (Tukdon
   mein tod ke banao.)

6. **Multi-level:** `const int** cpp = pp;` reject kyun hota hai? Ek concrete
   const-violation banao jo yeh allow kar deta.

---

## Interview questions

1. `const int* p`, `int* const p`, `const int* const p` — kya kya lock?
2. Reading rule for `const` + `*`?
3. `const` add kar sakte ho, remove nahi — `const_cast` kab, aur risk?
4. `const` pointer ka runtime cost?
5. `const int** = int**` kyun error?
6. Const-correct API — `print` vs `fill` signatures?

---

## Next
→ [`08-pointers-to-structs.md`](08-pointers-to-structs.md)
