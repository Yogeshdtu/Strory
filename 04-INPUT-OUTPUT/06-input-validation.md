# 06 — Input validation aur stream states

## Prerequisites
`02-cin-and-input.md`, `05-getline-and-strings.md`

## Yeh topic abhi kyun
User galat input dega. **Hamesha.** Ya to galti se, ya jaan-boojh kar.

Agar aapka program uspe crash ho jaata hai ya infinite loop mein chala jaata hai —
woh production-ready nahi hai.

---

## Stream states — 4 flags

Har stream ke paas 4 state bits hote hain:

| Flag | Matlab | Check |
|---|---|---|
| `goodbit` | Sab theek hai | `.good()` |
| `failbit` | Formatting/conversion fail hui (recoverable) | `.fail()` |
| `eofbit` | End of file/input pahunch gaya | `.eof()` |
| `badbit` | Serious error (stream corrupt) | `.bad()` |

```cpp
std::cin.good();     // goodbit set hai? (koi error nahi)
std::cin.fail();     // failbit YA badbit set hai?
std::cin.eof();      // eofbit set hai?
std::cin.bad();      // badbit set hai?
std::cin.clear();    // saare flags saaf karo
std::cin.rdstate();  // raw state bits
```

### 🔑 `fail()` `badbit` ko bhi include karta hai

```cpp
if (std::cin.fail())    // failbit OR badbit
if (std::cin.bad())     // sirf badbit
```

---

## Galat input pe kya hota hai

```cpp
int age;
std::cin >> age;        // user types: "abc"
```

1. `>>` `abc` ko `int` mein convert nahi kar paata
2. **`failbit` set** ho jaata hai
3. `age = 0` set ho jaata hai (C++11 se; pehle undefined tha)
4. **`abc` buffer mein hi pada rehta hai** ← yeh sabse important hai
5. Har aage ka `>>` **turant fail** hoga (stream fail state mein hai)

---

## ⚠️ THE INFINITE LOOP

```cpp
int num;
while (true) {
    std::cout << "Number: ";
    std::cin >> num;                    // fail state mein KUCH NAHI padhta
    std::cout << "Aapne " << num << " likha\n";
}
```

User `abc` type karta hai → **infinite loop**, screen bhar jaata hai.

**Kyun?**
- Stream fail state mein hai → `>>` kuch nahi karta, turant return
- `abc` buffer mein hi hai → kabhi consume nahi hoga
- Loop chalta rehta hai

---

## Fix: `clear()` + `ignore()`

Do cheezein karni padti hain:

```cpp
#include <limits>

if (std::cin.fail()) {
    std::cin.clear();                                                   // 1. flags saaf
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 2. buffer saaf
}
```

**Dono zaroori hain:**
- `clear()` — stream ko wapas usable banata hai
- `ignore()` — galat input buffer se hataata hai

Sirf `clear()` karoge → galat input abhi bhi buffer mein hai → dobara fail hoga.
Sirf `ignore()` karoge → stream abhi bhi fail state mein hai → kuch nahi padhega.

---

## Robust input loop (yeh pattern yaad kar lo)

```cpp
#include <iostream>
#include <limits>

int readInt(const char* prompt) {
    int value;
    while (true) {
        std::cout << prompt;

        if (std::cin >> value) {
            // Success -- par buffer mein bacha hua kachra bhi saaf karo
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }

        // Fail hua
        if (std::cin.eof()) {
            std::cerr << "\nInput khatam ho gaya.\n";
            std::exit(EXIT_FAILURE);         // ya throw karo
        }

        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Galat input. Number likho.\n";
    }
}
```

**⚠️ `eof()` check zaroori hai** — warna Ctrl+D pe infinite loop ban jaayega
(EOF ke baad `clear()` karke bhi kuch nahi milega).

---

## Range validation

Type sahi hona kaafi nahi — value bhi sahi honi chahiye:

```cpp
int readIntInRange(const char* prompt, int min, int max) {
    while (true) {
        int value = readInt(prompt);
        if (value >= min && value <= max) return value;
        std::cout << "Value " << min << " se " << max << " ke beech honi chahiye.\n";
    }
}

// Use:
int age = readIntInRange("Umar (1-120): ", 1, 120);
```

---

## Better approach: `getline` + parse

Yeh **zyada robust** hai aur production mein aksar yahi use hota hai:

```cpp
#include <string>
#include <charconv>       // C++17 -- fast, no exceptions, no allocation
#include <optional>

std::optional<int> parseInt(const std::string& s) {
    int value{};
    const char* begin = s.data();
    const char* end   = s.data() + s.size();

    auto [ptr, ec] = std::from_chars(begin, end, value);

    if (ec != std::errc{}) return std::nullopt;      // parse fail
    if (ptr != end)        return std::nullopt;      // trailing garbage ("12abc")

    return value;
}

int readIntSafe(const char* prompt) {
    std::string line;
    while (true) {
        std::cout << prompt;
        if (!std::getline(std::cin, line)) {
            std::cerr << "\nInput khatam.\n";
            std::exit(EXIT_FAILURE);
        }
        if (auto v = parseInt(line)) return *v;
        std::cout << "Galat input. Number likho.\n";
    }
}
```

