# 10 — Initialization forms

## Prerequisites
`03-declaration-definition-initialization.md`, `05-int-deep-dive.md`

## Yeh topic abhi kyun
C++ mein variable initialize karne ke **6+ tareeke** hain. Yeh confusing hai. Lekin
inme se ek clearly best hai — aur woh **narrowing bugs se bachata hai**.

---

## Saare tareeke

```cpp
int a = 5;          // 1. copy initialization
int b(5);           // 2. direct initialization
int c{5};           // 3. direct-list initialization (brace)     ⭐ BEST
int d = {5};        // 4. copy-list initialization
int e{};            // 5. value initialization -> 0
int f = int(5);     // 6. functional cast style
int g;              // 7. default initialization -> ⚠️ GARBAGE (local)
auto h = 5;         // 8. auto deduction
```

---

## `{}` best kyun hai? — NARROWING

Yeh sabse important reason hai.

```cpp
int a = 3.99;       // ✅ compile ho jaata hai, a = 3 (SILENT data loss!)
int b(3.99);        // ✅ compile ho jaata hai, b = 3 (SILENT data loss!)
int c{3.99};        // ❌ COMPILE ERROR: narrowing conversion
```

**`{}` narrowing conversions ko rok deta hai.** Yeh feature hai, bug nahi.

### Aur examples

```cpp
int x = 300;
char a = x;         // ✅ compile hota hai, a = 44 (300 % 256) -- SILENT BUG
char b{x};          // ❌ error: narrowing conversion

double d = 3.14;
float f1 = d;       // ✅ precision loss, silent
float f2{d};        // ❌ error (double -> float narrowing)

unsigned u = -1;    // ✅ 4294967295 -- silent
unsigned v{-1};     // ❌ error
```

### Narrowing kya hai?

Ek conversion narrowing hai agar:
- Floating point → integer
- Bade type se chhote type mein (agar value fit na kare)
- Integer → floating point (agar exactly represent na ho sake)
- Signed ↔ unsigned (agar value negative ho ya range mein na aaye)

**Exception:** Agar value **compile-time constant** hai aur exactly fit ho jaati hai:
```cpp
char a{100};        // ✅ OK -- 100 char mein fit ho jaata hai, compile time pe pata hai
char b{300};        // ❌ error -- fit nahi hota
```

---

## `{}` — value initialization

```cpp
int a{};            // 0
double b{};         // 0.0
bool c{};           // false
char d{};           // '\0'
int* e{};           // nullptr
std::string f{};    // "" (empty string)
```

**Yeh `= 0` se behtar hai** kyunki yeh **har type** ke liye kaam karta hai — templates
mein bahut useful:

```cpp
template <typename T>
void func() {
    T value{};      // ✅ har type ke liye sensible default
    // T value = 0; // ❌ std::string ke liye kaam nahi karega
}
```

---

## ⚠️ The Most Vexing Parse

```cpp
int a();            // ⚠️ yeh VARIABLE nahi hai!
                    //    yeh ek FUNCTION declaration hai:
                    //    "a ek function hai jo kuch nahi leta aur int return karta hai"
```

Yeh classic C++ gotcha hai. Aur classes ke saath aur bura hota hai:

```cpp
class Timer { public: Timer() {} };

Timer t1;           // ✅ object banaya
Timer t2();         // ⚠️ FUNCTION declaration! (Most Vexing Parse)
Timer t3{};         // ✅ object banaya -- braces se koi ambiguity nahi
```

```cpp
std::string s(std::string());     // ⚠️ function declaration!
std::string s{std::string{}};     // ✅ object
```

**`{}` se yeh problem hi nahi aati.** Ek aur reason braces use karne ka.

---

## ⚠️ `{}` ka apna gotcha: `std::vector`

Braces perfect nahi hain. `initializer_list` ke saath ek surprise hai:

```cpp
std::vector<int> v1(5, 10);     // 5 elements, har ek 10:  {10,10,10,10,10}
std::vector<int> v2{5, 10};     // 2 elements:              {5, 10}
```

**Bilkul alag results!**

**Rule:** `{}` initializer_list ko **prefer** karta hai. Agar constructor mein
`initializer_list` version hai, woh chuna jaayega.

```cpp
std::vector<int> v3(5);          // 5 elements, sab 0
std::vector<int> v4{5};          // 1 element: {5}
```

Isliye **containers ke liye** dhyaan se socho ki `()` chahiye ya `{}`.

---

## Recommended style

```cpp
// ✅ Default: brace initialization
int count{0};
double price{100.50};
std::string name{"Rahul"};
std::vector<int> numbers{1, 2, 3, 4, 5};

// ✅ Jab constructor arguments dene hon (size, capacity, etc.)
std::vector<int> buffer(1000);           // 1000 elements
std::vector<int> filled(10, 42);         // 10 elements, sab 42

// ✅ auto ke saath = use karo
auto x = 5;
auto name = std::string{"Rahul"};

// ✅ Value initialization
int counter{};                            // 0
```

### Google/LLVM style vs Modern C++ style

Kuch codebases `=` prefer karti hain readability ke liye:
```cpp
int count = 0;
```

Kuch `{}` prefer karti hain safety ke liye:
```cpp
int count{0};
```

