# 12 — `auto` aur type deduction

## Prerequisites
`11-const-and-constexpr.md`

## Yeh topic abhi kyun
`auto` C++11 ka sabse zyada use hone wala feature hai. Sahi use se code saaf hota hai,
galat use se confusing.

---

## `auto` kya karta hai?

**`auto` = "compiler, tu khud type nikal le."**

```cpp
auto a = 5;              // int
auto b = 3.14;           // double
auto c = 'x';            // char
auto d = "hello";        // const char*
auto e = true;           // bool
auto f = 5u;             // unsigned int
auto g = 5L;             // long
```

**Important:** Yeh **dynamic typing nahi hai.** Type compile time pe fix ho jaata hai
aur kabhi badalta nahi.

```cpp
auto x = 5;      // x hamesha int rahega
x = "hello";     // ❌ ERROR
```

---

## `auto` ke saath initialization ZAROORI hai

```cpp
auto x;          // ❌ ERROR: type deduce nahi kar sakte
auto y = 5;      // ✅
```

Yeh actually ek **fayda** hai — `auto` uninitialized variables rok deta hai.

---

## `auto` kab use karein? ✅

### 1. Lambe types (sabse bada use case)

```cpp
// ❌ Bina auto
std::map<std::string, std::vector<int>>::const_iterator it = myMap.begin();

// ✅ auto ke saath
auto it = myMap.begin();
```

### 2. Range-based for loops

```cpp
std::vector<Order> orders;

for (const auto& order : orders) { }     // ✅
for (auto& order : orders) { }           // ✅ modify karna ho to
for (auto order : orders) { }            // ⚠️ COPY banti hai -- mehngi!
```

### 3. Lambdas

```cpp
auto lambda = [](int x) { return x * 2; };    // ✅ lambda ka type likha nahi ja sakta
```

Lambda ka type compiler generate karta hai aur uska naam aapko nahi pata. `auto` hi
ekmatra tareeka hai (ya `std::function`, jo slow hai).

### 4. Template code

```cpp
template <typename T, typename U>
auto add(T a, U b) {
    return a + b;         // return type compiler nikaal lega
}
```

### 5. Jab type obvious ho

```cpp
auto orderBook = std::make_unique<OrderBook>();     // ✅ clear
auto count = static_cast<std::size_t>(n);           // ✅ clear
```

---

## `auto` kab NAHI use karein? ❌

### 1. Jab type clarity important ho

```cpp
auto price = getPrice();       // ❌ int hai? double hai? int64_t hai?
std::int64_t price = getPrice();  // ✅ clear
```

Financial code mein type matter karta hai. Explicit rakho.

### 2. Jab aap specific type chahte ho

```cpp
auto x = 5;              // int
float y = 5;             // ✅ agar float chahiye to likho
```

### 3. Simple types mein

```cpp
auto i = 0;              // ⚠️ `int i = 0;` zyada clear hai
```

---

## ⚠️ `auto` ke traps

### Trap 1: `auto` `const` aur `&` **gira deta hai**

```cpp
const std::vector<int> v = {1, 2, 3};

auto a = v;              // ⚠️ type: std::vector<int>  (const GAYAB!)
                         //    aur ek pura COPY bani!
auto& b = v;             // ✅ const std::vector<int>&
const auto& c = v;       // ✅ const std::vector<int>&  (clearest)
```

**Rule:** `auto` value se copy banata hai. Reference chahiye to `auto&` ya `const auto&`
likho.

### Trap 2: Loop mein accidental copies

```cpp
std::vector<std::string> names = { /* 1 million strings */ };

for (auto name : names) { }          // ⚠️ har iteration mein ek STRING COPY!
for (const auto& name : names) { }   // ✅ koi copy nahi
```

**Yeh performance ka bada difference hai.** Bade objects ke liye hamesha `const auto&`.

> **HFT relevance:** Ek accidental copy hot path mein allocation trigger kar sakti hai
> (string, vector). Woh 100-10000 ns kha jaayegi. Isliye HFT code reviews mein
> `for (auto x : container)` red flag hota hai.

### Trap 3: `auto` aur proxy objects

```cpp
std::vector<bool> v = {true, false};
auto x = v[0];                       // ⚠️ x ka type bool NAHI hai!
                                     //    Woh vector<bool>::reference proxy hai
```

(Yaad hai file 08 ka `vector<bool>` trap?)

Same problem `std::valarray`, Eigen matrices, aur expression templates ke saath.

**Solution:** `auto x = static_cast<bool>(v[0]);` ya seedha `bool x = v[0];`

### Trap 4: `auto` string literals ke saath

```cpp
auto s = "hello";                    // const char*, NOT std::string
s.size();                            // ❌ error

auto s2 = std::string{"hello"};      // ✅ std::string
using namespace std::string_literals;
auto s3 = "hello"s;                  // ✅ std::string (s suffix, C++14)
```

