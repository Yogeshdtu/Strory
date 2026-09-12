# 10 — String streams

## Prerequisites
`09-file-streams.md`, `05-getline-and-strings.md`

## Yeh topic abhi kyun
String streams se aap **memory mein** stream operations kar sakte ho — parsing aur
string building ke liye. Yeh CSV parsing, number conversion, aur message building
mein bahut kaam aate hain.

---

## Teen types

```cpp
#include <sstream>

std::istringstream iss("input text");    // padhne ke liye
std::ostringstream oss;                  // likhne ke liye
std::stringstream  ss;                   // dono
```

Yeh bhi `istream`/`ostream` se derive karte hain — sab kuch same kaam karta hai.

---

## String building (`ostringstream`)

```cpp
#include <sstream>

std::ostringstream oss;
oss << "Name: " << name << ", Age: " << age;
std::string result = oss.str();
```

### Kab useful hai
```cpp
// Ek complex string banani hai
std::ostringstream oss;
oss << std::fixed << std::setprecision(2);
oss << "Order: " << symbol
    << " @ " << price
    << " x " << quantity;
logMessage(oss.str());
```

⚠️ **C++20 mein `std::format` behtar hai:**
```cpp
auto s = std::format("Order: {} @ {:.2f} x {}", symbol, price, quantity);
```

Par `ostringstream` abhi bhi useful hai jab aap **loop mein** build kar rahe ho.

---

## Parsing (`istringstream`)

```cpp
std::istringstream iss("42 3.14 hello");

int i;
double d;
std::string s;
iss >> i >> d >> s;
// i = 42, d = 3.14, s = "hello"
```

### CSV parsing
```cpp
std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::istringstream iss(line);
    std::string part;
    while (std::getline(iss, part, delim)) {
        parts.push_back(part);
    }
    return parts;
}

auto fields = split("NIFTY,21500,100,BUY", ',');
```

### Line ko tokens mein todna
```cpp
std::istringstream iss("the quick brown fox");
std::string word;
while (iss >> word) {
    std::cout << word << "\n";
}
```

---

## Number conversion

### String → number
```cpp
std::istringstream iss("42");
int value;
if (iss >> value) {
    // success
}
```

**Lekin behtar tareeke hain:**

```cpp
// C++11
int v1 = std::stoi("42");           // ⚠️ throws, aur "12abc" accept karta hai
double v2 = std::stod("3.14");

// C++17 -- BEST (fast, no alloc, no exceptions)
#include <charconv>
int value{};
auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
if (ec == std::errc{} && ptr == str.data() + str.size()) {
    // clean parse
}
```

### Number → string
```cpp
// C++11
std::string s1 = std::to_string(42);        // simple, par formatting control nahi

// stringstream
std::ostringstream oss;
oss << std::fixed << std::setprecision(2) << 3.14159;
std::string s2 = oss.str();

// C++17 -- fastest
char buf[32];
auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), 42);
std::string s3(buf, ptr);

// C++20 -- cleanest
std::string s4 = std::format("{:.2f}", 3.14159);
```

### Performance comparison

Typical (1 million conversions):
```
   std::from_chars    ~ 1.0x   (fastest, no alloc, no exceptions)
   std::stoi          ~ 2-3x
   istringstream      ~ 10-20x  (SLOWEST -- stream object banta hai)
```

> **HFT relevance:** `istringstream` **har baar ek object banata hai** aur aksar
> allocate karta hai. Hot path mein bilkul nahi. Market data parsing mein
> `from_chars` ya custom parsers use hote hain (folder 38).

---

## `stringstream` (dono)

```cpp
std::stringstream ss;
ss << "42 hello";           // likho

int i;
std::string s;
ss >> i >> s;               // padho
```

### Reuse karna — dhyaan se

```cpp
std::stringstream ss;

ss << "first";
std::string a = ss.str();

ss.str("");                 // content saaf karo
ss.clear();                 // ⚠️ FLAGS bhi saaf karo (eof set ho gaya hoga)

ss << "second";
std::string b = ss.str();
```

**Dono zaroori hain:**
- `str("")` — content saaf
- `clear()` — state flags saaf (padhne ke baad `eofbit` set ho jaata hai)

⚠️ Sirf `str("")` karoge to stream fail state mein reh jaayega aur aage kuch
kaam nahi karega.

---

## Practical: log line parser

