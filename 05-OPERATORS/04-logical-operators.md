# 04 — Logical operators aur short-circuit

## Prerequisites
`03-comparison-operators.md`, folder 03 file 08 (`bool`)

## Yeh topic abhi kyun
Conditions ko combine karna. Aur **short-circuit evaluation** — jo safety aur
performance dono ke liye critical hai.

---

## Teen operators

```cpp
a && b      // AND  -- dono true?
a || b      // OR   -- koi ek true?
!a          // NOT  -- ulta karo
```

### Truth tables

| `a` | `b` | `a && b` | `a \|\| b` |
|---|---|---|---|
| false | false | false | false |
| false | true  | false | true |
| true  | false | false | true |
| true  | true  | **true** | true |

| `a` | `!a` |
|---|---|
| false | true |
| true | false |

---

## 🔑 SHORT-CIRCUIT EVALUATION

**Yeh is file ka sabse important concept hai.**

```cpp
a && b      // agar `a` FALSE hai -> `b` EVALUATE HI NAHI HOTA
a || b      // agar `a` TRUE  hai -> `b` EVALUATE HI NAHI HOTA
```

**Kyun?** Kyunki result already pata hai:
- `false && anything` = always `false`
- `true || anything` = always `true`

### Yeh guaranteed hai
C++ standard guarantee karta hai:
1. Left operand **pehle** evaluate hota hai
2. Agar result decide ho gaya, right operand **evaluate nahi hota**
3. Left aur right ke beech ek **sequence point** hai

---

## Use case 1: SAFETY (null checks)

```cpp
// ✅ Safe -- agar ptr null hai, ptr->value chalega hi nahi
if (ptr != nullptr && ptr->value > 5) { }

// ❌ CRASH -- ptr->value pehle evaluate hoga
if (ptr->value > 5 && ptr != nullptr) { }
```

**Order matter karta hai.** Check pehle, use baad mein.

### Array bounds
```cpp
// ✅ Safe
if (index < size && arr[index] == target) { }

// ❌ Out of bounds access
if (arr[index] == target && index < size) { }
```

### Empty container
```cpp
// ✅ Safe
if (!v.empty() && v[0] == target) { }
if (v.size() > 0 && v.front() == target) { }
```

---

## Use case 2: PERFORMANCE

**Sasta check pehle, mehnga baad mein.**

```cpp
// ✅ Fast path pehle
if (cheapCheck() && expensiveCheck()) { }

// ❌ Hamesha mehnga check chalega
if (expensiveCheck() && cheapCheck()) { }
```

```cpp
// Real example
if (msg.type == MsgType::Order            // ✅ ek integer comparison
    && validateFullMessage(msg)) {         //    mehnga -- sirf zaroorat pe
    process(msg);
}
```

> **HFT relevance:** Hot path mein condition ka order matter karta hai. Sabse
> selective aur sasta check pehle rakho — usse zyada tar messages jaldi reject ho
> jaayenge aur mehnge checks kabhi chalenge hi nahi.

---

## Use case 3: Default values (`||` idiom)

```cpp
// Kuch languages mein:  x = a || b;
// C++ mein `||` bool deta hai, isliye:

int value = (input != 0) ? input : defaultValue;

// Ya C++17 se
if (auto v = tryGet(); v) { use(*v); }
```

---

## ⚠️ `&&` vs `&` — bahut important fark

```cpp
bool a = true, b = false;

a && b;     // LOGICAL AND  -- short-circuits
a &  b;     // BITWISE AND  -- dono ALWAYS evaluate
```

`bool` ke liye result **same** aata hai. Par behaviour alag hai:

```cpp
// ❌ CRASH
if (ptr != nullptr & ptr->value > 5) { }     // `&` -- dono evaluate honge!

// ✅ Safe
if (ptr != nullptr && ptr->value > 5) { }
```

**Yeh silent bug hai** — ek `&` bhoolne se crash.

### Aur `|` vs `||`
```cpp
if (checkA() | checkB()) { }      // ⚠️ dono chalenge
if (checkA() || checkB()) { }     // ✅ short-circuit
```

**Rule: booleans ke liye HAMESHA `&&` aur `||` use karo.** `&` aur `|` sirf
bitwise operations ke liye (file 05).

---

## ⚠️ Side effects aur short-circuit

```cpp
int i = 0;
if (false && (i++ > 0)) { }
std::cout << i;                   // 0 -- i++ CHALA HI NAHI

int j = 0;
if (true || (j++ > 0)) { }
std::cout << j;                   // 0 -- j++ CHALA HI NAHI
```

**Isliye conditions mein side effects se bacho:**
```cpp
// ❌ Confusing -- kab chalega pata nahi
if (checkA() && modifyGlobalState()) { }

// ✅ Clear
bool a = checkA();
if (a) {
    bool b = modifyGlobalState();
    // ...
}
```

---

## Truthiness (implicit bool conversion)

```cpp
if (5) { }              // true  (non-zero)
if (0) { }              // false
if (-1) { }             // true  (non-zero!)
if (0.0) { }            // false
if (0.1) { }            // true
if (ptr) { }            // true agar ptr != nullptr
if (nullptr) { }        // false
```

### Salah: explicit likho

