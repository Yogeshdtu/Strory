# 11 — Baaki operators

## Prerequisites
Is folder ke lessons 01–10

## Yeh topic abhi kyun
Kuch operators bache hain jo alag categories mein nahi aate. Inhe cover karke
folder complete ho jaayega.

---

## `sizeof`

```cpp
sizeof(int)         // 4
sizeof(x)           // variable ka size
sizeof x            // brackets optional variables ke liye
sizeof(arr) / sizeof(arr[0])      // array element count (purana idiom)
```

**Folder 03 file 14 mein detail hai.** Recap:

- **Compile-time operator** — runtime pe kuch nahi karta
- Return type `std::size_t`
- Operand **evaluate nahi hota**:
  ```cpp
  int i = 5;
  sizeof(i++);        // i NAHI badhta
  ```
- Array decay ka trap:
  ```cpp
  void f(int arr[]) { sizeof(arr); }      // pointer size, array nahi
  ```

### Modern alternatives
```cpp
#include <iterator>
std::size(arr);          // C++17 -- element count
std::ssize(arr);         // C++20 -- signed count
arr.size();              // std::array, std::vector
```

---

## `sizeof...` (variadic templates)

```cpp
template <typename... Args>
void func(Args... args) {
    std::cout << sizeof...(Args);      // kitne types
    std::cout << sizeof...(args);      // kitne arguments
}
```

Folder 21 mein detail.

---

## `alignof` aur `alignas`

```cpp
alignof(int)              // 4
alignof(double)           // 8

struct alignas(64) CacheLine {
    int data[16];
};
alignof(CacheLine);       // 64
```

Folder 03 file 14 aur folder 32 mein detail.

> **HFT relevance:** `alignas(64)` false sharing rokta hai — do threads ke variables
> ko alag cache lines mein daalta hai.

---

## `typeid` aur RTTI

```cpp
#include <typeinfo>

int x = 5;
std::cout << typeid(x).name();          // "i" (mangled)
std::cout << typeid(int).name();
```

Demangle karo:
```bash
./program | c++filt -t
```

### Polymorphic types ke saath
```cpp
Base* p = new Derived();
std::cout << typeid(*p).name();         // "Derived" -- runtime pe pata chalta hai
```

⚠️ **`typeid` polymorphic types pe RTTI use karta hai** — slow hai (~20-50 ns).
HFT hot path mein nahi.

Aur `-fno-rtti` ke saath yeh kaam nahi karega. Folder 25 mein detail.

### Better alternative
```cpp
// Compile-time type info
#include <type_traits>
std::is_same_v<decltype(x), int>        // ✅ compile time, zero cost
```

---

## `decltype`

```cpp
int x = 5;
decltype(x) y = 10;         // y ka type int

std::vector<int> v;
decltype(v)::value_type item;    // int
```

Folder 03 file 12 mein basic hai, folder 21 mein deep.

---

## `noexcept` operator

Do meanings hain:

```cpp
// 1. SPECIFIER -- "yeh function throw nahi karega"
void f() noexcept { }

// 2. OPERATOR -- "kya yeh expression throw kar sakta hai?"
bool b = noexcept(f());     // true
```

Folder 23 mein detail.

---

## Cast operators

Folder 03 file 13 se recap:

```cpp
static_cast<T>(x)           // ✅ normal conversions -- 95% cases
const_cast<T>(x)            // ⚠️ const hataana
reinterpret_cast<T>(x)      // ⚠️ bits ko dobara dekhna -- khatarnaak
dynamic_cast<T>(x)          // runtime polymorphic downcast (slow)

(T)x                        // ❌ C-style -- avoid
T(x)                        // ❌ functional style -- avoid
```

Aur C++20:
```cpp
#include <bit>
std::bit_cast<T>(x);        // ✅ safe type punning, constexpr
```

---

## `new` aur `delete`

```cpp
int* p = new int(5);
delete p;

int* arr = new int[10];
delete[] arr;               // ⚠️ delete[] -- brackets zaroori!

// Placement new (folder 25)
void* buffer = ::operator new(sizeof(T));
T* obj = new (buffer) T();
obj->~T();                  // manual destructor
::operator delete(buffer);
```

⚠️ **Modern C++ mein aap yeh shayad hi likhoge** — `std::vector`, `std::unique_ptr`,
`std::make_unique` use karo (folder 17).

Folder 14 mein detail.

---

## Member access operators

