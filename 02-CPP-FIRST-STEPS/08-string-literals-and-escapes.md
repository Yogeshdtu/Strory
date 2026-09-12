# 08 — String literals aur escape sequences

## Prerequisites
`02-anatomy-line-by-line.md`, folder 01 lesson 10 (bits/ASCII)

## Yeh topic abhi kyun
Aap har program mein text print karoge. `"..."` mein kya-kya ho sakta hai, aur `\n`
jaisi cheezein kaise kaam karti hain — yeh abhi clear karna zaroori hai. Aur folder 10
(strings) mein yeh foundation kaam aayegi.

---

## String literal kya hai?

```cpp
"Hello World"
```

Do double-quotes ke beech ka text.

### Iska type

```cpp
auto s = "Hello";       // type: const char*
```

Actually `"Hello"` ka type `const char[6]` hai:

```
   Index:  0    1    2    3    4    5
   Value: 'H'  'e'  'l'  'l'  'o'  '\0'
                                    ^
                              NULL TERMINATOR
                            (automatic lagta hai)
```

**6 characters**, 5 nahi. Aakhir mein `\0` (value 0) automatic lagta hai.

**Yeh `\0` kyun?** Kyunki C-style strings mein length store nahi hoti. `\0` hi batata
hai ki string kahan khatam hui. Folder 10 mein poora.

### String literals read-only hoti hain

```cpp
const char* s = "Hello";
s[0] = 'J';                // ❌ CRASH! Undefined Behaviour

char arr[] = "Hello";      // ✅ yeh copy hai, modify kar sakte ho
arr[0] = 'J';              // ✅ theek hai
```

String literals `.rodata` section mein hoti hain (yaad hai folder 01 lesson 11?),
jo read-only memory hai.

---

## Escape sequences — poori list

`\` ke baad ka character **special** ban jaata hai.

| Escape | Naam | ASCII | Kya karta hai |
|---|---|---|---|
| `\n` | newline | 10 | agli line pe jao |
| `\t` | horizontal tab | 9 | tab stop tak jao |
| `\v` | vertical tab | 11 | (rarely used) |
| `\b` | backspace | 8 | ek character peeche |
| `\r` | carriage return | 13 | line ke shuru mein jao |
| `\f` | form feed | 12 | (printers ke liye) |
| `\a` | alert / bell | 7 | beep! 🔔 |
| `\\` | backslash | 92 | ek actual `\` |
| `\'` | single quote | 39 | ek actual `'` |
| `\"` | double quote | 34 | ek actual `"` |
| `\?` | question mark | 63 | (trigraphs ke liye, ab useless) |
| `\0` | null | 0 | string terminator |
| `\nnn` | octal | — | `\101` = 'A' (65 decimal) |
| `\xhh` | hex | — | `\x41` = 'A' |
| `\uXXXX` | unicode | — | `\u00E9` = 'é' |

---

## Sabse zaroori: `\n`

```cpp
std::cout << "Line 1\n";
std::cout << "Line 2\n";
```

Output:
```
Line 1
Line 2
```

Bina `\n`:
```cpp
std::cout << "Line 1";
std::cout << "Line 2";
```

Output:
```
Line 1Line 2
```

**`\n` EK character hai, do nahi.** Verify:
```cpp
std::cout << sizeof("a\n");     // 3 (a, newline, \0)
```

---

## `\n` vs `std::endl` — important!

```cpp
std::cout << "Hi\n";           // ✅ recommended
std::cout << "Hi" << std::endl; // ⚠️ slow
```

| | `\n` | `std::endl` |
|---|---|---|
| Kya karta hai | newline character daalta hai | newline daalta hai **+ buffer FLUSH karta hai** |
| Speed | fast | **slow** |
| Kab use karein | 99% cases | jab turant output chahiye |

### Flush kya hai?

`cout` buffered hai — output turant screen pe nahi jaata, pehle ek buffer mein jama
hota hai. Buffer bharne pe ya program khatam hone pe woh screen pe jaata hai.

`endl` har baar **zabardasti** buffer khali karta hai — matlab ek syscall (`write`).
Aur syscalls mehnge hote hain (~500ns+).

### Benchmark khud dekho

```cpp
#include <iostream>
#include <chrono>

