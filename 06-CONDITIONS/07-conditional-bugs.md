# 07 — Conditional bugs — pehchano aur roko

## Prerequisites
- [`01-if-statement.md`](01-if-statement.md) … [`06-if-with-initializer.md`](06-if-with-initializer.md)
- `05-OPERATORS/03-comparison-operators.md`, `05-OPERATORS/04-logical-operators.md`
- `03-VARIABLES-DATA-TYPES/06-floating-point.md`

## Yeh topic abhi kyun
Condition ke bugs iss liye khatarnak hain kyunki woh **compile ho jaate hain aur
chal jaate hain** — bas galat result dete hain, crash nahi. Kabhi-kabhi to sirf
kuch inputs pe.

Yeh lesson 7 classic condition bugs ka catalogue hai: kaise dikhte hain, kyun
hote hain, compiler pakadta hai ya nahi, aur kaise roke. Saath: `examples/
03_condition_bugs.cpp` — sab ek jagah, chal ke dekh sakte ho.

---

## Bug 1 — `=` vs `==` (assignment as condition)

```cpp
int status = 0;                 // 0 = success

if (status = 1) {               // ⚠️ ASSIGNMENT -- status ab 1, aur 1 = truthy
    handleError();              // hamesha chalega
}
```

`=` assign karta hai, `==` compare. `if (status = 1)` pehle `status` ko `1` set
karta hai, phir `1` ko condition (true) maanta hai. Aapka error-handler har baar
chalta hai, aur `status` corrupt.

### Compiler
`g++ -Wall` → `-Wparentheses`:
```
warning: suggest parentheses around assignment used as truth value
```

### Bachne ke tareeke
```cpp
// 1. Warnings ON + CI mein -Werror
g++ -std=c++20 -Wall -Wextra -Werror

// 2. bool ko seedha likho -- yahan `=` typo compile error banega
if (isReady) { }               // ✅   (if (isReady = true) -> error, kyunki bool = bool ...
                               //       actually warning; but `if (ok)` idiom safest)

// 3. jab jaan-boojh kar assign karna ho, DOUBLE parens se intent batao
if ((n = read(fd)) > 0) { }    // "haan, maine yeh assign kiya" -- warning silent
```

### Intentional assignment-in-condition (idiom)
```cpp
// C-style read loop -- yeh jaan-boojh kar hai
while ((bytes = ::read(fd, buf, sizeof buf)) > 0) {
    process(buf, bytes);
}
```
Extra `( )` compiler ko batati hai "typo nahi hai". Modern C++ mein aksar
init-`if`/`while` (file 06) isse replace kar deta hai.

---

## Bug 2 — `&` vs `&&` (aur `|` vs `||`)

```cpp
if (ptr != nullptr & ptr->value > 5) { }    // ⚠️ `&` bitwise -- short-circuit NAHI
```

`&&` short-circuit karta hai: `ptr != nullptr` false hua to `ptr->value` **chhua
hi nahi jaata**. `&` (bitwise) **dono side hamesha** evaluate karta hai → `ptr`
null hone pe bhi `ptr->value` → **null deref crash**.

`bool` values pe `&`/`|` ka *result* aksar sahi aata hai — isi liye bug chhupa
rehta hai jab tak woh ek null/out-of-bounds case na maare.

### Compiler
GCC is exact pattern pe hamesha warn nahi karta (Clang `-Wbitwise-instead-of-
logical` deta hai). Isliye **aadat**: booleans ke liye hamesha `&&` / `||`.

```cpp
if (ptr != nullptr && ptr->value > 5) { }   // ✅
```

`&` / `|` sirf bitwise math ke liye (folder 05 file 05).

---

## Bug 3 — Floating point `==`

```cpp
double sum = 0.1 + 0.2;
if (sum == 0.3) { }          // ⚠️ FALSE
```

`0.1`, `0.2`, `0.3` binary floating-point mein **exact nahi bante** (folder 03
file 06). `0.1 + 0.2` = `0.30000000000000004…` — `0.3` se ~`1e-17` door.

### Sahi tareeka — epsilon comparison
```cpp
#include <cmath>
#include <algorithm>

bool nearlyEqual(double a, double b,
                 double relEps = 1e-9, double absEps = 1e-12) {
    double diff = std::fabs(a - b);
    if (diff <= absEps) return true;                       // dono ~0
    return diff <= relEps * std::max(std::fabs(a), std::fabs(b));
}

if (nearlyEqual(0.1 + 0.2, 0.3)) { }    // ✅ true
```

### NaN — har comparison false
```cpp
double x = std::nan("");
if (x == x)  { }     // FALSE (NaN apne barabar bhi nahi)
if (x != x)  { }     // TRUE  -- "yeh NaN hai" ka ek check
if (std::isnan(x)) { }   // ✅ saaf tareeka
```

