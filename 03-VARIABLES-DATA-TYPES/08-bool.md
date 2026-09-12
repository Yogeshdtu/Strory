# 08 — `bool`

## Prerequisites
`05-int-deep-dive.md`

## Yeh topic abhi kyun
`bool` sabse simple type hai — sirf 2 values. Lekin uske conversions mein surprises
hain, aur `std::vector<bool>` ek famous C++ trap hai.

---

## Basic

```cpp
bool isMarketOpen = true;
bool hasError = false;
```

Sirf do values: `true` aur `false`.

**Size: 1 byte** (typically). Ek bit kaafi hoti, par memory **byte-addressable** hai
(yaad hai folder 01 lesson 11?), isliye minimum 1 byte.

```cpp
std::cout << sizeof(bool);      // 1
```

---

## Printing

```cpp
bool b = true;
std::cout << b;                          // 1   ⚠️ "true" nahi!
std::cout << std::boolalpha << b;        // true
std::cout << std::noboolalpha << b;      // 1   (wapas)
```

**Default mein `bool` `1`/`0` print hota hai.** `std::boolalpha` se `true`/`false`.

---

## Conversions: number → bool

**Rule: `0` = `false`, **kuch bhi aur** = `true`.**

```cpp
bool a = 0;         // false
bool b = 1;         // true
bool c = 42;        // true
bool d = -1;        // true  (non-zero hai)
bool e = 0.0;       // false
bool f = 0.1;       // true
bool g = nullptr;   // false
```

Isliye yeh kaam karta hai:
```cpp
int count = 5;
if (count) { }              // "agar count non-zero hai"
if (count != 0) { }         // ✅ same cheez, par CLEARER
```

**Salah:** Explicit comparison likho. `if (count != 0)` `if (count)` se behtar hai —
padhne wale ko turant pata chal jaata hai ki `count` ek number hai, bool nahi.

---

## Conversions: bool → number

```cpp
bool t = true;
bool f = false;
int a = t;          // 1
int b = f;          // 0
```

`true` **hamesha exactly 1** hota hai (`int` mein convert hone pe), aur `false` **0**.

### Isse trick nikalti hai
```cpp
// Kitne elements condition satisfy karte hain?
int count = 0;
for (int x : numbers) {
    count += (x > 100);      // true = 1, false = 0
}
```

Yeh **branchless** hai — koi `if` nahi. HFT mein yeh technique use hoti hai
(folder 36 mein detail).

---

## Logical operators (preview)

```cpp
bool a = true, b = false;

a && b      // AND: dono true?          -> false
a || b      // OR:  koi ek true?        -> true
!a          // NOT: ulta karo           -> false
```

### Short-circuit evaluation 🔑

```cpp
if (ptr != nullptr && ptr->value > 5) { }
//  ^^^^^^^^^^^^^^^^^^ agar yeh false hai, to doosra part CHALEGA HI NAHI
```

`&&` mein: agar left `false` hai, right evaluate nahi hota.
`||` mein: agar left `true` hai, right evaluate nahi hota.

Yeh **safety** ke liye use hota hai (null check) aur **performance** ke liye
(mehnga check baad mein rakho).

```cpp
// ✅ safe
if (index < size && arr[index] == target) { }

// ❌ crash ho sakta hai
if (arr[index] == target && index < size) { }
```

Detail folder 05 aur 06 mein.

---

## ⚠️ `std::vector<bool>` — C++ ka famous mistake

```cpp
std::vector<int>  vi(10);      // normal vector
std::vector<bool> vb(10);      // ⚠️ SPECIAL CASE!
```

`std::vector<bool>` ek **specialization** hai jo har bool ko **1 bit** mein store karti
hai, 1 byte mein nahi. Memory bachane ke liye.

**Lekin isse yeh problems aati hain:**

```cpp
std::vector<bool> v(10);

bool& ref = v[0];           // ❌ ERROR! v[0] bool& return nahi karta
auto x = v[0];              // ⚠️ x ka type bool NAHI hai, ek proxy object hai
bool* p = &v[0];            // ❌ ERROR! address nahi le sakte
```

`v[0]` ek **proxy object** return karta hai, `bool&` nahi. Isliye:
- Reference nahi le sakte
- Pointer nahi le sakte
- Generic code mein toot jaata hai
- Thread-safety issues (do threads alag "elements" likhein to same byte pe likhenge!)

### Solutions