int main() {
    const int N = 100000;

    auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) std::cout << i << "\n";
    auto t2 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) std::cout << i << std::endl;
    auto t3 = std::chrono::steady_clock::now();

    std::cerr << "\\n:    " 
              << std::chrono::duration<double,std::milli>(t2-t1).count() << " ms\n";
    std::cerr << "endl: " 
              << std::chrono::duration<double,std::milli>(t3-t2).count() << " ms\n";
}
```

```bash
g++ -std=c++20 -O2 endl_test.cpp -o endl_test
./endl_test > /dev/null      # output ko discard karo, sirf timing dekho
```

Aapko **5-50x** ka fark dikhega.

> **HFT relevance:** Logging hot path ka common bottleneck hai. `endl` har log line pe
> syscall karega. HFT systems mein logging asynchronous hoti hai — hot path sirf ek
> lock-free queue mein message daalta hai, aur ek alag thread use disk pe likhta hai.
> Folder 41 mein.

---

## Escape ki zarurat kab padti hai?

### Quotes ke andar quotes
```cpp
std::cout << "Usne kaha "Hello"";       // ❌ compiler confuse
std::cout << "Usne kaha \"Hello\"";     // ✅
```
Output: `Usne kaha "Hello"`

### Backslash
```cpp
std::cout << "C:\Users\Rahul";          // ❌ \U aur \R invalid escapes hain
std::cout << "C:\\Users\\Rahul";        // ✅
```
Output: `C:\Users\Rahul`

**Yeh Windows paths ka classic problem hai.**

### Tab se alignment
```cpp
std::cout << "Naam\tUmar\tCity\n";
std::cout << "Rahul\t25\tDelhi\n";
std::cout << "Priya\t30\tMumbai\n";
```
Output:
```
Naam	Umar	City
Rahul	25	Delhi
Priya	30	Mumbai
```

(Alignment perfect nahi hoti kyunki tab stops fixed positions pe hote hain.
Better alignment ke liye `<iomanip>` ka `std::setw` — folder 04 mein.)

---

## Raw string literals (C++11) — bahut kaam ki cheez

Jab bahut saare backslashes hon, escape karna painful ho jaata hai:

```cpp
// ❌ escape hell
std::string regex = "\\d{3}-\\d{4}";
std::string path  = "C:\\Users\\Rahul\\Documents\\file.txt";
```

**Raw string** use karo:
```cpp
// ✅ jo likha hai, wahi hai
std::string regex = R"(\d{3}-\d{4})";
std::string path  = R"(C:\Users\Rahul\Documents\file.txt)";
```

### Syntax
```cpp
R"(content)"
```

`R"(` se shuru, `)"` pe khatam. Beech mein **kuch bhi escape nahi hota**.

### Multi-line raw strings
```cpp
std::string json = R"({
    "symbol": "NIFTY",
    "price": 21500,
    "path": "C:\data\feed.log"
})";
```

Newlines bhi as-is aate hain. JSON, SQL, regex, file paths — sab ke liye perfect.

### Agar content mein `)"` ho?
Custom delimiter use karo:
```cpp
R"DELIM(yeh )" bhi safe hai)DELIM"
```

---

## Character literal vs String literal

```cpp
'A'        // CHARACTER literal - type: char, size 1 byte
"A"        // STRING literal - type: const char[2] ('A' + '\0'), size 2 bytes
```

**Yeh alag cheezein hain!**

```cpp
char c = 'A';        // ✅
char c = "A";        // ❌ error: cannot convert 'const char*' to 'char'

std::cout << 'A';    // A
std::cout << "A";    // A     (same output, alag mechanism)

std::cout << sizeof('A');    // 1  (C++ mein char, C mein 4!)
std::cout << sizeof("A");    // 2
```

### Character bhi number hai
```cpp
char c = 'A';
std::cout << c;                    // A
std::cout << static_cast<int>(c);  // 65
std::cout << c + 1;                // 66  (int mein promote ho gaya!)
std::cout << static_cast<char>(c + 1); // B
```

---

## String literal concatenation (adjacent literals)

Do string literals agar side-by-side hon, to compiler unhe **jod deta hai**:

```cpp
std::cout << "Hello " "World\n";     // "Hello World\n" ban jaata hai
```

Yeh lambi strings todne ke liye kaam aata hai:
```cpp
std::cout << "Yeh ek bahut lambi line hai jo "
             "kai lines mein todi gayi hai taaki "
             "code padhne mein aasan rahe.\n";
```

**Note:** Yeh sirf **literals** ke liye kaam karta hai, variables ke liye nahi:
```cpp
const char* a = "Hello";
std::cout << a " World";     // ❌ error
```

---

## Encoding prefixes (jhalak)

```cpp
"text"        // const char[]        - narrow (usually UTF-8)
u8"text"      // const char8_t[]     - UTF-8 (C++20)
u"text"       // const char16_t[]    - UTF-16
U"text"       // const char32_t[]    - UTF-32
L"text"       // const wchar_t[]     - wide (platform-dependent)
```

Abhi bas jaan lo ki yeh exist karte hain. 99% cases mein aap plain `"text"` use karoge.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > escapes.cpp << 'END'
#include <iostream>

