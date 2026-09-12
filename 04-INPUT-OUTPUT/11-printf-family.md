# 11 — `printf` family

## Prerequisites
`08-std-format.md`

## Yeh topic abhi kyun
`printf` C se aaya hai. Aap ise **likhoge nahi**, par aapko **padhna** aana chahiye —
kyunki purana code, C libraries, aur bahut se examples ismein hain.

Aur uske **security problems** samajhna zaroori hai.

---

## Basic

```cpp
#include <cstdio>

printf("Hello, World\n");
printf("Number: %d\n", 42);
printf("Name: %s, Age: %d\n", "Rahul", 25);
printf("Pi: %.2f\n", 3.14159);
```

---

## Format specifiers

```
   %[flags][width][.precision][length]specifier
```

### Common specifiers
| Spec | Type | Example |
|---|---|---|
| `%d` / `%i` | `int` | `42` |
| `%u` | `unsigned int` | `42` |
| `%ld` | `long` | `42` |
| `%lld` | `long long` | `42` |
| `%zu` | `size_t` | `42` |
| `%f` | `double` | `3.140000` |
| `%.2f` | `double`, 2 decimals | `3.14` |
| `%e` | scientific | `3.14e+00` |
| `%g` | shorter of f/e | `3.14` |
| `%s` | `const char*` | `text` |
| `%c` | `char` | `A` |
| `%x` / `%X` | hex | `ff` / `FF` |
| `%o` | octal | `377` |
| `%p` | pointer | `0x7ffd...` |
| `%%` | literal `%` | `%` |

### Width aur alignment
```cpp
printf("%10d|\n", 42);        //         42|
printf("%-10d|\n", 42);       // 42        |
printf("%010d|\n", 42);       // 0000000042|
printf("%+d\n", 42);          // +42
printf("%10.2f|\n", 3.14159); //       3.14|
```

### Fixed-width types
```cpp
#include <cinttypes>

std::int64_t x = 42;
printf("%" PRId64 "\n", x);          // portable macro
printf("%lld\n", (long long)x);      // ya cast karo
```

`PRId64` jaise macros portable hain (Windows pe `%lld` alag ho sakta hai).

---

## ⚠️ THE BIG PROBLEM: type safety NAHI hai

`printf` **variadic function** hai — compiler arguments ke types check nahi karta.

```cpp
printf("%d\n", "hello");        // ⚠️ UB -- string ko int ki tarah padha
printf("%s\n", 42);             // ⚠️ CRASH -- 42 ko address ki tarah padha
printf("%d %d\n", 1);           // ⚠️ UB -- doosra argument hai hi nahi
printf("%d\n", 3.14);           // ⚠️ UB -- double ko int ki tarah padha
printf("%f\n", 42);             // ⚠️ UB -- int ko double ki tarah padha
```

**Yeh sab compile ho jaate hain.** Crash runtime pe hoga — ya usse bhi bura, silently
garbage dega.

### GCC/Clang thoda bacha lete hain
```bash
g++ -Wall -Wformat -Wformat-security file.cpp
```
```
warning: format '%d' expects argument of type 'int', but argument 2 has type 'const char*'
```

**Yeh sirf literal format strings pe kaam karta hai.** Agar format string variable
mein hai, compiler kuch nahi kar sakta.

---

## 🔴 Format string vulnerability

Yeh ek **real security bug class** hai.

```cpp
char userInput[100];
scanf("%s", userInput);
printf(userInput);              // ⚠️⚠️ CRITICAL SECURITY BUG
```

Agar user `%s%s%s%s%s` type kare:
- `printf` stack se 5 pointers padhne ki koshish karega
- Jo hai nahi
- **Crash, ya memory leak, ya arbitrary read**

Aur `%n` specifier to **memory mein likh** sakta hai — matlab arbitrary write.

