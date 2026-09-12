# 05 — `getline` aur string input

## Prerequisites
`02-cin-and-input.md`

## Yeh topic abhi kyun
`>>` space pe ruk jaata hai — jo naam, address, ya kisi bhi sentence ke liye bekaar hai.
`getline` ka poora tareeka yahan.

Aur `>>` + `getline` ka classic bug — jo har beginner ko kaatta hai.

---

## `std::getline`

```cpp
#include <string>
#include <iostream>

std::string line;
std::getline(std::cin, line);
```

**Yeh poori line padhta hai**, newline tak. Newline **consume** ho jaata hai par
string mein **nahi aata**.

### Signature
```cpp
std::istream& getline(std::istream& is, std::string& str);
std::istream& getline(std::istream& is, std::string& str, char delim);
```

### Custom delimiter
```cpp
std::string field;
std::getline(std::cin, field, ',');      // comma tak padho
```

CSV parsing ke liye bahut useful.

---

## `>>` vs `getline`

```cpp
// Input: "Rahul Kumar Sharma"

std::string a;
std::cin >> a;                    // a = "Rahul"        (space pe ruka)

std::string b;
std::getline(std::cin, b);        // b = "Rahul Kumar Sharma"  (poori line)
```

| | `>>` | `getline` |
|---|---|---|
| Kahan tak padhta | whitespace tak | newline (ya delim) tak |
| Leading whitespace | skip karta hai | ❌ **nahi** — woh bhi le leta hai |
| Delimiter consume | nahi (buffer mein chhod deta hai) | haan (par string mein nahi) |
| Kis liye | ek word, ek number | poori line, sentences |

---

## ⚠️ THE CLASSIC BUG: `>>` ke baad `getline`

Yeh **har beginner** ko kaatta hai. Dhyaan se.

```cpp
int age;
std::string name;

std::cout << "Umar? ";
std::cin >> age;                  // user types: 25<Enter>

std::cout << "Naam? ";
std::getline(std::cin, name);     // ⚠️ RUKTA HI NAHI -- name khali!

std::cout << "[" << name << "]\n";     // []
```

### Kya hua — step by step

```
   User ne type kiya: "25\n"
   Buffer: "25\n"

   std::cin >> age
      -> "25" padha  (age = 25)
      -> '\n' pe RUK gaya, use buffer mein CHHOD diya
      Buffer: "\n"

   std::getline(std::cin, name)
      -> buffer mein pehla character hi '\n' hai
      -> "khaali line" mil gayi, turant return
      name = ""
```

### Fix 1: `ignore()` (recommended)

```cpp
#include <limits>

std::cin >> age;
std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
std::getline(std::cin, name);      // ✅ ab kaam karega
```

`ignore(count, delim)` = "count characters tak skip karo, ya `delim` milne tak."

`std::numeric_limits<std::streamsize>::max()` = "jitna bhi chahiye" (practically infinite).

### Fix 2: `ignore()` simple version

```cpp
std::cin >> age;
std::cin.ignore();                 // sirf 1 character skip
std::getline(std::cin, name);
```

⚠️ Yeh **kam robust** hai — agar user ne `25   ` (trailing spaces) type kiya to
kaam nahi karega.

### Fix 3: Sab kuch `getline` se padho

```cpp
std::string ageStr, name;
std::getline(std::cin, ageStr);
int age = std::stoi(ageStr);       // manually convert
std::getline(std::cin, name);      // ✅ koi problem nahi
```

**Yeh sabse robust hai** — aur production code mein aksar yahi use hota hai,
kyunki isse input validation bhi aasan ho jaati hai.

---

## Rule (yaad kar lo)

> **`>>` aur `getline` mix kar rahe ho? Beech mein `ignore()` daalo.**
>
> Ya behtar: **sirf `getline` use karo aur manually parse karo.**

---

## Lines padhna — loop

```cpp
std::string line;
while (std::getline(std::cin, line)) {
    std::cout << "Line: " << line << "\n";
}
```

`getline` `istream&` return karta hai, jo bool mein convert hota hai.
EOF pe loop ruk jaata hai.

### File se
```cpp
#include <fstream>

std::ifstream file("data.txt");
std::string line;
while (std::getline(file, line)) {
    // process line
}
```

---

## CSV parsing (practical example)

```cpp
#include <sstream>
#include <string>
#include <vector>

std::vector<std::string> splitCSV(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) {      // comma delimiter
        fields.push_back(field);
    }
    return fields;
}

// Use:
std::string line = "NIFTY,21500,100,BUY";
auto fields = splitCSV(line);
// fields = {"NIFTY", "21500", "100", "BUY"}
```

⚠️ **Yeh naive CSV parser hai.** Real CSV mein quoted fields (`"a,b",c`) aur escaped
quotes hote hain. Production mein proper parser use karo.

Full string streams file 10 mein.

---

## Empty lines aur whitespace

