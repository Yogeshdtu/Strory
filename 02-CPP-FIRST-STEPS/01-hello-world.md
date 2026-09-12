# 01 — Hello World

## Prerequisites
Folder 01 complete. Compiler installed.

## Yeh topic abhi kyun
Kyunki pehla successful program chalana ek **psychological milestone** hai. Iske baad
aap "programmer" ho. Ek baar screen pe apna output dekh lo, phir hum uske andar ghusenge.

---

## Program

Ek file banao — `hello.cpp` — aur **apne haath se** yeh type karo:

```cpp
#include <iostream>

int main() {
    std::cout << "Hello World\n";
    return 0;
}
```

⚠️ **Copy-paste mat karo.** Type karo. Galti hogi. Achhi baat hai.

---

## Chalao

```bash
cd ~/cpp-practice
g++ -std=c++20 -Wall -Wextra hello.cpp -o hello
./hello
```

**Output:**
```
Hello World
```

Bas. Ho gaya. Aap ne C++ program likh liya. 🎉

---

## Kya-kya hua (short version)

```
   hello.cpp (aapka text)
        |
        |  g++ ...
        v
   hello (executable file)
        |
        |  ./hello
        v
   "Hello World" screen pe
```

Detail version lesson 09 mein hai. Abhi bas yeh samjho ki **do alag steps** hain:
compile karna aur chalana.

---

## Command ko todte hain

```bash
g++ -std=c++20 -Wall -Wextra hello.cpp -o hello
^^^ ^^^^^^^^^^ ^^^^^ ^^^^^^^ ^^^^^^^^^ ^^ ^^^^^
 |      |        |      |        |      |    |
 |      |        |      |        |      |    +-- output file ka naam
 |      |        |      |        |      +------- "output" flag
 |      |        |      |        +-------------- input file
 |      |        |      +----------------------- aur zyada warnings
 |      |        +------------------------------ common warnings ON
 |      +--------------------------------------- C++20 standard use karo
 +---------------------------------------------- GNU C++ compiler
```

### Agar `-o hello` na likho?
Output file ka naam `a.out` ho jayega (purani Unix parampara). Phir `./a.out` chalana padega.

### `-Wall -Wextra` kyun?
Warnings ON karne ke liye. Yeh **hamesha** lagao. 90% bugs warnings mein pehle dikh
jaate hain. HFT codebases mein aksar `-Werror` bhi hota hai (warning = build fail).

---

## `./` kyun likha?

`./hello` mein `./` ka matlab hai **"current folder"**.

Agar aap sirf `hello` likhoge, terminal system folders (`/usr/bin`, etc.) mein dhoondhega,
current folder mein nahi. Security ke liye current folder search list mein nahi hota.

---

## Ab thoda khelo (yeh zaroor karo)

### Experiment 1: Message badlo
```cpp
std::cout << "Mera naam Rahul hai\n";
```
Recompile karo, chalao.

### Experiment 2: Do lines
```cpp
std::cout << "Line ek\n";
std::cout << "Line do\n";
```

### Experiment 3: `\n` hata do
```cpp
std::cout << "Hello World";
```
Ab kya hua? Output ke baad prompt usi line pe aa gaya na? `\n` ka kaam yahi tha —
nayi line pe jaana.

### Experiment 4: Semicolon hata do (**yeh zaroor karo**)
```cpp
std::cout << "Hello World\n"
return 0;
```
Compile karo. Error dekho:
```
error: expected ';' before 'return'
```
**Yeh aapka pehla compiler error hai.** Ise dekhkar khush ho — compiler aapki madad
kar raha hai.

### Experiment 5: `std::` hata do
```cpp
cout << "Hello World\n";
```
Error:
```
error: 'cout' was not declared in this scope
```

### Experiment 6: `#include` hata do
Poori line hata do. Kya error aaya?

### Experiment 7: `return 0;` hata do
Kya error aaya?
<details><summary>Answer</summary>
**Koi error nahi!** Program chal jayega. Kyunki `main()` mein `return 0;` **optional**
hai — compiler apne aap laga deta hai. Yeh sirf `main()` ke liye hai, baaki functions
ke liye nahi. Detail lesson 04 mein.
</details>

---

## Common problems

| Problem | Kya karo |
|---|---|
| `g++: command not found` | Compiler install nahi hai → setup guide dekho |
| `No such file or directory: hello.cpp` | Galat folder mein ho → `pwd` aur `ls` karo |
| `permission denied` | `chmod +x hello` chalao |
| Compile hua par `hello: command not found` | `./` bhool gaye → `./hello` likho |
| Windows pe `./hello` kaam nahi kar raha | `.\hello.exe` try karo |
| Output nahi dikh raha | Shayad `\n` nahi hai — output buffer mein atka hai |

---

## Yeh 6 lines actually kya hain?

Abhi ek quick overview. Detail agli file mein:

```cpp
#include <iostream>              // 1. iostream ka code yahan paste karo
                                 // 2. khali line (compiler ignore karta hai)
int main() {                     // 3. main function shuru
    std::cout << "Hello World\n";// 4. text screen pe bhejo
    return 0;                    // 5. OS ko batao: sab theek hai
}                                // 6. main function khatam
```

Har line mein bahut kuch chhupa hai. Agli file mein hum har ek token kholenge.

---

## Exercises

1. `hello.cpp` likho, compile karo, chalao. (Kar liya? Achha.)
2. Upar wale saare 7 experiments karo. Har error message note karo.
3. Ek program likho jo aapka naam, city, aur goal print kare — 3 alag lines mein.
4. `-o` flag hata ke compile karo. Kaunsi file bani? Use chalao.
5. `-Wall -Wextra` ke bina compile karo. Koi fark? (Abhi shayad nahi — aage padega.)

---

## Next
→ [`02-anatomy-line-by-line.md`](02-anatomy-line-by-line.md) — **is folder ki sabse
important file**