```cpp
std::vector<char>     v1(10);          // ✅ 1 byte per element, normal behaviour
std::vector<uint8_t>  v2(10);          // ✅ same
std::deque<bool>      v3(10);          // ✅ normal (specialize nahi hua)
std::bitset<10>       v4;              // ✅ agar size compile time pe pata ho
std::array<bool, 10>  v5;              // ✅ fixed size
```

> **HFT relevance:** `std::vector<bool>` ke bit operations extra CPU instructions
> lete hain (mask, shift) aur **false sharing** create karte hain (do threads alag
> indices pe likhein, par same cache line — folder 32). HFT mein isse hamesha bacha
> jaata hai.

---

## `bool` aur branch prediction (HFT preview)

```cpp
if (someCondition) {
    // path A
} else {
    // path B
}
```

CPU **guess** karti hai ki kaunsa path chalega (branch prediction). Agar guess sahi:
~0 cost. Agar galat: **~15-20 cycles ki penalty** (pipeline flush).

```cpp
// ✅ Predictable -- CPU seekh legi
for (int i = 0; i < n; ++i) {
    if (i < n/2) doA(); else doB();     // pattern hai
}

// ❌ Unpredictable -- har baar 50% chance
for (int i = 0; i < n; ++i) {
    if (random() % 2) doA(); else doB();     // ~50% mispredict
}
```

**Branchless alternative:**
```cpp
// Branch ke saath
int max = (a > b) ? a : b;

// Branchless (compiler aksar khud kar deta hai - cmov instruction)
int max = a * (a > b) + b * (a <= b);
```

Yeh optimization folder 31/36 mein detail mein. Abhi bas jaan lo ki **`bool` ka
pattern performance affect karta hai.**

---

## `bool` ke saath common bugs

### Bug 1: `=` vs `==`
```cpp
bool flag = false;
if (flag = true) { }        // ⚠️ assignment! hamesha true
if (flag == true) { }       // ✅ comparison
if (flag) { }               // ✅ best
```

### Bug 2: `== true` likhna
```cpp
if (isReady == true) { }    // ⚠️ redundant, aur `=` typo ka risk
if (isReady) { }            // ✅ better
if (!isReady) { }           // ✅ negation ke liye
```

### Bug 3: Uninitialized bool
```cpp
bool flag;                  // ⚠️ garbage!
if (flag) { }               // random behaviour
```

Aur bhi bura: uninitialized `bool` ki value **`0` ya `1` ke alawa** kuch ho sakti hai
(memory mein jo garbage tha). Phir `flag` aur `!flag` **dono true** ho sakte hain! 😱

```cpp
bool flag{};      // ✅ false
bool flag = false; // ✅
```

### Bug 4: Bitwise vs Logical
```cpp
bool a = true, b = false;

a && b      // ✅ logical AND, short-circuits
a & b       // ⚠️ bitwise AND -- kaam karta hai par short-circuit NAHI karta

// Yeh crash karega:
if (ptr != nullptr & ptr->val > 5) { }     // ⚠️ dono evaluate honge!
if (ptr != nullptr && ptr->val > 5) { }    // ✅ safe
```

---

## Hands-on

