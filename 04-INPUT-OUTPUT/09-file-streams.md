# 09 — File streams

## Prerequisites
`06-input-validation.md`, `05-getline-and-strings.md`

## Yeh topic abhi kyun
Ab tak aapka data program ke saath mar jaata tha. Files se woh **bacha** rahega.

Aur file streams RAII ka pehla practical example hain — destructor automatically
file band kar deta hai.

---

## Teen file stream types

```cpp
#include <fstream>

std::ifstream in("data.txt");      // input  (padhne ke liye)
std::ofstream out("data.txt");     // output (likhne ke liye)
std::fstream  io("data.txt");      // dono
```

Yeh `istream`/`ostream` se **derive** karte hain — matlab `<<`, `>>`, `getline`,
manipulators — sab kuch same kaam karta hai.

---

## Likhna

```cpp
#include <fstream>
#include <iostream>

int main() {
    std::ofstream out("output.txt");

    if (!out) {                                  // ✅ HAMESHA check karo
        std::cerr << "File nahi khul payi\n";
        return 1;
    }

    out << "Line 1\n";
    out << "Number: " << 42 << "\n";
    out << std::fixed << std::setprecision(2) << 3.14159 << "\n";

    // out.close();      // ZARURAT NAHI -- destructor kar dega (RAII)
    return 0;
}                        // <- yahan file automatically band ho gayi
```

⚠️ **`std::ofstream out("file.txt")` file ko TRUNCATE kar deta hai** — poora content
mit jaata hai. Append karne ke liye mode chahiye (neeche).

---

## Padhna

```cpp
#include <fstream>
#include <string>
#include <iostream>

int main() {
    std::ifstream in("data.txt");
    if (!in) {
        std::cerr << "File nahi mili\n";
        return 1;
    }

    // Line by line -- sabse common
    std::string line;
    while (std::getline(in, line)) {
        std::cout << line << "\n";
    }

    return 0;
}
```

### Word by word
```cpp
std::string word;
while (in >> word) {
    std::cout << word << "\n";
}
```

### Numbers
```cpp
int num;
while (in >> num) {
    total += num;
}
```

### Poori file ek string mein
```cpp
#include <sstream>

std::ifstream in("data.txt");
std::stringstream buffer;
buffer << in.rdbuf();                    // poora content
std::string content = buffer.str();
```

Ya (C++11, efficient):
```cpp
std::ifstream in("data.txt", std::ios::binary);
std::string content((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
```

---

## File modes

```cpp
std::ofstream out("file.txt", std::ios::app);       // append
```

| Mode | Matlab |
|---|---|
| `std::ios::in` | padhne ke liye (ifstream ka default) |
| `std::ios::out` | likhne ke liye (ofstream ka default) |
| `std::ios::app` | **append** — hamesha end mein likho |
| `std::ios::ate` | kholte hi end pe jao (par seek kar sakte ho) |
| `std::ios::trunc` | content mita do (ofstream ka default) |
| `std::ios::binary` | **binary mode** — koi text translation nahi |

Combine karo `|` se:
```cpp
std::ofstream out("log.txt", std::ios::app | std::ios::binary);
std::fstream f("data.bin", std::ios::in | std::ios::out | std::ios::binary);
```

### ⚠️ `binary` mode kab zaroori hai

**Windows pe** text mode `\n` ko `\r\n` mein convert kar deta hai (likhte waqt) aur
ulta (padhte waqt).

Binary data ke liye yeh **corruption** karta hai:
```cpp
std::ofstream out("data.bin", std::ios::binary);       // ✅ zaroori
out.write(reinterpret_cast<const char*>(&value), sizeof(value));
```

Linux pe koi fark nahi padta, par **hamesha `binary` likho** binary data ke liye —
portable rehta hai.

---

## Error handling

```cpp
std::ifstream in("data.txt");

// Tareeka 1: bool conversion (sabse common)
if (!in) { /* fail */ }

// Tareeka 2: is_open()
if (!in.is_open()) { /* fail */ }

// Tareeka 3: detailed
if (in.fail()) { /* formatting/open error */ }
if (in.bad())  { /* serious I/O error */ }
if (in.eof())  { /* end of file */ }
```

### Padhne ke baad kyun ruka?

```cpp
int num;
while (in >> num) { /* ... */ }

if (in.eof()) {
    std::cout << "File poori padh li\n";              // ✅ normal
} else if (in.fail()) {
    std::cerr << "Galat data mila\n";                 // ⚠️ parse error
} else if (in.bad()) {
    std::cerr << "I/O error\n";                       // ⚠️ disk problem
}
```

**Yeh check zaroori hai** — warna aapko pata nahi chalega ki file poori padhi ya
beech mein corrupt data mila.

### Exceptions (optional)
```cpp
std::ifstream in;
in.exceptions(std::ios::failbit | std::ios::badbit);
try {
    in.open("data.txt");
    // ...
} catch (const std::ios_base::failure& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

⚠️ EOF bhi failbit set karta hai — isliye yeh pattern loops mein awkward hai.

---

## RAII — file automatically band hoti hai

```cpp
{
    std::ofstream out("data.txt");
    out << "content\n";
}   // <- destructor chala, file BAND ho gayi, buffer FLUSH hua
```

**Yeh RAII hai** (folder 02 lesson 06 se yaad hai?). Aapko `close()` call karne ki
zarurat nahi.

### Exception ke saath bhi kaam karta hai
```cpp
void writeData() {
    std::ofstream out("data.txt");
    out << "line 1\n";
    throw std::runtime_error("something failed");
    out << "line 2\n";       // kabhi nahi chalega
}   // <- file phir bhi BAND hoti hai (stack unwinding mein destructor chalta hai)
```

**Yehi RAII ka poora point hai.** Manual `close()` ke saath yeh guarantee nahi milti.

### `close()` kab manually karein?

```cpp
std::ofstream out("data.txt");
out << "content\n";
out.close();                     // ✅ ab band

