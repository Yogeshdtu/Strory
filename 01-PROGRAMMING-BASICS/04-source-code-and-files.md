# 04 — Source code aur files

## Prerequisites
`03-what-is-a-programming-language.md`

## Yeh topic abhi kyun
Aap C++ likhne wale ho. Woh likhoge **kahan**? Kis file mein? Us file ka naam kya hoga?
Yeh sab abhi clear kar lete hain, taaki agle folder mein confusion na ho.

---

## Source code kya hai?

**Source code = woh text jo aap likhte ho.**

Bas. Yeh koi jaadu nahi hai. Yeh **plain text** hai — bilkul waise jaise Notepad mein
likha hua kuch bhi.

```cpp
#include <iostream>

int main() {
    std::cout << "Hello World\n";
    return 0;
}
```

Yeh upar wali cheez ek text file hai. Aap ise Notepad mein bhi likh sakte ho.
Ise **special** iss baat se banaya jaata hai ki compiler ise samajh sakta hai.

---

## File extension kya hai?

File ke naam ke **aakhir mein**, dot ke baad jo aata hai.

```
   hello.cpp
   ^^^^^ ^^^
   naam  extension
```

Extension ka kaam: **batana ki file ke andar kis tarah ka data hai.**

| Extension | Kya hai |
|---|---|
| `.txt` | plain text |
| `.jpg` | photo |
| `.mp4` | video |
| `.cpp` | C++ source code |
| `.exe` | Windows executable |

### Important sach 🔑

**Extension sirf ek naam hai. Aur kuch nahi.**

Aap `hello.cpp` ka naam badalkar `hello.banana` kar sakte ho — file ka content bilkul
same rahega. Bas ab aapke computer ko nahi pata ki isse kaise kholna hai, aur compiler
shikayat karega.

Extension ek **hint** hai, **rule** nahi. Yeh baat aage bahut kaam aayegi jab hum
object files aur binaries dekhenge.

---

## C++ ke file extensions

### Source files (jahan actual code hota hai)

| Extension | Kab use hota hai |
|---|---|
| `.cpp` | **Sabse common.** Yahi use karo |
| `.cc` | Google aur kuch companies use karti hain |
| `.cxx`, `.C` | Purana, kam use hota hai |

**Is course mein hum `.cpp` use karenge.**

### Header files (jahan declarations hoti hain)

| Extension | Kab use hota hai |
|---|---|
| `.h` | C se aaya, C++ mein bhi bahut use hota hai |
| `.hpp` | "yeh pakka C++ hai" batane ke liye |
| `.hxx` | kam common |
| *(no extension)* | Standard library: `<iostream>`, `<vector>` |

Note karo: `<iostream>` mein koi extension nahi hai. Standard library headers aise hi
hote hain. Yeh design decision hai, taaki wo aapki files se clash na karein.

### Generated files (compiler banata hai, aap nahi)

| Extension | Kya hai |
|---|---|
| `.o` / `.obj` | Object file — compiled, par abhi tak linked nahi |
| `.a` / `.lib` | Static library — object files ka bundle |
| `.so` / `.dll` / `.dylib` | Dynamic/shared library |
| `.exe` / *(no ext)* | Final executable |
| `.s` / `.asm` | Assembly output |

Yeh sab folder 24 mein detail mein aayenge. Abhi bas naam se parichay ho gaya.

---

## Header vs Source — ek chhota preview

Abhi poora nahi samjhoge. Bas idea le lo:

```
   math.h  (HEADER)                   math.cpp  (SOURCE)
   +---------------------+            +---------------------------+
   | int add(int, int);  |            | int add(int a, int b) {   |
   |                     |            |     return a + b;         |
   | "aisa function      |            | }                         |
   |  exist karta hai"   |            |                           |
   | (DECLARATION)       |            | "aur woh yeh karta hai"   |
   +---------------------+            | (DEFINITION)              |
                                      +---------------------------+
```

**Header = promise.** "Ek `add` function hai, do int leta hai, ek int deta hai."
**Source = delivery.** "Aur yeh raha woh function, actually."

Kyun alag? Kyunki doosri files ko sirf **promise** chahiye hota hai use karne ke liye.
Unhe implementation dekhne ki zarurat nahi. Yeh **declaration vs definition** ka fark hai —
C++ ka ek core concept, folder 08 aur 24 mein detail mein.

---

## Encoding — ek chhota sa but important note

Text file mein har character actually ek **number** hota hai. `A` = 65, `B` = 66, etc.
Isko **encoding** kehte hain.

- **ASCII** — purana, 128 characters (English, digits, symbols)
- **UTF-8** — modern standard, duniya ki har bhasha (हिंदी, 中文, emoji 🎉)

Aapko abhi kuch karne ki zarurat nahi — VS Code default mein UTF-8 use karta hai.
Bas yaad rakho ki `'A'` ek number hai. Folder 03 mein `char` padhte waqt yeh kaam aayega.

---

## Ek chhota experiment karo (abhi)

Terminal kholo:

```bash
cd ~
mkdir -p cpp-practice && cd cpp-practice

# ek file banao
echo 'yeh sirf text hai' > test.cpp

# uska content dekho
cat test.cpp

# compile karne ki koshish karo
g++ test.cpp -o test
```

**Kya hoga?** Compiler error dega. Kyunki `yeh sirf text hai` valid C++ nahi hai.

Yeh important lesson hai: **`.cpp` extension file ko C++ nahi banata. Content banata hai.**

Ab file ka naam badal do:
```bash
mv test.cpp test.banana
cat test.banana
```
Content wahi hai! Extension sirf label tha.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`.cpp` file special format hai" | Nahi, plain text hai. Notepad se bhi likh sakte ho |
| "Extension badalne se file badal jaati hai" | Nahi, sirf label badalta hai |
| "`.h` aur `.hpp` mein technical fark hai" | Compiler ke liye koi fark nahi. Sirf convention |
| "Header files compile hoti hain" | Nahi! Header ko `.cpp` mein *paste* kiya jaata hai (preprocessor), phir `.cpp` compile hoti hai |
| "Ek project mein sirf ek file ho sakti hai" | Nahi, bade projects mein hazaaron `.cpp` files hoti hain |

---

## Exercises

1. Ek folder banao `cpp-practice/day1`. Usme 3 files banao: `a.cpp`, `b.h`, `notes.txt`.
   `ls -la` se confirm karo.

2. Yeh sochkar batao — kya `main.cpp` aur `Main.cpp` alag files hain?
   <details><summary>Answer</summary>
   Linux/Mac pe: **haan, alag hain** (case-sensitive filesystem).
   Windows pe: **nahi, same hain** (case-insensitive).
   Yeh real bug ka source hai — Windows pe project chalta hai, Linux pe build fail.
   HFT mein sab Linux hai, isliye hamesha exact case use karo.
   </details>

3. `.o` file kya hoti hai? `.exe` se kya fark hai?
   <details><summary>Answer</summary>
   `.o` = ek `.cpp` file ka compiled output, par abhi tak adhoora — usme doosre files ke
   functions ke references "khali" pade hain. `.exe` = sab `.o` files jodne (link) ke baad
   bana complete, chalne wala program.
   </details>

4. Standard library headers (`<iostream>`) mein extension kyun nahi hoti?
   <details><summary>Answer</summary>
   Taaki wo aapki files se naam clash na karein, aur taaki implementation ko azadi mile
   (kuch compilers mein `<vector>` actually ek file bhi nahi hoti — built-in ho sakti hai).
   </details>

---

## Next
→ [`05-compiler-vs-interpreter.md`](05-compiler-vs-interpreter.md)
