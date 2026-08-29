# 02 — `std::cin` aur input

## Prerequisites
`01-cout-deep-dive.md`

## Yeh topic abhi kyun
Ab tak aapke programs **baat nahi karte the** — bas output dete the. Ab woh user se
input lenge. Iske bina koi interactive program nahi ban sakta.

---

## Basic

```cpp
#include <iostream>

int main() {
    int age;
    std::cout << "Aapki umar? ";
    std::cin >> age;                    // input lo
    std::cout << "Aap " << age << " saal ke ho\n";
}
```

`>>` = **stream extraction operator** (`<<` ka ulta).

Yaad rakhne ka tareeka:
```
   cout << data      // data cout ki taraf JA raha hai
   cin  >> data      // data cin se AA raha hai
```

Arrows hamesha **data ke bahav ki taraf** point karte hain.

---

## `>>` kaise kaam karta hai

```cpp
int x;
std::cin >> x;
```

Steps:
1. **Leading whitespace SKIP karta hai** (space, tab, newline)
2. Us type ke liye jitne characters valid hain, utne padhta hai
3. Pehle invalid character pe **ruk jaata hai** (aur use buffer mein chhod deta hai)
4. Value convert karke variable mein daal deta hai

### Example trace

Input: `   42abc`

```cpp
int x;
std::cin >> x;
```

```
   Buffer: "   42abc\n"
            ^^^         <- whitespace skip
               ^^       <- "42" padha (int ke liye valid)
                 ^      <- 'a' pe ruk gaya (int ke liye invalid)
   
   x = 42
   Buffer mein bacha: "abc\n"
```

---

## Chaining

```cpp
int a, b, c;
std::cin >> a >> b >> c;
```

Input `1 2 3` de sakte ho, ya:
```
1
2
3
```

**Dono chalega** — kyunki `>>` whitespace (newline bhi) skip karta hai.

---

## ⚠️ Trap 1: Whitespace pe ruk jaata hai

```cpp
std::string name;
std::cout << "Poora naam? ";
std::cin >> name;
std::cout << "Namaste " << name << "\n";
```

Input: `Rahul Sharma`
Output: `Namaste Rahul` ← **"Sharma" gayab!**

**Kyun?** `>>` space pe ruk jaata hai.

**Fix:** `std::getline` use karo (file 05 mein detail):
```cpp
std::string name;
std::getline(std::cin, name);      // ✅ poori line
```

---

## ⚠️ Trap 2: Type mismatch se stream FAIL ho jaata hai

```cpp
int age;
std::cin >> age;
```

Agar user `abc` likhe:
1. `>>` `abc` ko `int` mein convert nahi kar paata
2. Stream **fail state** mein chala jaata hai
3. `age` ko `0` set kar diya jaata hai (C++11 se)
4. `abc` **buffer mein hi pada rehta hai**
5. **Har aage ka `>>` turant fail hoga** (kyunki stream fail state mein hai)

```cpp
int a, b;
std::cin >> a;      // "abc" -> FAIL
std::cin >> b;      // turant fail -- kuch padha hi nahi
std::cout << a << " " << b;      // 0 0
```

### Yeh infinite loop banata hai

```cpp
int num;
while (true) {
    std::cout << "Number: ";
    std::cin >> num;              // fail state mein kuch nahi padhta
    std::cout << num << "\n";     // ⚠️ INFINITE LOOP
}
```

**Fix file 06 mein** (input validation).

---

## ⚠️ Trap 3: `>>` newline chhod deta hai

Yeh **sabse common beginner bug** hai.

```cpp
int age;
std::string name;

std::cin >> age;                  // user: "25\n"
                                  //   "25" padha, "\n" BUFFER MEIN CHHOD DIYA
std::getline(std::cin, name);     // ⚠️ turant "\n" padh liya -> name KHALI!

std::cout << "Naam: [" << name << "]\n";     // Naam: []
```

**Kya hua:**
```
   Buffer: "25\n"
   cin >> age      -> "25" padha, x=25.  Buffer: "\n"
   getline(...)    -> "\n" tak padha (kuch nahi mila) -> name = ""
```

### Fix

```cpp
#include <limits>

int age;
std::string name;

std::cin >> age;
std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');   // ✅ line saaf karo
std::getline(std::cin, name);
```

`ignore(n, delim)` = "n characters tak skip karo, ya `delim` milne tak — jo pehle ho."

**Shortcut (thoda kam robust):**
```cpp
std::cin.ignore();      // sirf 1 character skip (usually kaafi hota hai)
```

**Rule:** `>>` ke baad `getline` use karna hai? To beech mein `ignore()` daalo.

---

## `cin` ke member functions

```cpp
char c;
std::cin.get(c);              // ek character (whitespace bhi!)
c = std::cin.get();           // ek character, int return karta hai

std::cin.getline(buffer, 100);   // C-string version (std::getline prefer karo)

std::cin.peek();              // agla character DEKHO, padho mat
std::cin.unget();             // pichla character wapas buffer mein
std::cin.putback(c);          // ek character wapas daalo

std::cin.ignore(n, delim);    // skip karo
std::cin.ignore();            // 1 character skip

std::cin.gcount();            // pichli unformatted read mein kitne chars aaye
```

### `>>` vs `get()`

