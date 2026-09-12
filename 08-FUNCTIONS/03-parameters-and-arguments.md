# 03 — Parameters aur arguments — pass by value, reference, pointer

## Prerequisites
- [`02-declaration-vs-definition.md`](02-declaration-vs-definition.md)
- `07-LOOPS/04-range-based-for.md` (copy vs reference, ~50x cost measured)
- `05-OPERATORS/08-ternary-operator.md` mein references chhue the

## Yeh topic abhi kyun
"Function ko data kaise doon?" — 3 tareeke hain: **value** (copy), **reference**
(alias), **pointer** (address). Har ek ka alag matlab: kaun modify kar sakta hai,
kitna copy hota hai, `nullptr` possible hai ya nahi. Galat choice = slow code
(bade objects ki copy) ya bug (galti se original badal diya).

Yeh references (folder 13) aur pointers (folder 12) ka pehla proper parichay bhi hai.

---

## Shabd: parameter vs argument

```cpp
int add(int a, int b) { return a + b; }   // a, b = PARAMETERS  (function ke locals)

add(3, 4);                                 // 3, 4 = ARGUMENTS   (call pe pass ki values)
```

"Parameter" = function ki definition mein. "Argument" = call site pe. (Roz-marra
mein log dono ko mila dete hain — theek hai, par interview mein fark pata ho.)

---

## 1. Pass by VALUE — copy

```cpp
void increment(int x) {     // x = argument ki COPY
    x = x + 1;              // sirf copy badli
}

int n = 5;
increment(n);
std::cout << n;             // 5 -- unchanged
```

- Function ko ek **apni copy** milti hai
- Andar kuch bhi karo — **caller ka original safe**
- Har call pe copy ki cost

### Copy ki cost

| Type | Copy cost |
|---|---|
| `int`, `double`, `char`, pointer | ~free (ek register) |
| `std::string` (long) | heap allocation + memcpy |
| `std::vector<T>` | heap allocation + har element copy |
| Bada `struct` (100 bytes) | 100 bytes memcpy |

Folder 07 file 04 mein measure kiya: `std::string` ki bekaar copy → **~50x**
slower loop.

**Rule: chhoti trivial types → by value. Bade objects → by `const&` (neeche).**

---

## 2. Pass by REFERENCE — alias (`&`)

```cpp
void increment(int& x) {    // x IS the caller's variable (naya naam)
    x = x + 1;
}

int n = 5;
increment(n);
std::cout << n;             // 6 -- badal gaya!
```

`int& x` — `x` koi copy nahi, `n` ka doosra naam. Andar `x` badalna = `n`
badalna. Koi copy nahi banti.

### `const&` — no copy, no modify (bade objects ke liye DEFAULT)

```cpp
double average(const std::vector<int>& v) {   // koi copy nahi; v badal nahi sakte
    long long s = 0;
    for (int x : v) s += x;
    return static_cast<double>(s) / static_cast<double>(v.size());
}

std::vector<int> big(1'000'000);
average(big);              // vector copy NAHI hua -- sirf ek address pass hua
```

`const std::vector<int>& v`:
- **No copy** — bas reference (practically ek pointer, 8 bytes)
- **`const`** — function `v` ko modify nahi kar sakta (compiler enforce karta hai)
- Caller ko guarantee: mera data safe hai

### Decision table

| Kya chahiye | Parameter type |
|---|---|
| Chhoti value, read-only | `int x` (by value) |
| Bada object, read-only | `const BigType& x` |
| Caller ka object modify karna hai | `BigType& x` (non-const ref) |
| "Optional" — ho bhi sakta hai, na bhi | `const BigType* x` (pointer, nullptr allowed) ya `std::optional` |
| Function ko apni copy chahiye (usse modify karega, caller ka safe rahe) | `BigType x` (by value) — aur `std::move` se pass |

**Sabse common default: `const T&` for anything bigger than a pointer.**

---

## 3. Pass by POINTER — address (`*`)

```cpp
void increment(int* p) {   // p = ek ADDRESS
    if (p == nullptr) return;   // ⚠️ pointer nullptr ho sakta hai -- check!
    *p = *p + 1;            // *p = "jis pe p point karta hai"
}

int n = 5;
increment(&n);             // &n = "n ka address"
std::cout << n;            // 6
```

Reference jaisa effect (original modify hota hai), par:

| | Reference `T&` | Pointer `T*` |
|---|---|---|
| `nullptr` ho sakta hai? | **Nahi** — hamesha kisi valid object ko bind | **Haan** — `nullptr` check karna padta hai |
| Re-bind (kisi aur pe point)? | Nahi | Haan |
| Syntax | `x` (seedha) | `*p` (dereference) |
| Call site | `f(n)` (dikhta nahi ki modify hoga) | `f(&n)` (`&` se hint milta hai) |
| Kab use | default, jab object hamesha maujood ho | optional param, ya C API, ya array + size |

Modern C++ mein **references prefer** karo. Pointer tab jab: `nullptr` meaningful
ho, C API se baat karni ho, ya array pass karna ho.

Poora pointers folder 12 mein, references folder 13 mein.

---

## Special cases

### Array parameter → pointer decay (folder 09 preview)

```cpp
void process(int arr[], int size);   // ⚠️ `int arr[]` yahan `int* arr` ban jaata hai
                                     //    size ALAG se pass karna padta hai -- array apna size "bhool" jaata hai
```

`void process(int* arr, std::size_t size)` — same cheez, honest syntax. Better:
`std::span<int>` (folder 09/19) — pointer + size ek saath.

### `const` value parameter — caller ko farq nahi

