# 05 — Array → pointer DECAY

## Prerequisites
- [`03-accessing-elements.md`](03-accessing-elements.md) (`arr[i]` == `*(arr+i)`)
- `08-FUNCTIONS/03-parameters-and-arguments.md` (pass by pointer)

## Yeh topic abhi kyun
**Yeh folder ka sabse important lesson hai.** Jab array ko function ko pass karo
(ya ki aur contexts mein use karo), woh apne pehle element ke **pointer mein
"decay"** ho jaata hai — array ka **size gayab**. Yeh #1 array bug ka source hai,
aur pointers (folder 12) ka darwaza.

---

## Decay kya hai

```cpp
int arr[10];

int* p = arr;          // arr -> &arr[0]  (koi & nahi lagaya -- automatic)
```

Zyada tar expressions mein, `arr` ka type `int[10]` **decay** hoke `int*` (pointer
to first element) ban jaata hai. `arr` ab bas ek address hai — `10` kho gaya.

### Decay kab hota hai
- Function argument pass karte waqt
- `auto p = arr;` → `p` is `int*`, not `int[10]`
- Pointer arithmetic: `arr + 1`
- Comparison, assignment to pointer

### Decay kab NAHI hota
- `sizeof(arr)` → poora array size (jab tak `arr` array ho)
- `&arr` → `int(*)[10]` (pointer to array — alag type)
- `std::size(arr)`, range-for over `arr`
- Reference-to-array parameter `int (&r)[10]`
- `decltype(arr)` → `int[10]`

---

## `sizeof` ka classic trap

```cpp
void printSize(int arr[]) {                  // <- yeh JHOOTH hai
    std::cout << sizeof(arr);                // 8  -- POINTER ka size!
}

int main() {
    int data[10];
    std::cout << sizeof(data) << "\n";       // 40  -- array ka size (main mein)
    printSize(data);                          // 8   -- decay ho gaya
}
```

`g++ -Wall` → `-Wsizeof-array-argument`:
```
warning: 'sizeof' on array function parameter 'arr' will return size of 'int*'
```

**`examples/02_array_decay.cpp`** — yeh warning jaan-boojh kar aati hai (teaching).

### Decay-driven off-by-one
```cpp
void process(int arr[]) {
    for (std::size_t i = 0; i < sizeof(arr) / sizeof(arr[0]); ++i) ...
    //                          8 / 4 = 2   -- sirf 2 elements! (chahe array 100 ka ho)
}
```

---

## Function parameter — teen "array" syntaxes, sab pointer

```cpp
void f(int arr[10]);    //  ┐
void f(int arr[]);      //  ├─ teenon EXACTLY SAME: void f(int* arr)
void f(int* arr);       //  ┘   (`10` compiler ignore karta hai)
```

Yeh ek hi function hain — overload nahi (`02_array_decay.cpp` mein `sameFn`).
Iska matlab: **function ko array ki size khud nahi milti — aapko pass karni
padegi.**

---

## Size pass karne ke tareeke

### 1. Explicit size parameter (C style)
```cpp
long long sum(const int* arr, std::size_t n) {
    long long s = 0;
    for (std::size_t i = 0; i < n; ++i) s += arr[i];
    return s;
}
sum(data, std::size(data));
```

### 2. `std::span` (C++20) — BEST modern (file 09)
```cpp
long long sum(std::span<const int> data) {   // ptr + len ek saath
    long long s = 0;
    for (int x : data) s += x;
    return s;
}
sum(data);   // C array, std::array, std::vector -- sab chalega, size auto
```

### 3. Reference-to-array — size template se deduce
```cpp
template <std::size_t N>
long long sum(const int (&arr)[N]) {         // N compiler nikalta hai
    long long s = 0;
    for (std::size_t i = 0; i < N; ++i) s += arr[i];
    return s;
}
sum(data);   // no size needed -- par sirf compile-time-sized arrays pe
```

### 4. `std::array` — value type, size type mein (file 08)
```cpp
long long sum(const std::array<int, 10>& a);   // size fixed in the type
```

---

## Pointer vs array — same nahi hain

```cpp
int arr[10];
int* ptr = arr;

sizeof(arr)     // 40   (array)
sizeof(ptr)     // 8    (pointer)

&arr            // int(*)[10]   -- pointer to WHOLE array
&ptr            // int**        -- pointer to a pointer

arr = something;   // ❌ ERROR -- array name assign nahi ho sakta
ptr = something;   // ✅ pointer re-bind ho sakta hai
```

