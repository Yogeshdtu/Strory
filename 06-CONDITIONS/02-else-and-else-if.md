# 02 — `else` aur `else if`

## Prerequisites
- [`01-if-statement.md`](01-if-statement.md)

## Yeh topic abhi kyun
`if` akela "haan/kuch nahi" deta hai. `else` doosra raasta jodta hai: "haan **ya**
warna". `else if` se aap kai raaste bana sakte ho — grading, categorization, state
machines, message routing. Yeh roz ka pattern hai.

Saath hi ek classic ambiguity — **dangling else** — jo braces na hone pe silent
bug banata hai.

---

## `else` — "warna"

```cpp
if (condition) {
    // A -- condition true
} else {
    // B -- condition false
}
```

**Exactly ek block chalega** — dono kabhi nahi, na hi zero.

```cpp
if (number % 2 == 0) {
    std::cout << "even\n";
} else {
    std::cout << "odd\n";
}
```

`else` ka apna koi condition nahi hota — woh bas "upar wala `if` fail hua" case
pakadta hai.

### `else` akela nahi reh sakta
```cpp
else { }        // ❌ COMPILE ERROR -- 'else' without a previous 'if'
```

---

## `else if` — chain

```cpp
if (score >= 90) {
    grade = 'A';
} else if (score >= 80) {
    grade = 'B';
} else if (score >= 70) {
    grade = 'C';
} else {
    grade = 'F';
}
```

### `else if` koi keyword nahi hai

Yeh bas `else` ke baad ek aur `if` hai. Asli structure:

```cpp
if (score >= 90) {
    grade = 'A';
} else {
    if (score >= 80) {
        grade = 'B';
    } else {
        if (score >= 70) {
            grade = 'C';
        } else {
            grade = 'F';
        }
    }
}
```

`else` ke body mein agar **ek hi statement** ho (aur woh `if` ho), to braces optional
hain — isliye `} else if (...) {` ek line mein likh dete hain. Bilkul same cheez,
padhne mein aasan.

### Chain kaise chalti hai

1. Upar se neeche har condition **turn by turn** check hoti hai
2. **Pehla `true`** jeet gaya → uska block chala → **poori chain khatam** (baaki
   conditions check hi nahi hoti)
3. Koi bhi `true` nahi → `else` block (agar hai)

```
score = 85:
   85 >= 90 ?  nahi
   85 >= 80 ?  HAAN  -> grade = 'B'  -> chain khatam (>=70 check nahi hua)
```

---

## ⚠️ Order matter karta hai — overlapping conditions

```cpp
// ❌ GALAT order
if (score >= 70)      grade = 'C';
else if (score >= 80) grade = 'B';    // kabhi nahi chalega!
else if (score >= 90) grade = 'A';    // kabhi nahi chalega!
```

`score = 95` → pehli condition `95 >= 70` **true** → `grade = 'C'` → chain khatam.
`B` aur `A` dead code hain.

**Rule:** overlapping ranges mein **sabse specific / sabse tight condition pehle**.
Yahan: bada number pehle.

```cpp
// ✅ SAHI
if (score >= 90)      grade = 'A';
else if (score >= 80) grade = 'B';
else if (score >= 70) grade = 'C';
else                  grade = 'F';
```

### Mutually exclusive conditions mein order se farq nahi (correctness ka)

```cpp
if (day == 1) ...
else if (day == 2) ...
else if (day == 3) ...
```

Yahan ek waqt mein sirf ek hi true ho sakta hai, to order correctness ko nahi
badalta. Par **performance** ke liye: **sabse common case pehle** rakho — average
mein kam comparisons.

> **HFT relevance:** Message dispatch (`if (type == Quote) … else if (type == Trade)
> … else if (type == Heartbeat) …`) mein sabse zyada aane wala message type pehle
> rakho. Har `else if` ek compare + branch hai. Agar 95% messages `Quote` hain aur
> woh chain mein 4th hai, to har message pe 3 fizool compares. (Aur agar cases
> integer/enum pe hain — `switch` use karo, file 04–05: compiler jump table bana
> deta hai, O(1).)

---

## ⚠️ DANGLING ELSE

Yeh classic ambiguity hai. `else` kis `if` se judega?

```cpp
if (a)
    if (b)
        foo();
else            // <-- yeh kis if ka else hai?
    bar();
```

**Rule: `else` hamesha SABSE PAAS wale (innermost) unmatched `if` se judta hai.**

To upar ka code asal mein aisa hai:

```cpp
if (a) {
    if (b) {
        foo();
    } else {        // 'else' INNER if (b) ka hai
        bar();
    }
}
// a false -> kuch nahi hota. bar() sirf tab jab a=true, b=false.
```

Par indentation dhoka de rahi thi — dikhta tha jaise `else` `if (a)` ka ho.

### Fix — braces

```cpp
// Agar else INNER if ka chahiye:
if (a) {
    if (b) foo();
    else   bar();
}

// Agar else OUTER if ka chahiye:
if (a) {
    if (b) foo();
} else {
    bar();
}
```

GCC/Clang `-Wdangling-else` (part of `-Wall`) se warn karte hain jab aap braces
chhodte ho aisi nested situation mein. Is course ka rule (file 01) — **hamesha
braces** — isko poori tarah khatam kar deta hai.

---

## `else` ke bina — early exit pattern

Kai baar `else` ki zaroorat hi nahi — pehle wala block `return`/`break`/`continue`
karta hai:

