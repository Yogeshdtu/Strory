# 01 — `std::cout` deep dive

## Prerequisites
Folder 02 lesson 02 (`std::cout` ka basic parichay), folder 03 (types)

## Yeh topic abhi kyun
Aapne `std::cout` folder 02 se use kiya hai. Ab uske **andar** dekhenge — kyunki
`<<` ka mechanism samajhna operator overloading (folder 15) ka pehla asli example hai.

---

## `cout` ek OBJECT hai, function nahi

Yeh dobara bol raha hoon kyunki yeh sabse common galatfehmi hai.

```cpp
namespace std {
    extern ostream cout;      // ek GLOBAL OBJECT
}
```

- Type: `std::ostream` (actually `std::basic_ostream<char>`)
- Yeh ek global object hai jo program start pe ban jaata hai
- Woh **stdout** (file descriptor 1) se juda hua hai

Iska matlab: `std::cout` ke paas **member functions** hain:

```cpp
std::cout.put('A');              // ek character
std::cout.write("Hello", 5);     // raw bytes
std::cout.flush();               // buffer khali karo
std::cout.good();                // stream theek hai?
```

---

## `<<` kaise kaam karta hai

`std::cout << 42` actually ek **function call** hai:

```cpp
operator<<(std::cout, 42);
```

`iostream` mein har type ke liye ek overload hai:

```cpp
// Member functions (built-in types ke liye)
ostream& operator<<(int);
ostream& operator<<(double);
ostream& operator<<(bool);
ostream& operator<<(const void*);

// Free functions (aur types ke liye)
ostream& operator<<(ostream&, const char*);
ostream& operator<<(ostream&, char);
ostream& operator<<(ostream&, const std::string&);
```

Compiler **overload resolution** karke sahi version chunta hai (folder 08 mein detail).

---

## Chaining ka mechanism

```cpp
std::cout << "Naam: " << name << ", Umar: " << age << "\n";
```

Yeh kaam karta hai kyunki **har `<<` `ostream&` return karta hai**:

```
   std::cout << "Naam: "     ->  returns std::cout&
   (std::cout) << name       ->  returns std::cout&
   (std::cout) << ", Umar: " ->  returns std::cout&
   (std::cout) << age        ->  returns std::cout&
   (std::cout) << "\n"       ->  returns std::cout&
```

**Left-to-right** chalta hai (kyunki `<<` left-associative hai).

### Yeh reference return karta hai, copy nahi

```cpp
ostream& operator<<(...)    // ^ yeh & important hai
```

Agar yeh copy return karta, to har `<<` mein ek naya stream banta — bilkul galat.
Streams **copy nahi ho sakte** (unka copy constructor deleted hai).

---

## ⚠️ Precedence trap

`<<` ki precedence samajhna zaroori hai:

```cpp
std::cout << 1 + 2 << "\n";       // 3   -- `+` ki precedence zyada hai
std::cout << 1 - 2 << "\n";       // -1  -- theek hai

std::cout << 1 < 2 << "\n";       // ❌ GALAT!
// Yeh parse hota hai: (std::cout << 1) < (2 << "\n")
// Kyunki << ki precedence < se ZYADA hai

std::cout << (1 < 2) << "\n";     // ✅ 1  -- brackets zaroori
```

### Rule
`<<` ki precedence:
- **kam** hai: `+`, `-`, `*`, `/`, `%` se → yeh sab pehle chalte hain ✅
- **zyada** hai: `<`, `>`, `==`, `&&`, `||`, `?:` se → inhe brackets chahiye ⚠️

```cpp
std::cout << a + b;          // ✅ theek
std::cout << a * b;          // ✅ theek
std::cout << (a < b);        // ⚠️ brackets chahiye
std::cout << (a == b);       // ⚠️ brackets chahiye
std::cout << (x ? "ha" : "na");  // ⚠️ brackets chahiye
```

**Simple rule:** agar comparison ya logical operator hai, brackets laga do.

---

## `cout` kaunse types print kar sakta hai?

```cpp
std::cout << 42;              // int
std::cout << 3.14;            // double
std::cout << 'A';             // char -> A
std::cout << "text";          // const char*
std::cout << true;            // bool -> 1
std::cout << std::string("s");// std::string
std::cout << nullptr;         // nullptr
std::cout << &someVar;        // pointer -> address (hex)
```

### ⚠️ Traps

**Trap 1: `char*` vs other pointers**
```cpp
int x = 5;
int* ip = &x;
char* cp = someCharBuffer;

std::cout << ip;        // address print hoga (0x7ffd...)
std::cout << cp;        // ⚠️ STRING print hogi! (char* ko string maana jaata hai)
std::cout << (void*)cp; // ✅ address print hoga
```

**Trap 2: `uint8_t` / `int8_t`**
```cpp
std::uint8_t v = 65;
std::cout << v;                       // "A" -- character!
std::cout << +v;                      // 65
std::cout << static_cast<int>(v);     // 65
```

(Yaad hai folder 03 file 09 se?)

**Trap 3: `bool`**
```cpp
std::cout << true;                    // 1
std::cout << std::boolalpha << true;  // true
```

**Trap 4: Custom types**
```cpp
struct Point { int x, y; };
Point p{1, 2};
std::cout << p;      // ❌ COMPILE ERROR -- koi overload nahi hai
```

Fix — apna overload likho (folder 15 mein detail):
```cpp
std::ostream& operator<<(std::ostream& os, const Point& p) {
    return os << "(" << p.x << ", " << p.y << ")";
}
std::cout << p;      // ✅ (1, 2)
```