### ✅ Sahi tareeka
```cpp
printf("%s", userInput);        // ✅ format string CONSTANT hai
fputs(userInput, stdout);       // ✅ aur bhi simple
std::cout << userInput;         // ✅ C++ way
```

**Rule: format string kabhi user input se mat banao.**

```bash
g++ -Wformat-security file.cpp      # yeh flag isko pakadta hai
```

---

## `scanf` — aur bhi khatarnaak

```cpp
char buffer[10];
scanf("%s", buffer);            // ⚠️ BUFFER OVERFLOW -- koi limit nahi!
```

User 100 characters type kare → stack corrupt → crash ya exploit.

### Kam-bura versions
```cpp
scanf("%9s", buffer);           // max 9 chars + '\0'
fgets(buffer, sizeof(buffer), stdin);      // better
```

### ✅ C++ mein
```cpp
std::string s;
std::cin >> s;                  // ✅ automatically resize hoti hai
std::getline(std::cin, s);      // ✅
```

**`gets()` to itna khatarnaak tha ki C11 se standard se HATA DIYA GAYA.**
Agar kahin dikhe — woh code 20 saal purana hai.

---

## `printf` vs C++ streams vs `std::format`

| | `printf` | iostream | `std::format` |
|---|---|---|---|
| Type safety | ❌ | ✅ | ✅ |
| Compile-time check | ⚠️ partial | ✅ | ✅ |
| Custom types | ❌ | ✅ | ✅ |
| `std::string` | ❌ `.c_str()` | ✅ | ✅ |
| Speed | fast | **slow** | **fastest** |
| Verbosity | compact | verbose | compact |
| Security | ⚠️ format string attacks | ✅ | ✅ |
| Reorder args | ❌ | ❌ | ✅ |

### Decision
```
   C++20 available?  -> std::format / std::print     ✅ BEST
   C++17 ya purana?  -> fmtlib (same API)            ✅
   Nahi mil sakta?   -> iostream                     ✅ safe
   C code maintain karna hai? -> printf              (padho, likho mat)
```

---

## `printf` family ke members

```cpp
printf(fmt, ...);                       // stdout pe
fprintf(stream, fmt, ...);              // kisi bhi FILE* pe
fprintf(stderr, "Error: %s\n", msg);    // stderr pe

sprintf(buffer, fmt, ...);              // ⚠️ buffer mein -- OVERFLOW RISK
snprintf(buffer, size, fmt, ...);       // ✅ size-limited version

vprintf(fmt, va_list);                  // va_list versions
vsnprintf(buffer, size, fmt, va_list);
```

### `sprintf` vs `snprintf`

```cpp
char buf[10];
sprintf(buf, "%s", veryLongString);      // ⚠️ BUFFER OVERFLOW
snprintf(buf, sizeof(buf), "%s", veryLongString);   // ✅ truncate ho jaata hai
```

**`sprintf` kabhi mat use karo.** Hamesha `snprintf`.

`snprintf` return value = kitne characters **likhne the** (truncation se pehle):
```cpp
int needed = snprintf(buf, sizeof(buf), "%s", str);
if (needed >= static_cast<int>(sizeof(buf))) {
    // truncate hua -- bada buffer chahiye
}
```

---

## Kab `printf` theek hai?

1. **C code maintain kar rahe ho**
2. **C library ke saath interop**
3. **Debugging** — quick aur dirty
4. **Embedded systems** jahan iostream bahut heavy hai
5. **Signal handlers** — `printf` async-signal-safe **nahi** hai, par `write()` hai

Baaki sab jagah: **`std::format` ya iostream**.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > printfdemo.cpp << 'END'
#include <cstdio>
#include <cinttypes>
#include <cstring>