// Ab file dobara use kar sakte ho
std::ifstream in("data.txt");
```

Ya scope se control karo (better):
```cpp
{
    std::ofstream out("data.txt");
    out << "content\n";
}   // band ho gayi
std::ifstream in("data.txt");
```

---

## Binary I/O

```cpp
#include <fstream>
#include <cstdint>

struct Record {
    std::uint64_t id;
    std::int64_t  price;
    std::uint32_t quantity;
};

// LIKHNA
{
    std::ofstream out("data.bin", std::ios::binary);
    Record r{12345, 2150050, 100};
    out.write(reinterpret_cast<const char*>(&r), sizeof(r));
}

// PADHNA
{
    std::ifstream in("data.bin", std::ios::binary);
    Record r{};
    in.read(reinterpret_cast<char*>(&r), sizeof(r));
    if (in.gcount() != sizeof(r)) { /* incomplete read */ }
}
```

### ⚠️ Binary I/O ke khatre

1. **Padding** — struct ka layout compiler pe depend karta hai
2. **Endianness** — alag machine pe bytes ulte
3. **Type sizes** — `long` alag platform pe alag
4. **Pointers/virtual functions** — inhe kabhi serialize mat karo

**Rules:**
```cpp
// ✅ Verify karo
static_assert(std::is_trivially_copyable_v<Record>);
static_assert(sizeof(Record) == 24);

// ✅ Fixed-width types use karo (folder 03 file 09)
// ✅ Explicit endianness handle karo
// ✅ Version number daalo file header mein
```

> **HFT relevance:** Market data files (PCAP, ITCH dumps) binary hote hain.
> Yahi patterns folder 38 mein use honge.

---

## File position (seeking)

```cpp
std::fstream f("data.bin", std::ios::in | std::ios::out | std::ios::binary);

// Position padho
std::streampos pos = f.tellg();      // get position (read)
std::streampos pos2 = f.tellp();     // put position (write)

// Position set karo
f.seekg(0);                              // shuru mein
f.seekg(0, std::ios::end);               // end mein
f.seekg(-10, std::ios::end);             // end se 10 bytes peeche
f.seekg(100, std::ios::beg);             // shuru se 100 bytes

// File size nikalna
f.seekg(0, std::ios::end);
auto size = f.tellg();
f.seekg(0, std::ios::beg);
```

**Better (C++17):**
```cpp
#include <filesystem>
auto size = std::filesystem::file_size("data.txt");
```

---

## Performance tips

```cpp
// 1. Bada buffer set karo (bade files ke liye)
std::ifstream in("big.txt");
char buffer[1 << 20];                          // 1 MB
in.rdbuf()->pubsetbuf(buffer, sizeof(buffer));

// 2. Line-by-line reading mein string reuse karo
std::string line;
line.reserve(256);                             // pehle se allocate
while (std::getline(in, line)) { /* ... */ }   // ✅ same string reuse hoti hai

// 3. endl use MAT karo files mein
out << "line\n";                               // ✅
out << "line" << std::endl;                    // ❌ har line pe flush = slow

// 4. Bahut bade files ke liye mmap (folder 29)
```

---

## Hands-on

`examples/06_file_io.cpp` chalao.

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 06_file_io.cpp -o fileio && ./fileio
cat output.txt
xxd data.bin | head
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`close()` call karna zaroori hai" | RAII kar deta hai. Sirf early close ke liye |
| "`ofstream` file mein add karta hai" | ❌ **Truncate** karta hai! `app` mode chahiye |
| "Binary mode ki zarurat nahi Linux pe" | Portable code ke liye hamesha likho |
| "Struct seedha write kar sakte ho" | Padding, endianness, sizes — sab verify karo |
| "File open hone ka check optional hai" | ⚠️ Hamesha check karo |

---

## Exercises

1. Ek file likho aur padho. `close()` mat call karo — verify karo ki RAII ne kaam kiya.

2. Truncate bug reproduce karo:
   ```cpp
   { std::ofstream out("t.txt"); out << "First\n"; }
   { std::ofstream out("t.txt"); out << "Second\n"; }     // First gaya!
   ```
   Ab `std::ios::app` se fix karo.

3. Ek file ki lines count karo.

4. CSV file padho aur fields parse karo (file 05 ka `split` use karo).

5. Binary record write/read karo `static_assert` ke saath.

6. File size teen tareekon se nikalo: `seekg`+`tellg`, `std::filesystem::file_size`,
   aur `stat()`.

7. Exception safety test:
   ```cpp
   void f() {
       std::ofstream out("t.txt");
       out << "before\n";
       throw std::runtime_error("boom");
   }
   try { f(); } catch (...) {}
   // t.txt mein "before" hai? (RAII ne flush kiya?)
   ```

8. Ek log file appender likho jo har run mein timestamp ke saath line add kare.

---

## Interview questions

1. `ofstream` default mein file truncate karta hai ya append?
2. RAII file streams mein kaise kaam karta hai?
3. Binary mode kab zaroori hai?
4. Struct ko binary write karne mein kya problems hain?
5. Exception aane pe file band hoti hai?

---

## Next
→ [`10-string-streams.md`](10-string-streams.md)