**Dono theek hain.** Team ki convention follow karo. Agar aapka choice hai — `{}` lo,
kyunki narrowing protection free milti hai.

---

## Aggregate initialization (structs/arrays)

```cpp
struct Point { int x; int y; };

Point p1{10, 20};                   // ✅ aggregate init
Point p2 = {10, 20};                // ✅ same
Point p3{};                         // ✅ dono 0
Point p4{10};                       // ✅ x=10, y=0 (baaki value-initialized)

int arr1[5]{1, 2, 3, 4, 5};         // ✅
int arr2[5]{};                      // ✅ sab 0
int arr3[5]{1, 2};                  // ✅ {1, 2, 0, 0, 0}
```

### Designated initializers (C++20) ⭐

```cpp
struct Order {
    std::uint64_t id;
    std::int64_t  price;
    std::uint32_t quantity;
};

Order o{.id = 12345, .price = 21500, .quantity = 100};    // ✅ C++20
```

**Bahut readable hai** — khaas kar bade structs mein.

⚠️ C++ mein rules C se strict hain:
- Order same hona chahiye jaise struct mein
- Skip kar sakte ho, par re-order nahi

```cpp
Order o1{.id = 1, .quantity = 100};              // ✅ price skip kiya (0 ho jaayega)
Order o2{.quantity = 100, .id = 1};              // ❌ error: order galat hai
```

---

## Class members ki initialization (preview)

```cpp
class Order {
    std::uint64_t id_{0};              // ✅ default member initializer (C++11)
    std::int64_t  price_{0};
    std::uint32_t quantity_{0};

public:
    Order() = default;                 // sab members apne defaults se initialize honge

    Order(std::uint64_t id, std::int64_t price)
        : id_{id}, price_{price} {}    // ✅ member initializer list
};
```

Detail folder 15 mein.

---

## Hands-on

`examples/05_initialization.cpp` chalao:

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 05_initialization.cpp -o init && ./init
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Sab initialization forms same hain" | `{}` narrowing rokta hai, baaki nahi |
| "`int a();` ek variable hai" | ❌ Function declaration hai (Most Vexing Parse) |
| "`vector<int> v{5,10}` = 5 elements" | ❌ 2 elements: `{5, 10}` |
| "`int a;` mein a = 0" | Local mein garbage |
| "`{}` hamesha best hai" | Containers ke size arguments mein `()` chahiye |

---

## Exercises

1. Kaunse compile honge, kaunse nahi?
   ```cpp
   a) int a = 3.99;
   b) int b{3.99};
   c) char c{100};
   d) char d{300};
   e) unsigned e{-1};
   f) double f{3};
   g) float g{3.14};
   h) float h{3.14159265358979};
   ```
   <details><summary>Answers</summary>
   a) ✅ (a=3, silent loss)
   b) ❌ narrowing
   c) ✅ (100 char mein fit hai, compile-time constant)
   d) ❌ narrowing (300 fit nahi hota)
   e) ❌ narrowing (negative → unsigned)
   f) ✅ (int → double exact hai)
   g) ✅ (3.14 double literal hai, par... actually depends on compiler;
      GCC allow karta hai kyunki value exactly representable maani jaati hai —
      strictly standard mein yeh narrowing hai. `3.14f` likhna safe hai)
   h) ❌ narrowing (double → float, precision loss)
   </details>

2. Vector trap test karo:
   ```cpp
   std::vector<int> v1(5, 10);
   std::vector<int> v2{5, 10};
   std::cout << v1.size() << " " << v2.size() << "\n";
   for (int x : v1) std::cout << x << " "; std::cout << "\n";
   for (int x : v2) std::cout << x << " "; std::cout << "\n";
   ```

3. Most Vexing Parse experience karo:
   ```cpp
   #include <iostream>
   struct Timer { Timer() { std::cout << "Timer bana\n"; } };
   int main() {
       Timer t1;
       Timer t2();
       Timer t3{};
       std::cout << "done\n";
   }
   ```
   Kitne "Timer bana" print hue? Kyun?
   <details><summary>Answer</summary>
   **2** — `t1` aur `t3`. `Timer t2();` ek function declaration hai, object nahi.
   `-Wall` `-Wvexing-parse` (Clang) warning deta hai.
   </details>

4. Designated initializers try karo (C++20):
   ```cpp
   struct Config { int width; int height; bool fullscreen; };
   Config c{.width = 1920, .height = 1080, .fullscreen = true};
   ```

5. Value initialization test:
   ```cpp
   int a{}; double b{}; bool c{}; char d{}; int* e{};
   std::cout << a << " " << b << " " << c << " " 
             << static_cast<int>(d) << " " << e << "\n";
   ```

6. Narrowing bug dhoondho aur `{}` se fix karo:
   ```cpp
   double price = 100.75;
   int roundedPrice = price;       // silent truncation
   ```

---

## Interview questions

1. `int a = 5;`, `int b(5);`, `int c{5};` mein kya fark hai?
2. Narrowing conversion kya hai? `{}` usse kaise bachata hai?
3. Most Vexing Parse kya hai?
4. `std::vector<int> v(5, 10)` aur `v{5, 10}` mein fark?
5. Value initialization kya hai?

---

## Next
→ [`11-const-and-constexpr.md`](11-const-and-constexpr.md)