### Trap 5: Integer division

```cpp
auto result = 7 / 2;                 // int -> 3, not 3.5
auto result2 = 7.0 / 2;              // double -> 3.5
```

`auto` aapki galti nahi pakadta — woh bas type deduce karta hai.

---

## `auto` ke variants

```cpp
auto        x = expr;      // value (const aur & gir jaate hain)
auto&       x = expr;      // lvalue reference
const auto& x = expr;      // const reference (read-only access)
auto&&      x = expr;      // forwarding reference (folder 18 mein)
auto*       x = expr;      // pointer (documents intent)
```

### `auto*`
```cpp
int value = 5;
auto  p1 = &value;         // int*  (kaam karta hai)
auto* p2 = &value;         // int*  (clearer -- "yeh pointer hai")
```

---

## `decltype` — "iska type kya hai?"

```cpp
int x = 5;
decltype(x) y = 10;              // y ka type int hai

std::vector<int> v;
decltype(v)::value_type item;    // int
```

### `decltype` vs `auto`

```cpp
const int  ci = 5;
const int& cr = ci;

auto     a = cr;        // int          (const aur & gir gaye)
decltype(cr) b = ci;    // const int&   (EXACT type)
```

`decltype` **exact type** deta hai, `auto` "value semantics" wala type.

### `decltype(auto)` (C++14)

```cpp
decltype(auto) func() {
    return someExpression;      // exact type preserve karta hai (references bhi)
}
```

Yeh perfect forwarding mein use hota hai — folder 18/21 mein.

---

## Structured bindings (C++17) — `auto` ka best friend

```cpp
std::map<std::string, int> prices = {{"NIFTY", 21500}, {"BANKNIFTY", 46000}};

// ❌ Purana tareeka
for (const auto& pair : prices) {
    std::cout << pair.first << " = " << pair.second << "\n";
}

// ✅ Structured bindings
for (const auto& [symbol, price] : prices) {
    std::cout << symbol << " = " << price << "\n";
}
```

Multiple return values ke liye bhi:
```cpp
std::pair<bool, int> tryParse(const std::string& s);

auto [success, value] = tryParse("123");
if (success) { /* use value */ }
```

Structs ke saath bhi:
```cpp
struct Point { int x, y; };
Point p{10, 20};
auto [px, py] = p;         // px = 10, py = 20
```

---

## Trailing return type

```cpp
// C++11 style
template <typename T, typename U>
auto add(T a, U b) -> decltype(a + b) {
    return a + b;
}

// C++14 se simpler
template <typename T, typename U>
auto add(T a, U b) {
    return a + b;
}
```

---

## Style guidelines

### AAA (Almost Always Auto) — Herb Sutter ka style
```cpp
auto count = 0;
auto name = std::string{"Rahul"};
auto price = 100.5;
```
Consistent hai, par kuch log ko readable nahi lagta.

### Conservative style (zyada common)
```cpp
int count = 0;                            // simple types explicit
auto it = container.begin();              // lambe types auto
const auto& item = getItem();             // references
```

**Meri salah (aur is course ka style):**
- Simple types → explicit (`int`, `double`, `std::int64_t`)
- Iterators, lambdas, templates → `auto`
- Loops → `const auto&` (ya `auto&` agar modify karna ho)
- Financial/protocol values → **hamesha explicit** (`std::int64_t price`)

---

## Hands-on

