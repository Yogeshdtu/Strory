# 08 — `std::format` (C++20) aur `std::print` (C++23)

## Prerequisites
`07-manipulators.md`

## Yeh topic abhi kyun
Manipulators verbose hain, sticky hain, aur galtiyan karwaate hain. `std::format`
**modern, type-safe, aur clean** tareeka hai.

Agar aapka compiler support karta hai — **yahi use karo**.

---

## Support check

```cpp
#include <version>
#if __cpp_lib_format >= 201907L
    // std::format available hai
#endif
```

| Compiler | `std::format` |
|---|---|
| GCC 13+ | ✅ |
| Clang 17+ | ✅ |
| MSVC 19.29+ | ✅ |
| Purane | ❌ — [fmtlib](https://github.com/fmtlib/fmt) use karo (same API) |

```bash
g++ --version        # 13 ya upar chahiye
```

Agar nahi hai, `fmt` library install karo — API bilkul same hai:
```cpp
#include <fmt/format.h>
fmt::format("{}", x);      // std::format jaisa hi
```

---

## Basic

```cpp
#include <format>
#include <iostream>

int main() {
    std::string s = std::format("Hello, {}!", "World");
    std::cout << s << "\n";

    std::cout << std::format("{} + {} = {}\n", 2, 3, 2 + 3);
}
```

`{}` = placeholder. Argument automatically sahi type mein format ho jaata hai.

---

## Manipulators se comparison

```cpp
double price = 21500.5;

// ❌ Manipulators -- verbose, sticky, reset karna padta hai
std::cout << std::fixed << std::setprecision(2)
          << std::setw(12) << price << "\n";
std::cout << std::defaultfloat << std::setprecision(6);   // reset

// ✅ std::format -- ek line, koi state nahi
std::cout << std::format("{:>12.2f}\n", price);
```

| | Manipulators | `std::format` |
|---|---|---|
| Verbose | ✅ bahut | ❌ compact |
| Sticky state | ⚠️ haan (bugs) | ✅ nahi |
| Type safety | ✅ haan | ✅ haan |
| Reorder arguments | ❌ nahi | ✅ haan |
| Speed | slow | **faster** |
| Compile-time check | ❌ | ✅ (format string constexpr hai) |

---

## Format specification

```
   {[index][:[fill][align][sign][#][0][width][.precision][type]]}
```

### Argument index
```cpp
std::format("{0} {1} {0}", "A", "B");        // "A B A"
std::format("{} {}", 1, 2);                   // "1 2" (automatic)
```

### Alignment aur fill
```cpp
std::format("{:<10}|", 42);       // "42        |"   left
std::format("{:>10}|", 42);       // "        42|"   right
std::format("{:^10}|", 42);       // "    42    |"   center
std::format("{:*>10}|", 42);      // "********42|"   fill with *
std::format("{:0>5}", 42);        // "00042"
std::format("{:05}", 42);         // "00042"  (zero-pad shortcut)
```

### Numbers
```cpp
std::format("{:d}", 42);          // "42"     decimal
std::format("{:b}", 42);          // "101010" binary
std::format("{:o}", 42);          // "52"     octal
std::format("{:x}", 255);         // "ff"     hex
std::format("{:X}", 255);         // "FF"     HEX
std::format("{:#x}", 255);        // "0xff"   with prefix
std::format("{:#b}", 5);          // "0b101"

std::format("{:+}", 42);          // "+42"    always sign
std::format("{: }", 42);          // " 42"    space for positive
```

### Floating point
```cpp
double pi = 3.14159265;

std::format("{:.2f}", pi);        // "3.14"       fixed
std::format("{:.4f}", pi);        // "3.1416"
std::format("{:e}", pi);          // "3.141593e+00"  scientific
std::format("{:.2e}", pi);        // "3.14e+00"
std::format("{:g}", pi);          // "3.14159"   general
std::format("{:a}", pi);          // hex float
std::format("{:10.2f}", pi);      // "      3.14"
std::format("{:<10.2f}|", pi);    // "3.14      |"
```

### Strings aur chars
```cpp
std::format("{:>10}", "abc");     // "       abc"
std::format("{:.3}", "abcdefgh"); // "abc"  (truncate)
std::format("{:c}", 65);          // "A"
```

### Bool
```cpp
std::format("{}", true);          // "true"    ← boolalpha default hai!
std::format("{:d}", true);        // "1"
```

Note: `std::format` mein `bool` **default se `true`/`false`** print hota hai —
`cout` ke ulta.

### Pointers
```cpp
int x = 5;
std::format("{}", static_cast<void*>(&x));    // "0x7ffd..."
```

---

## Dynamic width aur precision

```cpp
int w = 10;
int p = 3;
std::format("{:{}.{}f}", 3.14159, w, p);      // "     3.142"
```

Nested `{}` se runtime values pass kar sakte ho.

---

## Compile-time format string checking 🔑

**Yeh `std::format` ka sabse bada faayda hai.**

```cpp
std::format("{} {}", 1);              // ❌ COMPILE ERROR -- kam arguments
std::format("{:d}", "text");          // ❌ COMPILE ERROR -- galat type
std::format("{:.2f}", 42);            // ❌ COMPILE ERROR -- int pe float spec

// printf mein yeh sab RUNTIME pe crash karta:
printf("%d %d", 1);                   // ⚠️ UB -- garbage ya crash
printf("%d", "text");                 // ⚠️ UB
printf("%s", 42);                     // ⚠️ CRASH
```

`printf` format string aur arguments ka match **runtime pe** hota hai (aur aksar
hota hi nahi). `std::format` **compile time pe** check karta hai.

---

## `std::print` (C++23)

C++23 mein aur bhi simple:

```cpp
#include <print>

std::print("Hello, {}!\n", "World");
std::println("Hello, {}!", "World");      // \n automatically
std::print(stderr, "Error: {}\n", msg);   // stderr pe
```

**Aur yeh `std::cout << std::format(...)` se tez hai** — kyunki beech mein
`std::string` nahi banti.

| Method | Allocation? |
|---|---|
| `std::cout << std::format(...)` | ✅ string banti hai |
| `std::print(...)` | ❌ seedha stream mein |
| `std::format_to(out_iter, ...)` | ❌ aapke buffer mein |

---

## `format_to` — allocation avoid karo

```cpp
#include <format>
#include <array>

char buffer[256];
auto result = std::format_to_n(buffer, sizeof(buffer),
                               "Price: {:.2f}", 21500.5);
std::size_t len = result.size;
// buffer mein formatted text hai, koi heap allocation nahi hui
```

> **HFT relevance:** `std::format` ek `std::string` return karta hai — matlab
> **potential heap allocation**. Hot path mein `format_to`/`format_to_n` use karo
> ek pre-allocated buffer ke saath. Ya better: hot path mein format karo hi mat
> (raw values queue mein daalo, logger thread format kare — folder 41).

---

## Custom types ke liye formatter

```cpp
#include <format>

struct Point { int x, y; };

template <>
struct std::formatter<Point> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();          // koi custom spec nahi
    }

    auto format(const Point& p, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};

// Ab kaam karta hai:
std::cout << std::format("Point: {}\n", Point{1, 2});     // "Point: (1, 2)"
```

Yeh templates use karta hai (folder 21), abhi bas pattern dekh lo.

---

## `printf` se comparison

```cpp
// printf -- type-UNSAFE
printf("%s is %d years old\n", name, age);
// %d ki jagah galti se %s? -> CRASH ya garbage

// std::format -- type-SAFE
std::cout << std::format("{} is {} years old\n", name, age);
// type automatically deduce hota hai -- galti possible hi nahi
```

| | `printf` | `std::format` |
|---|---|---|
| Type safety | ❌ nahi | ✅ compile-time |
| Custom types | ❌ nahi | ✅ formatter se |
| `std::string` support | ❌ `.c_str()` chahiye | ✅ seedha |
| Argument reorder | ❌ (POSIX `%1$s` non-standard) | ✅ |
| Speed | fast | **faster** |
| Security | ⚠️ format string attacks | ✅ safe |

---

## Performance

Typical benchmark (100k formatted lines):

```
   printf              ~ 1.0x  (baseline)
   std::format         ~ 0.8x  (thoda tez)
   std::print          ~ 0.7x  (aur tez)
   iostream + manip    ~ 2-3x  (SLOW)
```

`std::format` `printf` se **tez** hai kyunki format string compile time pe parse
ho jaati hai.

---

## Hands-on

`examples/05_format_cpp20.cpp` chalao:

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 05_format_cpp20.cpp -o fmt && ./fmt
```

Agar `<format>` nahi mila, GCC 13+ chahiye. Version check:
```bash
g++ --version
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::format` `printf` jaisa unsafe hai" | Compile-time type-checked hai |
| "`format` slow hai" | `printf` se **tez** hai |
| "`{}` mein type likhna padta hai" | Nahi — automatically deduce hota hai |
| "`format` allocate nahi karta" | `std::string` return karta hai — allocation ho sakti hai |
| "`{:.2}` `{:.2f}` jaisa hai" | Nahi — `.2` significant digits, `.2f` fixed decimals |

---

## Exercises

1. Yeh manipulator code `std::format` mein convert karo:
   ```cpp
   std::cout << std::setw(12) << std::left << "NIFTY"
             << std::right << std::setw(10) << std::fixed
             << std::setprecision(2) << 21500.5 << "\n";
   ```
   <details><summary>Answer</summary>

   ```cpp
   std::cout << std::format("{:<12}{:>10.2f}\n", "NIFTY", 21500.5);
   ```
   </details>

2. Ek table banao `std::format` se (file 07 wala same table).

3. Compile-time safety test karo:
   ```cpp
   std::format("{} {}", 1);          // error dekho
   std::format("{:d}", "text");      // error dekho
   ```

4. Hex dump `std::format` se likho:
   ```cpp
   std::format("{:02x} ", byte);
   ```

5. `Point` ke liye custom formatter likho.

6. `format_to_n` se ek fixed buffer mein format karo (koi allocation nahi).

7. Benchmark: `printf` vs `std::format` vs `iostream + manipulators`, 100k lines.

---

## Interview questions

1. `std::format` `printf` se safe kyun hai?
2. `std::format` aur `std::print` mein fark?
3. Hot path mein `std::format` ka kya problem hai?
4. `{:.2}` aur `{:.2f}` mein fark?

---

## Next
→ [`09-file-streams.md`](09-file-streams.md)
