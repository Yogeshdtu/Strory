# 02 — Variable kya hai?

## Prerequisites
`01-what-is-data.md`, folder 01 lesson 11 (memory basics)

## Yeh topic abhi kyun
Yeh **poore course ka sabse foundational concept** hai. Variable samjhe bina aap pointer,
reference, object, memory, ownership — kuch nahi samajh paoge.

Isliye hum ise bahut slow lenge.

---

## Sabse pehle: intuition

> **Variable ko ek LABELLED BOX samjho.**
>
> Box ke andar hum koi value rakh sakte hain.
> Box ke bahar ek naam ka label laga hota hai.
> Naam se hum box tak pahunch sakte hain.

```
        +----------+
        |          |
        |    20    |     <- box ke ANDAR ki value
        |          |
        +----------+
           "age"          <- box ka NAAM (label)
```

```cpp
int age = 20;
```

- `age` = box ka **naam** (label)
- `20` = box ke andar ki **value**
- `int` = box ka **type** (matlab: kitna bada box aur usme kya rakh sakte ho)

---

## Variable kyun chahiye?

Bina variables ke aap sirf yeh kar sakte ho:

```cpp
std::cout << 20;          // sirf fixed value print kar sakte ho
std::cout << 20 * 12;     // ya calculation
```

Har cheez **hard-coded** hogi. Kuch bhi yaad nahi rakh sakte, kuch bhi badal nahi sakte.

Variables se:

```cpp
int age = 20;             // yaad rakh liya
age = age + 1;            // badal diya
std::cout << age;         // 21
```

Ab aap:
- **Data yaad rakh sakte ho**
- **Use badal sakte ho**
- **Use naam se refer kar sakte ho** (bar bar value likhne ki jagah)
- **Calculations kar sakte ho** jinka result store ho

---

## Ab memory mein kya hua? (asli picture)

Jab aap likhte ho:
```cpp
int age = 20;
```

Compiler yeh karta hai:

### Step 1: Jagah reserve karta hai
`int` = 4 bytes. To compiler stack pe 4 bytes ki jagah reserve karta hai.

```
   Memory (stack)
   +---------+---------+---------+---------+
   | byte 0  | byte 1  | byte 2  | byte 3  |
   +---------+---------+---------+---------+
   ^
   address: 0x7ffd4a2b3c44
```

### Step 2: Naam ko address se jodta hai
Compiler apni symbol table mein likh leta hai:
```
   "age"  ->  address 0x7ffd4a2b3c44, type int, size 4 bytes
```

### Step 3: Value likhta hai
`20` ko binary mein badal ke un 4 bytes mein likh deta hai:

```
   20 in binary (32 bits): 00000000 00000000 00000000 00010100

   Little-endian machine pe memory mein:
   +---------+---------+---------+---------+
   |  0x14   |  0x00   |  0x00   |  0x00   |
   +---------+---------+---------+---------+
   0x...c44  0x...c45  0x...c46  0x...c47
```

### 🔑 Bahut important baat

**`age` naam sirf COMPILE TIME pe exist karta hai.**

Chalte hue program mein koi "age" nahi hota. Compiler ne `age` ko address se replace
kar diya hota hai. Assembly mein aapko sirf addresses dikhenge, naam nahi.

Naam **aapke liye** hai, computer ke liye nahi.

Verify karo:
```bash
cat > v.cpp << 'END'
int main() { int age = 20; return age; }
END
g++ -S -O0 v.cpp -o - | grep -A6 "^main:"
```
Aapko `age` naam kahin nahi dikhega — sirf `-4(%rbp)` jaisa kuch, jo stack offset hai.

---

## Variable ke 4 hisse

```cpp
int age = 20;
^^^ ^^^ ^ ^^
 |   |  |  |
 |   |  |  +-- VALUE: kya store karna hai
 |   |  +----- ASSIGNMENT: value ko box mein daalo
 |   +-------- NAME: box ka label
 +------------ TYPE: box ka size aur matlab
```

### 1. TYPE — `int`
Batata hai:
- **Size**: 4 bytes
- **Interpretation**: signed integer
- **Range**: −2,147,483,648 se 2,147,483,647
- **Valid operations**: `+`, `-`, `*`, `/`, `%`, comparison, bitwise...

### 2. NAME — `age`
Aapka diya hua label. Rules:
- Letters, digits, `_` se ban sakta hai
- Digit se **shuru nahi** ho sakta
- Keywords use nahi kar sakte (`int`, `return`, `class`...)
- **Case-sensitive**: `age`, `Age`, `AGE` teen alag variables hain