int main() {
    std::cout << "===== NEWLINE =====\n";
    std::cout << "Line 1\nLine 2\nLine 3\n";

    std::cout << "\n===== TAB =====\n";
    std::cout << "Naam\tUmar\tCity\n";
    std::cout << "Rahul\t25\tDelhi\n";
    std::cout << "Priyanka\t30\tMumbai\n";
    std::cout << "(dhyaan do: naam ki length se alignment bigad gaya)\n";

    std::cout << "\n===== QUOTES =====\n";
    std::cout << "Usne kaha \"Namaste\"\n";
    std::cout << "Yeh single quote: \'\n";

    std::cout << "\n===== BACKSLASH =====\n";
    std::cout << "Windows path: C:\\Users\\Rahul\n";

    std::cout << "\n===== RAW STRING =====\n";
    std::cout << R"(Yahan \n escape NAHI hota, literally \n dikhta hai)" << "\n";
    std::cout << R"(Windows path: C:\Users\Rahul)" << "\n";

    std::cout << "\n===== MULTI-LINE RAW =====\n";
    std::cout << R"({
    "symbol": "NIFTY",
    "price": 21500
})" << "\n";

    std::cout << "\n===== SIZES =====\n";
    std::cout << "sizeof(\"\")      = " << sizeof("")      << "  (sirf \\0)\n";
    std::cout << "sizeof(\"A\")     = " << sizeof("A")     << "  (A + \\0)\n";
    std::cout << "sizeof(\"Hello\") = " << sizeof("Hello") << "  (5 chars + \\0)\n";
    std::cout << "sizeof(\"a\\n\")   = " << sizeof("a\n")   << "  (\\n EK char hai)\n";
    std::cout << "sizeof('A')     = " << sizeof('A')     << "  (char literal)\n";

    std::cout << "\n===== CHAR IS A NUMBER =====\n";
    char c = 'A';
    std::cout << "'A' as char: " << c << "\n";
    std::cout << "'A' as int:  " << static_cast<int>(c) << "\n";
    std::cout << "'A' + 1:     " << static_cast<char>(c + 1) << "\n";

    std::cout << "\n===== ADJACENT CONCATENATION =====\n";
    std::cout << "Yeh " "ek " "hi " "string " "hai\n";

    std::cout << "\n===== HEX AND OCTAL ESCAPES =====\n";
    std::cout << "\x48\x65\x6C\x6C\x6F" << " (hex for Hello)\n";
    std::cout << "\110\145\154\154\157" << " (octal for Hello)\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra escapes.cpp -o escapes && ./escapes
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`\n` do characters hain" | Ek character (ASCII 10) |
| "`'A'` aur `"A"` same hain" | `char` vs `const char[2]` — bilkul alag |
| "`endl` aur `\n` same hain" | `endl` flush bhi karta hai — slow |
| "String literal modify kar sakte ho" | ❌ UB. Read-only memory mein hai |
| "`sizeof("Hello")` = 5" | 6 hai — null terminator bhi ginta hai |
| "`\` sirf `\n` ke liye hai" | Bahut saare escapes hain |

---

## Exercises

1. `escapes.cpp` chalao. Har section ka output samjho.

2. Ek program likho jo yeh exact output de:
   ```
   Naam: "Rahul"
   Path: C:\Users\Rahul
   Tab	separated	values
   ```
   <details><summary>Answer</summary>

   ```cpp
   std::cout << "Naam: \"Rahul\"\n";
   std::cout << "Path: C:\\Users\\Rahul\n";
   std::cout << "Tab\tseparated\tvalues\n";
   ```
   Ya raw strings se:
   ```cpp
   std::cout << R"(Naam: "Rahul")" << "\n";
   std::cout << R"(Path: C:\Users\Rahul)" << "\n";
   ```
   </details>

3. `sizeof` predict karo:
   - `sizeof("")`
   - `sizeof("abc")`
   - `sizeof("a\tb")`
   - `sizeof("\\")`
   <details><summary>Answers</summary>1, 4, 4, 2</details>

4. Yeh program kyun crash karega?
   ```cpp
   char* s = "Hello";     // (C++11 se yeh compile bhi nahi hoga)
   s[0] = 'J';
   ```
   <details><summary>Answer</summary>
   String literal read-only memory mein hai. Usko modify karna UB hai.
   C++11 se `char*` mein string literal assign karna bhi illegal hai
   (`const char*` chahiye). Copy chahiye to: `char s[] = "Hello";`
   </details>

5. `endl` benchmark chalao. Kitna fark mila?

6. Ek ASCII table print karo (32 se 126 tak):
   ```cpp
   for (int i = 32; i < 127; ++i) {
       std::cout << i << " = " << static_cast<char>(i) << "\t";
       if ((i - 31) % 8 == 0) std::cout << "\n";
   }
   ```
   (Loop abhi nahi padha — bas copy karke chalao aur output dekho.)

---

## Interview questions

1. `\n` aur `std::endl` mein kya fark hai? Kaunsa use karna chahiye?
2. `'A'` aur `"A"` mein kya fark hai?
3. `sizeof("Hello")` kya hai aur kyun?
4. String literal ko modify kar sakte ho?
5. Raw string literal kya hai? Kab use karte hain?

---

## Next
→ [`09-compilation-pipeline-revisited.md`](09-compilation-pipeline-revisited.md)