```cpp
// ⚠️ Kaam karta hai, par ambiguous
if (count) { }
if (ptr) { }

// ✅ Clearer
if (count != 0) { }
if (ptr != nullptr) { }
```

**Exception:** `bool` variables ke liye seedha likho:
```cpp
if (isReady) { }              // ✅ best
if (isReady == true) { }      // ⚠️ redundant, aur `=` typo ka risk
if (!isReady) { }             // ✅
```

---

## De Morgan's laws

Conditions simplify karne ke liye:

```
   !(a && b)   ==   !a || !b
   !(a || b)   ==   !a && !b
```

```cpp
// Yeh dono same hain
if (!(isReady && hasData)) { }
if (!isReady || !hasData) { }

// Yeh bhi
if (!(x > 5 || y < 3)) { }
if (x <= 5 && y >= 3) { }
```

**Practical use:** Double negatives hatana.
```cpp
// ⚠️ Confusing
if (!(!isValid || !isReady)) { }

// ✅ De Morgan se simplify
if (isValid && isReady) { }
```

---

## Operator precedence

```
   !           (highest)
   ...
   <  <=  >  >=
   ==  !=
   &
   ^
   |
   &&
   ||          (lowest)
```

**Iska matlab:**
```cpp
a < b && c < d          // ≡ (a < b) && (c < d)     ✅ jo aap chahte the
a && b || c             // ≡ (a && b) || c          ⚠️ AND pehle
!a && b                 // ≡ (!a) && b              ✅
```

### ⚠️ `&&` aur `||` mix karne pe brackets lagao

```cpp
// ⚠️ Ambiguous padhne mein
if (a || b && c) { }

// ✅ Clear
if (a || (b && c)) { }
```

`-Wparentheses` isko warn karta hai.

---

## Branchless alternative (preview)

```cpp
// Branch ke saath
int result = 0;
if (condition) result = 1;

// Branchless -- bool ko int mein
int result = condition;              // true=1, false=0

// Counting without if
int count = 0;
for (int x : arr) count += (x > threshold);      // ✅ branchless
```

> **HFT relevance:** Branch misprediction ~15-20 cycles ki penalty deti hai.
> Agar branch unpredictable hai (50/50), branchless code tez ho sakta hai.
> Par agar branch **predictable** hai, branch version tez hai (CPU sahi guess
> karti hai, ~0 cost).
>
> **Isliye pehle measure karo, phir optimize.** Folder 31/36 mein poora.

---

## Hands-on

`examples/04_short_circuit.cpp` chalao.

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 04_short_circuit.cpp -o sc && ./sc
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`&` aur `&&` same hain" | `&&` short-circuits, `&` nahi |
| "Dono operands hamesha evaluate hote hain" | ❌ Short-circuit ho sakta hai |
| "Condition ka order matter nahi karta" | ⚠️ Safety aur performance dono ke liye karta hai |
| "`if (x == true)` clearer hai" | `if (x)` behtar — kam typo risk |
| "Branchless hamesha tez hai" | Predictable branches free hote hain |

---

## Exercises

1. Short-circuit demonstrate karo:
   ```cpp
   #include <iostream>
   bool sideEffect(const char* name) {
       std::cout << name << " chala! ";
       return true;
   }
   int main() {
       std::cout << "Test 1: "; if (false && sideEffect("A")) {} std::cout << "\n";
       std::cout << "Test 2: "; if (true  || sideEffect("B")) {} std::cout << "\n";
       std::cout << "Test 3: "; if (false &  sideEffect("C")) {} std::cout << "\n";
       std::cout << "Test 4: "; if (true  |  sideEffect("D")) {} std::cout << "\n";
   }
   ```
   Kaunse tests mein "chala!" dikha?
   <details><summary>Answer</summary>
   Test 3 aur 4 mein. `&` aur `|` short-circuit **nahi** karte.
   </details>

2. Null-safety bug reproduce karo:
   ```cpp
   struct Node { int value; };
   Node* ptr = nullptr;
   if (ptr->value > 5 && ptr != nullptr) { }     // ⚠️ crash
   ```
   Fix karo aur verify karo.

3. De Morgan se simplify karo:
   ```cpp
   if (!(isReady && hasData)) { }
   if (!(x > 5 || y < 3)) { }
   if (!(!a || !b)) { }
   ```

4. Precedence test karo:
   ```cpp
   bool a = true, b = false, c = true;
   std::cout << (a || b && c) << " ";
   std::cout << ((a || b) && c) << " ";
   std::cout << (a || (b && c)) << "\n";
   ```

5. Branchless counting likho:
   ```cpp
   // Array mein kitne elements > 100 hain? (bina if ke)
   ```

6. Performance ordering: ek condition likho jisme sasta check pehle ho.
   Phir order ulta karke timing compare karo (mehnge check ko `sleep` ya heavy
   computation banao).

---

## Interview questions

1. Short-circuit evaluation kya hai? Do use cases do.
2. `&&` aur `&` mein fark?
3. De Morgan's laws batao.
4. Condition ka order kyun matter karta hai?
5. Branchless code kab tez hota hai, kab nahi?

---

## Next
→ [`05-bitwise-operators.md`](05-bitwise-operators.md)
