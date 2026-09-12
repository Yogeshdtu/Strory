# 06 — Pointers aur arrays

## Prerequisites
- [`05-pointer-arithmetic.md`](05-pointer-arithmetic.md)
- `09-ARRAYS/05-array-decay.md` (decay wahan ho chuka — yahan usse jodenge)

## Yeh topic abhi kyun
Arrays aur pointers **bahut kareeb** hain (`arr[i] == *(arr+i)`) par **ek nahi**
hain. Folder 09 file 05 mein decay dekha tha; yahan pointer ki taraf se poora
picture, aur yeh ki "kab array use karein, kab pointer."

---

## Array pointer mein decay hota hai (recap)

```cpp
int arr[5] = {1, 2, 3, 4, 5};

int* p = arr;          // arr -> &arr[0]  (apne aap, & lagane ki zaroorat nahi)
p == &arr[0]           // true
*p == arr[0]           // true
p[2] == arr[2]         // true -- p[i] kisi bhi pointer pe chalta hai
```

Zyadatar expressions mein `arr` (type `int[5]`) apne aap `int*` ban jaata hai, jo
pehle element pe point karta hai.

---

## Par array ≠ pointer

| | `int arr[5]` | `int* p` |
|---|---|---|
| `sizeof` | 20 (poora array) | 8 (ek pointer) |
| `&` | `int(*)[5]` (array ka pointer) | `int**` |
| Assign kar sakte ho | ❌ `arr = ...` error | ✅ `p = ...` |
| Memory mein kya | 5 ints, yahin par | 8 bytes, jisme ek address hai |
| `std::size(x)` | ✅ 5 | ❌ compile error |

```cpp
int arr[5];  int* p = arr;
sizeof(arr)   // 20
sizeof(p)     // 8
arr = p;      // ❌ ERROR -- array ka naam modifiable lvalue nahi hai
p = arr;      // ✅
```

Seedhi baat: `arr` ek *non-modifiable lvalue* hai jo decay hota hai; `p` ek normal
variable hai.

---

## Function parameters — hamesha pointer bante hain

```cpp
void f(int arr[]);      //  ┐
void f(int arr[10]);    //  ├─ TEENO ka matlab ek hi hai:  void f(int* arr)
void f(int* arr);       //  ┘   ([] mein likhi size ignore ho jaati hai)
```

`f` ke andar `arr` ek `int*` hai — `sizeof(arr)` 8 hai, aur `std::size(arr)`
compile hi nahi hoga. Isliye **size alag se pass karo, ya `std::span` use karo**
(folder 09 file 07, file 09).

```cpp
long long sum(const int* a, std::size_t n);
long long sum(std::span<const int> a);          // ✅ modern tareeka
```

---

## `p[i]` sirf decayed arrays pe nahi, har pointer pe chalta hai

```cpp
int* p = new int[10];
p[3] = 5;              // *(p + 3)

std::vector<int> v(10);
int* vp = v.data();
vp[3] = 5;             // wahi baat
```

Subscript hai hi pointer arithmetic — usse fark nahi padta ki pointer array se
aaya, `new[]` se, ya `.data()` se. **Par bounds ki zimmedari aapki hai** — raw
pointer pe `.at()` jaisa kuch nahi hota.

---

## Arrays of pointers vs pointers to arrays

```cpp
int* ptrs[3];          // 3 int* ka array          -- (ptrs[i] ek int* hai)
int (*arrPtr)[3];      // ek int[3] ka pointer     -- (*arrPtr ek int[3] hai)

// argv char* ka array hai:
int main(int argc, char* argv[]);   // char** argv -- C-strings ka array
```

Padhne ka tareeka: **naam se shuru karo, phir bahar ki taraf**. `int* ptrs[3]` —
`ptrs` ek array hai (`[3]`) jisme `int*` hain. `int (*arrPtr)[3]` — `arrPtr` ek
pointer hai (`*`) jo `int[3]` pe point karta hai.

---

## Multidimensional (folder 09 file 06)

```cpp
int m[3][4];
m           // int(*)[4] mein decay hota hai  -- 4 ke ek row ka pointer
m[1]        // int[4] -> int* mein decay  -- row 1 ka pehla element
&m[0][0]    // int*  -- flat shuruaat

void f(int m[][4]);        // == int (*m)[4]
void f(int** m);           // ❌ int[3][4] ke saath compatible NAHI hai
```

---

## Andar kya hota hai