⚠️ Integers ke liye `==` bilkul theek hai — yeh trap sirf `float`/`double` ka hai.
`-Wfloat-equal` (opt-in) har float `==` pe warn karta hai.

---

## Bug 4 — Missing braces

```cpp
if (isAdmin)
    grantPanel();
    grantDeleteRights();     // ⚠️ if ke BAHAR -- har user ko mil gaya
```

Bina `{ }` ke `if` sirf **agli ek statement** control karta hai (file 01). Doosri
line hamesha chalti hai. Yahan har non-admin ko delete rights mil gaye.

### Compiler
`g++ -Wall` → `-Wmisleading-indentation`:
```
warning: this 'if' clause does not guard... this statement is misleadingly indented
```
(Sirf tab jab indentation dhoka de rahi ho.)

### Fix
```cpp
if (isAdmin) {
    grantPanel();
    grantDeleteRights();
}
```
**Rule: hamesha braces.** Isse Bug 4, dangling-else (file 02), aur aadhe Bug 5 ek
saath khatam.

---

## Bug 5 — Stray semicolon

```cpp
if (lives <= 0);              // ⚠️ `;` = empty statement = poora if body
{
    gameOver();               // yeh block ab if se juda NAHI -- hamesha chalega
}
```

`if (cond);` — woh `;` khali statement hai, wahi `if` ka body ban gaya. Neeche ka
block `if` se independent hai.

Yeh `for`/`while` ke saath aur bhi ghaatak:
```cpp
for (int i = 0; i < n; ++i);      // ⚠️ loop body khali -- sirf i badhta hai
{
    sum += arr[i];                 // ek hi baar chalega, i == n ke saath (out of bounds!)
}
```

### Compiler
`g++ -Wall` → `-Wempty-body`:
```
warning: suggest braces around empty body in an 'if' statement
```

### Fix
Semicolon hatao, braces lagao. Warnings ON.

---

## Bug 6 — Chained comparison `a < x < b`

```cpp
int x = 100;
if (0 < x < 10) { }          // ⚠️ hamesha TRUE
```

C++ mein comparison chain **nahi** hoti (Python se alag). Parse:
```
   (0 < x) < 10
   (0 < 100) < 10
   true      < 10
   1         < 10
   true
```

`x` `-5` hota to: `(0 < -5)` = `false` = `0`, `0 < 10` = `true` — **phir bhi true**.
Yeh condition har `x` ke liye true (kyunki `0` ya `1`, dono `< 10`).

### Compiler
`g++ -Wall` → `-Wparentheses`:
```
warning: comparisons like 'X<=Y<=Z' do not have their mathematical meaning
```

### Fix
```cpp
if (0 < x && x < 10) { }     // ✅
```

---

## Bug 7 — Unsigned in conditions

```cpp
std::vector<int> v;                       // khali
if (v.size() - 1 >= 0) {                  // ⚠️ hamesha true
    use(v[v.size() - 1]);                 // v[HUGE] -- out of bounds UB
}
```

Do problems:
1. `v.size()` ka type **unsigned** (`size_t`). Khali pe `size() - 1` = `0 - 1` =
   wrap around → `18446744073709551615`.
2. `unsigned >= 0` **hamesha true** — unsigned kabhi negative nahi hota.

Loop version — classic infinite loop:
```cpp
for (std::size_t i = v.size() - 1; i >= 0; --i) { }   // ⚠️ i >= 0 hamesha true
```

### Compiler
`g++ -Wall -Wextra` → `-Wtype-limits`:
```
warning: comparison of unsigned expression in '>= 0' is always true
```
Aur mixed signed/unsigned pe `-Wsign-compare`.

### Fix
```cpp
if (!v.empty()) { use(v.back()); }               // ✅ guard
for (std::size_t i = v.size(); i-- > 0; ) { }     // ✅ safe reverse idiom
for (auto i = std::ssize(v) - 1; i >= 0; --i) { } // ✅ C++20 signed size
for (const auto& x : v) { }                       // ✅ best
```

---

## Bonus — `switch` fallthrough (file 04 se)

```cpp
switch (cmd) {
    case START: init();       // ⚠️ break missing -> STOP wala code bhi chala
    case STOP:  cleanup(); break;
}
```
`-Wimplicit-fallthrough` (`-Wextra`) pakadta hai. Intentional ho to
`[[fallthrough]];`.

---

## The one rule that catches most of these

```bash
g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Werror
```