```bash
cd ~/cpp-practice
cat > bools.cpp << 'END'
#include <iostream>
#include <vector>
#include <bitset>

int main() {
    std::cout << "===== BASIC =====\n";
    bool t = true, f = false;
    std::cout << "default print: " << t << " " << f << "\n";
    std::cout << std::boolalpha;
    std::cout << "boolalpha:     " << t << " " << f << "\n";
    std::cout << std::noboolalpha;
    std::cout << "sizeof(bool):  " << sizeof(bool) << " byte\n";

    std::cout << "\n===== NUMBER -> BOOL =====\n";
    std::cout << std::boolalpha;
    std::cout << "bool(0)    = " << static_cast<bool>(0)    << "\n";
    std::cout << "bool(1)    = " << static_cast<bool>(1)    << "\n";
    std::cout << "bool(42)   = " << static_cast<bool>(42)   << "\n";
    std::cout << "bool(-1)   = " << static_cast<bool>(-1)   << "\n";
    std::cout << "bool(0.0)  = " << static_cast<bool>(0.0)  << "\n";
    std::cout << "bool(0.1)  = " << static_cast<bool>(0.1)  << "\n";
    std::cout << std::noboolalpha;

    std::cout << "\n===== BOOL -> NUMBER =====\n";
    std::cout << "int(true)  = " << static_cast<int>(true)  << "\n";
    std::cout << "int(false) = " << static_cast<int>(false) << "\n";

    std::cout << "\n===== BRANCHLESS COUNTING =====\n";
    int numbers[] = {50, 150, 200, 80, 300, 20};
    int count = 0;
    for (int x : numbers) {
        count += (x > 100);      // true=1, false=0 -- koi if nahi!
    }
    std::cout << "100 se bade: " << count << "\n";

    std::cout << "\n===== SHORT CIRCUIT =====\n";
    int* nullPtr = nullptr;
    // Agar && short-circuit na karta, yeh crash karta:
    if (nullPtr != nullptr && *nullPtr > 5) {
        std::cout << "kabhi nahi chalega\n";
    } else {
        std::cout << "short-circuit ne bacha liya (crash nahi hua)\n";
    }

    std::cout << "\n===== vector<bool> TRAP =====\n";
    std::vector<int>  vi = {1, 0, 1};
    std::vector<bool> vb = {true, false, true};

    std::cout << "vector<int> size: " << vi.size() << "\n";
    std::cout << "vector<bool> size: " << vb.size() << "\n";

    int& refInt = vi[0];          // ✅ kaam karta hai
    refInt = 99;
    std::cout << "vi[0] after ref change: " << vi[0] << "\n";

    // bool& refBool = vb[0];     // ❌ COMPILE ERROR -- uncomment karke dekho
    std::cout << "bool& ref = vb[0];  <- yeh COMPILE nahi hoga!\n";
    std::cout << "(vector<bool> proxy object return karta hai, reference nahi)\n";

    std::cout << "\n===== ALTERNATIVES =====\n";
    std::vector<char> safeBools = {1, 0, 1};
    char& refChar = safeBools[0];      // ✅ kaam karta hai
    refChar = 0;
    std::cout << "vector<char> works: " << static_cast<int>(safeBools[0]) << "\n";

    std::bitset<8> bits{0b10110101};
    std::cout << "bitset<8>: " << bits << ", count of 1s: " << bits.count() << "\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra bools.cpp -o bools && ./bools
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`bool` 1 bit ka hota hai" | 1 **byte** (memory byte-addressable hai) |
| "`cout << true` 'true' print karta hai" | `1` print karta hai. `boolalpha` chahiye |
| "`std::vector<bool>` normal vector hai" | ❌ Special bit-packed specialization hai |
| "`if (x == true)` behtar hai" | `if (x)` behtar hai — kam typo risk |
| "`&` aur `&&` same hain bools ke liye" | `&&` short-circuits, `&` nahi |
| "Uninitialized bool false hoga" | ❌ Garbage. `flag` aur `!flag` dono true ho sakte hain |

---

## Exercises

1. `bools.cpp` chalao. `bool& refBool = vb[0];` uncomment karo — kya error aaya?

2. Predict karo:
   ```cpp
   std::cout << (true + true) << "\n";
   std::cout << (true * 5) << "\n";
   std::cout << !5 << "\n";
   std::cout << !!5 << "\n";
   ```
   <details><summary>Answer</summary>
   `2`, `5`, `0`, `1`
   - `true + true` = 1 + 1 = 2 (int mein promote)
   - `!5` = `!true` = `false` = 0
   - `!!5` = `!false` = `true` = 1 (yeh "bool mein convert karo" ka purana idiom hai)
   </details>

3. Branchless counting likho: array mein kitne even numbers hain (bina `if` ke)?
   <details><summary>Answer</summary>

   ```cpp
   int count = 0;
   for (int x : arr) count += (x % 2 == 0);
   ```
   </details>

4. Short-circuit test:
   ```cpp
   #include <iostream>
   bool sideEffect() { std::cout << "chala! "; return true; }
   int main() {
       std::cout << "Test 1: ";
       if (false && sideEffect()) { }
       std::cout << "\nTest 2: ";
       if (true || sideEffect()) { }
       std::cout << "\nTest 3: ";
       if (false & sideEffect()) { }
       std::cout << "\n";
   }
   ```
   Kaunse tests mein "chala!" dikha?
   <details><summary>Answer</summary>
   Sirf Test 3 mein. `&&` aur `||` short-circuit karte hain, `&` nahi.
   </details>

5. Ek `bool` array ke liye `std::vector<bool>` aur `std::vector<char>` ka memory usage
   compare karo (1 million elements).

---

## Interview questions

1. `bool` ki size kya hai aur kyun?
2. `std::vector<bool>` mein kya problem hai?
3. `&&` aur `&` mein kya fark hai?
4. Short-circuit evaluation kya hai? Ek use case do.
5. Uninitialized `bool` khatarnaak kyun hai?

---

## Next
→ [`09-fixed-width-types.md`](09-fixed-width-types.md)