```cpp
#include <sstream>
#include <string>
#include <iostream>

struct LogEntry {
    std::string timestamp;
    std::string level;
    std::string message;
};

bool parseLogLine(const std::string& line, LogEntry& entry) {
    // Format: "2026-08-28T10:30:00 INFO Order received"
    std::istringstream iss(line);

    if (!(iss >> entry.timestamp >> entry.level)) return false;

    std::getline(iss, entry.message);           // baaki poori line
    // Leading space hata do
    if (!entry.message.empty() && entry.message.front() == ' ') {
        entry.message.erase(0, 1);
    }
    return true;
}
```

---

## Practical: CSV row parser

```cpp
struct Trade {
    std::string symbol;
    std::int64_t priceInTicks;
    std::uint32_t quantity;
    char side;
};

bool parseTrade(const std::string& line, Trade& t) {
    std::istringstream iss(line);
    std::string field;

    if (!std::getline(iss, t.symbol, ',')) return false;

    if (!std::getline(iss, field, ',')) return false;
    t.priceInTicks = std::stoll(field);

    if (!std::getline(iss, field, ',')) return false;
    t.quantity = static_cast<std::uint32_t>(std::stoul(field));

    if (!std::getline(iss, field, ',')) return false;
    if (field.empty()) return false;
    t.side = field[0];

    return true;
}
```

⚠️ Yeh naive hai — quoted fields handle nahi karta. Aur `stoll` throw kar sakta hai.
Production mein `from_chars` + proper CSV rules.

---

## `str()` ki cost

```cpp
std::ostringstream oss;
oss << "some content";

std::string s = oss.str();          // ⚠️ COPY banti hai!
```

C++20 se move version bhi hai:
```cpp
std::string s = std::move(oss).str();      // ✅ C++20 -- move, no copy
```

---

## Alternatives (jab performance matter kare)

| Use case | Slow | Fast |
|---|---|---|
| String building | `ostringstream` | `std::format` / `format_to` / `string::append` |
| Number → string | `ostringstream` | `std::to_chars` |
| String → number | `istringstream` | `std::from_chars` |
| Splitting | `istringstream` + `getline` | `string_view` + manual scan |

### Fast splitting with `string_view` (C++17)
```cpp
#include <string_view>
#include <vector>

std::vector<std::string_view> splitFast(std::string_view s, char delim) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        std::size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            parts.push_back(s.substr(start));
            break;
        }
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}
```

**Yeh koi string copy nahi karta** — sirf views banata hai. 10-50x tez.

⚠️ **Dhyaan:** views original string ko point karte hain. Agar original mar gayi,
views dangling ho jaayenge. (Folder 10 mein detail.)

---

## Hands-on

`examples/07_stringstream_parse.cpp` chalao.

```bash
cd examples
g++ -std=c++20 -O2 -Wall -Wextra 07_stringstream_parse.cpp -o ssparse && ./ssparse
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`stringstream` fast hai" | Slowest option hai. `from_chars` 10-20x tez |
| "`ss.str("")` kaafi hai reuse ke liye" | `clear()` bhi chahiye |
| "`str()` cheap hai" | Copy banata hai (C++20 se move version hai) |
| "`stoi` safe hai" | `"12abc"` accept karta hai, aur throw karta hai |
| "String views free hain" | Lifetime ka dhyaan rakhna padta hai |

---

## Exercises

1. Ek `split()` function likho `istringstream` se, phir `string_view` se. Benchmark karo.

2. Log line parser likho aur test karo.

3. Reuse bug reproduce karo:
   ```cpp
   std::stringstream ss;
   ss << "42";
   int a; ss >> a;
   ss.str("100");                    // clear() nahi kiya
   int b; ss >> b;
   std::cout << a << " " << b;       // b kya aaya?
   ```
   <details><summary>Answer</summary>
   `b` change nahi hoga (garbage ya 0) — kyunki pehle read ke baad `eofbit` set ho
   gaya tha, aur `clear()` nahi kiya. Fix: `ss.clear();` add karo.
   </details>

4. Number conversion benchmark: `istringstream` vs `stoi` vs `from_chars`
   (1 million conversions).

5. CSV parser likho jo:
   - Fields split kare
   - Numbers `from_chars` se parse kare
   - Errors gracefully handle kare

6. Ek `join()` function likho (split ka ulta):
   ```cpp
   std::string join(const std::vector<std::string>& parts, char delim);
   ```

---

## Interview questions

1. `stringstream` slow kyun hai?
2. `str("")` ke baad `clear()` kyun chahiye?
3. `from_chars` `stoi` se behtar kyun hai?
4. `string_view` se splitting ka faayda aur khatra?

---

## Next
→ [`11-printf-family.md`](11-printf-family.md)
