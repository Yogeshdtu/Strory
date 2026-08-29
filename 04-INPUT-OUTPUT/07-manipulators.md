# 07 — Manipulators (`<iomanip>`)

## Prerequisites
`01-cout-deep-dive.md`, folder 03 file 06 (floating point)

## Yeh topic abhi kyun
Numbers ko theek se dikhana zaroori hai — tables, prices, percentages. Aur floating
point ki precision control karna, jo folder 03 mein aapne dekha tha kitna important hai.

---

## Manipulator kya hai?

Ek **special object/function** jo stream ke behaviour ko badal deta hai.

```cpp
std::cout << std::hex << 255;      // ff
std::cout << std::setw(10) << 42;  //         42
```

Do type ke hote hain:
- **`<iostream>` mein** — `endl`, `flush`, `hex`, `dec`, `boolalpha`
- **`<iomanip>` mein** — `setw`, `setprecision`, `setfill`, `setbase`

---

## 🔑 Sticky vs one-shot — sabse important baat

| Manipulator | Sticky? |
|---|---|
| `std::setw()` | ❌ **sirf agli value pe** |
| Baaki sab (`setprecision`, `hex`, `fixed`, `setfill`...) | ✅ **jab tak badlo mat** |

```cpp
std::cout << std::setw(10) << 1 << 2 << 3 << "\n";
//         ^^^^^^^^^^^^^^ sirf `1` pe laga
// Output: "         123"

std::cout << std::hex << 255 << " " << 16 << "\n";
//         ^^^^^^^^^ sab pe laga
// Output: "ff 10"
```

**Yeh sabse common manipulator bug hai.**

Alignment ke liye har value pe `setw` dobara lagana padta hai:
```cpp
std::cout << std::setw(10) << 1
          << std::setw(10) << 2
          << std::setw(10) << 3 << "\n";
```

---

## Width aur alignment

```cpp
#include <iomanip>

std::cout << std::setw(10) << 42 << "|\n";           //         42|
std::cout << std::left  << std::setw(10) << 42 << "|\n";  // 42        |
std::cout << std::right << std::setw(10) << 42 << "|\n";  //         42|
std::cout << std::internal << std::setw(10) << -42 << "|\n"; // -       42|

std::cout << std::setfill('0') << std::setw(5) << 42 << "\n";   // 00042
std::cout << std::setfill('.') << std::setw(10) << "abc" << "\n"; // .......abc
std::cout << std::setfill(' ');      // reset karo!
```

⚠️ **`setfill` sticky hai** — reset karna mat bhoolna, warna aage sab kuch `0` se
bhar jaayega.

### Table banana

```cpp
std::cout << std::left
          << std::setw(12) << "Symbol"
          << std::right
          << std::setw(10) << "Price"
          << std::setw(8)  << "Qty" << "\n";
std::cout << std::string(30, '-') << "\n";

std::cout << std::left  << std::setw(12) << "NIFTY"
          << std::right << std::setw(10) << 21500
          << std::setw(8)  << 100 << "\n";
```

Output:
```
Symbol           Price     Qty
------------------------------
NIFTY            21500     100
```

---

## Floating point formatting

Yeh sabse zyada use hota hai.

```cpp
double pi = 3.14159265358979;

// Default: 6 SIGNIFICANT digits
std::cout << pi << "\n";                                    // 3.14159

// setprecision -- default mode mein SIGNIFICANT digits
std::cout << std::setprecision(3) << pi << "\n";            // 3.14
std::cout << std::setprecision(10) << pi << "\n";           // 3.141592654

// fixed -- decimal ke BAAD ke digits
std::cout << std::fixed << std::setprecision(2) << pi << "\n";   // 3.14
std::cout << std::fixed << std::setprecision(4) << pi << "\n";   // 3.1416

// scientific
std::cout << std::scientific << std::setprecision(3) << pi << "\n"; // 3.142e+00

// hexfloat (C++11)
std::cout << std::hexfloat << pi << "\n";                   // 0x1.921fb5...p+1

// Reset
std::cout << std::defaultfloat;
```

### 🔑 `setprecision` ka matlab mode pe depend karta hai

```cpp
double x = 1234.5678;

std::cout << std::setprecision(3) << x << "\n";
//  default mode -> 3 SIGNIFICANT digits -> 1.23e+03

std::cout << std::fixed << std::setprecision(3) << x << "\n";
//  fixed mode -> 3 digits AFTER decimal -> 1234.568
```

**Yeh confuse karta hai.** Money/prices ke liye hamesha `fixed` use karo.

### Money formatting

```cpp
double price = 21500.5;
std::cout << "Rs " << std::fixed << std::setprecision(2) << price << "\n";  // Rs 21500.50
```

⚠️ **Lekin yaad rakho (folder 03 file 06):** money ke liye `double` use mat karo!
Integer paise use karo:

```cpp
std::int64_t paise = 2150050;
std::cout << "Rs " << (paise / 100) << "."
          << std::setfill('0') << std::setw(2) << (paise % 100)
          << std::setfill(' ') << "\n";                      // Rs 21500.50
```

---

## Number base

```cpp
int n = 255;

std::cout << std::dec << n << "\n";      // 255
std::cout << std::oct << n << "\n";      // 377
std::cout << std::hex << n << "\n";      // ff
std::cout << std::uppercase << std::hex << n << "\n";   // FF
std::cout << std::showbase << std::hex << n << "\n";    // 0xff

std::cout << std::dec;                    // reset -- ZAROORI hai
```

⚠️ **`hex` sticky hai.** Reset karna bhool gaye to aage sab numbers hex mein aayenge.
Yeh silent confusion banata hai.