```cpp
int f(const int x);        // caller ke liye `int f(int x)` jaisa hi -- x copy hai to const kya
int f(int x);              // ⚠️ yeh SAME declaration hai (top-level const ignore hota hai)
```

`const` value param sirf function-body ke andar "main isse nahi badlunga" ka
signal hai — declaration mein likhne ki zaroorat nahi.

### `std::string_view` — string parameters ke liye

```cpp
void log(std::string_view msg);      // ✅ koi copy nahi, char* / std::string / literal sab chalega
void log(const std::string& msg);    // ⚠️ literal / char* pass karne pe temp std::string banega
```

`std::string_view` = pointer + length, no ownership, no copy. String **read-only
parameters** ke liye best (folder 10).

---

## Andar kya hota hai

- **By value**: argument ka data function ke stack frame / register mein copy
  hota hai (lesson 05). Bade object → memcpy / element-wise copy.
- **By reference / pointer**: sirf **address** (8 bytes) pass hota hai — ek
  register. Function `*` / indirect access se caller ke data ko chhuta hai.
- `-O2` pe: chhoti functions inline hone pe yeh copy/indirection **gayab** ho
  jaata hai — compiler seedha caller ke data pe kaam karta hai.

> **HFT relevance:** Hot path functions bade messages/structs ko **`const T&`**
> (ya `std::string_view` / `std::span`) se lete hain — ek accidental `Order` ya
> `MarketDataMsg` copy = memcpy + shayad allocator hit. Aur jahan function ko
> genuinely apni copy chahiye (buffer mein rakhna), **`T` by value + `std::move`**
> (folder 18) — copy elision se free. Rule: pointer-size se bada = `const&` by
> default.

---

## Hands-on

```bash
./build.ps1 08-FUNCTIONS/examples/01_first_functions.cpp
./build.ps1 fast 07-LOOPS/examples/02_range_based.cpp   # copy vs reference cost
```

Ek program likho: `void byVal(std::vector<int> v)`, `void byRef(std::vector<int>&
v)`, `void byConstRef(const std::vector<int>& v)` — teenon 1M-element vector pass
karo, `<chrono>` se time compare karo.

---

## ⚠️ Traps

### Trap 1 — bada object by value
```cpp
double sum(std::vector<int> v);      // ⚠️ har call pe poora vector copy
double sum(const std::vector<int>& v);   // ✅
```

### Trap 2 — modify karna tha, `const&` / value le liya
```cpp
void sortIt(std::vector<int> v) { std::sort(v.begin(), v.end()); }  // ⚠️ copy sort hui, caller ka nahi
void sortIt(std::vector<int>& v) { ... }   // ✅
```

### Trap 3 — pointer param, `nullptr` check nahi
```cpp
void f(int* p) { *p = 1; }            // 💥 p == nullptr -> crash
void f(int* p) { if (p) *p = 1; }     // ✅  (ya reference use karo)
```

### Trap 4 — reference to temporary
```cpp
const std::string& name = getName();   // getName() by-value lautta -> temp
// C++: const& temporary ki life extend karta hai (yahan safe)
// par: struct member const& = dangling. Aur function return const& to local = UB (lesson 06)
```

### Trap 5 — `const` value param declaration mein
```cpp
void f(const int x);   // caller ke liye bekaar; aur `void f(int x)` se conflict nahi (same decl)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "By value bhi original badal sakta hai" | Nahi — copy. Reference/pointer se badalta hai |
| "Reference aur pointer same" | Ref: no null, no rebind, seedha syntax. Ptr: null OK, rebind, `*` |
| "`const&` slow hai (indirection)" | Bade objects pe bahut fast (no copy); `-O2` aksar hata deta hai |
| "String param `const std::string&` best" | `std::string_view` — literal/char* pe temp nahi banta |
| "Array pass karne pe size bhi jaata hai" | Nahi — decay to pointer; size alag pass karo / `std::span` |

---

## Exercises

1. **swap:** `void swap(int& a, int& b)` likho (reference se). Phir pointer version
   `void swap(int* a, int* b)`. Dono test karo.

2. **Cost measure:** 1M-element `std::vector<int>` — `byValue`, `byRef`,
   `byConstRef` versions, `<chrono>` se time. Ratio likho.

3. **Modify:** `void doubleAll(std::vector<int>& v)` — har element 2x. `const&` se
   likhne ki koshish karo — error kya?

4. **nullptr:** `int lengthOr0(const char* s)` — `s == nullptr` pe `0`, warna
   `std::strlen(s)`. `nullptr` aur `"hello"` dono pass karo.

5. **string_view:** `void greet(std::string_view name)` — pass karo: string
   literal, `std::string`, `const char*`. Sab bina copy? Ab `const std::string&`
   version se compare — literal pass pe kya extra hua?

6. **out-parameter:** `bool parseInt(std::string_view s, int& out)` — parse
   success/fail return karo, value `out` mein. Use karo.

7. **`std::span`:** `long long sum(std::span<const int> data)` — `std::vector`,
   `std::array`, aur C-array teenon pass karo. Ek hi function sab pe kaam karta hai?

---

## Interview questions

1. Parameter aur argument mein fark?
2. Pass by value / reference / pointer — teenon ka behaviour aur kab kaunsa?
3. `const T&` parameter ka kya faayda bade objects pe?
4. Reference aur pointer parameter mein 3 fark?
5. `std::string_view` vs `const std::string&` parameter — kab farq padta hai?
6. Array parameter ke saath kya hota hai (decay)? Size kaise dete ho?
7. Function ko apni copy chahiye ho jise woh modify karega — kaise pass karo?

---

## Next
→ [`04-return-values.md`](04-return-values.md)
