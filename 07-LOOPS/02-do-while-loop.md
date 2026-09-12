# 02 — `do-while` loop

## Prerequisites
- [`01-while-loop.md`](01-while-loop.md)

## Yeh topic abhi kyun
`while` pehle check karta hai — agar condition shuru se false ho, body **kabhi
nahi** chalti. Kabhi-kabhi aapko body **kam se kam ek baar** chahiye hoti hai:
menu dikhana, input maangna, ek retry karna. `do-while` yahi karta hai — **pehle
chalao, phir check karo.**

Yeh sabse kam use hone wala loop hai, par jahan fit baithta hai wahan sabse saaf
hota hai. Aur iska `;` ek common typo ka source hai.

---

## Anatomy

```cpp
do {
    // body -- ek baar to CHALEGI HI
} while (condition);       // <-- SEMICOLON zaroori
```

| `while` loop | `do-while` loop |
|---|---|
| check → body → check → body … | **body** → check → body → check … |
| body **0 ya zyada** baar | body **1 ya zyada** baar |
| `}` ke baad kuch nahi | `}` ke baad `while (…)` **aur `;`** |

### Execution

```cpp
int n = 0;
do {
    std::cout << "chala (n = " << n << ")\n";
    ++n;
} while (n < 0);          // 0 < 0 false -- par body already ek baar chal chuki
```

Output: `chala (n = 0)` — condition false thi, phir bhi ek baar chali.

---

## Kab use karein

### 1. Input validation — "pehle poocho, phir jaancho"

```cpp
int age;
do {
    std::cout << "Apni age daalo (1-120): ";
    std::cin >> age;
} while (age < 1 || age > 120);      // galat input -> dobara poocho
```

`while` se likhne pe pehle ek dummy value chahiye hoti — awkward:
```cpp
int age = -1;                        // ⚠️ artificial initial value
while (age < 1 || age > 120) {
    std::cout << "Apni age daalo: ";
    std::cin >> age;
}
```

### 2. Menu loop

```cpp
int choice;
do {
    showMenu();
    choice = readChoice();
    handle(choice);
} while (choice != 0);               // 0 = exit
```

### 3. Retry ek baar to karna hi hai

```cpp
int attempt = 0;
bool ok;
do {
    ok = tryConnect();
    ++attempt;
} while (!ok && attempt < MAX_RETRIES);
```

### 4. `do { } while (false)` — ek "single-pass block" idiom

```cpp
do {
    if (!step1()) break;
    if (!step2()) break;
    if (!step3()) break;
    commit();
} while (false);                     // loop nahi -- bas `break` se jump-to-end
cleanup();
```

Yeh C-era macro trick hai (aur multi-statement macros ke liye). Modern C++ mein
aksar guard clauses (folder 06 file 03) ya RAII (folder 17) behtar hain — par
aapko purane codebases mein dikhega.

---

## ⚠️ Traps

### Trap 1 — `;` bhool jaana
```cpp
do {
    work();
} while (cond)          // ❌ COMPILE ERROR: expected ';'
```
`do-while` **akela loop hai jisme `while (...)` ke baad `;` lagta hai.**

### Trap 2 — `;` ko `while` loop ka header samajh lena
```cpp
do {
    work();
} while (cond);
{
    // ⚠️ yeh ALAG block hai -- kai log samajhte hain yeh loop ka hissa hai
}
```

### Trap 3 — condition body ke variable pe depend karti hai jo scope mein nahi
```cpp
do {
    int x = compute();
} while (x > 0);        // ❌ ERROR -- x block ke andar declare hua, condition mein visible nahi
```
Fix: `x` ko loop se **pehle** declare karo.

### Trap 4 — jab body ko sach mein 0 baar bhi chalna chahiye
```cpp
// ❌ Agar list khali ho to bhi ek baar process kar dega
do {
    process(list.front());          // 💥 khali list pe UB
    list.pop_front();
} while (!list.empty());

// ✅ while
while (!list.empty()) {
    process(list.front());
    list.pop_front();
}
```
**Rule: agar "sh_ayad zero baar" possible hai → `while`. "Hamesha ek baar" → `do-while`.**