```cpp
int age;         // ✅
int _count;      // ✅
int order2;      // ✅
int 2order;      // ❌ digit se shuru
int my-var;      // ❌ hyphen allowed nahi
int class;       // ❌ keyword hai
int my var;      // ❌ space allowed nahi
```

### 3. `=` — assignment (initialization)
Yeh "barabar hai" nahi hai! Yeh **"daalo"** hai. Detail file 04 mein.

### 4. VALUE — `20`
Yeh ek **literal** hai — code mein seedha likhi hui value.

---

## Variables use karna

```cpp
#include <iostream>

int main() {
    int age = 20;

    // Padho
    std::cout << age << "\n";              // 20

    // Calculation mein use karo
    std::cout << age * 12 << "\n";         // 240 (mahine)

    // Badlo
    age = 21;
    std::cout << age << "\n";              // 21

    // Apne aap se update karo
    age = age + 1;
    std::cout << age << "\n";              // 22

    // Doosre variable mein copy karo
    int nextYear = age + 1;
    std::cout << nextYear << "\n";         // 23

    return 0;
}
```

### `age = age + 1;` — yeh kaise kaam karta hai?

Math mein yeh **impossible equation** hai. Programming mein yeh **instruction** hai:

```
   1. RIGHT side padho:   age + 1   ->  22 + 1  ->  23
   2. Result LEFT side ke box mein daalo:  age = 23
```

**Hamesha right pehle, phir left.** Yeh rule yaad rakho.

---

## Multiple variables

```cpp
// Alag alag lines (recommended)
int price = 100;
int quantity = 5;
int total = price * quantity;

// Ek line mein (legal, par bacho)
int a = 1, b = 2, c = 3;

// ⚠️ Yeh trap hai (pointers ke saath)
int* p1, p2;     // p1 pointer hai, p2 SIRF int hai! (folder 12 mein)
```

**Salah:** Ek line, ek variable. Padhne mein aasan, bugs kam.

---

## Variable ki lifetime (scope se juda)

Yaad hai folder 02 lesson 06?

```cpp
int main() {
    int x = 10;          // x yahan BANA
    {
        int y = 20;      // y yahan BANA
        std::cout << x;  // ✅ dono dikhte hain
    }                    // y yahan MARA
    std::cout << y;      // ❌ error
}                        // x yahan mara
```

Variable apne block ke saath **paida hota hai aur marta hai**. Yeh automatic hai.

---

## Uninitialized variable — ⚠️ khatarnaak

```cpp
int x;                    // koi value nahi di!
std::cout << x;           // ⚠️ UNDEFINED BEHAVIOUR
```

**Kya value aayegi?** Kuch bhi. Jo bhi us memory mein pehle se pada tha — garbage.

```cpp
#include <iostream>
int main() {
    int a;                     // uninitialized
    int b = 0;                 // initialized
    std::cout << "a = " << a << "\n";     // ⚠️ garbage
    std::cout << "b = " << b << "\n";     // 0
}
```

Har run pe alag value aa sakti hai. Ya same aa sakti hai (aur aap sochoge sab theek hai)
— aur phir production mein toot jaayega.

### 🔑 GOLDEN RULE
> **Har variable ko declare karte hi initialize karo. Har baar. Bina exception ke.**

```cpp
int x = 0;        // ✅
int y{};          // ✅ (brace init, 0 se initialize hota hai)
int z;            // ❌ mat karo
```

**Note:** Global aur `static` variables automatically 0 se initialize hote hain.
Sirf **local** (automatic) variables garbage hote hain.

```cpp
int globalVar;              // ✅ automatically 0

int main() {
    static int staticVar;   // ✅ automatically 0
    int localVar;           // ⚠️ GARBAGE
}
```

---

## Apni aankhon se dekho

```bash
cd ~/cpp-practice
cat > variable.cpp << 'END'
#include <iostream>

int main() {
    int age = 20;

    // Value
    std::cout << "Value: " << age << "\n";

    // ADDRESS -- `&` operator address deta hai
    // (yeh folder 12 mein detail mein padhenge)
    std::cout << "Address: " << &age << "\n";

    // SIZE
    std::cout << "Size: " << sizeof(age) << " bytes\n";

    // Badlo aur dekho -- address WAHI rehta hai, value badalti hai
    std::cout << "\nBadal rahe hain...\n";
    age = 21;
    std::cout << "Value: " << age << "\n";
    std::cout << "Address: " << &age << "  <- WAHI address!\n";

    // Doosra variable -- alag address
    int height = 175;
    std::cout << "\nheight ka address: " << &height << "\n";
    std::cout << "age ka address:    " << &age << "\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra variable.cpp -o variable && ./variable
```