```cpp
// else ke saath
std::string_view sign(int x) {
    if (x > 0) {
        return "positive";
    } else if (x < 0) {
        return "negative";
    } else {
        return "zero";
    }
}

// else ke bina -- "guard" style, flatter (file 03 mein poora)
std::string_view sign(int x) {
    if (x > 0) return "positive";
    if (x < 0) return "negative";
    return "zero";
}
```

Dono sahi hain. Doosra flatter hai — ek indentation level kam. File 03 mein trade-off.

---

## Memory model — koi allocation nahi

`if`/`else if`/`else` sirf **control flow** hai — koi memory allocate/free nahi
hoti. Compiler isse compare + jump instructions mein badal deta hai. Chain jitni
lambi, utne zyada compares worst case mein.

```
if / else if / else if / else   (4-way)

   cmp ... ; je  block1
   cmp ... ; je  block2
   cmp ... ; je  block3
   jmp block4            ; else
```

`n`-way chain = worst case `n-1` compares. `switch` (integer/enum pe) yeh O(1)
kar sakta hai — file 05.

---

## Hands-on

`examples/01_if_else.cpp` ka section 3 (`else-if CHAIN`) dekho aur chalao:

```bash
./build.ps1 06-CONDITIONS/examples/01_if_else.cpp
```

`score` values `{95, 82, 71, 40}` pe grade output. Phir khud chain ka order ulta
karke dekho — kya hota hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`else if` ek keyword hai" | `else` + naya `if` — braces optional isliye ek line |
| "Chain mein saari conditions check hoti hain" | Pehla `true` → chain khatam |
| "Overlapping ranges mein order se farq nahi" | Bahut farq — specific condition pehle |
| "`else` kisi bhi upar wale `if` se judega" | Sabse paas (innermost) unmatched `if` se |
| "`else { }` akela likh sakte ho" | Nahi — pehle `if` chahiye |

---

## Exercises

1. **Output predict karo:**
   ```cpp
   int n = 15;
   if (n % 3 == 0) std::cout << "fizz ";
   else if (n % 5 == 0) std::cout << "buzz ";
   else std::cout << n << " ";
   ```
   <details><summary>Answer</summary>
   `fizz ` — `15 % 3 == 0` true → chain khatam. `buzz` kabhi nahi (yeh classic
   FizzBuzz bug hai — dono check chahiye the `&&` se, ya alag `if`s).
   </details>

2. **Order bug fix karo:**
   ```cpp
   char speedRating(int kmph) {
       if (kmph > 0)    return 'D';
       else if (kmph > 50)  return 'C';
       else if (kmph > 100) return 'B';
       else if (kmph > 150) return 'A';
       else return 'X';
   }
   ```
   `speedRating(120)` kya deta hai? Kya dena chahiye tha? Fix karo.
   <details><summary>Answer</summary>
   `'D'` deta hai — `120 > 0` pehle match. Fix: conditions ko descending order
   mein (`> 150` pehle). `> 0` wala case sabse aakhri (`else if (kmph > 0)`).
   </details>

3. **Dangling else:** is code ka asli behaviour likho (kaunsa `else` kis `if` ka) —
   ```cpp
   int x = 5, y = -3;
   if (x > 0)
       if (y > 0)
           std::cout << "dono positive\n";
   else
       std::cout << "x non-positive?\n";
   ```
   `x=5, y=-3` pe output kya? Kya woh galat lag raha hai? Braces se fix karo.
   <details><summary>Answer</summary>
   `else` `if (y > 0)` ka hai. Output: `x non-positive?` — jabki `x` positive hai!
   Message jhootha hai. Braced: `if (x > 0) { if (y > 0) {...} else {...} }`.
   </details>

4. **Chain → guard:** yeh `if/else if/else` ko `return` wali flat guard style mein
   badlo —
   ```cpp
   std::string_view categorize(int age) {
       std::string_view r;
       if (age < 0)       r = "invalid";
       else if (age < 13) r = "child";
       else if (age < 20) r = "teen";
       else               r = "adult";
       return r;
   }
   ```

5. **Performance ordering:** ek 5-way `else if` chain likho jo message `type`
   (int) pe dispatch kare. `type` values ki frequency `{A:80%, B:15%, C:3%,
   D:1.5%, E:0.5%}` hai. Chain kis order mein likhoge aur kyun?

6. **Missing `else`:** yeh function har input pe value return karta hai?
   ```cpp
   int classify(int x) {
       if (x > 0) return 1;
       else if (x < 0) return -1;
       // x == 0 ?
   }
   ```
   Compile karo `-Wall` ke saath. Warning kya aayi?
   <details><summary>Answer</summary>
   `-Wreturn-type`: "control reaches end of non-void function". `x == 0` pe koi
   `return` nahi → UB. Fix: aakhri mein `return 0;`.
   </details>

---

## Interview questions

1. `else if` C++ ka keyword hai? Nahi to yeh kaam kaise karta hai?
2. `if/else if` chain mein kitni conditions evaluate hoti hain?
3. Overlapping conditions wali chain mein order kyun matter karta hai? Example do.
4. Dangling else problem kya hai? `else` kis `if` se bind hota hai?
5. Mutually-exclusive `else if` chain mein order performance ko kaise affect karta hai?
6. `if/else if` chain vs `switch` — kab kaunsa?

---

## Next
→ [`03-nested-conditions.md`](03-nested-conditions.md)