```cpp
char c;
std::cin >> c;       // whitespace SKIP karta hai
std::cin.get(c);     // whitespace bhi PADHTA hai (space, \n sab)
```

---

## Reading until EOF

```cpp
int num;
while (std::cin >> num) {          // ✅ idiomatic
    std::cout << num * 2 << "\n";
}
```

**Yeh kaam kaise karta hai?**

`std::cin >> num` `istream&` return karta hai. Aur `istream` ka ek
**`explicit operator bool()`** hai jo batata hai ki stream theek hai ya nahi.

```cpp
while (std::cin >> num)     // ≡  while (static_cast<bool>(std::cin >> num))
```

Loop tab tak chalta hai jab tak:
- EOF na aaye (Ctrl+D Linux/Mac, Ctrl+Z Windows), ya
- Koi conversion fail na ho

### Lines ke liye
```cpp
std::string line;
while (std::getline(std::cin, line)) {
    std::cout << "Line: " << line << "\n";
}
```

---

## `cin` `cout` se juda hua hai (tie)

```cpp
std::cout << "Naam? ";       // koi \n nahi -- buffer mein pada hai
std::cin >> name;            // ⚠️ prompt dikha bhi nahi?
```

Actually **dikhega** — kyunki `std::cin` `std::cout` se **tied** hai.
Har `cin` operation se pehle `cout` automatically flush ho jaata hai.

```cpp
std::cin.tie(&std::cout);      // yeh default hai
std::cin.tie(nullptr);         // tie hatao -- FAST, par prompts atak sakte hain
```

Performance ke liye tie hatana file 12 mein.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > input.cpp << 'END'
#include <iostream>
#include <string>
#include <limits>

int main() {
    std::cout << "===== BASIC INPUT =====\n";
    int age;
    std::cout << "Aapki umar? ";
    std::cin >> age;
    std::cout << "Aap " << age << " saal ke ho\n";

    std::cout << "\n===== THE NEWLINE TRAP =====\n";
    std::string name;
    std::cout << "Poora naam? ";

    // ⚠️ Bina ignore() ke, getline turant khali string dega
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, name);

    std::cout << "Namaste, [" << name << "]\n";
    std::cout << "(agar ignore() nahi hota, yeh KHALI hota)\n";

    std::cout << "\n===== MULTIPLE VALUES =====\n";
    int a, b;
    std::cout << "Do numbers (space se alag): ";
    std::cin >> a >> b;
    std::cout << a << " + " << b << " = " << (a + b) << "\n";

    std::cout << "\n===== STREAM STATE =====\n";
    std::cout << "good(): " << std::cin.good() << "\n";
    std::cout << "fail(): " << std::cin.fail() << "\n";
    std::cout << "eof():  " << std::cin.eof()  << "\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra input.cpp -o input && ./input
```

**Ab jaan-boojh kar galat input do** — umar mein `abc` likho. Kya hua?

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`cin >> name` poori line padhta hai" | Space pe ruk jaata hai. `getline` chahiye |
| "Galat input pe program crash hoga" | Nahi — stream fail state mein jaata hai, chalta rehta hai |
| "`>>` ke baad `getline` seedha kaam karega" | ❌ Newline atka hota hai. `ignore()` chahiye |
| "`cin.get()` `>>` jaisa hai" | `get()` whitespace bhi padhta hai |
| "Prompt ke baad `endl` chahiye" | Nahi — `cin` `cout` se tied hai, auto-flush hota hai |

---

## Exercises

1. `input.cpp` chalao. Umar mein `abc` daalo. Kya hua? Baaki input kaam kiya?

2. `ignore()` wali line hata do aur dobara chalao. `name` khali aa gaya?

3. Ek program likho jo user se 3 numbers le aur unka average print kare.

4. Yeh program mein bug hai. Dhoondho aur fix karo:
   ```cpp
   int count;
   std::string item;
   std::cout << "Kitne? ";
   std::cin >> count;
   std::cout << "Kya? ";
   std::getline(std::cin, item);
   std::cout << count << " x " << item << "\n";
   ```
   <details><summary>Answer</summary>
   Newline trap. `std::cin >> count` ke baad `\n` buffer mein hai.
   Fix: `std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');`
   `getline` se pehle.
   </details>

5. EOF tak numbers padho aur unka sum print karo:
   ```cpp
   int num, sum = 0;
   while (std::cin >> num) sum += num;
   std::cout << "Sum: " << sum << "\n";
   ```
   Chalao aur `Ctrl+D` (Linux/Mac) ya `Ctrl+Z` (Windows) se end karo.

6. `peek()` use karke bina padhe agla character dekho:
   ```cpp
   std::cout << "Agla char: " << (char)std::cin.peek() << "\n";
   char c;
   std::cin >> c;
   std::cout << "Padha: " << c << "\n";
   ```

7. `>>` aur `get()` ka fark test karo — space daalke dekho.

---

## Interview questions

1. `>>` whitespace ke saath kya karta hai?
2. Galat type ka input dene pe kya hota hai?
3. `>>` ke baad `getline` kyun fail karta hai?
4. `while (std::cin >> x)` kaise kaam karta hai?
5. `cin.tie()` kya karta hai?

---

## Next
→ [`03-buffering-and-flushing.md`](03-buffering-and-flushing.md)
