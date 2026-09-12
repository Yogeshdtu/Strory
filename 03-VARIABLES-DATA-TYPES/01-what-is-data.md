# 01 — Data kya hai?

## Prerequisites
Folder 01 lesson 10 (bits, bytes, binary)

## Yeh topic abhi kyun
Hum "data store karna" seekhne wale hain. To pehle yeh clear karein ki **data** hai kya,
aur computer ke liye woh kaisa dikhta hai.

---

## Data = information

Data matlab **koi bhi information jo aap store ya process karna chahte ho**.

Examples:
- Aapki umar: `20`
- Aapka naam: `"Rahul"`
- Ek stock ka price: `21500.75`
- Kya market khula hai: `true` / `false`
- Ek order ka ID: `98765432109876`

---

## Computer ke liye sab kuch BITS hai

Yaad hai folder 01 lesson 10? Memory mein sirf **bits** hote hain — 0 aur 1.

To yeh 8 bits:
```
   01000001
```

Yeh kya hai?
- Number **65**?
- Character **'A'**?
- Ek chhota sa flag pattern?
- Kisi bade number ka ek hissa?

**Bits khud kuch nahi batate.** Woh sirf bits hain.

---

## Isliye TYPE chahiye

**Type = bits ka matlab.**

Type compiler ko batata hai:
1. **Kitne bytes** lagenge
2. **Kaise interpret** karna hai (number? character? decimal?)
3. **Kaunse operations** valid hain
4. **Kitni range** hai

```cpp
int      a = 65;      // 4 bytes, integer ke roop mein padho -> 65
char     b = 65;      // 1 byte, character ke roop mein padho -> 'A'
```

Same bits, alag matlab. Type hi fark banata hai.

---

## Ek chhota demo

```cpp
#include <iostream>
#include <bitset>

int main() {
    int  asInt  = 65;
    char asChar = 65;

    std::cout << "Bits: " << std::bitset<8>(65) << "\n\n";
    std::cout << "int ke roop mein:  " << asInt  << "\n";
    std::cout << "char ke roop mein: " << asChar << "\n";
}
```

**Output:**
```
Bits: 01000001

int ke roop mein:  65
char ke roop mein: A
```

**Bilkul same bits. Alag type. Alag matlab.**

---

## C++ statically typed hai

C++ mein har variable ka type **compile time pe fix** ho jaata hai, aur kabhi badalta nahi.

```cpp
int x = 5;
x = "hello";     // ❌ ERROR - x hamesha int rahega
```

Python mein aisa nahi hai:
```python
x = 5
x = "hello"      # ✅ Python mein chalta hai
```

### Static typing ke fayde

| Fayda | Kaise |
|---|---|
| **Errors jaldi pakde jaate hain** | Compile time pe, chalane se pehle |
| **Speed** | Runtime pe type check nahi karna padta |
| **Memory efficiency** | Compiler ko exact size pata hai |
| **Better optimization** | Compiler zyada assume kar sakta hai |
| **Self-documenting** | Type dekh ke pata chal jaata hai kya hai |

> **HFT relevance:** Static typing hi wajah hai ki C++ predictable hai. Python mein
> har operation pe runtime type check hota hai — woh nanoseconds nahi de sakta.
> Aur compile-time type errors ka matlab: aapka bug production mein nahi jaayega.

---

## C++ ke fundamental types

Abhi bas overview. Detail agli files mein.

```
                    FUNDAMENTAL TYPES
                            |
        +-------------------+-------------------+
        |                   |                   |
     void            ARITHMETIC            nullptr_t
   (kuch nahi)             |
                           |
        +------------------+------------------+
        |                                     |
     INTEGRAL                          FLOATING POINT
        |                                     |
   +----+----+----+                    +------+------+
   |    |    |    |                    |      |      |
  bool char  int  ...                float double  long double
```

### Quick reference

| Type | Size (typical) | Kya rakhta hai | Example |
|---|---|---|---|
| `bool` | 1 byte | `true` / `false` | `bool isOpen = true;` |
| `char` | 1 byte | ek character | `char grade = 'A';` |
| `short` | 2 bytes | chhota integer | `short year = 2026;` |
| `int` | **4 bytes** | integer | `int age = 20;` |
| `long` | 8 bytes (Linux) | bada integer | `long big = 10000000000L;` |
| `long long` | 8 bytes | bahut bada integer | `long long x = 9e18;` |
| `float` | 4 bytes | decimal (kam precision) | `float pi = 3.14f;` |
| `double` | 8 bytes | decimal (achhi precision) | `double pi = 3.14159;` |
| `void` | — | "kuch nahi" | `void func();` |

**⚠️ "typical" kyun likha?** Kyunki C++ standard exact sizes guarantee **nahi** karta!
Sirf minimums aur relationships guarantee karta hai. Folder ki file 09 aur 14 mein
detail — aur wahi HFT ke liye critical hai.

---

## Signed vs Unsigned (quick recap)

Har integer type ka `signed` aur `unsigned` version hota hai:

```cpp
int           a = -5;        // signed (default)
unsigned int  b = 5;         // sirf positive (aur zero)
```

| | Range (32-bit) |
|---|---|
| `int` (signed) | −2,147,483,648 to 2,147,483,647 |
| `unsigned int` | 0 to 4,294,967,295 |

Detail file 05 mein.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Type sirf documentation ke liye hai" | Type se memory size aur behaviour decide hoti hai |
| "`int` hamesha 4 bytes hota hai" | Standard guarantee nahi karta. Practically 4, par assume mat karo |
| "Data aur information alag hain" | Programming mein aksar same use hote hain |
| "Bits ka apna matlab hota hai" | Bits ka matlab TYPE deta hai |

---

## Exercises

1. Yeh bits `01100001` kya hai?
   - `int` ke roop mein?
   - `char` ke roop mein?
   <details><summary>Answer</summary>
   `int`: 97. `char`: 'a'. (Yaad hai `'a' = 97`?)
   </details>

2. Yeh program chalao:
   ```cpp
   #include <iostream>
   int main() {
       int  i = 72;
       char c = 72;
       std::cout << "int:  " << i << "\n";
       std::cout << "char: " << c << "\n";
   }
   ```
   Output kya aaya? Kyun?

3. Static typing ke 3 fayde likho.

4. Kya C++ mein yeh chalega? Kyun/kyun nahi?
   ```cpp
   int x = 5;
   x = 3.7;
   ```
   <details><summary>Answer</summary>
   **Chalega** — par `x` mein `3` aayega, `3.7` nahi. Decimal part cut jaayega
   (truncate, round nahi). `-Wconversion` warning dega. Yeh implicit conversion hai —
   file 13 mein detail.
   </details>

5. Sochkar batao: agar C++ mein types nahi hote, to `5 + 3` aur `"5" + "3"` mein
   compiler kaise fark karta?

---

## Next
→ [`02-what-is-a-variable.md`](02-what-is-a-variable.md)