```bash
cd ~/cpp-practice
cat > autodemo.cpp << 'END'
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <typeinfo>

int main() {
    std::cout << "===== BASIC DEDUCTION =====\n";
    auto a = 5;           // int
    auto b = 3.14;        // double
    auto c = 'x';         // char
    auto d = "hello";     // const char*
    auto e = true;        // bool

    std::cout << "sizeof(a=5)     = " << sizeof(a) << " (int)\n";
    std::cout << "sizeof(b=3.14)  = " << sizeof(b) << " (double)\n";
    std::cout << "sizeof(c='x')   = " << sizeof(c) << " (char)\n";
    std::cout << "sizeof(d=\"hi\") = " << sizeof(d) << " (const char* -- POINTER!)\n";
    std::cout << "sizeof(e=true)  = " << sizeof(e) << " (bool)\n";

    std::cout << "\n===== CONST/REF DROPPED =====\n";
    const std::vector<int> v = {1, 2, 3};
    auto copy = v;              // COPY! const gir gaya
    // copy.push_back(4);       // ✅ chalta hai -- const nahi raha
    const auto& ref = v;        // ✅ reference, no copy
    std::cout << "auto copy = v;      -> ek poori COPY bani\n";
    std::cout << "const auto& ref = v; -> koi copy nahi\n";

    std::cout << "\n===== LOOP COPIES =====\n";
    std::vector<std::string> names = {"Rahul", "Priya", "Amit"};

    std::cout << "for (auto n : names)       -> har baar STRING COPY\n";
    std::cout << "for (const auto& n : names) -> koi copy nahi  <- YEH USE KARO\n";

    for (const auto& n : names) std::cout << "  " << n << "\n";

    std::cout << "\n===== STRUCTURED BINDINGS =====\n";
    std::map<std::string, int> prices = {{"NIFTY", 21500}, {"BANKNIFTY", 46000}};
    for (const auto& [symbol, price] : prices) {
        std::cout << "  " << symbol << " = " << price << "\n";
    }

    std::cout << "\n===== LAMBDA =====\n";
    auto doubler = [](int x) { return x * 2; };
    std::cout << "  doubler(21) = " << doubler(21) << "\n";

    std::cout << "\n===== TRAP: STRING LITERAL =====\n";
    auto s1 = "hello";                     // const char*
    auto s2 = std::string{"hello"};        // std::string
    std::cout << "  auto s = \"hello\"          -> const char* (.size() nahi chalega)\n";
    std::cout << "  auto s = std::string{...} -> std::string, size = " << s2.size() << "\n";

    std::cout << "\n===== TRAP: INTEGER DIVISION =====\n";
    auto r1 = 7 / 2;
    auto r2 = 7.0 / 2;
    std::cout << "  auto r = 7 / 2    -> " << r1 << " (int division!)\n";
    std::cout << "  auto r = 7.0 / 2  -> " << r2 << "\n";

    std::cout << "\n===== decltype =====\n";
    int x = 5;
    decltype(x) y = 10;
    std::cout << "  decltype(x) y = 10;  -> y ka type int hai, value " << y << "\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra autodemo.cpp -o autodemo && ./autodemo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`auto` dynamic typing hai" | Nahi! Type compile time pe fix hota hai |
| "`auto` slow hai" | Zero runtime cost — sirf compile-time deduction |
| "`auto x = v` reference banata hai" | ❌ **Copy** banata hai. `auto&` reference deta hai |
| "`auto` `const` preserve karta hai" | Nahi, gira deta hai (top-level) |
| "`auto s = "text"` `std::string` deta hai" | `const char*` deta hai |

---

## Exercises

1. Har `auto` ka type batao:
   ```cpp
   auto a = 5;
   auto b = 5.0;
   auto c = 5.0f;
   auto d = 'c';
   auto e = "text";
   auto f = std::string("text");
   auto g = true;
   auto h = 5u;
   auto i = {1, 2, 3};
   ```
   <details><summary>Answers</summary>
   a) `int`  b) `double`  c) `float`  d) `char`  e) `const char*`
   f) `std::string`  g) `bool`  h) `unsigned int`  i) `std::initializer_list<int>` ⚠️
   </details>

2. Copy vs reference test — timing karo:
   ```cpp
   std::vector<std::string> big(1000000, "some long string here");
   
   auto t1 = std::chrono::steady_clock::now();
   for (auto s : big) { volatile auto len = s.size(); }
   auto t2 = std::chrono::steady_clock::now();
   for (const auto& s : big) { volatile auto len = s.size(); }
   auto t3 = std::chrono::steady_clock::now();
   ```
   Kitna fark aaya?

3. Yeh code fix karo:
   ```cpp
   const std::vector<int> data = {1, 2, 3};
   auto d = data;
   d.push_back(4);        // original ko modify karna tha, par nahi hua!
   ```
   <details><summary>Answer</summary>
   `data` `const` hai — usko modify kar hi nahi sakte. Agar modify karna hai to
   `const` hataana padega. `auto d = data` ne ek copy banayi, isliye
   `push_back` copy pe hua, original pe nahi.
   </details>

4. Structured bindings se ek `std::pair` return karne wala function likho aur use karo.

5. `vector<bool>` trap:
   ```cpp
   std::vector<bool> v = {true, false};
   auto x = v[0];
   // x ka type kya hai? bool nahi!
   std::cout << typeid(x).name() << "\n";
   ```

6. `decltype` vs `auto` compare karo:
   ```cpp
   const int ci = 5;
   const int& cr = ci;
   auto a = cr;
   decltype(cr) b = ci;
   // a aur b ke types alag hain -- kaise verify karoge?
   ```

---

## Interview questions

1. `auto` kya karta hai? Woh dynamic typing hai?
2. `auto x = v;` aur `auto& x = v;` mein fark?
3. `auto` `const` preserve karta hai?
4. `decltype` aur `auto` mein fark?
5. `for (auto x : v)` mein kya problem ho sakti hai?
6. Structured bindings kya hain?

---

## Next
→ [`13-type-conversions.md`](13-type-conversions.md)