---

## `cout` ke member functions

```cpp
// Raw output -- koi formatting nahi
std::cout.put('A');                 // ek char
std::cout.write("Hello", 5);        // exactly 5 bytes (embedded '\0' bhi)

// Buffer control
std::cout.flush();                  // buffer khali karo

// State
std::cout.good();                   // sab theek?
std::cout.fail();                   // koi formatting error?
std::cout.bad();                    // serious error?

// Formatting (yeh sab file 07 mein)
std::cout.width(10);
std::cout.precision(3);
std::cout.fill('0');
```

### `write()` kab kaam aata hai

```cpp
const char data[] = {'A', '\0', 'B'};
std::cout << data;              // "A" -- '\0' pe ruk gaya
std::cout.write(data, 3);       // "A\0B" -- teenon bytes
```

Binary data ke liye `write()` hi sahi hai.

---

## Performance ki jhalak

```cpp
std::cout << "a" << "b" << "c" << "\n";     // 4 alag operator<< calls
std::cout << "abc\n";                        // 1 call
```

Har `<<` ek function call hai. Chhoti strings ko **ek hi literal** mein jodna
thoda tez hai — aur padhne mein bhi behtar.

Aur adjacent string literals compiler khud jod deta hai:
```cpp
std::cout << "abc" "def" "\n";      // compile time pe "abcdef\n" ban gaya, 1 call
```

Poori I/O performance file 12 mein.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > coutdeep.cpp << 'END'
#include <iostream>
#include <string>

struct Point { int x, y; };

// Apna operator<< -- folder 15 mein poora padhenge
std::ostream& operator<<(std::ostream& os, const Point& p) {
    return os << "(" << p.x << ", " << p.y << ")";
}

int main() {
    std::cout << "===== CHAINING =====\n";
    std::cout << "A" << "B" << "C" << "\n";

    std::cout << "\n===== TYPES =====\n";
    std::cout << "int:     " << 42 << "\n";
    std::cout << "double:  " << 3.14 << "\n";
    std::cout << "char:    " << 'A' << "\n";
    std::cout << "bool:    " << true << " (boolalpha: "
              << std::boolalpha << true << std::noboolalpha << ")\n";
    std::cout << "string:  " << std::string("hello") << "\n";
    std::cout << "custom:  " << Point{1, 2} << "\n";

    std::cout << "\n===== PRECEDENCE =====\n";
    std::cout << "1 + 2   = " << 1 + 2 << "  (+ pehle chala)\n";
    std::cout << "(1 < 2) = " << (1 < 2) << "  (brackets ZAROORI the)\n";
    // std::cout << 1 < 2;    // ❌ uncomment karo -- error dekho

    std::cout << "\n===== POINTER TRAP =====\n";
    int x = 5;
    int* ip = &x;
    const char* cp = "text";
    std::cout << "int*:        " << ip << "   <- address\n";
    std::cout << "const char*: " << cp << "     <- STRING (address nahi!)\n";
    std::cout << "(void*)cp:   " << static_cast<const void*>(cp) << "  <- ab address\n";

    std::cout << "\n===== MEMBER FUNCTIONS =====\n";
    std::cout.put('X');
    std::cout.put('\n');
    std::cout.write("Hello", 5);
    std::cout.put('\n');

    const char data[] = {'A', '\0', 'B'};
    std::cout << "operator<<: " << data << "   <- \\0 pe ruk gaya\n";
    std::cout << "write():    ";
    std::cout.write(data, 3);
    std::cout << "   <- teenon bytes\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra coutdeep.cpp -o coutdeep && ./coutdeep
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`cout` ek function hai" | Ek `std::ostream` **object** hai |
| "`<<` ek arrow hai" | Overloaded operator hai (originally left-shift) |
| "`cout << a < b` chalega" | ❌ Precedence bug — brackets chahiye |
| "`char*` ka address print hoga" | ❌ String print hogi. `(void*)` cast karo |
| "Har type print ho jaata hai" | Custom types ke liye apna overload chahiye |

---

## Exercises

1. `coutdeep.cpp` chalao. Har section samjho.

2. `std::cout << 1 < 2;` uncomment karo. Error kya aaya? Kyun?
   <details><summary>Answer</summary>
   `(std::cout << 1)` ek `ostream&` deta hai, aur `ostream < int` ka koi
   `operator<` nahi hai → compile error. Precedence ki wajah se.
   </details>

3. Predict karo:
   ```cpp
   std::cout << 2 + 3 * 4 << " ";
   std::cout << (2 > 1) << " ";
   std::cout << 10 % 3 << "\n";
   ```
   <details><summary>Answer</summary>`14 1 1`</details>

4. `Point` ke liye `operator<<` likho jo `[x=1, y=2]` format mein print kare.

5. Ek `Money` struct banao (paise mein `int64_t`) aur uska `operator<<` likho jo
   `Rs 100.50` format mein print kare.
   <details><summary>Answer</summary>

   ```cpp
   #include <iomanip>
   struct Money { std::int64_t paise; };
   std::ostream& operator<<(std::ostream& os, const Money& m) {
       return os << "Rs " << (m.paise / 100) << "."
                 << std::setfill('0') << std::setw(2) << (m.paise % 100)
                 << std::setfill(' ');
   }
   ```
   </details>

6. `std::cout.write()` se ek binary buffer print karo jisme `\0` ho.

---

## Next
→ [`02-cin-and-input.md`](02-cin-and-input.md)
