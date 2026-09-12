# 02 — `&` — the address-of operator

## Prerequisites
- [`01-what-is-a-pointer.md`](01-what-is-a-pointer.md)

## Yeh topic abhi kyun
Pointer banane ke liye ek address chahiye. `&x` deta hai. Yeh chhota operator hai
par iske ki uses (aur ek naming collision — reference declarations) samajhna zaroori.

---

## `&x` = "x ka address"

```cpp
int x = 42;
int* p = &x;           // &x -> x ka address, type int*

std::cout << &x;       // 0x7ffc... (implementation-defined hex)
std::cout << &p;       // p ka apna address (int**)
```

Har **lvalue** ka `&` liya ja sakta hai — matlab har woh cheez jiski memory mein
apni ek jagah ho (named object, array element, struct member, dereferenced
pointer). Jiski koi jagah hi nahi (jaise ek temporary), uska address nahi milta:

```cpp
&x            // ✅ variable
&arr[3]       // ✅ array element
&s.member     // ✅ struct member
&*p           // ✅ == p
&(x + 1)      // ❌ ERROR -- (x+1) ek temporary (prvalue) hai, uska koi ghar nahi
&42           // ❌ ERROR -- literal
&func         // ✅ function ka address (file 11)
```

---

## `&` ke 3 alag meanings (context se pehchano)

```cpp
int x = 5;

int* p = &x;           // 1. ADDRESS-OF operator (unary, expression mein)
int& r = x;            // 2. REFERENCE declaration (folder 13) -- "r int ka reference hai"
int a = 3 & 5;         // 3. BITWISE AND (binary operator, folder 05)
```

Pehchanne ka aasaan tareeka:

- **Expression mein, ek hi operand** → address-of.
- **Declaration mein, type ke baad** → reference.
- **Do operands ke beech** → bitwise AND.

---

## Address ko print / store karna

```cpp
int x = 1;
int* p = &x;

std::cout << p;                                    // hex mein, ostream ke void* overload se
std::cout << static_cast<const void*>(&x);         // saaf-saaf -- char* ke liye zaroori (warna string chhap jaayegi!)
std::uintptr_t n = reinterpret_cast<std::uintptr_t>(p);   // address ko integer ki tarah
std::printf("%p\n", static_cast<void*>(p));         // C-style
```

⚠️ `std::cout << charPtr` address nahi, **string** print karta hai — address
dekhna ho to `void*` mein cast karo.

---

## Addresses runtime ki values hain

```cpp
int a, b;
std::cout << (&a < &b);     // ⚠️ result badalta rehta hai -- stack layout ki koi guarantee nahi
```

- Har run pe addresses badal jaate hain (ASLR — address space layout randomization).
- Alag-alag variables ka aapsi order **unspecified** hai (`<` sirf ek hi array ke
  andar matlab rakhta hai — file 05).
- `==` / `!=` kisi bhi do pointers ke beech hamesha theek hai.

---

## `std::addressof` — jab `&` overload ho

```cpp
#include <memory>
T* real = std::addressof(obj);   // asli address deta hai, chahe T ne operator& overload kiya ho
```

Kuch classes `operator&` overload kar leti hain (kam hota hai, jaise kuch smart
handles). `std::addressof` us overload ko bypass kar deta hai. Generic code mein
isi ko prefer karo.

---

## Andar kya hota hai

- Local ke liye `&x` → compiler `frame_base + offset_of_x` nikaalta hai (ek `lea`
  instruction — "load effective address"). Koi memory access nahi hota.
- Global ke liye `&x` → `.data`/`.bss` mein pade symbol ka address.
- Jo variable register mein reh sakta tha, uska `&` lene se woh stack pe aa jaata
  hai (kyunki ab use ek address chahiye) — yeh ek chhota optimization inhibitor hai.

> **HFT relevance:** Low-level code mein `&` har jagah hai — buffer ka address,
> struct field ka, array slot ka, jo syscall ya kisi pointer-based data structure
> ko dena hota hai. Cost ke hisaab se yeh free hai (ek `lea`, koi load nahi). Ek
> hi performance note hai: kisi cheez ka address le lene se compiler use call ke
> aar-paar register mein nahi rakh paata. Isliye hot code mein scalars ka bina
> matlab `&` lena kam karo.

---

## Hands-on

`examples/01_basic_pointers.cpp`, `examples/07_pointer_diagrams.cpp`:

```bash
./build.ps1 12-POINTERS/examples/07_pointer_diagrams.cpp
```

---

## ⚠️ Traps

### Trap 1 — temporary / literal ka `&`
```cpp
int* p = &(a + b);   // ❌ ERROR -- prvalue ka koi address nahi hota
```

### Trap 2 — `cout << charPtr` string print karta hai
```cpp
char c = 'A';
std::cout << &c;                          // ⚠️ ise char* maanta hai -> "A" + '\0' tak ka kachra
std::cout << static_cast<void*>(&c);      // ✅ asli address
```

### Trap 3 — alag-alag variables ke addresses compare karna
```cpp
if (&a < &b) { }   // ⚠️ unspecified. Sirf ek hi array ke andar matlab rakhta hai
```

### Trap 4 — aisa address rakhna jo baad mein dangle karega
```cpp
int* p;
{ int x = 1; p = &x; }   // ⚠️ block ke baad p dangling (file 12)
```

### Trap 5 — `&arr` vs `arr` (folder 09 file 05)
```cpp
int arr[10];
&arr        // int(*)[10]  -- poore array ka pointer
arr         // int*        -- &arr[0] mein decay ho jaata hai
// address ki value same, par type alag -> pointer arithmetic bhi alag
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`&` sirf address-of hai" | Reference declaration bhi, bitwise AND bhi — context dekho |
| "`char c` ke liye `cout << &c` address deta hai" | Woh string print karta hai — `void*` mein cast karo |
| "Addresses har run mein same rehte hain" | ASLR unhe randomize kar deta hai |
| "`&a < &b` ka matlab hai" | Sirf ek hi array ke andar |
| "`&arr` == `arr`" | Value same, type alag (`int(*)[10]` vs `int*`) |

---

## Exercises

1. **Kiska address hai:** har ek ke liye batao `&` compile hoga ya nahi — `x`,
   `arr[2]`, `x + 1`, `s.field`, `*p`, `42`, `main`.

2. **Addresses print karo:** kuch locals aur ek array ke liye `&x`, `&y`,
   `&arr[0]`, `&arr[1]` print karo. Kaunse differences `sizeof(int)` ke barabar hain?

3. **char\* trap:** `char c = 'Z'; std::cout << &c;` aur `std::cout <<
   static_cast<void*>(&c);` — output mein kya fark aaya?

4. **ASLR:** ek program likho jo `&someLocal` print kare, aur use teen baar alag-
   alag chalao. Address same aaya? (Aksar nahi.)

5. **`&arr` vs `arr`:** `int a[5];` — `sizeof(*(&a))` aur `sizeof(*a)`? `&a + 1`
   aur `a + 1` mein kitne bytes ka fark?

6. **`std::addressof`:** generic library code `&x` ki jagah `std::addressof(x)`
   kyun use karti hai?

---

## Interview questions

1. `&x` kya deta hai? Kis cheez ka `&` le sakte ho?
2. `&` ke 3 meanings (context se)?
3. `cout << charPointer` kya print karta hai, kyun?
4. Do unrelated variables ke addresses compare karna — defined?
5. `&arr` aur `arr` mein fark (type)?
6. `std::addressof` kab `&` se better?

---

## Next
→ [`03-dereferencing.md`](03-dereferencing.md)