`arr` aur `&arr[0]` ki **value same** (address), par **type alag** (`int[10]` vs
`int*`). `arr` ek non-modifiable lvalue hai jo pointer mein decay hota hai.

---

## Andar kya hota hai

- Decay compile-time pe hota hai — koi runtime cost. Bas compiler `int[10]` ko
  `int*` treat karta hai us expression mein.
- Function call pe: array ka **address** (8 bytes) register/stack mein jaata hai
  — poora array copy **nahi** hota (yeh actually efficient hai).
- Size info compiler ke type system mein thi, woh `int*` mein nahi jaati.

> **HFT relevance:** Decay hi wajah hai ki raw arrays function boundaries pe
> unsafe hain — size lost, off-by-one, buffer overrun. HFT code mein array
> parameters **hamesha `std::span<T>`** (ya `std::span<const T>` for read) —
> pointer + length as one unit, zero overhead, works with any contiguous storage,
> `.size()` always correct. C-style `(ptr, len)` pairs sirf C-API boundaries pe.
> `std::string_view` (folder 10) string parameters ke liye yehi cheez.

---

## Hands-on

`examples/02_array_decay.cpp` — `sizeof` trap, `int* p = data`, size-param sum,
reference-to-array sum:

```bash
./build.ps1 09-ARRAYS/examples/02_array_decay.cpp
```

---

## ⚠️ Traps

### Trap 1 — `sizeof(param)` inside function
```cpp
void f(int a[]) { int n = sizeof(a) / sizeof(a[0]); }   // ⚠️ = 8/4 = 2
```

### Trap 2 — `void f(int a[10])` — "10" enforce hota hai socna
```cpp
void f(int a[10]);   f(smallArray3);   // ⚠️ compiles fine -- 10 ignored
```

### Trap 3 — array ko `auto` mein
```cpp
auto x = arr;        // x is int* (decayed), NOT int[N]
auto& r = arr;       // ✅ r is int(&)[N]
```

### Trap 4 — returning a local array
```cpp
int* makeArray() { int a[5] = {}; return a; }   // ⚠️ decays to pointer to dead local -> UB
```

### Trap 5 — `arr == arr2` compares pointers, not contents
```cpp
int a[3] = {1,2,3}, b[3] = {1,2,3};
if (a == b) { }      // ⚠️ address compare (always false here). std::equal / std::array ==
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`void f(int a[10])` array leta hai" | Pointer leta hai — `10` ignored |
| "`sizeof(a)` function ke andar array size" | Pointer size (8) — decay hua |
| "Array aur pointer same cheez hain" | Value same (address), type/behaviour alag |
| "`auto x = arr` array copy karta hai" | `x` = `int*` (decayed) |
| "`arr1 == arr2` contents compare" | Addresses compare karta hai |

---

## Exercises

1. **`sizeof` trap:** `int data[20]` — `main()` mein `sizeof(data)`, ek function
   `void f(int a[])` ke andar `sizeof(a)`. Dono print. `-Wall` warning?

2. **3 sum functions:** `sum(const int*, size_t)`, `template<size_t N> sum(const
   int(&)[N])`, `sum(std::span<const int>)` — teenon likho, same array pe test.

3. **Decay explicit:** `int a[5] = {1,2,3,4,5}; int* p = a;` — `p == &a[0]`?
   `*p`? `p[2]`? `sizeof(a)` vs `sizeof(p)`?

4. **`&arr` type:** `int a[10];` — `&a` ka type kya (`decltype`)? `&a + 1` kitne
   bytes aage? (`sizeof(a)` = 40).

5. **Return local array bug:** `int* bad() { int a[3] = {1,2,3}; return a; }` —
   `-Wall` warning? Chalao — `bad()[0]` kya deta hai (UB)?

6. **`std::span` fix:** ek `void scale(int arr[], int n, int factor)` ko
   `void scale(std::span<int> arr, int factor)` mein badlo. C array, `std::array`,
   `std::vector` — teenon pe call karo.

---

## Interview questions

1. Array decay kya hai? Kab hota hai, kab nahi?
2. `void f(int a[10])`, `void f(int a[])`, `void f(int* a)` — fark?
3. Function ke andar `sizeof(arrayParam)` kya deta hai aur kyun?
4. Array aur pointer — 3 concrete differences (`sizeof`, `&`, assignment)?
5. Function ko array + uska size pass karne ke 4 tareeke?
6. `std::span` decay problem ko kaise solve karta hai?

---

## Next
→ [`06-multidimensional-arrays.md`](06-multidimensional-arrays.md)