---

## Andar kya hota hai

`do-while` `while` se **thoda simple** assembly deta hai — pehli iteration ke liye
condition check skip hota hai:

```cpp
do { body(); } while (cond);
```

```asm
.loop:
        call    body
        cmp     ...              ; condition
        jne     .loop            ; true -> wapas upar
```

`while` ko pehle ek "condition check pe jump" chahiye hota hai (ya condition ki
copy loop ke top pe). `do-while` mein woh nahi. Farq micro hai — `-O2` pe compiler
dono ko optimal bana deta hai. **Choose readability, not this.**

> **HFT relevance:** Seedha koi bada connection nahi — `do-while` config parsing,
> reconnection logic, aur test harness code mein dikhta hai, hot path mein kam.
> Ek chhoti baat: `do { } while (0)` pattern se banaye gaye purane C macros hot
> code mein ho sakte hain; unhe modern `constexpr` functions / `[[gnu::always_inline]]`
> se replace karna common cleanup hai (folder 08, 33).

---

## Hands-on

`examples/01_loop_types.cpp` ka section 2 (`do-while`) — retry-until-valid demo:

```bash
./build.ps1 07-LOOPS/examples/01_loop_types.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`do-while` ke baad `;` optional hai" | Zaroori — bina uske compile error |
| "`do-while` `while` se tez hai" | Micro farq; `-O2` pe barabar. Readability se decide |
| "Body ke variable ko condition mein use kar sakta hoon" | Nahi — woh block-scoped hai; loop se pehle declare karo |
| "Input loops hamesha `do-while`" | Sirf jab "pehle poocho" natural ho; warna `while` bhi theek |
| "`do { } while(false)` ek loop hai" | Practically ek `break`-able block — 1 hi pass |

---

## Exercises

1. **Output:**
   ```cpp
   int i = 5;
   do { std::cout << i << " "; --i; } while (i > 5);
   ```
   <details><summary>Answer</summary>`5 ` — body ek baar chali, phir `4 > 5` false.</details>

2. **`while` vs `do-while`:** dono se `n` ke digits gino. `n = 0` pe kaunsa `1`
   deta hai (sahi) aur kaunsa `0` (bug)?
   <details><summary>Answer</summary>`do-while` `1` deta hai (ek baar chal ke `0 / 10 = 0`, count 1) — `0` ke liye yeh sahi hai. Plain `while (n > 0)` `0` deta hai.</details>

3. **Menu loop:** ek `do-while` menu banao: `1) add  2) list  0) exit`. `0` pe hi
   niklo.

4. **Convert:** yeh `do-while` ko `while` mein badlo (dummy initial value ke saath).
   Kaunsa saaf hai?
   ```cpp
   std::string line;
   do { std::getline(std::cin, line); } while (line.empty());
   ```

5. **Bug spot:**
   ```cpp
   do {
       int x;
       std::cin >> x;
       sum += x;
   } while (x != -1);
   ```
   <details><summary>Answer</summary>`x` `do` block ke andar declared → `while (x != -1)` scope error. `x` ko loop se pehle declare karo.</details>

6. **`do { } while(0)`:** ek function likho jisme 4 steps hain, koi bhi fail ho to
   cleanup karke exit. Pehle `do { } while(0) + break` se, phir guard clauses se.
   Kaunsa readable?

---

## Interview questions

1. `while` aur `do-while` — output kaise alag ho sakta hai same condition pe?
2. `do-while` ka `;` kahan aur kyun?
3. `do-while` ke body mein declare kiya variable condition mein kyun nahi milta?
4. `do { } while(false)` idiom kya hai, kyun use hota tha?
5. Ek scenario jahan `do-while` galat choice hai (aur `while` sahi)?

---

## Next
→ [`03-for-loop.md`](03-for-loop.md)