| Bug | Warning | `-Wall`/`-Wextra` catch? |
|---|---|---|
| 1. `=` vs `==` | `-Wparentheses` | ✅ |
| 2. `&` vs `&&` | (`-Wbitwise-instead-of-logical`, Clang) | ⚠️ aadat pe |
| 3. float `==` | `-Wfloat-equal` (opt-in) | ❌ default |
| 4. missing braces | `-Wmisleading-indentation` | ✅ (agar indent dhoka de) |
| 5. stray `;` | `-Wempty-body` | ✅ |
| 6. chained compare | `-Wparentheses` | ✅ |
| 7. unsigned condition | `-Wtype-limits`, `-Wsign-compare` | ✅ |
| fallthrough | `-Wimplicit-fallthrough` | ✅ |

Jo compiler nahi pakadta (2, 3) — unke liye **aadat, code review, aur unit tests**.

---

## Hands-on

```bash
./build.ps1 06-CONDITIONS/examples/03_condition_bugs.cpp
```

Compile ke waqt **warnings padho** — GCC in mein se 5 khud dikha dega. Program
har bug ka `[buggy]` vs `[fixed]` output side-by-side deta hai. File ke neeche
notes/answers block hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`if (a = b)` compile error" | Sirf warning — silently galat chalta hai |
| "`&` aur `&&` bool pe same" | Result aksar same, par `&` short-circuit nahi karta → crash risk |
| "`0.1+0.2 == 0.3`" | `false` — epsilon comparison karo |
| "`if (cond);` kuch nahi karta" | Woh `if` ka poora body ban jaata hai (empty) |
| "`0 < x < 10` range check hai" | `(0<x) < 10` — hamesha true |
| "`size() - 1` -1 deta hai khali pe" | Unsigned wrap → bahut bada number |

---

## Exercises

1. **Har bug spot karo** (pehle padh ke, phir compile karke):
   ```cpp
   int n = 5;
   if (n = 10) foo();
   if (0 <= n <= 100) bar();
   if (v.size() - 1 > 0) baz();
   double d = a * 0.1;
   if (d == 0.5) qux();
   ```
   <details><summary>Answer</summary>
   L2: `=` → `==`. L3: chained → `0 <= n && n <= 100`. L4: unsigned, `> 0` on
   empty wraps → `!v.empty() && ...`. L6: float `==` → epsilon.
   </details>

2. **`&` vs `&&` demo:** ek `sideEffect()` function jo counter badhata hai, use
   `false && sideEffect()` aur `false & sideEffect()` mein daalo. Counter compare.

3. **Float trap:** predict karo —
   ```cpp
   std::cout << (0.1 + 0.2 == 0.3) << " "
             << (0.5 + 0.25 == 0.75) << " "
             << (1.0 / 3.0 * 3.0 == 1.0);
   ```
   <details><summary>Answer</summary>
   `0 1 ?` — pehla false. Doosra `1` (0.5, 0.25, 0.75 sab exact powers-of-2).
   Teesra platform pe depend (aksar `1` — rounding cancel ho jaata hai, par
   guarantee nahi).
   </details>

4. **Stray `;`:** yeh code likho, chalao, samjho —
   ```cpp
   int count = 0;
   for (int i = 0; i < 5; ++i);
       count++;
   std::cout << count << "\n";
   ```
   `count` kya aaya? Kyun `5` nahi?
   <details><summary>Answer</summary>
   `1`. Loop body khali `;` hai — sirf `i` 0→5 jaata hai. `count++` loop ke baad
   ek baar chalta hai.
   </details>

5. **Fix + warnings:** yeh file `-Wall -Wextra -Werror` se compile karo, har error
   ek-ek karke fix karo —
   ```cpp
   #include <vector>
   int main() {
       std::vector<int> v = {1,2,3};
       int s = 0;
       for (int i = v.size() - 1; i >= 0; --i) s += v[i];
       bool done = false;
       if (done = true) return s;
       return 0;
   }
   ```

6. **Intentional assignment:** ek `while` loop likho jo `std::getline` se lines
   padhe, condition mein assignment ke saath. Warning aayi? Double-parens se
   silence karo (ya init-`while` use karo).

---

## Interview questions

1. `if (x = 5)` kya karta hai? Compile hota hai? Kaise pakde?
2. `&` aur `&&` mein fark? Kaunsa condition mein galti se lag jaye to crash?
3. `0.1 + 0.2 == 0.3` kya deta hai aur kyun? Sahi comparison kaise?
4. `NaN == NaN` ka result?
5. `if (cond);` (stray semicolon) ka kya asar? Compiler warning?
6. `a < b < c` C++ mein kya evaluate karta hai?
7. `for (size_t i = n-1; i >= 0; --i)` mein kya problem? 2 fixes.
8. Kaunse condition bugs `-Wall -Wextra` pakadta hai, kaunse nahi?

---

## Next
→ [`08-branch-prediction-intro.md`](08-branch-prediction-intro.md)