**Dhyaan do:**
- Value badli, par **address wahi raha** — box wahi hai, andar ki cheez badli
- Do variables ke addresses alag hain — alag boxes
- Addresses aksar 4 ya 8 ke gap pe hote hain (size + alignment)

---

## Variable vs Value vs Address — teen alag cheezein

```cpp
int age = 20;
```

| Cheez | Kya hai | Kaise access karein |
|---|---|---|
| **Name** | `age` | source code mein likha naam |
| **Value** | `20` | `age` |
| **Address** | `0x7ffd...` | `&age` |
| **Type** | `int` | `decltype(age)` |
| **Size** | `4` | `sizeof(age)` |

Yeh teen (value, address, type) hi pointers aur references ka poora foundation hain.
Folder 12/13 mein wapas aayenge.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Variable ek naam hai" | Variable ek **named memory location** hai. Naam sirf compile time pe hai |
| "`int x;` mein x ki value 0 hoti hai" | ❌ **Local variables mein garbage hoti hai.** Globals mein 0 |
| "`age = age + 1` mathematically galat hai" | Yeh equation nahi, instruction hai. Right pehle, phir left |
| "Variable ka address badal sakta hai" | Uski lifetime mein nahi badalta |
| "`int a, b;` mein dono ek hi box hain" | Do alag boxes, do alag addresses |

---

## Exercises

1. Yeh code memory diagram ke saath samjhao:
   ```cpp
   int a = 5;
   int b = a;
   a = 10;
   ```
   `b` ki value kya hai? Kyun?
   <details><summary>Answer</summary>
   `b` = 5.

   ```
   int a = 5;      ->  a: [5]
   int b = a;      ->  a: [5]   b: [5]     <- COPY bani, connection nahi
   a = 10;         ->  a: [10]  b: [5]     <- sirf a badla
   ```
   `b = a` ne value **copy** ki, link nahi banaya. Yeh baat references (folder 13) mein
   contrast karegi.
   </details>

2. `variable.cpp` chalao. Address note karo. Program dobara chalao — kya address same hai?
   <details><summary>Answer</summary>
   Aksar **alag** hoga, kyunki modern OS **ASLR** (Address Space Layout Randomization)
   use karta hai — security ke liye har run pe memory layout randomize hota hai.
   Isse buffer overflow attacks mushkil ho jaate hain.
   </details>

3. Yeh valid variable names hain ya nahi?
   ```
   myVar, 2fast, _private, my-var, MyVar, my var, return, order_1, $price
   ```
   <details><summary>Answers</summary>
   ✅ `myVar`, `_private`, `MyVar`, `order_1`
   ❌ `2fast` (digit se shuru), `my-var` (hyphen), `my var` (space), `return` (keyword),
      `$price` (`$` standard nahi hai — kuch compilers allow karte hain, par mat use karo)
   </details>

4. Uninitialized variable ka experiment:
   ```cpp
   #include <iostream>
   int main() {
       int local;
       static int stat;
       std::cout << "local: " << local << "\n";
       std::cout << "static: " << stat << "\n";
   }
   ```
   `-Wall` ke saath compile karo. Kya warning aayi? 5 baar chalao — value badli?

5. Teen variables banao (`price`, `quantity`, `total`) aur total calculate karke print karo.

6. Yeh program trace karo (har line ke baad values likho):
   ```cpp
   int x = 10;
   int y = 20;
   int temp = x;
   x = y;
   y = temp;
   ```
   Aakhir mein `x` aur `y` kya hain?
   <details><summary>Answer</summary>
   `x = 20`, `y = 10`. Yeh classic **swap** hai. `temp` ki zarurat isliye hai kyunki
   `x = y` karne se `x` ki purani value kho jaati.
   </details>

---

## Interview questions

1. Variable memory mein kaise store hota hai?
2. Uninitialized local variable ki value kya hoti hai?
3. Local aur global variable ki default initialization mein kya fark hai?
4. Variable ka naam runtime pe exist karta hai?
5. `int a = 5; int b = a;` — `a` badalne se `b` badlega?

---

## Next
→ [`03-declaration-definition-initialization.md`](03-declaration-definition-initialization.md)