### Hex dump (practical)

```cpp
void hexDump(const unsigned char* data, std::size_t len) {
    std::cout << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < len; ++i) {
        std::cout << std::setw(2) << static_cast<int>(data[i]) << " ";
        if ((i + 1) % 16 == 0) std::cout << "\n";
    }
    std::cout << std::dec << std::setfill(' ') << "\n";
}
```

Market data debugging mein yeh bahut kaam aata hai.

---

## Bool, sign, aur baaki

```cpp
std::cout << std::boolalpha << true << "\n";        // true
std::cout << std::noboolalpha << true << "\n";      // 1

std::cout << std::showpos << 42 << "\n";            // +42
std::cout << std::noshowpos << 42 << "\n";          // 42

std::cout << std::showpoint << 1.0 << "\n";         // 1.00000
std::cout << std::noshowpoint << 1.0 << "\n";       // 1
```

---

## State save/restore — RAII pattern

Manipulators sticky hain, isliye function se bahar nikalte waqt state restore karna
achhi practice hai:

```cpp
#include <ios>

class IosFlagSaver {
    std::ios& stream_;
    std::ios::fmtflags flags_;
    std::streamsize precision_;
    char fill_;
public:
    explicit IosFlagSaver(std::ios& s)
        : stream_(s), flags_(s.flags()),
          precision_(s.precision()), fill_(s.fill()) {}

    ~IosFlagSaver() {
        stream_.flags(flags_);
        stream_.precision(precision_);
        stream_.fill(fill_);
    }

    IosFlagSaver(const IosFlagSaver&) = delete;
    IosFlagSaver& operator=(const IosFlagSaver&) = delete;
};

void printHex(int n) {
    IosFlagSaver saver(std::cout);          // ✅ automatic restore
    std::cout << std::hex << std::showbase << n << "\n";
}   // <- yahan flags restore ho gaye

int main() {
    printHex(255);
    std::cout << 255 << "\n";               // ✅ 255 (decimal) -- state kharab nahi hui
}
```

Yeh RAII ka ek aur example hai (folder 17).

---

## Member function versions

Manipulators ke equivalent member functions bhi hain:

```cpp
std::cout.width(10);                    // ≡ std::setw(10)
std::cout.precision(3);                 // ≡ std::setprecision(3)
std::cout.fill('0');                    // ≡ std::setfill('0')
std::cout.setf(std::ios::fixed);        // ≡ std::fixed
std::cout.unsetf(std::ios::fixed);
std::cout.flags(std::ios::hex | std::ios::showbase);
```

Manipulators zyada readable hain, par member functions se aap current value **padh**
bhi sakte ho:
```cpp
auto oldPrec = std::cout.precision();
```

---

## ⚠️ `std::format` behtar hai (C++20)

Manipulators verbose aur error-prone hain. C++20 mein `std::format` hai:

```cpp
// Manipulators
std::cout << std::fixed << std::setprecision(2)
          << std::setw(10) << price << "\n";
std::cout << std::defaultfloat << std::setprecision(6);   // reset karna padta hai

// std::format -- ek line, koi state nahi
std::cout << std::format("{:>10.2f}\n", price);
```

Detail agli file mein.

---

## Hands-on

`examples/04_manipulators.cpp` chalao.

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 04_manipulators.cpp -o manip && ./manip
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`setw` sticky hai" | ❌ Sirf agli value pe lagta hai |
| "`setprecision` hamesha decimals dikhata hai" | Default mode mein **significant digits** |
| "`hex` ek baar use karke bhool jao" | Sticky hai — `std::dec` se reset karo |
| "`setfill` reset ki zarurat nahi" | Reset karo, warna aage sab bhar jaayega |
| "Manipulators value badalte hain" | Sirf **display** badalte hain |

---

## Exercises

1. Yeh table exactly banao:
   ```
   Symbol          Price      Qty     Value
   ----------------------------------------
   NIFTY        21500.50      100   2150050
   BANKNIFTY    46200.25       50   2310013
   ```

2. `setw` ka sticky-nahi behaviour demonstrate karo:
   ```cpp
   std::cout << std::setw(8) << 1 << 2 << 3 << "\n";
   std::cout << std::setw(8) << 1 << std::setw(8) << 2 << std::setw(8) << 3 << "\n";
   ```

3. `setprecision` ka mode-dependent behaviour test karo:
   ```cpp
   double x = 1234.5678;
   std::cout << std::setprecision(3) << x << "\n";
   std::cout << std::fixed << std::setprecision(3) << x << "\n";
   ```

4. `IosFlagSaver` class likho aur test karo ki state restore hota hai.

5. Ek `hexDump()` function likho jo 16 bytes per line dikhaye, offset ke saath:
   ```
   0000: 41 42 43 44 45 46 47 48  49 4a 4b 4c 4d 4e 4f 50
   ```

6. Money formatter likho (integer paise se):
   ```cpp
   std::string formatMoney(std::int64_t paise);   // 2150050 -> "Rs 21,500.50"
   ```
   (Comma separators add karna bonus hai.)

7. `hex` reset bhoolne ka bug reproduce karo:
   ```cpp
   std::cout << std::hex << 255 << "\n";
   std::cout << 100 << "\n";              // 64, not 100!
   ```

---

## Interview questions

1. `setw` aur `setprecision` mein sticky behaviour ka fark?
2. `setprecision` `fixed` ke saath aur bina uske alag kaise hai?
3. Manipulator state restore kaise karte hain?
4. `std::format` manipulators se behtar kyun hai?

---

## Next
→ [`08-std-format.md`](08-std-format.md)