```cpp
std::string line;
std::getline(std::cin, line);

if (line.empty()) {
    // user ne bas Enter dabaya
}

// Whitespace-only line check
bool isBlank = line.find_first_not_of(" \t\r\n") == std::string::npos;
```

### `\r\n` ka problem (Windows files)

Windows files mein lines `\r\n` se khatam hoti hain. Linux pe padhne pe `\r` **string
mein reh jaata hai**:

```cpp
std::getline(file, line);
// line = "data\r"    <- ⚠️ trailing \r
```

Yeh silent bugs banata hai (comparisons fail, lengths galat).

**Fix:**
```cpp
if (!line.empty() && line.back() == '\r') {
    line.pop_back();
}
```

> Yeh real problem hai. Agar aapka parser Windows-generated CSV padh raha hai
> aur values match nahi kar rahi — yahi wajah hai.

---

## `getline` ki C-string version (avoid karo)

```cpp
char buffer[100];
std::cin.getline(buffer, 100);       // ⚠️ member function, alag cheez
```

- Fixed size — overflow ka risk
- Agar line 100 se lambi hui to stream fail ho jaata hai
- `std::string` version hamesha behtar hai

**Aur `gets()` to kabhi mat use karna** — woh itna khatarnaak tha ki C11 se
standard se **hata diya gaya**.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > getlinedemo.cpp << 'END'
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <limits>

std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::istringstream ss(line);
    std::string part;
    while (std::getline(ss, part, delim)) parts.push_back(part);
    return parts;
}

int main() {
    std::cout << "===== THE CLASSIC BUG =====\n";
    int age;
    std::string name;

    std::cout << "Umar likho: ";
    std::cin >> age;

    // ⚠️ Yeh line COMMENT karke dekho -- name khali aayega
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::cout << "Poora naam likho: ";
    std::getline(std::cin, name);

    std::cout << "Umar: " << age << ", Naam: [" << name << "]\n";
    std::cout << "(agar naam khali hai -> ignore() line comment ho gayi hai)\n";

    std::cout << "\n===== CSV PARSING =====\n";
    std::string csvLine = "NIFTY,21500,100,BUY";
    auto fields = split(csvLine, ',');
    std::cout << "Input: " << csvLine << "\n";
    std::cout << "Fields (" << fields.size() << "):\n";
    for (std::size_t i = 0; i < fields.size(); ++i) {
        std::cout << "  [" << i << "] = " << fields[i] << "\n";
    }

    std::cout << "\n===== TRAILING \\r HANDLING =====\n";
    std::string windowsLine = "data\r";
    std::cout << "Before: length = " << windowsLine.size() << "\n";
    if (!windowsLine.empty() && windowsLine.back() == '\r') windowsLine.pop_back();
    std::cout << "After:  length = " << windowsLine.size() << "\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra getlinedemo.cpp -o getlinedemo && ./getlinedemo
```

**Zaroor karo:** `ignore()` wali line comment karke dobara chalao. Bug reproduce karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`cin >> str` poori line padhta hai" | Space pe ruk jaata hai |
| "`getline` newline string mein daalta hai" | Consume karta hai, par string mein nahi daalta |
| "`>>` ke baad `getline` seedha chalega" | ❌ `ignore()` chahiye |
| "`getline` leading spaces skip karta hai" | ❌ Nahi — woh bhi string mein aate hain |
| "`\r` ki problem nahi hoti" | Windows files padhte waqt hoti hai |

---

## Exercises

1. `getlinedemo.cpp` chalao. Phir `ignore()` line comment karke dobara chalao —
   bug dekho.

2. Ek program likho jo user se poora naam aur address le (dono mein spaces honge).

3. Ek CSV file padho aur har line ke fields print karo.

4. Yeh bug fix karo:
   ```cpp
   int n;
   std::cout << "Kitne items? ";
   std::cin >> n;
   for (int i = 0; i < n; ++i) {
       std::string item;
       std::cout << "Item " << i+1 << ": ";
       std::getline(std::cin, item);
       std::cout << "  -> " << item << "\n";
   }
   ```
   <details><summary>Answer</summary>
   `>>` ke baad `ignore()` missing hai. Pehla `getline` khali aayega aur baaki shift
   ho jaayenge.

   Fix: `std::cin >> n;` ke turant baad
   `std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');`
   </details>

5. Ek function likho jo string se trailing `\r` aur whitespace hata de (trim):
   ```cpp
   std::string trim(const std::string& s);
   ```

6. `split()` function ko generalize karo taaki multiple delimiters handle kar sake.

---

## Interview questions

1. `>>` aur `getline` mein kya fark hai?
2. `>>` ke baad `getline` kyun fail karta hai? Fix?
3. `getline` ka delimiter kahan jaata hai?
4. `std::getline` aur `cin.getline()` mein fark?
5. Windows files Linux pe padhne mein kya problem hoti hai?

---

## Next
→ [`06-input-validation.md`](06-input-validation.md)
