# 10 — Evaluation order aur sequencing

## Prerequisites
`09-precedence-associativity.md`, `02-increment-decrement.md`

## Yeh topic abhi kyun
**Precedence batata hai ki operators kaise GROUP hote hain.
Evaluation order batata hai ki woh kab CHALTE hain.**

Yeh do alag cheezein hain — aur inka fark na samajhna UB ka source hai.

---

## Precedence ≠ Evaluation order

```cpp
f() + g() * h()
```

**Precedence** batata hai: `f() + (g() * h())` — grouping.

**Evaluation order** batata hai: `f`, `g`, `h` **kis order mein call honge**?

**Jawab: UNSPECIFIED.** Compiler koi bhi order chun sakta hai.

```cpp
int a() { std::cout << "a"; return 1; }
int b() { std::cout << "b"; return 2; }
int c() { std::cout << "c"; return 3; }

int x = a() + b() * c();
// Output: "abc" ya "cba" ya "bca" ... koi bhi ho sakta hai
```

**Yeh UB nahi hai** — bas unspecified hai. Result correct hoga, par order predict
nahi kar sakte.

---

## Sequencing rules (C++11 terminology)

Purani C++ mein "sequence points" the. C++11 se better terminology hai:

| Term | Matlab |
|---|---|
| **sequenced-before** | A pakka B se pehle |
| **sequenced-after** | A pakka B ke baad |
| **unsequenced** | koi order nahi, **overlap bhi ho sakta hai** ⚠️ |
| **indeterminately sequenced** | ek pehle chalega, par pata nahi kaunsa (overlap nahi) |

---

## Kahan order GUARANTEED hai

### 1. `;` — full expression boundary
```cpp
a();            // pakka pehle
b();            // pakka baad mein
```

### 2. `&&`, `||` — short-circuit
```cpp
f() && g();     // f() pakka pehle, aur g() shayad chale hi na
f() || g();     // f() pakka pehle
```

### 3. `?:` — condition pehle
```cpp
cond() ? a() : b();      // cond() pakka pehle, phir SIRF ek branch
```

### 4. `,` comma operator
```cpp
a(), b();       // a() pakka pehle
```

### 5. C++17 ne yeh add kiye ✅
```cpp
a[b]                    // a pehle, phir b (C++17)
a->b                    // a pehle
a << b                  // a pehle, phir b  ← isliye cout chaining safe hai
a >> b                  // a pehle
a = b                   // b PEHLE, phir a  ← right side pehle
a += b                  // b pehle
new T(expr)             // expr pehle
```

**C++17 se pehle** yeh sab unspecified the!

### 6. Function call
```cpp
f(a(), b(), c());
// a, b, c ka order UNSPECIFIED (C++17 mein bhi!)
// par har argument POORA evaluate hota hai doosre se pehle
// (indeterminately sequenced -- overlap nahi hota)
```

---

## ⚠️ Kahan order NAHI hai — UB ka khatra

### Rule
> **Agar ek scalar object ko ek expression mein DO baar modify karo,
> aur woh unsequenced hon → UNDEFINED BEHAVIOUR.**

```cpp
int i = 0;
i = i++ + ++i;              // ⚠️ UB (C++17 mein bhi confusing)
i = ++i + i++;              // ⚠️ UB
arr[i] = i++;               // ⚠️ UB (C++17 se pehle)
f(i++, i++);                // ⚠️ unspecified order
++i = i;                    // ⚠️
i = i++;                    // ⚠️ UB
```

### Classic example
```cpp
int i = 5;
i = i++;
// GCC:   i = 5 ya 6
// Clang: kuch aur
// -O2:   shayad kuch aur
// ⚠️ Standard kehta hai: kuch bhi ho sakta hai
```

---

## C++17 ne kya fix kiya

C++17 se yeh **defined** ho gaye:

```cpp
// ✅ C++17 mein defined
a[i] = i++;                 // a pehle, phir i (subscript rule)
std::cout << i++ << i++;    // left-to-right guaranteed
f(a) + g(b)                 // ab bhi order unspecified, par overlap nahi
```

**Lekin ab bhi UB:**
```cpp
i = i++ + 1;                // ⚠️ ab bhi UB
i = ++i;                    // ⚠️ ab bhi UB
```

**Aur ab bhi unspecified:**
```cpp
f(g(), h());                // g aur h ka order
```

---

## 🔑 Practical rules

Standard ke details yaad karne ki zarurat nahi. **Yeh 4 rules follow karo:**

### Rule 1: Ek statement, ek modification
```cpp
// ❌
i = i++ + ++i;

// ✅
++i;
int temp = i;
++i;
i = temp + i;
```

### Rule 2: Function calls arguments mein side effects mat daalo
```cpp
// ❌ order unspecified
process(getNext(), getNext());

// ✅
auto first = getNext();
auto second = getNext();
process(first, second);
```

