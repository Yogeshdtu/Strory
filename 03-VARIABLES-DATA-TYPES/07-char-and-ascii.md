# 07 — `char` aur ASCII

## Prerequisites
`05-int-deep-dive.md`, folder 02 lesson 08 (string literals)

## Yeh topic abhi kyun
`char` ek chhota type hai par usmein 3 gotchas hain jo interviews mein aate hain.
Aur folder 10 (strings) aur folder 38 (binary market data parsing) mein `char` ka
role central hai.

---

## `char` kya hai?

**`char` = ek character. Aur woh actually ek chhota integer hai.**

```cpp
char grade = 'A';
char digit = '7';
char space = ' ';
```

**Size: hamesha 1 byte.** Yeh C++ mein guaranteed hai (`sizeof(char) == 1`, by definition).

---

## 🔑 `char` ek NUMBER hai

Yeh sabse important baat hai.

```cpp
char c = 'A';
std::cout << c;                          // A
std::cout << static_cast<int>(c);        // 65
std::cout << c + 0;                      // 65 (int mein promote ho gaya)
```

`'A'` ki value **65** hai. Woh ek number hai jo memory mein `01000001` ki tarah pada hai.
`cout` use character ki tarah dikhata hai kyunki uska type `char` hai.

### Isse trick nikalti hain

```cpp
// Lowercase -> Uppercase
char lower = 'a';
char upper = lower - 32;         // 97 - 32 = 65 = 'A'
// ya
char upper2 = lower - ('a' - 'A');   // zyada readable

// Character digit -> integer digit
char digitChar = '7';
int digit = digitChar - '0';     // 55 - 48 = 7

// Integer -> character digit
int n = 5;
char nChar = '0' + n;            // 48 + 5 = 53 = '5'

// Alphabet position
char letter = 'e';
int position = letter - 'a' + 1;  // 5 (e alphabet mein 5th hai)
```

**Yeh tricks bahut use hoti hain** — parsing mein, competitive programming mein,
aur market data protocols mein.

---

## ASCII table (zaroori parts)

| Range | Kya | Note |
|---|---|---|
| 0–31 | Control characters | `\0`=0, `\t`=9, `\n`=10, `\r`=13 |
| 32 | Space | |
| 48–57 | `'0'`–`'9'` | `'0'` = 48 |
| 65–90 | `'A'`–`'Z'` | `'A'` = 65 |
| 97–122 | `'a'`–`'z'` | `'a'` = 97 |
| 127 | DEL | |

**Yaad rakhne wale numbers:**
- `'0'` = **48**
- `'A'` = **65**
- `'a'` = **97**
- `'a' - 'A'` = **32**

---

## ⚠️ GOTCHA 1: `char` signed hai ya unsigned?

**Jawab: implementation-defined!** 😱

C++ mein actually **teen** alag char types hain:

```cpp
char           c;     // ⚠️ signed ya unsigned -- platform pe depend
signed char    sc;    // pakka signed:   -128 to 127
unsigned char  uc;    // pakka unsigned:    0 to 255
```

**Yeh teen ALAG types hain** (unlike `int`, jahan `int` aur `signed int` same hain).

| Platform | plain `char` |
|---|---|
| x86/x86-64 Linux, Windows, macOS | **signed** |
| ARM (Linux, Android) | **unsigned** |
| PowerPC | unsigned |

### Yeh kab bug banta hai?

```cpp
char c = 200;                       // x86 pe: -56 (overflow)
                                    // ARM pe: 200
if (c > 128) { /* x86 pe kabhi nahi chalega! */ }
```

```cpp
// Binary data padhte waqt -- CLASSIC BUG
char buffer[100];
read(fd, buffer, 100);
int value = buffer[0];              // ⚠️ agar byte 0xFF hai:
                                    //    x86 pe: -1
                                    //    ARM pe: 255
```

### Solution: binary data ke liye hamesha `unsigned char` ya `std::byte`

```cpp
unsigned char buffer[100];          // ✅ hamesha 0-255
// ya C++17 mein:
std::byte buffer[100];              // ✅ "yeh raw bytes hain, numbers nahi"
```

> **HFT relevance:** Market data parsing mein aap raw bytes padhte ho. Agar aapne
> `char` use kiya aur code ARM pe deploy hua (ya alag compiler), aapke price values
> galat ho jaayenge. **Hamesha `uint8_t` ya `std::byte` use karo binary data ke liye.**

### Check karo aapke system pe
```cpp
#include <iostream>
#include <limits>
int main() {
    std::cout << "char is " 
              << (std::numeric_limits<char>::is_signed ? "SIGNED" : "UNSIGNED") << "\n";
    std::cout << "char min: " << static_cast<int>(std::numeric_limits<char>::min()) << "\n";
    std::cout << "char max: " << static_cast<int>(std::numeric_limits<char>::max()) << "\n";
}
```

---

## ⚠️ GOTCHA 2: `cout` `uint8_t` ko character samajhta hai