```cpp
obj.member          // object se
ptr->member         // pointer se (≡ (*ptr).member)
obj.*memberPtr      // member pointer se
ptr->*memberPtr     // pointer + member pointer
::name              // global scope
Class::member       // class scope
ns::name            // namespace scope
```

Folder 12 aur 15 mein detail.

---

## Subscript `[]`

```cpp
arr[i]              // ≡ *(arr + i)  -- built-in arrays ke liye
v[i]                // std::vector ka operator[]  -- NO bounds check
v.at(i)             // bounds check karta hai, throws
```

### 🔑 Interesting fact
```cpp
arr[i]  ==  *(arr + i)  ==  *(i + arr)  ==  i[arr]
```

```cpp
int arr[] = {10, 20, 30};
std::cout << arr[1];        // 20
std::cout << 1[arr];        // 20  -- legal! (par kabhi mat likhna)
```

Kyunki `[]` addition ka syntactic sugar hai, aur addition commutative hai.

### C++23: multi-dimensional subscript
```cpp
// C++23
matrix[i, j];               // ab yeh multi-arg subscript hai
// C++20 mein: comma OPERATOR tha (deprecated warning)
```

---

## Function call `()`

```cpp
f(a, b);            // function call
obj(a, b);          // functor -- operator() overload
lambda(a, b);       // lambda call
```

Function call bhi ek **operator** hai jo overload ho sakta hai:
```cpp
struct Adder {
    int operator()(int a, int b) const { return a + b; }
};
Adder add;
add(2, 3);          // 5
```

Yeh **functors** hain — folder 19/21 mein.

---

## `co_await`, `co_yield`, `co_return` (C++20)

Coroutines ke operators. Folder 22 mein detail.

```cpp
Task f() {
    co_await something;
    co_yield value;
    co_return result;
}
```

---

## `<=>` spaceship (C++20)

File 03 mein cover ho chuka. Recap:
```cpp
struct Point {
    int x, y;
    auto operator<=>(const Point&) const = default;
    bool operator==(const Point&) const = default;
};
```

---

## Operators jo overload NAHI ho sakte

```cpp
::          // scope resolution
.           // member access
.*          // member pointer access
?:          // ternary
sizeof
alignof
typeid
noexcept
co_await    // (technically ho sakta hai, par alag tareeke se)
```

Baaki sab overload ho sakte hain. Folder 15 mein detail.

---

## Complete operator list (reference)

```
Arithmetic:      +  -  *  /  %  ++  --
Comparison:      ==  !=  <  >  <=  >=  <=>
Logical:         &&  ||  !
Bitwise:         &  |  ^  ~  <<  >>
Assignment:      =  +=  -=  *=  /=  %=  &=  |=  ^=  <<=  >>=
Member access:   .  ->  .*  ->*  []  ::
Other:           ()  ,  ?:  sizeof  alignof  typeid  noexcept
Memory:          new  new[]  delete  delete[]
Casts:           static_cast  dynamic_cast  const_cast  reinterpret_cast
Coroutine:       co_await  co_yield  co_return
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`sizeof` runtime pe chalta hai" | Compile-time operator |
| "`typeid` free hai" | Polymorphic types pe RTTI lookup — slow |
| "`arr[i]` sirf arrays ke liye hai" | `*(arr + i)` ka sugar — `i[arr]` bhi legal |
| "Saare operators overload ho sakte hain" | `::` `.` `?:` `sizeof` nahi |
| "`delete` aur `delete[]` same hain" | ❌ Mix karna UB hai |

---

## Exercises

1. `i[arr]` test karo:
   ```cpp
   int arr[] = {10, 20, 30};
   std::cout << arr[1] << " " << 1[arr] << "\n";
   ```

2. `sizeof` evaluate nahi karta — verify karo:
   ```cpp
   int i = 5;
   std::cout << sizeof(i++) << " " << i << "\n";
   ```

3. `typeid` try karo aur demangle karo:
   ```bash
   ./program | c++filt -t
   ```

4. Ek functor banao:
   ```cpp
   struct Multiplier {
       int factor;
       int operator()(int x) const { return x * factor; }
   };
   Multiplier times3{3};
   std::cout << times3(7);      // 21
   ```

5. `std::size` vs `sizeof` idiom compare karo:
   ```cpp
   int arr[10];
   std::cout << sizeof(arr)/sizeof(arr[0]) << " " << std::size(arr) << "\n";
   ```

6. `alignof` test karo aur `alignas(64)` ka asar dekho.

---

## Next
→ [`12-exercises.md`](12-exercises.md)