int main() {
    printf("===== BASIC =====\n");
    printf("Integer: %d\n", 42);
    printf("Float:   %.2f\n", 3.14159);
    printf("String:  %s\n", "hello");
    printf("Char:    %c\n", 'A');
    printf("Hex:     %x / %X / %#x\n", 255, 255, 255);
    printf("Pointer: %p\n", (void*)&main);
    printf("Percent: %%\n");

    printf("\n===== WIDTH / ALIGNMENT =====\n");
    printf("[%10d]\n", 42);
    printf("[%-10d]\n", 42);
    printf("[%010d]\n", 42);
    printf("[%+d]\n", 42);
    printf("[%10.3f]\n", 3.14159);

    printf("\n===== FIXED WIDTH TYPES =====\n");
    int64_t big = 9000000000LL;
    printf("int64_t: %" PRId64 "\n", big);
    size_t sz = 12345;
    printf("size_t:  %zu\n", sz);

    printf("\n===== snprintf (SAFE) =====\n");
    char buf[10];
    int needed = snprintf(buf, sizeof(buf), "%s", "This is a very long string");
    printf("buffer:  '%s'\n", buf);
    printf("needed:  %d bytes (buffer sirf %zu ka hai)\n", needed, sizeof(buf));
    printf("truncate hua? %s\n", (needed >= (int)sizeof(buf)) ? "HAAN" : "nahi");

    printf("\n===== TYPE SAFETY DEMO =====\n");
    printf("Yeh sab COMPILE ho jaate hain par UB hain:\n");
    printf("  printf(\"%%d\", \"text\")   -- string ko int ki tarah\n");
    printf("  printf(\"%%s\", 42)        -- 42 ko address ki tarah -> CRASH\n");
    printf("  printf(\"%%d %%d\", 1)      -- doosra argument hai hi nahi\n");
    printf("\n-Wall -Wformat se GCC in par warning deta hai (literal strings mein)\n");
    printf("std::format mein yeh sab COMPILE ERROR hote hain.\n");

    printf("\n===== SECURITY =====\n");
    const char* userInput = "Hello %s %s %s";
    printf("GALAT: printf(userInput)     <- format string attack!\n");
    printf("SAHI:  printf(\"%%s\", userInput) -> %s\n", userInput);

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra -Wformat -Wformat-security printfdemo.cpp -o printfdemo
./printfdemo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`printf` type-safe hai" | ❌ Bilkul nahi — runtime UB |
| "`-Wall` sab pakad leta hai" | Sirf literal format strings mein |
| "`sprintf` theek hai" | ❌ Buffer overflow. `snprintf` use karo |
| "`printf(userInput)` chalega" | ⚠️ **Security vulnerability** |
| "`%d` `long` ke liye chalega" | ❌ `%ld` chahiye — warna UB |

---

## Exercises

1. `printfdemo.cpp` chalao. Warnings padho.

2. Type mismatch bug banao aur `-Wformat` se pakdo:
   ```cpp
   printf("%d\n", "hello");
   printf("%s\n", 42);
   ```

3. `snprintf` truncation test karo.

4. Yeh code `std::format` mein convert karo:
   ```cpp
   printf("%-12s %10.2f %8d\n", symbol, price, qty);
   ```
   <details><summary>Answer</summary>

   ```cpp
   std::cout << std::format("{:<12} {:>10.2f} {:>8}\n", symbol, price, qty);
   ```
   </details>

5. Format string vulnerability demonstrate karo (safely, `%x` se):
   ```cpp
   const char* evil = "%x %x %x %x";
   printf(evil);           // stack se garbage padhega
   printf("\n");
   ```
   ⚠️ `-Wformat-security` warning deta hai.

6. Benchmark: `printf` vs `std::cout` vs `std::format`, 100k lines.

---

## Interview questions

1. `printf` type-safe kyun nahi hai?
2. Format string vulnerability kya hai?
3. `sprintf` aur `snprintf` mein fark?
4. `printf` iostream se tez kyun hai?
5. `%d` ki jagah `%ld` kab chahiye?

---

## Next
→ [`12-io-performance.md`](12-io-performance.md)