```cpp
#include <cstdint>
#include <iostream>

int main() {
    std::uint8_t value = 65;
    std::cout << value << "\n";     // ⚠️ "A" print hoga, "65" nahi!
}
```

**Kyun?** `uint8_t` actually `unsigned char` ka alias hai. Aur `cout` ke liye
`unsigned char` ek **character** hai.

### Solution
```cpp
std::cout << static_cast<int>(value) << "\n";       // 65
std::cout << +value << "\n";                        // 65 (unary + promote karta hai)
```

**`+value` ek chhota trick hai** — unary `+` integer promotion trigger karta hai.

---

## ⚠️ GOTCHA 3: `sizeof('A')`

```cpp
// C++ mein
std::cout << sizeof('A');       // 1   (char literal)

// C mein (agar kabhi C code dekho)
printf("%zu", sizeof('A'));     // 4   (C mein char literals int hote hain!)
```

C aur C++ mein yeh fark hai. C++ mein `'A'` ka type `char` hai.

---

## Character classification (`<cctype>`)

```cpp
#include <cctype>

std::isalpha(c);      // letter hai?
std::isdigit(c);      // digit hai?
std::isalnum(c);      // letter ya digit?
std::isspace(c);      // space/tab/newline?
std::isupper(c);      // uppercase?
std::islower(c);      // lowercase?
std::ispunct(c);      // punctuation?
std::toupper(c);      // uppercase mein badlo
std::tolower(c);      // lowercase mein badlo
```

### ⚠️ Inka ek trap hai

Yeh functions `int` lete hain, aur unka argument **`unsigned char` ki range mein**
hona chahiye (ya `EOF`).

```cpp
char c = someChar;
std::isalpha(c);                                     // ⚠️ UB agar c negative ho
std::isalpha(static_cast<unsigned char>(c));         // ✅ sahi
```

Agar `c` negative hai (jaise `'é'` ka byte x86 pe), to yeh UB hai.

---

## Character types (C++ ke saare)

```cpp
char        c  = 'A';       // 1 byte, basic character set
signed char sc = 'A';       // 1 byte, pakka signed
unsigned char uc = 'A';     // 1 byte, pakka unsigned
char8_t     c8 = u8'A';     // C++20, UTF-8 code unit (1 byte)
char16_t    c16 = u'A';     // UTF-16 code unit (2 bytes)
char32_t    c32 = U'A';     // UTF-32 code unit (4 bytes)
wchar_t     wc = L'A';      // wide char (Windows: 2 bytes, Linux: 4 bytes)
```

99% cases mein aap plain `char` use karoge. Baaki Unicode-heavy code mein.

---

## Unicode ka chhota note

ASCII sirf 128 characters cover karta hai — English ke liye kaafi hai.

Hindi, Chinese, emoji ke liye **Unicode** chahiye. Modern standard: **UTF-8**.

UTF-8 mein ek character 1-4 bytes le sakta hai:
```cpp
const char* hindi = "नमस्ते";        // har character 3 bytes
const char* emoji = "🎉";            // 4 bytes
std::cout << sizeof("🎉");           // 5 (4 bytes + \0)
```

**Iska matlab:** `std::string` ki `.size()` aapko **bytes** deti hai, **characters**
nahi. Yeh folder 10 mein detail mein aayega.

```cpp
std::string s = "नमस्ते";
std::cout << s.size();      // 18 bytes, 6 "characters" nahi
```

---

## Hands-on