### Rule 3: Conditions mein side effects se bacho
```cpp
// ❌ short-circuit se kabhi chalega kabhi nahi
if (check() && modify()) { }

// ✅
bool ok = check();
if (ok) {
    bool result = modify();
    // ...
}
```

### Rule 4: Confusion ho to alag lines mein todo
```cpp
// ❌ "clever"
result = arr[i++] + arr[i++];

// ✅ clear
int a = arr[i]; ++i;
int b = arr[i]; ++i;
result = a + b;
```

**Clarity > cleverness. Hamesha.**

---

## Comma operator

```cpp
a, b        // a evaluate karo (result discard), phir b evaluate karo
            // poore expression ki value = b
```

```cpp
int x = (1, 2, 3);      // x = 3
```

### Kahan use hota hai

**1. `for` loops mein** (yahan legit hai)
```cpp
for (int i = 0, j = n - 1; i < j; ++i, --j) {
    std::swap(arr[i], arr[j]);
}
```

**2. Macros mein** (avoid)

### ⚠️ Comma operator vs comma separator

```cpp
f(a, b);          // yeh SEPARATOR hai -- do arguments
f((a, b));        // yeh OPERATOR hai -- ek argument, value = b

int arr[] = {1, 2, 3};        // separator
int x = (1, 2, 3);            // operator -> 3
```

### Classic bug
```cpp
// ❌ Multi-dimensional array access ki koshish (C++23 se pehle)
arr[i, j];         // ⚠️ comma OPERATOR -- yeh arr[j] hai!
                   //    C++20 mein deprecated, C++23 mein multi-dim subscript aaya

arr[i][j];         // ✅ sahi tareeka
```

---

## Function arguments ka order

```cpp
void log(int a, int b, int c);

int counter = 0;
log(counter++, counter++, counter++);      // ⚠️ order unspecified
```

**GCC** aksar right-to-left evaluate karta hai, **MSVC** bhi, par **guarantee nahi**.

```cpp
// GCC output shayad: log(2, 1, 0)
// Aur compiler version badalne pe badal sakta hai
```

**Fix:**
```cpp
int a = counter++;
int b = counter++;
int c = counter++;
log(a, b, c);            // ✅ order clear
```

---

## `std::cout` chaining safe hai (C++17 se)

```cpp
int i = 0;
std::cout << i++ << " " << i++ << "\n";
```

**C++17 se:** left-to-right guaranteed → `0 1`
**C++17 se pehle:** unspecified → `0 1` ya `1 0`

**Lekin phir bhi aisa mat likho** — confusing hai.

---

## Detection tools

```bash
# UBSan kuch cases pakadta hai
g++ -std=c++20 -fsanitize=undefined -g file.cpp -o file && ./file

# Compiler warnings
g++ -Wall -Wextra -Wsequence-point file.cpp
```

```
warning: operation on 'i' may be undefined [-Wsequence-point]
```

**⚠️ Lekin yeh sab cases nahi pakadte.** Best defence: aisa code likho hi mat.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > evalorder.cpp << 'END'
#include <iostream>

int trace(const char* name, int value) {
    std::cout << name;
    return value;
}