**Fayde:**
- Stream kabhi fail state mein nahi jaata
- Poori line hamesha consume hoti hai (koi leftover nahi)
- Trailing garbage detect ho jaata hai (`"12abc"` reject)
- `from_chars` **allocate nahi karta, exceptions nahi throw karta** — fast

### `stoi` se comparison

```cpp
// stoi -- exceptions throw karta hai
try {
    int v = std::stoi(line);
} catch (const std::invalid_argument&) {
    // parse fail
} catch (const std::out_of_range&) {
    // number bahut bada
}

// ⚠️ stoi ka trap: "12abc" ko 12 accept kar leta hai! (trailing garbage ignore)
std::stoi("12abc");      // 12 -- koi error nahi
```

`from_chars` yeh galti nahi karta.

> **HFT relevance:** `from_chars`/`to_chars` (C++17) **allocation-free aur
> exception-free** hain. Market data parsing mein `stoi`/`stod` ki jagah yahi use
> hote hain. Folder 38 mein detail.

---

## Reading until EOF properly

```cpp
int num;
while (std::cin >> num) {
    process(num);
}

// Loop khatam hone ke baad -- KYUN khatam hua?
if (std::cin.eof()) {
    std::cout << "Sab input padh liya.\n";           // ✅ normal
} else if (std::cin.fail()) {
    std::cerr << "Galat data mila.\n";               // ⚠️ error
} else if (std::cin.bad()) {
    std::cerr << "Stream corrupt ho gaya.\n";        // ⚠️ serious
}
```

**Yeh check important hai** — warna aapko pata nahi chalega ki data khatam hua ya
corrupt tha.

---

## Exceptions enable karna (rarely used)

```cpp
std::cin.exceptions(std::ios::failbit | std::ios::badbit);

try {
    int x;
    std::cin >> x;
} catch (const std::ios_base::failure& e) {
    std::cerr << "Input error: " << e.what() << "\n";
}
```

⚠️ Practically yeh kam use hota hai — kyunki EOF bhi exception throw karta hai,
jo aksar normal condition hoti hai.

---

## Hands-on

`examples/08_robust_input.cpp` mein poora bulletproof input system hai.

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 08_robust_input.cpp -o robust && ./robust
```

**Test karo:**
- `abc` (galat type)
- `12abc` (trailing garbage)
- `999999999999999` (overflow)
- Khali line (bas Enter)
- `Ctrl+D` (EOF)

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Galat input pe program crash hoga" | Nahi — stream fail state mein jaata hai |
| "Sirf `clear()` kaafi hai" | ❌ `ignore()` bhi chahiye, warna dobara fail |
| "`stoi` safe hai" | `"12abc"` accept kar leta hai. `from_chars` behtar |
| "`fail()` sirf failbit check karta hai" | `badbit` bhi check karta hai |
| "EOF check ki zarurat nahi" | ⚠️ Bina uske Ctrl+D pe infinite loop |

---

## Exercises

1. Infinite loop bug reproduce karo:
   ```cpp
   int num;
   while (true) {
       std::cout << "Number: ";
       std::cin >> num;
       std::cout << num << "\n";
   }
   ```
   `abc` type karo. `Ctrl+C` se rokna.

2. Ab `clear()` + `ignore()` se fix karo.

3. `readInt()` function likho jo:
   - Galat input pe dobara poochhe
   - EOF pe cleanly exit kare
   - Buffer hamesha saaf rakhe

4. `readIntInRange()` likho aur test karo.

5. `parseInt()` `from_chars` se likho aur in inputs pe test karo:
   `"42"`, `"-17"`, `"abc"`, `"12abc"`, `""`, `"  42"`, `"99999999999999999999"`
   <details><summary>Expected</summary>
   `42` ✅, `-17` ✅, fail ❌, fail ❌ (trailing), fail ❌, fail ❌ (leading space —
   `from_chars` whitespace skip nahi karta), fail ❌ (out of range)
   </details>

6. `stoi` ka trailing-garbage trap demonstrate karo:
   ```cpp
   std::cout << std::stoi("12abc") << "\n";      // 12 -- koi error nahi!
   ```

7. Ek menu-driven program likho jo:
   - 1-5 ke beech choice le
   - Galat input handle kare
   - `q` pe exit kare

---

## Interview questions

1. Stream ke 4 states kaunse hain?
2. Galat input pe `>>` kya karta hai?
3. `clear()` aur `ignore()` dono kyun chahiye?
4. `stoi` aur `from_chars` mein fark?
5. EOF check kyun zaroori hai input loop mein?

---

## Next
→ [`07-manipulators.md`](07-manipulators.md)