```bash
cd ~/cpp-practice
cat > chars.cpp << 'END'
#include <iostream>
#include <cctype>
#include <limits>
#include <cstdint>

int main() {
    std::cout << "===== char IS a number =====\n";
    char c = 'A';
    std::cout << "'A' as char: " << c << "\n";
    std::cout << "'A' as int:  " << static_cast<int>(c) << "\n";
    std::cout << "'A' + 1:     " << static_cast<char>(c + 1) << "\n";

    std::cout << "\n===== USEFUL TRICKS =====\n";
    std::cout << "'a' - 'A'      = " << ('a' - 'A') << "  (case offset)\n";
    std::cout << "'7' - '0'      = " << ('7' - '0') << "  (char -> digit)\n";
    std::cout << "'0' + 5        = " << static_cast<char>('0' + 5) << "  (digit -> char)\n";
    std::cout << "'e' - 'a' + 1  = " << ('e' - 'a' + 1) << "  (alphabet position)\n";

    std::cout << "\n===== IS char SIGNED? =====\n";
    std::cout << "char is " 
              << (std::numeric_limits<char>::is_signed ? "SIGNED" : "UNSIGNED") << "\n";
    std::cout << "char range: " << static_cast<int>(std::numeric_limits<char>::min())
              << " to " << static_cast<int>(std::numeric_limits<char>::max()) << "\n";

    std::cout << "\n===== THE 200 TRAP =====\n";
    char signedC = static_cast<char>(200);
    unsigned char unsignedC = 200;
    std::cout << "char 200         = " << static_cast<int>(signedC) << "\n";
    std::cout << "unsigned char 200 = " << static_cast<int>(unsignedC) << "\n";
    std::cout << "(x86 pe pehla -56 hoga, ARM pe 200)\n";

    std::cout << "\n===== uint8_t COUT TRAP =====\n";
    std::uint8_t val = 65;
    std::cout << "std::cout << val:              " << val << "  <- character!\n";
    std::cout << "std::cout << +val:             " << +val << "  <- number\n";
    std::cout << "std::cout << (int)val:         " << static_cast<int>(val) << "\n";

    std::cout << "\n===== CLASSIFICATION =====\n";
    char tests[] = {'A', 'z', '5', ' ', '!', '\n'};
    for (char t : tests) {
        auto u = static_cast<unsigned char>(t);   // UB se bachne ke liye
        std::cout << "'" << (t == '\n' ? ' ' : t) << "' (" << static_cast<int>(t) << ")"
                  << " alpha=" << (std::isalpha(u) ? 1 : 0)
                  << " digit=" << (std::isdigit(u) ? 1 : 0)
                  << " space=" << (std::isspace(u) ? 1 : 0)
                  << " upper=" << (std::isupper(u) ? 1 : 0) << "\n";
    }

    std::cout << "\n===== SIZES =====\n";
    std::cout << "sizeof(char)     = " << sizeof(char)     << "\n";
    std::cout << "sizeof('A')      = " << sizeof('A')      << "  (C mein 4 hota!)\n";
    std::cout << "sizeof(\"A\")      = " << sizeof("A")      << "  (string: 'A' + '\\0')\n";
    std::cout << "sizeof(wchar_t)  = " << sizeof(wchar_t)  << "\n";
    std::cout << "sizeof(char16_t) = " << sizeof(char16_t) << "\n";

    std::cout << "\n===== ASCII TABLE (printable) =====\n";
    for (int i = 32; i < 127; ++i) {
        std::cout << i << "=" << static_cast<char>(i) << "  ";
        if ((i - 31) % 10 == 0) std::cout << "\n";
    }
    std::cout << "\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra chars.cpp -o chars && ./chars
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`char` sirf letters ke liye hai" | `char` ek 1-byte integer hai, kuch bhi rakh sakta hai |
| "`char` signed hota hai" | Implementation-defined! x86 pe signed, ARM pe unsigned |
| "`char` aur `signed char` same hain" | ❌ **Teen alag types** hain |
| "`uint8_t` number ki tarah print hoga" | ❌ Character print hoga (`unsigned char` ka alias hai) |
| "`sizeof('A')` = 4" | C++ mein 1. (C mein 4) |
| "`string.size()` characters deta hai" | Bytes deta hai. UTF-8 mein alag ho sakta hai |

---

## Exercises

1. `chars.cpp` chalao. Aapke system pe `char` signed hai ya unsigned?

2. Ek function likho jo lowercase string ko uppercase mein badle (bina `toupper` use kiye):
   ```cpp
   char toUpperManual(char c) {
       // yahan likho
   }
   ```
   <details><summary>Answer</summary>

   ```cpp
   char toUpperManual(char c) {
       if (c >= 'a' && c <= 'z') return static_cast<char>(c - ('a' - 'A'));
       return c;
   }
   ```
   </details>

3. Ek digit string ko integer mein badlo (bina `stoi` use kiye):
   ```cpp
   // "12345" -> 12345
   ```
   <details><summary>Answer</summary>

   ```cpp
   int result = 0;
   const char* s = "12345";
   for (int i = 0; s[i] != '\0'; ++i) {
       result = result * 10 + (s[i] - '0');
   }
   ```
   Yeh exact technique HFT market data parsers mein use hoti hai (ASCII protocols ke liye).
   </details>

4. Predict karo:
   ```cpp
   std::uint8_t a = 200;
   std::cout << a << "\n";
   std::cout << +a << "\n";
   std::cout << a + 0 << "\n";
   ```
   <details><summary>Answer</summary>
   Pehla: ASCII 200 ka character (terminal pe kuch garbage ya È).
   Doosra: `200`.
   Teesra: `200`.
   </details>

5. `char` overflow test:
   ```cpp
   char c = 127;
   c = c + 1;
   std::cout << static_cast<int>(c);
   ```
   Kya aaya? (Hint: `-128` — wrap around, par technically implementation-defined
   conversion hai)

6. Ek chhota "caesar cipher" likho jo har letter ko 3 aage shift kare
   (`'a'` → `'d'`, `'z'` → `'c'`).

---

## Interview questions

1. `char` signed hai ya unsigned?
2. `char`, `signed char`, `unsigned char` — kitne alag types hain?
3. `sizeof('A')` C aur C++ mein alag kyun hai?
4. `std::cout << uint8_t(65)` kya print karega aur kyun?
5. Binary data ke liye kaunsa type use karna chahiye?
6. `std::string("नमस्ते").size()` kya dega?

---

## Next
→ [`08-bool.md`](08-bool.md)