- Decay compile-time pe hota hai — expression mein `arr` ko bas `&arr[0]` maan
  liya jaata hai (sirf type badalta hai, koi code generate nahi hota).
- Function ko array dene pe sirf **address** (8 bytes) jaata hai — elements ki
  copy nahi hoti. Yeh efficient hai, par **size kho jaati hai**.
- `p[i]` aur `arr[i]` bilkul same scaled load mein compile hote hain.
- `int (*)[N]` vs `int**`: pehla ek hi indirection hai contiguous memory mein;
  doosra double indirection hai bikhri hui memory mein.

> **HFT relevance:** Hot code contiguous data ko `std::span` ki tarah pass karta
> hai (pointer + size, decay ka confusion nahi, size-safe) aur bounded index pe
> `[]` lagata hai. Raw `int*` + manual size sirf C-API aur syscall boundaries pe
> dikhta hai. Intrusive structures aur arenas jaan-boojh kar raw pointers use
> karte hain. `int**` wale "2D arrays" avoid kiye jaate hain (bikhri memory,
> cache ke liye bura) — unki jagah flat buffer + stride (folder 09 file 06,
> folder 39).

---

## Hands-on

`examples/02_pointer_arithmetic.cpp`, aur folder 09 ka `02_array_decay.cpp`:

```bash
./build.ps1 12-POINTERS/examples/02_pointer_arithmetic.cpp
```

---

## ⚠️ Traps

### Trap 1 — parameter pe `sizeof`
```cpp
void f(int a[]) { int n = sizeof(a) / sizeof(a[0]); }   // ⚠️ 8/4 = 2 milega
```

### Trap 2 — `arr = otherArr`
```cpp
int a[5], b[5];  a = b;   // ❌ ERROR. std::copy use karo, ya std::array (usme a = b chalta hai)
```

### Trap 3 — 2D array ke liye `int**`
```cpp
void f(int** m);  int grid[3][4];  f(grid);   // ❌ type mismatch
```

### Trap 4 — `p[i]` end ke baad
```cpp
int* p = arr;  p[10] = 0;   // ⚠️ raw pointer pe koi bounds check nahi -- UB
```

### Trap 5 — decayed local array return karna
```cpp
int* f() { int a[5]; return a; }   // ⚠️ mar chuke local ka pointer return -> UB
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Array aur pointer ek hi cheez hain" | Jude hue hain (`arr[i]==*(arr+i)`) par type/size/assignability alag |
| "`void f(int a[10])` 10 enforce karta hai" | Woh `int* a` hai — `10` ignore ho jaata hai |
| "`p[i]` sirf arrays pe chalta hai" | Kisi bhi pointer pe — par bounds check nahi |
| "`int**` ek 2D array hai" | Woh double indirection hai; decayed 2D array `int(*)[N]` hai |
| "Array pass karne pe copy hoti hai" | Sirf address jaata hai (size kho jaati hai) |

---

## Exercises

1. **Decay:** `int a[8];` — `sizeof(a)` nikalo, phir ek function `void g(int p[])`
   banao jo `sizeof(p)` print kare. Dono values kya aayin?

2. **Non-array pe `p[i]`:** `int* p = new int[5]{10,20,30,40,50};` — `p[0..4]` aur
   `*(p+2)` print karo. Phir `delete[] p;`.

3. **Arrays of pointers:** `const char* days[] = {"Mon","Tue","Wed"};` — har ek
   print karo; `sizeof(days)`; `days[1][0]`.

4. **Pointer to array:** `int a[4] = {1,2,3,4}; int (*pa)[4] = &a;` — `(*pa)[2]`?
   `pa + 1` (kitne bytes)? `sizeof(*pa)`?

5. **2D param:** `int rowSum(int m[][3], int rows, int r)` likho, `int grid[2][3]`
   ke saath call karo. Phir `int**` se try karo — compile error aaya?

6. **argv:** `argv` ko `char**` ki tarah print karo — `nullptr` tak loop chalao.
   `argv[0]` mein kya hai?

---

## Interview questions

1. Array decay — kya, kab? Array aur pointer mein 4 differences?
2. Function ke andar array-parameter ka `sizeof`?
3. `int* ptrs[3]` vs `int (*p)[3]` — kya kya hai?
4. `int**` aur `int(*)[N]` — 2D ke context mein fark?
5. `p[i]` kis-kis pointer pe chalta hai? Bounds?
6. `arr = otherArr` kyun error?

---

## Next
→ [`07-const-and-pointers.md`](07-const-and-pointers.md)