int main() {
    std::cout << "===== 1. FUNCTION ARGUMENT ORDER (unspecified) =====\n";
    std::cout << "  f(a(), b(), c()) mein order: ";
    const int sum = trace("a", 1) + trace("b", 2) + trace("c", 3);
    std::cout << "  -> sum = " << sum << "\n";
    std::cout << "  (aapke compiler ka order upar dikha -- guarantee NAHI hai)\n";

    std::cout << "\n===== 2. GUARANTEED: && SHORT-CIRCUIT =====\n";
    std::cout << "  false && f(): ";
    if (false && trace("SHOULD-NOT-RUN", 1)) {}
    std::cout << "  (kuch nahi chala) ✅\n";

    std::cout << "  true || f():  ";
    if (true || trace("SHOULD-NOT-RUN", 1)) {}
    std::cout << "  (kuch nahi chala) ✅\n";

    std::cout << "\n===== 3. GUARANTEED: ?: =====\n";
    std::cout << "  cond ? a() : b(): ";
    const int r = true ? trace("TRUE-BRANCH", 1) : trace("FALSE-BRANCH", 2);
    std::cout << " -> " << r << "  (sirf ek branch chala) ✅\n";

    std::cout << "\n===== 4. GUARANTEED: cout << (C++17) =====\n";
    int i = 0;
    std::cout << "  i=0; cout << i++ << i++;  ->  ";
    std::cout << i++ << " " << i++;
    std::cout << "   (C++17 se left-to-right guaranteed)\n";

    std::cout << "\n===== 5. GUARANTEED: assignment (C++17) =====\n";
    int j = 0;
    int arr[3] = {10, 20, 30};
    arr[j] = j++;                   // C++17: arr[0] = 0, phir j=1
    std::cout << "  arr[j] = j++;  ->  arr[0]=" << arr[0] << " j=" << j
              << "   (C++17 mein defined)\n";

    std::cout << "\n===== 6. ⚠️ UB -- YEH KABHI MAT LIKHNA =====\n";
    std::cout << "  int i = 0; i = i++ + ++i;    <- UB\n";
    std::cout << "  int i = 5; i = i++;          <- UB\n";
    std::cout << "  f(i++, i++);                 <- unspecified order\n";
    std::cout << "\n  Yahan demonstrate NAHI kar rahe -- kyunki UB ka matlab\n";
    std::cout << "  kuch bhi ho sakta hai, aur us output pe bharosa nahi kar sakte.\n";
    std::cout << "\n  Detect karo: g++ -Wall -Wsequence-point\n";
    std::cout << "               g++ -fsanitize=undefined\n";

    std::cout << "\n===== 7. COMMA OPERATOR =====\n";
    const int x = (1, 2, 3);
    std::cout << "  int x = (1, 2, 3);  ->  x = " << x
              << "   (last value)\n";

    std::cout << "  for loop mein legit use:\n";
    int a[] = {1, 2, 3, 4, 5};
    std::cout << "    reverse: ";
    for (int lo = 0, hi = 4; lo < hi; ++lo, --hi) {
        const int t = a[lo]; a[lo] = a[hi]; a[hi] = t;
    }
    for (const int v : a) std::cout << v << " ";
    std::cout << "\n";

    std::cout << "\n===== 8. SAFE PATTERN =====\n";
    int counter = 0;
    const int p = counter++;
    const int q = counter++;
    const int s = counter++;
    std::cout << "  Alag lines mein: p=" << p << " q=" << q << " s=" << s
              << "   ✅ order bilkul clear\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra -Wsequence-point evalorder.cpp -o evalorder && ./evalorder
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Precedence evaluation order batati hai" | ❌ Do alag cheezein hain |
| "Function arguments left-to-right chalte hain" | ❌ Unspecified (C++17 mein bhi) |
| "`i = i++` `i` ko badha dega" | ⚠️ UB — kuch bhi ho sakta hai |
| "C++17 ne saare order issues fix kar diye" | Kuch fix kiye, par UB cases ab bhi UB hain |
| "Compiler UB pakad lega" | ⚠️ Sirf kuch cases |

---

## Exercises

1. `evalorder.cpp` chalao. Aapke compiler ka argument order kya hai?

2. Kaunse UB hain, kaunse safe?
   ```cpp
   a) i = i + 1;
   b) i = i++;
   c) i = ++i;
   d) ++i; ++i;
   e) f(i++, i++);
   f) a[i] = i++;         // C++17 mein?
   g) std::cout << i++ << i++;    // C++17 mein?
   h) i++ + i++;
   ```
   <details><summary>Answers</summary>
   a) ✅ safe
   b) ⚠️ UB
   c) ⚠️ UB
   d) ✅ safe (alag statements, `;` sequences them)
   e) ⚠️ unspecified order (UB nahi, par unpredictable)
   f) ✅ C++17 se defined
   g) ✅ C++17 se defined (left-to-right)
   h) ⚠️ UB
   </details>

3. `-Wsequence-point` ke saath UB code compile karo. Warning aayi?
   ```cpp
   int i = 0;
   i = i++ + ++i;
   ```

4. UBSan try karo:
   ```bash
   g++ -std=c++20 -fsanitize=undefined -g ub.cpp -o ub && ./ub
   ```

5. Yeh unsafe code fix karo:
   ```cpp
   int idx = 0;
   process(data[idx++], data[idx++], data[idx++]);
   ```

6. Comma operator ka bug reproduce karo:
   ```cpp
   int arr[3][3] = {};
   int i = 1, j = 2;
   // std::cout << arr[i, j];      // C++20 mein deprecated warning
   ```

7. Different compilers/optimization levels pe argument order test karo:
   ```bash
   g++ -O0 evalorder.cpp -o e0 && ./e0 | head -3
   g++ -O2 evalorder.cpp -o e2 && ./e2 | head -3
   clang++ -O2 evalorder.cpp -o ec && ./ec | head -3
   ```

---

## Interview questions

1. Precedence aur evaluation order mein fark?
2. `i = i++ + ++i` mein kya problem hai?
3. Function arguments kis order mein evaluate hote hain?
4. C++17 ne evaluation order mein kya badla?
5. `&&` ka evaluation order guaranteed hai?

---

## Next
→ [`11-other-operators.md`](11-other-operators.md)
